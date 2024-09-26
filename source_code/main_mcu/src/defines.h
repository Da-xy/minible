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
/*!  \file     defines.h
*    \brief    Generic type defines
*    Created:  10/11/2017
*    Author:   Mathieu Stephan
*/
#ifndef DEFINES_H_
#define DEFINES_H_

#include <stdio.h>
#include <inttypes.h>
#include <nodemgmt_defines.h>

/* Defines */
#define AUX_MCU_MESSAGE_REPLY_TIMEOUT_MS    1500

/* Fonts defines */
#define FONT_UBUNTU_MONO_BOLD_30_ID 0
#define FONT_UBUNTU_MEDIUM_15_ID    1
#define FONT_UBUNTU_MEDIUM_17_ID    2
#define FONT_UBUNTU_REGULAR_16_ID   3
#define FONT_UBUNTU_REGULAR_13_ID   4

/* Macros */
#define XSTR(x)                             STR(x)
#define STR(x)                              #x
#define ARRAY_SIZE(x)                       (sizeof((x)) / sizeof((x)[0]))
#define MEMBER_SIZE(type, member)           sizeof(((type*)0)->member)
#define MEMBER_ARRAY_SIZE(type, member)     (sizeof(((type*)0)->member) / sizeof(((type*)0)->member[0]))
#define MEMBER_SUB_ARRAY_SIZE(type, member) (sizeof(((type*)0)->member[0]) / sizeof(((type*)0)->member[0][0]))

/* Platform define macros */
#define IS_V1_PLAT_DEFINED(version)         defined(PLAT_V##version##_SETUP)
#define IS_V1_PLAT_IN_RANGE_1_TO_2          (IS_V1_PLAT_DEFINED(1) || IS_V1_PLAT_DEFINED(2))
#define IS_V1_PLAT_IN_RANGE_1_TO_3          (IS_V1_PLAT_DEFINED(1) || IS_V1_PLAT_DEFINED(2) || IS_V1_PLAT_DEFINED(3))
#define IS_V1_PLAT_IN_RANGE_1_TO_6          (IS_V1_PLAT_DEFINED(1) || IS_V1_PLAT_DEFINED(2) || IS_V1_PLAT_DEFINED(3) || IS_V1_PLAT_DEFINED(4) || IS_V1_PLAT_DEFINED(5) || IS_V1_PLAT_DEFINED(6))
#define IS_V1_PLAT_IN_RANGE_3_TO_5          (IS_V1_PLAT_DEFINED(3) || IS_V1_PLAT_DEFINED(4) || IS_V1_PLAT_DEFINED(5))
#define IS_V1_PLAT_IN_RANGE_3_TO_7          (IS_V1_PLAT_DEFINED(3) || IS_V1_PLAT_DEFINED(4) || IS_V1_PLAT_DEFINED(5) || IS_V1_PLAT_DEFINED(6) || IS_V1_PLAT_DEFINED(7))
#define IS_V1_PLAT_IN_RANGE_4_TO_7          (IS_V1_PLAT_DEFINED(4) || IS_V1_PLAT_DEFINED(5) || IS_V1_PLAT_DEFINED(6) || IS_V1_PLAT_DEFINED(7))
#define IS_V1_PLAT_IN_RANGE_6_TO_7          (IS_V1_PLAT_DEFINED(6) || IS_V1_PLAT_DEFINED(7))

/* Standard defines */
#define SHA256_OUTPUT_LENGTH    32
#define AES_KEY_LENGTH          256
#define AES_BLOCK_SIZE          128
#define AES256_CTR_LENGTH       AES_BLOCK_SIZE
#define FALSE                   0
#define TRUE                    (!FALSE)
#define NULLPTR                 (void*)0

/* Debugging defines */
#define DEBUG_STACK_TRACKING_COOKIE 0x5D

