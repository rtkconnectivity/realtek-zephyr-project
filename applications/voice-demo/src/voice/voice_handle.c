/*
 * Copyright (c) 2025 Realtek Semiconductor Corp.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/init.h>
#include <zephyr/device.h>
#include <zephyr/bluetooth/addr.h>
#include <zephyr/logging/log.h>
#include <config.h>
#include <voice/voice_driver.h>
#include <voice/voice_handle.h>
#include <zephyr/drivers/i2s.h>
#include <zephyr/audio/codec.h>
#include <zephyr/drivers/uart.h>
#include <os_mem.h>
#include <ble/ble.h>
#include <loop_queue.h>
#include "trace.h"
#include "rtl_pinmux.h"
#include "ble/hog.h"
#if (VOICE_ENC_TYPE == SW_SBC_ENC)
#include "voice/sbc.h"
#elif (VOICE_ENC_TYPE == SW_IMA_ADPCM_ENC)
#include "voice/ima_adpcm_enc.h"
#endif
#if IS_ENABLED(CONFIG_VOICE_PPT_MASTER)
#include <ppt/voice_ppt_master.h>
#endif

LOG_MODULE_DECLARE(app, CONFIG_APP_LOG_LEVEL);

static struct k_thread voice_rx_data;
K_THREAD_STACK_DEFINE(voice_rx_stack, 768); /* if stack size < 768, hardfault will occur */
struct k_work voice_work_q;

/*============================================================================*
 *                              Local Variables
 *============================================================================*/
#if (VOICE_ENC_TYPE == SW_MSBC_ENC)
static uint8_t msbc_tog = 0;
#endif
#if (VOICE_FLOW_SEL == ATV_GOOGLE_VOICE_FLOW)
/* voice_exception_timer is used to detect audio transfer and active remote exception*/
TimerHandle_t voice_exception_timer = NULL;
#endif

static T_LOOP_QUEUE_DEF *p_voice_queue;

/*============================================================================*
 *                              Global Variables
 *============================================================================*/
T_VOICE_GLOBAL_DATA voice_global_data;

struct voice_msg {
    uint8_t *buf;
    uint32_t len;
};

K_MSGQ_DEFINE(voice_msgq, sizeof(struct voice_msg), 6, 4);

/*============================================================================*
 *                              Functions Declaration
 *============================================================================*/
static void voice_handle_init_data(void);
static void voice_handle_init_encode_param(void);
static void voice_handle_deinit_encode_param(void);
static bool voice_handle_notify_voice_data(void);
static void voice_handle_encode_raw_data(uint8_t *p_input_data, int32_t input_size,
                                         uint8_t *p_output_data, int32_t *p_output_size);
static void voice_handle_rx_data_old(void);
static void voice_handle_rx_data_callback(struct k_work *item);
#if (VOICE_FLOW_SEL == ATV_GOOGLE_VOICE_FLOW)
static void voice_handle_exception_timer_cb(TimerHandle_t p_timer) DATA_RAM_FUNCTION;
#elif (VOICE_FLOW_SEL == RTK_GATT_VOICE_FLOW)
static void voice_handle_notify_gatt_voice_start(void);
static void voice_handle_notify_gatt_voice_stop(void);
#endif
bool voice_handle_start_mic(void);
void voice_handle_stop_mic(void);
#if (VOICE_FLOW_SEL == ATV_GOOGLE_VOICE_FLOW)
bool voice_handle_atvv_audio_start(ATV_AUDIO_START_REASON reason);
bool voice_handle_atvv_audio_stop(ATV_AUDIO_STOP_REASON reason);
bool voice_handle_atvv_mic_open_error(ATV_MIC_OPEN_ERROR reason);
void voice_handle_atvv_init_data(void);
void voice_handle_atv_audio_sync(void);
#endif

/******************************************************************
 * @brief   Initialize voice handle data.
 * @param   none
 * @return  none
 * @retval  void
 */
void voice_handle_init_data(void)
{
    LOG_DBG("[voice_handle_init_data] init data");
    /* enable notify voice data for test */
    memset(&voice_global_data, 0, sizeof(voice_global_data));
    voice_global_data.is_allowed_to_notify_voice_data = true;
}

/******************************************************************
 * @brief   init voice encode parameters.
 * @param   none
 * @return  none
 * @retval  void
 */
