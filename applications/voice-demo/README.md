# Voice Demo — RTL8762GN

> **语言**: [English](README.en.md) | 中文

基于 Zephyr RTOS 的语音采集与无线传输演示工程，运行于 Realtek RTL8762GN EVB。  
支持 BLE HID 语音传输和 2.4G PPT 无线语音传输（Master→Slave→USB UAC）两种主要模式，
以及四个阶段的 UART 数据导出功能。

---

## 目录

- [硬件环境](#硬件环境)
- [功能概述](#功能概述)
- [代码架构](#代码架构)
- [模式一：UART 数据导出](#模式一uart-数据导出)
- [模式二：BLE HID 语音传输](#模式二ble-hid-语音传输)
- [模式三：2.4G PPT 无线语音传输](#模式三24g-ppt-无线语音传输)
- [构建与烧录](#构建与烧录)
- [关键配置速查](#关键配置速查)

---

## 硬件环境

| 项目 | 规格 |
|------|------|
| 芯片 | RTL8762GN |
| 开发板 | RTL8762GN EVB |
| MIC 类型 | 模拟差分麦克风 (AMIC)，引脚 P2_6 / P2_7 |
| MIC BIAS | H_0 (MICBIAS) |
| I2S 接口 | I2S0，BCLK=P5_7，SDI=P2_4，SDO=P4_3（也可不配，内部与codec绑定） |
| 日志/UART dump | UART3，TX=P3_1，RX=P3_3，波特率 2 000 000 |
| 按键 | GPIOA10（测试用 MIC 触发键） |

---

## 功能概述

| 模式 | 传输介质 | 编码 | 状态 |
|------|---------|------|------|
| UART 数据导出（4 阶段） | UART3 | 无（PCM 原始/解码数据）或编码码流 | 已验证 |
| BLE HID 语音传输 | BLE GATT (HID over GATT) | mSBC / SBC / IMA-ADPCM | 已验证 |
| 2.4G PPT 主/从传输 + USB UAC | 2.4G 私有协议 (ppt_sync) | SBC (master→slave) + USB Audio (slave→host) | **已验证（端到端）** |

---

## 代码架构

```
voice-demo/
├── CMakeLists.txt          # 构建入口；PPT 源码按角色条件编译（Master/Slave）
├── Kconfig                 # 应用级 Kconfig：日志级别、BT 设备名、PPT 角色选择、UAC 采样率
├── prj.conf                # 默认配置（PPT Slave + USB Audio；BLE 被注释）
├── rtl8762gn_evb.overlay   # 板级硬件描述（I2S、Codec、UART、USB 音频 UAC）
├── inc/
│   ├── config.h            # ★ 所有特性宏开关（编码类型、语音流协议、UART dump 阶段）
│   ├── voice/
│   │   ├── voice_driver.h  # I2S + Codec 驱动接口、帧参数宏
│   │   └── voice_handle.h  # 语音流状态机接口
│   ├── ble/
│   │   ├── ble.h           # BLE 初始化 / 连接管理
│   │   ├── hog.h           # HID over GATT 报告发送
│   │   └── hid.h           # HID 描述符定义
│   ├── ppt/
│   │   ├── ppt_protocol.h  # 2.4G 数据包格式定义
│   │   ├── voice_ppt_master.h
│   │   └── voice_ppt_slave.h
│   ├── button.h / button_handle.h
│   └── loop_queue.h        # 环形帧队列
├── src/
│   ├── main.c              # 应用入口；按角色初始化 BLE / PPT
│   ├── loop_queue.c
│   ├── voice/
│   │   ├── voice_driver.c  # Codec + I2S 初始化、DMA 接收、超时保护
│   │   └── voice_handle.c  # ★ 核心：帧编码 → 队列 → 发送；UART dump 阶段1/2
│   ├── ble/
│   │   ├── ble.c           # BLE 广播 / 配对 / 连接回调
│   │   └── hog.c           # HID Report 封装与 notify
│   ├── button/
│   │   ├── button.c        # GPIO 按键驱动
│   │   └── button_handle.c # 按键事件 → voice_handle_mic_key_pressed/released
│   └── ppt/
│       ├── voice_ppt_master.c  # [仅 Master] ppt_sync 配对/连接，发送编码帧
│       ├── voice_ppt_slave.c   # [仅 Slave]  接收 SBC 帧，解码 → app_usb_audio_send；UART dump 阶段3
└──     └── usb_audio.c         # [仅 Slave]  USB UAC 麦克风：ring buf 预填充 → UAC IN；UART dump 阶段4
```

### 数据流简图

```
【Master 端】
[MIC] → I2S DMA → voice_rx_thread ──[UART dump 阶段1: 原始 PCM]──→
                       │
                 voice_handle_rx_data_callback (k_work)
                       │
               encode_raw_data()  ←── config.h: VOICE_ENC_TYPE
              (mSBC / SBC / IMA-ADPCM)
                       │──[UART dump 阶段2: 编码码流]──→
                       │
               ┌───────┴──────────────┐
               │                      │
          BLE HOG notify         PPT Master send
          hog_send_voice_report  app_ppt_send_voice_data
                                       │
                                  [2.4G Air]
                                       │
【Slave 端】                      PPT Slave receive
                                 sbc_decode()
                                       │──[UART dump 阶段3: 解码 PCM]──→
                                 app_usb_audio_send()
                                       │
                                 USB UAC ring buf
                                       │──[UART dump 阶段4: UAC IN 帧]──→
                                  USB Audio (16-bit, Mono)
                                       │
                                  [Host PC / Dongle]
```

**UART dump 各阶段说明**

| 阶段 | 宏定义 | 适用角色 | 抓取内容 | 用途 |
|------|--------|---------|---------|------|
| 1 | `FEATURE_SUPPORT_UART_DUMP_VOICE_RAW_DATA` | Master | 编码前原始 PCM | 验证 MIC 采集效果 |
| 2 | `FEATURE_SUPPORT_UART_DUMP_VOICE_ENCODE_DATA` | Master | 编码后码流 | 验证编码器输出 |
| 3 | `FEATURE_SUPPORT_UART_DUMP_VOICE_DECODE_DATA` | Slave | 解码后 PCM | 验证 Slave 解码质量 |
| 4 | `FEATURE_SUPPORT_UART_DUMP_UAC_SEND_DATA` | Slave | UAC IN 发送帧 | 验证 USB 输出数据 |

---

## 模式一：UART 数据导出

通过 UART 抓取语音链路中任意阶段的数据，用于调试和验证各模块的工作状态。
全部阶段默认关闭（宏值为 `0`），按需在 `inc/config.h` 中单独启用。

### 启用步骤

在 `inc/config.h` 中将对应阶段的宏设为 `1`：

```c
/* 阶段 1：Master 端原始 PCM（编码前，适用于 MIC 硬件验证） */
#define FEATURE_SUPPORT_UART_DUMP_VOICE_RAW_DATA     1

/* 阶段 2：Master 端编码码流（SBC/mSBC 帧，适用于编码器调试） */
#define FEATURE_SUPPORT_UART_DUMP_VOICE_ENCODE_DATA  1

/* 阶段 3：Slave 端解码 PCM（2.4G 接收解码后，适用于链路音质验证） */
#define FEATURE_SUPPORT_UART_DUMP_VOICE_DECODE_DATA  1

/* 阶段 4：Slave 端 UAC IN 帧（USB 发送前，适用于 USB 数据验证） */
#define FEATURE_SUPPORT_UART_DUMP_UAC_SEND_DATA      1
```

> 阶段 1、2 在 Master 固件中生效；阶段 3、4 在 Slave 固件中生效。
> 多个阶段可以同时启用。

### 串口接收设置

| 参数 | 值 |
|------|----|
| 端口 | UART3（板上 USB-to-UART 或外接）|
| 波特率 | 2 000 000 |
| 数据位 | 8 |
| 停止位 | 1 |
| 校验 | 无 |
| 流控 | 无 |

> **注意**：UART dump 输出原始二进制字节，请使用支持**二进制/HEX 模式**保存的串口工具（如 Tera Term、MobaXterm）。
> 避免使用 ASCII 纯文本模式，否则 0x00、0x0A 等字节会被转义或丢失。

### 各阶段数据格式与 Audacity 导入参数

#### 阶段 1 / 阶段 3：PCM 原始数据

| 参数 | 值 |
|------|----|
| 编码 | Signed 16-bit PCM |
| 字节序 | Little-Endian |
| 声道 | 1（单声道）|
| 采样率 | 16000 Hz |

Audacity 导入步骤：**文件 → 导入 → 原始数据 (Raw Data)**，填入上述参数后确认即可播放。

#### 阶段 2：编码码流

SBC / mSBC 编码帧为压缩二进制格式，**无法直接在 Audacity 中播放**。
可使用配套的 SBC 解码工具或参考 `libsbc` 对码流进行解码后再导入。

#### 阶段 4：UAC IN 帧数据

UAC IN 帧为 PCM 数据（按 USB 帧大小对齐），导入参数与阶段 1/3 相同。

---

## 模式二：BLE HID 语音传输

设备通过 BLE 广播，与上位机（HID Host）配对后，按下 MIC 键开始录音，编码后的语音帧经 HID Report Notify 发送到 Host 端。

### 默认配置

| 参数 | 值 |
|------|----|
| BLE 设备名 | `Zephyr Voice demo` |
| 编码格式 | SBC（`VOICE_ENC_TYPE = SW_SBC_ENC`）|
| 语音流协议 | IFLYTEK_VOICE_FLOW（`VOICE_FLOW_SEL = IFLYTEK_VOICE_FLOW`）|
| 采样率 | 16 kHz，16-bit，Mono |

### 编码类型选择

在 `inc/config.h` 中修改 `VOICE_ENC_TYPE` 和 `VOICE_FLOW_SEL`：

| 语音流 (`VOICE_FLOW_SEL`) | 编码 (`VOICE_ENC_TYPE`) | 帧大小 | 适用场景 |
|--------------------------|------------------------|--------|---------|
| `IFLYTEK_VOICE_FLOW` | `SW_MSBC_ENC` | 120 B | 讯飞语音识别（默认）|
| `RTK_GATT_VOICE_FLOW` | `SW_SBC_ENC` | 可调 | 通用 GATT 传输 |
| `ATV_GOOGLE_VOICE_FLOW` | `SW_IMA_ADPCM_ENC` | 134 B | Android TV 遥控 |

### Host 端要求

- 需要配套上位机或 Host 驱动解析 HID Report 并对编码数据进行解码。
- 数据包格式（RTK_GATT_VOICE_FLOW 示例）：

  ```
  Byte[0]   = VOICE_PACKET_TYPE_VOICE_DATA (0x01) 或 VOICE_PACKET_TYPE_VOICE_CTRL
  Byte[1:2] = 有效载荷长度 (little-endian)
  Byte[3:]  = 编码数据
  ```

### 操作方式

1. 上电，设备自动开始 BLE 广播。
2. Host 扫描并配对 `Zephyr Voice demo`。
3. 按下 GPIOA10/GPIOA4 按键（MIC 键）→ 开始录音并发送语音数据。
4. 松开按键 → 停止录音，剩余队列数据发送完毕后结束。

---

## 模式三：2.4G PPT 无线语音传输

> **当前状态**：端到端验证通过。Master（MIC 设备）采集语音，经 2.4G 无线链路传输至 Slave（Dongle），
> Slave 解码后通过 USB Audio Class（UAC）呈现为 PC 上的麦克风设备。

### 系统拓扑

```
[MIC 设备 (Master)]  ←→  2.4G  ←→  [Dongle (Slave)]  →  USB UAC  →  [PC/Host]
  RTL8762GN                              RTL8762GN              USB 麦克风
```

### 启用方法

在 `prj.conf` 中选择角色（两者**互斥**，不可同时启用）：

**Master（MIC 设备端）：**
```kconfig
CONFIG_REALTEK_PPT=y
CONFIG_VOICE_PPT_MASTER=y   # 取消注释
# CONFIG_VOICE_PPT_SLAVE=y  # 注释掉
# 同时注释掉 CONFIG_BT=y 及 BLE 相关配置（两者互斥）
```

**Slave（Dongle 端，当前默认配置）：**
```kconfig
CONFIG_REALTEK_PPT=y
CONFIG_VOICE_PPT_SLAVE=y    # 已默认启用
CONFIG_USB_DEVICE_STACK=y
CONFIG_USB_DEVICE_AUDIO=y
CONFIG_NET_BUF=y
CONFIG_RING_BUFFER=y
# 同时注释掉 CONFIG_BT=y 及 BLE 相关配置
```

> **Slave 依赖说明**：`voice_ppt_slave.c` 链接 `libppt_sync_slave.a`，该库随 hal_realtek 仓库分发。  
> 请确认 `modules/hal/realtek` 中已包含对应库文件。
>
> **Master 依赖说明**：`voice_ppt_master.c` 链接 `libppt_sync_master.a`，同上。

### 数据流说明

| 角色 | 职责 |
|------|------|
| **Master** | 采集 MIC → SBC/mSBC 编码 → `app_ppt_send_voice_data()` → 2.4G 发送 |
| **Slave** | 2.4G 接收 → `sbc_decode()` 还原 PCM → `app_usb_audio_send()` → USB UAC ring buf → USB 音频流 |

### USB Audio 规格（Slave 端）

| 参数 | 值 |
|------|----|
| USB 类 | USB Audio Class (UAC) |
| 设备类型 | Microphone |
| 采样率 | 16 kHz（默认，可通过 `APP_UAC_SAMPLE_RATE_48K` Kconfig 切换为 48 kHz）|
| 位宽 | 16-bit |
| 声道 | Mono |

### 配对与连接流程

1. 两块开发板上电后，各自调用 `sync_enable()` 初始化 2.4G 协议栈。
2. 首次上电（无绑定信息）：双方均调用 `sync_pair()` 互相发现并配对，绑定信息写入 NVM。
3. 后续上电（已有绑定）：自动调用 `sync_connect()` 直连，无需重新配对。
4. Master 连接成功后，按下按键启动 MIC 录音并开始发送语音数据。
5. Slave 插入 PC USB 口后，PC 会识别到一个 USB 麦克风设备，可直接录音。

---

## 构建与烧录

### 环境准备

参考仓库根目录 `README.md` 完成 Zephyr SDK 和 west 工具链配置。

### 构建命令

```bash
# 进入工程目录
cd applications/voice-demo

# 2.4G PPT Slave (Dongle) 模式 — 当前默认配置
west build -b rtl8762gn_evb

# 2.4G PPT Master 模式
west build -b rtl8762gn_evb -- -DCONFIG_REALTEK_PPT=y -DCONFIG_VOICE_PPT_MASTER=y

# BLE 模式（取消 prj.conf 中 CONFIG_BT=y 注释后使用）
west build -b rtl8762gn_evb
```

### 烧录

```bash
west flash
```

---

## 关键配置速查

### `inc/config.h` — 特性开关

| 宏 | 默认值 | 适用角色 | 说明 |
|----|--------|---------|------|
| `FEATURE_SUPPORT_UART_DUMP_VOICE_RAW_DATA` | `0` | Master | UART 输出编码前原始 PCM |
| `FEATURE_SUPPORT_UART_DUMP_VOICE_ENCODE_DATA` | `0` | Master | UART 输出编码后码流 |
| `FEATURE_SUPPORT_UART_DUMP_VOICE_DECODE_DATA` | `0` | Slave | UART 输出解码后 PCM |
| `FEATURE_SUPPORT_UART_DUMP_UAC_SEND_DATA` | `0` | Slave | UART 输出 UAC IN 发送帧 |
| `VOICE_FLOW_SEL` | `IFLYTEK_VOICE_FLOW` | Master | 语音流协议选择 |
| `VOICE_ENC_TYPE` | 由 `VOICE_FLOW_SEL` 决定 | Master | 编码算法 |
| `VOICE_MIC_TYPE` | `AMIC_TYPE` | Master | MIC 类型（AMIC/DMIC）|
| `CODEC_SAMPLE_RATE_SEL` | `CODEC_SAMPLE_RATE_16KHz` | Master | ADC 采样率 |
| `SUPPORT_SW_EQ` | `0` | Master | 软件均衡器（预留）|

### `prj.conf` / `Kconfig` — Kconfig 开关

| 配置项 | 说明 |
|--------|------|
| `CONFIG_BT=y` | 启用 BLE（与 PPT 互斥）|
| `CONFIG_REALTEK_PPT=y` | 启用 2.4G PPT 传输 |
| `CONFIG_VOICE_PPT_MASTER=y` | PPT Master 角色 |
| `CONFIG_VOICE_PPT_SLAVE=y` | PPT Slave 角色（当前默认）|
| `CONFIG_USB_DEVICE_STACK=y` | USB 设备栈（Slave 必须）|
| `CONFIG_USB_DEVICE_AUDIO=y` | USB Audio 类（Slave 必须）|
| `APP_UAC_SAMPLE_RATE_16K` | UAC 采样率 16 kHz（默认）|
| `APP_UAC_SAMPLE_RATE_48K` | UAC 采样率 48 kHz |

### 日志查看

| 用途 | UART | 波特率 |
|------|------|--------|
| Zephyr log（系统日志） | UART2 | 2 000 000 |
| UART dump（语音数据） | UART3，TX=P3_1 | 2 000 000 |

两路串口相互独立，同时使用不会相互干扰。
