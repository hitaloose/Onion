#ifndef MAINUI_MENU_H__
#define MAINUI_MENU_H__

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "components/list.h"
#include "utils/str.h"

// A ListItem is ~31 KB (value labels, preview path), too much for a folder of
// thousands of ROMs on a 256 MB device. Menus keep light entries and render
// through a List that only holds the visible rows.

typedef enum {
    ENTRY_MENU,   // opens another menu (payload: menu kind)
    ENTRY_SYSTEM, // opens a system's ROM folder (index: system)
    ENTRY_FOLDER, // opens a ROM subfolder (path: relative folder)
    ENTRY_ROM,    // launches a ROM (path: relative file)
    ENTRY_GAME,   // launches a recent/favorite (index: game entry)
    ENTRY_APP,    // launches an app (path: app folder, index: app)
    ENTRY_EMPTY   // placeholder for an empty menu
} EntryType;

typedef struct {
    EntryType type;
    char label[STR_MAX];
    char path[STR_MAX];
    int index;
} MenuEntry;

typedef enum {
    MENU_MAIN,
    MENU_RECENTS,
    MENU_FAVORITES,
    MENU_SYSTEMS,
    MENU_EXPERT,
    MENU_ROMS,
    MENU_APPS,
    MENU_SETTINGS, // main screen tab only: launches Tweaks
    MENU_KIND_COUNT
} MenuKind;

typedef struct {
    MenuKind kind;
    char title[STR_MAX];
    int system;           // MENU_ROMS: index in the system list
    char folder[STR_MAX]; // MENU_ROMS: folder relative to the system ROM root
    MenuEntry *entries;
    int count;
    int capacity;
    int active;
    int top;
} Menu;

void menu_init(Menu *menu, MenuKind kind, const char *title)
{
    memset(menu, 0, sizeof(Menu));
    menu->kind = kind;
    strncpy(menu->title, title, STR_MAX - 1);
}

MenuEntry *menu_add(Menu *menu, EntryType type, const char *label,
                    const char *path, int index)
{
    if (menu->count == menu->capacity) {
        menu->capacity = menu->capacity ? menu->capacity * 2 : 32;
        menu->entries = realloc(menu->entries, menu->capacity * sizeof(MenuEntry));
    }
    MenuEntry *entry = &menu->entries[menu->count++];
    entry->type = type;
    strncpy(entry->label, label, STR_MAX - 1);
    entry->label[STR_MAX - 1] = '\0';
    strncpy(entry->path, path ? path : "", STR_MAX - 1);
    entry->path[STR_MAX - 1] = '\0';
    entry->index = index;
    return entry;
}

void menu_free(Menu *menu)
{
    free(menu->entries);
    menu->entries = NULL;
    menu->count = menu->capacity = 0;
}

MenuEntry *menu_current(Menu *menu)
{
    return menu->count > 0 ? &menu->entries[menu->active] : NULL;
}

static int _menu_compare(const void *a, const void *b)
{
    const MenuEntry *ea = a, *eb = b;
    // Folders before files
    if ((ea->type == ENTRY_FOLDER) != (eb->type == ENTRY_FOLDER))
        return ea->type == ENTRY_FOLDER ? -1 : 1;
    return strcasecmp(ea->label, eb->label);
}

void menu_sort(Menu *menu)
{
    qsort(menu->entries, menu->count, sizeof(MenuEntry), _menu_compare);
}

// Moves the selection, wrapping around on single steps and clamping on
// longer moves. Paged menus (grids) scroll a whole page at a time.
void menu_move(Menu *menu, int delta, int visible, bool paged)
{
    if (menu->count == 0)
        return;
    int target = menu->active + delta;
    if (delta == 1 || delta == -1)
        target = (target + menu->count) % menu->count;
    else if (target < 0)
        target = 0;
    else if (target >= menu->count)
        target = menu->count - 1;
    menu->active = target;

    if (paged)
        menu->top = menu->active - menu->active % visible;
    else if (menu->active < menu->top)
        menu->top = menu->active;
    else if (menu->active >= menu->top + visible)
        menu->top = menu->active - visible + 1;
}

// Fills a List of `visible` rows with the window of entries around the selection
void menu_fillView(Menu *menu, List *view, int visible, const char *preview_path)
{
    // The renderer loads the preview into the item, free it before reuse
    for (int i = 0; i < view->item_count; i++) {
        if (view->items[i].preview_ptr)
            SDL_FreeSurface((SDL_Surface *)view->items[i].preview_ptr);
    }
    memset(view->items, 0, visible * sizeof(ListItem));
    view->item_count = 0;
    view->scroll_pos = 0;
    view->scroll_height = visible;
    view->active_pos = menu->active - menu->top;
    strncpy(view->title, menu->title, STR_MAX - 1);

    for (int i = menu->top; i < menu->count && i < menu->top + visible; i++) {
        ListItem *item = &view->items[view->item_count];
        item->_id = view->item_count++;
        item->item_type = ACTION;
        strncpy(item->label, menu->entries[i].label, STR_MAX - 1);
        if (i == menu->active && preview_path)
            strncpy(item->preview_path, preview_path, sizeof(item->preview_path) - 1);
    }
}

#endif // MAINUI_MENU_H__
