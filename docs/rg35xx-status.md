# RG35XX port: status and open issues

## Open issues

- **Battery header on battery power is still unconfirmed.** The gauge reports 0% until it measures,
  and while charging the header shows the charging icon, so the header at boot has only been checked
  on USB. Test after the next reboot without the cable.
- **Hi-res frames are downscaled**: RetroArch draws 320x240 for the hardware 2x scaling, so SNES
  hi-res (512 wide) and PS1 640x480 modes lose detail. Could switch to a 640x480 mode for those.
- **CPU clock**: capped at 1200 MHz because 1488 MHz hung the device under load while charging.
  1296/1392 MHz untested.
- **`LD_PRELOAD=.../libpadsp.so`** is still hardcoded in GameSwitcher (`gs_history.h`),
  `state.h`, `apps.h`, `randomGamePicker.c` and some scripts. On the RG35XX the loader only prints
  a warning and goes on.
- **mGBA core**: the libretro fork builds with CMake, not `Makefile.libretro`; `cores.sh` skips it.
- **Expert tab, RApp and standalone emulators** are not in MainUI yet.
- The FAT partition always logs "Volume was not properly unmounted": kernel 3.10 does not clear the
  dirty flag on remount read-only. Real corruption shows as "invalid cluster chain" errors.

## Fixed, for the record

- Battery 0% at boot turned into a shutdown on the next game exit: the gauge reports 0% until it
  measures, keymon armed `/tmp/.lowBat` from that 0, and `runtime.sh` only acts on the flag when a
  game ends (`bootScreen lowBat`, `.offOrder`, `shutdown`). That is the "device turns itself off
  when leaving a game or GameSwitcher" report, with no log because it is a clean shutdown. Now only
  a real, non-charging reading above 0% arms the flag, it is dropped when the battery recovers, and
  `runtime.sh` logs both the low-battery and the `.offOrder` shutdown to `runtime.log`. While batmon
  has not written `/tmp/percBat`, the headers follow the gauge once a second.

- Card corruption: rootfs.img was loop-mounted read-write from the card, so the card could never
  be remounted read-only at shutdown. Now read-only through `loopro`, with tmpfs on /tmp, /run and
  /appconfigs.
- Frozen screens / black screen on launch: SDL fbcon VT_LOCKSWITCHes while in graphics mode, so a
  program that died (or was stopped) in graphics mode left VT switching locked and the next SDL
  program waited forever in VT_WAITACTIVE. SDL is patched to stay on the active VT without locking.
- Slow games: the `ondemand` governor never left 240 MHz; `interactive` is used now.
- RetroArch read `~/.config/retroarch` instead of Onion's `.retroarch`, crashed on duped (NULL)
  frames, drew nothing with bilinear scaling to ABGR, and used Miyoo button numbers.
