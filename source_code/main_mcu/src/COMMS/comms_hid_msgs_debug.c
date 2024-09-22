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
/*!  \file     comms_hid_msgs_debug.c
*    \brief    HID debug communications
*    Created:  06/03/2018
*    Author:   Mathieu Stephan
*/
#include <stdarg.h>
#include <string.h>
#include <asf.h>
#include "comms_hid_msgs_debug_defines.h"
#include "comms_hid_msgs_debug.h"
#include "comms_hid_msgs.h"
#include "gui_dispatcher.h"
#include "logic_aux_mcu.h"
#include "comms_aux_mcu.h"
#include "driver_sercom.h"
#include "oled_wrapper.h"
#include "logic_device.h"
#include "driver_timer.h"
#include "platform_io.h"
#include "logic_power.h"
#include "dataflash.h"
#include "main.h"
#include "dma.h"
/* Variable to know if we're allowing bundle upload */
BOOL comms_hid_msgs_debug_upload_allowed = FALSE;


#ifdef DEBUG_USB_PRINTF_ENABLED
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsuggest-attribute=format"
/*! \fn     comms_hid_msgs_debug_printf(const char *fmt, ...) 
*   \brief  Output debug string to USB
*/
void comms_hid_msgs_debug_printf(const char *fmt, ...) 
{
    char buf[64];    
    va_list ap;    
    va_start(ap, fmt);

    /* Print into our temporary buffer */
    int16_t hypothetical_nb_chars = vsnprintf(buf, sizeof(buf), fmt, ap);
    
    /* No error? */
    if (hypothetical_nb_chars > 0)
    {
        /* Compute number of chars printed to our buffer */
        uint16_t actual_printed_chars = (uint16_t)hypothetical_nb_chars < sizeof(buf)-1? (uint16_t)hypothetical_nb_chars : sizeof(buf)-1;
        
        /* Use message to send to aux mcu as temporary buffer */        
        aux_mcu_message_t* temp_tx_message_pt = comms_aux_mcu_get_free_tx_message_object_pt();
        memset((void*)temp_tx_message_pt, 0, sizeof(aux_mcu_message_t));
        temp_tx_message_pt->message_type = AUX_MCU_MSG_TYPE_USB;
        temp_tx_message_pt->hid_message.message_type = HID_CMD_ID_DBG_MSG;
        temp_tx_message_pt->hid_message.payload_length = actual_printed_chars*2 + 2;
        temp_tx_message_pt->payload_length1 = temp_tx_message_pt->hid_message.payload_length + sizeof(temp_tx_message_pt->hid_message.payload_length) + sizeof(temp_tx_message_pt->hid_message.message_type);
        
        /* Copy to message payload */
        for (uint16_t i = 0; i < actual_printed_chars; i++)
        {
            temp_tx_message_pt->hid_message.payload_as_uint16[i] = buf[i];
        }
        
        /* Send message */
        comms_aux_mcu_send_message(temp_tx_message_pt);
    }
    va_end(ap);
}
#pragma GCC diagnostic pop
#endif

