# Port do Onion OS para o Anbernic RG35XX (original): passagem de contexto

Documento para continuar o trabalho em outra sessão. Complementa:
- `docs/rg35xx-hardware.md`: fatos de hardware medidos no aparelho
- `docs/rg35xx-status.md`: pendências conhecidas
- `docs/rg35xx-kernel.md`: kernel próprio (fase G1)

Repositório: fork `git@hitaloose.github.com:hitaloose/Onion.git` (remote `origin`), branch
`rg35xx-port`. `upstream` = OnionUI/Onion (só para puxar atualizações; nada é enviado para lá).
O dono do projeto pretende mantê-lo como port no próprio GitHub, sem mandar PR ao Onion.

## 1. Estado atual (funciona no aparelho)

- Boot real: `misc/dmenu.bin` próprio → monta `.tmp_update/rootfs.img` (Buildroot, glibc armhf,
  **somente leitura** via `loopro`, tmpfs em /tmp, /run, /appconfigs) → chroot → `updater` →
  `runtime.sh` do Onion. O `dmenu.bin` original fica em `misc/dmenu.bin.stock`.
- MainUI próprio (`src/mainUI/`), temas do Onion, GameSwitcher (overlay com captura da tela),
  keymon (volume, brilho, suspensão, MENU curto/longo/duplo), batmon, desligamento limpo.
- RetroArch 1.22.2 + patches do Onion + patches RG35XX; 9 cores compilados; 14 sistemas.
- Performance (benchmark sem limite de FPS, CPU limitada a 1,2 GHz, com USB): SNES 84, GB 218,
  GBA 126, NES 131, MD 167, PCE 146 FPS.

**Ponto crítico (resolvido em parte pela G1):** o port rodava sobre o **kernel do Garlic** (o `misc` veio da imagem do
Garlic gravada no cartão: `uImage`, `kernel.dtb`, módulos `gpio_keys_polled.ko` "RG35XX Gamepad"
do Black-Seraph, etc.). Não podemos redistribuí-lo. Um release público exige kernel próprio ou
compatibilidade com o kernel de fábrica da Anbernic. Ver o plano da GPU (seção 6), que resolve isso.

## 2. Arquitetura e arquivos principais

| Caminho | Papel |
|---|---|
| `platform/rg35xx/misc/dmenu.bin` | Hook de boot: CPU, I/O, ADB, troca `rootfs.img.new`, `loopro`, chroot, `trap '' HUP` |
| `platform/rg35xx/buildroot/` | BR2_EXTERNAL: defconfig, `board/` (overlay, post-build, busybox.fragment), `package/onion-customer` (fontes + jsonval), `patches/sdl/0100..0103` |
| `platform/rg35xx/buildroot/build.sh` | Build do rootfs e SDK em Docker (volume `onion-rg35xx-br`, FS case-sensitive) |
| `platform/rg35xx/toolchain/Dockerfile` | Imagem `rg35xx-toolchain` |
| `platform/rg35xx/tools/make.sh` | Roda comandos com o SDK (`PLATFORM=rg35xx`, `CROSS_COMPILE`) |
| `platform/rg35xx/dist.sh` | Compila tudo e monta o cartão em `cache/rg35xx/sd` (remove todo ELF do Miyoo de `static/`) |
| `platform/rg35xx/retroarch/build.sh`, `cores.sh`, `patches/`, `autoconfig/` | RetroArch e cores |
| `platform/rg35xx/tools/devchroot.sh`, `cleanreboot.sh` | Desenvolvimento via ADB |
| `platform/rg35xx/tools/{padmapd,vtreset,loopro}` | `src/padmapd/padmapd.c`, `tools/vtreset.c`, `tools/loopro.c` |
| `src/padmapd/` | Gamepad → teclado uinput com keycodes do Miyoo (`/dev/input/onion-kbd`), repetição 250/50 ms |
| `src/mainUI/` | Launcher (menu.h com lista "em janela": um ListItem tem ~31 KB) |
| `static/build/.tmp_update/runtime.sh` | Ramos `is_rg35xx` (detecção pelo `/sys/class/power_supply/atc260x-usb`) |
| `static/build/.tmp_update/res/rg35xx_retroarch.cfg` | Drivers, 320x240, binds do joystick RG35XX (aplicado por `force_retroarch_cfg`) |
| `src/common/config.mk` | `PLATFORM_RG35XX`, `PLATFORM_DEVICE`, flags Cortex-A9 |

