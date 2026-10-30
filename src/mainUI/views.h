#ifndef MAINUI_VIEWS_H__
#define MAINUI_VIEWS_H__

// The screens of the Miyoo MainUI, drawn from the theme skin so any Onion
// theme looks the same as on the Miyoo Mini. Positions come from screenshots
// of Onion on the Miyoo Mini and page sizes from Onion's state.h; see
// docs/rg35xx-mainui.md for what is measured and what is still a guess.

#include <SDL/SDL.h>
#include <SDL/SDL_image.h>
#include <SDL/SDL_ttf.h>

#include "components/list.h"
#include "system/lang.h"
#include "theme/theme.h"

#include "./library.h"
#include "./menu.h"

// Main screen: four ic-<tab>-{n,f}.png tiles and one dot per tab. Tiles are
// 156x328 or 156x278 depending on the theme; the Miyoo centers them on y = 230
// (328 at y = 66 in Silky, 278 at y = 91 in the Blueprint preview)
#define MAIN_VISIBLE 4
#define MAIN_TILE_X 10
#define MAIN_TILE_CY 230
#define MAIN_TILE_STEP 154
#define MAIN_TILE_W 156
#define MAIN_LABEL_Y 285
#define MAIN_DOT_Y 389
#define MAIN_DOT_GAP 5 // dots are dot-a.png wide plus this gap (24 + 5 in Silky, 13 + 5 in Analogue)

// Games tab: 4x2 grid of bg-game-item-{n,f}.png (154x170)
#define GAMES_COLS 4
#define GAMES_ROWS 2
#define GAMES_X 12
#define GAMES_Y 70
#define GAMES_CELL_W 154
#define GAMES_CELL_H 170
#define GAMES_ICON_Y 70  // icon center, from the cell top
#define GAMES_LABEL_Y 150 // label center, from the cell top

// Expert tab: 3x3 grid of bg-ra-list-item.png (214x120)
#define EXPERT_COLS 3
#define EXPERT_ROWS 3
#define EXPERT_X -1
#define EXPERT_Y 60
#define EXPERT_CELL_W 214
#define EXPERT_CELL_H 120

#define LIST_VISIBLE 6
#define APPS_VISIBLE 4

#define S(x) ((int)((x)*g_scale))

// Entries a screen shows at once (a page on grids)
int view_visible(MenuKind kind)
{
    switch (kind) {
    case MENU_MAIN:
        return MAIN_VISIBLE;
    case MENU_SYSTEMS:
        return GAMES_COLS * GAMES_ROWS;
    case MENU_EXPERT:
        return EXPERT_COLS * EXPERT_ROWS;
    case MENU_APPS:
        return APPS_VISIBLE;
    default:
        return LIST_VISIBLE;
    }
}

int view_columns(MenuKind kind)
{
    switch (kind) {
    case MENU_SYSTEMS:
        return GAMES_COLS;
    case MENU_EXPERT:
        return EXPERT_COLS;
    default:
        return 1;
    }
}

bool view_isPaged(MenuKind kind)
{
    return kind == MENU_SYSTEMS || kind == MENU_EXPERT;
}

// Icons named in config.json files, loaded on demand. Only one or two pages
// are on screen at a time, so a small ring of slots is enough.
#define ICON_CACHE_SIZE 24

typedef struct {
    char path[STR_MAX];
    SDL_Surface *surface;
} CachedIcon;

static CachedIcon _icon_cache[ICON_CACHE_SIZE];
static int _icon_cache_next = 0;

static SDL_Surface *_view_icon(const char *path)
{
    if (!path || strlen(path) == 0)
        return NULL;
    for (int i = 0; i < ICON_CACHE_SIZE; i++) {
        if (strcmp(_icon_cache[i].path, path) == 0)
            return _icon_cache[i].surface;
    }
    CachedIcon *slot = &_icon_cache[_icon_cache_next];
    _icon_cache_next = (_icon_cache_next + 1) % ICON_CACHE_SIZE;
    if (slot->surface)
        SDL_FreeSurface(slot->surface);
    strncpy(slot->path, path, STR_MAX - 1);
    slot->surface = IMG_Load(path); // a failed load is cached as NULL too
    return slot->surface;
}

// Skin images the shared theme resources do not cover
typedef enum {
    SKIN_GAME_ITEM_N,
    SKIN_GAME_ITEM_F,
    SKIN_RA_LIST_ITEM,
    SKIN_TAB_FIRST, // ic-<tab>-n, ic-<tab>-f for each MenuKind
    SKIN_COUNT = SKIN_TAB_FIRST + MENU_KIND_COUNT * 2
} SkinImage;

static SDL_Surface *_skin[SKIN_COUNT];
static bool _skin_loaded[SKIN_COUNT];