void voice_handle_init_encode_param(void)
{
    voice_global_data.is_encoder_init = false;

#if (VOICE_ENC_TYPE == SW_MSBC_ENC)
	/* mSBC only supports Mono channel with 16KHz sample rate */
	msbc_init();
	voice_global_data.is_encoder_init = true;

#elif (VOICE_ENC_TYPE == SW_SBC_ENC)
    /* bitpool is support to adjust, higher bitpool will get higher voice quality and larger encoded frame size */
    sbc_enc_params.bitpool = BIT_POOL_SIZE;
    sbc_enc_params.blockNumber = SBC_BLOCKS16;
    sbc_enc_params.allocMethod = SBC_ALLOCLOUDNESS;
    sbc_enc_params.subbandNumber = SBC_SUBBANDS8;

    /* just support mono mode */
    sbc_enc_params.channelMode = SBC_MODE_MONO;
    /* config sample rate according to codec parameters */
    sbc_enc_params.samplingFrequency = SBC_FREQU16000;

    LOG_DBG("[voice_handle_init_encode_param] params: %d, %d, %d",
                    sbc_enc_params.bitpool, sbc_enc_params.channelMode, sbc_enc_params.samplingFrequency);
    sbc_init_encoder();

    voice_global_data.is_encoder_init = true;
#elif (VOICE_ENC_TYPE == SW_IMA_ADPCM_ENC)
    memset(&ima_adpcm_global_state, 0, sizeof(ima_adpcm_global_state));
    voice_global_data.is_encoder_init = true;
#endif
}

/******************************************************************
 * @brief   deinit voice encode parameters.
 * @param   none
 * @return  none
 * @retval  void
 */
void voice_handle_deinit_encode_param(void)
{
    if (voice_global_data.is_encoder_init == true)
    {
        voice_global_data.is_encoder_init = false;

#if (VOICE_ENC_TYPE == SW_MSBC_ENC)
        msbc_deinit();
#endif
    }
}

/******************************************************************
 * @brief   voice handle out queue.
 * @param   none
 * @return  result
 * @retval  true or false
 */
bool voice_handle_notify_voice_data(void)
{
    bool result = false;

    if (p_voice_queue == NULL)
    {
        LOG_DBG("[voice_handle_out_queue] Queue buffer is NOT initialiezed!");
        result = false;
    }
    else if (loop_queue_is_empty(p_voice_queue) == true)
    {
        LOG_DBG("[voice_handle_out_queue] Voice Queue is empty.");
        result = false;
    }
    else
    {
        uint32_t queue_item_cnt = loop_queue_get_vailid_data_size(p_voice_queue) / p_voice_queue->item_size;

        LOG_DBG("[voice_handle_out_queue] queue_item_cnt is %d",queue_item_cnt);

        if (loop_queue_is_empty(p_voice_queue) == false)
        {
            /* attempt to send voice data */
#if (VOICE_FLOW_SEL == RTK_GATT_VOICE_FLOW)
            uint8_t notify_data_len = p_voice_queue->item_size + 3;
            uint8_t *p_notify_buffer = k_malloc(notify_data_len);

            if ((p_notify_buffer != NULL) &&
                (true == (loop_queue_copy_buf(p_voice_queue, p_notify_buffer + 3, p_voice_queue->item_size))))
            {
                /* add packet header */
                p_notify_buffer[0] = VOICE_PACKET_TYPE_VOICE_DATA;
                p_notify_buffer[1] = (uint8_t)p_voice_queue->item_size;
                p_notify_buffer[2] = (uint8_t)(p_voice_queue->item_size >> 8);

				int ret = hog_send_voice_report(p_notify_buffer);
                if (ret == 0)
                {
                    LOG_DBG("send voice data over gatt success !");
                    result = loop_queue_read_buf(p_voice_queue, p_notify_buffer, p_voice_queue->item_size);
                } else {
                    LOG_DBG("send voice data over gatt fail err %d",ret);
                }
                k_free(p_notify_buffer);
            }
            else
            {
                LOG_DBG("[voice_handle_out_queue] p_notify_buffer allocate failed!");
            }
#else
#if IS_ENABLED(CONFIG_VOICE_PPT_MASTER)
            uint8_t notify_data_len = p_voice_queue->item_size;
            uint8_t *p_notify_buffer = k_malloc(notify_data_len);

            if ((p_notify_buffer != NULL) &&
                (true == (loop_queue_copy_buf(p_voice_queue, p_notify_buffer, p_voice_queue->item_size))))
            {
                app_ppt_send_voice_data(p_notify_buffer);
                result = loop_queue_read_buf(p_voice_queue, p_notify_buffer, p_voice_queue->item_size);
                k_free(p_notify_buffer);
            }
            else
            {
                result = false;
            }
#elif IS_ENABLED(CONFIG_BT)
            uint8_t notify_data_len = p_voice_queue->item_size;
            uint8_t *p_notify_buffer = k_malloc(notify_data_len);

            if ((p_notify_buffer != NULL) &&
                (true == (loop_queue_copy_buf(p_voice_queue, p_notify_buffer, p_voice_queue->item_size))))
            {
				int ret = hog_send_voice_report(p_notify_buffer);
                if (ret == 0)
                {
                    LOG_DBG("send voice data over gatt success !");
                    result = loop_queue_read_buf(p_voice_queue, p_notify_buffer, p_voice_queue->item_size);
                } else {
                    LOG_DBG("send voice data over gatt fail err %d",ret);
                }
                k_free(p_notify_buffer);
            }
            else
            {
                result = false;
            }
#endif /* CONFIG_VOICE_PPT_MASTER */
#endif /* VOICE_FLOW_SEL */
        }
    }

    return result;
}

