# Voice Demo — RTL8762GN

基于 Zephyr RTOS 的语音采集演示工程，运行于 Realtek RTL8762GN EVB。  
支持三种数据输出模式，可根据实际需求灵活选择。

---

## 目录

- [硬件环境](#硬件环境)
- [功能概述](#功能概述)
- [代码架构](#代码架构)
- [模式一：UART 原始数据导出](#模式一uart-原始数据导出)
- [模式二：BLE HID 语音传输](#模式二ble-hid-语音传输)
- [模式三：2.4G PPT 无线语音传输（实验性）](#模式三24g-ppt-无线语音传输实验性)
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
| UART 原始数据导出 | UART3 | 无（PCM 原始数据） | 已验证 |
| BLE HID 语音传输 | BLE GATT (HID over GATT) | mSBC / SBC / IMA-ADPCM | 已验证 |
| 2.4G PPT 主/从传输 | 2.4G 私有协议 (ppt_sync) | mSBC (master→slave) + USB Audio (slave→host) | 代码完整，**尚未完整验证** |

---

## 代码架构

```
voice-demo/
├── CMakeLists.txt          # 构建入口；PPT 源码由 CONFIG_REALTEK_PPT 条件编译
├── Kconfig                 # 应用级 Kconfig：日志级别、BT 设备名、PPT 角色选择
├── prj.conf                # 默认配置（BLE + 音频；PPT/USB 被注释）
├── rtl8762gn_evb.overlay   # 板级硬件描述（I2S、Codec、UART、USB 音频 UAC）
├── inc/
│   ├── config.h            # ★ 所有特性宏开关（编码类型、语音流协议、UART dump）
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
│   │   └── voice_handle.c  # ★ 核心：帧编码 → 队列 → 发送；UART dump
│   ├── ble/
│   │   ├── ble.c           # BLE 广播 / 配对 / 连接回调
│   │   └── hog.c           # HID Report 封装与 notify
│   ├── button/
│   │   ├── button.c        # GPIO 按键驱动
│   │   └── button_handle.c # 按键事件 → voice_handle_mic_key_pressed/released
│   └── ppt/
│       ├── voice_ppt_master.c  # 2.4G Master：ppt_sync 配对/连接，发送编码帧
│       ├── voice_ppt_slave.c   # 2.4G Slave：接收 mSBC 帧，解码→USB Audio
│       └── usb_audio.c         # USB UAC 麦克风：上采样 16kHz→48kHz，ring buf→UAC
└── tools/
    ├── pkg_app/            # 预编译固件（ble 版本、master、dongle）
    └── md5/                # 固件完整性校验工具
```

### 数据流简图

```
[MIC] → I2S DMA → voice_rx_thread
                       │
                 voice_handle_rx_data_callback (k_work)
                       │
               encode_raw_data()  ←── config.h: VOICE_ENC_TYPE
              (mSBC / SBC / IMA-ADPCM)
                       │
               ┌───────┴──────────────┐
               │                      │
          BLE HOG notify         PPT Master send
          hog_send_voice_report  app_ppt_send_voice_data
                                       │
                                  [2.4G Air]
                                       │
                                 PPT Slave receive
                                 sbc_decode → app_usb_audio_send
                                       │
                                  USB UAC (48kHz, 16-bit, Mono)
                                       │
                                  [Host PC / Dongle]

[UART dump 旁路]：voice_rx_thread 在编码前直接 uart_poll_out 原始 PCM 字节
```

---

## 模式一：UART 原始数据导出

用于快速验证麦克风采集效果，**无需上位机解码**，通过 Audacity 即可直接播放。

### 启用步骤

在 `inc/config.h` 中确认以下宏为 `1`：

```c
#define FEATURE_SUPPORT_UART_DUMP_VOICE_RAW_DATA  1
```

该功能独立于 BLE / PPT，默认随 BLE 模式一起编译生效。

### 串口接收设置

| 参数 | 值 |
|------|----|
| 端口 | UART3（板上 USB-to-UART 或外接）|
| 波特率 | 2 000 000 |
| 数据位 | 8 |
| 停止位 | 1 |
| 校验 | 无 |
| 流控 | 无 |

> **注意**：UART dump 输出的是原始 PCM 数据。  
> 通过串口调试助手抓取并保存到文件里，需要以ASCII码格式保存。

### Audacity 播放方法

1. 打开 Audacity → **文件 → 导入 → 原始数据 (Raw Data)**
2. 选择保存的 `.raw` 文件
3. 导入参数：

   | 参数 | 值 |
   |------|----|
   | 编码 | Signed 16-bit PCM |
   | 字节序 | Little-Endian |
   | 声道 | 1（单声道）|
   | 采样率 | 16000 Hz |

4. 点击确认，即可播放录制内容。

---

## 模式二：BLE HID 语音传输

设备通过 BLE 广播，与上位机（HID Host）配对后，按下 MIC 键开始录音，编码后的语音帧经 HID Report Notify 发送到 Host 端。

### 默认配置

| 参数 | 值 |
|------|----|
| BLE 设备名 | `Zephyr Voice demo` |
| 编码格式 | mSBC（`VOICE_ENC_TYPE = SW_MSBC_ENC`）|
| 语音流协议 | IFLYTEK_VOICE_FLOW（`VOICE_FLOW_SEL = IFLYTEK_VOICE_FLOW`）|
| 帧大小 | 120 字节（2 × 60 字节 mSBC 帧）|
| 采样率 | 16 kHz，16-bit，Mono |

`prj.conf` 已默认开启 BLE 相关配置，无需额外修改即可构建。

### 编码类型选择

在 `inc/config.h` 中修改 `VOICE_ENC_TYPE` 和 `VOICE_FLOW_SEL`：

| 语音流 (`VOICE_FLOW_SEL`) | 编码 (`VOICE_ENC_TYPE`) | 帧大小 | 适用场景 |
|--------------------------|------------------------|--------|---------|
| `IFLYTEK_VOICE_FLOW` | `SW_MSBC_ENC` | 120 B | 讯飞语音识别 | 默认配置 |
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

## 模式三：2.4G PPT 无线语音传输（实验性）

> **当前状态**：代码框架完整，主/从两端逻辑已实现，但**尚未经过完整的端到端验证**。  
> 建议客户在充分理解代码逻辑后，根据实际硬件环境自行适配调试。

### 系统拓扑

```
[MIC 设备 (Master)]  ←→  2.4G  ←→  [Dongle (Slave)]  →  USB UAC  →  [PC/Host]
  RTL8762GN                              RTL8762GN              HID 麦克风
```

### 启用方法

在 `prj.conf` 中取消相关注释：

**Master（MIC 设备端）：**
```kconfig
CONFIG_REALTEK_PPT=y
CONFIG_VOICE_PPT_MASTER=y
# 同时注释掉 CONFIG_BT=y 及 BLE 相关配置（两者互斥）
```

**Slave（Dongle 端）：**
```kconfig
CONFIG_REALTEK_PPT=y
CONFIG_VOICE_PPT_SLAVE=y
CONFIG_USB_DEVICE_STACK=y
CONFIG_USB_DEVICE_AUDIO=y
CONFIG_NET_BUF=y
CONFIG_RING_BUFFER=y
# 同时注释掉 CONFIG_BT=y 及 BLE 相关配置
```

> **Slave 依赖说明**：`voice_ppt_slave.c` 链接 `libppt_sync_slave.a`，该库目前随 hal_realtek 仓库分发。  
> 请确认 `modules/hal/realtek` 中已包含对应库文件。

### 数据流说明

| 角色 | 职责 |
|------|------|
| **Master** | 采集 MIC → mSBC 编码 → `app_ppt_send_voice_data()` → 2.4G 发送 |
| **Slave** | 2.4G 接收 → `sbc_decode()` 还原 PCM → `app_usb_audio_send()` → USB UAC ring buf → USB 48kHz 音频流 |

### USB Audio 规格（Slave 端）

| 参数 | 值 |
|------|----|
| USB 类 | USB Audio Class (UAC 2.0) |
| 设备类型 | Microphone |
| 采样率 | 48 kHz（从 16 kHz 3× 零阶保持上采样）|
| 位宽 | 16-bit |
| 声道 | Mono |
| USB 帧大小 | 96 字节 / 1ms SOF |

---

## 构建与烧录

### 环境准备

参考仓库根目录 `README.md` 完成 Zephyr SDK 和 west 工具链配置。

### 构建命令

```bash
# 进入工程目录
cd applications/voice-demo

# BLE 模式（默认）
west build -b rtl8762gn_evb

# 2.4G PPT Master 模式
west build -b rtl8762gn_evb -- -DCONFIG_REALTEK_PPT=y -DCONFIG_VOICE_PPT_MASTER=y

# 2.4G PPT Slave (Dongle) 模式
west build -b rtl8762gn_evb -- -DCONFIG_REALTEK_PPT=y -DCONFIG_VOICE_PPT_SLAVE=y \
    -DCONFIG_USB_DEVICE_STACK=y -DCONFIG_USB_DEVICE_AUDIO=y
```

### 烧录

```bash
west flash

---

## 关键配置速查

### `inc/config.h` — 特性开关

| 宏 | 默认值 | 说明 |
|----|--------|------|
| `FEATURE_SUPPORT_UART_DUMP_VOICE_RAW_DATA` | `1` | 启用 UART 原始 PCM 输出 |
| `VOICE_FLOW_SEL` | `IFLYTEK_VOICE_FLOW` | 语音流协议选择 |
| `VOICE_ENC_TYPE` | 由 `VOICE_FLOW_SEL` 决定 | 编码算法 |
| `VOICE_MIC_TYPE` | `AMIC_TYPE` | MIC 类型（AMIC/DMIC）|
| `CODEC_SAMPLE_RATE_SEL` | `CODEC_SAMPLE_RATE_16KHz` | 采样率 |
| `SUPPORT_SW_EQ` | `0` | 软件均衡器（预留）|

### `prj.conf` — Kconfig 开关

| 配置项 | 说明 |
|--------|------|
| `CONFIG_BT=y` | 启用 BLE（与 PPT 互斥）|
| `CONFIG_REALTEK_PPT=y` | 启用 2.4G PPT 传输 |
| `CONFIG_VOICE_PPT_MASTER=y` | PPT Master 角色 |
| `CONFIG_VOICE_PPT_SLAVE=y` | PPT Slave 角色 |
| `CONFIG_USB_DEVICE_STACK=y` | USB 设备栈（Slave 必须）|
| `CONFIG_USB_DEVICE_AUDIO=y` | USB Audio 类（Slave 必须）|

### 日志查看

日志通过 UART2 输出（与 UART dump 共用同一串口，但使用 Zephyr log 格式，波特率同为 2 000 000）。  
UART dump 的原始 PCM 字节与 log 字节混合在同一串口流中——若需纯净的 PCM 数据，建议在调试阶段关闭 Zephyr log 或将 log 重定向到其他 UART。
