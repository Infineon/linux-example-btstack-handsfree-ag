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
* File Name: main.c
*
* Description: This is the source code for Linux CE Handsfree AG project.
*
* Related Document: See README.md
*
*******************************************************************************/

/*******************************************************************************
 *                           INCLUDES
 ******************************************************************************/
#include <sys/ioctl.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include <ctype.h> 
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include "wiced_bt_trace.h"
#include "wiced_bt_ble.h"
#include "wiced_bt_types.h"
#include "wiced_bt_dev.h"
#include "wiced_bt_stack.h"
#include "wiced_memory.h"
#include "platform_linux.h"
#include "utils_arg_parser.h"
#include "wiced_bt_cfg.h"
#include "hfag.h"

/*******************************************************************************
 *                               MACROS
 ******************************************************************************/
#define MAX_PATH                            (256U)
#define BDA_LEN                             (6U)

/*USER INPUT COMMANDS*/
#define EXIT                                (0U)
#define PRINT_MENU                          (1U)
#define SET_VISIBILITY                      (2U)
#define SET_PAIRING_MODE                    (3U)
#define SET_INQUIRY                         (4U)
#define HFAG_CONNECT                        (5U)
#define HFAG_DISCONNECT                     (6U)
#define HFAG_AUDIO_CONNECT                  (7U)
#define HFAG_AUDIO_DISCONNECT               (8U)
#define HFAG_PRINT_CONNECTION_DETAILS       (9U)
#define HFAG_SEND_AG_COMMAND                (10U)

#define DEV_NAME "/dev/gpiochip5"
/******************************************************************************
 *                               GLOBAL VARIABLES
 *****************************************************************************/
/* Application name */
static char g_app_name[MAX_PATH];
extern wiced_bt_device_address_t bt_device_address;

static const char app_menu[] = "\n\
---------------------HFAG MENU-----------------------\n\n\
    0.  Exit \n\
    1.  Print Menu \n\
    2.  Set Visibility \n\
    3.  Set Pairing Mode\n\
    4.  Set Inquiry \n\
    5.  HFAG Connect \n\
    6.  HFAG Disconnect \n\
    7.  Audio Connect \n\
    8.  Audio Disconnect \n\
    9.  Print HFAG Connection Details\n\
    10. Send AG cmd str\n\
Choose option -> ";


/****************************************************************************
 *                              FUNCTION DECLARATIONS
 ***************************************************************************/
uint32_t hci_control_proc_rx_cmd( uint8_t* p_buffer, uint32_t length );
void APPLICATION_START( void );

/******************************************************************************
 *                               FUNCTION DEFINITIONS
 *****************************************************************************/

/******************************************************************************
 * Function Name: hci_control_proc_rx_cmd()
 ******************************************************************************
 * Summary:
 *   Function to handle HCI receive (This will be called from
 *   porting layer - linux_tcp_server.c)
 *
 * Parameters:
 *   uint8_t* p_buffer  : rx buffer
 *  uint32_t length     : rx buffer length
 *
 * Return:
 *  status code
 *
 *****************************************************************************/
uint32_t hci_control_proc_rx_cmd( uint8_t* p_buffer, uint32_t length )
{
    return 0;
}

/******************************************************************************
 * Function Name: APPLICATION_START()
 ******************************************************************************
 * Summary:
 *   BT stack initialization function wrapper
 *
 * Parameters:
 *   None
 *
 * Return:
 *      None
 *
 *****************************************************************************/
void APPLICATION_START( void )
{
    hfag_application_start();
}


/*******************************************************************************
* Function Name: my_atoi()
********************************************************************************
* Summary:
*   check input is number or not
*
* Parameters:
*   numArray: input string
*   value: atoi value
*
* Return:
*   None
*
*******************************************************************************/
void my_atoi(char *numArray, int *value)
{
    int i;
    const int len = strlen(numArray);

    for (i = 0; i < len; i++)
    {
        if (!isdigit(numArray[i])) break;
    }
    *value = len == i ? atoi(numArray) : PRINT_MENU;
}

