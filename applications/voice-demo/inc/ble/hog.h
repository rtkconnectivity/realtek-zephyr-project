/*
 * Copyright (c) 2025 Realtek Semiconductor Corp.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <zephyr/kernel.h>
#include <zephyr/sys/util.h>
#include <ble/hid.h>

int hog_send_keyboard_report(uint8_t *keyboard_report);
int hog_send_consumer_report(uint16_t *consumer_report);
int hog_send_voice_report(uint8_t *voice_report);