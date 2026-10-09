/*!  \file     emu_fido.h
*    \brief    Interface of the emulator's FIDO2 (CTAP) host bridge
*    \note     Only uses plain C types, so that it can be included from the main MCU side as well as
*              from the aux MCU sources (their aux_mcu_message_t definitions cannot be mixed).
*/
#ifndef EMU_FIDO_H_
#define EMU_FIDO_H_

#include <stdint.h>

/* Size of a CTAPHID packet */
#define EMU_FIDO_HID_PACKET_SIZE    64

/*! \brief  Callback called with each CTAPHID packet the authenticator sends to the host */
typedef void (*emu_fido_hid_tx_callback_t)(const uint8_t* packet, uint16_t length);

#ifdef __cplusplus
extern "C" {
#endif

/*! \brief  Set (or clear with NULL) the callback receiving CTAPHID packets destined to the host */
void emu_fido_set_hid_tx_callback(emu_fido_hid_tx_callback_t callback);

/*! \brief  Main MCU side: take the next message the aux MCU (CTAP code) wants to send to the main MCU
*   \param  buffer  Where to copy the message (an aux_mcu_message_t)
*   \param  size    Size of the buffer, must be sizeof(aux_mcu_message_t)
*   \return 1 if a message was copied, 0 if there is none (or if the size is wrong)
*   \note   Called from emu_rcv_aux(), in the thread running the main MCU firmware
*/
int emu_fido_main_mcu_pop_request(void* buffer, uint32_t size);

/*! \brief  Main MCU side: deliver to the aux MCU (CTAP code) a message the main MCU sent to it
*   \param  message The message (an aux_mcu_message_t of type AUX_MCU_MSG_TYPE_FIDO2)
*   \param  size    Message size, must be sizeof(aux_mcu_message_t)
*   \note   Called from emu_send_aux(), in the thread running the main MCU firmware
*/
void emu_fido_main_mcu_push_response(const void* message, uint32_t size);

/*! \brief  Create the virtual FIDO HID device and start the thread serving it (Linux uhid only)
*   \return 0 if started, -1 on error (reason printed on stderr)
*/
int emu_fido_start(void);

/*! \brief  Stop the thread started by emu_fido_start() and remove the virtual HID device */
void emu_fido_stop(void);

#ifdef __cplusplus
}
#endif

#endif /* EMU_FIDO_H_ */
