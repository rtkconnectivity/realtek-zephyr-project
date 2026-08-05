# SPDX-License-Identifier: Apache-2.0
cmake_minimum_required(VERSION 3.20.0)

# Expose dts/bindings/ so Zephyr finds voice,test-pin.yaml
list(APPEND DTS_ROOT ${CMAKE_CURRENT_SOURCE_DIR})

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

project(voice-demo)

target_include_directories(app PRIVATE inc)

file(GLOB BLE_SOURCES src/ble/*.c)
file(GLOB BUTTON_SOURCES src/button/*.c)
file(GLOB VOICE_SOURCES src/voice/*.c)

target_sources(app PRIVATE
    src/main.c
    src/loop_queue.c
)

target_sources_ifdef(CONFIG_BT app PRIVATE ${BLE_SOURCES})
target_sources_ifdef(CONFIG_AUDIO app PRIVATE ${VOICE_SOURCES})
target_sources_ifdef(CONFIG_VOICE_PPT_MASTER app PRIVATE src/ppt/voice_ppt_master.c)
target_sources_ifdef(CONFIG_VOICE_PPT_MASTER app PRIVATE src/button/button.c)
target_sources_ifdef(CONFIG_VOICE_PPT_MASTER app PRIVATE src/button/button_handle.c)

target_sources_ifdef(CONFIG_VOICE_PPT_SLAVE app PRIVATE
    src/ppt/usb_audio.c
    src/ppt/voice_ppt_slave.c
)
