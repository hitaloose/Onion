// MainUI: Onion's own launcher, for devices without the closed Miyoo MainUI.
// It speaks the same contract with runtime.sh: the chosen game or app goes to
// /tmp/cmd_to_run.sh as `"<launch.sh>" "<rom>"`, launched ROMs go on top of
// Roms/recentlist.json and Y toggles Roms/favourite.json, both in the Miyoo
// one-JSON-object-per-line format that GameSwitcher reads.

#include <SDL/SDL.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "components/list.h"
#include "system/battery.h"
#include "system/display.h"
#include "system/keymap_sw.h"
#include "system/lang.h"
#include "system/settings.h"
#include "theme/background.h"
#include "theme/sound.h"
#include "theme/theme.h"
#include "utils/keystate.h"
#include "utils/log.h"
#include "utils/sdl_init.h"

#include "./library.h"
#include "./menu.h"

#define FRAMES_PER_SECOND 60
#define VISIBLE_ROWS 6
#define MAX_DEPTH 8
#define CMD_PATH "/tmp/cmd_to_run.sh"
#define STATE_PATH "/tmp/mainui_state"
#define TWEAKS_APP APP_DIR "/Tweaks"

static bool quit = false;
static KeyState keystate[320] = {(KeyState)0};

static Menu stack[MAX_DEPTH];
static int depth = 0;

static void sigHandler(int sig)
{
    if (sig == SIGINT || sig == SIGTERM)
        quit = true;
}

static Menu *current(void) { return &stack[depth]; }

static void fill_menu(Menu *menu)
{
    switch (menu->kind) {
    case MENU_MAIN:
        library_loadGames(RECENTLIST_PATH);
        if (game_count > 0)
            menu_add(menu, ENTRY_MENU, lang_get(LANG_RECENTS_TAB, LANG_FALLBACK_RECENTS_TAB), NULL, MENU_RECENTS);
        menu_add(menu, ENTRY_MENU, lang_get(LANG_FAVORITES_TAB, LANG_FALLBACK_FAVORITES_TAB), NULL, MENU_FAVORITES);
        menu_add(menu, ENTRY_MENU, lang_get(LANG_GAMES_TAB, LANG_FALLBACK_GAMES_TAB), NULL, MENU_SYSTEMS);
        menu_add(menu, ENTRY_MENU, lang_get(LANG_APPS_TAB, LANG_FALLBACK_APPS_TAB), NULL, MENU_APPS);
        if (is_dir(TWEAKS_APP))
            menu_add(menu, ENTRY_APP, lang_get(LANG_SETTINGS_TAB, "Settings"), TWEAKS_APP, 0);
        break;
    case MENU_RECENTS:
        library_fillGames(menu, RECENTLIST_PATH);
        break;
    case MENU_FAVORITES:
        library_fillGames(menu, FAVORITES_PATH);
        break;
    case MENU_SYSTEMS:
        library_loadSystems();
        library_fillSystems(menu);
        break;
    case MENU_ROMS:
        library_fillRoms(menu);
        break;
    case MENU_APPS:
        library_fillApps(menu);
        break;
    }

    if (menu->count == 0)
        menu_add(menu, ENTRY_EMPTY, "Empty", NULL, 0);
}

static void push_menu(MenuKind kind, const char *title, int system, const char *folder)
{
    if (depth + 1 >= MAX_DEPTH)
        return;
    Menu *menu = &stack[++depth];
    menu_init(menu, kind, title);
    menu->system = system;
    strncpy(menu->folder, folder ? folder : "", STR_MAX - 1);
    fill_menu(menu);
}

static void pop_menu(void)
{
    if (depth == 0)
        return;
    menu_free(&stack[depth--]);

    // Favorites may have changed in a submenu
    if (current()->kind == MENU_FAVORITES) {
        int active = current()->active, top = current()->top;
        menu_free(current());
        fill_menu(current());
        current()->active = active < current()->count ? active : current()->count - 1;
        current()->top = top <= current()->active ? top : current()->active;
    }
}

