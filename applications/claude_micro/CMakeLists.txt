# Copyright (c) 2025 Realtek Semiconductor Corp.
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(claude_micro)

set(BUTTON_SOURCES
    src/button/button.c
    src/button/button_handle.c
)

set(LED_SOURCES
    src/led/led_driver.c
)

set(VOICE_SOURCES
    src/voice/voice_driver.c
    src/voice/voice_handle.c
)

set(USB_SOURCES
    src/usb/usb_audio.c
    src/usb/usb_hid.c
    src/usb/usbd_init.c
)

target_sources(app PRIVATE
    src/main.c
    ${BUTTON_SOURCES}
    ${LED_SOURCES}
)

target_sources_ifdef(CONFIG_AUDIO app PRIVATE ${VOICE_SOURCES})

target_sources_ifdef(CONFIG_USB_DEVICE_STACK_NEXT app PRIVATE ${USB_SOURCES})

target_include_directories(app PRIVATE inc)