static const char *_tab_name(MenuKind kind)
{
    switch (kind) {
    case MENU_RECENTS:
        return "recent";
    case MENU_FAVORITES:
        return "favorite";
    case MENU_SYSTEMS:
        return "game";
    case MENU_EXPERT:
        return "retroarch";
    case MENU_APPS:
        return "app";
    case MENU_SETTINGS:
        return "setting";
    default:
        return NULL;
    }
}

static SDL_Surface *_view_skin(SkinImage image)
{
    if (_skin_loaded[image])
        return _skin[image];

    char name[STR_MAX] = "";
    if (image == SKIN_GAME_ITEM_N)
        strcpy(name, "bg-game-item-n");
    else if (image == SKIN_GAME_ITEM_F)
        strcpy(name, "bg-game-item-f");
    else if (image == SKIN_RA_LIST_ITEM)
        strcpy(name, "bg-ra-list-item");
    else {
        int tab = (image - SKIN_TAB_FIRST) / 2;
        const char *tab_name = _tab_name((MenuKind)tab);
        if (tab_name)
            snprintf(name, STR_MAX, "ic-%s-%s", tab_name,
                     (image - SKIN_TAB_FIRST) % 2 ? "f" : "n");
    }

    _skin[image] = strlen(name) ? theme_loadImage(theme()->path, name) : NULL;
    _skin_loaded[image] = true;
    return _skin[image];
}

static SDL_Surface *_tab_icon(MenuKind kind, bool active)
{
    return _view_skin((SkinImage)(SKIN_TAB_FIRST + kind * 2 + (active ? 1 : 0)));
}

static void _blit(SDL_Surface *surface, SDL_Surface *screen, int x, int y)
{
    if (!surface)
        return;
    SDL_Rect pos = {x, y};
    SDL_BlitSurface(surface, NULL, screen, &pos);
}

// Text centered on (center_x, center_y), cut to max_w
static void _render_text(SDL_Surface *screen, TTF_Font *font, const char *text,
                         SDL_Color color, int center_x, int center_y, int max_w)
{
    if (!font || !text || strlen(text) == 0)
        return;
    SDL_Surface *label = TTF_RenderUTF8_Blended(font, text, color);
    if (!label)
        return;
    SDL_Rect crop = {0, 0, label->w > max_w ? max_w : label->w, label->h};
    SDL_Rect pos = {center_x - crop.w / 2, center_y - label->h / 2};
    SDL_BlitSurface(label, &crop, screen, &pos);
    SDL_FreeSurface(label);
}

static void _render_footer(SDL_Surface *screen, bool show_back)
{
    theme_renderFooter(screen);
    theme_renderStandardHint(screen, lang_get(LANG_SELECT, LANG_FALLBACK_SELECT),
                             show_back ? lang_get(LANG_BACK, LANG_FALLBACK_BACK) : NULL);
}

static const char *_tab_label(MenuKind kind)
{
    switch (kind) {
    case MENU_RECENTS:
        return lang_get(LANG_RECENTS_TAB, LANG_FALLBACK_RECENTS_TAB);
    case MENU_FAVORITES:
        return lang_get(LANG_FAVORITES_TAB, LANG_FALLBACK_FAVORITES_TAB);
    case MENU_SYSTEMS:
        return lang_get(LANG_GAMES_TAB, LANG_FALLBACK_GAMES_TAB);
    case MENU_EXPERT:
        return lang_get(LANG_EXPERT_TAB, LANG_FALLBACK_EXPERT_TAB);
    case MENU_APPS:
        return lang_get(LANG_APPS_TAB, LANG_FALLBACK_APPS_TAB);
    case MENU_SETTINGS:
        return lang_get(LANG_SETTINGS_TAB, "Settings");
    default:
        return "";
    }
}

// Main screen: logo header, tab tiles, a dot per tab, A/B hints
void view_renderMain(SDL_Surface *screen, Menu *menu, int battery_percentage)
{
    SDL_BlitSurface(theme_background(), NULL, screen, NULL);
    theme_renderHeader(screen, NULL, true);

    TTF_Font *font = resource_getFont(GRID1x4);
    for (int slot = 0; slot < MAIN_VISIBLE && menu->top + slot < menu->count; slot++) {
        int i = menu->top + slot;
        bool active = i == menu->active;
        MenuKind tab = (MenuKind)menu->entries[i].index;
        int x = S(MAIN_TILE_X + slot * MAIN_TILE_STEP);

        SDL_Surface *icon = _tab_icon(tab, active);
        if (icon)
            _blit(icon, screen, x, S(MAIN_TILE_CY) - icon->h / 2);
        _render_text(screen, font, _tab_label(tab),
                     active ? theme()->grid.selectedcolor : theme()->grid.color,
                     x + S(MAIN_TILE_W) / 2, S(MAIN_LABEL_Y), S(MAIN_TILE_STEP));
    }

    SDL_Surface *dot_active = resource_getSurface(DOT_ACTIVE),
                *dot_neutral = resource_getSurface(DOT_NEUTRAL);
    int dot_step = (dot_active ? dot_active->w : 0) + S(MAIN_DOT_GAP);
    int dots_x = S(320) - menu->count * dot_step / 2;
    for (int i = 0; i < menu->count; i++)
        _blit(i == menu->active ? dot_active : dot_neutral, screen,
              dots_x + i * dot_step, S(MAIN_DOT_Y));

    _render_footer(screen, true);
    theme_renderHeaderBattery(screen, battery_percentage);
}

