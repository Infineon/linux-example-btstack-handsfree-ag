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
 * File Name: hfag.h
 *
 * Description: This is the include file for handsfree AG CE.
 *
 * Related Document: See README.md
 *
 *****************************************************************************/

#ifndef __APP_HFAG_H__
#define __APP_HFAG_H__

/******************************************************************************
 *          INCLUDES
 *****************************************************************************/
#include <stdio.h>
#include "wiced_bt_cfg.h"
#include "wiced_bt_trace.h"
#include "wiced_bt_hfp_ag.h"

/******************************************************************************
 *          MACROS
 *****************************************************************************/
//#define AUDIO_DEBUG
//#define DUMP_SCO_TO_FILE
//#define SAVE_PAIRING_KEY

/* SDP Record for Hands-Free AG */
#define HDLR_HANDSFREE_AG                   0x10001
#define HANDSFREE_AG_SCN                    0x01
#define HANDSFREE_AG_DEVICE_NAME            "Handsfree AG"
#define HANDSFREE_AG_NUM_SCB                1
#define HFAG_SDP_DB_SIZE                    (80U)

#if (BTM_WBS_INCLUDED == TRUE )
#define BT_AUDIO_HFP_SUPPORTED_FEATURES     ( HFP_AG_FEAT_VREC | HFP_AG_FEAT_CODEC | HFP_AG_FEAT_ESCO)
#define AG_SUPPORTED_FEATURES_ATT           ( WICED_BT_HFP_AG_SDP_FEATURE_VRECG | \
                                            WICED_BT_HFP_AG_SDP_FEATURE_WIDEBAND_SPEECH )

#else
#define BT_AUDIO_HFP_SUPPORTED_FEATURES     ( HFP_AG_FEAT_VREC | HFP_AG_FEAT_ESCO)
#define AG_SUPPORTED_FEATURES_ATT           ( WICED_BT_HFP_AG_SDP_FEATURE_VRECG )
#endif

#define HFP_VGM_VGS_DEFAULT                 7
#define HANDSFREE_AG_NVRAM_ID               WICED_NVRAM_VSID_START

/******************************************************************************
 *          STRUCTURES AND ENUMERATIONS
 *****************************************************************************/

/* The main application control block */
typedef struct
{
    wiced_bt_hfp_ag_session_cb_t  ag_scb[HANDSFREE_AG_NUM_SCB];       /* service control blocks */
    uint8_t pairing_allowed;
} hfag_control_cb_t;

/******************************************************************************
 *          FUNCTION PROTOTYPES
 *****************************************************************************/
void hfag_application_start( );
wiced_result_t hfag_inquiry( uint8_t enable );
wiced_result_t hfag_handle_set_pairability( uint8_t allowed );
wiced_result_t hfag_handle_set_visibility( uint8_t discoverability, uint8_t connectability );
void hfag_print_hfp_context( void );
uint8_t hfag_validate_app_handle( uint16_t handle );

void wait_init_done();
void notify_init_done();

#endif /* __APP_HFAG_H__ */
