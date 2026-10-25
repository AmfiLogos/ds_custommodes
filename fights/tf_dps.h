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
 * straight through. Nothing changes the game's state: the hooks read, the panel only shows. */
#pragma once
#include "dsl_sdk.h"

int  tf_dps_init(const DSLHost *host, DSLPlugin *self);   /* after il2cpp_init (tfw_init). 0 ok */
void tf_dps_world(const char *save_stem);                 /* a campaign load starts (tf_worlds.c h_deser): DPS world or not */