/******************************************************************
 * @brief   voice handle encode raw datae.
 * @param   p_input_data - point of input data
 * @param   input_size - input size
 * @param   p_output_data - point of output data
 * @param   p_output_size - point of output size
 * @return  none
 * @retval  void
 */
void voice_handle_encode_raw_data(uint8_t *p_input_data, int32_t input_size,
                                  uint8_t *p_output_data, int32_t *p_output_size)
{
#if (VOICE_ENC_TYPE == SW_MSBC_ENC)
    int encoded = 0;
    msbc_encode(p_input_data, VOICE_PCM_FRAME_SIZE, (uint8_t *)p_output_data + 2,
                VOICE_FRAME_SIZE_AFTER_ENC, &encoded);

    p_output_data[0] = 0x1;
    p_output_data[59] = 0x00;
    if ((msbc_tog & 0x1) == 0)
    {
        p_output_data[1] = 0x8;
    }
    else
    {
        p_output_data[1] = 0xc8;
    }

    msbc_encode(p_input_data + 240, VOICE_PCM_FRAME_SIZE,
                (uint8_t *)p_output_data + 62,
                VOICE_FRAME_SIZE_AFTER_ENC, &encoded);

    p_output_data[60] = 0x1;
    p_output_data[119] = 0x00;
    if ((msbc_tog & 0x1) == 0)
    {
        p_output_data[61] = 0x38;
    }
    else
    {
        p_output_data[61] = 0xf8;
    }
    *p_output_size = 120;
    msbc_tog++;

#elif (VOICE_ENC_TYPE == SW_SBC_ENC)
    uint8_t *p_input_buff = p_input_data;
    uint8_t *p_output_buff = p_output_data;
    int32_t tmp_output_size;
    while (input_size > 0)
    {
        tmp_output_size = VOICE_FRAME_SIZE_AFTER_ENC;
        if (input_size > VOICE_PCM_FRAME_SIZE)
        {
            sbc_encode(p_input_buff, VOICE_PCM_FRAME_SIZE, &sbc_enc_params, p_output_buff, &tmp_output_size);
            p_input_buff += VOICE_PCM_FRAME_SIZE;
            p_output_buff += tmp_output_size;
            *p_output_size += tmp_output_size;
            input_size -= VOICE_PCM_FRAME_SIZE;
        }
        else
        {
            sbc_encode(p_input_buff, input_size, &sbc_enc_params, p_output_buff, &tmp_output_size);

            *p_output_size += tmp_output_size;
            input_size = 0;
        }
    }
#elif (VOICE_ENC_TYPE == SW_IMA_ADPCM_ENC)
    int32_t tmp_size;
    p_output_data[0] = (uint8_t)(ima_adpcm_global_state.seq_id >> 8);
    p_output_data[1] = (uint8_t)(ima_adpcm_global_state.seq_id);
    ima_adpcm_global_state.seq_id++;
    p_output_data[2] = 0;
    p_output_data[3] = (uint8_t)(ima_adpcm_global_state.valprev >> 8);
    p_output_data[4] = (uint8_t) ima_adpcm_global_state.valprev;
    p_output_data[5] = ima_adpcm_global_state.index;

    tmp_size = ima_adpcm_encode((void *)p_input_data, &p_output_data[VOICE_FRAME_HEADER_SIZE],
                                VOICE_PCM_FRAME_SIZE / 2,
                                &ima_adpcm_global_state);
    *p_output_size = tmp_size + VOICE_FRAME_HEADER_SIZE;

    LOG_DBG("[voice_handle_encode_raw_data] *p_output_size = %d", *p_output_size);
#endif
}

