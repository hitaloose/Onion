# Onion for the Anbernic RG35XX (original)

An unofficial port of [Onion OS](https://github.com/OnionUI/Onion) to the **original Anbernic
RG35XX** (2022, Actions ATM7039S / S500, PowerVR SGX544 GPU, 640x480 screen). It is not for the
RG35XX Plus, H, SP or 2024, which use different hardware.

> **Work in progress: there is no release yet.** The port runs on the developer's device, but there
> is no image to install and the steps in `docs/` assume a development setup over ADB.

## About this fork

- **Independent project.** This repository is a separate fork maintained on its own. It is not
  affiliated with, endorsed by or supported by the OnionUI team, and there are no plans to send
  pull requests to [OnionUI/Onion](https://github.com/OnionUI/Onion): the changes here are made for
  the RG35XX and are not meant to be merged upstream.
- **Please do not report RG35XX problems to OnionUI.** Use this repository's issues instead.
- **Upstream updates** from OnionUI/Onion are pulled in from time to time; nothing is pushed back.
- **Branches.** Development happens on `main` until the first release. Releases will be tagged on
  `main`.
- **Miyoo Mini.** This fork targets the RG35XX only. For the Miyoo Mini and Mini+, use the
  [official Onion](https://github.com/OnionUI/Onion).

## What works

- Boots from the stock Anbernic bootloader and U-Boot into its own Linux 3.10 kernel
  ([rg35xx-cfw/s500_kernel](https://github.com/rg35xx-cfw/s500_kernel) with patches), its own boot
  ramdisk and a read-only Buildroot root filesystem, then Onion's `runtime.sh`. Nothing from other
  custom firmwares is needed.
- Onion's main menu, redrawn from the Onion theme files (home tabs, Games grid, Expert grid, lists,
  Apps), GameSwitcher, Tweaks, Theme Switcher, Activity Tracker, auto-save and resume, MENU
  shortcuts (short, long and double press).
- RetroArch 1.22.2 with Onion's patches, drawing with OpenGL ES 2 on the GPU (shaders included),
  vsync at the panel's 60.46 Hz, and CPU clock per core from Onion's `cpuclock.txt`.
- 15 systems: GB, GBC, GBA (gpSP, or mGBA per game), Super Game Boy, NES, FDS, SNES, Mega Drive,
  Master System, Game Gear, Sega CD, 32X, PlayStation, PC Engine and PC Engine CD.
- Battery level, charging screen, low-battery shutdown, earphone jack, volume and brightness keys,
  sleep, clean shutdown.

Known gaps and the plan are in [`docs/rg35xx-status.md`](docs/rg35xx-status.md) and
[`docs/rg35xx-handoff.md`](docs/rg35xx-handoff.md). Next steps include the first installable image,
more systems and cores, and N64/PSP on the GPU.

## Building

Everything builds in Docker (Buildroot, toolchains, kernel), from the repository root:

```sh
platform/rg35xx/buildroot/build.sh          # root filesystem and SDK (first run ~30 min)
platform/rg35xx/kernel/build.sh             # kernel, device tree and modules
platform/rg35xx/retroarch/build.sh          # RetroArch
platform/rg35xx/retroarch/cores.sh          # libretro cores
platform/rg35xx/dist.sh                     # Onion programs and the SD card in cache/rg35xx/sd
```

## Documentation

| File | Contents |
|---|---|
| [`docs/rg35xx-handoff.md`](docs/rg35xx-handoff.md) | Architecture, build and deploy, pitfalls, open items (Portuguese) |
| [`docs/rg35xx-status.md`](docs/rg35xx-status.md) | Open issues and fixes |
| [`docs/rg35xx-hardware.md`](docs/rg35xx-hardware.md) | Hardware facts measured on the device |
| [`docs/rg35xx-kernel.md`](docs/rg35xx-kernel.md) | Kernel, device tree, boot ramdisk, GPU |
| [`docs/rg35xx-mainui.md`](docs/rg35xx-mainui.md) | Main menu screens and theme files |

## Credits and license

- [Onion OS](https://github.com/OnionUI/Onion) by the OnionUI team and contributors: everything
  this port builds on. Onion is GPL-3.0, and so is this fork (see [`LICENSE`](LICENSE)).
- [RetroArch](https://github.com/libretro/RetroArch) and the [libretro](https://github.com/libretro)
  cores, under their own licenses.
- Kernel sources from [rg35xx-cfw/s500_kernel](https://github.com/rg35xx-cfw/s500_kernel) (Actions
  S500 BSP), GPL-2.0.
- Themes by their authors, from [OnionUI/Themes](https://github.com/OnionUI/Themes).
- Icons by [Icons8](https://icons8.com).
