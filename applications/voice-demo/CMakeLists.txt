# SPDX-License-Identifier: Apache-2.0
cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

project(voice-demo)

target_include_directories(app PRIVATE inc)

file(GLOB BLE_SOURCES src/ble/*.c)
file(GLOB BUTTON_SOURCES src/button/*.c)
file(GLOB VOICE_SOURCES src/voice/*.c)
file(GLOB PPT_SOURCES src/ppt/*.c)

target_sources(app PRIVATE
    src/main.c
    src/loop_queue.c
)

target_sources(app PRIVATE ${BUTTON_SOURCES})
target_sources_ifdef(CONFIG_BT app PRIVATE ${BLE_SOURCES})
target_sources_ifdef(CONFIG_AUDIO app PRIVATE ${VOICE_SOURCES})
target_sources_ifdef(CONFIG_REALTEK_PPT app PRIVATE ${PPT_SOURCES})
