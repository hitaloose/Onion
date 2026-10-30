# RG35XX (original): Onion kernel

Own kernel for the port (GPU plan phase G1), so a release does not depend on the Garlic OS kernel.
**Tested on the device (2026-10-26):** boots Onion; gamepad, ADB, CPU frequencies, battery, sound,
volume and games work.

## Build

```sh
platform/rg35xx/kernel/build.sh     # 1st time ~5 min (clone + full build), then incremental
```

Output in `cache/rg35xx/kernel/`, laid out like the `misc` partition: `uImage`, `kernel.dtb`,
`modules/*.ko`.

| Path | Role |
|---|---|
| `platform/rg35xx/kernel/toolchain/Dockerfile` | Ubuntu 14.04 amd64 + GCC 4.7.3 Linaro (runs under emulation on Apple Silicon) |
| `platform/rg35xx/kernel/build.sh` | Clones the source into the `rg35xx-kernel` volume, applies patches, builds, compiles the DTS |
| `platform/rg35xx/kernel/patches/` | `0001` xhci-trace include path (3.12 fix), `0002` Kconfig/Makefile for the gamepad, `0003` CPU frequency table, `0004` charger input threshold |
| `platform/rg35xx/kernel/drivers/rg35xx-gamepad.c` | Gamepad driver, copied into `drivers/input/joystick/` |
| `platform/rg35xx/kernel/rg35xx.config` | Changes merged over `rg35xx_atm7039_defconfig` |
| `platform/rg35xx/kernel/dts/` | `gs705a.dtsi` (stock board), `rg35xx.dts` (+ gamepad), battery variants |

- Source: `rg35xx-cfw/s500_kernel`, branch `rg35xx` (Linux 3.10.37, Actions SDK). Build log:
  `/work/build.log` in the volume.
