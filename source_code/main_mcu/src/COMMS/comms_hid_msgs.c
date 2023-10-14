/* 
 * This file is part of the Mooltipass Project (https://github.com/mooltipass).
 * Copyright (c) 2019 Stephan Mathieu
 * 
 * This program is free software: you can redistribute it and/or modify  
 * it under the terms of the GNU General Public License as published by  
 * the Free Software Foundation, version 3.
 *
 * This program is distributed in the hope that it will be useful, but 
 * WITHOUT ANY WARRANTY; without even the implied warranty of 
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU 
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */
/*!  \file     comms_hid_msgs.h
*    \brief    HID communications
*    Created:  06/03/2018
*    Author:   Mathieu Stephan
*/
#include <asf.h>
#include <string.h>
#include "smartcard_highlevel.h"
#include "platform_defines.h"
#include "logic_encryption.h"
#include "logic_smartcard.h"
#include "gui_dispatcher.h"
#include "comms_hid_msgs.h"
#include "logic_security.h"
#include "logic_database.h"
#include "logic_aux_mcu.h"
#include "comms_aux_mcu.h"
#include "driver_timer.h"
#include "logic_device.h"
#include "gui_prompts.h"
#include "platform_io.h"
#include "logic_power.h"
#include "logic_user.h"
#include "custom_fs.h"
#include "dataflash.h"
#include "text_ids.h"
#include "gui_menu.h"
#include "nodemgmt.h"
#include "bearssl.h"
#include "dbflash.h"
#include "utils.h"
#include "main.h"
#include "dma.h"
#include "rng.h"
/* Boolean to specify if bundle data upload is allowed */
BOOL comms_hid_msgs_bundle_upload_allowed = FALSE;


/*! \fn     comms_hid_msgs_fill_get_status_message_answer(uint16_t* msg_array_uint16)
*   \brief  Fill an array with the answer to the HID get_status message
*   \param  msg_array_uint16    4 bytes long array where the answer should be stored
*   \return payload size
*/
uint16_t comms_hid_msgs_fill_get_status_message_answer(uint16_t* msg_array_uint16)
{
    uint8_t* msg_array_uint8 = (uint8_t*)msg_array_uint16;
    msg_array_uint16[1] = 0x0000;
    msg_array_uint16[0] = 0x0000;
    
    // Last bit: is card inserted
    if (smartcard_low_level_is_smc_absent() != RETURN_OK)
    {
        msg_array_uint8[0] |= 0x01;
    }
    // Unlocking screen 0x02: not implemented on this device
    // Smartcard unlocked
    if (logic_security_is_smc_inserted_unlocked() != FALSE)
    {
        msg_array_uint8[0] |= 0x04;
    }
    // Unknown card
    if (gui_dispatcher_get_current_screen() == GUI_SCREEN_INSERTED_UNKNOWN)
    {
        msg_array_uint8[0] |= 0x08;
    }
    // Management mode (useful when dealing with multiple interfaces)
    if (logic_security_is_management_mode_set() != FALSE)
    {
        msg_array_uint8[0] |= 0x10;
    }
    // No bundle / firmware upload mode
    if ((gui_dispatcher_get_current_screen() == GUI_SCREEN_INVALID) || (gui_dispatcher_get_current_screen() == GUI_SCREEN_FW_FILE_UPDATE))
    {
        msg_array_uint8[0] |= 0x20;
    }
    
    /* Battery level */
    msg_array_uint8[1] = logic_power_get_battery_level() * 10;
    
    /* MSb of battery level to indicate charging status */
    if (logic_power_is_battery_charging() != FALSE)
    {
        msg_array_uint8[1] |= 0x80;
    }
    
    /* If user logged in, send user security preferences */
    if (logic_security_is_smc_inserted_unlocked() != FALSE)
    {
        msg_array_uint16[1] = logic_user_get_user_security_flags();
    }
    else
    {
        /* Otherwise, just pad */
        msg_array_uint16[1] = 0;
    }
    
    /* Settings changed flag */
    msg_array_uint8[4] = (uint8_t)logic_device_get_and_clear_settings_changed_flag();
    
    return 5;
}

/*! \fn     comms_hid_msgs_update_message_payload_length_fields(aux_mcu_message_t* message_pt, uint16_t hid_payload_size)
*   \brief  Update message payload length fields
*   \param  message_pt          Pointer to message to update
*   \param  hid_payload_size    Payload size in HID message
*/
void comms_hid_msgs_update_message_payload_length_fields(aux_mcu_message_t* message_pt, uint16_t hid_payload_size)
{
    message_pt->payload_length1 = hid_payload_size + sizeof(message_pt->hid_message.message_type) + sizeof(message_pt->hid_message.payload_length);
    message_pt->hid_message.payload_length = hid_payload_size;
}

/*! \fn     comms_hid_msgs_update_message_fields(aux_mcu_message_t* message_pt, BOOL usb_hid_message, uint16_t message_type, uint16_t hid_payload_size)
*   \brief  Update an HID message fields
*   \param  message_pt          Pointer to message to update
*   \param  usb_hid_message     TRUE for USB HID message
*   \param  message_type        HID message type
*   \param  hid_payload_size    Payload size in HID message
*/
void comms_hid_msgs_update_message_fields(aux_mcu_message_t* message_pt, BOOL usb_hid_message, uint16_t message_type, uint16_t hid_payload_size)
{
    if (usb_hid_message != FALSE)
    {
        message_pt->message_type = AUX_MCU_MSG_TYPE_USB;
    }
    else
    {
        message_pt->message_type = AUX_MCU_MSG_TYPE_BLE;
    }
    
    /* Update payload size */
    message_pt->payload_length1 = hid_payload_size + sizeof(message_pt->hid_message.message_type) + sizeof(message_pt->hid_message.payload_length);
    message_pt->hid_message.payload_length = hid_payload_size;
    message_pt->hid_message.message_type = message_type;
}    

/*! \fn     comms_hid_msgs_get_empty_hid_packet(BOOL usb_hid_message, uint16_t message_type, uint16_t hid_payload_size)
*   \brief  Get an empty HID message ready to be sent to aux
*   \param  usb_hid_message     TRUE for USB HID message
*   \param  message_type        HID message type
*   \param  hid_payload_size    Payload size in HID message
*   \return Pointer to the message ready to be sent
*/
aux_mcu_message_t* comms_hid_msgs_get_empty_hid_packet(BOOL usb_hid_message, uint16_t message_type, uint16_t hid_payload_size)
{
    aux_mcu_message_t* temp_send_message_pt;
    
    /* Get pointer to message to be sent */
    if (usb_hid_message != FALSE)
    {
        temp_send_message_pt = comms_aux_mcu_get_empty_packet_ready_to_be_sent(AUX_MCU_MSG_TYPE_USB);
    }
    else
    {
        temp_send_message_pt = comms_aux_mcu_get_empty_packet_ready_to_be_sent(AUX_MCU_MSG_TYPE_BLE);
    }   
    
    /* Update payload size */
    temp_send_message_pt->payload_length1 = hid_payload_size + sizeof(temp_send_message_pt->hid_message.message_type) + sizeof(temp_send_message_pt->hid_message.payload_length);
    temp_send_message_pt->hid_message.payload_length = hid_payload_size;
    temp_send_message_pt->hid_message.message_type = message_type;
    
    /* Return pointer */
    return temp_send_message_pt;
}

/*! \fn     comms_hid_msgs_send_ack_nack_message(BOOL usb_hid_message, uint16_t message_type, BOOL ack_message)
*   \brief  Send ACK or NACK message for a given message type
*   \param  usb_hid_message TRUE for USB HID message
*   \param  message_type    Message type for the (N)ACK
*   \param  ack_message     TRUE to send ACK message
*/
void comms_hid_msgs_send_ack_nack_message(BOOL usb_hid_message, uint16_t message_type, BOOL ack_message)
{
    aux_mcu_message_t* temp_tx_message_pt = comms_hid_msgs_get_empty_hid_packet(usb_hid_message, message_type, sizeof(uint8_t));
    if (ack_message == FALSE)
    {
        temp_tx_message_pt->hid_message.payload[0] = HID_1BYTE_NACK;
    } 
    else
    {
        temp_tx_message_pt->hid_message.payload[0] = HID_1BYTE_ACK;
    }
    
    /* Send message */
    comms_aux_mcu_send_message(temp_tx_message_pt);
}

