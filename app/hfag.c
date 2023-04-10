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
 * File Name: hfag.c
 *
 * Description: This is the source file for handsfree AG CE.
 *
 * Related Document: See README.md
 *
 *****************************************************************************/

/*******************************************************************************
*      INCLUDES
*******************************************************************************/
#include "wiced_bt_stack.h"
#include <string.h>
#include <stdlib.h>
#include "wiced_memory.h"
#include "stdio.h"
#include "wiced_bt_dev.h"
#include "hfag.h"
#include "wiced_memory.h"
#include "wiced_hal_nvram.h"
#include "wiced_bt_sco.h"
#include "audio_platform_common.h" /* ALSA */
#include <pthread.h>
#include <time.h>

/*******************************************************************************
 *       MACROS
 ******************************************************************************/
#define BT_STACK_HEAP_SIZE                      (0xF000U)
#define CASE_RETURN_STR( enum_val )             case enum_val: return #enum_val;
#define WICED_HS_EIR_BUF_MAX_SIZE               (264U)
#define HFAG_FIRST_VALID_NVRAM_ID               (0x10U)
#define HFAG_INVALID_NVRAM_ID                   (0x00U)
/* Size of the buffer used for holding the peer device key info */
#define KEY_INFO_POOL_BUFFER_SIZE               (155U)
/* Correspond's to the number of peer devices */
#define KEY_INFO_POOL_BUFFER_COUNT              (10U)
#define INQUIRY_DURATION                        (5U) /* in seconds */

#define HFAG_SAMPLING_WBS_FREQUENCY             (16000U)
#define HFAG_SAMPLING_NBS_FREQUENCY             (8000U)

#define HFAG_CHANNEL_MODE                       (0U) /* Mono */
#define HFAG_NUM_SUBBANDS                       (8U)
#define HFAG_NUM_CHANNELS                       (1U)
#define HFAG_ALLOCATION_METHOD                  (0U) /* Loudness */
#define HFAG_BITPOOL                            (26U)
#define HFAG_NUM_BLOCKS                         (15U)
#define HFAG_EIR_TYPE_FULL_NAME                 (0x09U)
#define HFAG_EIR_16BIT_UUID_LIST                (0x02U)

#ifdef DUMP_SCO_TO_FILE
#define SCO_DATA_LEN                            (1024U)
#endif

/*******************************************************************************
 *       STRUCTURES AND ENUMERATIONS
 ******************************************************************************/
typedef struct
{
    void    *p_next;
    uint16_t nvram_id;
    uint8_t  chunk_len;
    uint8_t  data[1];
} hfag_nvram_chunk_t;

/*******************************************************************************
 *       VARIABLE DEFINITIONS
 ******************************************************************************/
extern const wiced_bt_cfg_settings_t hfag_cfg_settings;
extern const uint8_t hfag_sdp_db[];
extern wiced_bt_device_address_t bt_device_address;
wiced_bt_heap_t *p_default_heap = NULL;
hfag_control_cb_t hfag_control_cb;
const wiced_bt_cfg_settings_t hfag_cfg_settings;
uint8_t pincode[4] = {0x30,0x30,0x30,0x30};
hfag_nvram_chunk_t *p_nvram_first = NULL;
wiced_bt_pool_t* p_key_info_pool; /* Pool for storing the key info */
wiced_bt_voice_path_setup_t ag_sco_path;

#ifdef DUMP_SCO_TO_FILE
FILE *fp = NULL;
uint8_t sco_data_copy[SCO_DATA_LEN];
#endif

pthread_cond_t cond_call_initial = PTHREAD_COND_INITIALIZER;
pthread_mutex_t cond_lock_initial = PTHREAD_MUTEX_INITIALIZER;

/*******************************************************************************
 *       FUNCTION DECLARATION
 ******************************************************************************/
extern uint16_t wiced_app_cfg_sdp_record_get_size(void);
static const char *hfag_get_bt_event_name( wiced_bt_management_evt_t event );
static void hfag_print_bd_address( wiced_bt_device_address_t bdadr );
static wiced_result_t hfag_management_callback
                            (
                               wiced_bt_management_evt_t event,
                               wiced_bt_management_evt_data_t *p_event_data
                            );
static wiced_result_t hfag_write_eir( void );
static void hfag_init( void );
static void hfag_event_cback
                           (
                                wiced_bt_hfp_ag_event_t evt,
                                uint16_t handle,
                                wiced_bt_hfp_ag_event_data_t *p_data
                           );
static int hfag_find_nvram_id(uint8_t *p_data, int len);
static int hfag_write_nvram(int nvram_id, int data_len, void *p_data);
static void hfag_delete_nvram( int nvram_id ,wiced_bool_t from_host);
static int hfag_read_nvram(int nvram_id, void *p_data, int data_len);
static int hfag_alloc_nvram_id( );
static const char *hfag_get_ag_event_name( wiced_bt_hfp_ag_event_t event );
static void hfag_inquiry_result_cback
                            (
                                wiced_bt_dev_inquiry_scan_result_t *p_inquiry_result,
                                uint8_t *p_eir_data
                            );
static void hfag_sco_data_app_callback
                            (
                                uint16_t sco_channel,
                                uint16_t length,
                                uint8_t* p_data
                            );