- Linux 3.10 needs an old GCC. The Garlic kernel was built with Linaro GCC 4.7.3
  (`blackseraph@Pandemonium`, build #116, Apr 2023), and Ubuntu 14.04's `gcc-4.7-arm-linux-gnueabihf`
  is the same compiler. The Linaro release downloads no longer work (they redirect to a contact page).
- Modules have the same vermagic as Garlic's (`3.10.37 SMP preempt mod_unload ARMv7 p2v8`).

## Config changes (`rg35xx.config`)

- `JOYSTICK_RG35XX=y`, `INPUT_JOYDEV=y` (gamepad, `js0` as on Garlic).
- `USB_G_ANDROID=y`: **ADB**. The ramdisk's `usbmond.sh` writes `/sys/class/android_usb/android0`;
  the repo defconfig has the Android gadget off, so ADB would not work without this.
- `USB_PLATFORM_LINUX` off: with it, the USB monitor ignores what `usbmond.sh` writes
  (`usb_con_msg`, `run`) and the gadget never connects. The Android defconfig has it off too.
- `IKCONFIG_PROC=y` (`/proc/config.gz`). The Garlic kernel has no embedded config.

## CPU frequencies (`patches/0003`)

The driver actually built is `drivers/cpufreq/s500-cpufreq.c` (not `owl-cpufreq.c`). Its table stops
at 900 MHz, listed twice, with 1104 MHz commented out. The patch puts in the table of the Garlic
kernel, found in its image (same board and `cpuvdd` regulator):

| MHz | 240 | 504 | 720 | 840 | 1008 | 1104 | 1200 | 1296 | 1392 | 1488 |
|---|---|---|---|---|---|---|---|---|---|---|
| mV | 975 | 1000 | 1025 | 1075 | 1100 | 1175 | 1200 | 1275 | 1325 | 1375 |

Onion still caps `scaling_max_freq` at 1200 MHz. `autoplug/plug_mask` does not exist: `hotplug-owl.c`
is never built because its Kconfig entry is misspelled (`config CONFIG_HOTPLUG_OWL`). All four cores
stay online, which is what `dmenu.bin` asks for with `plug_mask=0xF`; the write just fails.

## Device tree

The repo's `rg35xx_atm7039.dts` (Koriki) is **not used**. Compared with the stock `kernel.dtb`, it:
- puts `vbus_otg_en_gpios` on GPIO 78, which the stock tree uses for `charger_led2_ctl` (stock: 25);
- adds external PWM DC-DC regulators `vdd-core-dcdc`/`vdd-gpu-dcdc` that the stock tree does not have
  (a wrong PWM table would change the CPU/GPU voltage);
- has a different 2100 mAh battery table;
- drops `lcd_spics2_gpios` (GPIO 31) and `earphone_detect2_gpios`, and changes the LED settings;
- enables the GPU (stock: `status = "disabled"`) and sets `pixclock` 47000 (60 Hz) instead of 50000. The kernel does not program the LCD clock
  (U-Boot does), so `pixclock` only changes the reported mode: the panel runs at 60.46 Hz either way.

So `dts/gs705a.dtsi` is the Anbernic stock `kernel.dtb` (card image `35XX-64GB230309EN`, 2100 mAh
battery). It was first decompiled from Garlic's, whose only changes are a 2600 mAh battery table,
`boot_cap_threshold` 0 and no `moto_pwm_gpio`; the battery part was put back to stock on 2026-10-30
(Garlic's 2100/2800 mAh variants are gone with it). Its board
name is still the Actions reference design ("Actions GS705A board"). `rg35xx.dts` adds `model` and the
gamepad node, the earphone GPIO (see "Earphone jack") and `boot_cap_threshold` 0 (stock 3: the gauge reads 0% until
its first measurement), and `speaker_en_gpios` (GPIO 40) is active **high**. The stock tree
flags it active low; the Garlic kernel ignores the flag, but this codec driver follows it and kept the
speaker off (no sound, and RetroArch ran unthrottled because nothing blocked on audio). Decompiled
again, our DTB is identical to the stock one except for those changes and the GPU node. It is padded (`dtc -p 4096`) as the stock one is, so U-Boot has room to add `bootargs`.

The GPU and 60 Hz changes are left for later, one at a time (G2, and the 60 Hz item in the hand-off).

## Gamepad (`rg35xx-gamepad.c`)

Garlic's `gpio_keys_polled.ko` ("RG35XX Gamepad Driver", Black-Seraph, GPL v2 with no published
source) cannot load on this kernel: it calls `allen_adjust_volume`, which only the Garlic kernel
exports. Our driver is new code with the same evdev interface (name "RG35XX Gamepad", `BUS_HOST`
0001:0001 v0100, the same keys and axes), so `padmapd`, SDL and the RetroArch autoconfig need no
change. It is built in, so the ramdisk's `insmod gpio_keys_polled.ko` just fails.

GPIOs, read from the table in Garlic's module (all active low, polled; Garlic polls every 20 ms,
ours every 10 ms):

| Button | GPIO | | Button | GPIO |
|---|---|---|---|---|
| Up / Down / Left / Right | 86 / 89 / 88 / 87 | | L1 / R1 | 114 / 116 |
| A / B / X / Y | 91 / 92 / 90 / 93 | | L2 / R2 | 117 / 115 |
| START | 14 **or** 85 | | MENU | 16 **or** 71 |
| SELECT | 15 **or** 84 | | VOL+ / VOL- | 20 / 19 **or** 72 / 73 |

START, SELECT, MENU and the volume keys have two GPIOs (board revisions). The pins a board does not
use read active: on the test device, 14/15/16/19/20 read low all the time and the buttons are on
85/84/71/72/73. Garlic's driver picks the set on its first poll and ignores the pins that read pressed;
ours does the same at probe (a GPIO that reads active while another one with its code does not is
dropped, logged as "gpio N unused").

`input_set_abs_params()` does not set `EV_ABS` in this kernel; the driver sets it, or the d-pad and
L2/R2 axes are missing. Some of these GPIOs also appear in
stock nodes that are disabled or never loaded: camera (73, 114, module `owl_camera.ko`) and
`wifi_bt_power_ctl` (90, 91, 115, `status = "disabled"`).

## Volume and framebuffer