/* Enums */
typedef enum    {RETURN_MOOLTIPASS_INVALID = 0, RETURN_MOOLTIPASS_PB = 1, RETURN_MOOLTIPASS_BLOCKED = 2, RETURN_MOOLTIPASS_BLANK = 3, RETURN_MOOLTIPASS_USER = 4, RETURN_MOOLTIPASS_0_TRIES_LEFT = 5, RETURN_MOOLTIPASS_1_TRIES_LEFT = 6, RETURN_MOOLTIPASS_2_TRIES_LEFT = 7, RETURN_MOOLTIPASS_3_TRIES_LEFT = 8, RETURN_MOOLTIPASS_4_TRIES_LEFT = 9} mooltipass_card_detect_return_te;
typedef enum    {HID_MSG_RCVD, HID_DBG_MSG_RCVD, HID_CANCEL_MSG_RCVD, BL_MSG_RCVD, EVENT_MSG_RCVD, MAIN_MCU_MSG_RCVD, RNG_MSG_RCVD, FIDO2_MSG_RCVD, ERRONEOUS_MSG_RCVD, UNKNOW_MSG_RCVD, NO_MSG_RCVD, HID_REINDEX_BUNDLE_RCVD, BLE_CMD_MSG_RCVD, BLE_BOND_STORE_RCVD, BLE_6PIN_REQ_RCVD} comms_msg_rcvd_te;
typedef enum    {WHEEL_ACTION_NONE = 0, WHEEL_ACTION_UP = 1, WHEEL_ACTION_DOWN = 2, WHEEL_ACTION_SHORT_CLICK = 3, WHEEL_ACTION_LONG_CLICK = 4, WHEEL_ACTION_CLICK_UP = 5, WHEEL_ACTION_CLICK_DOWN = 6, WHEEL_ACTION_DISCARDED = 7, WHEEL_ACTION_VIRTUAL = 8} wheel_action_ret_te;
typedef enum    {MINI_INPUT_RET_TIMEOUT = -1, MINI_INPUT_RET_NONE = 0, MINI_INPUT_RET_NO = 1, MINI_INPUT_RET_YES = 2, MINI_INPUT_RET_BACK = 3, MINI_INPUT_RET_CARD_REMOVED = 4, MINI_INPUT_RET_CANCELED = 5, MINI_INPUT_RET_POWER_SWITCH = 6} mini_input_yes_no_ret_te;
typedef enum    {RETURN_AUX_STAT_OK, RETURN_AUX_STAT_TIMEOUT, RETURN_AUX_STAT_OK_WITH_BLE, RETURN_AUX_STAT_BLE_ISSUE, RETURN_AUX_STAT_INV_MAIN_MSG, RETURN_AUX_STAT_TOO_MANY_CB, RETURN_AUX_STAT_ADC_WATCHDOG_FIRED} aux_status_return_te;
typedef enum    {GUI_INFO_DISP_RET_OK = 0, GUI_INFO_DISP_RET_SCROLL_OR_MSG, GUI_INFO_DISP_RET_CLICK, GUI_INFO_DISP_RET_CARD_CHANGE, GUI_INFO_DISP_RET_LONG_CLICK, GUI_INFO_DISP_RET_BLE_PAIRED} gui_info_display_ret_te;
typedef enum    {RETURN_CARD_NDET, RETURN_CARD_TEST_PB, RETURN_CARD_4_TRIES_LEFT,  RETURN_CARD_3_TRIES_LEFT,  RETURN_CARD_2_TRIES_LEFT,  RETURN_CARD_1_TRIES_LEFT, RETURN_CARD_0_TRIES_LEFT} card_detect_return_te;
typedef enum    {MSG_NO_RESTRICT, MSG_RESTRICT_ALL, MSG_RESTRICT_ALLBUT_SN, MSG_RESTRICT_ALLBUT_BUNDLE, MSG_RESTRICT_ALLBUT_CANCEL, MSG_RESTRICT_ALLBUT_BOND_STORE} msg_restrict_type_te;
typedef enum    {LF_EN_MASK = 0x01, LF_ENT_KEY_MASK = 0x02, LF_LOGIN_MASK = 0x04, LF_WIN_L_SEND_MASK = 0x08, LF_CTRL_ALT_DEL_MASK = 0x10, LF_NO_PWD_PROMPT_MASK = 0x20} lock_feature_te;
typedef enum    {WAKEUP_REASON_NONE = 0, WAKEUP_REASON_AUX_MCU, WAKEUP_REASON_30M_TIMER, WAKEUP_REASON_OTHER, WAKEUP_REASON_BOOT, WAKEUP_REASON_INACTIVITY} platform_wakeup_reason_te;
typedef enum    {BUNDLE_UPLOAD_CTR_B1_ID = 1, AUTH_CHALLENGE_CTR_B1_ID = 2, AUTH_RESPONSE_CTR_B1_ID = 3, HASH1_CTR_B1_ID = 4, HASH2_CTR_B1_ID = 5} aes_operation_ctr_b1_purpose_te;
typedef enum    {ACC_DET_NOTHING, ACC_DET_MOVEMENT, ACC_DET_KNOCK, ACC_INVERT_SCREEN, ACC_NINVERT_SCREEN, ACC_FAILING, ACC_FREEFALL, ACC_STRONG_MOVE} acc_detection_te;
typedef enum    {UNLOCK_OK_RET = 0, UNLOCK_BLOCKED_RET, UNLOCK_BACK_RET, UNLOCK_CARD_REMOVED_RET, UNLOCK_CARD_ISSUE_RET} unlock_ret_type_te;
typedef enum    {RETURN_VCARD_NOK = -1, RETURN_VCARD_OK = 0, RETURN_VCARD_UNKNOWN = 1, RETURN_VCARD_BACK = 2} valid_card_det_return_te;
typedef enum    {RETURN_PIN_OK = 0, RETURN_PIN_NOK_3, RETURN_PIN_NOK_2, RETURN_PIN_NOK_1, RETURN_PIN_NOK_0} pin_check_return_te;
typedef enum    {OLED_STEPUP_SOURCE_NONE = 0, OLED_STEPUP_SOURCE_VBAT, OLED_STEPUP_SOURCE_3V3} oled_stepup_pwr_source_te;
typedef enum    {RETURN_NEW_PIN_NOK = -1, RETURN_NEW_PIN_OK = 0, RETURN_NEW_PIN_DIFF = 1} new_pinreturn_type_te;
typedef enum    {RETURN_REL = 0, RETURN_DET, RETURN_JDETECT, RETURN_JRELEASED, RETURN_INV_DET} det_ret_type_te;
typedef enum    {GUI_SEL_FAVORITE, GUI_SEL_SERVICE, GUI_SEL_DATA_SERVICE, GUI_SEL_CRED } gui_sel_item_te;
typedef enum    {RETURN_CLONING_DONE, RETURN_CLONING_INV_CARD, RETURN_CLONING_SAME_CARD} cloning_ret_te;
typedef enum    {RETURN_INVALID = -3, RETURN_BACK = -2, RETURN_NOK = -1, RETURN_OK = 0} ret_type_te;
typedef enum    {DISP_MSG_INFO = 0, DISP_MSG_WARNING = 1, DISP_MSG_ACTION = 2} display_message_te;
typedef enum    {CUSTOM_FS_INIT_OK = 0, CUSTOM_FS_INIT_NO_RWEE = 1} custom_fs_init_ret_type_te;
typedef enum    {COMPARE_MODE_MATCH = 0, COMPARE_MODE_COMPARE = 1} service_compare_mode_te;
typedef enum    {PLAT_ANDROID, PLAT_IOS, PLAT_MACOS, PLAT_WIN} platform_type_te;
typedef enum    {SERVICE_CRED_TYPE, SERVICE_DATA_TYPE} service_type_te;    
    

/* Typedefs */
typedef void (*void_function_ptr_type_t)(void);
typedef uint16_t cust_char_t;
typedef ret_type_te RET_TYPE;
typedef uint32_t nat_type_t;
typedef int32_t BOOL;

/* Structures */
typedef struct TOTPcredentials_s
{
    union
    {
        uint8_t TOTPsecret[TOTP_SECRET_MAX_LEN]; // Encrypted TOTP secret
        uint8_t TOTPsecret_ct[TOTP_SECRET_MAX_LEN];     // Cleartext TOTP secret
    };
    uint8_t TOTPsecretLen;        // Length of TOTPsecret
    uint8_t TOTPnumDigits;        // Number of digits for TOTP value
    uint8_t TOTPtimeStep;         // TOTP time step. Reserved for future. MUST be set to 30 when sent from host
    uint8_t TOTP_SHA_ver;         // TOTP SHA version. Reserved for future. MUST be set to 0 when sent from host. (Valid future values would be 0 - SHA1, 1 = SHA256, 2 - SHA512)
} TOTPcredentials_t;

#endif /* DEFINES_H_ */
