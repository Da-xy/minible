/*!  \file     emu_fido_rename.h
*    \brief    Symbol renames for the aux MCU FIDO2 (CTAP) sources built for the desktop emulator
*    \note     Force-included (-include) when compiling the aux sources and emu_fido_shim.c. The main MCU
*              firmware already defines timer_start_timer() & co with different semantics, so the aux
*              code must call the host implementations in emu_fido_shim.c instead.
*/
#ifndef EMU_FIDO_RENAME_H_
#define EMU_FIDO_RENAME_H_

#define timer_get_systick           emu_fido_timer_get_systick
#define timer_start_timer           emu_fido_timer_start_timer
#define timer_has_timer_expired     emu_fido_timer_has_timer_expired

#endif /* EMU_FIDO_RENAME_H_ */