Garlic's `/sys/class/volume/value` (0..40) comes from that same Garlic module and does not exist on
this kernel. `src/common/system/volume.h` writes the ALSA control `DAC PA Volume` (same 0..40 range)
through the control ioctl when the attribute is missing. Tested: the volume keys work.

owlfb reports `rgba 8/16,8/8,8/0` here (0xAARRGGBB, like the Miyoo); the Garlic kernel reports red in
the low byte. SDL and RetroArch (patch 0002) follow the reported layout; `display_fbColor()` now reads
it too (it swapped R and B unconditionally, which broke the GameSwitcher preview).

## GPU (G2, working)

OpenGL ES 2.0 renders on the panel: `platform/rg35xx/tools/egltest.c` (EGL + a spinning GLES2
triangle) runs smoothly, at about 789 fps (no vsync wait), `GL renderer: PowerVR SGX 544MP`.

- Kernel: `pvrsrvkm.ko` is `SGX_DDK_Linux_XOrg sgxddk MAIN@3625561` (1.14) and needs `drm.ko`; it
  creates `/dev/dri/card0`. The display class (`dc_omapfb3_linux`, omaplfb) is built into it (no
  `dc_owl.ko` as on Garlic/Android). `rg35xx.dts` enables `actions,atm7039c-gpu` (stock: `disabled`).
- Voltage: the PMIC has no GPU rail (`dcdc1` 0.7-1.4 V is the CPU, `dcdc2` 1.3-2.2 V, `dcdc3`
  2.6-3.3 V), so the driver falls back to `dcdc1` and only raises its minimum
  (`regulator_set_voltage(vdd, INT_MAX)`): 1025/1050/1125/1200 mV for 240/300/400/480 MHz, never
  above what the CPU uses at 1.2 GHz. `normal_value = <2>` (stock) runs the GPU at 400 MHz
  (1.125 V): stable through about 15 minutes of shader benchmarks, 20-30% faster than 300 MHz.
  Switch at runtime by writing 1005 (300 MHz) or 5009 (400 MHz) to
  `/sys/devices/b0300000.gpu/pvrsrv/devices`; `.../pvrsrv/runtime` shows the clocks. Temperature
  and battery drain at 400 MHz were not measured.
- User space: Buildroot package `powervr-sgx-um` (`platform/rg35xx/buildroot/package/`), the
  Cubieboard 6 `v2/` libraries pinned at rg35xx-cfw/cubieboard6_ubuntu_gpu_drivers@989041df, plus
  `libdrm`, headers and `egl.pc`/`glesv2.pc` in the SDK. Desktop GL, OpenCL and the DRI window system
  (X11) are left out.
- Window system: only `libpvrPVR2D_FLIPWSEGL.so` works (`/etc/powervr.ini`). LINUXFB is loaded and
  refuses the display (`eglInitialize` fails with `EGL_NOT_INITIALIZED`); FRONT and BLIT untested.
  The native window is NULL. FLIP calls `set_par` on owlfb for every frame (thousands of
  `set_par(0)` lines in `dmesg`). The IMG app hints are not read from the environment; to try
  another `powervr.ini`, bind-mount a file over `/onion/etc/powervr.ini` (the stock shell has no
  `printf`: write it with `echo`).
- Boot: `misc/dmenu.bin` insmods `/misc/modules/{drm,pvrsrvkm}.ko` and runs
  `pvrsrvctl --start --no-module` in the chroot, only when `drm.ko` is there (Garlic's misc has an
  Android `pvrsrvkm.ko` that does not match these libraries). Modules are stripped of debug symbols
  by `build.sh` (misc has about 3 MB free).
- RetroArch (G3, working): built with `--enable-egl --enable-opengles --enable-mali_fbdev`; patch
  `platform/rg35xx/retroarch/patches/0004` makes the `mali_fbdev` context pass a NULL native window.
  `runtime.sh` adds `res/rg35xx_retroarch_gpu.cfg` (`video_driver = "gl"`, `video_context_driver =
  "mali_fbdev"`) when `/dev/dri/card0` exists, otherwise RetroArch keeps the `sdl` driver (Garlic
  kernel). Input stays the SDL joystick, which works without SDL video. GL renders at the
  framebuffer size (640x480), so the owldss 2x scaling is not used.
