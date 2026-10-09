# FIDO2 in the desktop emulator: implementation log

Branch: `fido2-emulator-bridge` (created from `master`).

Goal: make the desktop emulator (`source_code/main_mcu`, `make -f Makefile.emu`) show up as a
usable FIDO2/CTAP2 authenticator on the host (browser, `libfido2`), so FIDO2 features can be
developed and tested without the hardware.

## 1. Findings (investigation)

### Why the emulator is not recognized today
- The emulator builds only the **main MCU** firmware. The CTAP2/CTAPHID stack
  (`aux_mcu/src/fido2/`) and the USB HID interfaces live on the **aux MCU** and are not built.
- `main_mcu/src/EMU/emu_aux_mcu.c` stands in for the aux MCU. Its only outside link is the Qt
  `QLocalSocket` `moolticuted_local_dev` (`EMU/emulator.cpp`), used by Moolticute for the custom
  Mooltipass HID protocol (`AUX_MCU_MSG_TYPE_USB`).
- `emu_send_aux()` has no `AUX_MCU_MSG_TYPE_FIDO2` case, so FIDO2 messages from the main MCU hit
  `default: break;` and are dropped.
- No `/dev/hidraw*` / uhid device with usage page `0xF1D0` exists, so nothing enumerates.

### Feasibility checks done
- `/dev/uhid` exists (`crw------- root`), so a virtual HID device is possible. Needs a udev rule
  or group for non-root access.
- The FIDO HID descriptor is already in `aux_mcu/src/USB/usb_descriptors.c:247` (usage page
  `0xF1D0`, 64-byte reports).
- Host-compile experiment (scratch shims, nothing committed): `ctap.c`, `ctap_parse.c`,
  `ctaphid.c`, `solo_compat_layer.c` and tinycbor (`cborencoder.c`, `cborparser.c`) compile with
  plain `gcc` given only two stub headers:
  - fake `asf.h` (stdint/stddef/string/stdio includes + `RTC_MODE2_CLOCK_Type` typedef)
  - fake `platform_io.h` (declares `platform_io_uart_debug_printf`)
  Built with `-DDEBUG_LOG_DISABLED`.
- Unresolved symbols after compiling (the full shim surface, 9 symbols):
  - to main MCU: `comms_main_mcu_get_empty_packet_ready_to_be_sent`,
    `comms_main_mcu_get_temp_rx_message_object_pt`, `comms_main_mcu_send_message`,
    `comms_main_mcu_routine`
  - CTAPHID output: `comms_raw_hid_get_send_buffer`, `comms_raw_hid_send_packet`
  - timers: `timer_get_systick`, `timer_start_timer`, `timer_has_timer_expired`

### Main MCU message path (emulator)
- `EMU/dma.c` calls `emu_send_aux()` for every message the main MCU sends and polls
  `emu_rcv_aux()` for incoming ones; both carry raw `aux_mcu_message_t` bytes.
- FIDO2 replies from `LOGIC/logic_fido2.c` reach `emu_send_aux()` with
  `message_type == AUX_MCU_MSG_TYPE_FIDO2` (subtypes `AUX_MCU_FIDO2_*_RSP`).
- FIDO2 requests can be injected through a mutex-protected queue checked by `emu_rcv_aux()`
  before its HID polling. `emu_rcv_aux()` returns one message per call.
- FIDO2 replies must not use the single-slot `response_valid` in `emu_aux_mcu.c`.
- `AUX_MCU_MSG_PAYLOAD_LENGTH` is 552 on both MCUs, so message layouts match.

### Constraint
Aux and main each have their own `comms_aux_mcu_defines.h` / `aux_mcu_message_t` with different
include guards, so they cannot be included in the same translation unit. The aux CTAP code must
be built as separate objects with the aux include path, and talk to the emulator through a small
C interface passing raw message bytes (plus a `sizeof(aux_mcu_message_t)` consistency check).

### Aux CTAP behaviour to remember
`ctap.c` sends a request to the main MCU and then busy-waits on
`comms_main_mcu_routine(TRUE, AUX_MCU_MSG_TYPE_FIDO2)` (`ctap.c` ~212, ~339, ~519), calling
`ctaphid_update_status()` every 50 ms for keepalives. The host shim has to block (mutex +
condition variable) while keeping the keepalives going, since user approval on the emulated GUI
can take many seconds.

## 2. Plan

Estimate: about 1.5 to 2.5 days to a working end-to-end `makeCredential` / `getAssertion`.

1. [ ] Host build of the aux CTAP sources under `source_code/main_mcu/src/EMU/fido/`
       (shim headers, Makefile.emu rules), linked into the emulator but unused.
2. [ ] Host shims for the 9 unresolved symbols (timers via `clock_gettime`).
3. [ ] `uhid` bridge (`EMU/fido/emu_fido_hid.c`): create the device with the FIDO descriptor,
       feed output reports to `ctaphid_handle_packet()`, send `ctaphid_write_block()` output back
       as input reports.
4. [ ] Checkpoint: `fido2-token -L` lists the device and `CTAPHID_INIT` works (no main MCU needed).
5. [ ] Blocking RPC to the main MCU: request queue read by `emu_rcv_aux()`, `AUX_MCU_MSG_TYPE_FIDO2`
       case in `emu_send_aux()` that wakes the waiting CTAP thread, keepalives while waiting.
6. [ ] End-to-end tests: `fido2-cred` / `fido2-assert`, then a browser (webauthn.io).
7. [ ] Docs: udev rule for `/dev/uhid` and hidraw access, how to run.

Fallback if the bridge turns out too costly: a test harness injecting raw CTAP2 CBOR requests
into the emulator (no browser/USB path).

## 3. Progress log

| Date | Step | Notes |
|---|---|---|
| 2026-10-09 | Investigation | Findings above. No firmware code changed yet. |
| 2026-10-09 | Branch | Created `fido2-emulator-bridge` from `master`. |
| 2026-10-09 | Host-compile experiment | Aux CTAP sources build on host with 2 stub headers; 9 symbols left to shim. |
