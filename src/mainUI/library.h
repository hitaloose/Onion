#ifndef MAINUI_LIBRARY_H__
#define MAINUI_LIBRARY_H__

#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>

#include "cjson/cJSON.h"
#include "components/JsonGameEntry.h"
#include "system/settings.h"
#include "utils/file.h"
#include "utils/json.h"
#include "utils/str.h"

#include "./menu.h"

#define EMU_DIR "/mnt/SDCARD/Emu"
#define APP_DIR "/mnt/SDCARD/App"
#define RAPP_DIR "/mnt/SDCARD/RApp"
#define ARCADE_NAMES_PATH "/mnt/SDCARD/BIOS/arcade_lists/arcade-rom-names.txt"
#define MAX_SYSTEMS 128
#define MAX_APPS 128
#define MAX_RECENTS 50
#define MAX_GAMELIST_NAMES 4096

// An Emu/<SYS>/config.json (or RApp/<core>/config.json), as read by the Miyoo MainUI
typedef struct {
    char dir[STR_MAX];      // /mnt/SDCARD/Emu/GBA
    char label[STR_MAX];    // GBA
    char launch[STR_MAX];   // /mnt/SDCARD/Emu/GBA/launch.sh
    char rompath[STR_MAX];  // ../../Roms/GBA (relative to dir)
    char imgpath[STR_MAX];  // ../../Roms/GBA/Imgs (relative to dir)
    char gamelist[STR_MAX]; // ../../Roms/GBA/miyoogamelist.xml
    char extlist[STR_MAX];  // bin|gba|zip|7z
    char icon[STR_MAX];     // absolute path, empty if none
    char iconsel[STR_MAX];  // icon when selected, absolute path
    int shortname;          // 1: arcade ROM names, shown with their titles
    int hidebios;           // 1: BIOS archives left out of the list
} System;

static System systems[MAX_SYSTEMS];
static int system_count = 0;

// An App/<name>/config.json
typedef struct {
    char dir[STR_MAX];
    char label[STR_MAX];
    char icon[STR_MAX]; // absolute path, empty if none
    char description[STR_MAX];
} App;

static App apps[MAX_APPS];
static int app_count = 0;

// Recents and favorites, one JSON object per line, newest first
static JsonGameEntry *games = NULL;
static int game_count = 0;

static bool _has_extension(const char *file_name, const char *extlist)
{
    const char *dot = strrchr(file_name, '.');
    if (!dot || strlen(extlist) == 0)
        return strlen(extlist) == 0;

    char list[STR_MAX];
    strncpy(list, extlist, STR_MAX - 1);
    list[STR_MAX - 1] = '\0';
    for (char *ext = strtok(list, "|"); ext; ext = strtok(NULL, "|")) {
        if (strcasecmp(dot + 1, ext) == 0)
            return true;
    }
    return false;
}

static bool _is_rom_entry(const char *folder, const struct dirent *ep, const char *extlist, bool *is_dir)
{
    char path[STR_MAX * 2];
    struct stat st;

    if (ep->d_name[0] == '.' || strcmp(ep->d_name, "Imgs") == 0)
        return false;

    snprintf(path, sizeof(path), "%s/%s", folder, ep->d_name);
    if (stat(path, &st) != 0)
        return false;

    *is_dir = S_ISDIR(st.st_mode);
    return *is_dir || _has_extension(ep->d_name, extlist);
}

// True if the folder holds at least one ROM, directly or in a subfolder
static bool _folder_has_roms(const char *folder, const char *extlist, int depth)
{
    DIR *dp = opendir(folder);
    struct dirent *ep;
    bool found = false;
    bool is_dir;

    if (!dp)
        return false;
    while (!found && (ep = readdir(dp))) {
        if (!_is_rom_entry(folder, ep, extlist, &is_dir))
            continue;
        if (!is_dir)
            found = true;
        else if (depth > 0) {
            char sub[STR_MAX * 2];
            snprintf(sub, sizeof(sub), "%s/%s", folder, ep->d_name);
            found = _folder_has_roms(sub, extlist, depth - 1);
        }
    }
    closedir(dp);
    return found;
}

void system_romRoot(const System *system, char *out)
{
    snprintf(out, STR_MAX * 2, "%s/%s", system->dir, system->rompath);
}