- Benchmark, Yoshi's Island (Beetle Supafaust), CPU 1.2 GHz, unthrottled, audio off: `sdl` 112 fps,
  `gl` 102-119 fps (the spread is load time and run to run noise).
- Shaders: `platform/rg35xx/retroarch/shaders.sh` collects the presets in `shaders/presets.txt`
  from libretro/glsl-shaders (pinned) with the files they use (`collect.py`), and `dist.sh` copies
  them to `RetroArch/.retroarch/shaders`. Ten presets, one per effect, with the measured fps in
  `presets.txt`. The SGX GLSL ES 1.00 compiler rejects a second `#define COMPAT_PRECISION` and
  non-square matrices (`mat4x3`); `glsl_es_fix.py` rewrites both. GLSL 1.20+ features (bitwise
  operators, array constructors) cannot be fixed that way. The GPU is the limit: shaders that
  run per output pixel with many dependent texture reads (xbr, xbrz, mmpx, sabr, lottes) run at
  5-40 fps. Pixel rounding (2xScaleHQ, hq2x, scale2x) is fast enough only for small screens
  (GB/GBC/GG and, at 400 MHz, GBA), because its cost scales with the source size; on SNES it
  stays just under 60 fps even at 400 MHz.
- **Vsync**: FLIP ignores `eglSwapInterval`, and the `dc_owl` flip does not wait either. owlfb's
  `OWLFB_WAITFORVSYNC` returns at once unless the vsync interrupt is enabled with
  `OWLFB_VSYNC_EVENT_EN` (the Android hwcomposer's job). RetroArch patch `0007` enables it and waits
  after each `eglSwapBuffers`; with `video_vsync` games run at the panel's 60.46 Hz.
- Testing pitfalls: RetroArch keeps the **first** occurrence of a repeated key in a config file
  (an appended line does not override an earlier one in the same file), and Onion's keymon
  SIGSTOPs RetroArch when the screen times out (stop keymon during long adb benchmarks).

## Earphone jack (`patches/0005`)

The jack is sensed on **GPIO 81** (high with a plug in), the stock tree's `earphone_detect2_gpios`.
`earphone_detect_gpios` (GPIO 28) never changes, and `earphone_detect_method = 3` is unknown to this
codec driver (it only registers its own detection for 1, IRQ, and 2, ADC). The Garlic kernel requests
GPIO 81 as `earphone_dect_new`. `rg35xx.dts` points `earphone_detect_gpios` at GPIO 81, flagged
active low as in the stock tree: `link-owl.c` reports `earphone_is_in()` the other way round, so the
flag makes `/sys/class/switch/h2w/state` read 2 (headphone) with a plug in and 0 without.
`link-owl.c` polls the GPIO every 200 ms; on Android the audio HAL then muted the speaker, so the speaker
kept playing next to the earphone. `patches/0005` switches the amplifier GPIO (40) on each change of
the jack state. The earphone output itself is always on and follows `DAC PA Volume`. Tested
(2026-10-30): sound only on the earphone with a plug in, only on the speaker without, volume keys on
both.

To read an unrequested GPIO through sysfs, write `in` to its `direction` after exporting it: until
then its input enable bit (`INEN`, 0xB01B0004 + 0xC per bank) stays off and it reads 0.

## Charger input threshold (`patches/0004`)

The RG35XX is DCIN only (`support_adaptor_type = 1`): any cable counts as a wall adapter, charged at
1000 mA (backlight on) or 1200 mA (off). The driver calls the adapter unplugged when `WALLV` drops
below `wall_v_thresh`, 4300 mV in this tree without an external DC-DC (`ext_dcdc_exist = 0`). A Mac
USB port sags to about 4.28 V at 660 mA, so the charger switched itself off and on every few seconds:
Mains online/offline uevents, the charger screen lighting up on each "device plugged in",
`battery/charger_online` flickering. The Garlic kernel's driver is a newer Anbernic version (battery
temperature control, more LED pins; no public source found) whose probe sets 3800 mV for
`ext_dcdc_exist = 0` (disassembled through its `/proc/kallsyms`); on the same port it charges steadily
at about 800 mA with 4.18 V in. The patch uses 3800 mV. Tested: 630 mA at 4.28 V with no drop, the
charger screen animates once and stays dark, and a 256 MB `adb push` while charging went through
(it failed repeatedly before; USB on a sagging port is the likely cause of the ADB drops too).

