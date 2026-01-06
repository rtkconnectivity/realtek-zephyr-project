/*
 * Copyright (c) 2025 Realtek Semiconductor Corp.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdint.h>
#include <zephyr/kernel.h>
#include <mem_types.h>

/*============================================================================*
 *                              Types
 *============================================================================*/
typedef void *T_LOOP_QUEUE_BUF;

/**
 * @brief Loop queue data struct
 */
typedef struct
{
    volatile bool               is_over_flow;       /* loop queue item size */
    volatile uint16_t           item_size;          /* buf data type */
    volatile uint16_t           buf_max_item_num;   /* Buffer size of loop queue */
    volatile uint16_t           read_index;         /* index of read queue */
    volatile uint16_t           write_index;        /* index of write queue */
    volatile T_LOOP_QUEUE_BUF   p_buf;              /* Buffer for loop queue */
} T_LOOP_QUEUE_DEF;

/*============================================================================*
 *                         Functions
 *============================================================================*/
T_LOOP_QUEUE_DEF *loop_queue_init(uint16_t buf_max_size, uint16_t item_size);
void loop_queue_deinit(T_LOOP_QUEUE_DEF **p_loop_queue);
bool loop_queue_is_full(T_LOOP_QUEUE_DEF *p_queue_struct, uint16_t write_size);
bool loop_queue_is_empty(T_LOOP_QUEUE_DEF *p_queue_struct);
uint16_t loop_queue_get_vailid_data_size(T_LOOP_QUEUE_DEF *p_queue_struct);
uint16_t loop_queue_get_free_data_size(T_LOOP_QUEUE_DEF *p_loop_queue);
bool loop_queue_write_buf(T_LOOP_QUEUE_DEF *p_queue_struct, T_LOOP_QUEUE_BUF p_buf, uint16_t size,
                          bool is_overflow_data_abandoned);
bool loop_queue_read_buf(T_LOOP_QUEUE_DEF *p_queue_struct, T_LOOP_QUEUE_BUF p_buf, uint16_t size);
bool loop_queue_copy_buf(T_LOOP_QUEUE_DEF *p_queue_struct, T_LOOP_QUEUE_BUF p_buf, uint16_t size);