// Config paths are absolute or relative to the config's folder
static void _config_path(cJSON *config, const char *key, const char *dir, char *out)
{
    char value[STR_MAX] = "";
    out[0] = '\0';
    if (!json_getString(config, key, value) || strlen(value) == 0)
        return;
    if (value[0] == '/')
        strncpy(out, value, STR_MAX - 1);
    else
        snprintf(out, STR_MAX, "%s/%s", dir, value);
    if (!is_file(out))
        out[0] = '\0';
}

// Loads the systems of Emu (Games tab) or RApp (Expert tab) that have ROMs
void library_loadSystems(const char *base_dir)
{
    DIR *dp = opendir(base_dir);
    struct dirent *ep;

    system_count = 0;
    if (!dp)
        return;

    while ((ep = readdir(dp)) && system_count < MAX_SYSTEMS) {
        char config_path[STR_MAX * 2];
        if (ep->d_name[0] == '.')
            continue;
        snprintf(config_path, sizeof(config_path), "%s/%s/config.json", base_dir, ep->d_name);
        if (!is_file(config_path))
            continue;

        cJSON *config = json_load(config_path);
        if (!config)
            continue;

        System *system = &systems[system_count];
        memset(system, 0, sizeof(System));
        snprintf(system->dir, STR_MAX, "%s/%s", base_dir, ep->d_name);

        char launch[STR_MAX] = "launch.sh";
        json_getString(config, "label", system->label);
        json_getString(config, "launch", launch);
        json_getString(config, "rompath", system->rompath);
        json_getString(config, "imgpath", system->imgpath);
        json_getString(config, "gamelist", system->gamelist);
        json_getString(config, "extlist", system->extlist);
        json_getInt(config, "shortname", &system->shortname);
        json_getInt(config, "hidebios", &system->hidebios);
        _config_path(config, "icon", system->dir, system->icon);
        _config_path(config, "iconsel", system->dir, system->iconsel);
        cJSON_Delete(config);
        char launch_path[STR_MAX * 2];
        snprintf(launch_path, sizeof(launch_path), "%s/%s", system->dir, launch);
        strncpy(system->launch, launch_path, STR_MAX - 1);

        if (strlen(system->label) == 0)
            strncpy(system->label, ep->d_name, STR_MAX - 1);

        char rom_root[STR_MAX * 2];
        system_romRoot(system, rom_root);
        if (_folder_has_roms(rom_root, system->extlist, 1))
            system_count++;
    }
    closedir(dp);
}

void library_fillSystems(Menu *menu)
{
    for (int i = 0; i < system_count; i++)
        menu_add(menu, ENTRY_SYSTEM, systems[i].label, NULL, i);
    menu_sort(menu);
}

// Display names from miyoogamelist.xml (<game><path>./file</path><name>..</name>)
typedef struct {
    char file[STR_MAX];
    char name[STR_MAX];
} GameName;

static GameName *_gamelist_names = NULL;
static int _gamelist_count = 0;

static void _gamelist_load(const System *system)
{
    char path[STR_MAX * 2];
    _gamelist_count = 0;
    if (strlen(system->gamelist) == 0)
        return;

    snprintf(path, sizeof(path), "%s/%s", system->dir, system->gamelist);
    char *xml = file_read(path);
    if (!xml)
        return;

    if (!_gamelist_names)
        _gamelist_names = malloc(MAX_GAMELIST_NAMES * sizeof(GameName));

    for (char *game = strstr(xml, "<game>"); game && _gamelist_count < MAX_GAMELIST_NAMES;
         game = strstr(game + 6, "<game>")) {
        char *end = strstr(game, "</game>");
        char *path_tag = strstr(game, "<path>");
        char *name_tag = strstr(game, "<name>");
        if (!end || !path_tag || !name_tag || path_tag > end || name_tag > end)
            continue;

        GameName *entry = &_gamelist_names[_gamelist_count];
        path_tag += 6;
        if (strncmp(path_tag, "./", 2) == 0)
            path_tag += 2;
        name_tag += 6;
        char *path_end = strstr(path_tag, "</path>");
        char *name_end = strstr(name_tag, "</name>");
        if (!path_end || !name_end || path_end - path_tag >= STR_MAX || name_end - name_tag >= STR_MAX)
            continue;

        memcpy(entry->file, path_tag, path_end - path_tag);
        entry->file[path_end - path_tag] = '\0';
        memcpy(entry->name, name_tag, name_end - name_tag);
        entry->name[name_end - name_tag] = '\0';
        char *amp = str_replace(entry->name, "&amp;", "&");
        strncpy(entry->name, amp, STR_MAX - 1);
        free(amp);
        _gamelist_count++;
    }
    free(xml);
}

