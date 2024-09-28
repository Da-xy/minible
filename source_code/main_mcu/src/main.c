#include <string.h>
#include <asf.h>
#include "mooltipass_graphics_bundle.h"
#include "se_smartcard_wrapper.h"
#include "logic_accelerometer.h"
#include "functional_testing.h"
#include "smartcard_lowlevel.h"
#include "platform_defines.h"
#include "logic_encryption.h"
#include "logic_smartcard.h"
#include "logic_bluetooth.h"
#include "gui_dispatcher.h"
#include "logic_security.h"
#include "logic_aux_mcu.h"
#include "driver_clocks.h"
#include "comms_aux_mcu.h"
#include "oled_wrapper.h"
#include "driver_timer.h"
#include "logic_device.h"
#include "gui_prompts.h"
#include "logic_power.h"
#include "platform_io.h"
#include "acc_wrapper.h"
#include "logic_user.h"
#include "custom_fs.h"
#include "dataflash.h"
#include "logic_gui.h"
#include "text_ids.h"
#include "nodemgmt.h"
#include "dbflash.h"
#include "inputs.h"
#include "utils.h"
#include "fuses.h"
#include "debug.h"
#include "main.h"
#include "rng.h"
#include "dma.h"

/* Our oled & dataflash & dbflash descriptors */
accelerometer_descriptor_t plat_acc_descriptor = {.sercom_pt = ACC_SERCOM, .cs_pin_group = ACC_nCS_GROUP, .cs_pin_mask = ACC_nCS_MASK, .int_pin_group = ACC_INT_GROUP, .int_pin_mask = ACC_INT_MASK, .evgen_sel = ACC_EV_GEN_SEL, .evgen_channel = ACC_EV_GEN_CHANNEL, .dma_channel = 3};
oled_descriptor_t plat_oled_descriptor = {.sercom_pt = OLED_SERCOM, .dma_trigger_id = OLED_DMA_SERCOM_TX_TRIG, .cs_pin_group = OLED_nCS_GROUP, .cs_pin_mask = OLED_nCS_MASK, .cd_pin_group = OLED_CD_GROUP, .cd_pin_mask = OLED_CD_MASK};
spi_flash_descriptor_t dataflash_descriptor = {.sercom_pt = DATAFLASH_SERCOM, .cs_pin_group = DATAFLASH_nCS_GROUP, .cs_pin_mask = DATAFLASH_nCS_MASK};
spi_flash_descriptor_t dbflash_descriptor = {.sercom_pt = DBFLASH_SERCOM, .cs_pin_group = DBFLASH_nCS_GROUP, .cs_pin_mask = DBFLASH_nCS_MASK};
/* A wheel action that may be used to pass to our GUI routine */
wheel_action_ret_te virtual_wheel_action = WHEEL_ACTION_NONE;
/* If we should power off asap */
BOOL main_should_power_off_asap = FALSE;
/* Flag when ADC watchdog fired */
BOOL main_adc_watchdog_fired = FALSE;
/* Flag when accelerometer watchdog fired */
BOOL main_acc_watchdog_fired = FALSE;
/* Know if debugger is present */
BOOL debugger_present = FALSE;
#ifndef EMULATOR_BUILD
/* Start of stack as defined by linker */
extern uint32_t _estack;
/* End of stack as defined by linker */
extern uint32_t _sstack;
#endif

/* Used to know if there is no bootloader and if the special card is inserted */
#ifdef DEVELOPER_FEATURES_ENABLED
BOOL special_dev_card_inserted = FALSE;
uint32_t* mcu_sp_rh_addresses = 0;
#endif

/*! \fn     main_create_virtual_wheel_movement(void)
*   \brief  Create virtual wheel movement
*/
void main_create_virtual_wheel_movement(void)
{
    virtual_wheel_action = WHEEL_ACTION_VIRTUAL;
}