/*! \fn     comms_hid_msgs_parse(hid_message_t* rcv_msg, uint16_t supposed_payload_length, msg_restrict_type_te answer_restrict_type, BOOL is_message_from_usb)
*   \brief  Parse an incoming message from USB or BLE
*   \param  rcv_msg                 Received message
*   \param  supposed_payload_length Supposed payload length
*   \param  answer_restrict_type    Enum restricting which messages we can answer
*   \param  is_message_from_usb     Boolean set to TRUE if message comes from USB
*/
void comms_hid_msgs_parse(hid_message_t* rcv_msg, uint16_t supposed_payload_length, msg_restrict_type_te answer_restrict_type, BOOL is_message_from_usb)
{
    nodemgmt_data_category_te data_type_for_operation = NODEMGMT_STANDARD_DATA_TYPE_ID;
    
    /* Check correct payload length */
    if ((supposed_payload_length != rcv_msg->payload_length) || (supposed_payload_length > sizeof(rcv_msg->payload)))
    {
        /* Silent error */
        return;
    }
    
    /* Aliases for HID commands */
    if ((rcv_msg->message_type == HID_CMD_ADD_NOTE_ID) ||
    (rcv_msg->message_type == HID_CMD_ACCESS_NOTE_ID) ||
    (rcv_msg->message_type == HID_CMD_MODIFY_NOTE_ID) ||
    (rcv_msg->message_type == HID_CMD_ADD_NOTE_DATA_ID) ||
    (rcv_msg->message_type == HID_CMD_SCAN_NOTE_ID) ||
    (rcv_msg->message_type == HID_CMD_DELETE_NOTE_ID))
    {
        data_type_for_operation = NODEMGMT_NOTES_DATA_TYPE_ID;
    }
    
    /* Store received message type in case one of the routines below does some communication */
    uint16_t max_payload_size = MEMBER_ARRAY_SIZE(hid_message_t,payload);
    uint16_t rcv_message_type = rcv_msg->message_type;
    BOOL is_aes_gcm_message = FALSE;
    
    /* Check if it's a AES-GCM message */
    if ((rcv_msg->message_type & HID_MESSAGE_AES_GCM_BITMASK) != 0)
    {
        /* If so, remove the bitmask, reduce max payload size and set the bool */
        rcv_msg->message_type &= ~HID_MESSAGE_AES_GCM_BITMASK;
        max_payload_size-= HID_MESSAGE_GCM_TAG_LGTH;
        is_aes_gcm_message = TRUE;
    }
    
    /* Checks based on restriction type: ignore all messages */
    BOOL should_ignore_message = FALSE;
    if ((answer_restrict_type == MSG_RESTRICT_ALL) && 
    (rcv_msg->message_type != HID_CMD_ID_PING) &&
    (rcv_msg->message_type != HID_CMD_GET_DEVICE_STATUS) &&
    (rcv_msg->message_type != HID_CMD_IM_LOCKED) &&
    (rcv_msg->message_type != HID_CMD_IM_UNLOCKED) &&
    (rcv_msg->message_type != HID_CMD_ID_SET_DATE))
    {
        should_ignore_message = TRUE;
    }
    
    /* Checks based on restriction type: ignore all messages but SN query */
    if ((answer_restrict_type == MSG_RESTRICT_ALLBUT_SN) && 
    (rcv_msg->message_type != HID_CMD_ID_PING) &&
    (rcv_msg->message_type != HID_CMD_GET_DEVICE_STATUS) &&
    (rcv_msg->message_type != HID_CMD_IM_LOCKED) &&
    (rcv_msg->message_type != HID_CMD_IM_UNLOCKED) &&
    (rcv_msg->message_type != HID_CMD_ID_SET_DATE) &&
    (rcv_msg->message_type != HID_CMD_GET_DEVICE_INT_SN) &&
    (rcv_msg->message_type != HID_CMD_SET_DEVICE_SN) &&
    (rcv_msg->message_type != HID_CMD_PREPARE_SN_FLASH) &&
    (rcv_msg->message_type != HID_CMD_SWITCH_OFF_NXT_DSC))
    {
        should_ignore_message = TRUE;
    }
    
    /* Checks based on restriction type: ignore all messages except cancel request */
    if ((answer_restrict_type == MSG_RESTRICT_ALLBUT_CANCEL) && 
        (rcv_msg->message_type != HID_CMD_ID_PING) &&
        (rcv_msg->message_type != HID_CMD_GET_DEVICE_STATUS) &&
        (rcv_msg->message_type != HID_CMD_ID_CANCEL_REQ) && 
        (rcv_msg->message_type != HID_CMD_IM_LOCKED) &&
        (rcv_msg->message_type != HID_CMD_IM_UNLOCKED) &&
        (rcv_msg->message_type != HID_CMD_ID_SET_DATE) &&
        (rcv_msg->message_type != HID_CMD_GET_DEVICE_INT_SN) &&
        (rcv_msg->message_type != HID_CMD_SET_DEVICE_SN) &&
        (rcv_msg->message_type != HID_CMD_PREPARE_SN_FLASH) &&
        (rcv_msg->message_type != HID_CMD_SWITCH_OFF_NXT_DSC))
    {
        should_ignore_message = TRUE;
    }
    
    /* All but bundle restriction */
    if ((answer_restrict_type == MSG_RESTRICT_ALLBUT_BUNDLE) &&
    (rcv_msg->message_type != HID_CMD_ID_PING) &&
    (rcv_msg->message_type != HID_CMD_GET_DEVICE_STATUS) &&
    (rcv_msg->message_type != HID_CMD_START_BUNDLE_UL) &&
    (rcv_msg->message_type != HID_CMD_BUNDLE_WRITE_256B) &&
    (rcv_msg->message_type != HID_CMD_BUNDLE_UL_DONE) &&
    (rcv_msg->message_type != HID_CMD_ID_CANCEL_REQ) &&
    (rcv_msg->message_type != HID_CMD_IM_LOCKED) &&
    (rcv_msg->message_type != HID_CMD_IM_UNLOCKED) &&
    (rcv_msg->message_type != HID_CMD_ID_SET_DATE) &&
    (rcv_msg->message_type != HID_CMD_GET_DEVICE_INT_SN) &&
    (rcv_msg->message_type != HID_CMD_ID_PLAT_INFO))
    {
        should_ignore_message = TRUE;
    }
    
    /* Depending on restriction, answer please retry */
    if (should_ignore_message != FALSE)
    {
        /* Send please retry */
        aux_mcu_message_t* temp_tx_message_pt = comms_hid_msgs_get_empty_hid_packet(is_message_from_usb, HID_CMD_ID_RETRY, 0);
        comms_aux_mcu_send_message(temp_tx_message_pt);
        return;
    }
    
    /* Check for commands for management mode */
    if ((rcv_msg->message_type >= HID_FIRST_CMD_FOR_MMM) && (rcv_msg->message_type <= HID_LAST_CMD_FOR_MMM) && (logic_security_is_management_mode_set() == FALSE))
    {
        /* Set nack, leave same command id */
        comms_hid_msgs_send_ack_nack_message(is_message_from_usb, rcv_message_type, FALSE);
        return;
    }
    
    /* Switch on command id */
    switch (rcv_msg->message_type)
    {
        case HID_CMD_ID_PING:
        {
            /* Simple ping: copy the message contents */
            aux_mcu_message_t* temp_send_message_pt = comms_hid_msgs_get_empty_hid_packet(is_message_from_usb, rcv_message_type, supposed_payload_length);
            memcpy((void*)temp_send_message_pt->hid_message.payload, (void*)rcv_msg->payload, supposed_payload_length);
            comms_aux_mcu_send_message(temp_send_message_pt);
            return;
        }

        case HID_CMD_GET_DEVICE_STATUS:
        {
            /* Get device status: call dedicated function */
            aux_mcu_message_t* temp_send_message_pt = comms_hid_msgs_get_empty_hid_packet(is_message_from_usb, rcv_message_type, 0);
            uint16_t payload_length = comms_hid_msgs_fill_get_status_message_answer(temp_send_message_pt->hid_message.payload_as_uint16);
            comms_hid_msgs_update_message_payload_length_fields(temp_send_message_pt, payload_length);
            comms_aux_mcu_send_message(temp_send_message_pt);
            return;
        }
        
        case HID_CMD_IM_LOCKED:
        {
            /* Inform that computer is locked */
            logic_user_inform_computer_locked_state(is_message_from_usb, TRUE);
            comms_hid_msgs_send_ack_nack_message(is_message_from_usb, rcv_message_type, TRUE);
            return;
        }
        
        case HID_CMD_IM_UNLOCKED:
        {
            /* Inform that computer is unlocked */
            logic_user_inform_computer_locked_state(is_message_from_usb, FALSE);
            comms_hid_msgs_send_ack_nack_message(is_message_from_usb, rcv_message_type, TRUE);
            return;
        }
        
        case HID_CMD_WAKE_UP_DEVICE:
        {
            /* Wake up the device */
            main_create_virtual_wheel_movement();
            comms_hid_msgs_send_ack_nack_message(is_message_from_usb, rcv_message_type, TRUE);
            return;
        }
        
        case HID_CMD_SET_CUST_BLE_NAME:
        {
            /* Setup custom bluetooth name: no check on payload as 0 termination on a fixed length string is in the called function */
            custom_fs_set_custom_ble_name(rcv_msg->payload);
            comms_hid_msgs_send_ack_nack_message(is_message_from_usb, rcv_message_type, TRUE);
            return;
        }
        
        case HID_CMD_GET_CUST_BLE_NAME:
        {
            aux_mcu_message_t* temp_send_message_pt = comms_hid_msgs_get_empty_hid_packet(is_message_from_usb, rcv_message_type, MEMBER_SIZE(custom_platform_settings_t, custom_ble_name));
            custom_fs_fetch_custom_ble_name(temp_send_message_pt->hid_message.payload);
            comms_aux_mcu_send_message(temp_send_message_pt);
            return;
        }        

        case HID_CMD_NIMH_RECONDITION:
        {
            uint32_t battery_discharge_time = UINT32_MAX;
            logic_power_battery_recondition(&battery_discharge_time);
            aux_mcu_message_t* temp_send_message_pt = comms_hid_msgs_get_empty_hid_packet(is_message_from_usb, rcv_message_type, sizeof(battery_discharge_time));
            temp_send_message_pt->hid_message.payload_as_uint32[0] = battery_discharge_time;
            comms_aux_mcu_send_message(temp_send_message_pt);
            return;
        }
        
        case HID_CMD_DISABLE_NO_PROMPT:
        {
            /* User logged in? */
            if (logic_security_is_smc_inserted_unlocked() != FALSE)
            {
                /* Do not set the flag if it's already set */
                if ((logic_user_get_user_security_flags() & USER_SEC_FLG_LOGIN_CONF) == 0)
                {
                    logic_user_set_user_security_flag(USER_SEC_FLG_LOGIN_CONF);
                }
                comms_hid_msgs_send_ack_nack_message(is_message_from_usb, rcv_message_type, TRUE);
            } 
            else
            {
                comms_hid_msgs_send_ack_nack_message(is_message_from_usb, rcv_message_type, FALSE);
            }
            return;
        }
        
        case HID_CMD_GET_DEVICE_INT_SN:
        {
            aux_mcu_message_t* temp_send_message_pt = comms_hid_msgs_get_empty_hid_packet(is_message_from_usb, rcv_message_type, sizeof(uint32_t));
            temp_send_message_pt->hid_message.payload_as_uint32[0] = custom_fs_get_platform_internal_serial_number();
            comms_aux_mcu_send_message(temp_send_message_pt);
            return;
        }
        
        case HID_CMD_PREPARE_SN_FLASH:
        {
            /* Check address length */
            if (custom_fs_get_platform_programmed_serial_number() == UINT32_MAX)
            {
                /* Send success byte as bluetooth enable is going to delay comms... */
                comms_hid_msgs_send_ack_nack_message(is_message_from_usb, rcv_message_type, TRUE);
                
                /* Enable BLE */
                logic_aux_mcu_enable_ble(FALSE);
                
                return;
            }
            else
            {
                /* Set failure byte */
                comms_hid_msgs_send_ack_nack_message(is_message_from_usb, rcv_message_type, FALSE);
                return;
            }        
        }
        
        case HID_CMD_SET_DEVICE_SN:
        {
            /* Check address length */
            if ((rcv_msg->payload_length == sizeof(uint32_t)) && (custom_fs_get_platform_programmed_serial_number() == UINT32_MAX))
            {
                /* Store new SN */
                custom_fs_program_serial_number(rcv_msg->payload_as_uint32[0]);

                /* Set success byte */
                comms_hid_msgs_send_ack_nack_message(is_message_from_usb, rcv_message_type, TRUE);
                return;
            }
            else
            {
                /* Set failure byte */
                comms_hid_msgs_send_ack_nack_message(is_message_from_usb, rcv_message_type, FALSE);
                return;
            }
        }
        
        case HID_CMD_SWITCH_OFF_NXT_DSC:
        {
            /* Set flag */
            logic_power_switch_off_after_usb_disconnect();
            
            /* Set success byte */
            comms_hid_msgs_send_ack_nack_message(is_message_from_usb, rcv_message_type, TRUE);
            return;            
        }
        
        case HID_CMD_ID_PLAT_INFO:
        {
            aux_mcu_message_t* temp_rx_message;
            aux_mcu_message_t* temp_tx_message_pt;
            
            /* Generate our packet */
            temp_tx_message_pt = comms_aux_mcu_get_empty_packet_ready_to_be_sent(AUX_MCU_MSG_TYPE_PLAT_DETAILS);
            
            /* Send message */
            comms_aux_mcu_send_message(temp_tx_message_pt);
            
            /* Wait for message from aux MCU */
            while(comms_aux_mcu_active_wait(&temp_rx_message, AUX_MCU_MSG_TYPE_PLAT_DETAILS, FALSE, -1) != RETURN_OK){}
            
            /* Copy message contents into send packet */
            temp_tx_message_pt = comms_hid_msgs_get_empty_hid_packet(is_message_from_usb, rcv_message_type, sizeof(temp_tx_message_pt->hid_message.platform_info));
            temp_tx_message_pt->hid_message.platform_info.main_mcu_fw_major = FW_MAJOR;
            temp_tx_message_pt->hid_message.platform_info.main_mcu_fw_minor = FW_MINOR;
            temp_tx_message_pt->hid_message.platform_info.aux_mcu_fw_major = temp_rx_message->aux_details_message.aux_fw_ver_major;
            temp_tx_message_pt->hid_message.platform_info.aux_mcu_fw_minor = temp_rx_message->aux_details_message.aux_fw_ver_minor;
            /* Programmed SN: if not set, return the internal one (also convenient for backward compatibility */
            temp_tx_message_pt->hid_message.platform_info.plat_serial_number = custom_fs_get_platform_programmed_serial_number();
            if (custom_fs_get_platform_programmed_serial_number() == UINT32_MAX)
            {
                temp_tx_message_pt->hid_message.platform_info.plat_serial_number = custom_fs_get_platform_internal_serial_number();
            }
            temp_tx_message_pt->hid_message.platform_info.memory_size = DBFLASH_CHIP;
            temp_tx_message_pt->hid_message.platform_info.bundle_version = custom_fs_get_platform_bundle_version();
            temp_tx_message_pt->hid_message.platform_info.plat_internal_serial_number = custom_fs_get_platform_internal_serial_number();
            
            /* Send message */
            comms_aux_mcu_send_message(temp_tx_message_pt);
            
            /* Rearm RX */
            comms_aux_arm_rx_and_clear_no_comms();
            return;
        }
        
        case HID_CMD_ID_SET_DATE:
        {
            /* Check address length */
            if (rcv_msg->payload_length == 6*sizeof(uint16_t))
            {
                /* Set Date */
                driver_timer_set_rtc_timestamp(rcv_msg->payload_as_uint16[0], rcv_msg->payload_as_uint16[1], rcv_msg->payload_as_uint16[2], rcv_msg->payload_as_uint16[3], rcv_msg->payload_as_uint16[4], rcv_msg->payload_as_uint16[5]);
                nodemgmt_set_current_date(nodemgmt_construct_date(rcv_msg->payload_as_uint16[0],rcv_msg->payload_as_uint16[1],rcv_msg->payload_as_uint16[2]));
                logic_device_set_time_set();
                
                /* Set success byte */
                comms_hid_msgs_send_ack_nack_message(is_message_from_usb, rcv_message_type, TRUE);
                return;
            }
            else
            {
                /* Set failure byte */
                comms_hid_msgs_send_ack_nack_message(is_message_from_usb, rcv_message_type, FALSE);
                return;
            }
        }
        
        case HID_CMD_ID_GET_32B_RNG:
        {
            aux_mcu_message_t* temp_tx_message_pt = comms_hid_msgs_get_empty_hid_packet(is_message_from_usb, rcv_message_type, 32);
            rng_fill_array(temp_tx_message_pt->hid_message.payload, 32);
            comms_aux_mcu_send_message(temp_tx_message_pt);
            return;
        }

        case HID_CMD_GET_CUR_CARD_CPZ:
        {
            /* Smartcard unlocked or unknown card inserted */
            if ((logic_security_is_smc_inserted_unlocked() != FALSE) || (gui_dispatcher_get_current_screen() == GUI_SCREEN_INSERTED_UNKNOWN))
            {
                aux_mcu_message_t* temp_tx_message_pt = comms_hid_msgs_get_empty_hid_packet(is_message_from_usb, rcv_message_type, MEMBER_SIZE(cpz_lut_entry_t,cards_cpz));
                smartcard_highlevel_read_code_protected_zone(temp_tx_message_pt->hid_message.payload);
                comms_aux_mcu_send_message(temp_tx_message_pt);
                return;
            }
            else
            {
                /* Set failure byte */
                comms_hid_msgs_send_ack_nack_message(is_message_from_usb, rcv_message_type, FALSE);
                return; 
            }         
        }

        case HID_CMD_GET_DEVICE_SETTINGS:
        {
            /* Get a dump of all device settings */
            aux_mcu_message_t* temp_tx_message_pt = comms_hid_msgs_get_empty_hid_packet(is_message_from_usb, rcv_message_type, 0);
            uint16_t payload_length = custom_fs_settings_get_dump(temp_tx_message_pt->hid_message.payload);
            comms_hid_msgs_update_message_payload_length_fields(temp_tx_message_pt, payload_length);
            comms_aux_mcu_send_message(temp_tx_message_pt);
            return; 
        }

        case HID_CMD_SET_DEVICE_SETTINGS:
        {
            /* Store previous menu settings */
            BOOL prev_menu_log_fav_setting = custom_fs_settings_get_device_setting(SETTINGS_LOGIN_AND_FAV_INVERTED);
            
            /* Store all settings */
            custom_fs_settings_store_dump(rcv_msg->payload);
            
            /* Wait for possible end of transfer */
            #ifdef OLED_INTERNAL_FRAME_BUFFER
            /* Wait for a possible ongoing previous flush */
            sh1122_check_for_flush_and_terminate(&plat_oled_descriptor);
            #endif
            
            /* Actions run when settings are updated */
            sh1122_set_contrast_current(&plat_oled_descriptor, logic_device_get_screen_current_for_current_use());
            gui_menu_update_menus();
            timer_delay_ms(15);
            
            /* Apply possible screen inversion */
            BOOL screen_inverted = logic_power_get_power_source() == BATTERY_POWERED?(BOOL)custom_fs_settings_get_device_setting(SETTINGS_LEFT_HANDED_ON_BATTERY):(BOOL)custom_fs_settings_get_device_setting(SETTINGS_LEFT_HANDED_ON_USB);
            sh1122_set_screen_invert(&plat_oled_descriptor, screen_inverted);
            timer_delay_ms(15);
            
            /* Did we change the menu order settings? */
            if ((prev_menu_log_fav_setting != custom_fs_settings_get_device_setting(SETTINGS_LOGIN_AND_FAV_INVERTED)) && (gui_dispatcher_get_current_screen() == GUI_SCREEN_MAIN_MENU) && (sh1122_is_oled_on(&plat_oled_descriptor) != FALSE))
            {
                gui_dispatcher_get_back_to_current_screen();
            }
            
            /* Set ack, leave same command id */
            comms_hid_msgs_send_ack_nack_message(is_message_from_usb, rcv_message_type, TRUE);
            return;
        }
        
        case HID_CMD_GET_USER_SETTINGS:
        {
            /* Get user security settings */
            if (logic_security_is_smc_inserted_unlocked() != FALSE)
            {
                aux_mcu_message_t* temp_tx_message_pt = comms_hid_msgs_get_empty_hid_packet(is_message_from_usb, rcv_message_type, sizeof(uint16_t));
                temp_tx_message_pt->hid_message.payload_as_uint16[0] = logic_user_get_user_security_flags();
                comms_aux_mcu_send_message(temp_tx_message_pt);
                return;
            } 
            else
            {
                aux_mcu_message_t* temp_tx_message_pt = comms_hid_msgs_get_empty_hid_packet(is_message_from_usb, rcv_message_type, 0);
                comms_aux_mcu_send_message(temp_tx_message_pt);
                return;
            }
        }
        
        case HID_CMD_GET_CATEGORIES_STR:
        {            
            _Static_assert(sizeof(nodemgmt_user_category_strings_t) == sizeof(nodemgmt_user_category_strings_t), "get/set categories message and db structure aren't the same");
            
            /* Get user categories strings */
            if (logic_security_is_smc_inserted_unlocked() != FALSE)
            {
                aux_mcu_message_t* temp_tx_message_pt = comms_hid_msgs_get_empty_hid_packet(is_message_from_usb, rcv_message_type, sizeof(nodemgmt_user_category_strings_t));
                nodemgmt_get_category_strings((nodemgmt_user_category_strings_t*)&temp_tx_message_pt->hid_message.get_set_cat_strings);
                comms_aux_mcu_send_message(temp_tx_message_pt);
                return;
                
            } 
            else
            {
                /* Set nack, leave same command id */
                comms_hid_msgs_send_ack_nack_message(is_message_from_usb, rcv_message_type, FALSE);
                return;
            }
        }
        
        case HID_CMD_SET_CATEGORIES_STR:
        {
            _Static_assert(sizeof(nodemgmt_user_category_strings_t) == sizeof(nodemgmt_user_category_strings_t), "get/set categories message and db structure aren't the same");
            
            /* Set user categories strings */
            if ((rcv_msg->payload_length == sizeof(nodemgmt_user_category_strings_t)) && (logic_security_is_smc_inserted_unlocked() != FALSE))
            {
                /* Copy locally */
                nodemgmt_user_category_strings_t category_strings;
                memcpy(&category_strings, &rcv_msg->get_set_cat_strings, sizeof(category_strings));
                
                if ((logic_security_is_management_mode_set() != FALSE) || (gui_prompts_ask_for_one_line_confirmation(SET_CAT_STRINGS_TEXT_ID, TRUE, FALSE, TRUE) == MINI_INPUT_RET_YES))
                {
                    /* Store category strings */
                    nodemgmt_set_category_strings(&category_strings);
                    
                    /* Set ack, leave same command id */
                    comms_hid_msgs_send_ack_nack_message(is_message_from_usb, rcv_message_type, TRUE);
                } 
                else
                {
                    /* Set nack, leave same command id */
                    comms_hid_msgs_send_ack_nack_message(is_message_from_usb, rcv_message_type, FALSE);
                }                  
                
                /* Update screen */
                gui_dispatcher_get_back_to_current_screen();           
            }
            else
            {
                /* Set nack, leave same command id */
                comms_hid_msgs_send_ack_nack_message(is_message_from_usb, rcv_message_type, FALSE);
            }
            
            return; 
        }    

        case HID_CMD_RESET_UNKNOWN_CARD:
        {
            /* Resetting card if an unknown one is inserted */
            if (gui_dispatcher_get_current_screen() == GUI_SCREEN_INSERTED_UNKNOWN)
            {
                /* Prompt user to unlock the card */
                logic_device_activity_detected();
                if (logic_smartcard_user_unlock_process() == UNLOCK_OK_RET)
                {
                    /* Erase card! */
                    smartcard_highlevel_erase_smartcard();

                    /* Set next screen */
                    gui_dispatcher_set_current_screen(GUI_SCREEN_INSERTED_INVALID, TRUE, GUI_INTO_MENU_TRANSITION);

                    /* Set success byte */
                    comms_hid_msgs_send_ack_nack_message(is_message_from_usb, rcv_message_type, TRUE);
                }
                else
                {
                    /* Set failure byte */
                    comms_hid_msgs_send_ack_nack_message(is_message_from_usb, rcv_message_type, FALSE);
                }

                /* Update screen */
                gui_dispatcher_get_back_to_current_screen();
            }
            else
            {
                /* Set failure byte */
                comms_hid_msgs_send_ack_nack_message(is_message_from_usb, rcv_message_type, FALSE);
            }
            
            return;
        }

        case HID_CMD_GET_NB_FREE_USERS:
        {
            uint8_t temp_uint8;

            /* Get number of free users */
            aux_mcu_message_t* temp_tx_message_pt = comms_hid_msgs_get_empty_hid_packet(is_message_from_usb, rcv_message_type, sizeof(uint8_t));
            temp_tx_message_pt->hid_message.payload[0] = custom_fs_get_nb_free_cpz_lut_entries(&temp_uint8);
            comms_aux_mcu_send_message(temp_tx_message_pt);
            return;
        }

        case HID_CMD_LOCK_DEVICE:
        {
            /* Lock device */
            if (logic_security_is_smc_inserted_unlocked() != FALSE)
            {
                /* Set flag */
                logic_user_set_user_to_be_logged_off_flag();

                /* Set success byte */
                comms_hid_msgs_send_ack_nack_message(is_message_from_usb, rcv_message_type, TRUE);
                return;
            }
            else
            {
                /* Set failure byte */
                comms_hid_msgs_send_ack_nack_message(is_message_from_usb, rcv_message_type, FALSE);
                return;
            }
        }
        
        case HID_CMD_SET_CUR_CATEGORY:
        {
            /* Set current category, only if no category has been selected */
            if ((nodemgmt_get_current_category() == 0) && (rcv_msg->payload[0] < NODEMGMT_NB_MAX_CATEGORIES))
            {
                /* Set flag */
                logic_user_set_category_to_switch_to(rcv_msg->payload[0]);

                /* Set success byte */
                comms_hid_msgs_send_ack_nack_message(is_message_from_usb, rcv_message_type, TRUE);
                return;
            }
            else
            {
                /* Set failure byte */
                comms_hid_msgs_send_ack_nack_message(is_message_from_usb, rcv_message_type, FALSE);
                return;
            }
        }        
        
        case HID_CMD_GET_START_PARENTS:
        {            
            /* Return correct size & data */
            aux_mcu_message_t* temp_tx_message_pt = comms_hid_msgs_get_empty_hid_packet(is_message_from_usb, rcv_message_type, 0);
            uint16_t payload_length = nodemgmt_get_start_addresses(temp_tx_message_pt->hid_message.payload_as_uint16)*sizeof(uint16_t);
            comms_hid_msgs_update_message_payload_length_fields(temp_tx_message_pt, payload_length);
            comms_aux_mcu_send_message(temp_tx_message_pt);
            return;
        }

        case HID_CMD_SET_CRED_ST_PARENT:
        {
            /* Check address length */
            if (rcv_msg->payload_length == 2*sizeof(uint16_t))
            {
                /* Store new address */
                nodemgmt_set_cred_start_address(rcv_msg->payload_as_uint16[1], rcv_msg->payload_as_uint16[0]);

                /* Set success byte */
                comms_hid_msgs_send_ack_nack_message(is_message_from_usb, rcv_message_type, TRUE);
                return;
            }
            else
            {
                /* Set failure byte */
                comms_hid_msgs_send_ack_nack_message(is_message_from_usb, rcv_message_type, FALSE);
                return;
            }
        }

        case HID_CMD_SET_DATA_ST_PARENT:
        {
            /* Check address length */
            if (rcv_msg->payload_length == 2*sizeof(uint16_t))
            {
                /* Store new address */
                nodemgmt_set_data_start_address(rcv_msg->payload_as_uint16[1], rcv_msg->payload_as_uint16[0]);

                /* Set success byte */
                comms_hid_msgs_send_ack_nack_message(is_message_from_usb, rcv_message_type, TRUE);
                return;
            }
            else
            {
                /* Set failure byte */
                comms_hid_msgs_send_ack_nack_message(is_message_from_usb, rcv_message_type, FALSE);
                return;
            }
        }

        case HID_CMD_SET_START_PARENTS:
        {
            /* Check address length */
            if (rcv_msg->payload_length == (MEMBER_SIZE(nodemgmt_profile_main_data_t, cred_start_addresses) + MEMBER_SIZE(nodemgmt_profile_main_data_t, data_start_addresses)))
            {
                /* Store new addresses */
                nodemgmt_set_start_addresses(rcv_msg->payload_as_uint16);

                /* Set success byte */
                comms_hid_msgs_send_ack_nack_message(is_message_from_usb, rcv_message_type, TRUE);
                return;
            }
            else
            {
                /* Set failure byte */
                comms_hid_msgs_send_ack_nack_message(is_message_from_usb, rcv_message_type, FALSE);
                return;
            }
        }

        case HID_CMD_GET_FREE_NODES:
        {
            /* Check for correct number of args and that not too many free slots have been requested */
            if ((rcv_msg->payload_length == 3*sizeof(uint16_t)) && (((uint32_t)(rcv_msg->payload_as_uint16[1]) + (uint32_t)(rcv_msg->payload_as_uint16[2])) <= (max_payload_size/sizeof(uint16_t))))
            {
                aux_mcu_message_t* temp_tx_message_pt = comms_hid_msgs_get_empty_hid_packet(is_message_from_usb, rcv_message_type, 0);
                uint16_t nb_nodes_found = nodemgmt_find_free_nodes(rcv_msg->payload_as_uint16[1], temp_tx_message_pt->hid_message.payload_as_uint16, rcv_msg->payload_as_uint16[2], &(temp_tx_message_pt->hid_message.payload_as_uint16[rcv_msg->payload_as_uint16[1]]), nodemgmt_page_from_address(rcv_msg->payload_as_uint16[0]), nodemgmt_node_from_address(rcv_msg->payload_as_uint16[0]));
                comms_hid_msgs_update_message_payload_length_fields(temp_tx_message_pt, nb_nodes_found*sizeof(uint16_t));
                comms_aux_mcu_send_message(temp_tx_message_pt);
                return;
            }
            else
            {
                /* Send failure */
                comms_hid_msgs_send_ack_nack_message(is_message_from_usb, rcv_message_type, FALSE);
                return;
            }
        }
        
        case HID_CMD_END_MMM:
        {            
            if (logic_security_is_management_mode_set() != FALSE)
            {
                /* Device state is going to change... */
                logic_device_set_state_changed();
                
                /* Clear bool */
                logic_device_activity_detected();
                logic_security_clear_management_mode();
                
                /* Trigger dedicated actions */
                nodemgmt_trigger_db_ext_changed_actions();
                
                /* Set next screen */
                gui_dispatcher_set_current_screen(GUI_SCREEN_MAIN_MENU, TRUE, GUI_INTO_MENU_TRANSITION);
                gui_dispatcher_get_back_to_current_screen();
                nodemgmt_scan_node_usage();
            }
            
            /* Set ack, leave same command id */
            comms_hid_msgs_send_ack_nack_message(is_message_from_usb, rcv_message_type, TRUE);
            return;
        }
        
        case HID_CMD_START_MMM:
        {            
            /* Smartcard unlocked? */
            if (logic_security_is_smc_inserted_unlocked() != FALSE)
            {                
                /* Prompt the user */
                if (gui_prompts_ask_for_one_line_confirmation(ID_STRING_ENTER_MMM, TRUE, FALSE, TRUE) == MINI_INPUT_RET_YES)
                {
                    /* Device state is going to change... */
                    logic_device_set_state_changed();
                    
                    if ((logic_user_get_user_security_flags() & USER_SEC_FLG_PIN_FOR_MMM) != 0)
                    {
                        /* As the following call takes a little while, cheat */
                        sh1122_fade_into_darkness(&plat_oled_descriptor, OLED_IN_OUT_TRANS);
                        
                        /* Require user to reauth himself */
                        if (logic_smartcard_remove_card_and_reauth_user(FALSE) == RETURN_OK)
                        {
                            /* Approved, set next screen */
                            gui_dispatcher_set_current_screen(GUI_SCREEN_MEMORY_MGMT, TRUE, GUI_INTO_MENU_TRANSITION);
                            
                            /* Update current state */
                            logic_security_set_management_mode(is_message_from_usb);
                            
                            /* Set success byte */
                            comms_hid_msgs_send_ack_nack_message(is_message_from_usb, rcv_message_type, TRUE);
                        } 
                        else
                        {
                            /* Couldn't reauth, set to inserted lock */
                            gui_dispatcher_set_current_screen(GUI_SCREEN_INSERTED_LCK, TRUE, GUI_OUTOF_MENU_TRANSITION);

                            /* Set failure byte */
                            comms_hid_msgs_send_ack_nack_message(is_message_from_usb, rcv_message_type, FALSE);
                        }
                    } 
                    else
                    {
                        /* Approved, set next screen */
                        gui_dispatcher_set_current_screen(GUI_SCREEN_MEMORY_MGMT, TRUE, GUI_INTO_MENU_TRANSITION);
                        
                        /* Update current state */
                        logic_security_set_management_mode(is_message_from_usb);
                        
                        /* Set success byte */
                        comms_hid_msgs_send_ack_nack_message(is_message_from_usb, rcv_message_type, TRUE);
                    }
                }
                else
                {
                    /* Set failure byte */
                    comms_hid_msgs_send_ack_nack_message(is_message_from_usb, rcv_message_type, FALSE);
                }
            
                /* Go back to old or updated screen */
                gui_dispatcher_get_back_to_current_screen();
            } 
            else
            {
                /* Set failure byte */
                comms_hid_msgs_send_ack_nack_message(is_message_from_usb, rcv_message_type, FALSE);
            }
            
            /* Return success or failure */
            return;
        }
        
        case HID_CMD_SCAN_FILE_ID:
        case HID_CMD_SCAN_NOTE_ID:
        {
            /* Check address length */
            if ((rcv_msg->payload_length == sizeof(uint16_t)) && (logic_security_is_smc_inserted_unlocked() != FALSE))
            {
                /* Prepare message to reply */
                aux_mcu_message_t* temp_send_message_pt = comms_hid_msgs_get_empty_hid_packet(is_message_from_usb, rcv_message_type, 0);
                                
                /* Get next node address */
                _Static_assert(sizeof(cust_char_t) == sizeof(uint16_t), "Invalid size reuse");
                uint16_t next_node_address = logic_database_search_for_next_data_parent_after_addr(rcv_msg->payload_as_uint16[0], data_type_for_operation, &temp_send_message_pt->hid_message.payload_as_cust_char_t[1]);
                
                /* Update next node address in reply */
                temp_send_message_pt->hid_message.payload_as_uint16[0] = next_node_address;
                
                /* Update payload length (if nothing is found, nothing is stored, and field therefore is 0s, and inner functions sanitize service name) */
                if (next_node_address == NODE_ADDR_NULL)
                {
                    comms_hid_msgs_update_message_payload_length_fields(temp_send_message_pt, sizeof(cust_char_t));
                } 
                else
                {
                    comms_hid_msgs_update_message_payload_length_fields(temp_send_message_pt, sizeof(cust_char_t) + utils_strlen(&temp_send_message_pt->hid_message.payload_as_uint16[1])*sizeof(cust_char_t) + sizeof(cust_char_t));                    
                }
                
                /* Send message */
                comms_aux_mcu_send_message(temp_send_message_pt);
                return;
            }
            else
            {
                /* Set nack, leave same command id */
                comms_hid_msgs_send_ack_nack_message(is_message_from_usb, rcv_message_type, FALSE);
                return;
            }            
        }
        
        case HID_CMD_READ_NODE:
        {
            /* Check address length */
            if (rcv_msg->payload_length == sizeof(uint16_t))
            {
                node_type_te temp_node_type;
                
                /* Check user permission */
                if (nodemgmt_check_user_permission(rcv_msg->payload_as_uint16[0], &temp_node_type) == RETURN_OK)
                {
                    if ((temp_node_type == NODE_TYPE_PARENT) || (temp_node_type == NODE_TYPE_PARENT_DATA) || (temp_node_type == NODE_TYPE_NULL))
                    {
                        /* Read and send parent node */
                        aux_mcu_message_t* temp_tx_message_pt = comms_hid_msgs_get_empty_hid_packet(is_message_from_usb, rcv_message_type, sizeof(parent_node_t));
                        nodemgmt_read_parent_node_data_block_from_flash(rcv_msg->payload_as_uint16[0], (parent_node_t*)temp_tx_message_pt->hid_message.payload_as_uint16);
                        comms_aux_mcu_send_message(temp_tx_message_pt);
                        return;
                    } 
                    else
                    {
                        /* Read and send child node */
                        aux_mcu_message_t* temp_tx_message_pt = comms_hid_msgs_get_empty_hid_packet(is_message_from_usb, rcv_message_type, sizeof(child_node_t));
                        nodemgmt_read_child_node_data_block_from_flash(rcv_msg->payload_as_uint16[0], (child_node_t*)temp_tx_message_pt->hid_message.payload_as_uint16);
                        comms_aux_mcu_send_message(temp_tx_message_pt);
                        return;
                    }
                } 
                else
                {
                    /* Set nack, leave same command id */
                    comms_hid_msgs_send_ack_nack_message(is_message_from_usb, rcv_message_type, FALSE);
                    return;
                }
            } 
            else
            {
                /* Set nack, leave same command id */
                comms_hid_msgs_send_ack_nack_message(is_message_from_usb, rcv_message_type, FALSE);
                return;
            }
        }

        case HID_CMD_WRITE_NODE:
        {
            node_type_te temp_node_type_te;

            /* Check for big or small node size */
            if ((rcv_msg->payload_length == sizeof(uint16_t) + sizeof(child_node_t)) \
                    && (nodemgmt_check_user_permission(rcv_msg->payload_as_uint16[0], &temp_node_type_te) == RETURN_OK) \
                    && (nodemgmt_check_user_permission(nodemgmt_get_incremented_address(rcv_msg->payload_as_uint16[0]), &temp_node_type_te) == RETURN_OK))
            {
                /* big node */
                nodemgmt_write_child_node_block_to_flash(rcv_msg->payload_as_uint16[0], (child_node_t*)&(rcv_msg->payload_as_uint16[1]), FALSE);

                /* Set success byte */
                comms_hid_msgs_send_ack_nack_message(is_message_from_usb, rcv_message_type, TRUE);
                return;
            }
            else if ((rcv_msg->payload_length == sizeof(uint16_t) + sizeof(parent_node_t)) \
                    && (nodemgmt_check_user_permission(rcv_msg->payload_as_uint16[0], &temp_node_type_te) == RETURN_OK))
            {
                /* small node */
                nodemgmt_write_parent_node_data_block_to_flash(rcv_msg->payload_as_uint16[0], (parent_node_t*)&(rcv_msg->payload_as_uint16[1]));

                /* Set success byte */
                comms_hid_msgs_send_ack_nack_message(is_message_from_usb, rcv_message_type, TRUE);
                return;
            }
            else
            {
                /* Set failure byte */
                comms_hid_msgs_send_ack_nack_message(is_message_from_usb, rcv_message_type, FALSE);
                return;
            }
        }

        case HID_CMD_GET_USER_CHANGE_NB :
        {
            /* Smartcard unlocked? */
            if (logic_security_is_smc_inserted_unlocked() != FALSE)
            {
                aux_mcu_message_t* temp_tx_message_pt = comms_hid_msgs_get_empty_hid_packet(is_message_from_usb, rcv_message_type, 2*sizeof(uint32_t));
                temp_tx_message_pt->hid_message.payload_as_uint32[0] = nodemgmt_get_cred_change_number();
                temp_tx_message_pt->hid_message.payload_as_uint32[1] = nodemgmt_get_data_change_number();
                comms_aux_mcu_send_message(temp_tx_message_pt);
                return;
            }
            else
            {
                /* Set failure byte */
                comms_hid_msgs_send_ack_nack_message(is_message_from_usb, rcv_message_type, FALSE);
                return;
            }
        }

        case HID_CMD_SET_CRED_CHANGE_NB :
        {
            /* Check address length */
            if (rcv_msg->payload_length == sizeof(uint32_t))
            {
                /* Store change number */
                nodemgmt_set_cred_change_number(rcv_msg->payload_as_uint32[0]);

                /* Set success byte */
                comms_hid_msgs_send_ack_nack_message(is_message_from_usb, rcv_message_type, TRUE);
                return;
            }
            else
            {
                /* Set failure byte */
                comms_hid_msgs_send_ack_nack_message(is_message_from_usb, rcv_message_type, FALSE);
                return;
            }
        }

        case HID_CMD_SET_DATA_CHANGE_NB :
        {
            /* Check address length */
            if (rcv_msg->payload_length == sizeof(uint32_t))
            {
                /* Store change number */
                nodemgmt_set_data_change_number(rcv_msg->payload_as_uint32[0]);

                /* Set success byte */
                comms_hid_msgs_send_ack_nack_message(is_message_from_usb, rcv_message_type, TRUE);
                return;
            }
            else
            {
                /* Set failure byte */
                comms_hid_msgs_send_ack_nack_message(is_message_from_usb, rcv_message_type, FALSE);
                return;
            }
        }

        case HID_CMD_GET_CTR_VALUE:
        {
            /* Read CTR value from flash and send it */
            aux_mcu_message_t* temp_tx_message_pt = comms_hid_msgs_get_empty_hid_packet(is_message_from_usb, rcv_message_type, MEMBER_SIZE(nodemgmt_profile_main_data_t, current_ctr));
            nodemgmt_read_profile_ctr(temp_tx_message_pt->hid_message.payload);
            comms_aux_mcu_send_message(temp_tx_message_pt);
            return;
        }

        case HID_CMD_SET_CTR_VALUE:
        {
            if (rcv_msg->payload_length == MEMBER_SIZE(nodemgmt_profile_main_data_t, current_ctr))
            {
                nodemgmt_set_profile_ctr(rcv_msg->payload);

                /* Set success byte */
                comms_hid_msgs_send_ack_nack_message(is_message_from_usb, rcv_message_type, TRUE);
                return;  
            }
            else
            {
                /* Set failure byte */
                comms_hid_msgs_send_ack_nack_message(is_message_from_usb, rcv_message_type, FALSE);
                return;
            }
        }

        case HID_CMD_SET_FAVORITE:
        {
            if (rcv_msg->payload_length == 4*sizeof(uint16_t))
            {
                nodemgmt_set_favorite(rcv_msg->payload_as_uint16[0], rcv_msg->payload_as_uint16[1], rcv_msg->payload_as_uint16[2], rcv_msg->payload_as_uint16[3]);
                
                /* Set success byte */
                comms_hid_msgs_send_ack_nack_message(is_message_from_usb, rcv_message_type, TRUE);
                return;
            }
            else
            {
                /* Set failure byte */
                comms_hid_msgs_send_ack_nack_message(is_message_from_usb, rcv_message_type, FALSE);
                return;
            }        
        }

        case HID_CMD_GET_FAVORITE:
        {
            if (rcv_msg->payload_length == 2*sizeof(uint16_t))
            {
                aux_mcu_message_t* temp_tx_message_pt = comms_hid_msgs_get_empty_hid_packet(is_message_from_usb, rcv_message_type, 2*sizeof(uint16_t));
                nodemgmt_read_favorite(rcv_msg->payload_as_uint16[0], rcv_msg->payload_as_uint16[1], &(temp_tx_message_pt->hid_message.payload_as_uint16[0]), &(temp_tx_message_pt->hid_message.payload_as_uint16[1]));
                comms_aux_mcu_send_message(temp_tx_message_pt);
                return;
            }
            else
            {
                /* Set failure byte */
                comms_hid_msgs_send_ack_nack_message(is_message_from_usb, rcv_message_type, FALSE);
                return;
            }       
        }

        case HID_CMD_GET_CPZ_LUT_ENTRY:
        {
            aux_mcu_message_t* temp_tx_message_pt = comms_hid_msgs_get_empty_hid_packet(is_message_from_usb, rcv_message_type, sizeof(cpz_lut_entry_t));
            logic_encryption_get_cpz_lut_entry(temp_tx_message_pt->hid_message.payload);
            comms_aux_mcu_send_message(temp_tx_message_pt);
            return;
        }

        case HID_CMD_GET_FAVORITES:
        {
            /* Return correct size & data */
            aux_mcu_message_t* temp_tx_message_pt = comms_hid_msgs_get_empty_hid_packet(is_message_from_usb, rcv_message_type, 0);
            uint16_t payload_length = nodemgmt_get_favorites(temp_tx_message_pt->hid_message.payload_as_uint16)*sizeof(favorite_addr_t);
            comms_hid_msgs_update_message_payload_length_fields(temp_tx_message_pt, payload_length);
            comms_aux_mcu_send_message(temp_tx_message_pt);
            return;
        }
        
        case HID_CMD_INFORM_CUR_SVC:
        {
            /* Fixed duration to answer */
            uint16_t temp_timer_id = timer_get_and_start_timer(100);
            
            /* Get service string length */
            uint16_t service_length = utils_strnlen(rcv_msg->payload_as_cust_char_t, max_payload_size/sizeof(cust_char_t));
            
            /* Length checks */
            if ((service_length < max_payload_size/sizeof(cust_char_t)) && (service_length < MEMBER_ARRAY_SIZE(parent_cred_node_t, service)))
            {
                /* Do we know the service? */
                uint16_t parent_address = logic_database_search_service(rcv_msg->payload_as_cust_char_t, COMPARE_MODE_MATCH, TRUE, NODEMGMT_STANDARD_CRED_TYPE_ID);
                uint16_t temp_child_address, temp_other_child_address;
                
                /* Do we know the service? */
                if (parent_address != NODE_ADDR_NULL)
                {
                    /* See how many credentials there are for this service */
                    uint16_t nb_logins_for_cred = logic_database_get_number_of_creds_for_service(parent_address, &temp_child_address, &temp_other_child_address, TRUE);
                    
                    /* Set preferred starting address */
                    if (nb_logins_for_cred != 0)
                    {
                        logic_user_set_preferred_starting_service(parent_address);
                    }
                }
            }
            
            /* Wait for fixed timeout */
            while (timer_has_allocated_timer_expired(temp_timer_id, TRUE) == TIMER_RUNNING);
            timer_deallocate_timer(temp_timer_id);
            
            /* Send ACK */
            comms_hid_msgs_send_ack_nack_message(is_message_from_usb, rcv_message_type, TRUE);
            return;
        }
        
        case HID_CMD_ID_GET_CRED:
        case HID_CMD_GET_TOTP_CODE:
        {            
            /* Incorrect service name index */
            if (rcv_msg->get_credential_request.service_name_index != 0)
            {
                aux_mcu_message_t* temp_tx_message_pt = comms_hid_msgs_get_empty_hid_packet(is_message_from_usb, rcv_message_type, 0);
                comms_aux_mcu_send_message(temp_tx_message_pt);
                return;
            }
            
            /* Empty service name */
            if (rcv_msg->get_credential_request.concatenated_strings[0] == 0)
            {
                aux_mcu_message_t* temp_tx_message_pt = comms_hid_msgs_get_empty_hid_packet(is_message_from_usb, rcv_message_type, 0);
                comms_aux_mcu_send_message(temp_tx_message_pt);
                return;
            }
            
            /* Max string length */
            uint16_t max_cust_char_length = (max_payload_size \
                                            - sizeof(rcv_msg->get_credential_request.service_name_index) \
                                            - sizeof(rcv_msg->get_credential_request.login_name_index))/sizeof(cust_char_t);
            
            /* Get service string length */
            uint16_t service_length = utils_strnlen(&(rcv_msg->get_credential_request.concatenated_strings[0]), max_cust_char_length);
            
            /* Too long length */
            if (service_length == max_cust_char_length)
            {
                aux_mcu_message_t* temp_tx_message_pt = comms_hid_msgs_get_empty_hid_packet(is_message_from_usb, rcv_message_type, 0);
                comms_aux_mcu_send_message(temp_tx_message_pt);
                return;
            }
            
            /* Reduce max length */
            max_cust_char_length -= (service_length + 1);
            
            /* Is the login specified? */
            if (rcv_msg->get_credential_request.login_name_index == UINT16_MAX)
            {                 
                /* Only allowed for the get cred message */
                if (rcv_message_type == HID_CMD_ID_GET_CRED)
                {
                    /* Request user to send credential */
                    logic_user_usb_get_credential(&(rcv_msg->get_credential_request.concatenated_strings[0]), 0, is_message_from_usb, FALSE);
                } 
                else
                {
                    aux_mcu_message_t* temp_tx_message_pt = comms_hid_msgs_get_empty_hid_packet(is_message_from_usb, rcv_message_type, 0);
                    comms_aux_mcu_send_message(temp_tx_message_pt);
                }
                return;         
            } 
            else
            {
                /* Check correct index */
                if (rcv_msg->get_credential_request.login_name_index != service_length + 1)
                {
                    aux_mcu_message_t* temp_tx_message_pt = comms_hid_msgs_get_empty_hid_packet(is_message_from_usb, rcv_message_type, 0);
                    comms_aux_mcu_send_message(temp_tx_message_pt);
                    return;
                }
                
                /* Check login format */
                uint16_t login_length = utils_strnlen(&(rcv_msg->get_credential_request.concatenated_strings[rcv_msg->get_credential_request.login_name_index]), max_cust_char_length);
                
                /* Too long length */
                if (login_length == max_cust_char_length)
                {
                    aux_mcu_message_t* temp_tx_message_pt = comms_hid_msgs_get_empty_hid_packet(is_message_from_usb, rcv_message_type, 0);
                    comms_aux_mcu_send_message(temp_tx_message_pt);
                    return;
                }
                
                /* Request user to send credential */
                logic_user_usb_get_credential(&(rcv_msg->get_credential_request.concatenated_strings[0]), &(rcv_msg->get_credential_request.concatenated_strings[rcv_msg->get_credential_request.login_name_index]), is_message_from_usb, (rcv_message_type==HID_CMD_GET_TOTP_CODE)?TRUE:FALSE);
                return;
            }
        }

        case HID_CMD_CHECK_PASSWORD:
        {
            if (timer_has_timer_expired(TIMER_CHECK_PASSWORD, FALSE) != TIMER_EXPIRED)
            {
                /* Timer hasn't expired... do not allow check */
                aux_mcu_message_t* temp_tx_message_pt = comms_hid_msgs_get_empty_hid_packet(is_message_from_usb, rcv_message_type, sizeof(uint8_t));
                temp_tx_message_pt->hid_message.payload[0] = HID_1BYTE_NA;
                comms_aux_mcu_send_message(temp_tx_message_pt);
                return;
            }
            else
            {            
                /********************************/
                /* Here comes the sanity checks */
                /********************************/
            
                /* Incorrect service name index */
                if (rcv_msg->check_credential.service_name_index != 0)
                {
                    comms_hid_msgs_send_ack_nack_message(is_message_from_usb, rcv_message_type, FALSE);
                    return;
                }
            
                /* Empty service name */
                if (rcv_msg->check_credential.concatenated_strings[0] == 0)
                {
                    comms_hid_msgs_send_ack_nack_message(is_message_from_usb, rcv_message_type, FALSE);
                    return;
                }
            
                /* Sequential order check */
                uint16_t temp_length = 0;
                uint16_t prev_length = 0;
                uint16_t prev_check_index = 0;
                uint16_t current_check_index = 0;
                uint16_t check_indexes[3] = {   rcv_msg->check_credential.service_name_index, \
                                                rcv_msg->check_credential.login_name_index, \
                                                rcv_msg->check_credential.password_index};
                uint16_t max_cust_char_length = (max_payload_size \
                                                - sizeof(rcv_msg->check_credential.service_name_index) \
                                                - sizeof(rcv_msg->check_credential.login_name_index) \
                                                - sizeof(rcv_msg->check_credential.password_index))/sizeof(cust_char_t);
            
                /* Check all fields */                              
                for (uint16_t i = 0; i < ARRAY_SIZE(check_indexes); i++)
                {
                    current_check_index = check_indexes[i];
                
                    /* If index is correct */
                    if (current_check_index != prev_check_index + prev_length)
                    {
                        comms_hid_msgs_send_ack_nack_message(is_message_from_usb, rcv_message_type, FALSE);
                        return;
                    }
                    
                    /* Get string length */
                    temp_length = utils_strnlen(&(rcv_msg->check_credential.concatenated_strings[current_check_index]), max_cust_char_length);
                    
                    /* Too long length */
                    if (temp_length == max_cust_char_length)
                    {
                        comms_hid_msgs_send_ack_nack_message(is_message_from_usb, rcv_message_type, FALSE);
                        return;
                    }
                    
                    /* Store previous index & length */
                    prev_length = temp_length + 1;
                    prev_check_index = current_check_index;
                    
                    /* Reduce max length */
                    max_cust_char_length -= (temp_length + 1);
                }    
            
                /* Proceed to other logic */
                if (logic_user_check_credential(    &(rcv_msg->check_credential.concatenated_strings[rcv_msg->check_credential.service_name_index]),\
                                                    &(rcv_msg->check_credential.concatenated_strings[rcv_msg->check_credential.login_name_index]),\
                                                    &(rcv_msg->check_credential.concatenated_strings[rcv_msg->check_credential.password_index])) == RETURN_OK)
                {
                    comms_hid_msgs_send_ack_nack_message(is_message_from_usb, rcv_message_type, TRUE);
                    return;           
                }
                else
                {
                    /* Not a match, arm timer */
                    timer_start_timer(TIMER_CHECK_PASSWORD, CHECK_PASSWORD_TIMER_VAL);
                    comms_hid_msgs_send_ack_nack_message(is_message_from_usb, rcv_message_type, FALSE);
                    return;
                }
            }
        }
        
        case HID_CMD_CHANGE_NODE_PWD:
        {
            node_type_te temp_node_type;
            
            /* Empty password */
            if (rcv_msg->change_node_password.new_password[0] == 0)
            {
                comms_hid_msgs_send_ack_nack_message(is_message_from_usb, rcv_message_type, FALSE);
                return;
            }
                
            /* Check simple mode, user permission and correct node type */
            if (((logic_user_get_user_security_flags() & USER_SEC_FLG_ADVANCED_MENU) == 0) && (nodemgmt_check_user_permission(rcv_msg->change_node_password.node_address, &temp_node_type) == RETURN_OK) && (temp_node_type == NODE_TYPE_CHILD))
            {
                logic_user_change_node_password(rcv_msg->change_node_password.node_address, rcv_msg->change_node_password.new_password);
                comms_hid_msgs_send_ack_nack_message(is_message_from_usb, rcv_message_type, TRUE);
                return;
            } 
            else
            {
                /* Set nack, leave same command id */
                comms_hid_msgs_send_ack_nack_message(is_message_from_usb, rcv_message_type, FALSE);
                return;
            }
        }
        
        case HID_CMD_ID_STORE_CRED:
        {               
            /********************************/
            /* Here comes the sanity checks */
            /********************************/
            
            /* Incorrect service name index */
            if (rcv_msg->store_credential.service_name_index != 0)
            {
                comms_hid_msgs_send_ack_nack_message(is_message_from_usb, rcv_message_type, FALSE);
                return;
            }
            
            /* Empty service name */
            if (rcv_msg->store_credential.concatenated_strings[0] == 0)
            {
                comms_hid_msgs_send_ack_nack_message(is_message_from_usb, rcv_message_type, FALSE);
                return;
            }
            
            /* Sequential order check */
            uint16_t temp_length = 0;
            uint16_t prev_length = 0;
            uint16_t prev_check_index = 0;
            uint16_t current_check_index = 0;
            uint16_t check_indexes[5] = {   rcv_msg->store_credential.service_name_index, \
                                            rcv_msg->store_credential.login_name_index, \
                                            rcv_msg->store_credential.description_index, \
                                            rcv_msg->store_credential.third_field_index, \
                                            rcv_msg->store_credential.password_index};
            uint16_t max_cust_char_length = (max_payload_size \
                                            - sizeof(rcv_msg->store_credential.service_name_index) \
                                            - sizeof(rcv_msg->store_credential.login_name_index) \
                                            - sizeof(rcv_msg->store_credential.description_index) \
                                            - sizeof(rcv_msg->store_credential.third_field_index) \
                                            - sizeof(rcv_msg->store_credential.password_index))/sizeof(cust_char_t);
            
            /* Check all fields */                              
            for (uint16_t i = 0; i < ARRAY_SIZE(check_indexes); i++)
            {
                current_check_index = check_indexes[i];
                
                /* If index indicates present field */
                if (current_check_index != UINT16_MAX)
                {
                    /* If index is correct */
                    if (current_check_index != prev_check_index + prev_length)
                    {
                        comms_hid_msgs_send_ack_nack_message(is_message_from_usb, rcv_message_type, FALSE);
                        return;
                    }
                    
                    /* Get string length */
                    temp_length = utils_strnlen(&(rcv_msg->store_credential.concatenated_strings[current_check_index]), max_cust_char_length);
                    
                    /* Too long length */
                    if (temp_length == max_cust_char_length)
                    {
                        comms_hid_msgs_send_ack_nack_message(is_message_from_usb, rcv_message_type, FALSE);
                        return;
                    }
                    
                    /* Store previous index & length */
                    prev_length = temp_length + 1;
                    prev_check_index = current_check_index;
                    
                    /* Reduce max length */
                    max_cust_char_length -= (temp_length + 1);
                }              
            }    
            
            /* Dirty hack for fields not set */
            uint16_t empty_string_index = utils_strlen(rcv_msg->store_credential.concatenated_strings);
            if (rcv_msg->store_credential.login_name_index == UINT16_MAX)
            {
                rcv_msg->store_credential.login_name_index = empty_string_index;
            }
            
            /* In case we only want to update certain fields */
            cust_char_t* description_field_pt = (cust_char_t*)0;
            cust_char_t* third_field_pt = (cust_char_t*)0;
            cust_char_t* password_field_pt = (cust_char_t*)0;
            if (rcv_msg->store_credential.description_index != UINT16_MAX)
            {
                description_field_pt = &(rcv_msg->store_credential.concatenated_strings[rcv_msg->store_credential.description_index]);
            }
            if (rcv_msg->store_credential.third_field_index != UINT16_MAX)
            {
                third_field_pt = &(rcv_msg->store_credential.concatenated_strings[rcv_msg->store_credential.third_field_index]);
            }
            if (rcv_msg->store_credential.password_index != UINT16_MAX)
            {
                password_field_pt = &(rcv_msg->store_credential.concatenated_strings[rcv_msg->store_credential.password_index]);
            }
            
            /* Proceed to other logic */
            if (logic_user_store_credential(    &(rcv_msg->store_credential.concatenated_strings[rcv_msg->store_credential.service_name_index]),\
                                                &(rcv_msg->store_credential.concatenated_strings[rcv_msg->store_credential.login_name_index]),\
                                                description_field_pt, third_field_pt, password_field_pt) == RETURN_OK)
            {
                comms_hid_msgs_send_ack_nack_message(is_message_from_usb, rcv_message_type, TRUE);
                return;        
            }
            else
            {
                comms_hid_msgs_send_ack_nack_message(is_message_from_usb, rcv_message_type, FALSE);
                return;
            }
        }

        case HID_CMD_ID_STORE_TOTP_CRED:
        {
            /********************************/
            /* Here comes the sanity checks */
            /********************************/

            /* Santity check that we can receive a full TOTP message in the receive buffer */
            _Static_assert(sizeof rcv_msg->payload >= sizeof rcv_msg->store_credential, "Receive buffer not large enough for TOTP credential");

            /* Empty service name */
            if (rcv_msg->store_TOTP_credential.service_name[0] == 0)
            {
                comms_hid_msgs_send_ack_nack_message(is_message_from_usb, rcv_message_type, FALSE);
                return;
            }

            cust_char_t* service_name_pt = rcv_msg->store_TOTP_credential.service_name;
            cust_char_t* login_name_pt = rcv_msg->store_TOTP_credential.login_name;
            TOTPcredentials_t *TOTPcreds = &rcv_msg->store_TOTP_credential.TOTPcreds;

            /* Ensure strings are null-terminated */
            service_name_pt[SERVICE_NAME_MAX_LEN-1] = '\0';
            login_name_pt[LOGIN_NAME_MAX_LEN-1] = '\0';

            /* Proceed to other logic */
            if (logic_user_store_TOTP_credential(service_name_pt, login_name_pt, TOTPcreds) == RETURN_OK)
            {
                comms_hid_msgs_send_ack_nack_message(is_message_from_usb, rcv_message_type, TRUE);
                return;
            }
            else
            {
                comms_hid_msgs_send_ack_nack_message(is_message_from_usb, rcv_message_type, FALSE);
                return;
            }
        }
        case HID_CMD_GET_DEVICE_LANG_ID:
        {
            /* Get device default language */
            aux_mcu_message_t* temp_tx_message_pt = comms_hid_msgs_get_empty_hid_packet(is_message_from_usb, rcv_message_type, sizeof(uint8_t));
            temp_tx_message_pt->hid_message.payload[0] = custom_fs_settings_get_device_setting(SETTING_DEVICE_DEFAULT_LANGUAGE);
            comms_aux_mcu_send_message(temp_tx_message_pt);
            return;
        }
        
        case HID_CMD_GET_USER_LANG_ID:
        {
            /* Get user language, device default language returned if no user logged in */
            if (logic_security_is_smc_inserted_unlocked() == FALSE)
            {
                aux_mcu_message_t* temp_tx_message_pt = comms_hid_msgs_get_empty_hid_packet(is_message_from_usb, rcv_message_type, sizeof(uint8_t));
                temp_tx_message_pt->hid_message.payload[0] = custom_fs_settings_get_device_setting(SETTING_DEVICE_DEFAULT_LANGUAGE);
                comms_aux_mcu_send_message(temp_tx_message_pt);
                return;          
            }
            else
            {
                aux_mcu_message_t* temp_tx_message_pt = comms_hid_msgs_get_empty_hid_packet(is_message_from_usb, rcv_message_type, sizeof(uint8_t));
                temp_tx_message_pt->hid_message.payload[0] = custom_fs_get_current_language_id();
                comms_aux_mcu_send_message(temp_tx_message_pt);
                return;              
            }
        }
        
        case HID_CMD_GET_USER_KEYB_ID:
        {
            /* Get user keyboard id */
            if (logic_security_is_smc_inserted_unlocked() == FALSE)
            {
                aux_mcu_message_t* temp_tx_message_pt = comms_hid_msgs_get_empty_hid_packet(is_message_from_usb, rcv_message_type, sizeof(uint8_t));
                temp_tx_message_pt->hid_message.payload[0] = 0;
                comms_aux_mcu_send_message(temp_tx_message_pt);
                return;
            } 
            else
            {
                aux_mcu_message_t* temp_tx_message_pt = comms_hid_msgs_get_empty_hid_packet(is_message_from_usb, rcv_message_type, sizeof(uint8_t));
                temp_tx_message_pt->hid_message.payload[0] = custom_fs_get_current_layout_id(rcv_msg->payload_as_uint16[0]);
                comms_aux_mcu_send_message(temp_tx_message_pt);
                return;
            }
        }
        
        case HID_CMD_GET_NB_LANGUAGES:
        {
            aux_mcu_message_t* temp_tx_message_pt = comms_hid_msgs_get_empty_hid_packet(is_message_from_usb, rcv_message_type, sizeof(uint16_t));
            temp_tx_message_pt->hid_message.payload_as_uint16[0] = (uint16_t)custom_fs_get_number_of_languages();
            comms_aux_mcu_send_message(temp_tx_message_pt);
            return;   
        }
        
        case HID_CMD_GET_NB_LAYOUTS:
        {
            aux_mcu_message_t* temp_tx_message_pt = comms_hid_msgs_get_empty_hid_packet(is_message_from_usb, rcv_message_type, sizeof(uint16_t));
            temp_tx_message_pt->hid_message.payload_as_uint16[0] = (uint16_t)custom_fs_get_number_of_keyb_layouts();
            comms_aux_mcu_send_message(temp_tx_message_pt);
            return;       
        }
        
        case HID_CMD_GET_LANGUAGE_DESC:
        {
            aux_mcu_message_t* temp_tx_message_pt = comms_hid_msgs_get_empty_hid_packet(is_message_from_usb, rcv_message_type, 0);
            
            /* This function checks for out of boundary conditions... */
            if (custom_fs_get_language_description(rcv_msg->payload[0], (cust_char_t*)temp_tx_message_pt->hid_message.payload_as_uint16) == RETURN_OK)
            {
                comms_hid_msgs_update_message_payload_length_fields(temp_tx_message_pt, utils_strlen((cust_char_t*)temp_tx_message_pt->hid_message.payload_as_uint16)*sizeof(uint16_t) + sizeof(uint16_t));
                comms_aux_mcu_send_message(temp_tx_message_pt);
                return;
            } 
            else
            {
                comms_hid_msgs_update_message_payload_length_fields(temp_tx_message_pt, sizeof(uint16_t));
                temp_tx_message_pt->hid_message.payload_as_uint16[0] = 0;
                comms_aux_mcu_send_message(temp_tx_message_pt);
                return;
            }
        }
        
        case HID_CMD_GET_LAYOUT_DESC:
        {
            aux_mcu_message_t* temp_tx_message_pt = comms_hid_msgs_get_empty_hid_packet(is_message_from_usb, rcv_message_type, 0);
            
            /* This function checks for out of boundary conditions... */
            if (custom_fs_get_keyboard_descriptor_string(rcv_msg->payload[0], (cust_char_t*)temp_tx_message_pt->hid_message.payload_as_uint16) == RETURN_OK)
            {
                comms_hid_msgs_update_message_payload_length_fields(temp_tx_message_pt, utils_strlen((cust_char_t*)temp_tx_message_pt->hid_message.payload_as_uint16)*sizeof(uint16_t) + sizeof(uint16_t));
                comms_aux_mcu_send_message(temp_tx_message_pt);
                return;
            }
            else
            {
                comms_hid_msgs_update_message_payload_length_fields(temp_tx_message_pt, sizeof(uint16_t));
                temp_tx_message_pt->hid_message.payload_as_uint16[0] = 0;
                comms_aux_mcu_send_message(temp_tx_message_pt);
                return;
            }
        }
        
        case HID_CMD_GET_FILE_DATA_ID:
        case HID_CMD_ACCESS_NOTE_ID:
        {
            cust_char_t* service_pointer = (cust_char_t*)0;
            
            /* New request or request for next bunch of data */
            if (rcv_msg->payload_length != 0)
            {
                /* Input sanitazing */
                uint16_t max_cust_char_length = max_payload_size/sizeof(cust_char_t);
                
                /* Get string length */
                uint16_t string_length = utils_strnlen(rcv_msg->payload_as_cust_char_t, max_cust_char_length);
                
                /* Check for valid length, not exceeding payload size */
                if ((string_length >= max_cust_char_length) || ((string_length + 1) != (rcv_msg->payload_length / (uint16_t)sizeof(cust_char_t))))
                {
                    /* Set failure byte */
                    aux_mcu_message_t* temp_tx_message_pt = comms_hid_msgs_get_empty_hid_packet(is_message_from_usb, rcv_message_type, sizeof(uint16_t));
                    temp_tx_message_pt->hid_message.payload_as_uint16[0] = HID_1BYTE_NACK;
                    comms_aux_mcu_send_message(temp_tx_message_pt);
                    return;
                }
                
                /* Set service pointer */
                service_pointer = rcv_msg->payload_as_cust_char_t;
            } 
            
            /* Buffer for decrypted data */
            uint8_t buffer[MEMBER_SIZE(child_data_node_t, data) + MEMBER_SIZE(child_data_node_t, data2)];
            memset(buffer, 0x00, sizeof(buffer));
            uint16_t decrypted_bytes_nb;
            
            /* If service is 0, query user and get the bytes, otherwise just get the bytes */
            if ((logic_security_is_smc_inserted_unlocked() != FALSE) && (logic_user_get_data_from_service(service_pointer, buffer, &decrypted_bytes_nb, is_message_from_usb, data_type_for_operation) == RETURN_OK))
            {
                /* Create reply message */
                aux_mcu_message_t* temp_tx_message_pt = comms_hid_msgs_get_empty_hid_packet(is_message_from_usb, rcv_message_type, sizeof(uint16_t) + sizeof(uint16_t) + decrypted_bytes_nb);
                memcpy((void*)&temp_tx_message_pt->hid_message.payload_as_uint16[2], (void*)buffer, decrypted_bytes_nb);
                temp_tx_message_pt->hid_message.payload_as_uint16[1] = decrypted_bytes_nb;
                temp_tx_message_pt->hid_message.payload_as_uint16[0] = HID_1BYTE_ACK;
                comms_aux_mcu_send_message(temp_tx_message_pt);
                return;
            }
            else
            {
                /* Set failure byte */
                aux_mcu_message_t* temp_tx_message_pt = comms_hid_msgs_get_empty_hid_packet(is_message_from_usb, rcv_message_type, sizeof(uint16_t));
                temp_tx_message_pt->hid_message.payload_as_uint16[0] = HID_1BYTE_NACK;
                comms_aux_mcu_send_message(temp_tx_message_pt);
                return;
            }        
        }
        
        case HID_CMD_TEST_FILE_ID:
        {
            /* Input sanitazing */
            uint16_t max_cust_char_length = max_payload_size/sizeof(cust_char_t);
            
            /* Get string length */
            uint16_t string_length = utils_strnlen(rcv_msg->payload_as_cust_char_t, max_cust_char_length);
            
            /* Check for valid length, not exceeding payload size, then prompt user */
            if ((string_length < max_cust_char_length) && ((string_length + 1) == (rcv_msg->payload_length / (uint16_t)sizeof(cust_char_t))) && (logic_security_is_smc_inserted_unlocked() != FALSE) && (logic_user_check_data_service(rcv_msg->payload_as_cust_char_t, data_type_for_operation) == RETURN_OK))
            {
                /* Set success byte */
                comms_hid_msgs_send_ack_nack_message(is_message_from_usb, rcv_message_type, TRUE);
                return;
            }
            else
            {
                /* Set failure byte */
                comms_hid_msgs_send_ack_nack_message(is_message_from_usb, rcv_message_type, FALSE);
                return;
            }
        }
        
        case HID_CMD_CREATE_FILE_ID:
        case HID_CMD_ADD_NOTE_ID:
        {
            /* Input sanitazing */
            uint16_t max_cust_char_length = max_payload_size/sizeof(cust_char_t);
            
            /* Get string length */
            uint16_t string_length = utils_strnlen(rcv_msg->payload_as_cust_char_t, max_cust_char_length);
            
            /* Check for valid length, not exceeding payload size, then prompt user */
            if ((string_length < max_cust_char_length) && ((string_length + 1) == (rcv_msg->payload_length / (uint16_t)sizeof(cust_char_t))) && (logic_security_is_smc_inserted_unlocked() != FALSE) && (logic_user_add_data_service(rcv_msg->payload_as_cust_char_t, is_message_from_usb, data_type_for_operation) == RETURN_OK))
            {
                /* Set success byte */
                comms_hid_msgs_send_ack_nack_message(is_message_from_usb, rcv_message_type, TRUE);
                return;
            }
            else
            {
                /* Set failure byte */
                comms_hid_msgs_send_ack_nack_message(is_message_from_usb, rcv_message_type, FALSE);
                return;
            }
        }
        
        case HID_CMD_DELETE_NOTE_ID:
        case HID_CMD_DELETE_FILE_ID:
        {
            /* Input sanitazing */
            uint16_t max_cust_char_length = max_payload_size/sizeof(cust_char_t);
            
            /* Get string length */
            uint16_t string_length = utils_strnlen(rcv_msg->payload_as_cust_char_t, max_cust_char_length);
            
            /* Check for valid length, not exceeding payload size, then prompt user */
            if ((string_length < max_cust_char_length) && ((string_length + 1) == (rcv_msg->payload_length / (uint16_t)sizeof(cust_char_t))) && (logic_security_is_smc_inserted_unlocked() != FALSE) && (logic_user_delete_data_service(rcv_msg->payload_as_cust_char_t, data_type_for_operation) == RETURN_OK))
            {
                /* Set success byte */
                comms_hid_msgs_send_ack_nack_message(is_message_from_usb, rcv_message_type, TRUE);
                return;
            }
            else
            {
                /* Set failure byte */
                comms_hid_msgs_send_ack_nack_message(is_message_from_usb, rcv_message_type, FALSE);
                return;
            }
        }
        
        case HID_CMD_MODIFY_FILE_ID:
        case HID_CMD_MODIFY_NOTE_ID:
        {
            /* Input sanitazing */
            uint16_t max_cust_char_length = max_payload_size/sizeof(cust_char_t);
            
            /* Get string length */
            uint16_t string_length = utils_strnlen(rcv_msg->payload_as_cust_char_t, max_cust_char_length);
            
            /* Check for valid length, not exceeding payload size, then prompt user */
            if ((string_length < max_cust_char_length) && ((string_length + 1) == (rcv_msg->payload_length / (uint16_t)sizeof(cust_char_t))) && (logic_security_is_smc_inserted_unlocked() != FALSE) && (logic_user_empty_data_service(rcv_msg->payload_as_cust_char_t, is_message_from_usb, data_type_for_operation) == RETURN_OK))
            {
                /* Set success byte */
                comms_hid_msgs_send_ack_nack_message(is_message_from_usb, rcv_message_type, TRUE);
                return;
            }
            else
            {
                /* Set failure byte */
                comms_hid_msgs_send_ack_nack_message(is_message_from_usb, rcv_message_type, FALSE);
                return;
            }
        }
        
        case HID_CMD_ADD_FILE_DATA_ID:
        case HID_CMD_ADD_NOTE_DATA_ID:
        {
            /* Check for correct packet size and try to store data */
            if ((rcv_msg->payload_length == sizeof(rcv_msg->store_data_in_file)) && (logic_user_add_data_to_current_service(&rcv_msg->store_data_in_file, is_message_from_usb) == RETURN_OK))
            {
                /* Set success byte */
                comms_hid_msgs_send_ack_nack_message(is_message_from_usb, rcv_message_type, TRUE);
                return;
            }
            else
            {
                /* Set failure byte */
                comms_hid_msgs_send_ack_nack_message(is_message_from_usb, rcv_message_type, FALSE);
                return;
            }
        }
        
        case HID_CMD_SET_USER_KEYB_ID:
        {
            BOOL is_usb_interface_wanted = (BOOL)rcv_msg->payload[0];
            uint8_t desired_layout_id = rcv_msg->payload[1];
            
            /* user logged in, id valid? */
            if ((logic_security_is_smc_inserted_unlocked() != FALSE) && (desired_layout_id < custom_fs_get_number_of_keyb_layouts()))
            {
                custom_fs_set_current_keyboard_id(desired_layout_id, is_usb_interface_wanted);
                logic_user_set_layout_id(desired_layout_id, is_usb_interface_wanted);
                
                /* Set success byte */
                comms_hid_msgs_send_ack_nack_message(is_message_from_usb, rcv_message_type, TRUE);
                return;
            }
            else
            {
                /* Set failure byte */
                comms_hid_msgs_send_ack_nack_message(is_message_from_usb, rcv_message_type, FALSE);
                return;
            }
        }
        
        case HID_CMD_TEMP_SET_KBD_LYT:
        {
            /* Same as above, but temporary */
            BOOL is_usb_interface_wanted = (BOOL)rcv_msg->payload[0];
            uint8_t desired_layout_id = rcv_msg->payload[1];
            
            /* user logged in, id valid? */
            if ((logic_security_is_smc_inserted_unlocked() != FALSE) && (desired_layout_id < custom_fs_get_number_of_keyb_layouts()))
            {
                /* Set keyboard id for ongoing session */
                custom_fs_set_current_keyboard_id(desired_layout_id, is_usb_interface_wanted);
                
                /* Set success byte */
                comms_hid_msgs_send_ack_nack_message(is_message_from_usb, rcv_message_type, TRUE);
                return;
            }
            else
            {
                /* Set failure byte */
                comms_hid_msgs_send_ack_nack_message(is_message_from_usb, rcv_message_type, FALSE);
                return;
            }
        }
        
        case HID_CMD_SET_USER_LANG_ID:
        {
            uint8_t desired_lang_id = rcv_msg->payload[0];
            
            /* user logged in, id valid? */
            if ((logic_security_is_smc_inserted_unlocked() != FALSE) && (desired_lang_id < custom_fs_get_number_of_languages()))
            {
                custom_fs_set_current_language(desired_lang_id);
                logic_user_set_language(desired_lang_id);
                
                /* Update display */
                if (gui_dispatcher_is_screen_saver_running() == FALSE)
                {
                    gui_dispatcher_get_back_to_current_screen();
                }
                
                /* Set success byte */
                comms_hid_msgs_send_ack_nack_message(is_message_from_usb, rcv_message_type, TRUE);
                return;
            }
            else
            {
                /* Set failure byte */
                comms_hid_msgs_send_ack_nack_message(is_message_from_usb, rcv_message_type, FALSE);
                return;
            }
        }
        
        case HID_CMD_SET_DEVICE_LANG_ID:
        {
            uint8_t desired_lang_id = rcv_msg->payload[0];
            
            /* user logged in, id valid? */
            if (desired_lang_id < custom_fs_get_number_of_languages())
            {
                custom_fs_set_device_default_language(desired_lang_id);
                
                /* Set success byte */
                comms_hid_msgs_send_ack_nack_message(is_message_from_usb, rcv_message_type, TRUE);
                return;
            }
            else
            {
                /* Set failure byte */
                comms_hid_msgs_send_ack_nack_message(is_message_from_usb, rcv_message_type, FALSE);
                return;
            }        
        }
        
        case HID_CMD_ADD_UNKNOWN_CARD_ID:
        {
            /* Check the args, check we're not authenticated, check that the user could unlock the card */
            if ((rcv_msg->payload_length == sizeof(hid_message_setup_existing_user_req_t)) && (gui_dispatcher_get_current_screen() == GUI_SCREEN_INSERTED_UNKNOWN))
            {
                uint8_t temp_buffer[AES_KEY_LENGTH/8];
                logic_device_activity_detected();
                uint8_t new_user_id;
                
                /* Copy locally */
                hid_message_setup_existing_user_req_t setup_existing_user_req;
                memcpy(&setup_existing_user_req, &rcv_msg->setup_existing_user_req, sizeof(setup_existing_user_req));
                
                /* Sanity checks */
                _Static_assert(sizeof(temp_buffer) >= SMARTCARD_CPZ_LENGTH, "Invalid buffer reuse");
                
                /* If feature is enabled */
                #ifndef AES_PROVISIONED_KEY_IMPORT_EXPORT_ALLOWED
                setup_existing_user_req.cpz_lut_entry.use_provisioned_key_flag = 0;
                memset(setup_existing_user_req.cpz_lut_entry.provisioned_key, 0, MEMBER_SIZE(cpz_lut_entry_t,provisioned_key));
                #endif
                
                /* Read code protected zone to compare with provided one */
                smartcard_highlevel_read_code_protected_zone(temp_buffer);
                
                /* Check that the provided CPZ is the current one, ask the user to unlock the card and check that we can add the user */
                if (    (memcmp(temp_buffer, setup_existing_user_req.cpz_lut_entry.cards_cpz, SMARTCARD_CPZ_LENGTH) == 0) && \
                        (smartcard_highlevel_check_hidden_aes_key_contents() == RETURN_OK) && \
                        (logic_smartcard_user_unlock_process() == UNLOCK_OK_RET) && \
                        (logic_user_create_new_user_for_existing_card(&setup_existing_user_req.cpz_lut_entry, setup_existing_user_req.security_preferences, setup_existing_user_req.language_id, setup_existing_user_req.usb_keyboard_id, setup_existing_user_req.ble_keyboard_id, &new_user_id) == RETURN_OK))
                {
                    /* Initialize user context, also sets user language */
                    logic_user_init_context(new_user_id);
                    
                    /* Fetch pointer to CPZ MCU stored entry (just created) */
                    cpz_lut_entry_t* cpz_stored_entry;
                    custom_fs_get_cpz_lut_entry(temp_buffer, &cpz_stored_entry);
                    
                    /* Init encryption handling */
                    smartcard_highlevel_read_aes_key(temp_buffer);
                    logic_encryption_init_context(temp_buffer, cpz_stored_entry);
                    
                    /* Set smartcard unlock & MMM flags */
                    logic_security_smartcard_unlocked_actions();
                    logic_security_set_management_mode(is_message_from_usb);
                    
                    /* Setup MMM */
                    gui_dispatcher_set_current_screen(GUI_SCREEN_MEMORY_MGMT, TRUE, GUI_INTO_MENU_TRANSITION);
                    
                    /* Clear temp vars */
                    memset((void*)temp_buffer, 0, sizeof(temp_buffer));
                    
                    /* Set success byte */
                    comms_hid_msgs_send_ack_nack_message(is_message_from_usb, rcv_message_type, TRUE);
                    
                    /* Go back to currently set screen */
                    gui_dispatcher_get_back_to_current_screen();
                    
                    return;
                }
                else
                {
                    /* Set failure byte */
                    comms_hid_msgs_send_ack_nack_message(is_message_from_usb, rcv_message_type, FALSE);
                    return;
                }
            }
            else
            {
                /* Set failure byte */
                comms_hid_msgs_send_ack_nack_message(is_message_from_usb, rcv_message_type, FALSE);
                return;
            }
        }
        
        case HID_CMD_DEV_AUTH_CHALLENGE:
        {
            if (rcv_msg->payload_length == sizeof(uint32_t) + (AES_BLOCK_SIZE/8))
            {
                br_aes_ct_ctrcbc_keys device_operations_aes_context;
                uint32_t suggested_counter_value = rcv_msg->payload_as_uint32[0];
                uint32_t password_buffer[AES_BLOCK_SIZE/8/sizeof(uint32_t)];
                uint8_t device_operations_aes_key[AES_KEY_LENGTH/8];
                uint8_t temp_ctr[AES256_CTR_LENGTH/8];
                
                /* Bruteforce delay */
                timer_delay_ms(2000 + rng_get_random_uint8_t());
                
                /* Get current authentication challenge counter */
                uint32_t current_counter_value = custom_fs_get_auth_challenge_counter();
                
                /* Check that suggested counter value is above our current one */
                if ((suggested_counter_value > current_counter_value) || (current_counter_value == UINT32_MAX))
                {
                    /* Fetch device operations key & static random data */
                    custom_fs_get_device_operations_aes_key(device_operations_aes_key);
                    
                    /* Authentication challenge operations: we use the suggested counter value as counter, for the bytes 12-15 of the Big Endian CTR (byte 1 is set to the purpose value) */
                    memset(temp_ctr, 0, sizeof(temp_ctr));
                    temp_ctr[1] = AUTH_CHALLENGE_CTR_B1_ID;
                    if ((current_counter_value == UINT32_MAX) || (suggested_counter_value == UINT32_MAX))
                    {
                        utils_uint32_t_to_be_array(&temp_ctr[12], UINT32_MAX);
                    } 
                    else
                    {
                        utils_uint32_t_to_be_array(&temp_ctr[12], suggested_counter_value);
                    }
                    
                    /* Initialize AES context */
                    br_aes_ct_ctrcbc_init(&device_operations_aes_context, device_operations_aes_key, AES_KEY_LENGTH/8);
                    
                    /* Prepare what we want to encrypt: suggested counter value + device SN */
                    _Static_assert(sizeof(password_buffer) == (AES_BLOCK_SIZE/8), "Invalid buffer size");
                    memset(password_buffer, 0, sizeof(password_buffer));
                    if (current_counter_value == UINT32_MAX)
                    {
                        password_buffer[0] = UINT32_MAX;
                    } 
                    else
                    {
                        password_buffer[0] = suggested_counter_value;
                    }
                    password_buffer[1] = custom_fs_get_platform_internal_serial_number();
                    br_aes_ct_ctrcbc_ctr(&device_operations_aes_context, (void*)temp_ctr, password_buffer, sizeof(password_buffer));
                    
                    /* Check for match */
                    if (utils_side_channel_safe_memcmp((uint8_t*)password_buffer, &rcv_msg->payload[sizeof(uint32_t)], sizeof(password_buffer)) == 0)
                    {
                        /* Sign challenge: sign the same thing but use a different byte 1 value) */
                        memset(temp_ctr, 0, sizeof(temp_ctr));
                        temp_ctr[1] = AUTH_RESPONSE_CTR_B1_ID;
                        if ((current_counter_value == UINT32_MAX) || (suggested_counter_value == UINT32_MAX))
                        {
                            utils_uint32_t_to_be_array(&temp_ctr[12], UINT32_MAX);
                        }
                        else
                        {
                            utils_uint32_t_to_be_array(&temp_ctr[12], suggested_counter_value);
                        }
                        memset(password_buffer, 0, sizeof(password_buffer));
                        if (current_counter_value == UINT32_MAX)
                        {
                            password_buffer[0] = UINT32_MAX;
                        }
                        else
                        {
                            password_buffer[0] = suggested_counter_value;
                        }
                        br_aes_ct_ctrcbc_ctr(&device_operations_aes_context, (void*)temp_ctr, password_buffer, sizeof(password_buffer));
                        
                        /* Increment challenge counter */
                        if (current_counter_value != UINT32_MAX)
                        {
                            custom_fs_set_auth_challenge_counter(suggested_counter_value);
                        }
                        
                        /* Send answer */
                        aux_mcu_message_t* temp_tx_message_pt = comms_hid_msgs_get_empty_hid_packet(is_message_from_usb, rcv_message_type, sizeof(password_buffer));
                        memcpy(temp_tx_message_pt->hid_message.payload, password_buffer, sizeof(password_buffer));
                        comms_aux_mcu_send_message(temp_tx_message_pt);
                        
                        /* memset everything */
                        memset(&device_operations_aes_context, 0, sizeof(device_operations_aes_context));
                        memset(device_operations_aes_key, 0, sizeof(device_operations_aes_key));
                        memset(temp_ctr, 0, sizeof(temp_ctr));                        
                        return;
                    }
                    else
                    {
                        /* memset everything */
                        memset(&device_operations_aes_context, 0, sizeof(device_operations_aes_context));
                        memset(device_operations_aes_key, 0, sizeof(device_operations_aes_key));
                        memset(temp_ctr, 0, sizeof(temp_ctr));
                        
                        /* Set nack, leave same command id */
                        comms_hid_msgs_send_ack_nack_message(is_message_from_usb, rcv_message_type, FALSE);
                        return;                        
                    }
                } 
                else
                {
                    /* Set nack, leave same command id */
                    comms_hid_msgs_send_ack_nack_message(is_message_from_usb, rcv_message_type, FALSE);
                    return;
                }                            
            }
            else
            {
                /* Set nack, leave same command id */
                comms_hid_msgs_send_ack_nack_message(is_message_from_usb, rcv_message_type, FALSE);
                return;                
            }
        }
        
        case HID_CMD_START_BUNDLE_UL:
        {
            /* Required actions when we start dealing with graphics memory */
            if ((is_message_from_usb != FALSE) && (rcv_msg->payload_length == (AES_BLOCK_SIZE/8)) && (logic_device_bundle_update_start(FALSE, rcv_msg->payload) == RETURN_OK))
            {
                /* Set bundle upload allowed boolean */
                comms_hid_msgs_bundle_upload_allowed = TRUE;
                
                /* Set state changed */
                logic_device_set_state_changed();
                
                /* Erase data flash */
                dataflash_bulk_erase_with_wait(&dataflash_descriptor);
                
                /* Set ack, leave same command id */
                comms_hid_msgs_send_ack_nack_message(is_message_from_usb, rcv_message_type, TRUE);
                return;
            }
            else
            {
                /* Set ack, leave same command id */
                comms_hid_msgs_send_ack_nack_message(is_message_from_usb, rcv_message_type, FALSE);
                return;
            }
        }
        
        case HID_CMD_BUNDLE_WRITE_256B:
        {
            if (comms_hid_msgs_bundle_upload_allowed != FALSE)
            {
                /* First 4 bytes is the write address, remaining 256 bytes is the payload */
                uint32_t* write_address = (uint32_t*)&rcv_msg->payload_as_uint32[0];
                dataflash_write_array_to_memory(&dataflash_descriptor, *write_address, &rcv_msg->payload[4], 256);
                
                /* Set ack, leave same command id */
                comms_hid_msgs_send_ack_nack_message(is_message_from_usb, rcv_message_type, TRUE);
                return;
            }
            else
            {
                /* Set nack, leave same command id */
                comms_hid_msgs_send_ack_nack_message(is_message_from_usb, rcv_message_type, FALSE);
                return;
            }
        }
        
        case HID_CMD_BUNDLE_UL_DONE:
        {
            if (comms_hid_msgs_bundle_upload_allowed != FALSE)
            {
                /* Do required actions: depending on the mini BLE version, it's possible we don't come back from this function (bootloader launched) */
                logic_device_bundle_update_end(FALSE);
                
                /* Call activity detected to prevent going to sleep directly after */
                logic_device_activity_detected();
                
                /* Reset boolean */
                comms_hid_msgs_bundle_upload_allowed = FALSE;
                
                /* Set ack, leave same command id */
                comms_hid_msgs_send_ack_nack_message(is_message_from_usb, rcv_message_type, TRUE);
                return;
            }
            else
            {
                /* Set nack, leave same command id */
                comms_hid_msgs_send_ack_nack_message(is_message_from_usb, rcv_message_type, FALSE);
                return;
            }
        }
        
        case HID_CMD_GET_DIAG_DATA:
        {
            aux_mcu_message_t* temp_tx_message_pt = comms_hid_msgs_get_empty_hid_packet(is_message_from_usb, rcv_message_type, sizeof(temp_tx_message_pt->hid_message.diag_bat_info_message));
            
            /* Copy interesting bits */
            cpu_irq_enter_critical();
            memcpy(&temp_tx_message_pt->hid_message.diag_bat_info_message.power_consumption_log, logic_power_get_power_consumption_log_pt(), sizeof(power_consumption_log_t));
            memcpy(&temp_tx_message_pt->hid_message.diag_bat_info_message.aux_eoc_data, logic_power_get_eoc_aux_diag_data_pt(), sizeof(power_eoc_event_diag_data_t));
            temp_tx_message_pt->hid_message.diag_bat_info_message.bandgap_measurement = platform_io_get_single_bandgap_measurement();
            cpu_irq_leave_critical();
            
            /* ... and send message */
            comms_aux_mcu_send_message(temp_tx_message_pt);
            return;
        }
        
        default: 
        {
            /* Flag invalid message */
            comms_aux_mcu_set_invalid_message_received();
            
            break;
        }            
    }
}
