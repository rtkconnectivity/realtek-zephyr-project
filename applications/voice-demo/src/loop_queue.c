/*
 * Copyright (c) 2025 Realtek Semiconductor Corp.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/bluetooth/addr.h>
#include <zephyr/logging/log.h>
#include <config.h>
#include <loop_queue.h>
#include <mem_types.h>
#include <os_mem.h>
#include "trace.h"
#include "rtl_pinmux.h"

LOG_MODULE_DECLARE(app, CONFIG_APP_LOG_LEVEL);

/*============================================================================*
 *                              Functions Declaration
 *============================================================================*/
bool loop_queue_is_full(T_LOOP_QUEUE_DEF *p_queue_struct, uint16_t write_size);
bool loop_queue_is_empty(T_LOOP_QUEUE_DEF *p_queue_struct);
uint16_t loop_queue_get_vailid_data_size(T_LOOP_QUEUE_DEF *p_queue_struct);
uint16_t loop_queue_get_free_data_size(T_LOOP_QUEUE_DEF *p_loop_queue);
bool loop_queue_write_buf(T_LOOP_QUEUE_DEF *p_queue_struct, T_LOOP_QUEUE_BUF p_buf, uint16_t size,
                          bool is_overflow_data_abandoned);
bool loop_queue_read_buf(T_LOOP_QUEUE_DEF *p_queue_struct, T_LOOP_QUEUE_BUF p_buf,
                         uint16_t size);
bool loop_queue_copy_buf(T_LOOP_QUEUE_DEF *p_queue_struct, T_LOOP_QUEUE_BUF p_buf,
                         uint16_t size);

/*============================================================================*
 *                              Global Functions
 *============================================================================*/
/******************************************************************
 * @brief   Initializes loop queue to their default reset values.
 * @param   buf_max_size - loop queue buffer max size.
 * @param   item_size - queue item size.
 * @param   ram_type - RAM_TYPE_DATA_ON or RAM_TYPE_BUFFER_ON.
 * @retval  void*
 */
T_LOOP_QUEUE_DEF *loop_queue_init(uint16_t item_cnt, uint16_t item_size)
{
    if ((item_cnt == 0) && (item_size == 0))
    {
        LOG_DBG("[loop queue] buf_max_size is invalid!");
        return NULL;
    }
    T_LOOP_QUEUE_DEF *p_loop_queue = NULL;
    p_loop_queue = k_malloc(sizeof(T_LOOP_QUEUE_DEF));
    if (p_loop_queue != NULL)
    {
        uint32_t buf_max_size = item_cnt * item_size;
        memset(p_loop_queue, 0, sizeof(T_LOOP_QUEUE_DEF));
        p_loop_queue->item_size = item_size;
        p_loop_queue->buf_max_item_num = item_cnt;
        p_loop_queue->is_over_flow = false;
        p_loop_queue->p_buf = NULL;
        p_loop_queue->p_buf = k_malloc(buf_max_size);

        if (p_loop_queue->p_buf != NULL)
        {
            memset((uint8_t *)p_loop_queue->p_buf, 0, buf_max_size);
        }
        else
        {
            k_free(p_loop_queue);
            p_loop_queue = NULL;
        }
    }
    return p_loop_queue;
}

/******************************************************************
 * @brief   Deinitializes loop queue
 * @param   p_loop_queue - point to loop queue which needs deinitialization.
 * @return  none
 * @retval  void
 */
void loop_queue_deinit(T_LOOP_QUEUE_DEF **p_loop_queue)
{
    if (*p_loop_queue != NULL)
    {
        if ((*p_loop_queue)->p_buf != NULL)
        {
            k_free((*p_loop_queue)->p_buf);
            (*p_loop_queue)->p_buf = NULL;
        }
        k_free((*p_loop_queue));
        *p_loop_queue = NULL;
    }
}

/******************************************************************
 * @brief   Get valid data length of loop queue.
 * @param   p_loop_queue - point to loop queue dta struct.
 * @return  valid data size of loop queue.
 * @retval  uint16_t
 */
uint16_t loop_queue_get_vailid_data_size(T_LOOP_QUEUE_DEF *p_loop_queue)
{
    if (p_loop_queue == NULL)
    {
        LOG_DBG("[loop queue] pointer is invalid!");
        return 0;
    }

    if (p_loop_queue->write_index == p_loop_queue->read_index)
    {
        if (p_loop_queue->is_over_flow == true)
        {
            return (p_loop_queue->item_size * p_loop_queue->buf_max_item_num);
        }
        else
        {
            return 0;
        }
    }
    else
    {
        return (p_loop_queue->item_size * ((p_loop_queue->buf_max_item_num + p_loop_queue->write_index -
                                            p_loop_queue->read_index) % p_loop_queue->buf_max_item_num));
    }
}

