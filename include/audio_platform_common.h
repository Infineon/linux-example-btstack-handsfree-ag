/*
* Copyright 2022, Cypress Semiconductor Corporation (an Infineon company) or
* an affiliate of Cypress Semiconductor Corporation.  All rights reserved.
*
* This software, including source code, documentation and related
* materials ("Software") is owned by Cypress Semiconductor Corporation
* or one of its affiliates ("Cypress") and is protected by and subject to
* worldwide patent protection (United States and foreign),
* United States copyright laws and international treaty provisions.
* Therefore, you may use this Software only as provided in the license
* agreement accompanying the software package from which you
* obtained this Software ("EULA").
* If no EULA applies, Cypress hereby grants you a personal, non-exclusive,
* non-transferable license to copy, modify, and compile the Software
* source code solely for use in connection with Cypress's
* integrated circuit products.  Any reproduction, modification, translation,
* compilation, or representation of this Software except as specified
* above is prohibited without the express written permission of Cypress.
*
* Disclaimer: THIS SOFTWARE IS PROVIDED AS-IS, WITH NO WARRANTY OF ANY KIND,
* EXPRESS OR IMPLIED, INCLUDING, BUT NOT LIMITED TO, NONINFRINGEMENT, IMPLIED
* WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. Cypress
* reserves the right to make changes to the Software without notice. Cypress
* does not assume any liability arising out of the application or use of the
* Software or any product or circuit described in the Software. Cypress does
* not authorize its products for use in any products where a malfunction or
* failure of the Cypress product may reasonably be expected to result in
* significant property damage, injury or death ("High Risk Product"). By
* including Cypress's product in a High Risk Product, the manufacturer
* of such system or application assumes all risk of such use and in doing
* so agrees to indemnify Cypress against all liability.
*/
/******************************************************************************
 * File Name: audio_platform_common.h
 *
 * Description: This file contains the of data types and wrapper function for
 * Linux platform specific audio framework (ALSA).
 *
 * Related Document: See README.md
 *
 ******************************************************************************/
#ifndef AUDIO_PLATFORM_COMMON_H_
#define AUDIO_PLATFORM_COMMON_H_

/*******************************************************************************
*      INCLUDES
*******************************************************************************/
#include <stdio.h>
#include "wiced_memory.h"

/*******************************************************************************
*       MACROS
*******************************************************************************/
/* #define AUDIO_TESTING */

/*******************************************************************************
*       STRUCTURES AND ENUMERATIONS
*******************************************************************************/
typedef struct playback_config_params_tag
{
    int16_t sampling_freq;        /*16k, 32k, 44.1k or 48k*/
    int16_t channel_mode;         /*mono, dual, streo or joint streo*/
    int16_t num_of_subbands;       /*4 or 8*/
    int16_t num_of_channels;
    int16_t num_of_blocks;         /*4, 8, 12 or 16*/
    int16_t allocation_method;    /*loudness or SNR*/
    int16_t bit_pool;
} playback_config_params;

/*******************************************************************************
*       FUNCTION DEFINITIONS
*******************************************************************************/
void init_audio(playback_config_params pb_config_params);

void deinit_audio(void);

void alsa_write_pcm_data(uint8_t* p_rx_media, uint16_t media_len);

void alsa_set_volume(uint8_t volume);

#endif /* AUDIO_PLATFORM_COMMON_H_ */
