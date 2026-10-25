/* tf_dps - Test Fights' DPS Check while the fight runs (owner 2026-10-25). The practice scarecrow (the wiki's DPS target,
 * BUILDING_DungeonScareCrow with 1,000,000 durability and its own self repair) in the game's bottom inspect panel, and a
 * line per hit on it in the multiplayer mod's chat:
 *   - the panel's durability "<now> / <max>" with the language's thousands separator, on one line, smaller only when it
 *     would not fit (the stock panel wrapped "1000000" onto a second line);
 *   - the panel's durability and bar follow the building while it is shown (the stock panel sets a building's durability
 *     only when the panel is (re)filled, so it stood still while the mouse stayed on the scarecrow);
 *   - each hit: "<unit>: <skill> <damage>[ <Crit|Guard>]" ("Miss" / "Immune" with no number), the unit's name, the skill
 *     or attack in the game's language, the damage as the game's own floating number shows it. Into the multiplayer
 *     mod's chat (mp_api v3 chat_local: this screen only, never sent, no rate limit); without it (or with "Chat: Off")
 *     into a small log panel of our own (wheel scrolls it).
 * Everything here is only active in a world loaded from a DPS Check save (tfw_dps_made); anywhere else every hook passes
 * straight through. Nothing changes the game's state: the hooks read, the panel only shows.
 *
 * The hit events below are the ONE source of the chat log and the DPS meter (tf_meter.c): both always agree. */
#pragma once
#include "dsl_sdk.h"
#include <stddef.h>
#include <stdint.h>

int  tf_dps_init(const DSLHost *host, DSLPlugin *self);   /* after il2cpp_init (tfw_init). 0 ok */
void tf_dps_world(const char *save_stem);                 /* a campaign load starts (tf_worlds.c h_deser): DPS world or not */

/* ---- the hits (one per DamageSimulator.Simulate on the scarecrow) */
enum { TFD_NORMAL, TFD_CRIT, TFD_GUARD, TFD_MISS, TFD_IMMUNE };   /* Refactor.Combat.ExecutionDataResult */
typedef struct tf_hit {
    double t;                    /* tf_dps_clock() at the hit */
    float dmg;                   /* TotalDamage (0 for a miss / immune) */
    int res;                     /* TFD_* */
    int basic;                   /* 1 = an auto attack (shown as the game's "Basic attack") */
    int unit;                    /* index into tf_dps_unit() (one per attacking unit, first hit first) */
    char skill_key[80];          /* SKILL_... / AFFECTER_... ("" unknown) */
    char skill[96];              /* its name in the game's language at the hit ("Basic attack" for an auto attack) */
} tf_hit;
typedef struct tf_unit {
    uint8_t guid[16];            /* the unit's entity Guid */
    char name[96];               /* EntityDisplayNameExtensions.GetDisplayName (as the stock unit panel) */
    char portrait[64];           /* UnitEntity.PortraitID ("" none): cm_icon(CM_ICON_PORTRAIT, ...) */
} tf_unit;
int    tf_dps_active(void);                  /* 1 = a DPS Check world is loaded and shown (not the title) */
size_t tf_dps_hits(const tf_hit **out);      /* this fight's hits, oldest first; the pointer is valid until the next hit */
const tf_unit *tf_dps_unit(int i);           /* NULL past the end */
int    tf_dps_units(void);
double tf_dps_clock(void);                   /* game seconds (stops while the game is paused, follows its speed) */
double tf_dps_repaired(void);                /* durability the scarecrow repaired itself this fight (sum of its rises) */
double tf_dps_durability(double *max);       /* the scarecrow's durability now (-1 = not known yet) */

/* ---- switches (remembered: cm_prefs); defaults: speech off, heal numbers off, log in chat on, start on first hit on */
enum { TFD_SW_SPEECH, TFD_SW_HEALNUM, TFD_SW_CHATLOG, TFD_SW_FIRSTHIT, TFD_SW_N };
int  tf_dps_switch(int which);
void tf_dps_switch_set(int which, int on);
const char *tf_dps_switch_label(int which);  /* translated label ("Scarecrow speech", ...) */
/* the DPS Check part of the Test Fights setup (tf_ui.c, under the DPS card): the switches as toggles; returns its height */
float tf_dps_setup_rows(DSLObj parent, float y, float w);

/* ---- Restart test (every Test Fights world): the same test again from the save the Start made (tf_worlds.c) */
int  tf_restart_ready(void);                 /* 1 = a test world is loaded and its start save is kept */
int  tf_restart(void);                       /* 0 = the load is queued */
void tf_dps_restarting(void);                /* tf_worlds.c: the next DPS world is a restart (log starts "--- restart ---") */