static void save_state(void)
{
    FILE *fp = fopen(STATE_PATH, "w");
    if (!fp)
        return;
    for (int i = 0; i <= depth; i++)
        fprintf(fp, "%d %d\n", stack[i].active, stack[i].top);
    fclose(fp);
}

static void write_command(const char *launch, const char *rompath)
{
    FILE *fp = fopen(CMD_PATH, "w");
    if (!fp)
        return;
    if (rompath)
        fprintf(fp, "\"%s\" \"%s\"\n", launch, rompath);
    else
        fprintf(fp, "\"%s\"\n", launch);
    fflush(fp);
    fsync(fileno(fp));
    fclose(fp);
}

static void launch_game(JsonGameEntry *entry)
{
    library_updateGameList(RECENTLIST_PATH, entry, true, MAX_RECENTS);
    write_command(entry->launch, entry->rompath);
    save_state();
    quit = true;
}

static void launch_app(const char *app_dir)
{
    char launch[STR_MAX * 2];
    snprintf(launch, sizeof(launch), "%s/launch.sh", app_dir);
    write_command(launch, NULL);
    save_state();
    quit = true;
}

// Opens the selected entry; with `launch` false only submenus are opened
static void activate(bool launch)
{
    Menu *menu = current();
    MenuEntry *entry = menu_current(menu);
    char title[STR_MAX * 2];

    if (!entry)
        return;

    switch (entry->type) {
    case ENTRY_MENU:
        push_menu((MenuKind)entry->index, entry->label, 0, NULL);
        break;
    case ENTRY_SYSTEM:
        push_menu(MENU_ROMS, entry->label, entry->index, NULL);
        break;
    case ENTRY_FOLDER:
        snprintf(title, STR_MAX, "%s", entry->label);
        push_menu(MENU_ROMS, title, menu->system, entry->path);
        break;
    case ENTRY_ROM:
        if (launch) {
            JsonGameEntry game = library_romEntry(menu, entry);
            launch_game(&game);
        }
        break;
    case ENTRY_GAME:
        if (launch)
            launch_game(&games[entry->index]);
        break;
    case ENTRY_APP:
        if (launch)
            launch_app(entry->path);
        break;
    case ENTRY_EMPTY:
        break;
    }
}

// Reopens the menus and selections saved before the last launch
static void restore_state(void)
{
    FILE *fp = fopen(STATE_PATH, "r");
    int active, top, level = 0;

    if (!fp)
        return;
    while (fscanf(fp, "%d %d", &active, &top) == 2) {
        if (level > 0) {
            int before = depth;
            activate(false);
            if (depth == before)
                break; // the saved entry no longer opens a menu
        }
        Menu *menu = current();
        if (active >= 0 && active < menu->count) {
            menu->active = active;
            menu->top = top >= 0 && top <= active ? top : active;
            if (menu->active >= menu->top + VISIBLE_ROWS)
                menu->top = menu->active - VISIBLE_ROWS + 1;
        }
        level++;
    }
    fclose(fp);
}

static void toggle_favorite(void)
{
    Menu *menu = current();
    MenuEntry *entry = menu_current(menu);
    JsonGameEntry game;

    if (!entry)
        return;
    if (entry->type == ENTRY_ROM)
        game = library_romEntry(menu, entry);
    else if (entry->type == ENTRY_GAME)
        game = games[entry->index];
    else
        return;

    bool is_favorite = library_isFavorite(game.rompath);
    library_updateGameList(FAVORITES_PATH, &game, !is_favorite, 0);

    if (menu->kind == MENU_FAVORITES) {
        int active = menu->active;
        menu_free(menu);
        fill_menu(menu);
        menu->active = active < menu->count ? active : menu->count - 1;
        menu->top = 0;
        menu_move(menu, 0, VISIBLE_ROWS);
    }
}

static const char *preview_path(void)
{
    static char path[STR_MAX];
    Menu *menu = current();
    MenuEntry *entry = menu_current(menu);

    if (!entry)
        return NULL;
    if (entry->type == ENTRY_ROM) {
        JsonGameEntry game = library_romEntry(menu, entry);
        strncpy(path, game.imgpath, STR_MAX - 1);
    }
    else if (entry->type == ENTRY_GAME)
        strncpy(path, games[entry->index].imgpath, STR_MAX - 1);
    else
        return NULL;
    return strlen(path) > 0 ? path : NULL;
}

