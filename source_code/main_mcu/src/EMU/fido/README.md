# FIDO2 in the desktop emulator

The emulator can appear on the host as a real FIDO2 security key. The aux MCU CTAP2/CTAPHID code
(`source_code/aux_mcu/src/fido2/`) is compiled for the host and runs next to the emulated main MCU.
A Linux `uhid` virtual HID device (`emu_fido_hid.c`) carries the CTAPHID packets, so `libfido2`
tools and browsers can talk to it. Linux only.

## One-time host setup

```sh
sudo modprobe uhid                                   # persist: echo uhid | sudo tee /etc/modules-load.d/uhid.conf
echo 'KERNEL=="uhid", TAG+="uaccess"' | sudo tee /etc/udev/rules.d/70-uhid-emu.rules
sudo udevadm control --reload && sudo udevadm trigger
sudo apt install fido2-tools                         # optional, for the command line tests
```

Log out and in again (or check `getfacl /dev/uhid`) so your user gets access to `/dev/uhid`.
The created `/dev/hidrawN` node is normally made accessible by systemd's `uaccess`. If a browser
cannot see the key, add a rule for it, for example
`KERNEL=="hidraw*", ATTRS{idVendor}=="1209", ATTRS{idProduct}=="4322", TAG+="uaccess"` (a virtual
uhid device has no USB parent, so match with `SUBSYSTEM=="hidraw", KERNELS=="*1209:4322*"` if the
attribute match does not apply on your system).

## Running

```sh
cd source_code/main_mcu
make -f Makefile.emu
./Release-emu/minible --fido --set-time --bundle emu_assets/miniblebundle.img
```

- `--fido` creates the virtual key `1209:4322` (the real device is `1209:4321`).
- `--set-time` sets the emulated device time from the host clock. Without it the firmware shows
  "Time not set" and refuses makeCredential (Moolticute normally sets the time).
- The emulated profile and credentials live in `eeprom.bin` / `dbflash.bin` in the directory the
  emulator is started from (git-ignored). Complete the first-run tutorial and unlock the card once.
- Input is the mouse on the OLED window: left click = wheel click (yes), right click = long press,
  scroll = wheel.
- After editing `FIDO_DEFINES`/`FIDO_FLAGS` in `Makefile.emu`, `rm -rf Release-emu/fido`.

## Checks

```sh
fido2-token -L                     # lists /dev/hidrawN vendor=0x1209, product=0x4322
fido2-token -I /dev/hidrawN        # getInfo
cd $(mktemp -d)
printf '%s\n' "$(head -c32 /dev/urandom | base64)" example.com "user$RANDOM" "$(head -c16 /dev/urandom | base64)" | fido2-cred -M /dev/hidrawN es256 > cred.txt
fido2-cred -V -o pubkey.pem es256 < cred.txt && echo "credential OK"
printf '%s\n' "$(head -c32 /dev/urandom | base64)" example.com "$(sed -n 5p cred.txt)" | fido2-assert -G /dev/hidrawN es256 > assert.txt
fido2-assert -V pubkey.pem es256 < assert.txt && echo "assertion OK"
```

Approve the prompts on the emulator window. A browser test: register and log in on webauthn.io.
`FIDO_DEBUG=1` makes libfido2 dump the traffic.

## Limits

- One credential per user name per site (firmware limitation, also on hardware). A second
  registration with the same user name but another user handle is refused with
  `CTAP2_ERR_LIMIT_EXCEEDED` before any prompt is shown. Use a different user name per test.
- Only the main MCU firmware is emulated: BLE, USB enumeration and the aux MCU update path are not.
- Design and history: `IMPLEMENTATION.md` at the repository root.