/*******************************************************************************
 *       FUNCTION DEFINITION
 ******************************************************************************/

/*******************************************************************************
 * Function Name: hfag_application_start
 *******************************************************************************
 * Summary:
 *  Set device configuration and start BT stack initialization. The actual
 *  application initialization will happen when stack reports that BT device
 *  is ready.
 *
 * Parameters: NONE
 *
 * Return: NONE
 *
 ******************************************************************************/
void hfag_application_start()
{
    printf("************* Handsfree AG Application Start ************************\n");

    /* Register call back and configuration with stack and
     * Check if stack initialization was successful */
    if ( WICED_BT_SUCCESS ==  wiced_bt_stack_init (hfag_management_callback, &hfag_cfg_settings) )
    {
        printf("Bluetooth Stack Initialization Successful \n");
        /* Create default heap */
        p_default_heap = wiced_bt_create_heap("default_heap", NULL, BT_STACK_HEAP_SIZE, NULL, WICED_TRUE);
        if ( p_default_heap == NULL )
        {
            printf("create default heap error: size %d\n", BT_STACK_HEAP_SIZE);
            exit(EXIT_FAILURE);
        }
    }
    else
    {
       printf("Bluetooth Stack Initialization failed!! \n");
       exit(EXIT_FAILURE);
    }

    wait_init_done();
}

/*******************************************************************************
 * Function Name: hfag_management_callback
 *******************************************************************************
 * Summary:
 *   This is a Bluetooth stack event handler function to receive management
 *   events from the Bluetooth stack and process as per the application.
 *
 * Parameters:
 *   wiced_bt_management_evt_t event : Bluetooth  event code of one byte length
 *   wiced_bt_management_evt_data_t *p_event_data : Pointer to BT management
 *                                                  event structures
 *
 * Return:
 *  wiced_result_t: Error code from WICED_RESULT_LIST or BT_RESULT_LIST
 *
 ******************************************************************************/