/*! \fn     main_platform_init(void)
*   \brief  Initialize our platform
*/
void main_platform_init(void)
{
    /* Initialization results vars */
    custom_fs_init_ret_type_te custom_fs_return = CUSTOM_FS_INIT_NO_RWEE;
    RET_TYPE bundle_integrity_check_return = RETURN_NOK;
    RET_TYPE custom_fs_init_return = RETURN_NOK;
    RET_TYPE dataflash_init_return = RETURN_NOK;
    BOOL low_battery_at_boot = FALSE;
    RET_TYPE fuses_ok = RETURN_NOK;
    
    /* Low level port initializations for power supplies */
    platform_io_keep_power_on();                                            // Keep the power on
    platform_io_init_power_ports();                                         // Init power port, needed to test if we are battery or usb powered
    platform_io_init_no_comms_signal();                                     // Init no comms signal, used later as wakeup for the aux MCU

    /* Check if fuses are correctly programmed (required to read flags), if so initialize our flag system, then finally check if we previously powered off due to low battery and still haven't charged since then */
    if ((fuses_check_program(FALSE) == RETURN_OK) && (custom_fs_settings_init() == CUSTOM_FS_INIT_OK) && (custom_fs_get_device_flag_value(PWR_OFF_DUE_TO_BATTERY_FLG_ID) != FALSE))
    {
        /* Check if USB 3V3 is present and if so clear flag */
        if (platform_io_is_usb_3v3_present_raw() == FALSE)
        {
            platform_io_cutoff_power();
            while(1);
        }
        else
        {
            low_battery_at_boot = TRUE;
            custom_fs_set_device_flag_value(PWR_OFF_DUE_TO_BATTERY_FLG_ID, FALSE);
        }
    }
    
    /* Measure battery voltage */
    platform_io_init_bat_adc_measurements();                                // Initialize ADC measurements
    platform_io_enable_vbat_to_oled_stepup();                               // Enable vbat to oled stepup
    platform_io_get_vbat_conversion_result_and_trigger_conversion();        // Start one measurement
    while(platform_io_is_vbat_conversion_result_ready() == FALSE);          // Do measurement even if we are USB powered, to leave exactly 180ms for platform boot

    /* Check if battery powered and under-voltage */
    uint32_t battery_voltage = platform_io_get_vbat_conversion_result_and_trigger_conversion();
    if ((platform_io_is_usb_3v3_present_raw() == FALSE) && (battery_voltage < BATTERY_ADC_OUT_CUTOUT))
    {
        platform_io_cutoff_power();
        while(1);
    }
    
    /* If we're USB powered and measured voltage is too low, flag it (device switched off for months...) */
    if (platform_io_is_usb_3v3_present_raw() != FALSE)
    {
        /* Real ratio is 3300 / 3188 */
        battery_voltage = (battery_voltage*265) >> 8;
        
        /* Check for low voltage */
        if (battery_voltage < BATTERY_ADC_OUT_CUTOUT)
        {
            low_battery_at_boot = TRUE;
        }
    }
    
    /* Check fuses, depending on platform program them if incorrectly set */
    #if IS_V1_PLAT_IN_RANGE_1_TO_6 || defined(V2_PLAT_V1_SETUP)
    fuses_ok = fuses_check_program(TRUE);
    #else
    fuses_ok = fuses_check_program(FALSE);
    #endif
    while(fuses_ok != RETURN_OK);
    
#ifndef EMULATOR_BUILD
    /* Check if debugger present */
    if (DSU->STATUSB.bit.DBGPRES != 0)
    {
        debugger_present = TRUE;
        
        /* Debugger connected but we are not on a dev platform? */
        #ifndef NO_SECURITY_BIT_CHECK
        while(1);
        #endif
    }
    
    /* Switch to 48MHz */
    clocks_start_48MDFLL();
#endif
    
    /* Second custom FS init (as fuses may have been programmed since the first), check for data flash, absence of bundle and bundle integrity */
    platform_io_init_flash_ports();
    custom_fs_return = custom_fs_settings_init();
    custom_fs_set_dataflash_descriptor(&dataflash_descriptor);
    dataflash_init_return = dataflash_check_presence(&dataflash_descriptor);
    if (dataflash_init_return == RETURN_OK)
    {
        custom_fs_init_return = custom_fs_init();
        if (custom_fs_init_return == RETURN_OK)
        {
            /* Bundle integrity check */
            bundle_integrity_check_return = custom_fs_compute_and_check_external_bundle_crc32();
        }
    }
    
    /* DMA transfers inits, timebase, platform ios, enable comms */
    dma_init();
    logic_power_init(low_battery_at_boot);
    timer_initialize_timebase();
    platform_io_init_ports();
    comms_aux_arm_rx_and_clear_no_comms();
    platform_io_init_bat_adc_measurements();
    logic_device_set_wakeup_reason(WAKEUP_REASON_BOOT);
    
    /* Initialize OLED screen */
    if (platform_io_is_usb_3v3_present_raw() == FALSE)
    {
        logic_power_set_power_source(BATTERY_POWERED);
        platform_io_power_up_oled(FALSE);
    } 
    else
    {
        platform_io_bypass_3v3_detection_debounce();
        logic_power_set_power_source(USB_POWERED);
        platform_io_power_up_oled(TRUE);
    }
    oled_init_display(&plat_oled_descriptor, FALSE, logic_device_get_screen_current_for_current_use());
    
    /* Release aux MCU reset (v2 platform only) */
    platform_io_release_aux_reset();

    /* Check initialization results */
    if (custom_fs_return == CUSTOM_FS_INIT_NO_RWEE)
    {
        oled_put_error_string(&plat_oled_descriptor, u"No RWWE");
        while(1);        
    }    
    
    /* Check for data flash */
    if (dataflash_init_return != RETURN_OK)
    {
        oled_put_error_string(&plat_oled_descriptor, u"No Dataflash");
        while(1);
    }
    
    /* Check for DB flash */
    if (dbflash_check_presence(&dbflash_descriptor) != RETURN_OK)
    {
        oled_put_error_string(&plat_oled_descriptor, u"No DB Flash");
        while(1);
    }
    
    /* Check for accelerometer presence */
    if (acc_check_presence_and_configure(&plat_acc_descriptor) != RETURN_OK)
    {
        oled_put_error_string(&plat_oled_descriptor, u"No Accelerometer");
        while(1);
    }

#ifndef EMULATOR_BUILD    
    /* Is Aux MCU present? */
    if (comms_aux_mcu_send_receive_ping() != RETURN_OK)
    {
        /* Try to reset our comms link */
        dma_aux_mcu_disable_transfer();
        comms_aux_arm_rx_and_clear_no_comms();
        
        /* Try again */
        if (comms_aux_mcu_send_receive_ping() != RETURN_OK)
        {
            /* Use hard comms reset procedure */
            comms_aux_mcu_hard_comms_reset_with_aux_mcu_reboot();
            
            BOOL prev_usb_present_state = platform_io_is_usb_3v3_present_raw();
            if (comms_aux_mcu_send_receive_ping() != RETURN_OK)
            {
                oled_put_error_string(&plat_oled_descriptor, u"No Aux MCU");
                uint16_t battery_rescue_mode_counter = 0;
                while(1)
                {
                    /* Switch off when disconnected form USB */
                    if ((prev_usb_present_state != FALSE) && (platform_io_is_usb_3v3_present_raw() == FALSE))
                    {
                        logic_device_power_off();
                        while(1);
                    }
                    
                    /* Battery rescue mode */
                    if (inputs_get_wheel_action(FALSE, FALSE) == WHEEL_ACTION_CLICK_UP)
                    {
                        /* Dangerously directly pipe the PWMed LDO 3V3 output into the battery, averaging at 40mA (measured) */
                        if ((battery_rescue_mode_counter++ == 5) && (prev_usb_present_state != FALSE))
                        {
                            oled_clear_current_screen(&plat_oled_descriptor);
                            oled_put_error_string(&plat_oled_descriptor, u"Recovery in progress");
                            /* PWM for 2 hours, doesn't get uglier than this */
                            for (uint32_t i = 0; i < 36000000UL; i++)
                            {
                                platform_io_enable_vbat_to_oled_stepup();
                                DELAYUS(100);
                                platform_io_disable_vbat_to_oled_stepup();
                                DELAYUS(100);
                            }
                            oled_clear_current_screen(&plat_oled_descriptor);
                            oled_put_error_string(&plat_oled_descriptor, u"Disconnect USB");
                        }
                    }
                }
            }                
        }
    }
#endif
    
    /* If debugger attached, let the aux mcu know it shouldn't use the no comms signal */
    if (debugger_present != FALSE)
    {
        comms_aux_mcu_send_simple_command_message(MAIN_MCU_COMMAND_NO_COMMS_UNAV);
        comms_aux_mcu_wait_for_aux_event(AUX_MCU_EVENT_NO_COMMS_INFO_RCVD);
    }    

    /* If USB present, send USB attach message */
    if (platform_io_is_usb_3v3_present_raw() != FALSE)
    {
        comms_aux_mcu_send_simple_command_message(MAIN_MCU_COMMAND_ATTACH_USB);
        comms_aux_mcu_wait_for_aux_event(AUX_MCU_EVENT_ATTACH_CMD_RCVD);
        logic_power_usb_enumerate_just_sent();
    }
    
#ifndef EMULATOR_BUILD
    /* Check for non-RF functional testing passed */
    #ifdef DEVELOPER_FEATURES_ENABLED
    if ((custom_fs_get_device_flag_value(FUNCTIONAL_TEST_PASSED_FLAG_ID) == FALSE) && (mcu_sp_rh_addresses[1] != 0x0201))
    #else
    if (custom_fs_get_device_flag_value(FUNCTIONAL_TEST_PASSED_FLAG_ID) == FALSE)
    #endif
    {
        /* First boot initializations: set auth counter value to 270 due to possible (non-security critical) hiccups on first mass production batch */
        custom_fs_set_auth_challenge_counter(270);
        timer_delay_ms(1);
        custom_fs_set_undefined_settings(TRUE);
        
        /* Then functional testing */
        functional_testing_start(TRUE);
    }

    /* Check for RF functional testing passed */
    #ifdef DEVELOPER_FEATURES_ENABLED
    if ((custom_fs_get_device_flag_value(RF_TESTING_PASSED_FLAG_ID) == FALSE) && (mcu_sp_rh_addresses[1] != 0x0201))
    #else
    if (custom_fs_get_device_flag_value(RF_TESTING_PASSED_FLAG_ID) == FALSE)
    #endif
    {
        /* Start continuous tone, wait for test to press long click or timeout to die */
        functional_rf_testing_start();
        uint16_t temp_timer_id = timer_get_and_start_timer(50000);
        while (inputs_get_wheel_action(FALSE, TRUE) != WHEEL_ACTION_LONG_CLICK)
        {
            /* Timer timeout, switch off platform */
            if ((timer_has_allocated_timer_expired(temp_timer_id, FALSE) == TIMER_EXPIRED) && (platform_io_is_usb_3v3_present() == FALSE))
            {
                /* Switch off platform */
                logic_device_power_off();
            }
            
            /* Handle possible power switches */
            logic_power_check_power_switch_and_battery(FALSE);
            
            /* Comms functions */
            comms_aux_mcu_routine(MSG_RESTRICT_ALL);
            
            /* In the past we had some units passing func test but having a failed ACC later on */
            if (logic_accelerometer_routine() == ACC_FAILING)
            {
                oled_clear_current_screen(&plat_oled_descriptor);
                oled_put_error_string(&plat_oled_descriptor, u"LIS2HH12 failed!");
                timer_delay_ms(20000);
            }
            
            /* Our assembler may solder the battery after the functional test */
            if (logic_power_get_battery_state() == BATTERY_ERROR)
            {
                oled_clear_current_screen(&plat_oled_descriptor);
                oled_put_error_string(&plat_oled_descriptor, u"Battery error!");
                timer_delay_ms(20000);                
            }
        }
        timer_deallocate_timer(temp_timer_id);
        custom_fs_set_device_flag_value(RF_TESTING_PASSED_FLAG_ID, TRUE);
        comms_aux_mcu_send_simple_command_message(MAIN_MCU_COMMAND_DTM_STOP);
        comms_aux_mcu_wait_for_aux_event(AUX_MCU_EVENT_TX_SWEEP_DONE);
        oled_clear_current_screen(&plat_oled_descriptor);
        logic_gui_disable_bluetooth(FALSE);
        #ifdef OLED_INTERNAL_FRAME_BUFFER
        oled_clear_frame_buffer(&plat_oled_descriptor);
        #endif
    }
#endif
    
    /* Display error messages if something went wrong during custom fs init and bundle check */
    #if defined(PLAT_V7_SETUP)
    if ((custom_fs_init_return != RETURN_OK) || (bundle_integrity_check_return != RETURN_OK) || ((custom_fs_get_device_flag_value(DEVICE_WENT_THROUGH_BOOTLOADER_FLAG_ID) != FALSE) && (custom_fs_get_device_flag_value(SUCCESSFUL_UPDATE_FLAG_ID) == FALSE)))
    #else
    if ((custom_fs_init_return != RETURN_OK) || (bundle_integrity_check_return != RETURN_OK))
    #endif
    {
        oled_put_error_string(&plat_oled_descriptor, u"No Bundle");
        uint16_t temp_timer_id = timer_get_and_start_timer(30000);
        
        /* Inform AUX MCU that we're in a pickle */
        comms_aux_mcu_update_device_status_buffer();
        
        /* Wait to load bundle from USB */
        while(1)
        {
            /* Communication function */
            comms_msg_rcvd_te msg_received = comms_aux_mcu_routine(MSG_RESTRICT_ALLBUT_BUNDLE);
            
            /* If we received any message, reset timer */
            if (msg_received != NO_MSG_RCVD)
            {
                timer_rearm_allocated_timer(temp_timer_id, 30000);
            }            
            
            /* Check for reindex bundle message */
            #ifdef DEBUG_USB_COMMANDS_ENABLED
            if (msg_received == HID_REINDEX_BUNDLE_RCVD)
            {
                /* Try to init our file system */
                custom_fs_init_return = custom_fs_init();
                if (custom_fs_init_return == RETURN_OK)
                {
                    break;
                }
            }
            #endif
            
            /* Handle possible power switches */
            logic_power_check_power_switch_and_battery(FALSE);
            
            /* Timer timeout, switch off platform */
            if ((timer_has_allocated_timer_expired(temp_timer_id, FALSE) == TIMER_EXPIRED) && (platform_io_is_usb_3v3_present() == FALSE))
            {
                /* Switch off platform */
                logic_device_power_off();
            }
        }
        
        /* Free timer */
        timer_deallocate_timer(temp_timer_id);
    }
    
    /* Set settings that may not have been set to an initial value (after going into the bootloader for example) */
    custom_fs_set_undefined_settings(FALSE);
    
    /* Apply possible screen inversion */
    BOOL screen_inverted = logic_power_get_power_source() == BATTERY_POWERED?(BOOL)custom_fs_settings_get_device_setting(SETTINGS_LEFT_HANDED_ON_BATTERY):(BOOL)custom_fs_settings_get_device_setting(SETTINGS_LEFT_HANDED_ON_USB);
    oled_set_screen_invert(&plat_oled_descriptor, screen_inverted);    
    inputs_set_inputs_invert_bool(screen_inverted);
    
    /* Program AUX if needed */
    if (custom_fs_get_device_flag_value(DEVICE_WENT_THROUGH_BOOTLOADER_FLAG_ID) != FALSE)
    {
        #if defined(PLAT_V7_SETUP)
        if (custom_fs_get_device_flag_value(SUCCESSFUL_UPDATE_FLAG_ID) != FALSE)
        {
            /* Detach from USB to get a free bus */
            comms_aux_mcu_send_simple_command_message(MAIN_MCU_COMMAND_DETACH_USB);
            comms_aux_mcu_wait_for_aux_event(AUX_MCU_EVENT_USB_DETACHED);
            
            /* Disable bluetooth if enabled */
            logic_aux_mcu_disable_ble(TRUE);
            
            /* Flash AUX */
            logic_aux_mcu_flash_firmware_update(TRUE);
        }
        
        /* Issue for some devices that had bundle < 4 & 7: some of them wouldn't have their auth counter set to 0 */
        if (custom_fs_get_platform_bundle_version() == 4)
        {
            custom_fs_set_auth_challenge_counter(100);
        }
        else if (custom_fs_get_platform_bundle_version() == 7)
        {
            custom_fs_set_auth_challenge_counter(200);
        }
        else if (custom_fs_get_platform_bundle_version() == 9)
        {
            custom_fs_set_auth_challenge_counter(250);
        }
        else if (custom_fs_get_platform_bundle_version() == 10)
        {
            custom_fs_set_auth_challenge_counter(256);
        }
        else if (custom_fs_get_platform_bundle_version() == 11)
        {
            custom_fs_set_auth_challenge_counter(265);
        }
        else if (custom_fs_get_platform_bundle_version() == 12)
        {
            custom_fs_set_auth_challenge_counter(270);
        }
        else if (custom_fs_get_platform_bundle_version() == 13)
        {
            custom_fs_set_auth_challenge_counter(275);
        }
        #endif
    }

    /* Actions for first user device boot */
    #ifdef DEVELOPER_FEATURES_ENABLED
    if (((custom_fs_get_device_flag_value(NOT_FIRST_BOOT_FLAG_ID) == FALSE) || (custom_fs_get_device_flag_value(DEVICE_WENT_THROUGH_BOOTLOADER_FLAG_ID) != FALSE)) && (mcu_sp_rh_addresses[1] != 0x0201))
    #else
    if ((custom_fs_get_device_flag_value(NOT_FIRST_BOOT_FLAG_ID) == FALSE) || (custom_fs_get_device_flag_value(DEVICE_WENT_THROUGH_BOOTLOADER_FLAG_ID) != FALSE))
    #endif
    {
        /* Select language and store it as default */
        if (gui_prompts_select_language_or_keyboard_layout(FALSE, TRUE, TRUE, FALSE) != RETURN_OK)
        {
            /* We're battery powered, the user didn't select anything, switch off device */
            logic_device_power_off();
            while(1);            
        }
        
        /* Store set language as device default one */
        custom_fs_set_device_default_language(custom_fs_get_current_language_id());
        
        /* Set flag if needed */
        if (custom_fs_get_device_flag_value(NOT_FIRST_BOOT_FLAG_ID) == FALSE)
        {
            custom_fs_set_device_flag_value(NOT_FIRST_BOOT_FLAG_ID, TRUE);
        }
        
        /* Clear frame buffer */
        oled_fade_into_darkness(&plat_oled_descriptor, OLED_IN_OUT_TRANS);
        
        /* Reset settings (leave disabled, no real need) */
        //custom_fs_set_undefined_settings(TRUE);
        //custom_fs_set_auth_challenge_counter(100);
    }
    
    /* Special developer features */
    #ifdef SPECIAL_DEVELOPER_CARD_FEATURE
    /* Check if this is running on a device without bootloader, add CPZ entry for special card */
    if (mcu_sp_rh_addresses[1] == 0x0201)
    {
        /* Special card has 0000 CPZ, set 0000 as nonce */
        //dbflash_format_flash(&dbflash_descriptor);
        //nodemgmt_format_user_profile(100, 0, 0);
        cpz_lut_entry_t special_user_profile;
        memset(&special_user_profile, 0, sizeof(special_user_profile));
        special_user_profile.user_id = 100;
                
        /* When developping on a newly flashed board: reset USB connection and reset defaults */
        if (custom_fs_store_cpz_entry(&special_user_profile, special_user_profile.user_id) == RETURN_OK)
        {
            if ((platform_io_is_usb_3v3_present_raw() != FALSE) && TRUE)
            {
                comms_aux_mcu_send_simple_command_message(MAIN_MCU_COMMAND_DETACH_USB);
                comms_aux_mcu_wait_for_aux_event(AUX_MCU_EVENT_USB_DETACHED);
                timer_delay_ms(2000);
                comms_aux_mcu_send_simple_command_message(MAIN_MCU_COMMAND_ATTACH_USB);
                comms_aux_mcu_wait_for_aux_event(AUX_MCU_EVENT_ATTACH_CMD_RCVD);
                logic_power_usb_enumerate_just_sent();
            }
        }   
        
        /* Disable tutorial */
        custom_fs_set_settings_value(SETTINGS_DEVICE_TUTORIAL, FALSE);     
    }
    #endif
    
    /* Clear went through bootloader flag */
    if (custom_fs_get_device_flag_value(DEVICE_WENT_THROUGH_BOOTLOADER_FLAG_ID) != FALSE)
    {
        custom_fs_set_device_flag_value(DEVICE_WENT_THROUGH_BOOTLOADER_FLAG_ID, FALSE);
    }

    /* Now that the platform is up and ready to go, our consumption log & calib data fetched, we can clear our consumption log data to enable faster storage at platform switch-off */
    custom_fs_clear_power_consumption_log_and_calib_data();
    
    /* Arm aux MCU ping timer */
    timer_start_timer(TIMER_AUX_MCU_PING, NB_MS_AUX_MCU_PING);
}

