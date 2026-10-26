/* mu_worlds - "World units" in My Units (owner 2026-10-25: "My save / world units ... so i can test things with my units").
 * The player's worlds from the folders Load Campaign lists (game saves AND modded saves), in ONE list (owner 2026-10-25:
 * no group headings, as his Load Campaign): one row per world (campaign guid), its newest save in either folder, its clan
 * name and save time, newest world first. The units of that save are shown read only (mu_save.h); a snapshot
 * copies chosen ones into My Units as normal units with their source (myunits.h mu_source). Pure C, no game code.
 * Saves are only read, never written, moved or renamed. */
#pragma once
#include <stddef.h>
#include "myunits.h"
#include "mu_save.h"

typedef struct mu_world {
    char path[1024];          /* the world's newest save file */
    char save[200];           /* its name without .json (the game's SaveName) */
    char name[96];            /* CampaignSaveHeader.ClanName (raw text, quotes as spaces); the file name when missing */
    char guid[40];            /* CampaignGuid: 32 hex, lower case */
    char when[24];            /* the save's time "YYYY-MM-DD HH:MM" (header SaveTime, else the file time) */
    char sort[24];            /* the same time to the second "YYYY-MM-DD HH:MM:SS": the list's order (newest first) */
    int group;                /* index of the folder its newest save is in (dirs list order) */
    long long mtime;          /* file time, seconds */
} mu_world;

/* our own worlds and temporary saves, by file name: never listed (multiplayer temps mp_*, MPFE_ / MPHub_ / MPPvP_
   copies, and the saves of the Foreign Expedition hub and PvP worlds, whose campaign guid starts with fe5ea00 / fe5eb /
   b45e0: ds_multiplayer src/mp_worldsave.h, same rule) */
int  mu_world_save_name(const char *file);
/* skip(guid, save_name) = 1: a world not to list (Test Fights' test worlds); may be NULL */
typedef int (*mu_world_skip_fn)(const char *guid, const char *save_name);
/* the worlds of these folders, one list: one per campaign guid (a world in both folders once), its newest file by file
   time; the worlds newest first by the save time they show (header SaveTime, the file time when it has none: a copied or
   synced save keeps its own time, as Load Campaign orders it). A file without a readable CampaignSaveHeader is left out.
   Returns the count. */
int  mu_worlds_list(const char *const *dirs, int ndirs, mu_world *out, int max, mu_world_skip_fn skip);
/* the normal game's Saves folder (env DSML_GAME_SAVES, set by both starters; "" when not set) */
void mu_game_saves_dir(char *out, size_t n);
/* the header of a save (guid, clan name, save time) into w, read without parsing the whole save: 0 ok */
int  mu_world_header(const char *path, mu_world *w);
/* unit `guid` of a save as a builder JSON build: what a snapshot stores in My Units and what Test Fights' World units
   hands the save generator (the same unit both ways). jfree it. NULL + err. */
jv  *mu_save_build_obj(const char *path, const char *guid, char *err, size_t errn);
/* the same build for Test Fights' World units, plus what the unit carries from the save, in "_cm" for the save generator's
   glue (fights/sg_glue.js carry, autoSkill): {"bag": [mu_save_unit_carry bag entries], "aff": [its AffecterHolders],
   "opt": its BehaviourOption flags, "autouse": its per-skill auto-use (mu_save_unit_auto)} ("aff" only with
   aff != 0: the "bring buffs and effects" switch). known(kind 0 item / 1 affecter, key) = 0 leaves an entry out and counts
   it in *skip_items / *skip_aff (the game's tables do not know the key); known NULL = all kept. jfree it. NULL + err. */
typedef int (*mu_known_fn)(int kind, const char *key);
jv  *mu_save_fight_build(const char *path, const char *guid, int aff, mu_known_fn known, int *skip_items, int *skip_aff, char *err, size_t errn);

/* Game side (units/mu_window.c, main thread; not in the host tests): the one World units list, shared by My Units and
   Test Fights (owner 2026-10-25: "Test fights should allow to select directly people from one of your current saves").
   mu_player_worlds = the folders Load Campaign lists (DSML_GAME_SAVES + the modded game's Saves), Test Fights' test
   worlds left out; returns the count. mu_world_units = the units of the world's save (mu_save_units) with the game's
   default names in the current language ("Unit" when none); the count, or -1 + err. */
int  mu_player_worlds(mu_world *out, int max);
int  mu_world_units(const mu_world *w, mu_save_unit *out, int max, char *err, size_t errn);

/* the snapshot: unit `guid` of the world's save -> a new My Units unit named `name` (NULL / "" = the build's own name)
   with its source (world, save, save time, snapshot time = now). Does NOT save the library. 0 ok (id_out), -1 + err. */
int  mu_snapshot(jv *lib, const mu_world *w, const char *guid, const char *name, char id_out[17], char *err, size_t errn);
/* the card's source line: "<world> · save <time> · snapshot <time>" (local times) */
void mu_source_line(const mu_source *s, char *out, size_t n);
/* the same, longest first, for the room on the card: the full line, "<world> · snapshot <time>", "<world> · <time>",
   "<world> · <MM-DD HH:MM>". Returns the count (4). */
int  mu_source_forms(const mu_source *s, char forms[4][200]);