static wiced_result_t hfag_management_callback(wiced_bt_management_evt_t event,
                                          wiced_bt_management_evt_data_t *p_event_data)
{
    wiced_bt_device_address_t bda = { 0 };
    wiced_result_t result = WICED_BT_SUCCESS;
    wiced_bt_dev_pairing_cplt_t *p_pairing_cmpl;
    uint8_t pairing_result;
    wiced_bt_dev_encryption_status_t *p_encryption_status;
    int nvram_id;
    int bytes_written, bytes_read;
    const uint8_t *link_key;

    WICED_BT_TRACE( "hfag_management_callback. Event: 0x%x %s\n", event, hfag_get_bt_event_name(event) );

    switch (event)
    {
    case BTM_ENABLED_EVT:
        /* Bluetooth Controller and Host Stack Enabled */
        if ( WICED_BT_SUCCESS == p_event_data->enabled.status )
        {
            wiced_bt_set_local_bdaddr((uint8_t *)bt_device_address, BLE_ADDR_PUBLIC);
            wiced_bt_dev_read_local_addr( bda );
            printf( "Local Bluetooth Address: " );
            hfag_print_bd_address( bda );

            result = hfag_write_eir( );
            if ( WICED_BT_SUCCESS != result )
            {
                WICED_BT_TRACE( "hfag_write_eir failed with result = %x\n", result);
            }

            /* initialize everything */
            memset( &hfag_control_cb, 0, sizeof( hfag_control_cb ) );

            WICED_BT_TRACE("wiced_app_cfg_sdp_record_get_size = %d\n", wiced_app_cfg_sdp_record_get_size());
            /* create SDP records */
            if ( !wiced_bt_sdp_db_init( ( uint8_t * )hfag_sdp_db, wiced_app_cfg_sdp_record_get_size( ) ))
            {
                WICED_BT_TRACE( "wiced_bt_sdp_db_init failed\n");
            }

            hfag_init( );

            p_key_info_pool = wiced_bt_create_pool
                                (
                                    "hfag",
                                    KEY_INFO_POOL_BUFFER_SIZE,
                                    KEY_INFO_POOL_BUFFER_COUNT,
                                    NULL
                                );
            WICED_BT_TRACE( "wiced_bt_create_pool %x\n", p_key_info_pool );
            notify_init_done();
        }
        else
        {
            printf("Bluetooth Enable Failed \n");
        }
        break;

    case BTM_DISABLED_EVT:
        printf("Bluetooth Disabled \n");
        break;

    case BTM_PIN_REQUEST_EVT:
        WICED_BT_TRACE("remote address= %B\n", p_event_data->pin_request.bd_addr);
        wiced_bt_dev_pin_code_reply(*p_event_data->pin_request.bd_addr, result, 4, &pincode[0]);
    break;

    case BTM_USER_CONFIRMATION_REQUEST_EVT:
        /* User confirmation request for pairing (sample app always accepts) */
        wiced_bt_dev_confirm_req_reply (WICED_BT_SUCCESS, p_event_data->user_confirmation_request.bd_addr);
        break;

    case BTM_PAIRING_IO_CAPABILITIES_BR_EDR_REQUEST_EVT:
        /* Use the default security for BR/EDR*/
        WICED_BT_TRACE("BTM_PAIRING_IO_CAPABILITIES_REQUEST_EVT bda %B\n",
                                        p_event_data->pairing_io_capabilities_br_edr_request.bd_addr);
        p_event_data->pairing_io_capabilities_br_edr_request.local_io_cap = BTM_IO_CAPABILITIES_NONE;
        p_event_data->pairing_io_capabilities_br_edr_request.auth_req = BTM_AUTH_SINGLE_PROFILE_GENERAL_BONDING_NO;
        p_event_data->pairing_io_capabilities_br_edr_request.oob_data     = WICED_FALSE;
        break;

    case BTM_PAIRING_COMPLETE_EVT:
        p_pairing_cmpl = &p_event_data->pairing_complete;

        if ( p_pairing_cmpl->transport == BT_TRANSPORT_BR_EDR )
        {
            pairing_result = p_pairing_cmpl->pairing_complete_info.br_edr.status;
        }
        else
        {
            pairing_result = p_pairing_cmpl->pairing_complete_info.ble.reason;
        }
        printf( "PAIRING COMPLETED\n" );
        break;

    case BTM_ENCRYPTION_STATUS_EVT:
        p_encryption_status = &p_event_data->encryption_status;
        WICED_BT_TRACE( "Encryption Status Event: bd ( %B ) res %d\n",
                                p_encryption_status->bd_addr, p_encryption_status->result );
        break;

    case BTM_PAIRED_DEVICE_LINK_KEYS_UPDATE_EVT:
        WICED_BT_TRACE("BTM_PAIRED_DEVICE_LINK_KEYS_UPDATE_EVT\n");

        /* This application supports a single paired host, we can save keys
         * under the same NVRAM ID overwriting previous pairing if any */
#ifdef SAVE_PAIRING_KEY
        (void)hfag_write_nvram
                        (
                            HANDSFREE_AG_NVRAM_ID,
                            sizeof(wiced_bt_device_link_keys_t),
                            &p_event_data->paired_device_link_keys_update
                        );
#endif
        link_key = p_event_data->paired_device_link_keys_update.key_data.br_edr_key;
        WICED_BT_TRACE(" LinkKey:%02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X\n",
                link_key[0], link_key[1], link_key[2], link_key[3], link_key[4], link_key[5], link_key[6],
                link_key[7], link_key[8], link_key[9], link_key[10], link_key[11], link_key[12], link_key[13],
                link_key[14], link_key[15]);
        break;

    case BTM_PAIRED_DEVICE_LINK_KEYS_REQUEST_EVT:
        // read existing key from the NVRAM
        if ( hfag_read_nvram (
                    HANDSFREE_AG_NVRAM_ID,
                    &p_event_data->paired_device_link_keys_request,
                    sizeof(wiced_bt_device_link_keys_t)) != 0 )
        {
            WICED_BT_TRACE("BTM_PAIRED_DEVICE_LINK_KEYS_REQUEST_EVT: successfully read\n");
            result = WICED_BT_SUCCESS;
        }
        else
        {
            result = WICED_BT_ERROR;
            WICED_BT_TRACE("Key retrieval failure\n");
        }
        break;

    case BTM_SCO_CONNECTED_EVT:
    case BTM_SCO_DISCONNECTED_EVT:
    case BTM_SCO_CONNECTION_REQUEST_EVT:
    case BTM_SCO_CONNECTION_CHANGE_EVT:
        wiced_bt_hfp_ag_sco_management_callback( event, p_event_data );
        break;

    default:
        result = WICED_BT_USE_DEFAULT_SECURITY;
        break;
    }
    return result;
}

/*******************************************************************************
 * Function Name: hfag_event_cback
 *******************************************************************************
 * Summary:
 *   This is a Bluetooth stack event handler function to receive HFAG related
 *   events from the stack and process as per the application.
 *
 * Parameters:
 *   wiced_bt_hfp_ag_event_t evt: AG event
 *   uint16_t handle: app handle
 *   hfp_ag_event_t *p_data: event data
 *
 * Return:
 *   NONE
 *
 ******************************************************************************/
