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
/*!  \file     platform_defines.h
*    \brief    Defines specific to our platform: SERCOMs, GPIOs...
*    Created:  10/11/2017
*    Author:   Mathieu Stephan
*/
#ifndef PLATFORM_DEFINES_H_
#define PLATFORM_DEFINES_H_

#include <asf.h>
#include "defines.h"

/**************** FIRMWARE DEFINES ****************/
#define FW_MAJOR    0
#define FW_MINOR    76

/* Changelog:
- v0.2: platform info message, flash aux mcu message, reindex bundle message
- v0.3: many things.... sleep problems solved
- v0.4: no comms signal checked (among others)
- v0.5: aux to main protocol change (requires hard programming for upgrade)
- v0.6: firmware shipped to beta testers
- v0.7: firmware upload on card inverted, settings enable/disable default selection set to current setting, pressing back when creating new user message fix
- v0.8: - not switching on screen when aux mcu wakes up main mcu
        - charge safety: allow charge whenever battery is below 60pct
        - logic change: now using an "all categories" filter instead of "default category"
- v0.9: - waiting for aux MCU confirmation when detaching USB
        - taking into account battery measurements only when USB isn't present
- v0.10:- faster UI
        - improved battery charging algorithm
- v0.11:- lock device on usb disconnect
        - prompts for credentials display after prompts for typing
        - switching menus: set default security options associated with each mode
        - full walk back implemented for favorite & login menus
- v0.12:- accelerometer used to wake up device when USB plugged, will prompt user for PIN if device is locked
        - double knock to approve storage and recall prompts, for advanced menu only
        - credentials display setting removed, will prompt whenever device isn't connected to anything or if user denied typing prompts
        - knock detection enabled as a user settings through the device settings menu
        - scrolling to allow dismissing going to sleep message
        - memory corruption bug fix for 3 lines notifications
- v0.13:- left handed mode when usb and battery connected: prompt when device is steadily held
        - accelerometer routine called in long functions
        - battery charge failed displayed on screen
        - correct knock detection icon
- v0.14:- Detecting failed accelerometer (values stuck)
- v0.15:- Periodic main MCU wakeup to check battery level
        - Using main MCU internal pullup to limit OLED stepup inrush current
- v0.16:- RF functional testing at first boot
        - Do not go to sleep when battery powered and debugger is connected
        - Main menu, bluetooth icon: if Bluetooth is disabled prompt to enable it. If Bluetooth enabled then go to menu
        - Reporting battery level during charge, animation changed accordingly
- v0.17:- Changed battery measurement logic to include a measurement queue to discard effects of USB plugging
- v0.18:- Handling both Bluetooth pair and connect messages
- v0.19:- Database model rework to include multiple standard credential types
        - Timeout to prevent aux mcu flooding
        - Webauthn support
        - BLE bonding information storage & recall
        - Pairing menu operational
        - Deny/Accept animation bug fix
        - Handle power switches during prompts
        - Allow aux MCU to overwrite battery status at first boot
- v0.20:- _unlock_ service feature
        - detection of computer shutdown
- v0.21:- fix of various glitches due to power disconnections
        - Bluetooth security increase
- v0.22:- René's release, various bug fixes
- v0.23:- more gracefully handling USB to battery power transition
- v0.24:- bt disconnect/switch feature
        - data storage feature
- v0.25:- release candidate firmware
        - device tutorial
        - increased long press to reboot timer to 10s
        - correctly computed string underlines
        - power consumption bug fix
        - always display an animation when charging
- v0.26:- adjusting timeouts to maximize battery life
        - bug fix: status change update on automatic lock
        - keyboard typing unicode range number increase
- v0.27:- debug menu when long press with invalid card
        - battery life improvements (smc pullup logic)
        - battery health test
- v0.28:- battery health test bug fix
- v0.29:- battery recondition debug menu item
- v0.30:- ble pairing bug fix
- v0.31:- tx buffer sending logic rewrite
- v0.32:- active_wait bug fix
- v0.33:- ios connectivity bug fix
- v0.34:- unlock feature over ble bug fix
        - smartcard power up sequence improvement
        - scroll wheel detection bug fix
        - high sleep power consumption bug fix
- v0.35:- correct behavior when browsing favorite with "all categories" filter selected
        - mooltipass mini password typing bug fix
- v0.36:- comms rearm on invalid payload length
- v0.37:- do not deal with comms during periodic wakeup
- v0.38:- card power up bug fix
        - false wheel detection during lock bug fix
- v0.39:- allow aux mcu wakeup when message needs to be sent during periodic wake ups
- v0.40:- invalid card inserted bug fix
        - faster card communications
- v0.41:- smarter battery status display
- v0.42:- various FIDO2 bug fixes
- v0.43:- user language / layout reset in case of change of # of languages/layouts on the device
        - always ask device default language after firmware update
        - unlock feature: do not prompt if computer is already unlocked (pending MC support)
        - enable / disable bluetooth before PIN prompt
        - device tutorial: only go to next step if correct action performed
        - different charge algorithm when the battery is at low voltage
        - device remembers battery voltage at last power off
        - aux-main communication bug fix
- v0.44:- aux-main communication real bug fix
- v0.45:- correct screen when importing unknown database
        - use of main MCU systick to prevent aux MCU message flooding
        - end of bundle upload bug fix
- v0.46:- disconnect from USB before updating aux
- v0.47:- more intuitive behavior of long click
- v0.48:- settings to disable bluetooth on lock & card removal
        - faster usb get credential answer for subdomains
        - erase profile feature bug fix
        - faster csv import feature through change node pwd command
        - inactivity timer for device lock 
        - credential loop in login menu
        - jumping through first letters bug fix
        - unlock feature: option to bypass password prompt
        - setting to switch off device after USB disconnection
        - updated status message to include battery pct & settings changed flag
- v0.49:- disabling bluetooth when updating platform
        - emergency power down
        - making sure computer lock status was received for no password prompt setting
        - battery status improvement
- v0.50:- small delay after enabling bluetooth
        - improved battery charging & status reading
- v0.51:- using allocated timers for timeouts (002 error fix)
        - handling all BLE pin requests
        - RTC switched to uint32_t mode
        - RTC calibration code
        - BLE disable as a command
- v0.52:- improved battery readings across reboots
        - improved battery charging logic
- v0.53:- bug fix: battery status across reboots
- v0.54:- 002/005 bug fix: configurable aux mcu timeout value
        - notification on failed BLE connections
        - new bundle upload technique
        - authentication challenge
        - hash display feature
        - updated dtm tx
        - new dtm rx
- v0.55:- switching to release build
- v0.56:- 005 error upon usb disconnecting during BLE enable
        - Unlock screen: bluetooth enable in the background
        - Unlock screen: bluetooth disable if enabled
        - Configurable information display delay
- v0.57:- ACK for bonding clear
        - improved battery status read
- v0.58:- bug fix: bluetooth disable on pin enter cancel
        - bug fix: screen oddities when saving settings
        - screen saver
        - removal of second clear comms on sleep wakeup
        - breaking change: TOTP secret size increase
        - settings: login menu to start with last queried service
        - bug fix: device incorrectly charging at boot when powered off
- v0.59:- check that time is set for TOTP
        - using a kind of time based counter for FIDO2
- v0.60:- bug fix: high power consumption during sleep
- v0.61:- bug fix: eternal sleep
- v0.62:- fixed sleep stability issues
        - modify file command (ssh feature support)
        - notes files support
- v0.63:- visual glitch fix on screen saver exit
        - on-board random password generation when no password specified
        - going to sleep: do not get interrupted by BLE command messages
        - new inactivity timer allowing minute granularity
        - bug fix: bluetooth shortcuts
- v0.64:- visual glitch fix on device lock with screen saver
        - correct decision on ADC measurement fallback
- v0.65:- temporary keyboard layout set command
        - visual glitch fix on device lock with screen saver
        - updated reconditioning code to output discharge time
        - updated reconditioning code for more current draw
        - define to stop platform on 00X error
- v0.66:- adjustement battery reading to be a bit more pessimistic
- v0.67:- remembering last used credential for a given service
- v0.68:- status message informing charge
        - kickstarter fw release
        - bundle v0
- v0.69:- rearms change number increment upon USB / BLE connection (in case the user isn't logged out between device connections)
        - ADC watchdog timer timeout warning
- v0.70:- new HID messages: get platform SN, switch off on next USB disconnect
        - only set not first boot flag when it is not set
- v0.71:- new message filter for all except SN query (used for label printing)
        - very low battery voltage detection at boot when USB powered
        - bug fix: wrong category when creating note
        - bug fix: reply with correct message for notes
        - functional testing bug fix: lower diff goal for bandgap measurement
        - use of two SNs: an internally programmed one and one set during mass production
        - previous mini generation data decryption bug fix and automatic trim for data nodes < 128B
        - bundle v1 (except for the mini 50 batch that doesn't contain prev gen decrypt bug fix)
- v0.72:- device wakeup on device timeout to consume power
        - more rigorous click wheel functional test        
        - USB commands to delete notes & files
        - longer wheel debounce
- v0.73:- fix visual glitch fix when saving language
        - bundle v2
- v0.74:- _unlock_ service check only if feature is enabled
        - mechanism to prevent infinite loop when searching for service
        - cleaner way of initializing user context
        - ignoring click up/down during pin entry
- v0.75:- mechanism to check for database loop when scanning for last nodes 
        - support for FIDO2 allowlist > 1
        - bundle v3
- v0.76:- updated functional test for quick wheel clicks
        - NVM wait for erase and writes
        - bundle v4
*/