Convenções: `PLATFORM_DEVICE` = hardware real (miyoomini e rg35xx); `PLATFORM_MIYOOMINI` só para o
que é exclusivo do Miyoo (shm com o MainUI fechado, render direto no fb com rotação).
`DEVICE_ID == RG35XX (35)`; `DISPLAY_ROTATED_180` (true só no Miyoo); `display_fbColor()` converte
ARGB ↔ ABGR (owlfb tem o vermelho no byte baixo); `HW_INPUT_DEVICE`.

## 3. Como compilar e implantar

```sh
platform/rg35xx/buildroot/build.sh          # rootfs + SDK (1ª vez ~30 min; depois incremental)
platform/rg35xx/retroarch/build.sh [clean]  # RetroArch
platform/rg35xx/retroarch/cores.sh [core..] # cores (flags pelo ambiente, nunca CFLAGS= no make)
platform/rg35xx/dist.sh                     # programas do Onion + cartão em cache/rg35xx/sd
```

Mudou um pacote do Buildroot? `make <pkg>-dirclean` dentro do volume antes do `build.sh`.

Implantar com o aparelho rodando o Onion (ADB habilitado por `/misc/enableADB`):
- **Nunca sobrescrever arquivo em uso.** Use `adb push x arquivo.new` + `busybox mv -f` no aparelho
  (vale para `runtime.sh`, `MainUI`, `retroarch`). O `rootfs.img` vai como `rootfs.img.new` e o
  `dmenu.bin` troca no próximo boot.
- Reinício: `platform/rg35xx/tools/devchroot.sh reboot` (usa `cleanreboot.sh`: para o Onion,
  `swapoff`, desmonta o chroot, remonta o cartão somente leitura e `reboot -f`). **Nunca `adb reboot`.**
- `misc` é montado read-only: `mount -o remount,rw /misc` para gravar e voltar para `ro`.
- Rodar programa SDL de teste: `devchroot.sh run <cmd>` (desacoplado do pty do adb).

## 4. Armadilhas já descobertas (não repetir)

- Shell do stock (Android): sem `head`/`printf`/`cut`; o `toolbox mv` não aceita opções; busybox
  antigo em `/system/bin/busybox` (1.14, sem `losetup -r`, sem `pkill`).
- `adb shell` sempre retorna 0; processos em background morrem quando a sessão adb termina (use
  `& wait $!`, ou mande o runtime executar via `/onion/tmp/cmd_to_run.sh`).
- `kill` por padrão de nome: o `grep -E "MainUI|..."` casa com a linha de comando do próprio shell
  do adb. Use script em `/tmp` com `[ "$p" = "$$" ]`, ou PIDs.
- ADB "cai" quando o cabo está na porta DC; precisa ser a porta OTG.
- Cartão FAT: "Volume was not properly unmounted" sempre aparece (kernel 3.10 não limpa a marca
  no remount ro). Corrupção real aparece como "invalid cluster chain". A causa da corrupção era o
  rootfs montado rw a partir do cartão (impedia remount ro) e `reboot -f` sem sync.
- `ondemand` fica preso em 240 MHz; usar `interactive`. A 1,488 GHz com USB carregando o aparelho
  travou 3 vezes; teto atual 1,2 GHz.
- SDL 1.2 fbcon: patches em `buildroot/patches/sdl/`: ficar no VT ativo sem `VT_LOCKSWITCH`
  (0100), `SDL_FBCON_NOKEYBOARD` (0101, usado pelo runtime para jogos), não restaurar modo do VT
  (0102) nem modo de vídeo (0103) ao sair. Sem isso: telas pretas travadas em `VT_WAITACTIVE`,
  teclas divididas entre GameSwitcher e RetroArch, MainUI sem teclado/ampliado.