static void hfag_event_cback ( wiced_bt_hfp_ag_event_t evt, uint16_t handle, wiced_bt_hfp_ag_event_data_t *p_data )
{
    WICED_BT_TRACE( "### %s: evt = %x: %s\n", __FUNCTION__, evt, hfag_get_ag_event_name( evt ) );
    switch( evt )
    {
    case WICED_BT_HFP_AG_EVENT_OPEN:
        if ( NULL != p_data )
        {
            printf("------------------------------------------------------\n");
            printf("WICED_BT_HFP_AG_EVENT_OPEN: Open status = %s\n", (p_data->open.status == 0) ? "Success" : "Failed");
            printf("------------------------------------------------------\n");
        }
        break;

    case WICED_BT_HFP_AG_EVENT_CLOSE:
        hfag_print_hfp_context();
        break;

    case WICED_BT_HFP_AG_EVENT_CONNECTED:
        hfag_print_hfp_context();
        break;

    case WICED_BT_HFP_AG_EVENT_AUDIO_OPEN:
        {
#ifdef DUMP_SCO_TO_FILE
            fp = fopen("audio.raw", "wb");
            if ( fp == NULL )
            {
                perror("fopen error: ");
            }
#endif
            playback_config_params pb_config_params;
#if ( BTM_WBS_INCLUDED == WICED_TRUE )
            if ( hfag_control_cb.ag_scb[handle-1].msbc_selected == WICED_TRUE )
            {
                WICED_BT_TRACE("WBS enabled\n");
                pb_config_params.sampling_freq = HFAG_SAMPLING_WBS_FREQUENCY; /* 16000 */
            }
            else
#endif
            {
                WICED_BT_TRACE("NBS enabled\n");
                pb_config_params.sampling_freq = HFAG_SAMPLING_NBS_FREQUENCY; /*8000 */
            }
            pb_config_params.channel_mode = HFAG_CHANNEL_MODE; /* Mono */
            pb_config_params.num_of_subbands = HFAG_NUM_SUBBANDS; /* 8 */
            pb_config_params.num_of_channels = HFAG_NUM_CHANNELS; /* 1 */
            pb_config_params.allocation_method = HFAG_ALLOCATION_METHOD; /* Loudness */
            pb_config_params.bit_pool = HFAG_BITPOOL; /* 26 */
            pb_config_params.num_of_blocks = HFAG_NUM_BLOCKS; /* 15 */

            init_audio(pb_config_params);

            hfag_print_hfp_context();
        }
        break;

    case WICED_BT_HFP_AG_EVENT_AUDIO_CLOSE:
#ifdef DUMP_SCO_TO_FILE
        if ( fp ) {
            fclose( fp );
            fp = NULL;
        }
#endif
        hfag_print_hfp_context();
        break;

    case WICED_BT_HFP_AG_EVENT_AT_CMD:
        WICED_BT_TRACE("cmd = %s\n", p_data->at_cmd.cmd_ptr);
        break;

    default:
        break;
     }
}

/*******************************************************************************
 * Function Name: hfag_write_eir
 *******************************************************************************
 * Summary:
 *   Prepare extended inquiry response data.  Current version publishes
 *   handsfree and generic audio services.
 *
 * Parameters:
 *   NONE
 *
 * Return:
 *   wiced_result_t : returns WICED_BT_SUCCESS(0) on success, non 0 values for
 *                    error
 *
 ******************************************************************************/
static wiced_result_t hfag_write_eir( void )
{
    /* pointer to the buffer to hold eir data */
    uint8_t *pBuf;
    /* pointer to point fo pBuf */
    uint8_t *p;
    uint8_t length;
    wiced_result_t result = WICED_FALSE;

    pBuf = (uint8_t*)wiced_bt_get_buffer( WICED_HS_EIR_BUF_MAX_SIZE );

    if ( pBuf )
    {
        p = pBuf;

        length = strlen( (char *)hfag_cfg_settings.device_name );

        *p++ = length + 1;
        *p++ = HFAG_EIR_TYPE_FULL_NAME;            /* EIR type full name */
        memcpy( p, hfag_cfg_settings.device_name, length );
        p += length;
        *p++ = ( 2 * 2 ) + 1;         /* length of services + 1 */
        /* EIR type full list of 16 bit service UUIDs */
        *p++ =   HFAG_EIR_16BIT_UUID_LIST;
        *p++ =   UUID_SERVCLASS_AG_HANDSFREE        & 0xff;
        *p++ = ( UUID_SERVCLASS_AG_HANDSFREE >> 8 ) & 0xff;
        *p++ =   UUID_SERVCLASS_GENERIC_AUDIO        & 0xff;
        *p++ = ( UUID_SERVCLASS_GENERIC_AUDIO >> 8 ) & 0xff;
        *p++ = 0;

        /* print EIR data */
        WICED_BT_TRACE_ARRAY( ( uint8_t* )( pBuf+1 ), MIN( p-( uint8_t* )pBuf,100 ), "EIR :" );
        result = wiced_bt_dev_write_eir( pBuf, (uint16_t)(p - pBuf) );
    }
    return result;
}

/*******************************************************************************
 * Function Name: hfag_init
 *******************************************************************************
 * Summary:
 *   Initialization function for HFAG. This function Starts up the Audio
 *   gateway service and Sets up the SCO Voice path.
 *
 * Parameters:
 *   NONE
 *
 * Return:
 *   NONE
 *
 ******************************************************************************/
static void hfag_init( void )
{
    /* Currently hfag_control_cb maintains 1 service control block for
     * connecting with 1 Handsfree Unit. Headset profile is not handled
     * currently. This can be extended to hold multiple connections and
     * Headset scb also */
    wiced_bt_hfp_ag_session_cb_t *p_scb = &hfag_control_cb.ag_scb[0];
    wiced_bt_dev_status_t result;
    int i;

    memset( &hfag_control_cb, 0, sizeof( hfag_control_cb ) );

    for ( i = 0; i < HANDSFREE_AG_NUM_SCB; i++, p_scb++ )
    {
        /* app_handle is provided by the application and hfag service control
         * block is maintained against this handle */
        p_scb->app_handle = ( uint16_t ) ( i + 1 );

        /* hf_profile_uuid to be updated with Handsfree Unit UUID, this
         * will be later used by profile for searching the sdp records of
         * the remote for connecting */
        p_scb->hf_profile_uuid = UUID_SERVCLASS_HF_HANDSFREE;
    }

    wiced_bt_hfp_ag_startup
                        (
                            &hfag_control_cb.ag_scb[0],
                            HANDSFREE_AG_NUM_SCB,
                            BT_AUDIO_HFP_SUPPORTED_FEATURES,
                            hfag_event_cback
                        );

    /* Set up the SCO path to be routed over HCI to the app */
    ag_sco_path.path = WICED_BT_SCO_OVER_HCI;
    ag_sco_path.p_sco_data_cb = &hfag_sco_data_app_callback;
    result = wiced_bt_sco_setup_voice_path(&ag_sco_path);

    WICED_BT_TRACE("[%s] SCO Setting up voice path = %d\n",__func__, result);
}