/**************** SETUP DEFINES ****************/
/*  This project should be built differently
 *  depending on the Mooltipass version.
 *  Simply define one of these:
 *
 *  PLAT_V1_SETUP
 *  => only 2 boards were made, one shipped to Miguel, one to Mathieu. No silkscreen marking
 *
 *  PLAT_V2_SETUP
 *  => board with the new LT1613 stepup, "beta v2 (new DC/DC)" silkscreen. SMC_POW_nEN pin change.
 *
 *  PLAT_V3_SETUP
 *  => MiniBLE v2 breakout, 13/06/2018
 * - MAIN_MCU_WAKE removed (AUX_Tx used for wakeup)
 * - FORCE_RESET_AUX removed (little added benefits)
 * - NO_COMMS added (see github pages)
 *
 * PLAT_V4_SETUP
 * => 4 Final form factor prototypes produced, 15/05/2019
 * - exact same layout as PLAT_V3_SETUP, except 1V5 3V3 stepup set to 3.16V
 *
 * PLAT_V5_SETUP
 * => 50 prototype units produced for beta testers, 01/10/2019
 * - exact same layout as PLAT_V4_SETUP, except external pullup on SMC detect & new scroll wheel
 *
 * PLAT_V6_SETUP
 * => 50 prototype units produced, 01/01/2020
 * - exact same layout as PLAT_V5_SETUP, except VOLED_1V2_EN swapped with SMC_PGM
 *
 * PLAT_V7_SETUP
 * => Kickstarter mass production setup
 * - exactly like PLAT_V6_SETUP, but with a different transistor for faster card communications
 */
 
 /* Features depending on the defined platform */
 #if defined(PLAT_V1_SETUP)
     #define OLED_PRINTF_ENABLED
     #define DEBUG_USB_COMMANDS_ENABLED
     #define DEBUG_MENU_ENABLED
     #define NO_SECURITY_BIT_CHECK
     #define DEBUG_USB_PRINTF_ENABLED
     #define DEVELOPER_FEATURES_ENABLED     
     #define BOD_NOT_ENABLED
     #define DBFLASH_CHIP_8M
#elif defined(PLAT_V2_SETUP)
     #define OLED_PRINTF_ENABLED
     #define DEBUG_USB_COMMANDS_ENABLED
     #define DEBUG_MENU_ENABLED
     #define NO_SECURITY_BIT_CHECK
     #define DEBUG_USB_PRINTF_ENABLED
     #define DEVELOPER_FEATURES_ENABLED
     #define BOD_NOT_ENABLED
     #define DBFLASH_CHIP_8M
#elif defined(PLAT_V3_SETUP) || defined(PLAT_V4_SETUP) || defined(PLAT_V5_SETUP) || defined(PLAT_V6_SETUP)
     #define OLED_PRINTF_ENABLED
     #define DEBUG_USB_COMMANDS_ENABLED
     #define DEBUG_MENU_ENABLED
     #define NO_SECURITY_BIT_CHECK
     #define DEBUG_USB_PRINTF_ENABLED
     #define DEVELOPER_FEATURES_ENABLED
     #define BOD_NOT_ENABLED
     #define DBFLASH_CHIP_8M
     #define STACK_MEASURE_ENABLED
#elif defined(PLAT_V7_SETUP)
     #define BOD_NOT_ENABLED
     #define DBFLASH_CHIP_8M
     #define HALT_ON_00X_ERROR
#endif

#if defined(EMULATOR_BUILD)
    #undef DEVELOPER_FEATURES_ENABLED
#endif

/* Developer features */
#ifdef DEVELOPER_FEATURES_ENABLED
    #define DEV_SKIP_INTRO_ANIM
    #define SPECIAL_DEVELOPER_CARD_FEATURE
#endif

/* Enums */
typedef enum {PIN_GROUP_0 = 0, PIN_GROUP_1 = 1} pin_group_te;
typedef uint32_t PIN_MASK_T;
typedef uint32_t PIN_ID_T;

/* Structs */
typedef struct
{
    Sercom* sercom_pt;
    pin_group_te cs_pin_group;
    PIN_MASK_T cs_pin_mask;
} spi_flash_descriptor_t;

/*********************************************/
/*  Bootloader and memory related defines    */
/*********************************************/
#ifdef FEATURE_NVM_RWWEE
    #define RWWEE_MEMORY ((volatile uint16_t *)NVMCTRL_RWW_EEPROM_ADDR)
#endif
/* Simply an array point to our internal memory */
#define NVM_MEMORY ((volatile uint16_t *)FLASH_ADDR)
/* Our firmware start address */
#if defined(PLAT_V7_SETUP)
    #define APP_START_ADDR (0x8000)
#else
    #define APP_START_ADDR (0x2000)
#endif
/* Internal storage slot for settings storage */
#define SETTINGS_STORAGE_SLOT               0
#define FLAGS_STORAGE_SLOT                  1
#define FIRST_CPZ_LUT_ENTRY_STORAGE_SLOT    4

/**********************/
/* Device limitations */
/**********************/
#define MAX_NUMBER_OF_USERS     112

/*****************/
/* Comms defines */
/*****************/
#define AUX_MCU_MSG_PAYLOAD_LENGTH  552
#define HID_PAYLOAD_SIZE            64
#define SET_DATE_MSG_INTERVAL_S     4096

/********************/
/* Settings defines */
/********************/
#define FIRMWARE_UPGRADE_FLAG   0x5478ABAA

/********************/
/* Timeout defines  */
/********************/
#define EMULATOR_SCREEN_TIMEOUT_MS  60000
#define SCREEN_TIMEOUT_MS           15000
#define SCREEN_TIMEOUT_MS_BAT_PWRD  7654
#define AUX_FLOOD_TIMEOUT_MS        1
#define SLEEP_AFTER_AUX_WAKEUP_MS   1234

/********************/
/* Voltage cutout   */
/********************/
#if defined(PLAT_V1_SETUP) || defined(PLAT_V2_SETUP) || defined(PLAT_V3_SETUP)
    #define BATTERY_ADC_100PCT_VOLTAGE  (1180*273/110)
    #define BATTERY_ADC_90PCT_VOLTAGE   (1180*273/110)
    #define BATTERY_ADC_80PCT_VOLTAGE   (1250*273/110)
    #define BATTERY_ADC_70PCT_VOLTAGE   (1220*273/110)
    #define BATTERY_ADC_60PCT_VOLTAGE   (1200*273/110)
    #define BATTERY_ADC_50PCT_VOLTAGE   (1180*273/110)
    #define BATTERY_ADC_40PCT_VOLTAGE   (1180*273/110)
    #define BATTERY_ADC_30PCT_VOLTAGE   (1180*273/110)
    #define BATTERY_ADC_20PCT_VOLTAGE   (1180*273/110)
    #define BATTERY_ADC_10PCT_VOLTAGE   (1180*273/110)
    #define BATTERY_ADC_OUT_CUTOUT      (1180*273/110)
    #define BATTERY_ADC_EMGCY_CUTOUT    (1180*273/110)
    #define BATTERY_ADC_800MV_VALUE     (1180*273/110)
