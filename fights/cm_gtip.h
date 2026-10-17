/* cm_gtip - the game's OWN hover card for a game entry on a kit widget (owner v0.1.11 #5: hovering the inscription icon
 * shows the game's hover card for that inscription). The kit's U->tooltip shows a title/body card; this one asks the
 * game's TooltipUI for the card of a key (TooltipUI.ShowTooltipAtScreenPosition(id, type, key, ...)), so the card is the
 * one the game shows for that inscription / item / skill, in the game's language. Hover = a per-frame mouse test. Main
 * thread. */
#pragma once
#include "dsl_sdk.h"

enum { CM_GTIP_SKILL = 1, CM_GTIP_AFFECTER = 2, CM_GTIP_ITEM = 3 };   /* Refactor.UI.TooltipType */
void cm_gtip_add(DSLObj target, int type, const char *key);         /* replaces the target's entry; NULL key = off */
void cm_gtip_test_hover(DSLObj target);                              /* test: as if the mouse were on target (NULL = off) */
int  cm_gtip_shown(char *key_out, size_t n);                         /* test: 1 = a card is shown (key_out = its key) */