/******************************************************************
 * @brief   Application code for voice data process.
 * @param   msg_sub_type - voice sub type msg.
 * @return  none
 * @retval  void
 */
void voice_handle_rx_data_callback(struct k_work *item)
{
    struct voice_msg ev;
    while(k_msgq_get(&voice_msgq, &ev, K_NO_WAIT) == 0) {
        if (!voice_driver_global_data.is_voice_driver_working)
		{
			LOG_DBG("[voice_handle_rx_data_callback] voice driver is not working");
			return;
		}

		int32_t output_size = 0;
		uint8_t encode_output_buffer[p_voice_queue->item_size];

		if (true == voice_global_data.is_pending_to_stop_recording)
		{
			if ((loop_queue_is_empty(p_voice_queue) == true) ||
				(voice_global_data.is_allowed_to_notify_voice_data == false))
			{
				/* voice buffer data has all been sent after voice key up */
#if (VOICE_FLOW_SEL == IFLYTEK_VOICE_FLOW)
				voice_handle_stop_mic();
				key_handle_release_event();
#elif (VOICE_FLOW_SEL == RTK_GATT_VOICE_FLOW)
                voice_handle_stop_mic();
                voice_handle_notify_gatt_voice_stop();
#endif
			}
			else
			{
				voice_handle_notify_voice_data();
			}
		}
		else if (voice_global_data.is_encoder_init == true)
		{
			/* encode raw data */
			voice_handle_encode_raw_data(ev.buf, VOICE_GDMA_FRAME_SIZE, encode_output_buffer,
										&output_size);
            LOG_DBG("[voice_handle_rx_data_callback] output size %d, item_size %d",output_size, p_voice_queue->item_size);
			if (output_size == p_voice_queue->item_size)
			{
#if FEATURE_SUPPORT_UART_DUMP_VOICE_ENCODE_DATA
				if (dev_uart != NULL) {
					for (int32_t i = 0; i < output_size; i++) {
						uart_poll_out(dev_uart, encode_output_buffer[i]);
					}
				}
#endif
				/* set is_overflow_data_abandoned to false, to drop oldest data if queue is full */
				loop_queue_write_buf(p_voice_queue, encode_output_buffer, p_voice_queue->item_size, false);
				if (voice_global_data.is_allowed_to_notify_voice_data == true)
				{
					voice_handle_notify_voice_data();
				}
				else
				{
					LOG_DBG("[voice_handle_rx_data_callback] not allowed to notify voice data");
				}
			}
			else
			{
				LOG_DBG("[voice_handle_rx_data_callback] encode failed %d %d", output_size,
								p_voice_queue->item_size);
			}
		}
		else
		{
			LOG_DBG("[voice_handle_rx_data_callback] Encoder is NOT initialized");
		}
    }
}

static void voice_rx_thread(void *p1, void *p2, void *p3)
{
    LOG_DBG("voice rx thread: ");
    ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);
    while (true) {
        void *mem_block;
        size_t rx_size;
        const struct i2s_config *rx_cfg;
        int ret = i2s_read(dev_i2s, &mem_block, &rx_size);
        if (ret) {
            LOG_ERR("[%s] ret%d line%d\n", __func__, ret, __LINE__);
        } else {
            voice_global_data.voice_data_total_cnt += rx_size;
            memcpy(voice_global_data.voice_data_buf.buf,mem_block,rx_size);
            /* Need to send msg to prevent long app processing time from affecting dma transfer */
            struct voice_msg ev = {
                .buf = voice_global_data.voice_data_buf.buf,
                .len = rx_size
            };
            LOG_DBG("voice recieve data: cur size %d, total size %d",rx_size, voice_global_data.voice_data_total_cnt);
            rx_cfg = i2s_config_get(dev_i2s, I2S_DIR_RX);
            k_mem_slab_free(rx_cfg->mem_slab, mem_block);
            k_msgq_put(&voice_msgq, &ev, K_NO_WAIT);
            k_work_submit(&voice_work_q);
        }
#if FEATURE_SUPPORT_UART_DUMP_VOICE_RAW_DATA
        if (dev_uart != NULL) {
            for (uint32_t i = 0; i < rx_size; i += 8) {
                uart_poll_out(dev_uart, voice_global_data.voice_data_buf.buf[i]);
                uart_poll_out(dev_uart, voice_global_data.voice_data_buf.buf[i + 1]);
                uart_poll_out(dev_uart, voice_global_data.voice_data_buf.buf[i + 2]);
                uart_poll_out(dev_uart, voice_global_data.voice_data_buf.buf[i + 3]);
                uart_poll_out(dev_uart, voice_global_data.voice_data_buf.buf[i + 4]);
                uart_poll_out(dev_uart, voice_global_data.voice_data_buf.buf[i + 5]);
                uart_poll_out(dev_uart, voice_global_data.voice_data_buf.buf[i + 6]);
                uart_poll_out(dev_uart, voice_global_data.voice_data_buf.buf[i + 7]);
            }
        }
#endif
    }
}