/*******************************************************************************
 *      NVRAM RELATED FUNCTION DEFINITIONS
 ******************************************************************************/

/*******************************************************************************
 * Function Name: handsfree_write_nvram
 *******************************************************************************
 * Summary:
 *   Write NVRAM function is called to store information in the NVRAM.
 *
 * Parameters:
 *   int nvram_id : Volatile Section Identifier. Application can use
 *                  the VS ids from WICED_NVRAM_VSID_START to
 *                  WICED_NVRAM_VSID_END
 *   int data_len : Length of the data to be written to the NVRAM
 *   void *p_data : Pointer to the data to be written to the NVRAM
 *
 * Return:
 *   int : number of bytes written, 0 on error
 *
 ******************************************************************************/
static int hfag_write_nvram(int nvram_id, int data_len, void *p_data)
{
    wiced_result_t result;
    int bytes_written = 0;

    if ( NULL != p_data )
    {
        bytes_written = wiced_hal_write_nvram(nvram_id, data_len, (uint8_t*)p_data, &result);
        WICED_BT_TRACE("NVRAM ID:%d written :%d bytes result:%d\n", nvram_id, bytes_written, result);
    }
    return (bytes_written);
}

/*******************************************************************************
 * Function Name:
 *******************************************************************************
 * Summary:
 *   Read data from the NVRAM and return in the passed buffer
 *
 * Parameters:
 *   int nvram_id : Volatile Section Identifier. Application can use
 *                  the VS ids from WICED_NVRAM_VSID_START to
 *                  WICED_NVRAM_VSID_END
 *   int data_len : Length of the data to be written to the NVRAM
 *   void *p_data : Pointer to the data to be written to the NVRAM
 *
 * Return:
 *   int : number of bytes written, 0 on error
 *
 ******************************************************************************/
static int hfag_read_nvram(int nvram_id, void *p_data, int data_len)
{
    uint16_t read_bytes = 0;
    wiced_result_t result;

    if ( ( NULL != p_data) && (data_len >= sizeof(wiced_bt_device_link_keys_t ) ) )
    {
        read_bytes = wiced_hal_read_nvram(nvram_id, sizeof(wiced_bt_device_link_keys_t), p_data, &result);
        WICED_BT_TRACE("NVRAM ID:%d read out of %d bytes:%d result:%d\n",
                                        nvram_id, sizeof(wiced_bt_device_link_keys_t), read_bytes, result);
    }
    return (read_bytes);
}

/*******************************************************************************
 *      GAP RELATED FUNCTION DEFINITIONS
 ******************************************************************************/

/*******************************************************************************
 * Function Name: hfag_handle_set_visibility
 *******************************************************************************
 * Summary:
 *   Sets the device discoverable and connectability mode
 *
 * Parameters:
 *   uint8_t discoverability : 1-discoverable, 0-Non-Discoverable
 *   uint8_t connectability : 1-Connectable, 0-Non-Connectable
 *
 * Return:
 *   wiced_result_t : returns WICED_BT_SUCCESS(0) on success, non 0 values for
 *                    error
 *
 ******************************************************************************/
wiced_result_t hfag_handle_set_visibility(uint8_t discoverability, uint8_t connectability)
{
    wiced_result_t result = WICED_BT_SUCCESS;

    /* we cannot be discoverable and not connectable */
    if ( ( ( discoverability != 0 ) && (connectability == 0 ) ) ||
           ( discoverability > 1 ) ||
           ( connectability > 1 ) )
    {
        printf("%s: Invalid Arguments\n", __FUNCTION__);
        result = WICED_FALSE;
    }
    else
    {
        result = wiced_bt_dev_set_discoverability((discoverability != 0) ?
                                            BTM_GENERAL_DISCOVERABLE : BTM_NON_DISCOVERABLE ,
                                            BTM_DEFAULT_DISC_WINDOW,
                                            BTM_DEFAULT_DISC_INTERVAL);
        if (WICED_BT_SUCCESS != result)
        {
            WICED_BT_TRACE("%s: wiced_bt_dev_set_discoverability failed. Status = %x\n", __FUNCTION__, result);
        }
        else
        {
            result = wiced_bt_dev_set_connectability((connectability != 0) ? WICED_TRUE : WICED_FALSE ,
                                                BTM_DEFAULT_CONN_WINDOW,
                                                BTM_DEFAULT_CONN_INTERVAL);
            if (WICED_BT_SUCCESS != result)
            {
                WICED_BT_TRACE("%s: wiced_bt_dev_set_connectability failed. Status = %x\n", __FUNCTION__, result);
            }
        }
    }
    return result;
}