// Games (systems with icon and label) and Expert (core logos) grids
void view_renderGrid(SDL_Surface *screen, Menu *menu, int battery_percentage)
{
    bool expert = menu->kind == MENU_EXPERT;
    int cols = expert ? EXPERT_COLS : GAMES_COLS,
        visible = view_visible(menu->kind),
        cell_w = S(expert ? EXPERT_CELL_W : GAMES_CELL_W),
        cell_h = S(expert ? EXPERT_CELL_H : GAMES_CELL_H),
        x0 = S(expert ? EXPERT_X : GAMES_X),
        y0 = S(expert ? EXPERT_Y : GAMES_Y);
    TTF_Font *font = resource_getFont(GRID3x4);

    SDL_BlitSurface(theme_background(), NULL, screen, NULL);
    theme_renderHeader(screen, menu->title, false);

    for (int slot = 0; slot < visible && menu->top + slot < menu->count; slot++) {
        int i = menu->top + slot;
        MenuEntry *entry = &menu->entries[i];
        bool active = i == menu->active;
        int x = x0 + (slot % cols) * cell_w, y = y0 + (slot / cols) * cell_h;

        if (expert) {
            if (active)
                _blit(_view_skin(SKIN_RA_LIST_ITEM), screen, x, y);
        }
        else
            _blit(_view_skin(active ? SKIN_GAME_ITEM_F : SKIN_GAME_ITEM_N), screen, x, y);

        SDL_Surface *icon = NULL;
        const char *label = entry->label;
        if (entry->type == ENTRY_SYSTEM) {
            System *system = &systems[entry->index];
            if (active)
                icon = _view_icon(system->iconsel);
            if (!icon)
                icon = _view_icon(system->icon);
        }

        SDL_Color color = active ? theme()->grid.selectedcolor : theme()->grid.color;
        if (expert) {
            // The core logo holds the name, the label is only a fallback
            if (icon)
                _blit(icon, screen, x + (cell_w - icon->w) / 2, y + (cell_h - icon->h) / 2);
            else
                _render_text(screen, font, label, color, x + cell_w / 2, y + cell_h / 2, cell_w);
        }
        else {
            int label_y = y + S(GAMES_LABEL_Y);
            if (icon)
                _blit(icon, screen, x + (cell_w - icon->w) / 2, y + S(GAMES_ICON_Y) - icon->h / 2);
            else
                label_y = y + cell_h / 2;
            _render_text(screen, font, label, color, x + cell_w / 2, label_y, cell_w);
        }
    }

    _render_footer(screen, true);
    int pages = (menu->count + visible - 1) / visible;
    theme_renderFooterStatus(screen, menu->count ? menu->top / visible + 1 : 0, pages);
    theme_renderHeaderBattery(screen, battery_percentage);
}

// Game lists (6 rows, box art on the right) and Apps (4 rows with icon and
// description), with the selected item number over the total
void view_renderList(SDL_Surface *screen, Menu *menu, List *view, bool show_back,
                     int battery_percentage)
{
    if (menu->kind == MENU_APPS) {
        for (int i = 0; i < view->item_count; i++) {
            MenuEntry *entry = &menu->entries[menu->top + i];
            if (entry->type != ENTRY_APP)
                continue;
            App *app = &apps[entry->index];
            view->items[i].icon_ptr = _view_icon(app->icon);
            strncpy(view->items[i].description, app->description, STR_MAX - 1);
        }
    }

    theme_renderHeader(screen, menu->title, false);
    theme_renderList(screen, view);

    // The icons belong to the cache, the List must not free them
    for (int i = 0; i < view->item_count; i++)
        view->items[i].icon_ptr = NULL;

    _render_footer(screen, show_back);
    theme_renderFooterStatus(screen, menu->active + 1, menu->count);
    theme_renderHeaderBattery(screen, battery_percentage);
}

void view_free(void)
{
    for (int i = 0; i < ICON_CACHE_SIZE; i++) {
        if (_icon_cache[i].surface)
            SDL_FreeSurface(_icon_cache[i].surface);
        _icon_cache[i].surface = NULL;
        _icon_cache[i].path[0] = '\0';
    }
    for (int i = 0; i < SKIN_COUNT; i++) {
        if (_skin[i])
            SDL_FreeSurface(_skin[i]);
        _skin[i] = NULL;
        _skin_loaded[i] = false;
    }
}

#endif // MAINUI_VIEWS_H__
