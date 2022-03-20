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
/*!  \file     logic_device.c
*    \brief    General logic for device
*    Created:  27/01/2019
*    Author:   Mathieu Stephan
*/


#ifndef LOGIC_DEVICE_H_
#define LOGIC_DEVICE_H_

#include "defines.h"

/* Defines */
// Min & max timeout value for user interactions
#define SETTING_MIN_USER_INTERACTION_TIMEOUT    7
#define SETTING_DFT_USER_INTERACTION_TIMEOUT    15
#define SETTING_MAX_USER_INTERACTION_TIMOUT     25
#define SETTING_MAX_USER_INTERACTION_TIMOUT_EMU 30

/* Prototypes */
ret_type_te logic_device_bundle_update_start(BOOL from_debug_messages, uint8_t* password);
volatile platform_wakeup_reason_te logic_device_get_wakeup_reason(void);
void logic_device_set_wakeup_reason(platform_wakeup_reason_te reason);
void logic_device_bundle_update_end(BOOL from_debug_messages);
uint8_t logic_device_get_screen_current_for_current_use(void);
BOOL logic_device_get_and_clear_settings_changed_flag(void);
BOOL logic_device_get_and_clear_usb_timeout_detected(void);
BOOL logic_device_get_state_changed_and_reset_bool(void);
volatile BOOL logic_device_get_aux_wakeup_rcvd(void);
void logic_device_set_usb_timeout_detected(void);
void logic_device_clear_aux_wakeup_rcvd(void);
void logic_device_set_settings_changed(void);
void logic_device_clear_wakeup_reason(void);
void logic_device_set_state_changed(void);
void logic_device_activity_detected(void);
void logic_device_set_time_set(void);
BOOL logic_device_is_time_set(void);
void logic_device_power_off(void);

#endif /* LOGIC_DEVICE_H_ */