/*******************************************************************************
 * Function Name: hfag_handle_set_pairability
 *******************************************************************************
 * Summary:
 *   Enable or disable pairing
 *
 * Parameters:
 *   uint8_t allowed : 1-pairable, 0-Not-Pairable
 *
 * Return:
 *   wiced_result_t : returns WICED_BT_SUCCESS(0) on success, non 0 values for
 *                    error
 *
 ******************************************************************************/
wiced_result_t hfag_handle_set_pairability (uint8_t allowed)
{
    wiced_result_t result = WICED_BT_SUCCESS;
    hfag_nvram_chunk_t *p_hfag_nvram;

    if ( allowed > 1 )
    {
        printf("%s: Invalid Argument\n", __FUNCTION__);
        result = WICED_FALSE;
    }
    else
    {
        if ( hfag_control_cb.pairing_allowed != allowed )
        {
            if ( allowed )
            {
                /* Check if key buffer pool has buffer available.
                 * If not, cannot enable pairing until nvram entries are deleted
                 */
                if ( ( p_hfag_nvram = ( hfag_nvram_chunk_t * )wiced_bt_get_buffer_from_pool( p_key_info_pool ) ) == NULL)
                {
                    allowed = 0; /* The key buffer pool is full therefore we cannot allow pairing to be enabled */
                    printf( "ERROR: The key buffer pool is full therefore we cannot allow pairing to be enabled\n" );
                    result = WICED_FALSE;
                }
                else
                {
                    wiced_bt_free_buffer( p_hfag_nvram );
                }
            }

            hfag_control_cb.pairing_allowed = allowed;
            wiced_bt_set_pairable_mode( hfag_control_cb.pairing_allowed, 0 );
            WICED_BT_TRACE( " Set the pairing allowed to %d \n", hfag_control_cb.pairing_allowed );
        }
    }
    return result;
}

/*******************************************************************************
 * Function Name: hfag_inquiry
 *******************************************************************************
 * Summary:
 *   Starts or Stops Device Inquiry
 *
 * Parameters:
 *   uint8_t enable : 1 - starts Inquiry, 0 - Stops Inquiry
 *
 * Return:
 *   wiced_result_t : returns WICED_BT_SUCCESS(0) on success, non 0 values for
 *                    error
 *
 ******************************************************************************/
wiced_result_t hfag_inquiry(uint8_t enable)
{
    wiced_result_t result = WICED_BT_SUCCESS;
    wiced_bt_dev_inq_parms_t params; /* params for starting inquiry */

    if ( enable == 1 )
    {
        memset(&params, 0, sizeof(params));

        params.mode             = BTM_GENERAL_INQUIRY;
        params.duration         = INQUIRY_DURATION;
        params.filter_cond_type = BTM_CLR_INQUIRY_FILTER;

        result = wiced_bt_start_inquiry(&params, &hfag_inquiry_result_cback);
        WICED_BT_TRACE("inquiry started:%d\n", result);
    }
    else if ( enable == 0 )
    {
        result = wiced_bt_cancel_inquiry();
        WICED_BT_TRACE("cancel inquiry:%d\n", result);
    }
    else
    {
        printf("%s: Invalid Argument\n", __FUNCTION__);
        result = WICED_FALSE;
    }
    return result;
}

/*******************************************************************************
 * Function Name: hfag_inquiry_result_cback
 *******************************************************************************
 * Summary:
 *   Callback function called from stack for Inquiry Results
 *
 * Parameters:
 *   wiced_bt_dev_inquiry_scan_result_t *p_inquiry_result : Inquiry results
 *   uint8_t *p_eir_data : EIR data
 *
 * Return:
 *
 ******************************************************************************/
static void hfag_inquiry_result_cback(wiced_bt_dev_inquiry_scan_result_t *p_inquiry_result,
                                                                    uint8_t *p_eir_data)
{
    if ( p_inquiry_result == NULL )
    {
        printf("Inquiry Complete \n");
    }
    else
    {
        printf("\n--------------------------------------------\n");
        printf("Inquiry Result: %02X %02X %02X %02X %02X %02X\n",
                                        p_inquiry_result->remote_bd_addr[0], p_inquiry_result->remote_bd_addr[1],
                                        p_inquiry_result->remote_bd_addr[2], p_inquiry_result->remote_bd_addr[3],
                                        p_inquiry_result->remote_bd_addr[4], p_inquiry_result->remote_bd_addr[5]);
        printf("Clock Offset = 0x%x\n", p_inquiry_result->clock_offset);
        printf("RSSI = %d\n", p_inquiry_result->rssi);
        printf("--------------------------------------------\n");
    }
}

/*******************************************************************************
 *      UTILITY FUNCTION DEFINITIONS
 ******************************************************************************/

/******************************************************************************
 * Function Name: hfag_print_bd_address()
 ******************************************************************************
 * Summary:
 *   This is the utility function that prints the address of the Bluetooth
 *   device
 *
 * Parameters:
 *   wiced_bt_device_address_t bdadr                : Bluetooth address
 *
 * Return:
 *  void
 *
 ******************************************************************************/
