/*!  \file     asf.h
*    \brief    Host stand-in for the Atmel Software Framework header, used only when building the aux MCU FIDO2 (CTAP) sources for the desktop emulator
*    \note     Must be found before aux_mcu/src/asf.h on the include path
*/
#ifndef EMU_FIDO_SHIM_ASF_H_
#define EMU_FIDO_SHIM_ASF_H_

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>

/* Needed by aux_mcu/src/TIMER/driver_timer.h */
typedef union
{
    struct
    {
        uint32_t SECOND:6;
        uint32_t MINUTE:6;
        uint32_t HOUR:5;
        uint32_t DAY:5;
        uint32_t MONTH:4;
        uint32_t YEAR:6;
    } bit;
    uint32_t reg;
} RTC_MODE2_CLOCK_Type;

#endif /* EMU_FIDO_SHIM_ASF_H_ */