/*! \fn     main_reboot(void)
*   \brief  Reboot
*/
void main_reboot(void)
{
#ifndef EMULATOR_BUILD
    /* Power down actions */
    logic_power_power_down_actions();
    
    /* Wait for accelerometer DMA transfer end */
    acc_check_data_received_flag_and_arm_other_transfer(&plat_acc_descriptor, TRUE);
    while (dma_acc_check_and_clear_dma_transfer_flag() == FALSE);
    
    /* Wait for end of message we were possibly sending */
    comms_aux_mcu_wait_for_message_sent();
    
    /* Power Off OLED screen */
    oled_off(&plat_oled_descriptor);
    platform_io_power_down_oled();
    
    /* No comms */
    platform_io_set_no_comms();
    
    /* Wait and reboot */
    timer_delay_ms(100);
    cpu_irq_disable();
    NVIC_SystemReset();
    while(1);
#else
    exit(0);
#endif
}

/*! \fn     main_standby_sleep(void)
*   \brief  Go to sleep
*/
void main_standby_sleep(void)
{
#ifndef EMULATOR_BUILD
    if (debugger_present == FALSE)
    {
        BOOL ports_set_for_sleep = FALSE;
        aux_mcu_message_t* temp_rx_message;
        BOOL comms_disabled_on_entry = TRUE;
    
        /* Only if we actually wokeup the aux mcu */
        if (comms_aux_mcu_are_comms_disabled() == FALSE)
        {
            /* Set boolean */
            comms_disabled_on_entry = FALSE;
            
            /* Send a go to sleep message to aux MCU, wait for ack, leave no comms high (automatically set when receiving the sleep received event) */
            comms_aux_mcu_send_simple_command_message(MAIN_MCU_COMMAND_SLEEP);
            while(comms_aux_mcu_active_wait(&temp_rx_message, AUX_MCU_MSG_TYPE_AUX_MCU_EVENT, FALSE, AUX_MCU_EVENT_SLEEP_RECEIVED) != RETURN_OK);
            
            /* Wait for end of message we were possibly sending */
            comms_aux_mcu_wait_for_message_sent();
            
            /* Disable aux MCU dma transfers */
            dma_aux_mcu_disable_transfer();
        }
    
        /* Wait for accelerometer DMA transfer end and put it to sleep */
        acc_check_data_received_flag_and_arm_other_transfer(&plat_acc_descriptor, TRUE);
        while (dma_acc_check_and_clear_dma_transfer_flag() == FALSE);
        acc_deassert_ncs_and_go_to_sleep(&plat_acc_descriptor);
    
        /* DB & Dataflash power down */
        dbflash_enter_ultra_deep_power_down(&dbflash_descriptor);
        dataflash_power_down(&dataflash_descriptor);
    
        /* Switch off OLED */
        oled_off(&plat_oled_descriptor);
        platform_io_power_down_oled();
    
        /* Errata 10416: disable interrupt routines */
        cpu_irq_enter_critical();
        
        /* Double check that we don't have an AUX MCU trying to talk to us (ex: periodic wake up that matches with aux mcu wakeup) */
        if ((logic_device_get_aux_wakeup_rcvd() == FALSE) || (comms_disabled_on_entry == FALSE))
        {
            /* Prepare the ports for sleep */
            platform_io_prepare_ports_for_sleep();
            ports_set_for_sleep = TRUE;
    
            /* Clear wakeup reason */
            logic_device_clear_wakeup_reason();
        
            /* Specify that comms are disabled */
            comms_aux_mcu_set_comms_disabled();
            
            /* Enter deep sleep */
            SCB->SCR = SCB_SCR_SLEEPDEEP_Msk;
            __DSB();
            __WFI();
        }
        else
        {
            logic_device_clear_wakeup_reason();
            logic_device_clear_aux_wakeup_rcvd();
            logic_device_set_wakeup_reason(WAKEUP_REASON_AUX_MCU);
        }   
    
        /* Damn errata... enable interrupts and give time to kick in */
        cpu_irq_leave_critical(); 
        DELAYUS(10);
    
        /* Prepare ports for sleep exit */
        if (ports_set_for_sleep != FALSE)
        {
            platform_io_prepare_ports_for_sleep_exit();
        }
    
        /* Dataflash power up */
        dataflash_exit_power_down(&dataflash_descriptor);
    
        /* Send any command to DB flash to wake it up (required in case of going to sleep twice) */
        dbflash_check_presence(&dbflash_descriptor);
        
        /* Resume accelerometer processing */
        acc_sleep_exit_and_dma_arm(&plat_acc_descriptor);
    
        /* Get wakeup reason */
        volatile platform_wakeup_reason_te wakeup_reason = logic_device_get_wakeup_reason();
        
        /* Re-enable AUX comms */
        if (wakeup_reason != WAKEUP_REASON_30M_TIMER)
        {
            platform_io_disable_no_comms_as_wakeup_interrupt();
            comms_aux_arm_rx_and_clear_no_comms();
        }
    
        /* Switch on OLED depending on wakeup reason */
        if (wakeup_reason == WAKEUP_REASON_30M_TIMER)
        {
            /* Timer whose sole purpose is to periodically wakeup device to check battery level */
            platform_io_power_up_oled(platform_io_is_usb_3v3_present_raw());
        
            /* 100ms to measure battery's voltage, after enabling OLED stepup */
            timer_start_timer(TIMER_SCREEN, 100);
            
            /* As the platform won't be awake for long, skip the battery measurement queue logic */
            logic_power_skip_queue_logic_for_upcoming_adc_measurements();
        }
        else if (wakeup_reason == WAKEUP_REASON_AUX_MCU)
        {
            /* AUX MCU woke up our device, set a sleep timer to a low value so we can deal with the incoming packet */
            timer_start_timer(TIMER_SCREEN, SLEEP_AFTER_AUX_WAKEUP_MS);
        }
        else
        {
            /* User action: switch on OLED screen */
            platform_io_power_up_oled(platform_io_is_usb_3v3_present_raw());
            oled_on(&plat_oled_descriptor);
            logic_device_activity_detected();
        }
    
        /* Clear wheel detection */
        inputs_clear_detections();
    }
#endif
}

