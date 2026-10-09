/*!  \file     emu_fido_shim.c
*    \brief    Host implementations of what the aux MCU FIDO2 (CTAP) sources expect from the aux MCU firmware
*    \note     Compiled against the AUX MCU headers (see Makefile.emu), not the main MCU ones. Timer
*              functions are renamed by emu_fido_rename.h. Only the CTAP thread may use them.
*/
#include <pthread.h>
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

/* Number of messages for the main MCU that can wait to be picked up. CTAP is request/response,
 * so one is enough in practice. */
#define REQUEST_QUEUE_LENGTH    4

/* Pause before resending a request the main MCU asked to retry, in ms, so that we do not spin */
#define RETRY_PAUSE_MS          2

/* How long comms_main_mcu_routine() waits for an answer before returning, in ms. ctap.c calls it in
 * a loop and sends a CTAPHID keepalive every 50ms, so this only needs to be short. */
#define ROUTINE_WAIT_MS         10

/* Link with the emulated main MCU: the CTAP thread produces requests and consumes the response, the
 * main MCU thread does the opposite. Everything below is protected by link_mutex. */
static pthread_mutex_t link_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t response_cond;
static pthread_once_t link_init_once = PTHREAD_ONCE_INIT;
static aux_mcu_message_t request_queue[REQUEST_QUEUE_LENGTH];
static uint32_t request_queue_head = 0;
static volatile uint32_t request_queue_count = 0;
static aux_mcu_message_t response_message;
static BOOL response_available = FALSE;

/* Last request sent, kept to resend it if the main MCU asks us to retry (it does so when it is busy,
 * like the real aux MCU firmware does in comms_main_mcu_routine()) */
static aux_mcu_message_t last_request;

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

/*! \fn     link_init(void)
*   \brief  Initialize the condition variable so that timed waits use the monotonic clock
*/
static void link_init(void)
{
    pthread_condattr_t attr;
    pthread_condattr_init(&attr);
    pthread_condattr_setclock(&attr, CLOCK_MONOTONIC);
    pthread_cond_init(&response_cond, &attr);
    pthread_condattr_destroy(&attr);
}

/*! \fn     link_size_is_valid(uint32_t size)
*   \brief  Check that the main MCU side agrees with us on the message size
*   \param  size    Size given by the main MCU side
*   \return TRUE if valid
*   \note   The aux and main MCU have separate definitions of aux_mcu_message_t that cannot be
*           included together, this size is the guard against them drifting apart.
*/
static BOOL link_size_is_valid(uint32_t size)
{
    if (size != sizeof(aux_mcu_message_t))
    {
        fprintf(stderr, "emu_fido: message size mismatch between aux and main MCU definitions (%u != %zu)\n", (unsigned)size, sizeof(aux_mcu_message_t));
        return FALSE;
    }
    return TRUE;
}

/*! \fn     emu_fido_main_mcu_pop_request(void* buffer, uint32_t size)
*   \brief  Main MCU side: take the next message the CTAP code wants to send to the main MCU
*   \param  buffer  Where to copy the message
*   \param  size    Size of the buffer, must be sizeof(aux_mcu_message_t)
*   \return 1 if a message was copied, 0 otherwise
*/
int emu_fido_main_mcu_pop_request(void* buffer, uint32_t size)
{
    int return_value = 0;

    /* Cheap unlocked test first: this is called from the main MCU polling loop */
    if ((request_queue_count == 0) || (link_size_is_valid(size) == FALSE))
    {
        return 0;
    }
    pthread_mutex_lock(&link_mutex);
    if (request_queue_count != 0)
    {
        memcpy(buffer, &request_queue[request_queue_head], sizeof(aux_mcu_message_t));
        request_queue_head = (request_queue_head + 1) % REQUEST_QUEUE_LENGTH;
        request_queue_count--;
        return_value = 1;
    }
    pthread_mutex_unlock(&link_mutex);
    return return_value;
}