void voice_handle_start_rx_data(void)
{
    static bool is_voice_rx_thread_created = false;
    if(!is_voice_rx_thread_created) {
        k_tid_t tid;
        tid = k_thread_create(&voice_rx_data, voice_rx_stack,
                            K_KERNEL_STACK_SIZEOF(voice_rx_stack),
                            voice_rx_thread, NULL, NULL, NULL,
                            K_PRIO_COOP(10), 0, K_NO_WAIT);
        k_thread_name_set(tid, "voice rx thread");
        if(tid) {
            is_voice_rx_thread_created = true;
        }
    } else {
        k_thread_resume(&voice_rx_data);
    }
}
#if (VOICE_FLOW_SEL == RTK_GATT_VOICE_FLOW)
/******************************************************************
 * @brief   voice handle notify GATT voice start status.
 * @param   none
 * @return  none
 * @retval  void
 */
void voice_handle_notify_gatt_voice_start(void)
{
#define GATT_VOICE_CTL_PACKET_LEN 6

    uint16_t param_len = 2;
    uint8_t notify_data_buff[GATT_VOICE_CTL_PACKET_LEN];
    int send_result;

    notify_data_buff[0] = VOICE_PACKET_TYPE_VOICE_CTRL;  /* packet type */
    notify_data_buff[1] = (uint8_t)param_len;  /* packet parameter length */
    notify_data_buff[2] = (uint8_t)(param_len >> 8);
    notify_data_buff[3] = GATT_VOICE_START;  /* packet command */
    /* packet parameter section */
#if (VOICE_ENC_TYPE == SW_SBC_ENC)
    notify_data_buff[4] = VOICE_GATT_CODEC_SBC;
    notify_data_buff[5] = BIT_POOL_SIZE;
#else
    notify_data_buff[4] = VOICE_GATT_CODEC_INVALID;
    notify_data_buff[5] = 0;
#endif

    send_result = hog_send_voice_report(notify_data_buff);

    if (send_result != 0)
    {
        LOG_ERR("[voice_handle_notify_gatt_voice_start] send data failed!");
    }
}

/******************************************************************
 * @brief   voice handle notify GATT voice stop status.
 * @param   none
 * @return  none
 * @retval  void
 */
void voice_handle_notify_gatt_voice_stop(void)
{
    uint16_t param_len = 2;
    uint8_t notify_data_buff[GATT_VOICE_CTL_PACKET_LEN];
    int send_result;

    notify_data_buff[0] = VOICE_PACKET_TYPE_VOICE_CTRL;  /* packet type */
    notify_data_buff[1] = (uint8_t)param_len;  /* packet parameter length */
    notify_data_buff[2] = (uint8_t)(param_len >> 8);
    notify_data_buff[3] = GATT_VOICE_STOP;  /* packet command */
    /* packet parameter section */
#if (VOICE_ENC_TYPE == SW_SBC_ENC)
    notify_data_buff[4] = VOICE_GATT_CODEC_SBC;
    notify_data_buff[5] = BIT_POOL_SIZE;
#else
    notify_data_buff[4] = VOICE_GATT_CODEC_INVALID;
    notify_data_buff[5] = 0;
#endif

    send_result = hog_send_voice_report(notify_data_buff);

    if (send_result != 0)
    {
        LOG_ERR("[voice_handle_notify_gatt_voice_stop] send data failed!");
    }
}
#endif

