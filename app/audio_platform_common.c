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
/*******************************************************************************
 * File Name: audio_platform_common.c
 *
 * Description: This file contains the of wrapper function implementation for
 * Linux platform specific audio framework (ALSA).
 *
 * Related Document: See README.md
 *
 ******************************************************************************/

/*******************************************************************************
 *      INCLUDES
 ******************************************************************************/
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "alsa/asoundlib.h"
#include "audio_platform_common.h"
#include "sbc_decoder.h"
#include "sbc_dec_func_declare.h"
#include "sbc_dct.h"
#include "sbc_types.h"
#include "wiced_bt_trace.h"

/*******************************************************************************
 *       MACROS
 ******************************************************************************/
#define MSBC_STATIC_MEM_SIZE      (1920U) /* BYTES */
#define MSBC_SCRATCH_MEM_SIZE     (2048U) /* BYTES */
#define ALSA_LATENCY              (80000U) /* value based on audio playback
                                            * testing for better audio*/

/*******************************************************************************
 *       VARIABLE DEFINITIONS
 ******************************************************************************/
/* MSBC Static memory */
static SINT32 static_mem[MSBC_STATIC_MEM_SIZE / sizeof (SINT32)];
/* MSBC Scratch memory */
static SINT32 scratch_mem[MSBC_SCRATCH_MEM_SIZE / sizeof (SINT32)];
static char *alsa_device = "default";
static int  PcmBytesPerFrame;
static  SBC_DEC_PARAMS  strDecParams = { 0 };
snd_pcm_t *p_alsa_handle = NULL;
snd_pcm_t *p_alsa_capture_handle = NULL; /* Capture Handle */
snd_pcm_hw_params_t *params; /* sound pcm hardware params */
snd_pcm_uframes_t frames;
uint16_t sample_rate = 0;
snd_pcm_format_t format;
static snd_mixer_elem_t* snd_mixer_elem = NULL;
static snd_mixer_t *snd_mixer_handle = NULL;
static snd_mixer_selem_id_t *snd_sid = NULL;
static long vol_max;
snd_pcm_uframes_t buffer_size = 0;
snd_pcm_uframes_t period_size = 0;

/*******************************************************************************
 *       FUNCTION DECLARATION
 ******************************************************************************/
static void alsa_volume_driver_deinit(void);

/*******************************************************************************
 *       FUNCTION DEFINITION
 ******************************************************************************/

/*******************************************************************************
 * function name: alsa_volume_driver_deinit
 *******************************************************************************
 * summary:
 *   de-initializes the alsa volume driver
 *
 * parameters:
 *   none
 *
 * return:
 *   none
 *
 ******************************************************************************/
static void alsa_volume_driver_deinit(void)
{
    if (snd_mixer_handle != NULL)
    {
        snd_mixer_close(snd_mixer_handle);
        snd_mixer_handle = NULL;
    }
    if (snd_sid != NULL)
    {
        snd_mixer_selem_id_free(snd_sid);
        snd_sid = NULL;
    }
    snd_mixer_elem = NULL;
}

/*******************************************************************************
 * Function Name: alsa_volume_driver_init
 *******************************************************************************
 * Summary:
 *   Initializes the ALSA Volume Driver
 *
 * Parameters:
 *   None
 *
 * Return:
 *   None
 *
 ******************************************************************************/