/******************************************************************************
 * Function Name: main()
 ******************************************************************************
 * Summary:
 *   Application entry function
 *
 * Parameters:
 *   int argc           : argument count
 *   char *argv[]       : list of arguments
 *
 * Return:
 *      None
 *
 *****************************************************************************/
int main( int argc, char* argv[] )
{
    int toggle_flag = 1;
    int len = 0; /* Length of application name */
    char patchFile[MAX_PATH]; /* Firmware patch file */
    char device[MAX_PATH]; /* Interface Device */
    char peer_ip_addr[16] = "000.000.000.000"; /* Peer IP Address */
    uint32_t baud = 0; /* HCI baud rate */
    uint32_t patch_baud = 0; /* Patch downloading baud rate */
    int spy_inst = 0; /* BTSPY instance */
    uint8_t is_socket_tcp = 0; /* BTSPY communication socket */
    unsigned int  choice = 0; /* Choice for User Input */
    char input_choice[3] = {0,0,0}; /* User Input Str */
    unsigned int handle; /* hasg application handle */

    cybt_controller_autobaud_config_t autobaud;

    /* Parse the arguments */
    memset( patchFile,0,MAX_PATH );
    memset( device,0,MAX_PATH );
    if ( PARSE_ERROR == arg_parser_get_args( argc, argv, device, bt_device_address,
                                            &baud, &spy_inst, peer_ip_addr,
                                            &is_socket_tcp, patchFile,
                                            &patch_baud,
                                            &autobaud ))
    {
        return EXIT_FAILURE;
    }

    /* Extract the application name */
    memset( g_app_name, 0, sizeof( g_app_name ) );
    len = strlen( argv[0] );
    if ( len >= MAX_PATH )
    {
        len = MAX_PATH - 1;
    }
    strncpy( g_app_name, argv[0], len );
    g_app_name[len] = '\0';

    cy_bt_spy_comm_init( is_socket_tcp, spy_inst, peer_ip_addr );
    printf( "cy_bt_spy_comm_init done\n");

    cy_platform_bluetooth_init( patchFile, device, baud, patch_baud, &autobaud );

    //printf( "Linux CE HFAG project initialization complete...\n" );
    printf("%s", app_menu);

    for ( ; ; )
    {
        fflush(stdin);
        if (scanf("%2s", input_choice) == EOF){
            printf( "Enter discoverability fail!!\n");
            choice = PRINT_MENU;
        }
        else{

            my_atoi(input_choice, &choice);
        }
        switch(choice)
        {
        case EXIT:
            exit(EXIT_SUCCESS);
        case PRINT_MENU:
            {
                printf("%s\n", app_menu);
            }
            break;

        case SET_VISIBILITY:
            {
                unsigned int discoverable;
                unsigned int connectable;
                printf("Enter discoverability: 0:Non Discoverable, 1: Discoverable\n");
                if (scanf("%u", &discoverable) == EOF){
                    printf( "Enter discoverability fail!!\n");
                    break;
                }
                printf("\nEnter connectability: 0:Non Connectable, 1: Connectable\n");
                if (scanf("%u", &connectable) == EOF){
                    printf( "Enter connectability fail!!\n");
                    break;
                }
                if ( hfag_handle_set_visibility((uint8_t)discoverable, (uint8_t)connectable) != WICED_BT_SUCCESS)
                {
                    WICED_BT_TRACE("hfag_handle_set_visibility retuned error\n");
                }
            }
            break;

        case SET_PAIRING_MODE:
            {
                unsigned int allowed;
                printf("Enter if pairing is allowed: 0: Not allowed, 1: Allowed\n");
                if (scanf("%d", &allowed) == EOF){
                    printf( "Enter allowed fail!!\n");
                    break;
                }
                if (hfag_handle_set_pairability((uint8_t)allowed) != WICED_BT_SUCCESS)
                {
                    WICED_BT_TRACE("hfag_handle_set_pairability retuned error\n");
                }
            }
            break;

        case SET_INQUIRY:
            {
                unsigned int enable;
                printf("Enter if Inquiry has to be enabled/disabled: 0: Disabled, 1: Enabled\n");
                if (scanf("%u", &enable) == EOF){
                    printf( "Enter enable fail!!\n");
                    break;
                }
                if ( !hfag_inquiry((uint8_t)enable) )
                {
                    WICED_BT_TRACE("hfag_inquiry retuned error");
                }
            }
            break;

        case HFAG_CONNECT:
            {
                BD_ADDR peer_bd_addr;
                int i = 0;
                unsigned int read;
                printf("Enter Peer BD Address as displayed in Inquiry Results \n(Example: 11 22 33 44 55 66): \n");
                for(i = 0; i < BDA_LEN; i++)
                {
                    if (scanf("%x", &read) == EOF){
                        printf( "Enter BD_ADDR fail!!\n");
                        break;
                    }
                    peer_bd_addr[i] = (unsigned char)read;
                }
                wiced_bt_hfp_ag_connect(peer_bd_addr);
            }
            break;

        case HFAG_DISCONNECT:
            {
                hfag_print_hfp_context();
                printf("Enter the Application Handle to disconnect as displayed in HFAG CONNECTION DETAILS: ");
                if (scanf("%x", &handle) == EOF){
                    printf( "Enter HFAG_DISCONNECT handle number fail!!\n");
                    break;                    
                }
                if ( hfag_validate_app_handle( (uint16_t)handle ) )
                {
                    wiced_bt_hfp_ag_disconnect((uint16_t)handle);
                }
                else
                {
                    printf("Invalid Handle\n");
                }
            }
            break;

        case HFAG_AUDIO_CONNECT:
            {
                hfag_print_hfp_context();
                printf("Enter the Application Handle as displayed in HFAG CONNECTION DETAILS: ");
                if (scanf("%x", &handle) == EOF){
                    printf( "Enter HFAG_AUDIO_CONNECT handle number fail!!\n");
                    break;
                }
                if ( hfag_validate_app_handle( (uint16_t)handle ) )
                {
                    wiced_bt_hfp_ag_audio_open( (uint16_t)handle );
                }
                else
                {
                    printf("Invalid Handle\n");
                }
            }
            break;

        case HFAG_AUDIO_DISCONNECT:
            {
                hfag_print_hfp_context();
                printf("Enter the Application Handle as displayed in HFAG CONNECTION DETAILS: ");
                if (scanf("%x", &handle) == EOF){
                    printf( "Enter HFAG_AUDIO_DISCONNECT handle number fail!!\n");
                    break;
                }
                if ( hfag_validate_app_handle( (uint16_t)handle ) )
                {
                    wiced_bt_hfp_ag_audio_close( (uint16_t)handle );
                }
                else
                {
                    printf("Invalid Handle\n");
                }
            }
            break;

        case HFAG_PRINT_CONNECTION_DETAILS:
            hfag_print_hfp_context();
            break;

        case HFAG_SEND_AG_COMMAND:
            {
                uint8_t ag_cmd[100];
                int ch;
                hfag_print_hfp_context();
                printf("Enter the Application Handle as displayed in HFAG CONNECTION DETAILS: ");
                if (scanf("%x", &handle) == EOF){
                    printf( "Enter HFAG_SEND_AG_COMMAND handle number fail!!\n");
                    break;
                }
                while ((ch = getchar()) != '\n');
                if ( hfag_validate_app_handle( (uint16_t)handle ) )
                {
                    printf("Enter the AG cmd str: ");
                    if (scanf("%[^\n]s",ag_cmd) == EOF){
                        printf( "Enter AG cmd str fail!!\n");
                        break;
                    }
                    wiced_bt_hfp_ag_send_cmd_str((uint16_t)handle, ag_cmd, strlen(ag_cmd));
                }
                else
                {
                    printf("Invalid Handle\n");
                }
            }
            break;

        default:
            printf("Invalid Input\n");
            break;
        }
    }
    return EXIT_SUCCESS;
}