On the charger screen only the power key acts (short press: screen on; long press: boot): the
gamepad's `BTN_*` codes reach `/charger` but it ignores them.

## Boot ramdisk (`platform/rg35xx/ramdisk/`)

`platform/rg35xx/ramdisk/build.sh <stock ramdisk.img>` takes the stock `misc/ramdisk.img` (Garlic
card image, Dec 2022 build; a U-Boot legacy image, type ramdisk, uncompressed newc cpio, load
`0x02000000`), puts our `init.rc` and `bootmode.sh` over it and writes `cache/rg35xx/ramdisk.img`.
The stock `init.rc` mounts `/system` and `/misc`, insmods `/misc/modules/{gpio_keys_polled,
atc260x_cap_gauge}.ko` and runs `loadapp.sh` → `dmenu.bin`; `init`, `/charger` and the scripts are
still the stock binaries.

**Reboot and charger mode.** With a cable plugged in, U-Boot's `check_power()` passes
`androidboot.mode=charger` unless the pstore flag `DIS_MCHRG` is set, and init then runs only `on
early-init` and `on charger` (no `class core`, so no `loadapp.sh`). The kernel sets `DIS_MCHRG` (bit 0
of `UV_INT_EN`, reg 0x37) before rebooting through `SYS_CTL0` bit 10, but that PMIC reset clears the
register (written by hand and reset: still the charger), so every reboot with a cable, and the
`/charger` power-key reboot, ended on the charger screen. The PMIC does keep what woke it, and the
kernel logs it at boot (`atc260x-pm: translated wakeup falgs: 0x...`, `OWL_PMIC_WAKEUP_SRC_*`):

| Boot | Flags |
|---|---|
| Power button, cable or not | `0x100` (ONOFF_LONG) |
| Cable plugged into the powered-off device | `0x200` (WALL_IN) |
| Reboot (`reboot -f`), cable or not | `0x0` |

`on charger` starts `bootmode.sh`, which reads that line and sets `sys.onion.boot`: `charger` only
when WALL_IN or VBUS_IN is set (or the line is missing), which runs the stock charger actions;
`system` otherwise, which runs `trigger early-boot` + `trigger boot`, the normal boot. Each decision
is appended to `/mnt/data/bootmode.log`. While `/mnt/data/.chargerlog` exists, charger mode also
mirrors the kernel log (where `/charger` writes) to `/mnt/data/charger-kmsg.log`, since there is no
ADB in that mode. Onion's own charging screen at boot (`chargingState` in
`runtime.sh`) is skipped on the RG35XX for the same reason. Tested: reboot with and without cable,
power button and cable plug while off, power key on the charger screen.

A U-Boot built from source (to check the wakeup flags there) was tried first and abandoned: the
Actions SDK tree has no board device tree with a `/pmu` node and its defconfig lacks `run`/hush, and
without a serial console two flashes failed blind (work parked in `~/Development/u-boot_s500-rg35xx`,
backups in `~/rg35xx-backup/`).

**Other changes over the stock ramdisk.** `initlogo.rle` is Onion's boot screen
(`src/bootScreen/res/bootScreen.png`, converted by `tools/bootlogo.py` to init's RGB565 RLE; init
draws it fine on the 32 bpp framebuffer): without it init writes "A N D R O I D" on `tty0`. The same
script writes `cache/rg35xx/boot_logo.bmp.gz` (24-bit BMP) for U-Boot's logo in `misc`, so the screen
shows one image from U-Boot until MainUI. `loadapp.sh` mounts the roms partition with `errors=continue`, as
Garlic does; the kernel default `remount-ro` left the card read-only after any FAT error, and
`fat_remount` in 3.10 ignores options, so it only works at that first mount.