static void alsa_volume_driver_init(void)
{
    long vol_min;
    WICED_BT_TRACE("alsa_volume_driver_init\n");

    alsa_volume_driver_deinit();

    snd_mixer_open(&snd_mixer_handle, 0);
    if (snd_mixer_handle == NULL)
    {
        WICED_BT_TRACE("alsa_volume_driver_init snd_mixer_open Failed\n");
        return;
    }
    snd_mixer_attach(snd_mixer_handle, "default");
    snd_mixer_selem_register(snd_mixer_handle, NULL, NULL);
    snd_mixer_load(snd_mixer_handle);

    snd_mixer_selem_id_malloc(&snd_sid);
    if (snd_sid == NULL)
    {
        alsa_volume_driver_deinit();
        WICED_BT_TRACE("alsa_volume_driver_init snd_mixer_selem_id_alloca Failed\n");
        return;
    }
    snd_mixer_selem_id_set_index(snd_sid, 0);
    snd_mixer_selem_id_set_name(snd_sid, "Master");

    snd_mixer_elem = snd_mixer_find_selem(snd_mixer_handle, snd_sid);

    if (snd_mixer_elem)
    {
        snd_mixer_selem_get_playback_volume_range(snd_mixer_elem, &vol_min, &vol_max);
        WICED_BT_TRACE("min volume %ld max volume %ld\n", vol_min, vol_max);
    }
    else
    {
        alsa_volume_driver_deinit();
        WICED_BT_TRACE("alsa_volume_driver_init snd_mixer_find_selem Failed\n");
    }
}

/*******************************************************************************
 * Function Name: alsa_set_volume
 *******************************************************************************
 * Summary:
 *   Sets the required volume level in the ALSA driver
 *
 * Parameters:
 *   Required volume
 *
 * Return:
 *   status code
 *
 ******************************************************************************/
void alsa_set_volume(uint8_t volume)
{
    WICED_BT_TRACE("alsa_set_volume volume %d",volume);
    if (snd_mixer_elem)
    {
        snd_mixer_selem_set_playback_volume_all(snd_mixer_elem,  volume * vol_max / 100);
    }
}

/*******************************************************************************
 * Function Name: init_audio
 *******************************************************************************
 * Summary:
 *   Initializes the ALSA driver and initializes SBC
 *
 * Parameters:
 *   playback_config_params pb_config_params : alsa configurations to be set
 *
 * Return:
 *   NONE
 *
 ******************************************************************************/
void init_audio(playback_config_params pb_config_params)
{
    int status;

    WICED_BT_TRACE("init_audio entry");

    memset (static_mem, 0, sizeof (static_mem));
    memset (scratch_mem, 0, sizeof (scratch_mem));
    memset (&strDecParams, 0, sizeof (strDecParams));

    strDecParams.s32StaticMem  = static_mem;
    strDecParams.s32ScratchMem = scratch_mem;

    strDecParams.numOfBlocks = pb_config_params.num_of_blocks;
    strDecParams.numOfChannels = pb_config_params.num_of_channels;
    strDecParams.numOfSubBands =  pb_config_params.num_of_subbands;
    strDecParams.allocationMethod = pb_config_params.allocation_method;
    sample_rate = pb_config_params.sampling_freq;
    format = SND_PCM_FORMAT_S16_LE; /* SND_PCM_FORMAT_U8; */

    SBC_Decoder_decode_Init (&strDecParams);

    WICED_BT_TRACE("nblocks %d nchannels %d nsubbands %d ameth %d freq %d format %d latency = %d",
                        strDecParams.numOfBlocks , strDecParams.numOfChannels, strDecParams.numOfSubBands,
                        strDecParams.allocationMethod, sample_rate, format, ALSA_LATENCY);

    PcmBytesPerFrame = strDecParams.numOfBlocks * strDecParams.numOfChannels * strDecParams.numOfSubBands * 2;
    printf("PcmBytesPerFrame = %d\n",PcmBytesPerFrame);

    /* If ALSA PCM driver was already open => close it */
    if (p_alsa_handle != NULL)
    {
        WICED_BT_TRACE("snd_pcm_close");
        snd_pcm_close(p_alsa_handle);
        p_alsa_handle = NULL;
    }
    WICED_BT_TRACE("snd_pcm_open");
    status = snd_pcm_open(&(p_alsa_handle), alsa_device, SND_PCM_STREAM_PLAYBACK, SND_PCM_NONBLOCK);

    if (status < 0)
    {
        WICED_BT_TRACE("snd_pcm_open failed: %s", snd_strerror(status));
    }
    else
    {
        WICED_BT_TRACE("ALSA driver opened");
        /* Configure ALSA driver with PCM parameters */
        status = snd_pcm_set_params(p_alsa_handle,
                                    format,
                                    SND_PCM_ACCESS_RW_INTERLEAVED,
                                    strDecParams.numOfChannels,
                                    sample_rate,
                                    1,
                                    ALSA_LATENCY);

        if (status < 0)
        {
            WICED_BT_TRACE("snd_pcm_set_params failed: %s", snd_strerror(status));
        }
        snd_pcm_get_params(p_alsa_handle, &buffer_size, &period_size);
        WICED_BT_TRACE("snd_pcm_get_params150ms bs %d ps %d", buffer_size, period_size);

    }
}