- `owldss`: `overlay0` aceita `input_size` ≠ `output_size` → escala por hardware. RetroArch desenha
  320x240 e o hardware amplia 2x. O RetroArch confere o modo do fb a cada quadro (patch 0002).
- RetroArch genérico lia `~/.config/retroarch`: patches 0001/0003 usam `$HOME/.retroarch`.
- Binds do `retroarch.cfg` do Onion usam a numeração do Miyoo; `rg35xx_retroarch.cfg` traduz.
- `keymon`: o "segurar MENU" funciona (~0,7 s). `quietMainUI` (injeta L1) foi desligado no RG35XX.
- `libpng`: headers do Onion são 1.2; no RG35XX `png/png.h` aponta para o 1.6 do sysroot.

## 5. Pendências (ver também `docs/rg35xx-status.md`)

1. Kernel próprio / independência do kernel do Garlic (resolvido pelo plano G1 abaixo).
2. Bateria mostra 0% nos primeiros ~10 s no MainUI.
3. 60 Hz (painel em 56,4 Hz; pixclock ~47026 ps daria 60 Hz) e vsync (`OWLFB_WAITFORVSYNC`;
   referência open source: MinUI do shauninman, plataforma rg35xx).
4. Clock por sistema e `target_loads` do `interactive` (bateria), vídeo em thread para PS1.
5. `LD_PRELOAD=libpadsp.so` fixo em GameSwitcher/state.h/apps.h/randomGamePicker e scripts.
6. mGBA (fork libretro usa CMake), aba Expert/RApp, mais sistemas/cores, instalador (Fase 6),
   logo de boot do Onion no `misc`, HDMI, segundo cartão, Tweaks sem opções do Miyoo.

## 6. Plano da GPU (PowerVR SGX544MP1): próximo trabalho

Decisão: implementar **no port**, não no Garlic (Garlic é fechado, uClibc softfp ARMv5, não
redistribuível; nosso rootfs é glibc armhf, a mesma ABI das bibliotecas da GPU).

Peças verificadas (todas na org GitHub `rg35xx-cfw`):

| Peça | Fonte | Versão |
|---|---|---|
| Kernel S500 para o RG35XX | `rg35xx-cfw/s500_kernel`, branch `rg35xx` (fork de `aron0330/s500_kernel`; base do Koriki) | Linux 3.10 |
| Driver de kernel da GPU (KM) | `drivers/gpu/pvr/eurasia_km_linux` nesse kernel | **1.14@3625561** "SGX_DDK_Linux_XOrg" |
| Bibliotecas UM | `rg35xx-cfw/cubieboard6_ubuntu_gpu_drivers`, pasta `v2/` (Cubieboard 6 = Actions S500) | **1.14@3625561** (confirmado em `libIMGegl.so`/`libGLESv2.so`) |

- As bibliotecas incluem `libsrv_um`, `libsrv_init`, `libIMGegl`, `libEGL`, `libGLES_CM`,
  `libGLESv2`, `libusc`, `libglslcompiler`, `libpvr2d`, WSEGLs (**`libpvrPVR2D_LINUXFBWSEGL.so`**
  para EGL direto no framebuffer, FLIP, FRONT, BLIT, DRI), `pvrsrvctl`, testes (`gltest2`,
  `sgx_blit_test`, `services_test`), `powervr.ini`, `install.sh`, headers EGL/GLES.
  Dependências: `libdl`, `libpthread`, `librt`, **`libdrm`**, `libgcc_s`, glibc ≥ 2.4, hard-float
  (`Tag_ABI_VFP_args: VFP registers`, VFPv3-D16).
- Licença (`TERMS_OF_USE.TXT`, Actions Zhuhai): "a limited, worldwide, non-exclusive right and
  license to modify, reproduce, distribute or publish the software" → redistribuível no release.
