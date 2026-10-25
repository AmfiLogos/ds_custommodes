/* tf_card - the game's own data behind the Test Fights boss card (owner v0.1.11 #2: the wiki's boss preview, art + name +
 * key stats, rebuilt in game style from the game's data and sprites; nothing is downloaded for it):
 *   stats  = the unit's UnitTable row (DataSheetManager.GetUnitData(key)), the rows the wiki's unit card shows
 *   art    = the unit prefab's Body sprite (ResourceLoader.LoadUnitObject / LoadAbyssObject for an Abyss unit), the
 *            same sprite the wiki's unit art is cut from (pipeline extract_db_icons.py unit_art)
 *   labels = the game's stat names, TEXTKEY_STATUS_<field>_NAME
 * Main thread. */
#pragma once
#include "cm_il.h"

enum { TFC_HP, TFC_PATK, TFC_MATK, TFC_ASPD, TFC_CRIT, TFC_PRES, TFC_MRES, TFC_MOVE, TFC_N };
extern const char *const tfc_field[TFC_N];               /* UnitTable field names, in card order */
int tfc_unit_stats(const char *unit_key, float out[TFC_N]);   /* 0 ok */
Obj tfc_unit_sprite(const char *unit_key);                     /* the Body sprite, or NULL */
Obj tfc_unit_art(const char *unit_key);                        /* the Body sprite cut to its visible pixels (else as it is), or NULL */
/* the game's own tables know this key: kind 0 = an item (DataSheetManager.GetItemData), 1 = an affecter (GetAffecterData).
   Both end in Dictionary GetValueOrDefault (checked in the 25846389 code): a missing key gives NULL, never an exception.
   1 known, 0 not known; tables not loaded yet = 1 (nothing is left out on a guess). For World units (mu_known_fn). */
int tfc_known(int kind, const char *key);