/******************************************************************
 * @brief   voice handle start mic and recording.
 * @param   none
 * @return  result
 * @retval  true or false
 */
bool voice_handle_start_mic(void)
{
    uint16_t item_size = 0;
    uint16_t queue_size = 0;

    if (voice_driver_global_data.is_voice_driver_working == true)
    {
        LOG_DBG("[voice_handle_start_mic] Voice driver is working, start failed!");
        return false;
    }

    LOG_DBG("[voice_handle_start_mic] start recording!");
    k_work_init(&voice_work_q, voice_handle_rx_data_callback);

    voice_handle_init_data();
    voice_handle_init_encode_param();
    LOG_DBG("after voice_handle_init_encode_param");

    item_size = VOICE_REPORT_FRAME_SIZE;
    if (item_size == 0)
    {
        LOG_DBG("[voice_handle_start_mic] Invalid item_size!");
        return false;
    }
    queue_size = VOICE_QUEUE_MAX_BUFFER_SIZE / item_size;
    p_voice_queue = loop_queue_init(queue_size, item_size);
    LOG_DBG("after loop_queue_init");
    voice_driver_init();

#if SUPPORT_SW_EQ
    bq_init();
#endif
    return true;
}

/******************************************************************
 * @brief   voice handle stop mic and recording.
 * @param   none
 * @return  result
 * @retval  true or false
 */
void voice_handle_stop_mic(void)
{
    LOG_DBG("stop recording!");
    k_thread_suspend(&voice_rx_data);

    if (voice_driver_global_data.is_voice_driver_working == false)
    {
        LOG_DBG("Voice driver is not working, stop failed!");
    }
    else
    {
        voice_driver_deinit();
        voice_handle_deinit_encode_param();
        loop_queue_deinit(&p_voice_queue);
    }
}

/******************************************************************
 * @brief   vocie key press msg handle.
 * @param   none
 * @return  result
 * @retval  true or false
 */
bool voice_handle_mic_key_pressed(void)
{
	LOG_DBG("[voice_handle_mic_key_pressed]");
    bool ret = true;

    if (true == voice_driver_global_data.is_voice_driver_working)
    {
        LOG_DBG("[voice_handle_mic_key_pressed] Voice driver is Working, start recording failed!");
        return false;
    }

#if (VOICE_FLOW_SEL == IFLYTEK_VOICE_FLOW)
    voice_handle_start_mic();
    // key_handle_notify_hid_key_event_by_index(VK_VOICE);
    voice_handle_start_rx_data();
#elif (VOICE_FLOW_SEL == RTK_GATT_VOICE_FLOW)
    voice_handle_start_mic();
    voice_handle_notify_gatt_voice_start();
    voice_global_data.is_allowed_to_notify_voice_data = true;
#endif
	return ret;
}

/******************************************************************
 * @brief   vocie key release msg handle.
 * @param   none
 * @return  none
 * @retval  void
 */
void voice_handle_mic_key_released(void)
{
	LOG_DBG("[voice_handle_mic_key_released]");
    if (voice_driver_global_data.is_voice_driver_working == false)
    {
        LOG_DBG("[voice_handle_mic_key_released] Voice driver is not working!");
        return;
    }

#if (VOICE_FLOW_SEL == IFLYTEK_VOICE_FLOW)
    if (loop_queue_is_empty(p_voice_queue) ||
        (false == voice_global_data.is_allowed_to_notify_voice_data))
    {
        /* stop voice recording immediately */
        LOG_DBG("stop mic when voice key released");
        voice_handle_stop_mic();
        key_handle_release_event();
    }
    else
    {
        /* stop voice in queue, and wait for all buffered voice data send */
        voice_global_data.is_pending_to_stop_recording = true;
    }
#elif (VOICE_FLOW_SEL == RTK_GATT_VOICE_FLOW)
    if (loop_queue_is_empty(p_voice_queue) ||
        (false == voice_global_data.is_allowed_to_notify_voice_data) ||
        (p_voice_queue == NULL))
    {
        /* stop voice recording immediately */
        voice_handle_stop_mic();
        voice_handle_notify_gatt_voice_stop();
    }
    else
    {
        /* stop voice in queue, and wait for all buffered voice data send */
        voice_global_data.is_pending_to_stop_recording = true;
    }
#endif
}