#elif defined(PLAT_V4_SETUP) || defined(PLAT_V5_SETUP) || defined(PLAT_V6_SETUP) || defined(PLAT_V7_SETUP)
    #define BATTERY_ADC_100PCT_VOLTAGE  (3435)
    #define BATTERY_ADC_90PCT_VOLTAGE   (3368)
    #define BATTERY_ADC_80PCT_VOLTAGE   (3335)
    #define BATTERY_ADC_70PCT_VOLTAGE   (3310)
    #define BATTERY_ADC_60PCT_VOLTAGE   (3294)
    #define BATTERY_ADC_50PCT_VOLTAGE   (3280)
    #define BATTERY_ADC_40PCT_VOLTAGE   (3260)
    #define BATTERY_ADC_30PCT_VOLTAGE   (3228)
    #define BATTERY_ADC_20PCT_VOLTAGE   (3080)
    #define BATTERY_ADC_10PCT_VOLTAGE   (2950)
    #define BATTERY_ADC_OUT_CUTOUT      (2850)
    #define BATTERY_ADC_EMGCY_CUTOUT    (2700)
    #define BATTERY_ADC_800MV_VALUE     (2056)
#endif
#define BATTERY_1V_WHEN_MEASURED_FROM_MAIN_WITH_USB         (1000*8192/3300)
#define BATTERY_ADC_FOR_BATTERY_STATUS_READ_STRAT_SWITCH    BATTERY_ADC_20PCT_VOLTAGE

/********************/
/* Display defines  */
/********************/
#define GUI_DISPLAY_WIDTH           256

/*************************/
/* Functionality defines */
/*************************/
/* specify that the flash is alone on the spi bus */
#define FLASH_ALONE_ON_SPI_BUS
/* Use DMA transfers to read from external flash */
#ifndef BOOTLOADER
    #define FLASH_DMA_FETCHES
#endif
/* Use DMA transfers to send data to OLED screen */
#ifndef BOOTLOADER
    #define OLED_DMA_TRANSFER
#endif
/* Use a frame buffer on the platform for non bootloader solutions */
#ifndef BOOTLOADER
    #define OLED_INTERNAL_FRAME_BUFFER
#endif
/* allow printf for the screen */
//#define OLED_PRINTF_ENABLED
/* Allow debug USB commands */
//#define DEBUG_USB_COMMANDS_ENABLED
/* Allow debug menu */
//#define DEBUG_MENU_ENABLED
/* No security bit check */
//#define NO_SECURITY_BIT_CHECK
/* Debug printf through USB */
//#define DEBUG_USB_PRINTF_ENABLED
/* Allow import / export of the provisioned aes key & flag */
#define AES_PROVISIONED_KEY_IMPORT_EXPORT_ALLOWED

/* GCLK ID defines */
#define GCLK_ID_48M             GCLK_CLKCTRL_GEN_GCLK0_Val
#define GCLK_ID_32K             GCLK_CLKCTRL_GEN_GCLK3_Val

/* ADC defines */
#if defined(PLAT_V1_SETUP) || defined(PLAT_V2_SETUP)
    #define VBAT_ADC_PIN_MUXPOS     ADC_INPUTCTRL_MUXPOS_PIN1_Val
#elif defined(PLAT_V3_SETUP) || defined(PLAT_V4_SETUP) || defined(PLAT_V5_SETUP) || defined(PLAT_V6_SETUP) || defined(PLAT_V7_SETUP)
    #define VBAT_ADC_PIN_MUXPOS     ADC_INPUTCTRL_MUXPOS_PIN0_Val
#endif

/* SERCOM defines */
#if defined(PLAT_V1_SETUP) || defined(PLAT_V2_SETUP)
    #define SMARTCARD_GCLK_SERCOM_ID    GCLK_CLKCTRL_ID_SERCOM5_CORE_Val
    #define SMARTCARD_MOSI_SCK_PADS     MOSI_P0_SCK_P3_SS_P1
    #define SMARTCARD_MISO_PAD          MISO_PAD1
    #define SMARTCARD_APB_SERCOM_BIT    SERCOM5_
    #define SMARTCARD_SERCOM            SERCOM5
    #define DATAFLASH_GCLK_SERCOM_ID    GCLK_CLKCTRL_ID_SERCOM2_CORE_Val
    #define DATAFLASH_MOSI_SCK_PADS     MOSI_P0_SCK_P1_SS_P2
    #define DATAFLASH_MISO_PAD          MISO_PAD2
    #define DATAFLASH_APB_SERCOM_BIT    SERCOM2_
    #define DATAFLASH_SERCOM            SERCOM2
    #define DBFLASH_GCLK_SERCOM_ID      GCLK_CLKCTRL_ID_SERCOM3_CORE_Val
    #define DBFLASH_MOSI_SCK_PADS       MOSI_P0_SCK_P1_SS_P2
    #define DBFLASH_MISO_PAD            MISO_PAD3
    #define DBFLASH_APB_SERCOM_BIT      SERCOM3_
    #define DBFLASH_SERCOM              SERCOM3
    #define AUXMCU_GCLK_SERCOM_ID       GCLK_CLKCTRL_ID_SERCOM4_CORE_Val
    #define AUXMCU_APB_SERCOM_BIT       SERCOM4_
    #define AUXMCU_SERCOM               SERCOM4
    #define AUXMCU_SERCOM_HANDLER       SERCOM4_Handler
    #define AUXMCU_SERCOM_INTERUPT      SERCOM4_IRQn
    #define AUXMCU_RX_TXPO              1
    #define AUXMCU_TX_PAD               3
    #define OLED_GCLK_SERCOM_ID         GCLK_CLKCTRL_ID_SERCOM0_CORE_Val
    #define OLED_MOSI_SCK_PADS          MOSI_P0_SCK_P1_SS_P2
    #define OLED_MISO_PAD               MISO_PAD3
    #define OLED_APB_SERCOM_BIT         SERCOM0_
    #define OLED_SERCOM                 SERCOM0
    #define ACC_GCLK_SERCOM_ID          GCLK_CLKCTRL_ID_SERCOM1_CORE_Val
    #define ACC_MOSI_SCK_PADS           MOSI_P0_SCK_P3_SS_P1
    #define ACC_MISO_PAD                MISO_PAD1
    #define ACC_APB_SERCOM_BIT          SERCOM1_
    #define ACC_SERCOM                  SERCOM1
