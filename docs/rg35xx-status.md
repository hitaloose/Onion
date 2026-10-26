# RG35XX port: status and open issues

## Open issues

- **Battery shows 0% for ~10 s after boot.** `battery_getPercentage()` now falls back to
  `/sys/class/power_supply/battery/capacity` while `/tmp/percBat` is missing (commit b27531f4), which
  stopped keymon from closing games for "low battery" right after boot, but the MainUI header still
  starts at 0%. Likely cause: `battery_hasChanged()` only refreshes when `/tmp/percBat` changes, and
  the first value MainUI reads may come before batmon writes it. To check: batmon start-up time
  (sqlite on the card), and whether MainUI should read the gauge on its first frame.
- **Software scaling is the main CPU cost in RetroArch.** Point scaling to 640x480 in C; SNES
  (supafaust) is at ~60 fps with no headroom. Next: hardware scaling on an owlfb overlay, or a NEON
  scaler.
- **`LD_PRELOAD=.../libpadsp.so`** is still hardcoded in GameSwitcher (`gs_history.h`),
  `state.h`, `apps.h`, `randomGamePicker.c` and some scripts. On the RG35XX the loader only prints
  a warning and goes on.
- **VT switch hang**: once seen, `vtreset` could not switch tty2 back to tty1 even with the VT in
  text/auto mode; a clean reboot cleared it. Not reproduced since.
- **mGBA core**: the libretro fork builds with CMake, not `Makefile.libretro`; `cores.sh` skips it.
- **Expert tab, RApp and standalone emulators** are not in MainUI yet.
- The FAT partition always logs "Volume was not properly unmounted": kernel 3.10 does not clear the
  dirty flag on remount read-only. Real corruption shows as "invalid cluster chain" errors.

## Fixed, for the record

- Card corruption: rootfs.img was loop-mounted read-write from the card, so the card could never
  be remounted read-only at shutdown. Now read-only through `loopro`, with tmpfs on /tmp, /run and
  /appconfigs.
- RetroArch read `~/.config/retroarch` instead of Onion's `.retroarch`, crashed on duped (NULL)
  frames, drew nothing with bilinear scaling to ABGR, and used Miyoo button numbers.
