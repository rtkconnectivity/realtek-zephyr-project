# Voice Demo — RTL8762GN

> **Language**: English | [中文](README.md)

A voice capture and wireless transmission demo application based on Zephyr RTOS, running on the
Realtek RTL8762GN EVB. Supports BLE HID voice transmission and 2.4G PPT wireless voice
(Master → Slave → USB UAC), as well as four-stage UART data export for debugging.

---

## Table of Contents

- [Hardware Setup](#hardware-setup)
- [Feature Overview](#feature-overview)
- [Code Architecture](#code-architecture)
- [Mode 1: UART Data Export](#mode-1-uart-data-export)
- [Mode 2: BLE HID Voice Transmission](#mode-2-ble-hid-voice-transmission)
- [Mode 3: 2.4G PPT Wireless Voice](#mode-3-24g-ppt-wireless-voice)
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
| UART Data Export (4 stages) | UART3 | None (raw/decoded PCM) or encoded bitstream | Verified |
| BLE HID Voice | BLE GATT (HID over GATT) | mSBC / SBC / IMA-ADPCM | Verified |
| 2.4G PPT Master/Slave + USB UAC | 2.4G proprietary (ppt_sync) | SBC (master→slave) + USB Audio (slave→host) | **Verified (end-to-end)** |

---

## Code Architecture

```
voice-demo/
├── CMakeLists.txt          # Build entry; PPT sources compiled per role (Master/Slave)
├── Kconfig                 # App-level Kconfig: log level, BT device name, PPT role, UAC sample rate
├── prj.conf                # Default config (PPT Slave + USB Audio; BLE commented out)
├── rtl8762gn_evb.overlay   # Board-level device tree (I2S, Codec, UART, USB audio UAC)
├── inc/
│   ├── config.h            # ★ All feature macros (codec type, voice flow, UART dump stages)
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
│   │   └── voice_handle.c  # ★ Core: frame encode → queue → send; UART dump stage 1/2
│   ├── ble/
│   │   ├── ble.c           # BLE advertising / pairing / connection callbacks
│   │   └── hog.c           # HID Report packaging and notify
│   ├── button/
│   │   ├── button.c        # GPIO button driver
│   │   └── button_handle.c # Button events → voice_handle_mic_key_pressed/released
│   └── ppt/
│       ├── voice_ppt_master.c  # [Master only] ppt_sync pairing/connect, send encoded frames
│       ├── voice_ppt_slave.c   # [Slave only]  receive SBC frames, decode → app_usb_audio_send; UART dump stage 3
└──     └── usb_audio.c         # [Slave only]  USB UAC mic: ring buf pre-fill gate → UAC IN; UART dump stage 4
```

### Data Flow Diagram

```
[Master Side]
[MIC] → I2S DMA → voice_rx_thread ──[UART dump stage 1: raw PCM]──→
                       │
                 voice_handle_rx_data_callback (k_work)
                       │
               encode_raw_data()  ←── config.h: VOICE_ENC_TYPE
              (mSBC / SBC / IMA-ADPCM)
                       │──[UART dump stage 2: encoded bitstream]──→
                       │
               ┌───────┴──────────────┐
               │                      │
          BLE HOG notify         PPT Master send
          hog_send_voice_report  app_ppt_send_voice_data
                                       │
                                  [2.4G Air]
                                       │
[Slave Side]                      PPT Slave receive
                                 sbc_decode()
                                       │──[UART dump stage 3: decoded PCM]──→
                                 app_usb_audio_send()
                                       │
                                 USB UAC ring buf
                                       │──[UART dump stage 4: UAC IN frames]──→
                                  USB Audio (16-bit, Mono)
                                       │
                                  [Host PC / Dongle]
```

**UART Dump Stage Summary**

| Stage | Macro | Role | Captured Data | Purpose |
|-------|-------|------|---------------|---------|
| 1 | `FEATURE_SUPPORT_UART_DUMP_VOICE_RAW_DATA` | Master | Raw PCM before encoding | Verify MIC capture |
| 2 | `FEATURE_SUPPORT_UART_DUMP_VOICE_ENCODE_DATA` | Master | Encoded bitstream | Verify encoder output |
| 3 | `FEATURE_SUPPORT_UART_DUMP_VOICE_DECODE_DATA` | Slave | Decoded PCM | Verify link audio quality |
| 4 | `FEATURE_SUPPORT_UART_DUMP_UAC_SEND_DATA` | Slave | UAC IN frames | Verify USB output data |

---

## Mode 1: UART Data Export

Capture voice data at any stage of the pipeline via UART for debugging and verification.
All stages are disabled by default (macro value `0`); enable them individually in `inc/config.h`.

### Enable

Set the desired stage macro to `1` in `inc/config.h`:

```c
/* Stage 1: Master raw PCM (before encoding, for MIC hardware verification) */
#define FEATURE_SUPPORT_UART_DUMP_VOICE_RAW_DATA     1

/* Stage 2: Master encoded bitstream (SBC/mSBC frames, for encoder debugging) */
#define FEATURE_SUPPORT_UART_DUMP_VOICE_ENCODE_DATA  1

/* Stage 3: Slave decoded PCM (after 2.4G receive and decode, for link quality check) */
#define FEATURE_SUPPORT_UART_DUMP_VOICE_DECODE_DATA  1

/* Stage 4: Slave UAC IN frames (before USB send, for USB data verification) */
#define FEATURE_SUPPORT_UART_DUMP_UAC_SEND_DATA      1
```

> Stages 1 and 2 take effect in the Master firmware; stages 3 and 4 in the Slave firmware.
> Multiple stages can be enabled simultaneously.

### Serial Port Settings

| Parameter | Value |
|-----------|-------|
| Port | UART3 (on-board USB-to-UART or external) |
| Baud rate | 2,000,000 |
| Data bits | 8 |
| Stop bits | 1 |
| Parity | None |
| Flow control | None |

> **Important**: UART dump outputs raw binary bytes. Use a serial tool that supports
> **binary / hex capture** (e.g., Tera Term, MobaXterm). Do not use ASCII text mode —
> bytes such as 0x00 or 0x0A will be escaped or dropped.

### Data Format and Audacity Import Settings

#### Stage 1 / Stage 3: Raw PCM Data

| Parameter | Value |
|-----------|-------|
| Encoding | Signed 16-bit PCM |
| Byte order | Little-Endian |
| Channels | 1 (Mono) |
| Sample rate | 16000 Hz |

In Audacity: **File → Import → Raw Data**, enter the parameters above and click OK to play.

#### Stage 2: Encoded Bitstream

SBC / mSBC encoded frames are compressed binary data and **cannot be played directly in Audacity**.
Use a compatible SBC decoder tool or `libsbc` to decode the bitstream first.

#### Stage 4: UAC IN Frame Data

UAC IN frames contain PCM data aligned to USB frame boundaries; use the same import settings
as stage 1 / stage 3.

---

## Mode 2: BLE HID Voice Transmission

The device advertises over BLE. After pairing with a HID Host, press the MIC key to start
recording. Encoded voice frames are delivered via HID Report Notify to the host.

### Default Configuration

| Parameter | Value |
|-----------|-------|
| BLE device name | `Zephyr Voice demo` |
| Encoding | SBC (`VOICE_ENC_TYPE = SW_SBC_ENC`) |
| Voice flow protocol | IFLYTEK_VOICE_FLOW (`VOICE_FLOW_SEL = IFLYTEK_VOICE_FLOW`) |
| Sample rate | 16 kHz, 16-bit, Mono |

### Encoding Type Selection

Modify `VOICE_ENC_TYPE` and `VOICE_FLOW_SEL` in `inc/config.h`:

| Voice Flow (`VOICE_FLOW_SEL`) | Encoding (`VOICE_ENC_TYPE`) | Frame Size | Use Case |
|------------------------------|-----------------------------|------------|----------|
| `IFLYTEK_VOICE_FLOW` | `SW_MSBC_ENC` | 120 B | iFlytek ASR (default) |
| `RTK_GATT_VOICE_FLOW` | `SW_SBC_ENC` | Configurable | Generic GATT transport |
| `ATV_GOOGLE_VOICE_FLOW` | `SW_IMA_ADPCM_ENC` | 134 B | Android TV remote |

### Host Requirements

- A companion host application or driver is required to parse HID Reports and decode the audio.
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

## Mode 3: 2.4G PPT Wireless Voice

> **Current Status**: End-to-end validation complete. The Master (MIC device) captures voice,
> transmits it over the 2.4G wireless link to the Slave (Dongle), and the Slave decodes and
> presents the audio to the PC as a USB Audio Class (UAC) microphone device.

### System Topology

```
[MIC Device (Master)]  ←→  2.4G  ←→  [Dongle (Slave)]  →  USB UAC  →  [PC/Host]
    RTL8762GN                              RTL8762GN             USB Microphone
```

### How to Enable

Select a role in `prj.conf` (the two roles are **mutually exclusive**):

**Master (MIC device side):**
```kconfig
CONFIG_REALTEK_PPT=y
CONFIG_VOICE_PPT_MASTER=y   # uncomment this
# CONFIG_VOICE_PPT_SLAVE=y  # comment out this
# Also comment out CONFIG_BT=y and all BLE-related options (mutually exclusive with PPT)
```

**Slave (Dongle side — current default configuration):**
```kconfig
CONFIG_REALTEK_PPT=y
CONFIG_VOICE_PPT_SLAVE=y    # already enabled by default
CONFIG_USB_DEVICE_STACK=y
CONFIG_USB_DEVICE_AUDIO=y
CONFIG_NET_BUF=y
CONFIG_RING_BUFFER=y
# Also comment out CONFIG_BT=y and BLE-related configs
```

> **Slave dependency**: `voice_ppt_slave.c` links against `libppt_sync_slave.a`, distributed
> with the hal_realtek repository. Ensure the library is present under `modules/hal/realtek`.
>
> **Master dependency**: `voice_ppt_master.c` links against `libppt_sync_master.a`, same location.

### Data Flow

| Role | Responsibility |
|------|----------------|
| **Master** | Capture MIC → SBC/mSBC encode → `app_ppt_send_voice_data()` → 2.4G transmit |
| **Slave** | 2.4G receive → `sbc_decode()` restore PCM → `app_usb_audio_send()` → USB UAC ring buf → USB audio stream |

### USB Audio Specification (Slave Side)

| Parameter | Value |
|-----------|-------|
| USB Class | USB Audio Class (UAC) |
| Device Type | Microphone |
| Sample Rate | 16 kHz (default; switchable to 48 kHz via `APP_UAC_SAMPLE_RATE_48K` Kconfig) |
| Bit Depth | 16-bit |
| Channels | Mono |

### Pairing and Connection Flow

1. Both boards power on and call `sync_enable()` to initialize the 2.4G stack.
2. **First power-on** (no bond stored): both sides call `sync_pair()` to discover each other
   and pair; bond information is written to NVM.
3. **Subsequent power-on** (bond exists): `sync_connect()` is called automatically for
   fast reconnection — no re-pairing needed.
4. Once the Master is connected, pressing the button starts MIC recording and voice transmission.
5. Plug the Slave (Dongle) into a PC USB port — the PC recognizes it as a USB microphone
   device and can record audio immediately.

---

## Build and Flash

### Environment Setup

Refer to the repository root `README.md` to configure the Zephyr SDK and `west` toolchain.

### Build Commands

```bash
# Enter the application directory
cd applications/voice-demo

# 2.4G PPT Slave (Dongle) mode — current default configuration
west build -b rtl8762gn_evb

# 2.4G PPT Master mode
west build -b rtl8762gn_evb -- -DCONFIG_REALTEK_PPT=y -DCONFIG_VOICE_PPT_MASTER=y

# BLE mode (uncomment CONFIG_BT=y in prj.conf first)
west build -b rtl8762gn_evb
```

### Flash

```bash
west flash
```

---

## Key Configuration Reference

### `inc/config.h` — Feature Switches

| Macro | Default | Role | Description |
|-------|---------|------|-------------|
| `FEATURE_SUPPORT_UART_DUMP_VOICE_RAW_DATA` | `0` | Master | UART output of raw PCM before encoding |
| `FEATURE_SUPPORT_UART_DUMP_VOICE_ENCODE_DATA` | `0` | Master | UART output of encoded bitstream |
| `FEATURE_SUPPORT_UART_DUMP_VOICE_DECODE_DATA` | `0` | Slave | UART output of decoded PCM |
| `FEATURE_SUPPORT_UART_DUMP_UAC_SEND_DATA` | `0` | Slave | UART output of UAC IN frames |
| `VOICE_FLOW_SEL` | `IFLYTEK_VOICE_FLOW` | Master | Voice flow protocol selection |
| `VOICE_ENC_TYPE` | Derived from `VOICE_FLOW_SEL` | Master | Encoding algorithm |
| `VOICE_MIC_TYPE` | `AMIC_TYPE` | Master | MIC type (AMIC / DMIC) |
| `CODEC_SAMPLE_RATE_SEL` | `CODEC_SAMPLE_RATE_16KHz` | Master | ADC sample rate |
| `SUPPORT_SW_EQ` | `0` | Master | Software equalizer (reserved) |

### `prj.conf` / `Kconfig` — Kconfig Switches

| Config | Description |
|--------|-------------|
| `CONFIG_BT=y` | Enable BLE (mutually exclusive with PPT) |
| `CONFIG_REALTEK_PPT=y` | Enable 2.4G PPT transport |
| `CONFIG_VOICE_PPT_MASTER=y` | PPT Master role |
| `CONFIG_VOICE_PPT_SLAVE=y` | PPT Slave role (current default) |
| `CONFIG_USB_DEVICE_STACK=y` | USB device stack (required for Slave) |
| `CONFIG_USB_DEVICE_AUDIO=y` | USB Audio class (required for Slave) |
| `APP_UAC_SAMPLE_RATE_16K` | UAC sample rate 16 kHz (default) |
| `APP_UAC_SAMPLE_RATE_48K` | UAC sample rate 48 kHz |

### Log Output

| Purpose | UART | Baud Rate |
|---------|------|-----------|
| Zephyr log (system logging) | UART2 | 2,000,000 |
| UART dump (voice data) | UART3, TX=P3_1 | 2,000,000 |

The two serial ports are independent and can be used simultaneously without interference.