- O `pvrsrvkm.ko` que vem com o Garlic é **1.12@3052601 Android** ("owl_android"), incompatível
  com essas bibliotecas. As `libEGL`/`libGLES` do rootfs do Garlic são **Mesa** (X11/DRI), ou seja,
  o Garlic não usa a GPU.
- A org também tem `u-boot_s500`, `buildroot_s500`, `rg35xx-batocera-toolchain`, `Koriki` e um
  fork do `batocera.linux` (o branch padrão não tem board RG35XX; procurar em outros branches).
- O Knulli lista o RG35XX original como "não suportado"; o kernel 4.9.170 citado lá é do BSP da
  Allwinner (RG35XX H/Plus/SP), não deste aparelho.
- Post não lido (Ko-fi bloqueia acesso automático): "RG35XX: Getting the GPU to work, Part 1",
  https://ko-fi.com/post/RG35XX-Getting-the-GPU-to-work-Part-1-T6T0JCHY2 — pedir ao usuário
  para colar o texto se precisar.

Fases:
- **G1 — kernel próprio: feito e testado no aparelho (2026-10-26).** Detalhes, decisões e
  pendências em `docs/rg35xx-kernel.md`. `platform/rg35xx/kernel/build.sh` (GCC 4.7.3 do
  Ubuntu 14.04) gera `uImage`, `kernel.dtb`, `dtbs/` e `modules/` em `cache/rg35xx/kernel/`. DTS =
  `kernel.dtb` do Garlic decompilado + gamepad + alto-falante ativo em nível alto. Driver próprio
  `rg35xx-gamepad`. Tabela de CPU do Garlic (patch 0003). ADB exige `USB_G_ANDROID=y` e
  `USB_PLATFORM_LINUX` desligado. Volume pelo ALSA `DAC PA Volume`; framebuffer ARGB (o
  `display_fbColor()` lê o layout). Pendências: reboot às vezes desliga, ADB demora a aparecer,
  texto "android" no boot, fone de ouvido não testado.
- **G2 — GPU acordando: feito (2026-10-26).** OpenGL ES 2.0 desenha no painel (`egltest`, ~789 fps
  sem vsync). Pacote Buildroot `powervr-sgx-um` + `libdrm`; WSEGL **FLIP** (a LINUXFB recusa o
  display); `dmenu.bin` carrega `drm.ko`/`pvrsrvkm.ko` do `misc` e roda `pvrsrvctl`. GPU a 300 MHz por
  enquanto. Detalhes em `docs/rg35xx-kernel.md`, seção GPU.
- **G3 — RetroArch GLES2: feito (2026-10-26).** Driver `gl` com contexto `mali_fbdev` (patch 0004:
  janela nativa NULL), ativado pelo `runtime.sh` quando `/dev/dri/card0` existe (senão `sdl`).
  Shaders GLSL funcionam (crt-pi 81 fps, lcd3x 74 fps no SNES). Vsync não tem efeito (FLIP); o ritmo
  vem do áudio. Shaders ainda não vão no cartão. Detalhes em `docs/rg35xx-kernel.md`, seção GPU.
- **G4 — 3D:** `mupen64plus-next` (GLES2), `ppsspp`. Expectativa: N64 jogável em parte do catálogo,
  PSP só jogos leves.

## 7. Aparelho de teste (estado no fim desta sessão)

- Cartão com imagem do Garlic regravada (partição de ROMs só 3,2 GB), Onion instalado por cima,
  `misc/dmenu.bin` = o nosso, `misc/dmenu.bin.stock` = o do Garlic, `misc/enableADB` presente.
- ROMs de teste em `Roms/{GB,GBC,GBA,FC,SFC,MD,PCE,GG}` (o de GG e dois `.7z` em Downloads são
  páginas HTML, downloads falhos). BIOS do PS1 em `BIOS/`.
- `.tmp_update/config/.logging` ligado (log em `.tmp_update/logs/runtime.log`).
- Para voltar ao Garlic: no `misc`, apagar `dmenu.bin` e renomear `dmenu.bin.stock` para `dmenu.bin`.