/******************************************************************
 * @brief   Get free data length of loop queue.
 * @param   p_queue_struct - point to loop queue dta struct.
 * @return  free data size of loop queue.
 * @retval  uint16_t
 */
uint16_t loop_queue_get_free_data_size(T_LOOP_QUEUE_DEF *p_loop_queue)
{
    if (p_loop_queue == NULL)
    {
        LOG_DBG("[loop queue] pointer is invalid!");
        return 0;
    }

    if (p_loop_queue->write_index == p_loop_queue->read_index)
    {
        if (p_loop_queue->is_over_flow == true)
        {
            return 0;
        }
        else
        {
            return (p_loop_queue->item_size * p_loop_queue->buf_max_item_num);
        }
    }
    else
    {
        return (p_loop_queue->item_size * ((p_loop_queue->buf_max_item_num - p_loop_queue->write_index +
                                            p_loop_queue->read_index) % p_loop_queue->buf_max_item_num));
    }
}

/******************************************************************
 * @brief   check loop queue if will full or not.
 * @param   p_loop_queue - point to loop queue dta struct.
 * @return  loop queue is full or not
 * @retval  TRUE - full
 * @retval  FALSE - not full
 */
bool loop_queue_is_full(T_LOOP_QUEUE_DEF *p_loop_queue, uint16_t write_size)
{
    if (p_loop_queue == NULL)
    {
        LOG_DBG("[loop queue] pointer is invalid!");
        return true;
    }
    if (write_size % p_loop_queue->item_size != 0)
    {
        LOG_DBG("[loop queue] buf_max_size is invalid!");
        return true;
    }

    if (p_loop_queue->is_over_flow == true)
    {
        LOG_DBG("[loop queue] Loop Queue is already full!");
        return true;
    }
    else if (write_size >= loop_queue_get_free_data_size(p_loop_queue))
    {
        LOG_DBG("[loop queue] Loop Queue will over flow!");
        return true;
    }

    return false;
}

/******************************************************************
 * @brief   check loop queue if empty or not.
 * @param   p_loop_queue - point to loop queue dta struct.
 * @return  loop queue is empty or not
 * @retval  TRUE - empty
 * @retval  FALSE - empty full
 */
bool loop_queue_is_empty(T_LOOP_QUEUE_DEF *p_loop_queue)
{
    if (p_loop_queue == NULL)
    {
        LOG_DBG("[loop queue] pointer is invalid!");
        return false;
    }

    if (p_loop_queue->is_over_flow == false && p_loop_queue->write_index == p_loop_queue->read_index)
    {
        return true;
    }

    return false;
}

/******************************************************************
 * @brief   Write source buffer data to loop queue.
 * @param   p_loop_queue - point to loop queue data struct.
 * @param   p_write_buf - point to write buffer.
 * @param   size - size of data to be written.
 * @return  result
 * @retval  true or false
 */
bool loop_queue_write_buf(T_LOOP_QUEUE_DEF *p_loop_queue, T_LOOP_QUEUE_BUF p_write_buf,
                          uint16_t size, bool is_overflow_data_abandoned)
{
    uint16_t queue_buf_cur_idx = 0;
    uint16_t write_buf_cur_idx = 0;
    uint16_t remain_len = 0;
    uint16_t write_len = 0;
    uint16_t data_size = 0;

    /* Check parameters */
    if (p_write_buf == NULL || p_loop_queue == NULL || p_loop_queue->p_buf == NULL)
    {
        LOG_DBG("[loop queue] pointer is invalid!");
        return false;
    }

    if (size <= 0 || size % p_loop_queue->item_size != 0)
    {
        LOG_DBG("[loop queue] write data size is invalid!");
        return false;
    }

    if (!loop_queue_is_full(p_loop_queue, size))
    {
        remain_len = size / p_loop_queue->item_size;
    }
    else
    {
        if (is_overflow_data_abandoned == true)
        {
            remain_len = loop_queue_get_free_data_size(p_loop_queue) / p_loop_queue->item_size;
        }
        else
        {
            remain_len = size / p_loop_queue->item_size;
        }
        p_loop_queue->is_over_flow = true;
    }

    while (remain_len)
    {
        if (p_loop_queue->write_index + remain_len <= p_loop_queue->buf_max_item_num)
        {
            write_len = remain_len;
        }
        else
        {
            write_len = p_loop_queue->buf_max_item_num - p_loop_queue->write_index;
        }

        queue_buf_cur_idx = p_loop_queue->item_size * p_loop_queue->write_index;
        data_size = p_loop_queue->item_size * write_len;
        memcpy((uint8_t *)p_loop_queue->p_buf + queue_buf_cur_idx,
               (uint8_t *)p_write_buf + write_buf_cur_idx, data_size);
        write_buf_cur_idx += data_size;
        p_loop_queue->write_index = (p_loop_queue->write_index + write_len) %
                                    p_loop_queue->buf_max_item_num;
        remain_len -= write_len;
    }

    if (p_loop_queue->is_over_flow == true)
    {
        p_loop_queue->read_index = p_loop_queue->write_index;
    }

    return true;
}