/*! \fn     comms_hid_msgs_parse_debug(hid_message_t* rcv_msg, uint16_t supposed_payload_length, msg_restrict_type_te answer_restrict_type, BOOL is_message_from_usb)
*   \brief  Parse an incoming message from USB or BLE
*   \param  rcv_msg                 Received message
*   \param  supposed_payload_length Supposed payload length
*   \param  answer_restrict_type    Enum restricting which messages we can answer
*   \param  is_message_from_usb     TRUE if message is from USB
*/
void comms_hid_msgs_parse_debug(hid_message_t* rcv_msg, uint16_t supposed_payload_length, msg_restrict_type_te answer_restrict_type, BOOL is_message_from_usb)
{    
    /* Check correct payload length */
    if ((supposed_payload_length != rcv_msg->payload_length) || (supposed_payload_length > sizeof(rcv_msg->payload)))
    {
        /* Silent error */
        return;
    }
    
    /* Checks based on restriction type */
    BOOL should_ignore_message = FALSE;
    if (answer_restrict_type == MSG_RESTRICT_ALL)
    {
        should_ignore_message = TRUE;
    }
    
    /* If do_not_deal_with is set, send please retry...  */
    if (should_ignore_message != FALSE)
    {
        /* Send please retry */
        aux_mcu_message_t* temp_tx_message_pt = comms_hid_msgs_get_empty_hid_packet(is_message_from_usb, HID_CMD_ID_RETRY, 0);
        comms_aux_mcu_send_message(temp_tx_message_pt);
        return;
    }
    
    /* By default: copy the same CMD identifier for TX message */
    uint16_t rcv_message_type = rcv_msg->message_type;
    
    /* Switch on command id */
    switch (rcv_msg->message_type)
    {
        case HID_CMD_ID_OPEN_DISP_BUFFER:
        {
            #ifndef MINIBLE_V2_TO_TACKLE
            /* Set pixel write window */
            oled_set_row_address(&plat_oled_descriptor, 0);
            oled_set_column_address(&plat_oled_descriptor, 0);
            
            /* Start filling the SSD1322 RAM */
            oled_start_data_sending(&plat_oled_descriptor);
            
            /* Set ack, leave same command id */
            comms_hid_msgs_send_ack_nack_message(is_message_from_usb, rcv_message_type, TRUE);
            return;
            #endif
        }    
        case HID_CMD_ID_SEND_TO_DISP_BUFFER:
        {            
            /* Send all pixels */
            for (uint16_t i = 0; i < supposed_payload_length; i++)
            {
                sercom_spi_send_single_byte_without_receive_wait(plat_oled_descriptor.sercom_pt, rcv_msg->payload[i]);
            }
            
            /* Set ack, leave same command id */
            comms_hid_msgs_send_ack_nack_message(is_message_from_usb, rcv_message_type, TRUE);
            return;
        }  
        case HID_CMD_ID_CLOSE_DISP_BUFFER:
        {            
            /* Wait for spi buffer to be sent */
            sercom_spi_wait_for_transmit_complete(plat_oled_descriptor.sercom_pt);
            
            /* Stop sending data */
            oled_stop_data_sending(&plat_oled_descriptor);            
            
            /* Set ack, leave same command id */
            comms_hid_msgs_send_ack_nack_message(is_message_from_usb, rcv_message_type, TRUE);
            return;
        }          
        case HID_CMD_ID_ERASE_DATA_FLASH:
        {
            /* Required actions when we start dealing with graphics memory */
            if (logic_device_bundle_update_start(TRUE, 0) == RETURN_OK)
            {
                /* Set upload allowed boolean */
                comms_hid_msgs_debug_upload_allowed = TRUE;
                
                /* Erase data flash */
                dataflash_bulk_erase_without_wait(&dataflash_descriptor);
                
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
        case HID_CMD_ID_IS_DATA_FLASH_READY:
        {
            if (dataflash_is_busy(&dataflash_descriptor) != FALSE)
            {
                /* Set nack, leave same command id */
                comms_hid_msgs_send_ack_nack_message(is_message_from_usb, rcv_message_type, FALSE);
                return;
            }
            else
            {
                /* Set ack, leave same command id */
                comms_hid_msgs_send_ack_nack_message(is_message_from_usb, rcv_message_type, TRUE);
                return;
            }
        }
        case HID_CMD_ID_DATAFLASH_WRITE_256B:
        {
            if (comms_hid_msgs_debug_upload_allowed != FALSE)
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
        case HID_CMD_ID_REINDEX_BUNDLE:
        {        
            if (comms_hid_msgs_debug_upload_allowed != FALSE)
            {
                /* Do required actions */
                logic_device_bundle_update_end(TRUE);
                
                /* Call activity detected to prevent going to sleep directly after */
                logic_device_activity_detected();
                
                /* Reset boolean */
                comms_hid_msgs_debug_upload_allowed = FALSE;
                
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
        case HID_CMD_ID_SET_OLED_PARAMS:
        {
            #ifndef MINIBLE_V2_TO_TACKLE
            /* vcomh change requires oled on / off */
            uint8_t vcomh = rcv_msg->payload[1];
            
            /* Do required actions */
            if (vcomh != 0xFF)
            {
                oled_off(&plat_oled_descriptor);
            }
            oled_set_contrast_current(&plat_oled_descriptor, rcv_msg->payload[0]);
            if (vcomh != 0xFF)
            {
                oled_set_vcomh_level(&plat_oled_descriptor, vcomh);
            }
            oled_set_vsegm_level(&plat_oled_descriptor, rcv_msg->payload[2]);
            oled_set_discharge_charge_periods(&plat_oled_descriptor, rcv_msg->payload[3] | (rcv_msg->payload[4] << 4));
            oled_set_discharge_vsl_level(&plat_oled_descriptor, rcv_msg->payload[5]);
            if (vcomh != 0xFF)
            {
                oled_on(&plat_oled_descriptor);
            }
            
            /* Set ack, leave same command id */
            comms_hid_msgs_send_ack_nack_message(is_message_from_usb, rcv_message_type, TRUE);
            return;
            #endif
        }
        case HID_CMD_ID_START_BOOTLOADER:
        {
            custom_fs_set_device_flag_value(DEVICE_WENT_THROUGH_BOOTLOADER_FLAG_ID, TRUE);
            custom_fs_settings_set_fw_upgrade_flag();
            main_reboot();      
        }
        case HID_CMD_ID_GET_ACC_32_SAMPLES:
        {
            while (lis2hh12_check_data_received_flag_and_arm_other_transfer(&plat_acc_descriptor, FALSE) == FALSE);
            aux_mcu_message_t* temp_tx_message_pt = comms_hid_msgs_get_empty_hid_packet(is_message_from_usb, rcv_message_type, sizeof(plat_acc_descriptor.fifo_read.acc_data_array));
            memcpy((void*)temp_tx_message_pt->hid_message.payload, (void*)plat_acc_descriptor.fifo_read.acc_data_array, sizeof(plat_acc_descriptor.fifo_read.acc_data_array));
            lis2hh12_check_data_received_flag_and_arm_other_transfer(&plat_acc_descriptor, TRUE);
            comms_aux_mcu_send_message(temp_tx_message_pt);
            return;
        }
        case HID_CMD_ID_FLASH_AUX_MCU:
        {            
            /* Wait for current packet reception and arm reception */
            dma_aux_mcu_wait_for_current_packet_reception_and_clear_flag();
            comms_aux_arm_rx_and_clear_no_comms();            
            logic_aux_mcu_flash_firmware_update(TRUE);            
            return;
        }
        case HID_CMD_ID_FLASH_AUX_AND_MAIN:
        {
            /* Detach from USB to get a free bus */
            comms_aux_mcu_send_simple_command_message(MAIN_MCU_COMMAND_DETACH_USB);
            comms_aux_mcu_wait_for_aux_event(AUX_MCU_EVENT_USB_DETACHED);
            
            /* Disable bluetooth if enabled */
            logic_aux_mcu_disable_ble(TRUE);
            
            /* Start by flashing aux */
            logic_aux_mcu_flash_firmware_update(FALSE);
            
            /* Then move on to main */
            custom_fs_set_device_flag_value(DEVICE_WENT_THROUGH_BOOTLOADER_FLAG_ID, TRUE);
            custom_fs_set_settings_value(SETTINGS_DEVICE_TUTORIAL, TRUE);
            custom_fs_settings_set_fw_upgrade_flag();
            main_reboot();            
        }
        case HID_CMD_ID_GET_DBG_PLAT_INFO:
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
            temp_tx_message_pt = comms_hid_msgs_get_empty_hid_packet(is_message_from_usb, rcv_message_type, sizeof(temp_tx_message_pt->hid_message.detailed_platform_info));
            memcpy((void*)temp_tx_message_pt->hid_message.detailed_platform_info.aux_mcu_infos, (void*)&temp_rx_message->aux_details_message, sizeof(temp_rx_message->aux_details_message));
            temp_tx_message_pt->hid_message.detailed_platform_info.main_mcu_fw_major = FW_MAJOR;
            temp_tx_message_pt->hid_message.detailed_platform_info.main_mcu_fw_minor = FW_MINOR;
            
            /* Send message */
            comms_aux_mcu_send_message(temp_tx_message_pt);
            
            /* Rearm message reception */
            comms_aux_arm_rx_and_clear_no_comms();
            return;
        }
        case HID_CMD_ID_GET_TIMESTAMP:
        {
            aux_mcu_message_t* temp_tx_message_pt;
            
            /* Get empty message, fill it and send it */
            temp_tx_message_pt = comms_hid_msgs_get_empty_hid_packet(is_message_from_usb, rcv_message_type, 4);
            temp_tx_message_pt->hid_message.payload_as_uint32[0] = driver_timer_get_rtc_timestamp_uint32t();
            comms_aux_mcu_send_message(temp_tx_message_pt);
            return;          
        }
        case HID_CMD_ID_GET_BATTERY_STATUS:
        {
            aux_mcu_message_t* temp_tx_message_pt;
            aux_mcu_message_t* temp_rx_message;
            uint16_t bat_adc_result;
            
            /* Keep screen on in case we're testing power consumption */
            logic_device_activity_detected();
            
            /* Start charging? */
            if (rcv_msg->payload[0] != 0)
            {
                comms_aux_mcu_send_simple_command_message(MAIN_MCU_COMMAND_NIMH_CHARGE);
                comms_aux_mcu_wait_for_aux_event(AUX_MCU_EVENT_CHARGE_STARTED);
                logic_power_set_battery_charging_bool(TRUE, FALSE);
            }
            
            /* Stop charging? */
            if (rcv_msg->payload[1] != 0)
            {
                comms_aux_mcu_send_simple_command_message(MAIN_MCU_COMMAND_STOP_CHARGE);
                comms_aux_mcu_wait_for_aux_event(AUX_MCU_EVENT_CHARGE_STOPPED);
                logic_power_set_battery_charging_bool(FALSE, FALSE);
            }
            
            /* Use USB to power the screen? */
            if (rcv_msg->payload[2] != 0)
            {
                oled_off(&plat_oled_descriptor);
                platform_io_disable_vbat_to_oled_stepup();
                platform_io_assert_oled_reset();
                timer_delay_ms(15);
                platform_io_power_up_oled(TRUE);
                oled_init_display(&plat_oled_descriptor, TRUE, logic_device_get_screen_current_for_current_use());
                gui_dispatcher_get_back_to_current_screen();
            }
            
            /* Use battery to power the screen? */
            if (rcv_msg->payload[3] != 0)
            {
                oled_off(&plat_oled_descriptor);
                platform_io_disable_3v3_to_oled_stepup();
                platform_io_assert_oled_reset();
                timer_delay_ms(15);
                platform_io_power_up_oled(FALSE);
                oled_init_display(&plat_oled_descriptor, TRUE, logic_device_get_screen_current_for_current_use());
                gui_dispatcher_get_back_to_current_screen();
            }   

            /* Force charge voltage ? */
            if (rcv_msg->payload_as_uint16[2] != 0)
            {
                /* Generate our force charge voltage packet */
                temp_tx_message_pt = comms_aux_mcu_get_empty_packet_ready_to_be_sent(AUX_MCU_MSG_TYPE_MAIN_MCU_CMD);
                temp_tx_message_pt->main_mcu_command_message.command = MAIN_MCU_COMMAND_FORCE_CHARGE_VOLT;
                temp_tx_message_pt->main_mcu_command_message.payload_as_uint16[0] = rcv_msg->payload_as_uint16[2];
                temp_tx_message_pt->payload_length1 = MEMBER_SIZE(main_mcu_command_message_t, command) + MEMBER_SIZE(main_mcu_command_message_t, payload_as_uint16[0]);
                
                /* Send message */
                comms_aux_mcu_send_message(temp_tx_message_pt);
                timer_delay_ms(100);
            }

            /* Stop force charge? */
            if (rcv_msg->payload[6] != 0)
            {
                comms_aux_mcu_send_simple_command_message(MAIN_MCU_COMMAND_STOP_FORCE_CHARGE);
                timer_delay_ms(100);
            }
            
            /* Generate our get battery charge status packet */
            temp_tx_message_pt = comms_aux_mcu_get_empty_packet_ready_to_be_sent(AUX_MCU_MSG_TYPE_NIMH_CHARGE);
            
            /* Send message */
            comms_aux_mcu_send_message(temp_tx_message_pt);
            
            /* Get ADC value for current conversion */
            while (platform_io_is_vbat_conversion_result_ready() == FALSE);
            bat_adc_result = platform_io_get_vbat_conversion_result_and_trigger_conversion();
            
            /* Wait for message from aux MCU */
            while(comms_aux_mcu_active_wait(&temp_rx_message, AUX_MCU_MSG_TYPE_NIMH_CHARGE, FALSE, -1) != RETURN_OK){}
                
            /* Prepare packet to send back */
            temp_tx_message_pt = comms_hid_msgs_get_empty_hid_packet(is_message_from_usb, rcv_message_type, sizeof(temp_tx_message_pt->hid_message.battery_status));
            temp_tx_message_pt->hid_message.battery_status.power_source = logic_power_get_power_source();
            temp_tx_message_pt->hid_message.battery_status.platform_charging = logic_power_is_battery_charging();
            temp_tx_message_pt->hid_message.battery_status.main_adc_battery_value = bat_adc_result;
            temp_tx_message_pt->hid_message.battery_status.aux_charge_status = temp_rx_message->nimh_charge_message.charge_status;
            temp_tx_message_pt->hid_message.battery_status.aux_battery_voltage = temp_rx_message->nimh_charge_message.battery_voltage;
            temp_tx_message_pt->hid_message.battery_status.aux_charge_current = temp_rx_message->nimh_charge_message.charge_current;
            temp_tx_message_pt->hid_message.battery_status.aux_stepdown_voltage = temp_rx_message->nimh_charge_message.stepdown_voltage;
            temp_tx_message_pt->hid_message.battery_status.aux_dac_register_val = temp_rx_message->nimh_charge_message.dac_data_reg;
            
            /* Rearm aux communications */    
            comms_aux_arm_rx_and_clear_no_comms();
            
            /* Send message */
            comms_aux_mcu_send_message(temp_tx_message_pt);
            return;
        }
        case HID_CMD_ID_SET_PLAT_UNIQUE_DATA:
        {
            #ifdef PLAT_V7_SETUP
            bl_section_last_row_t* bl_last_row_ptr = (bl_section_last_row_t*)(FLASH_ADDR + APP_START_ADDR - NVMCTRL_ROW_SIZE);
            bl_section_last_row_t* bl_section_last_row_to_flash_pt = (bl_section_last_row_t*)rcv_msg->payload_as_uint32;
            
            /* Automatic flash write, disable caching */
            NVMCTRL->CTRLB.bit.MANW = 0;
            NVMCTRL->CTRLB.bit.CACHEDIS = 1;
            
            /* Update the platform data */
            while ((NVMCTRL->INTFLAG.reg & NVMCTRL_INTFLAG_READY) == 0);
            NVMCTRL->ADDR.reg  = ((uint32_t)bl_last_row_ptr)/2;
            NVMCTRL->CTRLA.reg = NVMCTRL_CTRLA_CMD_ER | NVMCTRL_CTRLA_CMDEX_KEY;
            
            /* Flash bytes */
            for (uint32_t j = 0; j < 4; j++)
            {
                /* Flash 4 consecutive pages */
                while ((NVMCTRL->INTFLAG.reg & NVMCTRL_INTFLAG_READY) == 0);
                for(uint32_t i = 0; i < NVMCTRL_ROW_SIZE/4; i+=2)
                {
                    NVM_MEMORY[(((uint32_t)bl_last_row_ptr)+j*(NVMCTRL_ROW_SIZE/4)+i)/2] = bl_section_last_row_to_flash_pt->row_data[(j*(NVMCTRL_ROW_SIZE/4)+i)/2];
                }
            }
            
            /* Final wait, reset */
            while ((NVMCTRL->INTFLAG.reg & NVMCTRL_INTFLAG_READY) == 0);
            
            /* Disable bluetooth if enabled */
            logic_aux_mcu_disable_ble(TRUE);
            
            /* Re-enable tutorial */
            custom_fs_set_settings_value(SETTINGS_DEVICE_TUTORIAL, TRUE);
            
            /* Switch off screen */
            oled_off(&plat_oled_descriptor);
            platform_io_power_down_oled();
            
            /* Detach USB */
            comms_aux_mcu_send_simple_command_message(MAIN_MCU_COMMAND_DETACH_USB);
            comms_aux_mcu_wait_for_aux_event(AUX_MCU_EVENT_USB_DETACHED);
            timer_delay_ms(500);
            
            /* Reboot */
            NVIC_SystemReset();
            while(1);            
            #endif
            return;
        }            
        default: break;
    }
    
    return;
}
