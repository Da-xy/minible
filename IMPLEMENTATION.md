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

1. [x] Host build of the aux CTAP sources: shim headers in `source_code/main_mcu/src/EMU/fido/shim/`,
       `fido-objs` target in `Makefile.emu`. Objects are built but NOT linked into the emulator yet
       (linking needs the step 2 shims, otherwise the 9 symbols are unresolved).
2. [x] Host shims for the 9 unresolved symbols (`src/EMU/fido/emu_fido_shim.c`, timers via
       `clock_gettime`). The FIDO objects are now part of the emulator link, still unused at run time.
       The comms_main_mcu_* shims are placeholders until step 5.
3. [x] `uhid` bridge (`EMU/fido/emu_fido_hid.c`, started with `./Release-emu/minible --fido`): create the device with the FIDO descriptor,
       feed output reports to `ctaphid_handle_packet()`, send `ctaphid_write_block()` output back
       as input reports.
4. [x] Checkpoint: `fido2-token -L` lists the device and `CTAPHID_INIT` works (no main MCU needed).
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
| 2026-10-09 | Step 1 done | `make -f Makefile.emu fido-objs` builds `ctap`, `ctap_parse`, `ctaphid`, `solo_compat_layer`, `cborencoder`, `cborparser` into `Release-emu/fido/` with no warnings. Still exactly the same 9 unresolved symbols. Emulator link unchanged. |
| 2026-10-09 | Step 2 done | Added `emu_fido_shim.c`, `emu_fido.h`, `emu_fido_rename.h`; `FIDO_OBJS` added to the emulator link in `Makefile.emu`. Remaining undefined symbols of the fido objects: libc only. Host test (scratchpad, not committed): feeding a `CTAPHID_INIT` broadcast packet to `ctaphid_handle_packet()` returns one 64-byte reply (cid ffffffff, cmd 0x86, bcnt 17). Full emulator build and link verified (`make -f Makefile.emu`, Qt5 available after the shell change): `Release-emu/minible` links with the FIDO objects, no errors. |
| 2026-10-09 | Step 3 done | Added `emu_fido_hid.c` (Linux uhid, one thread), `--fido` command line option, `emu_fido_start()/stop()` in `emulator.cpp`. Tested headless (`QT_QPA_PLATFORM=offscreen`) with a raw hidraw Python script: the device shows up as `1209:4322` ("Mooltipass Mini BLE emulator FIDO"), `CTAPHID_INIT` and `CTAPHID_PING` get correct replies from the real aux CTAP code. `fido2-token -L` not tried yet (package not installed). |
| 2026-10-09 | Step 4 done | `fido2-token -L` lists `/dev/hidraw2: vendor=0x1209, product=0x4322`. `fido2-token -I /dev/hidraw2` returns proto 2, caps 0x0c (nowink, cbor, nomsg), aaguid 6db042d0-61af-404c-a887-e72e09ba7eb4, options rk/up/uv/noplat, maxmsgsiz 1024: libfido2 drives `CTAPHID_INIT` and CTAP2 `getInfo` against the host-built aux code. First two attempts failed with `FIDO_ERR_INTERNAL` at `dev->io.open` (before any traffic); not reproducible afterwards (6 successful runs, including under strace). |

## 4. Notes and open points

- Step 1 build details: aux sources get their own include path (`src/EMU/fido/shim` first so it
  beats `aux_mcu/src/asf.h`, then the aux dirs and `tinycbor/src`), `-DDEBUG_LOG_DISABLED`, and a
  separate object dir `$(OUTPUT_DIR)/fido`. They are not compiled with the main MCU's strict
  warning flags (third-party-derived Solo code).
- The full emulator could not be built in the shell used for this work: `pkg-config` finds no
  Qt5 packages and there is no `moc`/`qmake` (the shell is the VSCodium flatpak sandbox).
  `fido-objs` does not need Qt. Building and running the emulator itself (needed from step 5 on)
  must be done where Qt5 dev packages are available.
- Name collision: the main MCU defines `timer_get_systick`, `timer_start_timer` and
  `timer_has_timer_expired`. The aux objects are compiled with `-include emu_fido_rename.h`, which
  renames them to `emu_fido_timer_*`, so they bind to the host timers in the shim. None of the other
  66 global symbols defined by the aux objects clash with the main tree (checked by name).
- `Makefile.emu` does not track flag or header changes for the `fido/` objects. After changing
  `FIDO_DEFINES`/`FIDO_FLAGS`, run `rm -rf Release-emu/fido` (or `make -f Makefile.emu clean`).
- The FIDO block in `Makefile.emu` must stay above the `$(TARGET)` rule: make expands rule
  prerequisites immediately, so `FIDO_OBJS` has to be defined first.
- Shim timers are not thread safe: only the CTAP thread (step 3) may call the aux code.
- Host setup needed once (Linux): `sudo modprobe uhid` (persist with `/etc/modules-load.d/uhid.conf`),
  and give your user access to `/dev/uhid` with a udev rule
  `KERNEL=="uhid", TAG+="uaccess"` (e.g. `/etc/udev/rules.d/70-uhid-emu.rules`). A stale `/dev/uhid`
  node without the module loaded fails with "No such device". The created hidraw node got a user ACL
  automatically on the test machine (systemd uaccess), no extra rule was needed.
- The virtual device uses vendor/product `1209:4322`, different from the real device (`1209:4321`)
  so Moolticute and similar tools do not take it for hardware.
- `--fido` is opt-in: without it the emulator behaves as before. If `/dev/uhid` cannot be opened the
  emulator keeps running and prints the reason on stderr.
- `ctaphid_check_timeouts()` is never called by the real aux firmware either, so the bridge does
  not call it. `ctap_init()` is empty.
- CTAP requests that need the main MCU (makeCredential, getAssertion) will currently hit the
  placeholder `comms_main_mcu_*` shims: do not test them before step 5 (the CTAP thread would wait
  in a loop for an answer that never comes).
- Intermittent `FIDO_ERR_INTERNAL` seen twice on the first runs of step 4, failing at
  `fido_dev_open_tx: dev->io.open` (the open of the hidraw node itself). Cause not established; the
  user ACL on the new node (granted asynchronously by udev/logind) is the suspect. If it comes back,
  check `getfacl /dev/hidrawN` right after the failure and simply retry.
- `fido2-token -L` shows empty manufacturer/product strings (`( )`): uhid has no USB parent to take
  them from. Cosmetic only.