static const char *_gamelist_lookup(const char *relative_file)
{
    for (int i = 0; i < _gamelist_count; i++) {
        if (strcmp(_gamelist_names[i].file, relative_file) == 0)
            return _gamelist_names[i].name;
    }
    return NULL;
}

// Arcade titles for "shortname" systems, from the list the Miyoo MainUI reads
// through libgamename: one `rom "Title"` a line, sorted by ROM name
typedef struct {
    const char *rom;
    const char *title;
} ArcadeName;

static char *_arcade_text = NULL;
static ArcadeName *_arcade_names = NULL;
static int _arcade_count = 0;

static void _arcade_load(void)
{
    if (_arcade_text || !(_arcade_text = file_read(ARCADE_NAMES_PATH)))
        return;

    int lines = 1;
    for (char *c = _arcade_text; *c; c++)
        if (*c == '\n')
            lines++;
    _arcade_names = malloc(lines * sizeof(ArcadeName));
    if (!_arcade_names)
        return;

    char *line = _arcade_text;
    while (line && *line) {
        char *next = strchr(line, '\n');
        if (next)
            *next++ = '\0';
        char *title = line + strcspn(line, " \t");
        if (*title) {
            *title++ = '\0';
            title += strspn(title, " \t\"");
            char *end = title + strlen(title);
            while (end > title && (end[-1] == '\r' || end[-1] == '"'))
                *--end = '\0';
            _arcade_names[_arcade_count].rom = line;
            _arcade_names[_arcade_count].title = title;
            _arcade_count++;
        }
        line = next;
    }
}

static int _arcade_compare(const void *key, const void *entry)
{
    return strcmp((const char *)key, ((const ArcadeName *)entry)->rom);
}

static const char *_arcade_title(const char *rom)
{
    ArcadeName *found = _arcade_count > 0
                            ? bsearch(rom, _arcade_names, _arcade_count,
                                      sizeof(ArcadeName), _arcade_compare)
                            : NULL;
    return found ? found->title : NULL;
}

// The Miyoo's own BIOS list is not known: Neo Geo and titles that say BIOS
static bool _is_arcade_bios(const char *rom, const char *title)
{
    return strcmp(rom, "neogeo") == 0 ||
           (title && (strstr(title, "BIOS") || strstr(title, "Bios")));
}

static void _strip_extension(char *name)
{
    char *dot = strrchr(name, '.');
    if (dot && dot != name)
        *dot = '\0';
}

void library_fillRoms(Menu *menu)
{
    const System *system = &systems[menu->system];
    char rom_root[STR_MAX * 2], folder[STR_MAX * 3];
    DIR *dp;
    struct dirent *ep;
    bool is_dir;

    system_romRoot(system, rom_root);
    if (strlen(menu->folder) > 0)
        snprintf(folder, sizeof(folder), "%s/%s", rom_root, menu->folder);
    else
        strncpy(folder, rom_root, sizeof(folder) - 1);

    if (strlen(menu->folder) == 0)
        _gamelist_load(system);
    if (system->shortname || system->hidebios)
        _arcade_load();

    if (!(dp = opendir(folder)))
        return;

    while ((ep = readdir(dp))) {
        if (!_is_rom_entry(folder, ep, system->extlist, &is_dir))
            continue;

        char relative[STR_MAX * 2], label[STR_MAX];
        if (strlen(menu->folder) > 0)
            snprintf(relative, sizeof(relative), "%s/%s", menu->folder, ep->d_name);
        else
            strncpy(relative, ep->d_name, sizeof(relative) - 1);

        const char *name = _gamelist_lookup(relative);
        strncpy(label, name ? name : ep->d_name, STR_MAX - 1);
        label[STR_MAX - 1] = '\0';
        if (!name && !is_dir) {
            _strip_extension(label);
            if (system->shortname || system->hidebios) {
                const char *title = _arcade_title(label);
                if (system->hidebios && _is_arcade_bios(label, title))
                    continue;
                if (system->shortname && title)
                    strncpy(label, title, STR_MAX - 1);
            }
        }

        menu_add(menu, is_dir ? ENTRY_FOLDER : ENTRY_ROM, label, relative, menu->system);
    }
    closedir(dp);
    menu_sort(menu);
}

