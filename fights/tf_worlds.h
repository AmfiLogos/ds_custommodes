/* tf_worlds - test worlds (the saves Test Fights writes) never show in a Load list (flow I): the title's Load list, the
 * New Campaign screen and the multiplayer host list all go through the loader's load-list filter (dsl_sdk.h
 * loadlist_filter), which hides every save of a test world (tfw_is_test, below). The loader's Steam Cloud backup asks the
 * same filter, so a test world is never uploaded or downloaded. Autosaves the game makes inside a test fight carry the
 * same GUID, so they are hidden too.
 * Test worlds are throwaway copies made from the wiki's templates: back on the title after the fight their saves are
 * removed and the game refreshes its save list and the title's Continue (back to the player's own latest world). Writing a new test world also removes the files of older ones left over (game closed inside a fight).
 * Nothing of the player's own worlds is ever touched. The title never picks a test world as its background or Continue
 * save (also saves left by a crash), and while a test world is loaded the player's records (OutGame.json: tutorials,
 * items obtained, credits, Downfall unlock) are not written and are set back in memory when the test is left. */
#pragma once
#include "dsl_sdk.h"
#include <stddef.h>

int  tfw_init(const DSLHost *host, DSLPlugin *self);   /* registers the filter. 0 ok */
/* the save text from sg_generate -> <game data>/Saves/<SaveName>.json, listed as a test world. name_out = SaveName. 0 ok */
int  tfw_write(const char *save, size_t n, char *name_out, size_t name_n, char *err, size_t errn);
int  tfw_is_test(const char *guid, const char *save_name);   /* tfw_known with the game's data folder */
void tfw_loading(void);                                 /* our load is starting: keep the new test world on this title */

/* A test world is known forever (2026-10-26), on any computer, also after the title cleared the list:
 *  - the MARK: Test Fights gives every test world a campaign guid that starts with 7e57f167 and has version digit 0
 *    (7e57f167-xxxx-0xxx-...). The game, the wiki's generator and multiplayer make random version-4 guids (digit 4 at
 *    that place), so no other world can carry it; multiplayer's own worlds start with fe5ea00 / fe5eb / b45e0
 *    (ds_multiplayer src/mp_worldsave.h). Known by the guid, or by a 32-hex token of the save name (the game's
 *    "<clan>__AutoSave_<guid hex>_<n>" saves), so a copy from the cloud or another computer is known too;
 *  - the LIST (test_worlds.json "worlds"): test worlds whose files are still to remove; cleared on the title;
 *  - the RECORD (test_worlds_made.txt, appended, never cleared): every test world this install made, also those listed
 *    by an older version before the mark, so those stay known on this computer.
 * Never a name rule: a save downloaded from the wiki has the same kind of name and is the player's own world.
 * Pure C in tf_wikisave.c (fights/test/wikisave_test.c); the folder is <game data>/Mods/custom-modes. */
#define TFW_GUID_MARK "7e57f167"
int  tfw_guid_marked(const char *guid);                 /* 1 = the campaign guid (dashes or not) carries the mark */
void tfw_reg_dir(const char *dir);                      /* the folder of the list and the record; read at the next use */
struct jv;
/* a generated save becomes a test world: its campaign guid gets the mark (header, clan record, SaveName), it is listed and
   recorded (before its file exists: never visible). name = the SaveName. 0 ok, -1 = no usable name or guid */
int  tfw_take(struct jv *save, char *name, size_t name_n);
const char *tfw_listed(int i);                          /* the listed test worlds (32 hex), oldest first; NULL past the end */
void tfw_title_clear(void);                             /* on the title, their files removed: the list cleared, the record kept */
int  tfw_known(const char *guid, const char *save_name);   /* 1 = a test world: marked, listed or recorded */

/* Saves the wiki builder made (its save generator: Test Fights here, or a file downloaded from the wiki and put in the
 * Saves folder): the game must never ask "Lock Consecration Results" for them (pause reason ConsecrationNotice, a
 * Yes / No box). The generator's templates predate that setting, so its two fields are set the way the game's own
 * default leaves them: off and acknowledged. Saves the player made in the game are never touched.
 * A wiki-made save is known by its name, which only the generator writes (savegen.js finish()):
 *   [<team>_]<BossRush-<boss> | Arena-<monsters> | DPS Check | FinishedBase>_<difficulty>__AutoSave_<32 hex>_<n>
 *   <clan>_DSBuilder+<n>units_<difficulty | AsIs>__AutoSave_<32 hex>_<n>  (and the older "DSBuilder_" names)
 * At load: a hook on SaveLoadHelper.DeserializeCampaignSaveDataSafely(json, settings, path) hands the game the fixed
 * text (the file itself stays as it is). */
int  tfw_wiki_made(const char *save_name);              /* 1 = a name only the wiki's save generator writes */
int  tfw_dps_made(const char *save_name);               /* 1 = one of those, and a DPS Check (tf_dps.h) */
int  tfw_consecration_fix(struct jv *save);             /* off + acknowledged; 1 = something changed */
