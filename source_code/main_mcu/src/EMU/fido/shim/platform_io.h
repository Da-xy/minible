/*!  \file     platform_io.h
*    \brief    Host stand-in for aux_mcu/src/PLATFORM/platform_io.h, used only when building the aux MCU FIDO2 (CTAP) sources for the desktop emulator
*    \note     Must be found before the aux MCU platform_io.h on the include path
*/
#ifndef EMU_FIDO_SHIM_PLATFORM_IO_H_
#define EMU_FIDO_SHIM_PLATFORM_IO_H_

#include <stdint.h>

void platform_io_uart_debug_printf(const char *fmt, ...);

#endif /* EMU_FIDO_SHIM_PLATFORM_IO_H_ */
