# Voice Demo — RTL8762GN

> **Language**: English | [中文](README.md)

A voice capture demo application based on Zephyr RTOS, running on the Realtek RTL8762GN EVB.
Supports three data output modes that can be selected based on your requirements.

---

## Table of Contents

- [Hardware Setup](#hardware-setup)
- [Feature Overview](#feature-overview)
- [Code Architecture](#code-architecture)
- [Mode 1: UART Raw Data Export](#mode-1-uart-raw-data-export)
- [Mode 2: BLE HID Voice Transmission](#mode-2-ble-hid-voice-transmission)
- [Mode 3: 2.4G PPT Wireless Voice (Experimental)](#mode-3-24g-ppt-wireless-voice-experimental)
- [Build and Flash](#build-and-flash)
- [Key Configuration Reference](#key-configuration-reference)

---

## Hardware Setup

| Item | Specification |
|------|---------------|
| SoC | RTL8762GN |
| Board | RTL8762GN EVB |
| MIC Type | Analog differential microphone (AMIC), pins P2_6 / P2_7 |
| MIC BIAS | H_0 (MICBIAS) |
| I2S Interface | I2S0, BCLK=P5_7, SDI=P2_4, SDO=P4_3 |
| Log / UART dump | UART3, TX=P3_1, RX=P3_3, baud rate 2,000,000 |
| Button | GPIOA10 (MIC trigger key for testing) |

---

## Feature Overview

| Mode | Transport | Encoding | Status |
|------|-----------|----------|--------|
| UART Raw Data Export | UART3 | None (raw PCM) | Verified |
| BLE HID Voice | BLE GATT (HID over GATT) | mSBC / SBC / IMA-ADPCM | Verified |
| 2.4G PPT Master/Slave | 2.4G proprietary (ppt_sync) | mSBC (master→slave) + USB Audio (slave→host) | Code complete, **not fully validated** |

---

## Code Architecture

```
voice-demo/
├── CMakeLists.txt          # Build entry; PPT sources conditionally compiled via CONFIG_REALTEK_PPT
├── Kconfig                 # App-level Kconfig: log level, BT device name, PPT role
├── prj.conf                # Default config (BLE + audio; PPT/USB commented out)
├── rtl8762gn_evb.overlay   # Board-level device tree (I2S, Codec, UART, USB audio UAC)
├── inc/
│   ├── config.h            # ★ All feature macros (codec type, voice flow, UART dump)
│   ├── voice/
│   │   ├── voice_driver.h  # I2S + Codec driver interface, frame parameter macros
│   │   └── voice_handle.h  # Voice stream state machine interface
│   ├── ble/
│   │   ├── ble.h           # BLE init / connection management
│   │   ├── hog.h           # HID over GATT report send
│   │   └── hid.h           # HID descriptor definitions
│   ├── ppt/
│   │   ├── ppt_protocol.h  # 2.4G packet format definitions
│   │   ├── voice_ppt_master.h
│   │   └── voice_ppt_slave.h
│   ├── button.h / button_handle.h
│   └── loop_queue.h        # Circular frame queue
├── src/
│   ├── main.c              # App entry; initializes BLE or PPT based on role
│   ├── loop_queue.c
│   ├── voice/
│   │   ├── voice_driver.c  # Codec + I2S init, DMA receive, timeout guard
│   │   └── voice_handle.c  # ★ Core: frame encode → queue → send; UART dump
│   ├── ble/
│   │   ├── ble.c           # BLE advertising / pairing / connection callbacks
│   │   └── hog.c           # HID Report packaging and notify
│   ├── button/
│   │   ├── button.c        # GPIO button driver
│   │   └── button_handle.c # Button events → voice_handle_mic_key_pressed/released
│   └── ppt/
│       ├── voice_ppt_master.c  # 2.4G Master: ppt_sync pairing/connect, send encoded frames
│       ├── voice_ppt_slave.c   # 2.4G Slave: receive mSBC frames, decode → USB Audio
└──     └── usb_audio.c         # USB UAC mic: upsample 16kHz→48kHz, ring buf → UAC
```
### Data Flow Diagram

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

[UART dump bypass]: voice_rx_thread calls uart_poll_out on raw PCM bytes before encoding
```

---

## Mode 1: UART Raw Data Export

Used to quickly verify microphone capture quality. **No host-side decoder needed** — playable directly in Audacity.

### Enable

Confirm the following macro is set to `1` in `inc/config.h`:

```c
#define FEATURE_SUPPORT_UART_DUMP_VOICE_RAW_DATA  1
```

This feature is independent of BLE / PPT and is compiled in by default alongside the BLE build.

### Serial Port Settings

| Parameter | Value |
|-----------|-------|
| Port | UART3 (on-board USB-to-UART or external) |
| Baud rate | 2,000,000 |
| Data bits | 8 |
| Stop bits | 1 |
| Parity | None |
| Flow control | None |

> **Note**: UART dump outputs raw PCM data. Capture with a serial tool and save the file in ASCII format.

### Playback in Audacity

1. Open Audacity → **File → Import → Raw Data**
2. Select the saved `.raw` file
3. Import settings:

   | Parameter | Value |
   |-----------|-------|
   | Encoding | Signed 16-bit PCM |
   | Byte order | Little-Endian |
   | Channels | 1 (Mono) |
   | Sample rate | 16000 Hz |

4. Click OK to play the recorded audio.

---

## Mode 2: BLE HID Voice Transmission

The device advertises over BLE. After pairing with a HID Host, press the MIC key to start recording. Encoded voice frames are delivered via HID Report Notify to the host.

### Default Configuration

| Parameter | Value |
|-----------|-------|
| BLE device name | `Zephyr Voice demo` |
| Encoding | mSBC (`VOICE_ENC_TYPE = SW_MSBC_ENC`) |
| Voice flow protocol | IFLYTEK_VOICE_FLOW (`VOICE_FLOW_SEL = IFLYTEK_VOICE_FLOW`) |
| Frame size | 120 bytes (2 × 60-byte mSBC frames) |
| Sample rate | 16 kHz, 16-bit, Mono |

BLE-related options are enabled by default in `prj.conf`; no modification is needed to build.

### Encoding Type Selection

Modify `VOICE_ENC_TYPE` and `VOICE_FLOW_SEL` in `inc/config.h`:

| Voice Flow (`VOICE_FLOW_SEL`) | Encoding (`VOICE_ENC_TYPE`) | Frame Size | Use Case |
|------------------------------|-----------------------------|------------|----------|
| `IFLYTEK_VOICE_FLOW` | `SW_MSBC_ENC` | 120 B | iFlytek ASR (default) |
| `RTK_GATT_VOICE_FLOW` | `SW_SBC_ENC` | Configurable | Generic GATT transport |
| `ATV_GOOGLE_VOICE_FLOW` | `SW_IMA_ADPCM_ENC` | 134 B | Android TV remote |

### Host Requirements

- A companion host application or driver is required to parse HID Reports and decode the audio data.
- Packet format (RTK_GATT_VOICE_FLOW example):

  ```
  Byte[0]   = VOICE_PACKET_TYPE_VOICE_DATA (0x01) or VOICE_PACKET_TYPE_VOICE_CTRL
  Byte[1:2] = Payload length (little-endian)
  Byte[3:]  = Encoded data
  ```

### Usage

1. Power on — the device starts BLE advertising automatically.
2. The host scans and pairs with `Zephyr Voice demo`.
3. Press GPIOA10 (MIC key) → recording starts and voice data is sent.
4. Release the key → recording stops; remaining queued data is flushed before ending.

---

## Mode 3: 2.4G PPT Wireless Voice (Experimental)

> **Current Status**: The code framework is complete and both master/slave logic are implemented, but **end-to-end validation has not been fully completed**.
> Customers are advised to review the code logic and adapt it to their actual hardware environment before use.

### System Topology

```
[MIC Device (Master)]  ←→  2.4G  ←→  [Dongle (Slave)]  →  USB UAC  →  [PC/Host]
    RTL8762GN                              RTL8762GN             HID Microphone
```

### How to Enable

Uncomment the relevant lines in `prj.conf`:

**Master (MIC device side):**
```kconfig
CONFIG_REALTEK_PPT=y
CONFIG_VOICE_PPT_MASTER=y
# Also comment out CONFIG_BT=y and BLE-related configs (mutually exclusive)
```

**Slave (Dongle side):**
```kconfig
CONFIG_REALTEK_PPT=y
CONFIG_VOICE_PPT_SLAVE=y
CONFIG_USB_DEVICE_STACK=y
CONFIG_USB_DEVICE_AUDIO=y
CONFIG_NET_BUF=y
CONFIG_RING_BUFFER=y
# Also comment out CONFIG_BT=y and BLE-related configs
```

> **Slave dependency**: `voice_ppt_slave.c` links against `libppt_sync_slave.a`, which is distributed with the hal_realtek repository.
> Ensure the library is present under `modules/hal/realtek`.

### Data Flow

| Role | Responsibility |
|------|----------------|
| **Master** | Capture MIC → mSBC encode → `app_ppt_send_voice_data()` → 2.4G transmit |
| **Slave** | 2.4G receive → `sbc_decode()` restore PCM → `app_usb_audio_send()` → USB UAC ring buf → USB 48kHz audio stream |

### USB Audio Specification (Slave Side)

| Parameter | Value |
|-----------|-------|
| USB Class | USB Audio Class (UAC 2.0) |
| Device Type | Microphone |
| Sample Rate | 48 kHz (upsampled from 16 kHz via 3× zero-order hold) |
| Bit Depth | 16-bit |
| Channels | Mono |
| USB Frame Size | 96 bytes / 1ms SOF |

---

## Build and Flash

### Environment Setup

Refer to the repository root `README.md` to configure the Zephyr SDK and `west` toolchain.

### Build Commands

```bash
# Enter the application directory
cd applications/voice-demo

# BLE mode (default)
west build -b rtl8762gn_evb

# 2.4G PPT Master mode
west build -b rtl8762gn_evb -- -DCONFIG_REALTEK_PPT=y -DCONFIG_VOICE_PPT_MASTER=y

# 2.4G PPT Slave (Dongle) mode
west build -b rtl8762gn_evb -- -DCONFIG_REALTEK_PPT=y -DCONFIG_VOICE_PPT_SLAVE=y \
    -DCONFIG_USB_DEVICE_STACK=y -DCONFIG_USB_DEVICE_AUDIO=y
```

### Flash

```bash
west flash
```

Or use the pre-built firmware in `tools/pkg_app/` with MPTool.

---

## Key Configuration Reference

### `inc/config.h` — Feature Switches

| Macro | Default | Description |
|-------|---------|-------------|
| `FEATURE_SUPPORT_UART_DUMP_VOICE_RAW_DATA` | `1` | Enable UART raw PCM output |
| `VOICE_FLOW_SEL` | `IFLYTEK_VOICE_FLOW` | Voice flow protocol selection |
| `VOICE_ENC_TYPE` | Derived from `VOICE_FLOW_SEL` | Encoding algorithm |
| `VOICE_MIC_TYPE` | `AMIC_TYPE` | MIC type (AMIC / DMIC) |
| `CODEC_SAMPLE_RATE_SEL` | `CODEC_SAMPLE_RATE_16KHz` | Sample rate |
| `SUPPORT_SW_EQ` | `0` | Software equalizer (reserved) |

### `prj.conf` — Kconfig Switches

| Config | Description |
|--------|-------------|
| `CONFIG_BT=y` | Enable BLE (mutually exclusive with PPT) |
| `CONFIG_REALTEK_PPT=y` | Enable 2.4G PPT transport |
| `CONFIG_VOICE_PPT_MASTER=y` | PPT Master role |
| `CONFIG_VOICE_PPT_SLAVE=y` | PPT Slave role |
| `CONFIG_USB_DEVICE_STACK=y` | USB device stack (required for Slave) |
| `CONFIG_USB_DEVICE_AUDIO=y` | USB Audio class (required for Slave) |

### Log Output

Logs are output via UART2 using Zephyr log format at 2,000,000 baud.
Raw PCM bytes from UART dump and log output share the same UART stream — if clean PCM data is needed, disable Zephyr logging or redirect it to another UART during development.
