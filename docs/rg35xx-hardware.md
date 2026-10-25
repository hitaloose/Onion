# RG35XX (original): hardware notes for the Onion port

Facts marked **known** come from reading the Garlic OS install (`misc/dmenu.bin`, `CFW/start`,
RetroArch config, `strings` on its binaries). Facts marked **TODO** must be filled in from
`platform/rg35xx/tools/recon.sh` output on a real device.

## SoC and boot
- **known**: Actions ATM7039S ("owl"), 4x Cortex-A9, kernel 3.10.37 (`misc/uImage`), U-Boot 2015.04.
- **known**: `misc/` is `/dev/block/actb` (mmcblk0p1, vfat), mounted at `/misc`; roms partition is
  mmcblk0p4 at `/mnt/mmc`. TF2 is mmcblk1(p1).
- **known**: the ramdisk init runs `/loadapp.sh`, which loops on `/system/dmenu/dmenu_ln`. That runs
  `/misc/dmenu.bin` when present (else `/mnt/vendor/bin/dmenu.bin`), then `sh /tmp/.next` if it exists.
- **known**: the stock userland is glibc 2.15 armhf (`/lib/ld-linux-armhf.so.3`); `loadapp.sh` copies
  `/system/usr/local/lib` to `/` on boot. The stock shell has no `head`/`which`/`uname`; busybox is at
  `/system/bin/busybox`.
- **known**: 237 MB RAM visible, no swap, `zram0` present. VTs `tty0`..`tty63` exist (`tty1` active).
- **known**: modules loaded from `/misc/modules`: `gpio_keys_polled.ko` (gamepad), `atc260x_cap_gauge.ko`.

## Input
- **known**: kernel exposes an input device named "RG35XX Gamepad"; SDL sees it as a joystick.
  Buttons: A=0 B=1 X=2 Y=3 L1=5 R1=6 SELECT=7 START=8 MENU=9 VOL+=10 VOL-=11, L3=31 R3=32 (unused);
  d-pad is hat 0; L2/R2 are axes 2/5.
- **known** (`getevent`), gamepad is `event1`, handlers `js0 event1` (no `kbd`):

  | Button | evdev | | Button | evdev |
  |---|---|---|---|---|
  | A | `BTN_A` (304) | | SELECT | `BTN_SELECT` (314) |
  | B | `BTN_B` (305) | | START | `BTN_START` (315) |
  | X | `BTN_X` (307) | | MENU | `BTN_MODE` (316) |
  | Y | `BTN_Y` (308) | | VOL+ | `BTN_THUMBL` (317) |
  | L1 | `BTN_TL` (310) | | VOL- | `BTN_THUMBR` (318) |
  | R1 | `BTN_TR` (311) | | D-pad | `ABS_HAT0X`/`ABS_HAT0Y`, -1 = left/up |
  | L2 | `ABS_Z`, 0 or 255 | | R2 | `ABS_RZ`, 0 or 255 |

- **known**: power is `KEY_POWER` on `event0` ("atc260x_onoff"); the gamepad also sends `BTN_Z` at the
  same time, which must be ignored.
- **known**: `/dev/uinput` exists. `src/padmapd` uses it to expose an "Onion Keyboard" with the Miyoo
  keycodes (`keymap_hw.h`), linked at `/dev/input/onion-kbd`. Tested on device: it gets the `kbd`
  handler and all 18 keys (including L2/R2, d-pad and power) come out with the expected codes.

## Display
- **known**: 640x480, `/dev/fb0`, no rotation. HDMI mirrors via
  `/sys/devices/framebuffer.4/graphics/fb0/mirror_to_hdmi` when `/sys/class/switch/hdmi/state` is 1.
- **known**: `owlfb`, 32bpp, stride 2560, `pan` used for flipping. `virtual_size` is set by the running
  app, not fixed: stock Garlic leaves 640x1440, a Garlic variant left 640x960 panned to 0,480, and
  SDL 1.2 fbcon sets 640x960 at offset 0,0 on `SDL_SetVideoMode`.
- **known**: pixel layout `rgba 8/0,8/8,8/16,0/24`: red is the low byte (ABGR8888 as a 32-bit word).
  The Miyoo is ARGB8888, so direct framebuffer writes (OSD, screenshots) must swap R and B.

## Backlight
- **known**: `/sys/class/backlight/backlight.2/brightness`.
- **known**: `max_brightness` 1024 (stock boots at 512); `bl_power` also exists.

## Battery and charging
- **known**: `/sys/class/power_supply/battery/{capacity,charger_online,status,voltage_now,current_now}`.
  Also `atc260x-usb/online` and `atc260x-wall/online`.

## Rumble
- **known**: `/sys/class/power_supply/battery/moto` (0 = off).

## Audio
- **known**: ALSA, RetroArch uses 48000 Hz. The gamepad module handles volume keys in kernel
  (`allen_adjust_volume`) and has a `volume` sysfs attribute.
- **known**: one card, `atm7059link`: PCM 0 is the ATC2603C codec, 1 is HDMI, 2 is Bluetooth.
- **known**: volume is `/sys/class/volume/value` (40 on a fresh boot).
- **known**: the value stays the same after pressing VOL+ (tested with `padmapd` running), so the kernel
  does not change it by itself: keymon must write it.
- **TODO**: its range (write test values and listen).
- **known**: ALSA mixer (`amixer scontrols` in our rootfs) has no `Master`. Output-related controls:
  `DAC FL Gain`, `DAC FR Gain`, `DAC Digital FL FR`, `DAC FL FR PLAYBACK`, `DAC PA`,
  `DAC PA OUTPUT Stage`, `speaker on off switch`, `audio output mode switch`, `PA Output Swing Mux`.
  **TODO**: which one `/sys/class/volume/value` drives, and which to use for volume.

## CPU
- **known**: hotplug mask `/sys/devices/system/cpu/autoplug/plug_mask` (0xF = all cores on);
  cpufreq on cpu0, `interactive` governor, steps 504000..1488000 kHz.
- **known**: 240000 504000 720000 840000 1008000 1104000 1200000 1296000 1392000 1488000 kHz,
  driver `owl_cpu0`, cpus 0-3 share one policy; governors include `userspace` and `interactive`.

## Suspend
- **known**: `/sys/power/state` offers `freeze mem disk`; `wake_lock` and `autosleep` exist (Android PM).
  **TODO**: whether `mem` resumes correctly and which key wakes it.

## Rootfs (Phase 1, tested on device)
- **known**: our Buildroot image (glibc 2.41, headers 3.10) loop-mounts and runs in the chroot.
  The image must be built without `metadata_csum`, `metadata_csum_seed` and `orphan_file`: the 3.10
  ext4 driver refuses `metadata_csum_seed` ("unsupported optional features (2000)").
- **known**: SDL 1.2 `fbcon` opens 640x480x32 (red/green/blue bars confirmed on screen) and reports `Rshift=0 Gshift=8 Bshift=16`, so SDL itself
  handles the pixel order. Keyboard input reaches SDL through the VT (`padmapd` -> `tty1`).
- **known**: SDL fbcon must not run under a pty (e.g. a plain `adb shell`), or it reads keys from the
  pty. Taking the VT as controlling terminal sends SIGHUP, so the boot chain ignores HUP
  (`trap '' HUP` in `misc/dmenu.bin`).
- **known**: inside the chroot, run commands with `PATH=/usr/sbin:/usr/bin:/sbin:/bin`; the Android
  PATH is inherited otherwise.

## RTC
- **known**: `/dev/rtc0`.