/*! \fn     main(void)
*   \brief  Program Main
*/
#ifdef EMULATOR_BUILD
int minible_main(void);
int minible_main(void)
#else
int main(void)
#endif
{
    /* Initialize stack usage tracking */
    main_init_stack_tracking();

    /* Initialize our platform */
    main_platform_init();
    
    /* Activity detected */
    logic_device_activity_detected();
    
    /* Start animation */    
    if ((BOOL)custom_fs_settings_get_device_setting(SETTINGS_BOOT_ANIMATION) != FALSE)
    {
        for (uint16_t i = GUI_ANIMATION_FFRAME_ID; i < GUI_ANIMATION_NBFRAMES; i++)
        {
            timer_start_timer(TIMER_ANIMATIONS, 28);
            oled_display_bitmap_from_flash_at_recommended_position(&plat_oled_descriptor, i, FALSE);
            while(timer_has_timer_expired(TIMER_ANIMATIONS, TRUE) == TIMER_RUNNING)
            {
                logic_power_check_power_switch_and_battery(FALSE);
                comms_aux_mcu_routine(MSG_RESTRICT_ALLBUT_SN);
                logic_accelerometer_routine();
                
                /* Click to exit animation */
                if (inputs_get_wheel_action(FALSE, FALSE) == WHEEL_ACTION_SHORT_CLICK)
                {
                    i = 0x1234;
                    break;
                }
            }
        }
    }  
    
    /* Do we need to display device tutorial? */
    if ((BOOL)custom_fs_settings_get_device_setting(SETTINGS_DEVICE_TUTORIAL) != FALSE)
    {        
        /* Display tutorial */
        gui_prompts_display_tutorial();
        
        /* Tutorial displayed, reset boolean */
        custom_fs_set_settings_value(SETTINGS_DEVICE_TUTORIAL, FALSE);
    }
    
    /* Get current smartcard detection result */
    det_ret_type_te card_detection_res = se_smartcard_is_se_plugged();
        
    /* Set startup screen */
    gui_dispatcher_set_current_screen(GUI_SCREEN_NINSERTED, TRUE, GUI_INTO_MENU_TRANSITION);
    logic_device_activity_detected();
    logic_device_set_state_changed();
    if (card_detection_res != RETURN_JDETECT)
    {
        gui_dispatcher_get_back_to_current_screen();
    }
    
    /* Infinite loop */
    while(TRUE)
    {
        /* Power routine */
        logic_power_routine();
        
        /* Should we power off ASAP? */
        if ((main_should_power_off_asap != FALSE) && (platform_io_is_usb_3v3_present_raw() == FALSE))
        {
            oled_off(&plat_oled_descriptor);
            platform_io_power_down_oled();
            timer_delay_ms(100);
            platform_io_cutoff_power();
        }
        
        /* Check if we should switch categories */
        uint16_t temp_category;
        if (logic_user_is_category_to_be_switched(&temp_category) != FALSE)
        {
            /* Set new category */
            nodemgmt_set_current_category_id(temp_category);
            
            /* Invalidate preferred starting login */
            logic_user_invalidate_preferred_starting_service();
        }
        
        /* Check flag to be logged off */
        if ((logic_user_get_and_clear_user_to_be_logged_off_flag() != FALSE) && \
            (gui_dispatcher_get_current_screen() != GUI_SCREEN_NINSERTED) && \
            (gui_dispatcher_get_current_screen() != GUI_SCREEN_INSERTED_LCK) && \
            (gui_dispatcher_get_current_screen() != GUI_SCREEN_INSERTED_INVALID) && \
            (gui_dispatcher_get_current_screen() != GUI_SCREEN_INSERTED_UNKNOWN) && \
            (gui_dispatcher_get_current_screen() != GUI_SCREEN_MEMORY_MGMT) && \
            (gui_dispatcher_get_current_screen() != GUI_SCREEN_FW_FILE_UPDATE))            
        {
            /* Switch off device? */
            if (((BOOL)custom_fs_settings_get_device_setting(SETTINGS_SWITCH_OFF_ON_LOCK) != FALSE) && (logic_power_get_power_source() != USB_POWERED))
            {
                logic_device_power_off();
            }

            /* Disable bluetooth? */
            if (custom_fs_settings_get_device_setting(SETTINGS_DISABLE_BLE_ON_LOCK) != FALSE)
            {
                logic_gui_disable_bluetooth(TRUE);
            }
            logic_device_activity_detected();
            gui_dispatcher_set_current_screen(GUI_SCREEN_INSERTED_LCK, TRUE, GUI_OUTOF_MENU_TRANSITION);
            if (gui_dispatcher_is_screen_saver_running() == FALSE)
            {
                gui_dispatcher_get_back_to_current_screen();
            }
            logic_device_set_state_changed();
            logic_smartcard_handle_removed();
            timer_delay_ms(250);
        }
        
        /* Check if we should leave management mode */
        if ((gui_dispatcher_get_current_screen() == GUI_SCREEN_MEMORY_MGMT) && (logic_security_should_leave_management_mode() != FALSE))
        {
            /* Device state is going to change... */
            logic_device_set_state_changed();

            /* Clear bool */
            logic_device_activity_detected();
            logic_security_clear_management_mode();

            /* Set next screen */
            gui_dispatcher_set_current_screen(GUI_SCREEN_MAIN_MENU, TRUE, GUI_INTO_MENU_TRANSITION);
            gui_dispatcher_get_back_to_current_screen();
            nodemgmt_scan_node_usage();
        }
        
        /* Do not do anything if we're uploading new graphics contents */
        if (gui_dispatcher_get_current_screen() != GUI_SCREEN_FW_FILE_UPDATE)
        {
            /* Comms problem */
            if (comms_aux_mcu_get_and_clear_invalid_message_received() != FALSE)
            {
                gui_prompts_display_information_on_screen_and_wait(CONTACT_SUPPORT_005_TEXT_ID, DISP_MSG_WARNING, FALSE);
                gui_dispatcher_get_back_to_current_screen();   
                #ifdef HALT_ON_00X_ERROR
                main_should_power_off_asap = TRUE;
                #endif             
            }
            
            /* RX DMA problem */
            if (comms_aux_mcu_get_and_clear_rx_transfer_already_armed() != FALSE)
            {
                gui_prompts_display_information_on_screen_and_wait(CONTACT_SUPPORT_006_TEXT_ID, DISP_MSG_WARNING, FALSE);
                gui_dispatcher_get_back_to_current_screen();
                #ifdef HALT_ON_00X_ERROR
                main_should_power_off_asap = TRUE;
                #endif
            }
            
            /* TX buffer re requested problem */
            if (comms_aux_mcu_get_and_clear_second_tx_buffer_rerequested() != FALSE)
            {
                gui_prompts_display_information_on_screen_and_wait(CONTACT_SUPPORT_007_TEXT_ID, DISP_MSG_WARNING, FALSE);
                gui_dispatcher_get_back_to_current_screen();
                #ifdef HALT_ON_00X_ERROR
                main_should_power_off_asap = TRUE;
                #endif
            }
            
            #ifndef EMULATOR_BUILD
            /* ADC watchdog timer fired */
            if (main_adc_watchdog_fired != FALSE)
            {
                gui_prompts_display_information_on_screen_and_wait(CONTACT_SUPPORT_010_TEXT_ID, DISP_MSG_WARNING, FALSE);
                gui_dispatcher_get_back_to_current_screen();
                main_adc_watchdog_fired = FALSE;
            }
            
            #ifndef EMULATOR_BUILD
            /* Accelerometer watchdog timer fired */
            if (main_acc_watchdog_fired != FALSE)
            {
                gui_prompts_display_information_on_screen_and_wait(CONTACT_SUPPORT_011_TEXT_ID, DISP_MSG_WARNING, FALSE);
                timer_start_timer(TIMER_ACC_WATCHDOG, 60000);
                gui_dispatcher_get_back_to_current_screen();
                main_acc_watchdog_fired = FALSE;
            }
            #endif
            
            /* Many failed connection attempts */
            if (logic_bluetooth_get_and_clear_too_many_failed_connections() != FALSE)
            {
                gui_prompts_display_information_on_screen_and_wait(MANY_FAILED_CONNS_TEXT_ID, DISP_MSG_WARNING, FALSE);
                gui_dispatcher_get_back_to_current_screen();
                logic_gui_disable_bluetooth(FALSE);
            }
            #endif
            
            /* Aux MCU ping */
            #ifndef EMULATOR_BUILD
            if (timer_has_timer_expired(TIMER_AUX_MCU_PING, TRUE) == TIMER_EXPIRED)
            {
                if ((comms_aux_mcu_are_comms_disabled() == FALSE) && (debugger_present == FALSE))
                {
                    /* Ping the aux */
                    aux_status_return_te get_status_return = comms_aux_mcu_get_aux_status();
                    #ifdef HALT_ON_00X_ERROR
                    main_should_power_off_asap = TRUE;
                    #endif
                    
                    /* Check result */
                    if (get_status_return == RETURN_AUX_STAT_TIMEOUT)
                    {
                        gui_prompts_display_information_on_screen_and_wait(CONTACT_SUPPORT_002_TEXT_ID, DISP_MSG_WARNING, FALSE);
                        gui_dispatcher_get_back_to_current_screen();
                    }
                    else if (get_status_return == RETURN_AUX_STAT_BLE_ISSUE)
                    {
                        gui_prompts_display_information_on_screen_and_wait(CONTACT_SUPPORT_003_TEXT_ID, DISP_MSG_WARNING, FALSE);
                        gui_dispatcher_get_back_to_current_screen();
                    }
                    else if (get_status_return == RETURN_AUX_STAT_INV_MAIN_MSG)
                    {
                        gui_prompts_display_information_on_screen_and_wait(CONTACT_SUPPORT_004_TEXT_ID, DISP_MSG_WARNING, FALSE);
                        gui_dispatcher_get_back_to_current_screen();
                    }
                    else if (get_status_return == RETURN_AUX_STAT_TOO_MANY_CB)
                    {
                        gui_prompts_display_information_on_screen_and_wait(CONTACT_SUPPORT_008_TEXT_ID, DISP_MSG_WARNING, FALSE);
                        gui_dispatcher_get_back_to_current_screen();                        
                    }
                    else if (get_status_return == RETURN_AUX_STAT_ADC_WATCHDOG_FIRED)
                    {
                        gui_prompts_display_information_on_screen_and_wait(CONTACT_SUPPORT_009_TEXT_ID, DISP_MSG_WARNING, FALSE);
                        gui_dispatcher_get_back_to_current_screen();                        
                    }
                    else
                    {
                        main_should_power_off_asap = FALSE;
                    }
                }
                
                /* Rearm aux MCU ping timer */
                timer_start_timer(TIMER_AUX_MCU_PING, NB_MS_AUX_MCU_PING);
            }
            #endif
            
            /* Do appropriate actions on smartcard insertion / removal */
            if (card_detection_res == RETURN_JDETECT)
            {
                /* Light up the Mooltipass and call the dedicated function */
                logic_device_set_state_changed();
                logic_device_activity_detected();
                logic_smartcard_handle_inserted();
            }
            else if (card_detection_res == RETURN_JRELEASED)
            {
                /* Light up the Mooltipass and call the dedicated function */
                logic_device_activity_detected();
                logic_smartcard_handle_removed();
                logic_device_set_state_changed();
                logic_user_locked_feature_trigger();
                
                /* Disable bluetooth? */
                if (custom_fs_settings_get_device_setting(SETTINGS_DISABLE_BLE_ON_CARD_REMOVE) != FALSE)
                {
                    logic_gui_disable_bluetooth(TRUE);
                }
            
                /* Set correct screen */
                gui_prompts_display_information_on_screen_and_wait(CARD_REMOVED_TEXT_ID, DISP_MSG_INFO, FALSE);
                gui_dispatcher_set_current_screen(GUI_SCREEN_NINSERTED, TRUE, GUI_INTO_MENU_TRANSITION);
                gui_dispatcher_get_back_to_current_screen();
            }
            
            /* USB connection timeout */
            if (logic_device_get_and_clear_usb_timeout_detected() != FALSE)
            {
                /* Reset computer locked state */
                logic_user_reset_computer_locked_state(TRUE);
                
                /* Lock device */
                if (logic_security_is_smc_inserted_unlocked() != FALSE)
                {
                    /* Set flag */
                    logic_user_set_user_to_be_logged_off_flag();
                }
            }
            
            /* Make sure all power switches are handled before calling GUI code */
            logic_power_routine();
        
            /* GUI main loop, pass a possible virtual wheel action and reset it */
            gui_dispatcher_main_loop(virtual_wheel_action);
            virtual_wheel_action = WHEEL_ACTION_NONE;      
        }
        
        /* Communications */
        if (gui_dispatcher_get_current_screen() != GUI_SCREEN_FW_FILE_UPDATE)
        {
            comms_aux_mcu_routine(MSG_NO_RESTRICT);
        }
        else
        {
            comms_aux_mcu_routine(MSG_RESTRICT_ALLBUT_BUNDLE);            
        }
        
        /* ADC watchdog */
        if (timer_has_timer_expired(TIMER_ADC_WATCHDOG, TRUE) == TIMER_EXPIRED)
        {
            platform_io_get_vbat_conversion_result_and_trigger_conversion();
            main_adc_watchdog_fired = TRUE;
        }
        
        /* Accelerometer watchdog */
        if (timer_has_timer_expired(TIMER_ACC_WATCHDOG, TRUE) == TIMER_EXPIRED)
        {
            main_acc_watchdog_fired = TRUE;
        }

        /* Accelerometer routine */
        BOOL is_screen_on_copy = oled_is_oled_on(&plat_oled_descriptor);
        BOOL is_screen_saver_on_copy = gui_dispatcher_is_screen_saver_running();
        acc_detection_te accelerometer_routine_return = logic_accelerometer_routine();
        if (accelerometer_routine_return == ACC_FAILING)
        {
            /* Accelerometer failing */
            if (gui_prompts_display_information_on_screen_and_wait(CONTACT_SUPPORT_001_TEXT_ID, DISP_MSG_WARNING, FALSE) == GUI_INFO_DISP_RET_LONG_CLICK)
            {
                /* Trick to not brick the device: long click still allows comms */
                while (TRUE)
                {
                    comms_aux_mcu_routine(MSG_NO_RESTRICT);
                }
            }
            gui_dispatcher_get_back_to_current_screen();
        }
        else if (accelerometer_routine_return == ACC_DET_MOVEMENT)
        {
            /* Movement was detected by the accelerometer routine */
            if ((gui_dispatcher_get_current_screen() == GUI_SCREEN_INSERTED_LCK) && ((is_screen_on_copy == FALSE) || (is_screen_saver_on_copy != FALSE)))
            {
                /* Card inserted and device locked, simulate wheel action to prompt PIN entering */
                virtual_wheel_action = WHEEL_ACTION_VIRTUAL;
            }
            
            /* Movement stopped the screen saver */
            if (is_screen_saver_on_copy != FALSE)
            {
                gui_dispatcher_get_back_to_current_screen();
            }
        }
        else if (((accelerometer_routine_return == ACC_INVERT_SCREEN) || (accelerometer_routine_return == ACC_NINVERT_SCREEN)) && (gui_dispatcher_get_current_screen() != GUI_SCREEN_FW_FILE_UPDATE))
        {
            uint16_t prompt_id = accelerometer_routine_return == ACC_INVERT_SCREEN? QPROMPT_LEFT_HAND_MODE_TEXT_ID:QPROMPT_RIGHT_HAND_MODE_TEXT_ID;
            BOOL invert_bool = accelerometer_routine_return == ACC_INVERT_SCREEN? TRUE:FALSE;
            
            /* Make sure we're not harassing the user */
            if (timer_has_timer_expired(TIMER_HANDED_MODE_CHANGE, FALSE) == TIMER_EXPIRED)
            {
                oled_set_screen_invert(&plat_oled_descriptor, invert_bool);
                inputs_set_inputs_invert_bool(invert_bool);
                
                /* Ask the user to change mode */
                mini_input_yes_no_ret_te prompt_return = gui_prompts_ask_for_one_line_confirmation(prompt_id, FALSE, FALSE, TRUE);
                
                /* In case of power switch, do not touch anything to not confuse the user */
                if (prompt_return == MINI_INPUT_RET_POWER_SWITCH)
                {
                    /* Wait before asking again */
                    timer_start_timer(TIMER_HANDED_MODE_CHANGE, 30000);
                }
                else if (prompt_return == MINI_INPUT_RET_YES)
                {
                    /* Invert screen and inputs */
                    oled_set_screen_invert(&plat_oled_descriptor, invert_bool);
                    inputs_set_inputs_invert_bool(invert_bool);
                    
                    /* Store settings */
                    if (logic_power_get_power_source() == USB_POWERED)
                        custom_fs_set_settings_value(SETTINGS_LEFT_HANDED_ON_USB, (uint8_t)invert_bool);
                    else
                        custom_fs_set_settings_value(SETTINGS_LEFT_HANDED_ON_BATTERY, (uint8_t)invert_bool);
                        
                    /* Set flag */
                    logic_device_set_settings_changed();
                }
                else
                {
                    /* Wait before asking again */
                    timer_start_timer(TIMER_HANDED_MODE_CHANGE, 30000);
                    oled_set_screen_invert(&plat_oled_descriptor, !invert_bool);
                    inputs_set_inputs_invert_bool(!invert_bool);
                }
                gui_dispatcher_get_back_to_current_screen();
            } 
            else
            {
                /* Routine wants to rotate the screen but we already asked the user not long ago, rearm the timer */
                timer_start_timer(TIMER_HANDED_MODE_CHANGE, 30000);
            }            
        }
        
        /* Device state changed, inform aux MCU so it can update its buffer */
        if (logic_device_get_state_changed_and_reset_bool() != FALSE)
        {
            comms_aux_mcu_update_device_status_buffer();
        }
        
        /* Get current smartcard detection result */
        card_detection_res = se_smartcard_is_se_plugged();
    }
}