static void hfag_print_bd_address(wiced_bt_device_address_t bdadr)
{
    printf("%02X:%02X:%02X:%02X:%02X:%02X\n",
            bdadr[0], bdadr[1], bdadr[2], bdadr[3], bdadr[4], bdadr[5]);
}

/******************************************************************************
 * Function Name: hfag_get_bt_event_name
 ******************************************************************************
 * Summary:
 * The function converts the wiced_bt_management_evt_t enum value to its
 * corresponding string literal. This will help the programmer to debug easily
 * with log traces without navigating through the source code.
 *
 * Parameters:
 *  wiced_bt_management_evt_t event: Bluetooth management event type
 *
 * Return:
 *  char *: String for wiced_bt_management_evt_t
 *
 ******************************************************************************/
static const char *hfag_get_bt_event_name( wiced_bt_management_evt_t event )
{
    switch ( (int)event )
    {
    CASE_RETURN_STR( BTM_ENABLED_EVT )
    CASE_RETURN_STR( BTM_DISABLED_EVT )
    CASE_RETURN_STR( BTM_POWER_MANAGEMENT_STATUS_EVT )
    CASE_RETURN_STR( BTM_PIN_REQUEST_EVT )
    CASE_RETURN_STR( BTM_USER_CONFIRMATION_REQUEST_EVT )
    CASE_RETURN_STR( BTM_PASSKEY_NOTIFICATION_EVT )
    CASE_RETURN_STR( BTM_PASSKEY_REQUEST_EVT )
    CASE_RETURN_STR( BTM_KEYPRESS_NOTIFICATION_EVT )
    CASE_RETURN_STR( BTM_PAIRING_IO_CAPABILITIES_BR_EDR_REQUEST_EVT )
    CASE_RETURN_STR( BTM_PAIRING_IO_CAPABILITIES_BR_EDR_RESPONSE_EVT )
    CASE_RETURN_STR( BTM_PAIRING_IO_CAPABILITIES_BLE_REQUEST_EVT )
    CASE_RETURN_STR( BTM_PAIRING_COMPLETE_EVT )
    CASE_RETURN_STR( BTM_ENCRYPTION_STATUS_EVT )
    CASE_RETURN_STR( BTM_SECURITY_REQUEST_EVT )
    CASE_RETURN_STR( BTM_SECURITY_FAILED_EVT )
    CASE_RETURN_STR( BTM_SECURITY_ABORTED_EVT )
    CASE_RETURN_STR( BTM_READ_LOCAL_OOB_DATA_COMPLETE_EVT )
    CASE_RETURN_STR( BTM_REMOTE_OOB_DATA_REQUEST_EVT )
    CASE_RETURN_STR( BTM_PAIRED_DEVICE_LINK_KEYS_UPDATE_EVT )
    CASE_RETURN_STR( BTM_PAIRED_DEVICE_LINK_KEYS_REQUEST_EVT )
    CASE_RETURN_STR( BTM_LOCAL_IDENTITY_KEYS_UPDATE_EVT )
    CASE_RETURN_STR( BTM_LOCAL_IDENTITY_KEYS_REQUEST_EVT )
    CASE_RETURN_STR( BTM_BLE_SCAN_STATE_CHANGED_EVT )
    CASE_RETURN_STR( BTM_BLE_ADVERT_STATE_CHANGED_EVT )
    CASE_RETURN_STR( BTM_SMP_REMOTE_OOB_DATA_REQUEST_EVT )
    CASE_RETURN_STR( BTM_SMP_SC_REMOTE_OOB_DATA_REQUEST_EVT )
    CASE_RETURN_STR( BTM_SMP_SC_LOCAL_OOB_DATA_NOTIFICATION_EVT )
    CASE_RETURN_STR( BTM_SCO_CONNECTED_EVT )
    CASE_RETURN_STR( BTM_SCO_DISCONNECTED_EVT )
    CASE_RETURN_STR( BTM_SCO_CONNECTION_REQUEST_EVT )
    CASE_RETURN_STR( BTM_SCO_CONNECTION_CHANGE_EVT )
    CASE_RETURN_STR( BTM_BLE_CONNECTION_PARAM_UPDATE )
    }

    return NULL;
}

/******************************************************************************
 * Function Name: hfag_get_ag_event_name
 ******************************************************************************
 * Summary:
 * The function converts the wiced_bt_hfp_ag_event_t enum value to its
 * corresponding string literal. This will help the programmer to debug easily
 * with log traces without navigating through the source code.
 *
 * Parameters:
 *  wiced_bt_hfp_ag_event_t event: Bluetooth HF AG event type
 *
 * Return:
 *  char *: String for wiced_bt_management_evt_t
 *
 ******************************************************************************/
static const char *hfag_get_ag_event_name( wiced_bt_hfp_ag_event_t event )
{
    switch ( (int)event )
    {  
        CASE_RETURN_STR( WICED_BT_HFP_AG_EVENT_OPEN )
        CASE_RETURN_STR( WICED_BT_HFP_AG_EVENT_CLOSE )
        CASE_RETURN_STR( WICED_BT_HFP_AG_EVENT_CONNECTED )
        CASE_RETURN_STR( WICED_BT_HFP_AG_EVENT_AUDIO_OPEN )
        CASE_RETURN_STR( WICED_BT_HFP_AG_EVENT_AUDIO_CLOSE )
        CASE_RETURN_STR( WICED_BT_HFP_AG_EVENT_AT_CMD )
        CASE_RETURN_STR( WICED_BT_HFP_AG_EVENT_CLCC_REQ )
    }

    return NULL;
}