/******************************************************************
 * @brief   Read data from loop queue.
 * @param   p_queue_struct - point to loop queue data struct.
 * @param   p_read_buf - point to read buffer
 * @param   size - size of data to be read.
 * @return  result
 * @retval  true or false
 */
bool loop_queue_read_buf(T_LOOP_QUEUE_DEF *p_loop_queue, T_LOOP_QUEUE_BUF p_read_buf,
                         uint16_t size)
{
    uint16_t queue_buf_cur_idx = 0;
    uint16_t read_buf_cur_idx = 0;
    uint16_t remain_len = 0;
    uint16_t read_len = 0;
    uint16_t data_size = 0;

    /* Check parameters */
    if (p_read_buf == NULL || p_loop_queue == NULL || p_loop_queue->p_buf == NULL)
    {
        LOG_DBG("[loop queue] pointer is invalid!");
        return false;
    }

    if (size <= 0 || size % p_loop_queue->item_size != 0)
    {
        LOG_DBG("[loop queue] read data size is invalid!");
        return false;
    }

    if (loop_queue_get_vailid_data_size(p_loop_queue) < size)
    {
        LOG_DBG("[loop queue] there is not enough data to read!");
        return false;
    }

    remain_len = size / p_loop_queue->item_size;

    uint32_t s;
    /* Enter the critical section */
    s = irq_lock();

    while (remain_len)
    {
        if (p_loop_queue->read_index + remain_len <= p_loop_queue->buf_max_item_num)
        {
            read_len = remain_len;
        }
        else
        {
            read_len = p_loop_queue->buf_max_item_num - p_loop_queue->read_index;
        }

        queue_buf_cur_idx = p_loop_queue->item_size * p_loop_queue->read_index;
        data_size = p_loop_queue->item_size * read_len;
        memcpy((uint8_t *)p_read_buf + read_buf_cur_idx, (uint8_t *)p_loop_queue->p_buf + queue_buf_cur_idx,
               data_size);
        read_buf_cur_idx += data_size;
        p_loop_queue->read_index = (p_loop_queue->read_index + read_len) % p_loop_queue->buf_max_item_num;
        remain_len -= read_len;
    }

    p_loop_queue->is_over_flow = false;

    /* Exit the critical section */
    irq_unlock(s);

    return true;
}

/******************************************************************
 * @brief   copy buffer data from loop queue.
 * @param   p_loop_queue - point to loop queue data struct.
 * @param   p_copy_buf - point to copy buffer
 * @param   size - size of data to be copy.
 * @return  result
 * @retval  true or false
 */
bool loop_queue_copy_buf(T_LOOP_QUEUE_DEF *p_loop_queue, T_LOOP_QUEUE_BUF p_copy_buf,
                         uint16_t size)
{
    uint16_t queue_buf_cur_idx = 0;
    uint16_t copy_buf_cur_idx = 0;
    uint16_t remain_len = 0;
    uint16_t copy_len = 0;
    uint16_t data_size = 0;
    uint16_t copy_index = 0;

    /* Check parameters */
    if (p_copy_buf == NULL || p_loop_queue == NULL || p_loop_queue->p_buf == NULL)
    {
        LOG_DBG("[loop queue] pointer is invalid!");
        return false;
    }

    if (size <= 0 || size % p_loop_queue->item_size != 0)
    {
        LOG_DBG("[loop queue] read data size is invalid!");
        return false;
    }

    uint32_t key;
    /* Enter the critical section */
    key = irq_lock();

    if (loop_queue_get_vailid_data_size(p_loop_queue) < size)
    {
        LOG_DBG("[loop queue] there is not enough data to copy!");
        return false;
    }

    remain_len = size / p_loop_queue->item_size;
    copy_index = p_loop_queue->read_index;

    while (remain_len)
    {
        if (copy_index + remain_len <= p_loop_queue->buf_max_item_num)
        {
            copy_len = remain_len;
        }
        else
        {
            copy_len = p_loop_queue->buf_max_item_num - copy_index;
        }

        queue_buf_cur_idx = p_loop_queue->item_size * copy_index;
        data_size = p_loop_queue->item_size * copy_len;
        memcpy((uint8_t *)p_copy_buf + copy_buf_cur_idx, (uint8_t *)p_loop_queue->p_buf + queue_buf_cur_idx,
               data_size);
        copy_buf_cur_idx += data_size;
        copy_index = (copy_index + copy_len) % p_loop_queue->buf_max_item_num;
        remain_len -= copy_len;
    }

    /* Exit the critical section */
    irq_unlock(key);

    return true;
}