# RG35XX port: status and open issues

## Open issues

- **Hi-res frames are downscaled**: RetroArch draws 320x240 for the hardware 2x scaling, so SNES
  hi-res (512 wide) and PS1 640x480 modes lose detail. Could switch to a 640x480 mode for those.
- **CPU clock**: capped at 1200 MHz because 1488 MHz hung the device under load while charging.
  1296/1392 MHz untested. Games run with the performance governor at the core's `cpuclock.txt`
  (RetroArch patch 0006, defaults in `platform/rg35xx/configs/`); a Miyoo value above 1200 is capped.
- **Random Game app** is not on the card: `randomGamePicker` picks from the Miyoo MainUI's
  `<system>_cache6.db` SQLite caches, which our MainUI does not write, so it finds no games. It
  needs a fallback that scans the ROM folders (or the MainUI writing the caches).
- **MainUI**: the Miyoo screens are redrawn from the Onion theme files (`docs/rg35xx-mainui.md`).
  Still missing or unconfirmed: the box art position in game lists, the Miyoo Settings screen (the
  tab opens Tweaks), the empty list image, list marks and pop-up menus, theme background music.
- **GBA aspect**: Onion's `gpSP.cfg` sets Custom aspect plus `video_dingux_ipu_keep_aspect`, which only
  the Miyoo video driver reads; with `gl` it stretched to the 640x480 custom viewport. The RG35XX card
  gets its own `gpSP.cfg` (Core Provided, 3:2 at 640x427) and `gpSP.glslp` (sharp-bilinear-simple).
- **RApp packages** are not copied to the card by `dist.sh`, so the Expert tab is empty; standalone
  emulators are not built.
- The FAT partition always logs "Volume was not properly unmounted": kernel 3.10 does not clear the
  dirty flag on remount read-only. Real corruption shows as "invalid cluster chain" errors.

- **Arcade romsets**: MAME 2003-Plus wants the CPS2 sets with `.key` files and FB Alpha 2012 its
  own 0.2.97.24 sets; clones need the parent zip next to them. The test card's MAME 2003-Plus set
  runs `sf2`/`19xx` on FB Alpha, but not `sfa2` (no key) or the clone `ffightu` without `ffight`.
- **Per-core RetroArch overrides** from Onion that pick Miyoo drivers (`audio_driver = "sdl"`,
  `"oss"`, `sdl_dingux`) are stripped by `dist.sh`; other Miyoo-only keys
  (`video_dingux_ipu_keep_aspect`) are ignored by RetroArch here.

## Fixed, for the record

- Arcade (2026-10-30): MAME 2003-Plus, FB Alpha 2012 CPS-1, CPS-2 and Neo Geo built and on the card.
  Unthrottled at 1200 MHz: Final Fight (MAME) 116 fps, Street Fighter II (CPS-1) 160, 19XX (CPS-2)
  198, Metal Slug (Neo Geo) 185. cpuclock: CPS-1 840 (sf2 120 fps), CPS-2 1008 (only 19XX measured,
  147 at 840), Neo Geo 840 (Metal Slug 2 122 fps), MAME 1200 (default: hundreds of drivers). The game
  lists show arcade titles and hide the BIOS; the MAME tutorial video prompt is skipped (no ffplay).

- mGBA: the libretro fork has no `Makefile.libretro`; `cores.sh` builds it with CMake
  (`-DLIBMGBA_ONLY=ON -DBUILD_LIBRETRO=ON`, Buildroot's `toolchainfile.cmake`). It runs GBA when
  picked in the game list options and the Super Game Boy system. 1008 MHz: F-Zero Maximum Velocity
  87 fps unthrottled (102 at 1200, 74 at 840; gpSP 455 at 1200). Onion's `mGBA.cfg` forced the SDL
  audio driver and the Miyoo aspect; the RG35XX one is like gpSP's (core aspect, sharp-bilinear).

- `LD_PRELOAD=.../libpadsp.so` (Miyoo OSS-to-audioserver shim): the C programs use `PADSP_PRELOAD`
  (`src/common/utils/padsp.h`, empty off the Miyoo) and `dist.sh` drops the preload from the
  scripts it copies. The GLO and the runtime's `.miyoocmd` branch read the emulator path from the
  second word of `cmd_to_run.sh`, which only worked with the prefix (our MainUI never wrote it):
  they now take the first quoted field. `randomGamePicker` crashed with SIGFPE on `% 0` with no
  games (the Cortex-A9 divides in libgcc, which traps; the Miyoo's A7 divide returns 0).

- Battery shown empty for ~10 s after boot on battery power: the gauge reads 0 until about 5 s of
  uptime, batmon read that 0 on start, wrote `/tmp/percBat` = 0 and only checked again 15 s later.
  On the RG35XX it now leaves percBat unwritten and retries every second until the first real reading.

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
