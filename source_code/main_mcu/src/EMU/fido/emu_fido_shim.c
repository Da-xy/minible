/*!  \file     emu_fido_shim.c
*    \brief    Host implementations of what the aux MCU FIDO2 (CTAP) sources expect from the aux MCU firmware
*    \note     Compiled against the AUX MCU headers (see Makefile.emu), not the main MCU ones. Timer
*              functions are renamed by emu_fido_rename.h. Only the CTAP thread may use them.
*/
#include <stdio.h>
#include <time.h>
#include "comms_main_mcu.h"
#include "comms_raw_hid.h"
#include "driver_timer.h"
#include "emu_fido.h"

/* Number of milliseconds in a second, and of nanoseconds in a millisecond */
#define MS_PER_SECOND   1000u
#define NS_PER_MS       1000000u

/* Deadline of each timer in ms (systick domain), valid only if the matching armed flag is set */
static uint32_t timer_deadlines[TOTAL_NUMBER_OF_TIMERS];
static BOOL timer_armed[TOTAL_NUMBER_OF_TIMERS];

/* Buffers handed to the CTAP code */
static hid_packet_t hid_send_buffer;
static aux_mcu_message_t main_mcu_tx_message;
static aux_mcu_message_t main_mcu_rx_message;

/* Where packets for the host go */
static emu_fido_hid_tx_callback_t hid_tx_callback = NULL;

/*! \fn     emu_fido_set_hid_tx_callback(emu_fido_hid_tx_callback_t callback)
*   \brief  Set (or clear with NULL) the callback receiving CTAPHID packets destined to the host
*   \param  callback    The callback
*/
void emu_fido_set_hid_tx_callback(emu_fido_hid_tx_callback_t callback)
{
    hid_tx_callback = callback;
}

/*! \fn     timer_get_systick(void)
*   \brief  Get a millisecond counter (host monotonic clock)
*   \return Milliseconds, wrapping at 2^32 like the firmware counter
*/
uint32_t timer_get_systick(void)
{
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return (uint32_t)((uint64_t)now.tv_sec * MS_PER_SECOND + (uint64_t)now.tv_nsec / NS_PER_MS);
}

/*! \fn     timer_start_timer(timer_id_te uid, uint32_t val)
*   \brief  Start (or restart) a timer
*   \param  uid Unique ID
*   \param  val Delay in ms
*/
void timer_start_timer(timer_id_te uid, uint32_t val)
{
    timer_deadlines[uid] = timer_get_systick() + val;
    timer_armed[uid] = TRUE;
}

/*! \fn     timer_has_timer_expired(timer_id_te uid, BOOL clear)
*   \brief  Know if a timer expired and disarm it if so
*   \param  uid     Unique ID
*   \param  clear   Boolean to say if we disarm an expired timer
*   \return TIMER_EXPIRED or TIMER_RUNNING (see enum)
*/
timer_flag_te timer_has_timer_expired(timer_id_te uid, BOOL clear)
{
    /* Signed difference handles the 32-bit wrap */
    if ((timer_armed[uid] != FALSE) && ((int32_t)(timer_get_systick() - timer_deadlines[uid]) >= 0))
    {
        if (clear != FALSE)
        {
            timer_armed[uid] = FALSE;
        }
        return TIMER_EXPIRED;
    }
    return TIMER_RUNNING;
}

/*! \fn     comms_raw_hid_get_send_buffer(hid_interface_te hid_interface)
*   \brief  Get the buffer in which a CTAPHID packet to send is built
*   \param  hid_interface   Interface (only CTAP_INTERFACE is used here)
*   \return Pointer to the buffer
*/
hid_packet_t* comms_raw_hid_get_send_buffer(hid_interface_te hid_interface)
{
    (void)hid_interface;
    return &hid_send_buffer;
}

/*! \fn     comms_raw_hid_send_packet(hid_interface_te hid_interface, hid_packet_t* packet, BOOL wait_send, uint16_t payload_size)
*   \brief  Send a CTAPHID packet to the host
*   \param  hid_interface   Interface (only CTAP_INTERFACE is used here)
*   \param  packet          Packet to send
*   \param  wait_send       Unused, sending is synchronous
*   \param  payload_size    Packet size
*/
void comms_raw_hid_send_packet(hid_interface_te hid_interface, hid_packet_t* packet, BOOL wait_send, uint16_t payload_size)
{
    (void)hid_interface;
    (void)wait_send;
    if (hid_tx_callback != NULL)
    {
        hid_tx_callback(packet->raw_packet, payload_size);
    }
}

/*! \fn     comms_main_mcu_get_temp_rx_message_object_pt(void)
*   \brief  Get the buffer in which the main MCU answers are received
*   \return Pointer to the buffer
*/
aux_mcu_message_t* comms_main_mcu_get_temp_rx_message_object_pt(void)
{
    return &main_mcu_rx_message;
}

/*! \fn     comms_main_mcu_get_empty_packet_ready_to_be_sent(aux_mcu_message_t** message_pt_pt, uint16_t message_type)
*   \brief  Get an empty message to send to the main MCU
*   \param  message_pt_pt   Where to store the message pointer
*   \param  message_type    Message type
*/
void comms_main_mcu_get_empty_packet_ready_to_be_sent(aux_mcu_message_t** message_pt_pt, uint16_t message_type)
{
    memset(&main_mcu_tx_message, 0, sizeof(main_mcu_tx_message));
    main_mcu_tx_message.message_type = message_type;
    *message_pt_pt = &main_mcu_tx_message;
}

/*! \fn     comms_main_mcu_send_message(volatile aux_mcu_message_t* message, uint16_t message_length)
*   \brief  Send a message to the main MCU
*   \param  message         Message to send
*   \param  message_length  Message length
*   \note   TODO (step 5): hand the message to the emulated main MCU. For now it is only reported.
*/
void comms_main_mcu_send_message(volatile aux_mcu_message_t* message, uint16_t message_length)
{
    (void)message_length;
    fprintf(stderr, "emu_fido: main MCU link not implemented, dropping message type 0x%04x\n", (unsigned)message->message_type);
}

/*! \fn     comms_main_mcu_routine(BOOL filter_and_force_use_of_spare_receive_buffer, uint16_t expected_message_type)
*   \brief  Wait for a message from the main MCU
*   \param  filter_and_force_use_of_spare_receive_buffer    Unused
*   \param  expected_message_type                           Unused
*   \return RETURN_NOK as no message ever arrives
*   \note   TODO (step 5): block until the emulated main MCU answers.
*/
ret_type_te comms_main_mcu_routine(BOOL filter_and_force_use_of_spare_receive_buffer, uint16_t expected_message_type)
{
    (void)filter_and_force_use_of_spare_receive_buffer;
    (void)expected_message_type;
    return RETURN_NOK;
}
