/* tf_squad - the squad cards of the Test Fights window (owner 2026-11-02, PLAN_v013 item 30: "where you have written squad
 * 4/4 ... display the avatar and the NPC icon of each unit that we are bringing to the fight. With maybe their name as well";
 * answer 14:21: "npc icon is the ingame sprite"). Right after the "Squad: n / 4" label: one small card per unit going in,
 * with its portrait (the game's portrait sprite, as on the game's own unit portrait bar), its in-game sprite (the game's
 * own compositor, Refactor.UnitVisual.PaintAll: race body, face, hair, worn gear; mu_look.h), its name and level; a free
 * slot is a faint empty frame. Hover a card: the kit's hover card (name, level, race, background). Main thread. */
#pragma once
#include "dsl_sdk.h"
#include "../fe/fe_json.h"

/* one unit going in, from any of the window's three sources */
enum { TFS_LIB = 0, TFS_GAL = 1, TFS_SAVE = 2 };
typedef struct tfs_pick {
    int src;                       /* TFS_LIB: My Units (Unit Builder units too), TFS_GAL: a Wiki Build Gallery build,
                                      TFS_SAVE: a World units pick */
    const char *id;                /* LIB: the My Units unit id */
    const jv *build;               /* LIB: the unit's build object (library.json units[i].build) */
    const char *gal_json;          /* GAL: the builds (JSON array, as Test Fights keeps them) */
    int gal_i;                     /*      this build's index in it */
    const char *path, *guid;       /* SAVE: the world's save file, the unit's guid */
    const char *name;              /* the name the window shows for it (GAL: "Name  Lv N" is cut to the name) */
    int level;                     /* 0 = the build's own */
} tfs_pick;

/* the row: the label text, then max slots of equal width in x..x+w, y..y+h (TFS_ROW_H). Draws into parent (kept until the
   next call or tfs_clear). Returns the row height used. */
float tfs_draw(DSLObj parent, float x, float y, float w, float h, const char *label, uint32_t label_rgba, float label_size,
               const tfs_pick *p, int n, int max);
void  tfs_clear(void);             /* the window closed: forget the widgets */
#define TFS_ROW_H 84.f
