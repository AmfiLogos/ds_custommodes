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

/* ---- the events (ask 10): damage, heals, shield absorbs and effects put on, where a squad unit or the scarecrow takes
   part. Damage on the scarecrow is the DPS Check's measure (tf_dps_hits with kind TFK_DMG and on_dummy). */
enum { TFD_NORMAL, TFD_CRIT, TFD_GUARD, TFD_MISS, TFD_IMMUNE };   /* Refactor.Combat.ExecutionDataResult */
enum { TFK_DMG, TFK_HEAL, TFK_ABSORB, TFK_EFFECT };
typedef struct tf_hit {
    double t;                    /* tf_dps_clock() at the event */
    long long tick;              /* the game tick (one chat line per unit per tick) */
    int kind;                    /* TFK_* */
    float dmg;                   /* damage dealt (TotalDamage; 0 for a miss / immune), heal added, absorbed amount */
    float over;                  /* heal: the part past max health */
    int res;                     /* TFD_* */
    int basic;                   /* 1 = the game marks the skill a basic attack (SkillTableData.IsBasicAttackSkill) */
    int unit;                    /* the source: index into tf_dps_unit() */
    int target;                  /* the target: index into tf_dps_unit() */
    int on_dummy;                /* the target is the scarecrow */
    char skill_key[80];          /* SKILL_... / AFFECTER_... ("" unknown) */
    char skill[96];              /* its name in the game's language at the event ("<name> (basic)" for a basic attack) */
} tf_hit;
typedef struct tf_unit {
    uint8_t guid[16];            /* the entity's Guid */
    char name[96];               /* EntityDisplayNameExtensions.GetDisplayName (as the stock unit panel), numbered if repeated */
    char portrait[64];           /* UnitEntity.PortraitID ("" none): cm_icon(CM_ICON_PORTRAIT, ...) */
    int is_player, is_dummy;     /* a squad unit (EntityExtensions.IsPlayerUnit) / the scarecrow */
} tf_unit;
int    tf_dps_active(void);                  /* 1 = a DPS Check world is loaded and shown (not the title) */
size_t tf_dps_hits(const tf_hit **out);      /* this fight's hits, oldest first; the pointer is valid until the next hit */
const tf_unit *tf_dps_unit(int i);           /* NULL past the end */
int    tf_dps_units(void);
double tf_dps_clock(void);                   /* game seconds (stops while the game is paused, follows its speed) */
double tf_dps_repaired(void);                /* durability the scarecrow repaired itself this fight (sum of its rises) */
double tf_dps_durability(double *max);       /* the scarecrow's durability now (-1 = not known yet) */

/* ---- switches (remembered: cm_prefs); defaults: speech off, heal numbers off, log in chat on, start on first hit on */
enum { TFD_SW_SPEECH, TFD_SW_HEALNUM, TFD_SW_CHATLOG, TFD_SW_FIRSTHIT, TFD_SW_EFFECTS, TFD_SW_N };   /* effects in log: on */
int  tf_dps_switch(int which);
void tf_dps_switch_set(int which, int on);
const char *tf_dps_switch_label(int which);  /* translated label ("Scarecrow speech", ...) */
/* the DPS Check part of the Test Fights setup (tf_ui.c, under the DPS card): the switches as toggles; returns its height */

/* ---- Restart test (every Test Fights world): the same test again from the save the Start made (tf_worlds.c) */
int  tf_restart_ready(void);                 /* 1 = a test world is loaded and its start save is kept */
int  tf_restart(void);                       /* 0 = the load is queued */
void tf_dps_restarting(void);                /* tf_worlds.c: the next DPS world is a restart (log starts "--- restart ---") */