**Stock or Garlic?** Compared on 2026-10-30 with the Anbernic stock card image `35XX-64GB230309EN.IMG`
(firmware of 2023-03-09; a `TF1.img` backup has the same misc partition) and with Garlic's `garlic.img`.
All three cards have the same GPT layout (MISC 10 MB at sector 16384, SYSTEM 512 MB, CONFIG, ROMS).

| File | Where it comes from |
|---|---|
| `bootloader.img` (raw at 0x200200 on the card) | stock, byte for byte |
| U-Boot (raw at 0x300000; "U-Boot 2015.04 (Feb 25 2023 - 10:09:24)Actions-Semi") | stock, byte for byte |
| `ramdisk.img` | stock: same 43 entries, modes and contents; only the image header and cpio dates differ |
| `uenv.txt`, `{0..5}_charger_frame`, `charger_logo`, `battery_low`, `recovery_logo` (`.bmp.gz`) | stock, identical |
| `kernel.dtb` | Garlic: the stock tree with a 2600 mAh battery (stock: 2100 mAh, other OCV table, `terminal_voltage` 3450 vs 3200 mV), `boot_cap_threshold` 3 → 0 and no `moto_pwm_gpio`. Ours is back on the stock battery data |
| `dtbs/`, `dmenu.bin`, `hdmi.bin`, `def_config.bin`, `boot_logo`, `uImage`, `modules/` | Garlic (`uImage` built by blackseraph, 2023-04-06) |

The raw areas of `garlic.img` differ from stock only in the GPT and leftover FAT sectors in unused
space. Garlic's card image has no `bootloader.img`/`u-boot-dtb.img` in misc (only its CopyPaste
package does), so U-Boot does not need them there. The stock `/system/dmenu/dmenu_ln` runs
`/misc/dmenu.bin` when it exists, otherwise the Anbernic launcher `/mnt/vendor/bin/dmenu.bin`: that is
the hook both Garlic and this port use, so a stock card only needs our files in misc.

So nothing of Garlic's is left in the boot chain: the files are ours or stock.

## Installing

`misc` is only 10 MB (3.5 MB free), so two `uImage` do not fit. The Garlic kernel files are backed
up in `garlic-misc/` on the roms partition of the test card (and in
`RG35XX-CopyPasteOnTopOfStock.7z/misc/` on the Mac). `misc/modules/` holds only our
`atc260x_cap_gauge.ko`, `drm.ko` and `pvrsrvkm.ko`; `dmenu.bin.stock` is Garlic's.

Over ADB: `adb push` the file to `/mnt/mmc`, then `mount -o remount,rw /misc`, `busybox cp` over the
old one (U-Boot is the only reader of `uImage`/`kernel.dtb`/`ramdisk.img`), `sync`, `mount -o remount,ro /misc`,
and compare `md5sum`. Rollback: card in the Mac, copy `garlic-misc/*` back over `misc/`.

## Open issues

- **ADB is slow to appear after boot** and used to drop: the drops stopped with `patches/0004` (the
  charger cycling on a USB port); restarting the adb server on the Mac
  (`adb kill-server`) until it shows up has worked, and so has unplugging and replugging the cable. The USB monitor logs "set adaptor mode" when the
  host does not enumerate in time.
- **Leaving GameSwitcher for the game list powers the device off** now and then: seen with the
  `sdl` RetroArch driver and again with `gl` and the GPU at 400 MHz. Now explained by the battery
  flag: a 0% gauge reading at boot armed `/tmp/.lowBat` and the first game exit ran `bootScreen
  lowBat` + `shutdown`, which is why no log survived. keymon only arms it from a real reading now,
  `runtime.sh` clears it at boot and logs a shutdown (`:: Shutdown requested`), so a recurrence
  leaves a trace. Confirmed on battery power after a reboot (2026-10-30).
- **Unplugging USB once rebooted the device**: probably Onion's `chargingState`, which powers off when
  the cable goes and is no longer started at boot.
- **Black frame between U-Boot's logo and init's** when booting on battery (power key, no cable);
  not with a cable. Not investigated yet (2026-10-30), possibly older than today's kernel.