#elif defined(PLAT_V3_SETUP) || defined(PLAT_V4_SETUP) || defined(PLAT_V5_SETUP) || defined(PLAT_V6_SETUP) || defined(PLAT_V7_SETUP)
    #define SMARTCARD_GCLK_SERCOM_ID    GCLK_CLKCTRL_ID_SERCOM2_CORE_Val
    #define SMARTCARD_MOSI_SCK_PADS     MOSI_P3_SCK_P1_SS_P2
    #define SMARTCARD_MISO_PAD          MISO_PAD2
    #define SMARTCARD_APB_SERCOM_BIT    SERCOM2_
    #define SMARTCARD_SERCOM            SERCOM2
    #define DATAFLASH_GCLK_SERCOM_ID    GCLK_CLKCTRL_ID_SERCOM3_CORE_Val
    #define DATAFLASH_MOSI_SCK_PADS     MOSI_P2_SCK_P3_SS_P1
    #define DATAFLASH_MISO_PAD          MISO_PAD0
    #define DATAFLASH_APB_SERCOM_BIT    SERCOM3_
    #define DATAFLASH_SERCOM            SERCOM3
    #define DBFLASH_GCLK_SERCOM_ID      GCLK_CLKCTRL_ID_SERCOM1_CORE_Val
    #define DBFLASH_MOSI_SCK_PADS       MOSI_P3_SCK_P1_SS_P2
    #define DBFLASH_MISO_PAD            MISO_PAD2
    #define DBFLASH_APB_SERCOM_BIT      SERCOM1_
    #define DBFLASH_SERCOM              SERCOM1
    #define AUXMCU_GCLK_SERCOM_ID       GCLK_CLKCTRL_ID_SERCOM5_CORE_Val
    #define AUXMCU_APB_SERCOM_BIT       SERCOM5_
    #define AUXMCU_SERCOM               SERCOM5
    #define AUXMCU_SERCOM_HANDLER       SERCOM5_Handler
    #define AUXMCU_SERCOM_INTERUPT      SERCOM5_IRQn
    #define AUXMCU_RX_TXPO              1
    #define AUXMCU_TX_PAD               3
    #define OLED_GCLK_SERCOM_ID         GCLK_CLKCTRL_ID_SERCOM4_CORE_Val
    #define OLED_MOSI_SCK_PADS          MOSI_P2_SCK_P3_SS_P1
    #define OLED_MISO_PAD               MISO_PAD0
    #define OLED_APB_SERCOM_BIT         SERCOM4_
    #define OLED_SERCOM                 SERCOM4
    #define ACC_GCLK_SERCOM_ID          GCLK_CLKCTRL_ID_SERCOM0_CORE_Val
    #define ACC_MOSI_SCK_PADS           MOSI_P0_SCK_P3_SS_P1
    #define ACC_MISO_PAD                MISO_PAD1
    #define ACC_APB_SERCOM_BIT          SERCOM0_
    #define ACC_SERCOM                  SERCOM0
#endif

/* DMA channel descriptors */
#define DMA_DESCID_RX_COMMS         0
#define DMA_DESCID_RX_FS            1
#define DMA_DESCID_TX_FS            2
#define DMA_DESCID_TX_ACC           3
#define DMA_DESCID_TX_OLED          4
#define DMA_DESCID_RX_ACC           5
#define DMA_DESCID_TX_COMMS         6

/* External interrupts numbers */
#if defined(PLAT_V1_SETUP) || defined(PLAT_V2_SETUP)
    #define ACC_EXTINT_NUM              4
    #define ACC_EIC_SENSE_REG           SENSE4
    #define WHEEL_CLICK_EXTINT_NUM      8
    #define WHEEL_CLICK_EIC_SENSE_REG   SENSE0
    #define WHEEL_TICKA_EXTINT_NUM      0
    #define WHEEL_TICKA_EIC_SENSE_REG   SENSE0
    #define WHEEL_TICKB_EXTINT_NUM      1
    #define WHEEL_TICKB_EIC_SENSE_REG   SENSE1
    #define USB_3V3_EXTINT_NUM          15
    #define USB_3V3_EIC_SENSE_REG       SENSE7
#elif defined(PLAT_V3_SETUP) || defined(PLAT_V4_SETUP) || defined(PLAT_V5_SETUP) || defined(PLAT_V6_SETUP) || defined(PLAT_V7_SETUP)
    #define ACC_EXTINT_NUM                      9
    #define ACC_EIC_SENSE_REG                   SENSE1
    #define WHEEL_CLICK_EXTINT_NUM              8
    #define WHEEL_CLICK_EIC_SENSE_REG           SENSE0
    #define WHEEL_TICKA_EXTINT_NUM              15
    #define WHEEL_TICKA_EIC_SENSE_REG           SENSE7
    #define WHEEL_TICKB_EXTINT_NUM              2
    #define WHEEL_TICKB_EIC_SENSE_REG           SENSE2
    #define USB_3V3_EXTINT_NUM                  1
    #define USB_3V3_EIC_SENSE_REG               SENSE1
    #define AUX_MCU_TX_EXTINT_NUM               7
    #define AUX_MCU_TX_EIC_SENSE_REG            SENSE7
    #define AUX_MCU_NO_COMMS_EXTINT_NUM         10
    #define AUX_MCU_NO_COMMS_EXTINT_SENSE_REG   SENSE2
    #define SMC_DET_EXTINT_NUM                  3
    #define SMC_DET_EXTINT_SENSE_REG            SENSE3
#endif

/* User event channels mapping */
#define ACC_EV_GEN_CHANNEL          0
#define ACC_EV_GEN_SEL              (0x0C + ACC_EXTINT_NUM)

/* SERCOM trigger for flash data transfers */
#if defined(PLAT_V1_SETUP) || defined(PLAT_V2_SETUP)
    #define DATAFLASH_DMA_SERCOM_RXTRIG     0x05
    #define DATAFLASH_DMA_SERCOM_TXTRIG     0x06
    #define DBFLASH_DMA_SERCOM_RXTRIG       0x07
    #define DBFLASH_DMA_SERCOM_TXTRIG       0x08
    #define ACC_DMA_SERCOM_RXTRIG           0x03
    #define ACC_DMA_SERCOM_TXTRIG           0x04
    #define AUX_MCU_SERCOM_RXTRIG           0x09
    #define AUX_MCU_SERCOM_TXTRIG           0x0A
#elif defined(PLAT_V3_SETUP) || defined(PLAT_V4_SETUP) || defined(PLAT_V5_SETUP) || defined(PLAT_V6_SETUP) || defined(PLAT_V7_SETUP)
    #define DATAFLASH_DMA_SERCOM_RXTRIG     0x07
    #define DATAFLASH_DMA_SERCOM_TXTRIG     0x08
    #define DBFLASH_DMA_SERCOM_RXTRIG       0x03
    #define DBFLASH_DMA_SERCOM_TXTRIG       0x04
    #define ACC_DMA_SERCOM_RXTRIG           0x01
    #define ACC_DMA_SERCOM_TXTRIG           0x02
    #define AUX_MCU_SERCOM_RXTRIG           0x0B
    #define AUX_MCU_SERCOM_TXTRIG           0x0C
#endif

/* SERCOM trigger for OLED data transfers */
#if defined(PLAT_V1_SETUP) || defined(PLAT_V2_SETUP)
    #define OLED_DMA_SERCOM_TX_TRIG         0x02
#elif defined(PLAT_V3_SETUP) || defined(PLAT_V4_SETUP) || defined(PLAT_V5_SETUP) || defined(PLAT_V6_SETUP) || defined(PLAT_V7_SETUP)
    #define OLED_DMA_SERCOM_TX_TRIG         0x0A
#endif

/* Speed defines */
#define CPU_SPEED_HF                48000000UL
#define CPU_SPEED_MF                8000000UL
/* SMARTCARD SPI SCK =  48M / (2*(119+1)) = 200kHz (Supposed Max is 300kHz) */
#if defined(PLAT_V7_SETUP)
    #define SMARTCARD_BAUD_DIVIDER      119
#else
    #define SMARTCARD_BAUD_DIVIDER      239
#endif
/* OLED SPI SCK = 48M / 2*(5+1)) = 4MHz (Max from datasheet) */
/* Note: Has successfully been tested at 24MHz, but without speed improvements */
#define OLED_BAUD_DIVIDER           5
/* ACC SPI SCK = 48M / 2*(2+1)) = 8MHz (Max is 10MHz) */
#define ACC_BAUD_DIVIDER            2
/* DATA & DB FLASH SPI SCK = 48M / 2*(1+1)) = 12MHz (24MHz may not work on some boards when querying JEDEC ID) */
#define DATAFLASH_BAUD_DIVIDER      1
#define DBFLASH_BAUD_DIVIDER        1

/* Reboot timer */
#define NB_MS_WHEEL_PRESS_FOR_REBOOT    10000

/* AUX MCU ping */
#define NB_MS_AUX_MCU_PING              10000

/* Number of active wait loops before we give up trying to receive a message from aux */
#define NB_ACTIVE_WAIT_LOOP_TIMEOUT     10

/* Value set inside the MCU systick timer to not send 2 messages to the AUX MCU too close to each other (as the AUX DMA interrupt may take a little while to fire) */
#define MCU_SYSTICK_VAL_FOR_AUX_RX_TO   7200    // Around 150us

