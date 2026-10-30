# MainUI do RG35XX: as telas do Miyoo desenhadas a partir dos temas do Onion

No Miyoo Mini, a tela inicial com ícones, as grades e as listas de jogos são do MainUI **fechado** da
Miyoo (`/customer/app/MainUI`, na memória interna do aparelho). O Onion só o chama e o veste com os
temas. No RG35XX esse binário não existe e não pode ser redistribuído, e também não rodaria: ele usa
as bibliotecas `libmi_*` do chip SigmaStar para vídeo e áudio. Por isso `src/mainUI/` reimplementa as
telas, lendo os mesmos arquivos de tema (`skin/`, `config.json`, fontes, sons), para que qualquer tema
do Onion apareça igual. A tela é 640x480 nos dois aparelhos: os temas servem sem conversão.

## Referências usadas

Não existe documentação oficial de qual arquivo do skin aparece em qual tela. O guia do Onion
(onionui.github.io/docs/theme-design) e o repositório OnionUI/Themes só listam os nomes dos arquivos.
O que temos:

| Fonte | O que dá |
|---|---|
| `preview.png` de cada tema no OnionUI/Themes (480x360, captura da tela inicial no Miyoo, reduzida a 3/4) | posição das abas e das bolinhas com ícones de alturas diferentes (Analogue, Blueprint, ONION PS) |
| Tema Blueprint (Aemiii91), indicado no guia de temas como molde: cada imagem do skin tem o nome escrito e a borda desenhada | onde cada imagem fica na tela inicial |
| Guia de scraping do Onion (onionui.github.io/docs/advanced/scraping) | capas em `Roms/<sistema>/Imgs`, PNG de no máximo 250x360 |
| GIF de divulgação do Onion V4 (tela 320x240 dentro do GIF, recortada e ampliada para 640x480) | tela inicial com o tema Silky, medida pixel a pixel; Theme Switcher mostrando a tela inicial do ONION PS com 6 abas |
| Fotos de um review em vídeo (perspectiva corrigida) | aba Jogos (grade 4x2, número da página "1/2") e lista de ROMs (6 linhas, "111/205") |
| `src/common/system/state.h` do Onion (`write_mainui_state`) | itens por página em cada aba do MainUI do Miyoo: Recentes/Favoritos 6, Jogos 8, Expert 9, Apps 4; tela inicial com janela de 4 |
| OnionUI/Themes: `.github/schema.py` e `.github/whitelist*.txt` | formato completo do `config.json` e a lista oficial de arquivos do skin (Miyoo e extras do Onion) |
| README do tema ONION PS | `grid.font` é a fonte dos nomes dos consoles (o tema usa uma fonte vazia para escondê-los) |
| Tamanhos das imagens nos 24 temas da release 4.3.1 | encaixe de cada imagem (ver tabela abaixo) |

As imagens de referência não estão no repositório (são de terceiros).

## Telas e arquivos do tema

Legenda: **medido** = posição conferida contra captura do Miyoo; **confirmado** = layout confirmado
(foto ou `state.h`), posição aproximada; **deduzido** = só pelos tamanhos das imagens.

| Tela | Arquivos | Estado |
|---|---|---|
| Cabeçalho (todas) | `bg-title.png` 640x60; `miyoo-topbar.png` (logo, só na tela inicial); bateria `power-{0,20,50,80}%-icon.png`, `power-full-icon.png`, `ic-power-charge-100%.png`, texto `batteryPercentage`; título na fonte `title` | medido (renderizadores do Onion, `theme/render/header.h`) |
| Rodapé (todas) | `tips-bar-bg.png` 640x60; `icon-A-54.png`, `icon-B-54.png`, texto na fonte `hint`; contador nas cores `currentpage`/`total` | medido |
| Tela inicial | `background.png`; `ic-{recent,favorite,game,retroarch,app,setting}-{n,f}.png` 156x328 ou 156x278 (depende do tema); rótulo da aba na fonte `grid.font` tamanho `grid1x4`, cores `grid.color` / `grid.selectedcolor`; `dot-a.png` / `dot-n.png` | medido |
| Jogos | `bg-game-item-{n,f}.png` 154x170 em grade 4x2; ícone `icon`/`iconsel` do `Emu/*/config.json` (120x130 nos pacotes de ícones); nome do sistema na fonte `grid.font` | confirmado |
| Lista de ROMs, Recentes, Favoritos | `bg-list-s.png` 640x60 (linha selecionada), `div-line-h.png`, fonte `list`; capa em `preview-bg.png` 256x364 à direita | confirmado (sem capa); capa deduzida |
| Apps | `bg-list-l.png` 640x94, 4 por página; ícone 74x74 do `App/*/config.json`; `description` na segunda linha | 4 por página confirmado; desenho deduzido (é o mesmo do Tweaks) |
| Expert | `bg-ra-list-item.png` 214x120 em grade 3x3; logo do core 192x72 do `RApp/*/config.json` | 9 por página confirmado; desenho deduzido |
| Configurações do Miyoo | `icon-{brightness,language,device-info,key-setting,factory-reset}-48.png`, `icon-theme.png`, `icon-Shutdown.png`, `icon-{left,right}-arrow-24.png`; teste de botões: `bg-keysetting*.png`, `bg-io-testing.png`, `mask-*.png` | não implementado |
| Outros | `Empty.png`, `ic-favorite-mark.png`, `icon-folder.png`, `icon-game.png`, `list-num.png`, `num-bg.png`, `bg-pop-menu-{1..4}.png`, `pop-bg.png`, `bg-button-f.png`, `nav-bar-item-bg.png` | uso não confirmado, não usados |
| Sem uso no RG35XX | `icon-wifi-*.png`, `icon-hotspot-wifi.png`, `icon-setting-wifi.png` (sem wifi), `charging-0X.png` (carga é o `chargingState` do Onion) | — |

