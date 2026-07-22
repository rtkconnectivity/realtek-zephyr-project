/*
 * Copyright (c) 2025 Realtek Semiconductor Corp.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <zephyr/kernel.h>
#include <zephyr/sys/util.h>

/*============================================================================*
 *                              Macros
 *============================================================================*/

/**
 * VOICE Module config
 */
#define SUPPORT_VOICE_FEATURE               1  /* set 1 to support voice feature */
#define FEATURE_SUPPORT_UART_DUMP_VOICE_RAW_DATA 1 /* set 1 to support uart dump voice feature */

#if SUPPORT_VOICE_FEATURE

#define SUPPORT_MIC_BIAS_OUTPUT             1  /* set 1 to enable MIC bias output */

#define SUPPORT_UART_DUMP_FEATURE           0  /* set 1 to support UART dump PCM data */

#define SUPPORT_SW_EQ                       0  /* set 1 to support software equalizer */

#if SUPPORT_UART_DUMP_FEATURE
#define VOICE_UART_TEST_TX          P3_1
#endif

/* mic type definitions */
#define AMIC_TYPE                   0  /* Analog MIC */
#define DMIC_TYPE                   1  /* digital MIC */

/* amic input type definitions */
#define AMIC_INPUT_TYPE_DIFF        0  /* differential analog input */
#define AMIC_INPUT_TYPE_SINGLE      1  /* single-end analog input */

/* dmic data latch type definitions */
#define DMIC_DATA_LATCH_FALLING_EDGE    0  /* falling clock edge */
#define DMIC_DATA_LATCH_RISING_EDGE     1  /* rising clock edge */

/* codec sample rate type definitions */
#define CODEC_SAMPLE_RATE_8KHz        1  /* 8KHz codec sample rate */
#define CODEC_SAMPLE_RATE_16KHz       2  /* 16KHz codec sample rate */

#define CODEC_SAMPLE_RATE_SEL       CODEC_SAMPLE_RATE_16KHz

#define VOICE_MIC_TYPE             AMIC_TYPE

#if (VOICE_MIC_TYPE == AMIC_TYPE)
#define AMIC_MIC_N_PIN              P2_6  /* MIC_N is fixed to P2_6 */
#define AMIC_MIC_P_PIN              P2_7  /* MIC_P is fixed to P2_7 */
#define AMIC_MIC_BIAS_PIN           H_0   /* MICBIAS is fixed to H_0 */
#define AMIC_INPUT_TYPE_SEL         AMIC_INPUT_TYPE_DIFF
#elif (VOICE_MIC_TYPE == DMIC_TYPE)
#if (RCU_HD_PLATFORM_SEL == H_DEMO_RCU)
#define DMIC_CLK_PIN                P2_6  /* DMIC clock PIN, support PINMUX */
#define DMIC_DATA_PIN               P2_7  /* DMIC data PIN, support PINMUX */
#else
#define DMIC_CLK_PIN                P2_0  /* DMIC clock PIN, support PINMUX */
#define DMIC_DATA_PIN               P2_1  /* DMIC data PIN, support PINMUX */
#endif
#define DMIC0_DATA_LATCH_TYPE       DMIC_DATA_LATCH_FALLING_EDGE
#endif

/* voice flow type, default IFLYTEK_VOICE_FLOW */
#define IFLYTEK_VOICE_FLOW          0
#define HIDS_GOOGLE_VOICE_FLOW      1
#define ATV_GOOGLE_VOICE_FLOW       2
#define RTK_GATT_VOICE_FLOW         3

#define VOICE_FLOW_SEL              IFLYTEK_VOICE_FLOW

/* voice encode type */
#define SW_MSBC_ENC                 1  /* software msbc encode */
#define SW_SBC_ENC                  2  /* software sbc encode */
#define SW_IMA_ADPCM_ENC            3  /* software IMA/DVI adpcm encode */

/* voice encode config */
#if ((VOICE_FLOW_SEL == IFLYTEK_VOICE_FLOW) || (VOICE_FLOW_SEL == HIDS_GOOGLE_VOICE_FLOW))
#define VOICE_ENC_TYPE              SW_MSBC_ENC

#elif (VOICE_FLOW_SEL == ATV_GOOGLE_VOICE_FLOW)
/* ATV_GOOGLE_VOICE_FLOW must use SW_IMA_ADPCM_ENC */
#define VOICE_ENC_TYPE              SW_IMA_ADPCM_ENC

#elif (VOICE_FLOW_SEL == RTK_GATT_VOICE_FLOW)
#define VOICE_ENC_TYPE              SW_SBC_ENC
#endif

/* ATV Google voice config */
#if (VOICE_FLOW_SEL == ATV_GOOGLE_VOICE_FLOW)
/* ATV Google voice version type */
#define ATV_VERSION_0_4             1  /* v0.4 */
#define ATV_VERSION_1_0             2  /* v1.0 */

/* ATV Google voice assistant interaction type */
#define ATV_INTERACTION_MODEL_ON_REQUEST            1  /* on request */
#define ATV_INTERACTION_MODEL_PRESS_TO_TALK         2  /* PTT */
#define ATV_INTERACTION_MODEL_HOLD_TO_TALK          3  /* HTT */

#define ATV_VOICE_VERSION           ATV_VERSION_1_0  /* default version is v1.0 */
#define ATV_VOICE_INTERACTION_MODEL ATV_INTERACTION_MODEL_HOLD_TO_TALK  /* default assistant interaction is HTT */
#endif

#endif