/*******************************************************************************
 * Function Name: hfag_print_hfp_context
 *******************************************************************************
 * Summary:
 *   This Function prints the current HFAG connection status
 *
 * Parameters:
 *   NONE
 *
 * Return:
 *   NONE
 *
 ******************************************************************************/
void hfag_print_hfp_context(void)
{
    int i = 0;
    printf("\n----------------HFAG CONNECTION DETAILS----------------------------\n");
    printf("BD ADDRESS \t\t APPLICATION HANDLE \t SCO INDEX\n");
    for ( i = 0; i < HANDSFREE_AG_NUM_SCB; i++ )
    {
        printf("%02X %02X %02X %02X %02X %02X \t %X \t\t\t %X \n",
                                        hfag_control_cb.ag_scb[i].hf_addr[0],
                                        hfag_control_cb.ag_scb[i].hf_addr[1],
                                        hfag_control_cb.ag_scb[i].hf_addr[2],
                                        hfag_control_cb.ag_scb[i].hf_addr[3],
                                        hfag_control_cb.ag_scb[i].hf_addr[4],
                                        hfag_control_cb.ag_scb[i].hf_addr[5],
                                        hfag_control_cb.ag_scb[i].app_handle,
                                        hfag_control_cb.ag_scb[i].sco_idx);
    }
    printf("--------------------------------------------------------------------\n");
}

/*******************************************************************************
 * Function Name: hfag_validate_app_handle
 *******************************************************************************
 * Summary:
 *   This Function checks if the app handle is valid or not
 *
 * Parameters:
 *   uint16_t handle : app handle to be validated
 *
 * Return:
 *   uint8_t : returns 1 if the app handle is valid else returns 0
 *
 ******************************************************************************/
uint8_t hfag_validate_app_handle( uint16_t handle )
{
    uint8_t valid = 0;
    for ( int i = 0; i < HANDSFREE_AG_NUM_SCB; i++ )
    {
        if ( handle == hfag_control_cb.ag_scb[i].app_handle )
        {
            valid = 1;
            break;
        }
    }
    return valid;
}
/*******************************************************************************
 * Function Name: hfag_sco_data_app_callback
 *******************************************************************************
 * Summary:
 *   callback function called for incoming pcm data
 *
 * Parameters:
 *   uint16_t sco_channel : sco channel
 *   uint16_t length : SCO data callback length
 *   uint8_t* p_data : incoming SCO pcm data
 *
 * Return:
 *   NONE
 *
 ******************************************************************************/
static void hfag_sco_data_app_callback(uint16_t sco_channel, uint16_t length, uint8_t* p_data)
{
#ifdef AUDIO_DEBUG
    WICED_BT_TRACE("sco_data_app_callback-length =  (%d)\n", length);
#endif
    if ( length ) {
#ifdef DUMP_SCO_TO_FILE
        /* You can play the audio file generated (audio_mic.raw) using
         * the following aplay command:
         * aplay audio_mic.raw -c 1 -f S16_LE -r 8000
         * Replace 8000 with any frequency with which the alsa
         * is configured
         */
        if ( fp )
        {
            memset(sco_data_copy, 0, SCO_DATA_LEN);
            memcpy(sco_data_copy, p_data, length);
            fwrite(sco_data_copy, sizeof(unsigned char), length, fp);
        }
#endif
        alsa_write_pcm_data(p_data, length);

        wiced_result_t result = WICED_ERROR;

        /* Looping back to sco_idx for which the sco is opened */
        for ( int i = 0; i < HANDSFREE_AG_NUM_SCB; i++ )
        {
            if ( hfag_control_cb.ag_scb[i].b_sco_opened )
            {
                result = wiced_bt_sco_write_buffer( hfag_control_cb.ag_scb[i].sco_idx, p_data, length );
                if ( WICED_BT_SUCCESS != result )
                {
                    WICED_BT_TRACE("wiced_bt_sco_write_buffer error, sco_index = %d, result = %d\n",
                                                            hfag_control_cb.ag_scb[i].sco_idx, result);
                }
                break;
            }
        }
    }
}


void wait_init_done(){
    //WICED_BT_TRACE("wait_init_done[Waiting...]");
    //work-around for Hatchet2 autobaud
    //WICED_BT_TRACE("Please trigger REGON to enable autobaud FW download, and enter any input:()");
    pthread_mutex_lock(&cond_lock_initial);
    pthread_cond_wait(&cond_call_initial, &cond_lock_initial);
    pthread_mutex_unlock(&cond_lock_initial);
    WICED_BT_TRACE("wait_init_done[Release...]");
}

void notify_init_done(){
    WICED_BT_TRACE("notify_init_done");
    pthread_mutex_lock(&cond_lock_initial);
    pthread_cond_signal(&cond_call_initial);
    pthread_mutex_unlock(&cond_lock_initial);

}
/* END OF FILE [] */