Os ícones dos pacotes `Icons/*` e da pasta `icons/` dos temas funcionam sem nada a mais: o Tweaks
reescreve os caminhos `icon`/`iconsel` nos `config.json`.

## O que está implementado (`src/mainUI/views.h`)

Coordenadas em pixels de 640x480 (multiplicadas por `g_scale`, que é 1 no RG35XX).

- **Tela inicial** (medida contra o Silky do GIF; diferença ≤ 2 px): abas Recentes (se
  `.showRecents`), Favoritos, Jogos, Expert (se `.showExpert`), Apps, Configurações, as mesmas opções
  do Tweaks. Blocos em `x = 10 + 154·i`, centralizados na vertical em `y = 230` (o Silky tem 328 de
  altura e fica em `y = 66`; Analogue, Blueprint e ONION PS têm 278 e ficam em `y = 91`), janela de 4
  que acompanha a seleção. Rótulo
  centralizado no bloco, centro em `y = 285`, com o texto do arquivo de idioma (o `hideLabels.icons`
  do Onion já apaga esses textos). Uma bolinha por aba, `y = 389`, passo igual à largura
  do `dot-a.png` + 5 px (29 no Silky, 18 no Analogue), começando em `320 − n·passo/2`. ←/→ trocam de aba, A abre, Configurações abre o Tweaks.
- **Jogos**: grade 4x2 a partir de `(12, 70)`, células de 154x170, ícone com centro a 70 px do topo da
  célula, nome com centro a 132 px, fonte `grid3x4`. O nome fica dentro da caixa do `bg-game-item-f.png` (no Silky
  a caixa ocupa os 153 px de cima da imagem de 170, o resto é sombra; com 150 o nome cortava a borda). Setas andam na grade, L1/R1 trocam de página;
  rodapé com página/total. Sistema sem ícone mostra só o nome no centro da célula.
- **Expert**: grade 3x3 de `RApp/*` a partir de `(−1, 60)`, células de 214x120 (cobrem exatamente
  640x360), `bg-ra-list-item.png` atrás do selecionado, logo centralizado; o nome só aparece se não
  houver logo. Abre a lista de ROMs do `rompath` do core.
- **Listas** (ROMs, Recentes, Favoritos): o renderizador de listas do Onion (`theme_renderList`), 6
  linhas, capa à direita quando existe `Imgs/<rom>.png`, contador item/total.
- **Apps**: lista grande do Onion, 4 linhas, ícone e descrição do `config.json`.
- Os ícones dos `config.json` ficam num cache de 24 entradas (`_view_icon`); as imagens do skin que os
  recursos comuns do Onion não cobrem (`ic-*`, `bg-game-item-*`, `bg-ra-list-item`) são carregadas
  uma vez.

Testado no aparelho (2026-10-30) com os temas Silky (padrão), ONION PS Purple Grey e Analogue; a tela
inicial do Analogue e do ONION PS confere com o `preview.png` dos temas.
No ONION PS os rótulos das abas vêm desenhados nas próprias imagens `ic-*.png` e a `grid.font` é vazia:
os nomes dos consoles somem na grade, como no Miyoo.

## O que falta

1. **Lista com capa**: posição da capa e largura da lista quando há imagem não conferidas (usa o
   `theme_renderList` do Onion: `preview-bg.png` encostado à direita, imagem centralizada em `y = 240`).
   O guia de scraping do Onion só dá o tamanho máximo da capa (250x360) e não mostra a lista. Precisa
   de uma captura do Miyoo com capa.
2. **Configurações do Miyoo**: a aba abre o Tweaks. A tela original (brilho, idioma, tema, informações,
   teclas, restaurar, desligar, com os ícones `icon-*-48.png`) não foi vista; o volume e o brilho já
   têm atalhos no keymon. Decidir se vale fazer uma tela própria com esses ícones.
3. **Expert sem testar com núcleos**: o `dist.sh` do RG35XX não copia pacotes `RApp`, então a grade
   aparece vazia. Posição e destaque da grade são dedução.
