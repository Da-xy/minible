#ifndef EMU_AUX_MCU_H
#define EMU_AUX_MCU_H

#ifdef __cplusplus
extern "C" {
#endif

void emu_send_aux(char *data, int size);
int emu_rcv_aux(char *data, int size);

/*! \brief  Set the device date and time from the host clock (UTC), like Moolticute does with HID_CMD_ID_SET_DATE */
void emu_set_device_time_from_host(void);

#ifdef __cplusplus
}
#endif

#endif