/* PORT defines */
/* WHEEL ENCODER */
#if defined(PLAT_V1_SETUP) || defined(PLAT_V2_SETUP)
    #define WHEEL_A_GROUP           PIN_GROUP_0
    #define WHEEL_A_PINID           0
#elif defined(PLAT_V3_SETUP) || defined(PLAT_V4_SETUP) || defined(PLAT_V5_SETUP) || defined(PLAT_V6_SETUP) || defined(PLAT_V7_SETUP)
    #define WHEEL_A_GROUP           PIN_GROUP_0
    #define WHEEL_A_PINID           27
#endif
#define WHEEL_A_MASK            (1UL << WHEEL_A_PINID)
#if (WHEEL_A_PINID % 2) == 1
    #define WHEEL_A_PMUXREGID   PMUXO
#else
    #define WHEEL_A_PMUXREGID   PMUXE
#endif

#if defined(PLAT_V1_SETUP) || defined(PLAT_V2_SETUP)
    #define WHEEL_B_GROUP           PIN_GROUP_0
    #define WHEEL_B_PINID           1
#elif defined(PLAT_V3_SETUP) || defined(PLAT_V4_SETUP) || defined(PLAT_V5_SETUP) || defined(PLAT_V6_SETUP) || defined(PLAT_V7_SETUP)
    #define WHEEL_B_GROUP           PIN_GROUP_1
    #define WHEEL_B_PINID           2
#endif
#define WHEEL_B_MASK            (1UL << WHEEL_B_PINID)
#if (WHEEL_B_PINID % 2) == 1
    #define WHEEL_B_PMUXREGID   PMUXO
#else
    #define WHEEL_B_PMUXREGID   PMUXE
#endif

#if defined(PLAT_V1_SETUP) || defined(PLAT_V2_SETUP)
    #define WHEEL_SW_GROUP          PIN_GROUP_0
    #define WHEEL_SW_PINID          28
#elif defined(PLAT_V3_SETUP) || defined(PLAT_V4_SETUP) || defined(PLAT_V5_SETUP) || defined(PLAT_V6_SETUP) || defined(PLAT_V7_SETUP)
    #define WHEEL_SW_GROUP          PIN_GROUP_0
    #define WHEEL_SW_PINID          28
#endif
#define WHEEL_SW_MASK           (1UL << WHEEL_SW_PINID)
#if (WHEEL_SW_PINID % 2) == 1
    #define WHEEL_SW_PMUXREGID  PMUXO
#else
    #define WHEEL_SW_PMUXREGID  PMUXE
#endif

/* POWER & BLE SYSTEM */
#if defined(PLAT_V1_SETUP) || defined(PLAT_V2_SETUP)
    #define SWDET_EN_GROUP          PIN_GROUP_0
    #define SWDET_EN_PINID          2
#elif defined(PLAT_V3_SETUP) || defined(PLAT_V4_SETUP) || defined(PLAT_V5_SETUP) || defined(PLAT_V6_SETUP) || defined(PLAT_V7_SETUP)
    #define SWDET_EN_GROUP          PIN_GROUP_0
    #define SWDET_EN_PINID          15
#endif
#define SWDET_EN_MASK           (1UL << SWDET_EN_PINID)

#if defined(PLAT_V1_SETUP)
    #define SMC_POW_NEN_GROUP   PIN_GROUP_0
    #define SMC_POW_NEN_PINID   3
    #define SMC_POW_NEN_MASK    (1UL << SMC_POW_NEN_PINID)
#elif defined(PLAT_V2_SETUP)
    #define SMC_POW_NEN_GROUP   PIN_GROUP_0
    #define SMC_POW_NEN_PINID   30
    #define SMC_POW_NEN_MASK    (1UL << SMC_POW_NEN_PINID)
#elif defined(PLAT_V3_SETUP) || defined(PLAT_V4_SETUP) || defined(PLAT_V5_SETUP) || defined(PLAT_V6_SETUP) || defined(PLAT_V7_SETUP)
    #define SMC_POW_NEN_GROUP   PIN_GROUP_0
    #define SMC_POW_NEN_PINID   25
    #define SMC_POW_NEN_MASK    (1UL << SMC_POW_NEN_PINID)
#endif

#if defined(PLAT_V2_SETUP)
    #define VOLED_VIN_GROUP     PIN_GROUP_0
    #define VOLED_VIN_PINID     3
    #define VOLED_VIN_MASK      (1UL << VOLED_VIN_PINID)
    #define VOLED_VIN_PMUX_ID   PORT_PMUX_PMUXE_B_Val
    #if (VOLED_VIN_PINID % 2) == 1
        #define VOLED_VIN_PMUXREGID PMUXO
    #else
        #define VOLED_VIN_PMUXREGID PMUXE
    #endif
#elif defined(PLAT_V3_SETUP) || defined(PLAT_V4_SETUP) || defined(PLAT_V5_SETUP) || defined(PLAT_V6_SETUP) || defined(PLAT_V7_SETUP)
    #define VOLED_VIN_GROUP     PIN_GROUP_0
    #define VOLED_VIN_PINID     2
    #define VOLED_VIN_MASK      (1UL << VOLED_VIN_PINID)
    #define VOLED_VIN_PMUX_ID   PORT_PMUX_PMUXE_B_Val
    #if (VOLED_VIN_PINID % 2) == 1
        #define VOLED_VIN_PMUXREGID PMUXO
    #else
        #define VOLED_VIN_PMUXREGID PMUXE
    #endif
#endif

#if defined(PLAT_V1_SETUP) || defined(PLAT_V2_SETUP)
    #define BLE_EN_GROUP            PIN_GROUP_0
    #define BLE_EN_PINID            13
#elif defined(PLAT_V3_SETUP) || defined(PLAT_V4_SETUP) || defined(PLAT_V5_SETUP) || defined(PLAT_V6_SETUP) || defined(PLAT_V7_SETUP)
    #define BLE_EN_GROUP            PIN_GROUP_1
    #define BLE_EN_PINID            3
#endif
#define BLE_EN_MASK             (1UL << BLE_EN_PINID)

#if defined(PLAT_V1_SETUP) || defined(PLAT_V2_SETUP)
    #define USB_3V3_GROUP           PIN_GROUP_0
    #define USB_3V3_PINID           27
#elif defined(PLAT_V3_SETUP) || defined(PLAT_V4_SETUP) || defined(PLAT_V5_SETUP) || defined(PLAT_V6_SETUP) || defined(PLAT_V7_SETUP)
    #define USB_3V3_GROUP           PIN_GROUP_0
    #define USB_3V3_PINID           1
#endif
#define USB_3V3_MASK            (1UL << USB_3V3_PINID)
#if (USB_3V3_PINID % 2) == 1
    #define USB_3V3_PMUXREGID  PMUXO
#else
    #define USB_3V3_PMUXREGID  PMUXE
#endif

#if defined(PLAT_V1_SETUP) || defined(PLAT_V2_SETUP)
    #define VOLED_1V2_EN_GROUP      PIN_GROUP_1
    #define VOLED_1V2_EN_PINID      22
#elif defined(PLAT_V3_SETUP) || defined(PLAT_V4_SETUP) || defined(PLAT_V5_SETUP)
    #define VOLED_1V2_EN_GROUP      PIN_GROUP_0
    #define VOLED_1V2_EN_PINID      8
#elif defined(PLAT_V6_SETUP) || defined(PLAT_V7_SETUP)
    #define VOLED_1V2_EN_GROUP      PIN_GROUP_1
    #define VOLED_1V2_EN_PINID      8
#endif
#define VOLED_1V2_EN_MASK       (1UL << VOLED_1V2_EN_PINID)

#if defined(PLAT_V1_SETUP) || defined(PLAT_V2_SETUP)
    #define VOLED_3V3_EN_GROUP      PIN_GROUP_1
    #define VOLED_3V3_EN_PINID      23