4. **Apps**: só o número de linhas é confirmado; o desenho é o do Tweaks.
5. **Lista vazia**: mostra uma linha "Empty"; o Miyoo provavelmente usa `Empty.png` (208x62), posição
   desconhecida.
6. **Marcas nas listas**: `ic-favorite-mark.png` (jogo favorito), `icon-folder.png`/`icon-game.png`
   (pasta/jogo) não são usados; não se sabe se e onde o Miyoo os mostra.
7. **Menus pop-up** (`bg-pop-menu-*`, `pop-bg.png`, `bg-button-f.png`): não se sabe o que os abre no
   Miyoo. O menu de opções do jogo do Onion (GLO, `script/game_list_options.sh`) já existe à parte.
8. **Música de fundo do tema** (`sound/bgm.mp3`): o MainUI não toca; o som de navegação
   (`sound/change.wav`) toca.
9. **Grade de Jogos**: posições tiradas de foto em perspectiva (±5 px) e o tamanho da fonte
   (`grid3x4`) é suposição.
10. **Navegação**: na tela inicial e nas grades ←/→ dão a volta no fim da lista (↑/↓ param na borda);
    o comportamento do Miyoo nesse ponto não foi visto. `SELECT` e `START` não fazem nada; `X` é
    do keymon (atalho do Tweaks, padrão Search, que não vai no cartão).
11. **Logo no canto superior esquerdo**: é a imagem `miyoo-topbar.png` de cada tema (no Silky e no
    ONION PS ela diz "onion"; no tema de fábrica da Miyoo dizia "MIYOO"). Não é texto do MainUI.
12. **Ícones de cabeçalho extras** (fone de ouvido `headphone-icon.png`): não mostrados.
13. **Y**: com `mainui_button_y = "glo"` (padrão do Onion, `config/keymap.json`) abre o menu de
    opções do jogo (GLO) como no Miyoo: o MainUI lança a entrada com `/tmp/launch_alt` e o runtime
    abre o `game_list_options.sh`; o MainUI grava `/tmp/state.json` (só o `type` da aba, uma chave
    por linha, que é o que o GLO lê). Sem essa opção, Y marca/desmarca favorito. Testado (2026-10-30).
14. **Nomes de arcade** (`"shortname": 1`): o Miyoo traduz pela `libgamename`, que lê
    `BIOS/arcade_lists/arcade-rom-names.txt` (ordenado, `rom "Título"`); o MainUI lê o mesmo
    arquivo (busca binária) e o `dist.sh` copia só essa pasta do `static/dist/BIOS`. Com
    `"hidebios": 1` some `neogeo` e todo título que diz BIOS (a lista exata do Miyoo não é conhecida).
    Testado (2026-10-30).

Novas capturas do Onion no Miyoo (vídeos, fóruns) podem fechar os itens 1, 2, 5, 6, 7 e 9.

## Temas e idiomas

- `platform/rg35xx/tools/get_themes.sh` baixa o Silky by DiMo e os 15 temas da lista "remixed" do
  OnionUI/Themes (`.github/data/remixed.txt`, ~75 MB) para `cache/rg35xx/themes`; o `dist.sh` põe o
  Silky extraído em `Themes/` (é o tema padrão; escolhê-lo no Theme Switcher volta ao padrão) e os
  outros zipados. Com os 202 temas do repositório (~1 GB) o Theme Switcher levava minutos para abrir
  (extrai os previews com `7z` e carrega todos).
- Idioma: no Miyoo era escolhido nas Configurações de fábrica. Aqui fica em Tweaks → System →
  Language, que lista `miyoo/app/lang/*.lang` pelo nome em inglês e grava o `language` do
  `system.json`; o MainUI usa o novo idioma ao voltar do Tweaks. O `lt.lang` do Onion tem JSON
  inválido e não aparece.

## Como testar sem mexer no aparelho

Com o aparelho rodando o Onion e ADB pela porta OTG:

```sh
platform/rg35xx/tools/make.sh "make -C src/mainUI BUILD_DIR=/src/cache/rg35xx/dev"
adb push cache/rg35xx/dev/MainUI /mnt/mmc/miyoo/app/MainUI.new
adb shell /system/bin/busybox mv -f /mnt/mmc/miyoo/app/MainUI.new /mnt/mmc/miyoo/app/MainUI
platform/rg35xx/tools/devchroot.sh shell 'kill $(pidof MainUI)'   # o runtime.sh abre o novo
platform/rg35xx/tools/presskeys.sh right a      # aperta botões (padmapd/uinput)
platform/rg35xx/tools/screenshot.sh /tmp/x.png  # captura o framebuffer
```

O MainUI salva a posição em `/tmp/mainui_state` e reabre onde estava, então uma sequência de botões
depende da tela em que ele parou. Para testar outro tema, copie-o para `/mnt/mmc/Themes/` e troque o
`"theme"` do `/mnt/mmc/system.json` (guarde o original), depois reinicie o MainUI.
