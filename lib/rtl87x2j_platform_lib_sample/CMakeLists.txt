# Copyright (c) 2025 Realtek Semiconductor Corporation
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)

# soc.c includes the platform API while Zephyr is being configured. Add the
# external library's public headers before find_package(Zephyr), matching the
# integration role of an external library while keeping this sample standalone.
set(RTL87X2J_PLATFORM_LIB_DIR
    "${CMAKE_CURRENT_SOURCE_DIR}/platform_lib")
include_directories("${RTL87X2J_PLATFORM_LIB_DIR}/include")

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(rtl87x2j_platform_lib_sample)

target_sources(app PRIVATE src/main.c)

# Build the Makefile-based library with flags exported from this Zephyr image,
# expose it as an imported CMake target, and link it into the runnable app.
add_subdirectory(
  "${RTL87X2J_PLATFORM_LIB_DIR}"
  "${CMAKE_CURRENT_BINARY_DIR}/rtl87x2j_platform_lib"
)
target_link_libraries(app PUBLIC rtl87x2j_platform_lib)