#elif defined(PLAT_V3_SETUP) || defined(PLAT_V4_SETUP) || defined(PLAT_V5_SETUP) || defined(PLAT_V6_SETUP) || defined(PLAT_V7_SETUP)
    #define VOLED_3V3_EN_GROUP      PIN_GROUP_0
    #define VOLED_3V3_EN_PINID      0
#endif
#define VOLED_3V3_EN_MASK       (1UL << VOLED_3V3_EN_PINID)

#if defined(PLAT_V1_SETUP) || defined(PLAT_V2_SETUP)
    #define MCU_AUX_RST_EN_GROUP    PIN_GROUP_0
    #define MCU_AUX_RST_EN_PINID    15
    #define MCU_AUX_RST_EN_MASK     (1UL << MCU_AUX_RST_EN_PINID)
#endif

/* OLED */
#if defined(PLAT_V1_SETUP) || defined(PLAT_V2_SETUP)
    #define OLED_MOSI_GROUP         PIN_GROUP_0
    #define OLED_MOSI_PINID         4
#elif defined(PLAT_V3_SETUP) || defined(PLAT_V4_SETUP) || defined(PLAT_V5_SETUP) || defined(PLAT_V6_SETUP) || defined(PLAT_V7_SETUP)
    #define OLED_MOSI_GROUP         PIN_GROUP_1
    #define OLED_MOSI_PINID         10
#endif
#define OLED_MOSI_MASK          (1UL << OLED_MOSI_PINID)
#define OLED_MOSI_PMUX_ID       PORT_PMUX_PMUXE_D_Val
#if (OLED_MOSI_PINID % 2) == 1
    #define OLED_MOSI_PMUXREGID PMUXO
#else
    #define OLED_MOSI_PMUXREGID PMUXE
#endif

#if defined(PLAT_V1_SETUP) || defined(PLAT_V2_SETUP)
    #define OLED_SCK_GROUP          PIN_GROUP_0
    #define OLED_SCK_PINID          5
#elif defined(PLAT_V3_SETUP) || defined(PLAT_V4_SETUP) || defined(PLAT_V5_SETUP) || defined(PLAT_V6_SETUP) || defined(PLAT_V7_SETUP)
    #define OLED_SCK_GROUP          PIN_GROUP_1
    #define OLED_SCK_PINID          11
#endif
#define OLED_SCK_MASK           (1UL << OLED_SCK_PINID)
#define OLED_SCK_PMUX_ID        PORT_PMUX_PMUXO_D_Val
#if (OLED_SCK_PINID % 2) == 1
    #define OLED_SCK_PMUXREGID  PMUXO
#else
    #define OLED_SCK_PMUXREGID  PMUXE
#endif

#if defined(PLAT_V1_SETUP) || defined(PLAT_V2_SETUP)
    #define OLED_nCS_GROUP          PIN_GROUP_1
    #define OLED_nCS_PINID          9
#elif defined(PLAT_V3_SETUP) || defined(PLAT_V4_SETUP) || defined(PLAT_V5_SETUP) || defined(PLAT_V6_SETUP) || defined(PLAT_V7_SETUP)
    #define OLED_nCS_GROUP          PIN_GROUP_0
    #define OLED_nCS_PINID          14
#endif
#define OLED_nCS_MASK           (1UL << OLED_nCS_PINID)

#if defined(PLAT_V1_SETUP) || defined(PLAT_V2_SETUP)
    #define OLED_CD_GROUP           PIN_GROUP_0
    #define OLED_CD_PINID           6
#elif defined(PLAT_V3_SETUP) || defined(PLAT_V4_SETUP) || defined(PLAT_V5_SETUP) || defined(PLAT_V6_SETUP) || defined(PLAT_V7_SETUP)
    #define OLED_CD_GROUP           PIN_GROUP_0
    #define OLED_CD_PINID           12
#endif
#define OLED_CD_MASK            (1UL << OLED_CD_PINID)

#if defined(PLAT_V1_SETUP) || defined(PLAT_V2_SETUP)
    #define OLED_nRESET_GROUP       PIN_GROUP_0
    #define OLED_nRESET_PINID       7
#elif defined(PLAT_V3_SETUP) || defined(PLAT_V4_SETUP) || defined(PLAT_V5_SETUP) || defined(PLAT_V6_SETUP) || defined(PLAT_V7_SETUP)
    #define OLED_nRESET_GROUP       PIN_GROUP_0
    #define OLED_nRESET_PINID       13
#endif
#define OLED_nRESET_MASK        (1UL << OLED_nRESET_PINID)

/* DATAFLASH FLASH */
#if defined(PLAT_V1_SETUP) || defined(PLAT_V2_SETUP)
    #define DATAFLASH_MOSI_GROUP         PIN_GROUP_0
    #define DATAFLASH_MOSI_PINID         8
#elif defined(PLAT_V3_SETUP) || defined(PLAT_V4_SETUP) || defined(PLAT_V5_SETUP) || defined(PLAT_V6_SETUP) || defined(PLAT_V7_SETUP)
    #define DATAFLASH_MOSI_GROUP         PIN_GROUP_0
    #define DATAFLASH_MOSI_PINID         20
#endif
#define DATAFLASH_MOSI_MASK          (1UL << DATAFLASH_MOSI_PINID)
#define DATAFLASH_MOSI_PMUX_ID       PORT_PMUX_PMUXE_D_Val
#if (DATAFLASH_MOSI_PINID % 2) == 1
    #define DATAFLASH_MOSI_PMUXREGID PMUXO
#else
    #define DATAFLASH_MOSI_PMUXREGID PMUXE
#endif

#if defined(PLAT_V1_SETUP) || defined(PLAT_V2_SETUP)
    #define DATAFLASH_MISO_GROUP         PIN_GROUP_0
    #define DATAFLASH_MISO_PINID         10
    #define DATAFLASH_MISO_PMUX_ID       PORT_PMUX_PMUXE_D_Val
#elif defined(PLAT_V3_SETUP) || defined(PLAT_V4_SETUP) || defined(PLAT_V5_SETUP) || defined(PLAT_V6_SETUP) || defined(PLAT_V7_SETUP)
    #define DATAFLASH_MISO_GROUP         PIN_GROUP_0
    #define DATAFLASH_MISO_PINID         22
    #define DATAFLASH_MISO_PMUX_ID       PORT_PMUX_PMUXE_C_Val
#endif
#define DATAFLASH_MISO_MASK          (1UL << DATAFLASH_MISO_PINID)
#if (DATAFLASH_MISO_PINID % 2) == 1
    #define DATAFLASH_MISO_PMUXREGID PMUXO
#else
    #define DATAFLASH_MISO_PMUXREGID PMUXE
#endif

#if defined(PLAT_V1_SETUP) || defined(PLAT_V2_SETUP)
    #define DATAFLASH_SCK_GROUP          PIN_GROUP_0
    #define DATAFLASH_SCK_PINID          9
#elif defined(PLAT_V3_SETUP) || defined(PLAT_V4_SETUP) || defined(PLAT_V5_SETUP) || defined(PLAT_V6_SETUP) || defined(PLAT_V7_SETUP)
    #define DATAFLASH_SCK_GROUP          PIN_GROUP_0
    #define DATAFLASH_SCK_PINID          21
#endif
#define DATAFLASH_SCK_MASK           (1UL << DATAFLASH_SCK_PINID)
#define DATAFLASH_SCK_PMUX_ID        PORT_PMUX_PMUXO_D_Val
#if (DATAFLASH_SCK_PINID % 2) == 1
    #define DATAFLASH_SCK_PMUXREGID  PMUXO
#else
    #define DATAFLASH_SCK_PMUXREGID  PMUXE
#endif

#if defined(PLAT_V1_SETUP) || defined(PLAT_V2_SETUP)
    #define DATAFLASH_nCS_GROUP          PIN_GROUP_0
    #define DATAFLASH_nCS_PINID          11
#elif defined(PLAT_V3_SETUP) || defined(PLAT_V4_SETUP) || defined(PLAT_V5_SETUP) || defined(PLAT_V6_SETUP) || defined(PLAT_V7_SETUP)
    #define DATAFLASH_nCS_GROUP          PIN_GROUP_0
    #define DATAFLASH_nCS_PINID          23
