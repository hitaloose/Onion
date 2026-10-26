# RG35XX (original): Onion kernel

Own kernel for the port (GPU plan phase G1), so a release does not depend on the Garlic OS kernel.
**Tested on the device (2026-10-26):** boots Onion; gamepad, ADB, CPU frequencies, battery, sound,
volume and games work.

## Build

```sh
platform/rg35xx/kernel/build.sh     # 1st time ~5 min (clone + full build), then incremental
```

Output in `cache/rg35xx/kernel/`, laid out like the `misc` partition: `uImage`, `kernel.dtb`,
`dtbs/{2100,2600,2800}mAh.dtb`, `modules/*.ko`.

| Path | Role |
|---|---|
| `platform/rg35xx/kernel/toolchain/Dockerfile` | Ubuntu 14.04 amd64 + GCC 4.7.3 Linaro (runs under emulation on Apple Silicon) |
| `platform/rg35xx/kernel/build.sh` | Clones the source into the `rg35xx-kernel` volume, applies patches, builds, compiles the DTS |
| `platform/rg35xx/kernel/patches/` | `0001` xhci-trace include path (3.12 fix), `0002` Kconfig/Makefile for the gamepad, `0003` CPU frequency table |
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

The repo's `rg35xx_atm7039.dts` (Koriki) is **not used**. Compared with the Garlic `kernel.dtb`, it:
- puts `vbus_otg_en_gpios` on GPIO 78, which the stock tree uses for `charger_led2_ctl` (stock: 25);
- adds external PWM DC-DC regulators `vdd-core-dcdc`/`vdd-gpu-dcdc` that the stock tree does not have
  (a wrong PWM table would change the CPU/GPU voltage);
- has a different 2100 mAh battery table (terminal voltage 3450 vs 3200 mV) and no 2600/2800 tables;
- drops `lcd_spics2_gpios` (GPIO 31) and `earphone_detect2_gpios`, and changes the LED settings;
- enables the GPU (stock: `status = "disabled"`) and sets `pixclock` 47000 (60 Hz) instead of 50000.

So `dts/gs705a.dtsi` is the Garlic `kernel.dtb` decompiled verbatim (the 2600 mAh variant). Its board
name is still the Actions reference design ("Actions GS705A board"). `rg35xx.dts` adds `model` and the
gamepad node, and changes one thing: `speaker_en_gpios` (GPIO 40) is active **high**. The stock tree
flags it active low; the Garlic kernel ignores the flag, but this codec driver follows it and kept the
speaker off (no sound, and RetroArch ran unthrottled because nothing blocked on audio). `rg35xx-{2100,2800}mAh.dts` replace only `capacity` and `ocv_soc_*`, taken from
Garlic's `misc/dtbs/`. Decompiled again, our DTBs are identical to Garlic's except for those
additions. They are padded (`dtc -p 4096`) as the stock one is, so U-Boot has room to add `bootargs`.

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
  above what the CPU uses at 1.2 GHz. `normal_value = <0>` keeps the GPU at 300 MHz (stock 2 = 400
  MHz); try 400 MHz once things are stable.
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
  (GB/GBC/GG, GBA with less margin), because its cost scales with the source size. The GPU at
  400 MHz (`normal_value = <2>`) should give about a third more.
- **Vsync does nothing**: `video_vsync` on or off gives the same unthrottled speed (FLIP ignores
  `eglSwapInterval`). Games are paced by audio sync, as with the `sdl` driver.
- Testing pitfalls: RetroArch keeps the **first** occurrence of a repeated key in a config file
  (an appended line does not override an earlier one in the same file), and Onion's keymon
  SIGSTOPs RetroArch when the screen times out (stop keymon during long adb benchmarks).

## Still from Garlic

`misc/ramdisk.img` (Garlic build, Dec 2022), `bootloader.img`, `u-boot-dtb.img`, `uenv.txt` and the
boot images come from the Garlic card image. Our kernel boots with that ramdisk: its `init.rc` mounts
`/system` and `/misc`, insmods `/misc/modules/{gpio_keys_polled,atc260x_cap_gauge}.ko` and runs
`loadapp.sh` → `dmenu.bin`. A release also needs its own ramdisk, or proof that these files are the
stock Anbernic ones.

## Installing

`misc` is only 10 MB (3.5 MB free), so two `uImage` do not fit. The Garlic kernel files are backed
up in `garlic-misc/` on the roms partition of the test card (and in
`RG35XX-CopyPasteOnTopOfStock.7z/misc/` on the Mac). `misc/modules/` holds only our
`atc260x_cap_gauge.ko`.

Over ADB: `adb push` the file to `/mnt/mmc`, then `mount -o remount,rw /misc`, `busybox cp` over the
old one (U-Boot is the only reader of `uImage`/`kernel.dtb`), `sync`, `mount -o remount,ro /misc`,
and compare `md5sum`. Rollback: card in the Mac, copy `garlic-misc/*` back over `misc/`.

## Open issues

- **Reboot powers off** (`devchroot.sh reboot`, i.e. `reboot -f`): only the first one came back. With
  USB connected the device then shows the charging screen (Garlic's or Onion's) and only powers on
  after unplugging. `owl_pm_restart()` asks the PMIC for `OWL_PMIC_REBOOT_TGT_SYS`.
- **ADB is slow to appear after boot** and sometimes drops; restarting the adb server on the Mac
  (`adb kill-server`) until it shows up has worked, and so has unplugging and replugging the cable. The USB monitor logs "set adaptor mode" when the
  host does not enumerate in time.
- **"android" text on screen during boot**: init's console output on fbcon (`FRAMEBUFFER_CONSOLE=y`).
- **Headphone detection untested**: the stock tree sets `earphone_detect_method = 3` and
  `earphone_detect2_gpios`, which this codec driver does not know (it uses GPIO 28).
- **FAT mounts use `errors=remount-ro`** (Garlic: `errors=continue`): a real FAT error makes the card
  read-only until reboot.
- **Leaving GameSwitcher for the game list powered the device off** a few times (with the `sdl`
  RetroArch driver, GPU modules loaded); not reproduced after RetroArch moved to `gl`.
- **Unplugging USB once rebooted the device.**
- `misc/ramdisk.img`, the bootloaders and `uenv.txt` are still Garlic's (see above).