/*******************************************************************************
 * Function Name: deinit_audio
 *******************************************************************************
 * Summary:
 *   De-initializes the ALSA driver
 *
 * Parameters:
 *   None
 *
 * Return:
 *   None
 ******************************************************************************/
void deinit_audio(void)
{
    WICED_BT_TRACE("deinit_audio");
    if (p_alsa_handle != NULL)
    {
        WICED_BT_TRACE("snd_pcm_close");
        snd_pcm_close(p_alsa_handle);
        p_alsa_handle = NULL;
    }
    alsa_volume_driver_deinit();
}

/*******************************************************************************
 * Function Name: alsa_write_pcm_data
 *******************************************************************************
 * Summary:
 *   Writes the supplied PCM frames to ALSA driver
 *
 * Parameters:
 *   p_rx_media: The PCM buffer to be written
 *   media_len : Length of p_rx_media data
 *
 * Return:
 *   NONE
 *
 ******************************************************************************/
void alsa_write_pcm_data(uint8_t* p_rx_media, uint16_t media_len)
{
    /* pOut is pointer to buffer which holds decoded output pcm frames */
    uint8_t *pOut = p_rx_media;

    snd_pcm_sframes_t alsa_frames = 0;
    snd_pcm_sframes_t alsa_frames_to_send = 0;

    if (NULL != p_rx_media)
    {
        alsa_frames_to_send = media_len / strDecParams.numOfChannels;

        /*Bits per sample is 16 */
        alsa_frames_to_send = alsa_frames_to_send / format;

        if (p_alsa_handle == NULL)
        {
            WICED_BT_TRACE("ALSA is not configured, dropping the data pkt!!!!");
            return;
        }
#ifdef AUDIO_DEBUG
        WICED_BT_TRACE("alsa_write_pcm_data : Recd a2dp data len %d, alsa_frames_to_send %d\n",
                                                                    media_len, alsa_frames_to_send);
#endif
        while(1)
        {
            alsa_frames = snd_pcm_writei(p_alsa_handle, pOut, alsa_frames_to_send);
#ifdef AUDIO_DEBUG
            WICED_BT_TRACE("alsa_frames written = %d\n", alsa_frames);
#endif

            if (alsa_frames < 0)
            {
                alsa_frames = snd_pcm_recover(p_alsa_handle, alsa_frames, 0);
            }
            if (alsa_frames < 0)
            {
                WICED_BT_TRACE("app_avk_uipc_cback snd_pcm_writei failed %s", snd_strerror(alsa_frames));
                break;
            }
            if (alsa_frames > 0 && alsa_frames < alsa_frames_to_send)
            {
                WICED_BT_TRACE("app_avk_uipc_cback Short write (expected %li, wrote %li)",
                    (long) alsa_frames_to_send, alsa_frames);
            }
            if ( alsa_frames == alsa_frames_to_send )
            {
                break;
            }
            pOut += (alsa_frames*format*strDecParams.numOfChannels);
            alsa_frames_to_send = alsa_frames_to_send - alsa_frames;
        }
    }
}
