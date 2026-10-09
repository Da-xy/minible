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

/*! \brief  Set (or clear with NULL) the callback receiving CTAPHID packets destined to the host */
void emu_fido_set_hid_tx_callback(emu_fido_hid_tx_callback_t callback);

#endif /* EMU_FIDO_H_ */