int main(int argc, char *argv[])
{
    log_setName("MainUI");
    signal(SIGINT, sigHandler);
    signal(SIGTERM, sigHandler);

    SDL_InitDefault();
    settings_load();
    lang_load();

    // A command left from before means the runtime has not consumed it yet
    remove(CMD_PATH);

    menu_init(&stack[0], MENU_MAIN, "Onion");
    fill_menu(&stack[0]);
    restore_state();

    List view = list_create(VISIBLE_ROWS, LIST_SMALL);
    int battery_percentage = battery_getPercentage();
    bool changed = true;
    SDLKey changed_key = SDLK_UNKNOWN;

    uint32_t acc_ticks = 0, last_ticks = SDL_GetTicks(),
             time_step = 1000 / FRAMES_PER_SECOND;

    while (!quit) {
        uint32_t ticks = SDL_GetTicks();
        acc_ticks += ticks - last_ticks;
        last_ticks = ticks;

        if (updateKeystate(keystate, &quit, true, &changed_key)) {
            Menu *menu = current();
            int before = menu->active, depth_before = depth;

            if (keystate[SW_BTN_UP] >= PRESSED)
                menu_move(menu, -1, VISIBLE_ROWS);
            else if (keystate[SW_BTN_DOWN] >= PRESSED)
                menu_move(menu, 1, VISIBLE_ROWS);
            else if (keystate[SW_BTN_LEFT] >= PRESSED || keystate[SW_BTN_L1] >= PRESSED)
                menu_move(menu, -VISIBLE_ROWS, VISIBLE_ROWS);
            else if (keystate[SW_BTN_RIGHT] >= PRESSED || keystate[SW_BTN_R1] >= PRESSED)
                menu_move(menu, VISIBLE_ROWS, VISIBLE_ROWS);
            else if (changed_key == SW_BTN_A && keystate[SW_BTN_A] == PRESSED) {
                sound_change();
                activate(true);
                changed = true;
            }
            else if (changed_key == SW_BTN_B && keystate[SW_BTN_B] == PRESSED) {
                if (depth > 0) {
                    sound_change();
                    pop_menu();
                    changed = true;
                }
            }
            else if (changed_key == SW_BTN_Y && keystate[SW_BTN_Y] == PRESSED) {
                sound_change();
                toggle_favorite();
                changed = true;
            }

            if (current()->active != before && depth == depth_before) {
                sound_change();
                changed = true;
            }
        }

        if (battery_hasChanged(ticks, &battery_percentage))
            changed = true;

        if (quit)
            break;

        if (acc_ticks >= time_step) {
            if (changed) {
                Menu *menu = current();
                menu_fillView(menu, &view, VISIBLE_ROWS, preview_path());

                theme_renderHeader(screen, menu->title, false);
                theme_renderList(screen, &view);
                theme_renderFooter(screen);
                theme_renderStandardHint(screen, lang_get(LANG_SELECT, LANG_FALLBACK_SELECT),
                                         depth > 0 ? lang_get(LANG_BACK, LANG_FALLBACK_BACK) : NULL);
                theme_renderFooterStatus(screen, menu->active + 1, menu->count);
                theme_renderHeaderBattery(screen, battery_percentage);

                SDL_BlitSurface(screen, NULL, video, NULL);
                SDL_Flip(video);
                changed = false;
            }
            acc_ticks -= time_step;
        }

        SDL_Delay(5);
    }

    // Leave the screen black for the game or app being launched
    SDL_FillRect(video, NULL, 0);
    SDL_Flip(video);

    menu_fillView(current(), &view, 0, NULL);
    list_free(&view);
    while (depth >= 0)
        menu_free(&stack[depth--]);
    free(games);

    Mix_CloseAudio();
    lang_free();
    resources_free();
    SDL_FreeSurface(screen);
    SDL_FreeSurface(video);
    SDL_Quit();

    return EXIT_SUCCESS;
}