#endif
#define DATAFLASH_nCS_MASK           (1UL << DATAFLASH_nCS_PINID)

/* DBFLASH FLASH */
#if defined(PLAT_V1_SETUP) || defined(PLAT_V2_SETUP)
    #define DBFLASH_MOSI_GROUP         PIN_GROUP_0
    #define DBFLASH_MOSI_PINID         22
#elif defined(PLAT_V3_SETUP) || defined(PLAT_V4_SETUP) || defined(PLAT_V5_SETUP) || defined(PLAT_V6_SETUP) || defined(PLAT_V7_SETUP)
    #define DBFLASH_MOSI_GROUP         PIN_GROUP_0
    #define DBFLASH_MOSI_PINID         19
#endif
#define DBFLASH_MOSI_MASK          (1UL << DBFLASH_MOSI_PINID)
#define DBFLASH_MOSI_PMUX_ID       PORT_PMUX_PMUXE_C_Val
#if (DBFLASH_MOSI_PINID % 2) == 1
    #define DBFLASH_MOSI_PMUXREGID PMUXO
#else
    #define DBFLASH_MOSI_PMUXREGID PMUXE
#endif

#if defined(PLAT_V1_SETUP) || defined(PLAT_V2_SETUP)
    #define DBFLASH_MISO_GROUP         PIN_GROUP_0
    #define DBFLASH_MISO_PINID         25
#elif defined(PLAT_V3_SETUP) || defined(PLAT_V4_SETUP) || defined(PLAT_V5_SETUP) || defined(PLAT_V6_SETUP) || defined(PLAT_V7_SETUP)
    #define DBFLASH_MISO_GROUP         PIN_GROUP_0
    #define DBFLASH_MISO_PINID         18
#endif
#define DBFLASH_MISO_MASK          (1UL << DBFLASH_MISO_PINID)
#define DBFLASH_MISO_PMUX_ID       PORT_PMUX_PMUXE_C_Val
#if (DBFLASH_MISO_PINID % 2) == 1
    #define DBFLASH_MISO_PMUXREGID PMUXO
#else
    #define DBFLASH_MISO_PMUXREGID PMUXE
#endif

#if defined(PLAT_V1_SETUP) || defined(PLAT_V2_SETUP)
    #define DBFLASH_SCK_GROUP          PIN_GROUP_0
    #define DBFLASH_SCK_PINID          23
#elif defined(PLAT_V3_SETUP) || defined(PLAT_V4_SETUP) || defined(PLAT_V5_SETUP) || defined(PLAT_V6_SETUP) || defined(PLAT_V7_SETUP)
    #define DBFLASH_SCK_GROUP          PIN_GROUP_0
    #define DBFLASH_SCK_PINID          17
#endif
#define DBFLASH_SCK_MASK           (1UL << DBFLASH_SCK_PINID)
#define DBFLASH_SCK_PMUX_ID        PORT_PMUX_PMUXO_C_Val
#if (DBFLASH_SCK_PINID % 2) == 1
    #define DBFLASH_SCK_PMUXREGID  PMUXO
#else
    #define DBFLASH_SCK_PMUXREGID  PMUXE
#endif

#if defined(PLAT_V1_SETUP) || defined(PLAT_V2_SETUP)
    #define DBFLASH_nCS_GROUP          PIN_GROUP_0
    #define DBFLASH_nCS_PINID          24
#elif defined(PLAT_V3_SETUP) || defined(PLAT_V4_SETUP) || defined(PLAT_V5_SETUP) || defined(PLAT_V6_SETUP) || defined(PLAT_V7_SETUP)
    #define DBFLASH_nCS_GROUP          PIN_GROUP_0
    #define DBFLASH_nCS_PINID          16
#endif
#define DBFLASH_nCS_MASK           (1UL << DBFLASH_nCS_PINID)

/* ACCELEROMETER */
#if defined(PLAT_V1_SETUP) || defined(PLAT_V2_SETUP)
    #define ACC_MOSI_GROUP         PIN_GROUP_0
    #define ACC_MOSI_PINID         16
    #define ACC_MOSI_PMUX_ID       PORT_PMUX_PMUXE_C_Val
#elif defined(PLAT_V3_SETUP) || defined(PLAT_V4_SETUP) || defined(PLAT_V5_SETUP) || defined(PLAT_V6_SETUP) || defined(PLAT_V7_SETUP)
    #define ACC_MOSI_GROUP         PIN_GROUP_0
    #define ACC_MOSI_PINID         4
    #define ACC_MOSI_PMUX_ID       PORT_PMUX_PMUXE_D_Val
#endif
#define ACC_MOSI_MASK          (1UL << ACC_MOSI_PINID)
#if (ACC_MOSI_PINID % 2) == 1
    #define ACC_MOSI_PMUXREGID PMUXO
#else
    #define ACC_MOSI_PMUXREGID PMUXE
#endif

#if defined(PLAT_V1_SETUP) || defined(PLAT_V2_SETUP)
    #define ACC_MISO_GROUP         PIN_GROUP_0
    #define ACC_MISO_PINID         17
    #define ACC_MISO_PMUX_ID       PORT_PMUX_PMUXE_C_Val
#elif defined(PLAT_V3_SETUP) || defined(PLAT_V4_SETUP) || defined(PLAT_V5_SETUP) || defined(PLAT_V6_SETUP) || defined(PLAT_V7_SETUP)
    #define ACC_MISO_GROUP         PIN_GROUP_0
    #define ACC_MISO_PINID         5
    #define ACC_MISO_PMUX_ID       PORT_PMUX_PMUXE_D_Val
#endif
#define ACC_MISO_MASK          (1UL << ACC_MISO_PINID)
#if (ACC_MISO_PINID % 2) == 1
    #define ACC_MISO_PMUXREGID PMUXO
#else
    #define ACC_MISO_PMUXREGID PMUXE
#endif

#if defined(PLAT_V1_SETUP) || defined(PLAT_V2_SETUP)
    #define ACC_SCK_GROUP          PIN_GROUP_0
    #define ACC_SCK_PINID          19
    #define ACC_SCK_PMUX_ID        PORT_PMUX_PMUXO_C_Val
#elif defined(PLAT_V3_SETUP) || defined(PLAT_V4_SETUP) || defined(PLAT_V5_SETUP) || defined(PLAT_V6_SETUP) || defined(PLAT_V7_SETUP)
    #define ACC_SCK_GROUP          PIN_GROUP_0
    #define ACC_SCK_PINID          7
    #define ACC_SCK_PMUX_ID        PORT_PMUX_PMUXO_D_Val
#endif
#define ACC_SCK_MASK           (1UL << ACC_SCK_PINID)
#if (ACC_SCK_PINID % 2) == 1
    #define ACC_SCK_PMUXREGID  PMUXO
#else
    #define ACC_SCK_PMUXREGID  PMUXE
#endif

#if defined(PLAT_V1_SETUP) || defined(PLAT_V2_SETUP)
    #define ACC_nCS_GROUP          PIN_GROUP_0
    #define ACC_nCS_PINID          18
#elif defined(PLAT_V3_SETUP) || defined(PLAT_V4_SETUP) || defined(PLAT_V5_SETUP) || defined(PLAT_V6_SETUP) || defined(PLAT_V7_SETUP)
    #define ACC_nCS_GROUP          PIN_GROUP_0
    #define ACC_nCS_PINID          6
#endif
#define ACC_nCS_MASK           (1UL << ACC_nCS_PINID)

#if defined(PLAT_V1_SETUP) || defined(PLAT_V2_SETUP)
    #define ACC_INT_GROUP          PIN_GROUP_0
    #define ACC_INT_PINID          20
#elif defined(PLAT_V3_SETUP) || defined(PLAT_V4_SETUP) || defined(PLAT_V5_SETUP) || defined(PLAT_V6_SETUP) || defined(PLAT_V7_SETUP)
    #define ACC_INT_GROUP          PIN_GROUP_1
    #define ACC_INT_PINID          9
#endif
#define ACC_INT_MASK           (1UL << ACC_INT_PINID)
#if (ACC_INT_PINID % 2) == 1
    #define ACC_INT_PMUXREGID  PMUXO