#if defined(STACK_MEASURE_ENABLED) && !defined(EMULATOR_BUILD)
uint32_t main_stack_low_water_mark = ~0U;

/*! \fn     util_check_stack_usage(void)
*   \brief  check the stack usage
*   \return current low water mark
*/
uint32_t main_check_stack_usage(void)
{
    uint32_t curr_low_water_mark;
    uint32_t stack_start;
    uint32_t stack_end;
    uint32_t i;

    // Get the pointers to the start and end of the stack. These are symbols
    // available from the compiler/linker.
    stack_start = (uint32_t) &_estack;
    stack_end = (uint32_t) &_sstack;

    for (i = stack_end; i < stack_start; ++i)
    {
        uint8_t *ptr = (uint8_t *) i;
        if (*ptr != DEBUG_STACK_TRACKING_COOKIE)
        {
            break;
        }
    }
    curr_low_water_mark = i - stack_end;

    if (curr_low_water_mark < main_stack_low_water_mark)
    {
        main_stack_low_water_mark = curr_low_water_mark;
    }
    return main_stack_low_water_mark;
}

/*! \fn     main_init_stack_tracking(void)
*   \brief  Initialize stack tracking
*/
void main_init_stack_tracking(void)
{
    uint32_t stack_start;
    uint32_t stack_end;
    uint8_t *ptr;

    stack_start = utils_get_SP() - sizeof(uint32_t);
    stack_end = (uint32_t) &_sstack;

    // Inline implementation of memset since we we *might* be destroying
    // our own stack when calling the memset function.
    for (ptr = (uint8_t *) stack_end; (uint32_t) ptr < stack_start; ++ptr)
    {
        *ptr = DEBUG_STACK_TRACKING_COOKIE;
    }
}

#else

/*! \fn     main_check_stack_usage(void)
*   \brief  check the stack usage
*   \return current low water mark
*/
uint32_t main_check_stack_usage(void)
{
    return 0;
}

/*! \fn     main_init_stack_tracking(void)
*   \brief  Initialize stack tracking
*/
void main_init_stack_tracking(void) { }

#endif