/*! \fn     emu_fido_main_mcu_push_response(const void* message, uint32_t size)
*   \brief  Main MCU side: deliver a message from the main MCU to the CTAP code
*   \param  message The message
*   \param  size    Message size, must be sizeof(aux_mcu_message_t)
*/
void emu_fido_main_mcu_push_response(const void* message, uint32_t size)
{
    if (link_size_is_valid(size) == FALSE)
    {
        return;
    }
    pthread_once(&link_init_once, link_init);
    pthread_mutex_lock(&link_mutex);
    memcpy(&response_message, message, sizeof(aux_mcu_message_t));
    response_available = TRUE;
    pthread_cond_signal(&response_cond);
    pthread_mutex_unlock(&link_mutex);
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
*   \param  message_length  Message length, must be sizeof(aux_mcu_message_t)
*   \note   The message is queued and picked up by the emulated main MCU through emu_rcv_aux()
*/
void comms_main_mcu_send_message(volatile aux_mcu_message_t* message, uint16_t message_length)
{
    if (link_size_is_valid(message_length) == FALSE)
    {
        return;
    }
    pthread_mutex_lock(&link_mutex);
    memcpy(&last_request, (const void*)message, sizeof(aux_mcu_message_t));
    if (request_queue_count == REQUEST_QUEUE_LENGTH)
    {
        fprintf(stderr, "emu_fido: request queue full, dropping message type 0x%04x\n", (unsigned)message->message_type);
    }
    else
    {
        uint32_t tail = (request_queue_head + request_queue_count) % REQUEST_QUEUE_LENGTH;
        memcpy(&request_queue[tail], (const void*)message, sizeof(aux_mcu_message_t));
        request_queue_count++;
    }
    pthread_mutex_unlock(&link_mutex);
}

/*! \fn     comms_main_mcu_routine(BOOL filter_and_force_use_of_spare_receive_buffer, uint16_t expected_message_type)
*   \brief  Wait (for a short while) for a message from the main MCU
*   \param  filter_and_force_use_of_spare_receive_buffer    Unused, the message always goes to the temporary receive buffer
*   \param  expected_message_type                           Type of the message we are waiting for, others are discarded
*   \return RETURN_OK if the message was received (it is in the buffer given by comms_main_mcu_get_temp_rx_message_object_pt()), RETURN_NOK if nothing came in time
*/
ret_type_te comms_main_mcu_routine(BOOL filter_and_force_use_of_spare_receive_buffer, uint16_t expected_message_type)
{
    ret_type_te return_value = RETURN_NOK;
    BOOL resend_last_request = FALSE;
    struct timespec deadline;
    (void)filter_and_force_use_of_spare_receive_buffer;

    pthread_once(&link_init_once, link_init);
    clock_gettime(CLOCK_MONOTONIC, &deadline);
    deadline.tv_nsec += (long)ROUTINE_WAIT_MS * NS_PER_MS;
    if (deadline.tv_nsec >= (long)MS_PER_SECOND * NS_PER_MS)
    {
        deadline.tv_nsec -= (long)MS_PER_SECOND * NS_PER_MS;
        deadline.tv_sec++;
    }

    pthread_mutex_lock(&link_mutex);
    while (response_available == FALSE)
    {
        if (pthread_cond_timedwait(&response_cond, &link_mutex, &deadline) != 0)
        {
            break;
        }
    }
    if (response_available != FALSE)
    {
        response_available = FALSE;
        if (response_message.message_type == expected_message_type)
        {
            if ((response_message.message_type == AUX_MCU_MSG_TYPE_FIDO2) && (response_message.fido2_message.message_type == AUX_MCU_FIDO2_RETRY))
            {
                /* Main MCU is busy: send the same request again, the caller keeps waiting */
                resend_last_request = TRUE;
            }
            else
            {
                memcpy(&main_mcu_rx_message, &response_message, sizeof(aux_mcu_message_t));
                return_value = RETURN_OK;
            }
        }
    }
    pthread_mutex_unlock(&link_mutex);

    if (resend_last_request != FALSE)
    {
        struct timespec pause = {0, (long)RETRY_PAUSE_MS * NS_PER_MS};
        nanosleep(&pause, NULL);
        comms_main_mcu_send_message(&last_request, sizeof(aux_mcu_message_t));
    }
    return return_value;
}