void library_fillApps(Menu *menu)
{
    DIR *dp = opendir(APP_DIR);
    struct dirent *ep;

    app_count = 0;
    if (!dp)
        return;
    while ((ep = readdir(dp)) && app_count < MAX_APPS) {
        char config_path[STR_MAX * 2];
        if (ep->d_name[0] == '.')
            continue;
        App *app = &apps[app_count];
        memset(app, 0, sizeof(App));
        snprintf(app->dir, STR_MAX, APP_DIR "/%s", ep->d_name);
        snprintf(config_path, sizeof(config_path), "%s/config.json", app->dir);
        if (!is_file(config_path))
            continue;

        cJSON *config = json_load(config_path);
        if (!config)
            continue;
        json_getString(config, "label", app->label);
        json_getString(config, "description", app->description);
        _config_path(config, "icon", app->dir, app->icon);
        cJSON_Delete(config);
        if (strlen(app->label) == 0)
            strncpy(app->label, ep->d_name, STR_MAX - 1);
        menu_add(menu, ENTRY_APP, app->label, app->dir, app_count++);
    }
    closedir(dp);
    menu_sort(menu);
}

// Miyoo format paths: <emu dir>/<rompath>/<file>, e.g. /mnt/SDCARD/Emu/GBA/../../Roms/GBA/x.gba
JsonGameEntry library_romEntry(const Menu *menu, const MenuEntry *rom)
{
    const System *system = &systems[menu->system];
    JsonGameEntry entry = {.type = 5};
    char base[STR_MAX];

    strncpy(entry.label, rom->label, STR_MAX - 1);
    strncpy(entry.launch, system->launch, STR_MAX - 1);
    snprintf(entry.rompath, STR_MAX, "%s/%s/%s", system->dir, system->rompath, rom->path);

    strncpy(base, file_basename(rom->path), STR_MAX - 1);
    _strip_extension(base);
    snprintf(entry.imgpath, STR_MAX, "%s/%s/%s.png", system->dir, system->imgpath, base);
    if (!is_file(entry.imgpath))
        entry.imgpath[0] = '\0';
    return entry;
}

// Loads recentlist.json or favourite.json into `games`
void library_loadGames(const char *list_path)
{
    char line[STR_MAX * 6];
    FILE *fp = fopen(list_path, "r");

    free(games);
    games = NULL;
    game_count = 0;
    if (!fp)
        return;

    int capacity = 0;
    while (fgets(line, sizeof(line), fp)) {
        if (strlen(line) < 2)
            continue;
        JsonGameEntry entry = JsonGameEntry_fromJson(line);
        if (strlen(entry.rompath) == 0 || !exists(entry.rompath))
            continue;
        if (game_count == capacity) {
            capacity = capacity ? capacity * 2 : 32;
            games = realloc(games, capacity * sizeof(JsonGameEntry));
        }
        games[game_count++] = entry;
    }
    fclose(fp);
}

void library_fillGames(Menu *menu, const char *list_path)
{
    library_loadGames(list_path);
    for (int i = 0; i < game_count; i++)
        menu_add(menu, ENTRY_GAME, games[i].label, NULL, i);
}

// Rewrites a game list with `entry` first (add) or without it (remove)
void library_updateGameList(const char *list_path, JsonGameEntry *entry, bool add, int max_entries)
{
    char line[STR_MAX * 6], json[STR_MAX * 6], temp_path[STR_MAX];
    int written = 0;

    snprintf(temp_path, STR_MAX, "%s.tmp", list_path);
    FILE *out = fopen(temp_path, "w");
    if (!out)
        return;

    if (add) {
        JsonGameEntry_toJson(json, entry);
        fprintf(out, "%s\n", json);
        written++;
    }

    FILE *in = fopen(list_path, "r");
    if (in) {
        while (fgets(line, sizeof(line), in) && (max_entries == 0 || written < max_entries)) {
            if (strlen(line) < 2)
                continue;
            JsonGameEntry existing = JsonGameEntry_fromJson(line);
            if (strcmp(existing.rompath, entry->rompath) == 0)
                continue;
            fputs(line, out);
            if (line[strlen(line) - 1] != '\n')
                fputc('\n', out);
            written++;
        }
        fclose(in);
    }

    fflush(out);
    fsync(fileno(out));
    fclose(out);
    rename(temp_path, list_path);
}

bool library_isFavorite(const char *rompath)
{
    char line[STR_MAX * 6];
    bool found = false;
    FILE *fp = fopen(FAVORITES_PATH, "r");
    if (!fp)
        return false;
    while (!found && fgets(line, sizeof(line), fp)) {
        JsonGameEntry entry = JsonGameEntry_fromJson(line);
        found = strcmp(entry.rompath, rompath) == 0;
    }
    fclose(fp);
    return found;
}

#endif // MAINUI_LIBRARY_H__