#else
    #define ACC_INT_PMUXREGID  PMUXE
#endif

/* SMARTCARD */
#if defined(PLAT_V1_SETUP) || defined(PLAT_V2_SETUP)
    #define SMC_MOSI_GROUP         PIN_GROUP_1
    #define SMC_MOSI_PINID         2
#elif defined(PLAT_V3_SETUP) || defined(PLAT_V4_SETUP) || defined(PLAT_V5_SETUP) || defined(PLAT_V6_SETUP) || defined(PLAT_V7_SETUP)
    #define SMC_MOSI_GROUP         PIN_GROUP_0
    #define SMC_MOSI_PINID         11
#endif
#define SMC_MOSI_MASK          (1UL << SMC_MOSI_PINID)
#define SMC_MOSI_PMUX_ID       PORT_PMUX_PMUXE_D_Val
#if (SMC_MOSI_PINID % 2) == 1
    #define SMC_MOSI_PMUXREGID PMUXO
#else
    #define SMC_MOSI_PMUXREGID PMUXE
#endif

#if defined(PLAT_V1_SETUP) || defined(PLAT_V2_SETUP)
    #define SMC_MISO_GROUP         PIN_GROUP_1
    #define SMC_MISO_PINID         3
#elif defined(PLAT_V3_SETUP) || defined(PLAT_V4_SETUP) || defined(PLAT_V5_SETUP) || defined(PLAT_V6_SETUP) || defined(PLAT_V7_SETUP)
    #define SMC_MISO_GROUP         PIN_GROUP_0
    #define SMC_MISO_PINID         10
#endif
#define SMC_MISO_MASK          (1UL << SMC_MISO_PINID)
#define SMC_MISO_PMUX_ID       PORT_PMUX_PMUXE_D_Val
#if (SMC_MISO_PINID % 2) == 1
    #define SMC_MISO_PMUXREGID PMUXO
#else
    #define SMC_MISO_PMUXREGID PMUXE
#endif

#if defined(PLAT_V1_SETUP) || defined(PLAT_V2_SETUP)
    #define SMC_SCK_GROUP          PIN_GROUP_0
    #define SMC_SCK_PINID          21
    #define SMC_SCK_PMUX_ID        PORT_PMUX_PMUXO_C_Val
#elif defined(PLAT_V3_SETUP) || defined(PLAT_V4_SETUP) || defined(PLAT_V5_SETUP) || defined(PLAT_V6_SETUP) || defined(PLAT_V7_SETUP)
    #define SMC_SCK_GROUP          PIN_GROUP_0
    #define SMC_SCK_PINID          9
    #define SMC_SCK_PMUX_ID        PORT_PMUX_PMUXO_D_Val
#endif
#define SMC_SCK_MASK           (1UL << SMC_SCK_PINID)
#if (SMC_SCK_PINID % 2) == 1
    #define SMC_SCK_PMUXREGID  PMUXO
#else
    #define SMC_SCK_PMUXREGID  PMUXE
#endif

#if defined(PLAT_V1_SETUP) || defined(PLAT_V2_SETUP)
    #define SMC_RST_GROUP          PIN_GROUP_0
    #define SMC_RST_PINID          14
#elif defined(PLAT_V3_SETUP) || defined(PLAT_V4_SETUP) || defined(PLAT_V5_SETUP) || defined(PLAT_V6_SETUP) || defined(PLAT_V7_SETUP)
    #define SMC_RST_GROUP          PIN_GROUP_0
    #define SMC_RST_PINID          24
#endif
#define SMC_RST_MASK           (1UL << SMC_RST_PINID)

#if defined(PLAT_V1_SETUP) || defined(PLAT_V2_SETUP)
    #define SMC_PGM_GROUP          PIN_GROUP_0
    #define SMC_PGM_PINID          31
#elif defined(PLAT_V3_SETUP) || defined(PLAT_V4_SETUP) || defined(PLAT_V5_SETUP)
    #define SMC_PGM_GROUP          PIN_GROUP_1
    #define SMC_PGM_PINID          8
#elif defined(PLAT_V6_SETUP) || defined(PLAT_V7_SETUP)
    #define SMC_PGM_GROUP          PIN_GROUP_0
    #define SMC_PGM_PINID          8
#endif
#define SMC_PGM_MASK           (1UL << SMC_PGM_PINID)

#if defined(PLAT_V1_SETUP) || defined(PLAT_V2_SETUP)
    #define SMC_DET_GROUP          PIN_GROUP_0
    #define SMC_DET_PINID          12
#elif defined(PLAT_V3_SETUP) || defined(PLAT_V4_SETUP) || defined(PLAT_V5_SETUP) || defined(PLAT_V6_SETUP) || defined(PLAT_V7_SETUP)
    #define SMC_DET_GROUP          PIN_GROUP_0
    #define SMC_DET_PINID          3
#endif
#define SMC_DET_MASK           (1UL << SMC_DET_PINID)
#if (SMC_DET_PINID % 2) == 1
    #define SMC_DET_PMUXREGID  PMUXO
#else
    #define SMC_DET_PMUXREGID  PMUXE
#endif

/* AUX MCU COMMS */
#if defined(PLAT_V1_SETUP) || defined(PLAT_V2_SETUP)
    #define AUX_MCU_TX_GROUP       PIN_GROUP_1
    #define AUX_MCU_TX_PINID       11
#elif defined(PLAT_V3_SETUP) || defined(PLAT_V4_SETUP) || defined(PLAT_V5_SETUP) || defined(PLAT_V6_SETUP) || defined(PLAT_V7_SETUP)
    #define AUX_MCU_TX_GROUP       PIN_GROUP_1
    #define AUX_MCU_TX_PINID       23
#endif
#define AUX_MCU_TX_MASK        (1UL << AUX_MCU_TX_PINID)
#define AUX_MCU_TX_PMUX_ID     PORT_PMUX_PMUXO_D_Val
#if (AUX_MCU_TX_PINID % 2) == 1
    #define AUX_MCU_TX_PMUXREGID  PMUXO
#else
    #define AUX_MCU_TX_PMUXREGID  PMUXE
#endif

#if defined(PLAT_V1_SETUP) || defined(PLAT_V2_SETUP)
    #define AUX_MCU_RX_GROUP       PIN_GROUP_1
    #define AUX_MCU_RX_PINID       10
#elif defined(PLAT_V3_SETUP) || defined(PLAT_V4_SETUP) || defined(PLAT_V5_SETUP) || defined(PLAT_V6_SETUP) || defined(PLAT_V7_SETUP)
    #define AUX_MCU_RX_GROUP       PIN_GROUP_1
    #define AUX_MCU_RX_PINID       22
#endif
#define AUX_MCU_RX_MASK        (1UL << AUX_MCU_RX_PINID)
#define AUX_MCU_RX_PMUX_ID     PORT_PMUX_PMUXO_D_Val
#if (AUX_MCU_RX_PINID % 2) == 1
    #define AUX_MCU_RX_PMUXREGID  PMUXO
#else
    #define AUX_MCU_RX_PMUXREGID  PMUXE
#endif

#if defined(PLAT_V3_SETUP) || defined(PLAT_V4_SETUP) || defined(PLAT_V5_SETUP) || defined(PLAT_V6_SETUP) || defined(PLAT_V7_SETUP)
    #define AUX_MCU_NOCOMMS_GROUP   PIN_GROUP_0
    #define AUX_MCU_NOCOMMS_PINID   30
    #define AUX_MCU_NOCOMMS_MASK    (1UL << AUX_MCU_NOCOMMS_PINID)
    #if (AUX_MCU_NOCOMMS_PINID % 2) == 1
        #define AUX_MCU_NOCOMMS_PMUXREGID  PMUXO
    #else
        #define AUX_MCU_NOCOMMS_PMUXREGID  PMUXE
    #endif
#endif

#if defined(EMULATOR_BUILD)
    #undef FLASH_DMA_FETCHES
    #undef OLED_DMA_TRANSFER
#endif

#endif /* PLATFORM_DEFINES_H_ */
