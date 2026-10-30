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
#include "utils/flags.h"
#include "utils/keystate.h"
#include "utils/log.h"
#include "utils/sdl_init.h"

#include "./library.h"
#include "./menu.h"
#include "./views.h"

#define FRAMES_PER_SECOND 60
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

static void move(Menu *menu, int delta)
{
    menu_move(menu, delta, view_visible(menu->kind), view_isPaged(menu->kind));
}

static void add_tab(Menu *menu, MenuKind kind)
{
    menu_add(menu, ENTRY_MENU, _tab_label(kind), NULL, kind);
}

static void fill_menu(Menu *menu)
{
    switch (menu->kind) {
    case MENU_MAIN:
        // The tabs of the Miyoo MainUI, Recents and Expert as set in Tweaks
        if (settings.show_recents)
            add_tab(menu, MENU_RECENTS);
        add_tab(menu, MENU_FAVORITES);
        add_tab(menu, MENU_SYSTEMS);
        if (settings.show_expert)
            add_tab(menu, MENU_EXPERT);
        add_tab(menu, MENU_APPS);
        add_tab(menu, MENU_SETTINGS);
        break;
    case MENU_RECENTS:
        library_fillGames(menu, RECENTLIST_PATH);
        break;
    case MENU_FAVORITES:
        library_fillGames(menu, FAVORITES_PATH);
        break;
    case MENU_SYSTEMS:
        library_loadSystems(EMU_DIR);
        library_fillSystems(menu);
        break;
    case MENU_EXPERT:
        library_loadSystems(RAPP_DIR);
        library_fillSystems(menu);
        break;
    case MENU_ROMS:
        library_fillRoms(menu);
        break;
    case MENU_APPS:
        library_fillApps(menu);
        break;
    default:
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

// Tab of the last launch for the Onion game list options (GLO), which reads
// the "type" of the second level of the Miyoo /tmp/state.json, one key a line
static void write_tab_state(void)
{
    int type = -1;
    FILE *fp;

    if (depth > 0) {
        switch (stack[1].kind) {
        case MENU_RECENTS: type = 10; break;
        case MENU_FAVORITES: type = 2; break;
        case MENU_SYSTEMS: type = 1; break;
        case MENU_EXPERT: type = 16; break;
        case MENU_APPS: type = 3; break;
        default: break;
        }
    }
    if (type < 0 || !(fp = fopen("/tmp/state.json", "w"))) {
        remove("/tmp/state.json");
        return;
    }
    fprintf(fp, "{\n\t\"list\":\t[{\n\t\t\t\"title\":\t157,\n\t\t\t\"type\":\t0,\n"
                "\t\t}, {\n\t\t\t\"title\":\t0,\n\t\t\t\"type\":\t%d,\n\t\t}]\n}\n",
            type);
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
    write_tab_state();
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
        if (entry->index == MENU_SETTINGS) {
            // No Miyoo settings screen yet, Tweaks has the Onion settings
            if (launch && is_dir(TWEAKS_APP))
                launch_app(TWEAKS_APP);
        }
        else
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

// Y set to "glo" in Tweaks: as on the Miyoo, the entry is launched with
// /tmp/launch_alt (keymon also sets it on Y) and the runtime opens the game
// list options instead of the game. Elsewhere the flag is dropped, or the next
// launch would open the options
static void open_game_options(void)
{
    MenuEntry *entry = menu_current(current());

    if (entry && (entry->type == ENTRY_ROM || entry->type == ENTRY_GAME ||
                  entry->type == ENTRY_APP)) {
        temp_flag_set("launch_alt", true);
        activate(true);
    }
    else
        temp_flag_set("launch_alt", false);
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
            move(menu, 0);
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
        move(menu, 0);
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

    List list_view = list_create(LIST_VISIBLE, LIST_SMALL),
         apps_view = list_create(APPS_VISIBLE, LIST_LARGE);
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
            int before = menu->active, depth_before = depth,
                visible = view_visible(menu->kind), cols = view_columns(menu->kind);
            bool is_main = menu->kind == MENU_MAIN, is_grid = cols > 1;

            // Main screen: a row of tabs. Grids: arrows move on the grid,
            // L1/R1 change page. Lists: up/down, left/right/L1/R1 page.
            if (keystate[SW_BTN_UP] >= PRESSED) {
                if (!is_main)
                    move(menu, is_grid ? -cols : -1);
            }
            else if (keystate[SW_BTN_DOWN] >= PRESSED) {
                if (!is_main)
                    move(menu, is_grid ? cols : 1);
            }
            else if (keystate[SW_BTN_LEFT] >= PRESSED)
                move(menu, is_main || is_grid ? -1 : -visible);
            else if (keystate[SW_BTN_RIGHT] >= PRESSED)
                move(menu, is_main || is_grid ? 1 : visible);
            else if (keystate[SW_BTN_L1] >= PRESSED) {
                if (!is_main)
                    move(menu, -visible);
            }
            else if (keystate[SW_BTN_R1] >= PRESSED) {
                if (!is_main)
                    move(menu, visible);
            }
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
                if (strcmp(settings.mainui_button_y, "glo") == 0)
                    open_game_options();
                else {
                    toggle_favorite();
                    changed = true;
                }
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
                if (menu->kind == MENU_MAIN)
                    view_renderMain(screen, menu, battery_percentage);
                else if (view_isPaged(menu->kind))
                    view_renderGrid(screen, menu, battery_percentage);
                else {
                    List *view = menu->kind == MENU_APPS ? &apps_view : &list_view;
                    menu_fillView(menu, view, view_visible(menu->kind), preview_path());
                    view_renderList(screen, menu, view, depth > 0, battery_percentage);
                }

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

    menu_fillView(current(), &list_view, 0, NULL);
    menu_fillView(current(), &apps_view, 0, NULL);
    list_free(&list_view);
    list_free(&apps_view);
    view_free();
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
