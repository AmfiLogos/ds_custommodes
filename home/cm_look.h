// cm_look.h - Units Laboratory's widgets in the game's own text look (owner 2026-10-24: "Font inside of the units lab are
// not the same as the rest of the menus"). The loader kit draws body text with the body font's plain material and the
// title column's buttons at 18; the game's own menus (title column, Options, Load Campaign) draw body text and buttons with
// the language's body font and its SoftOutline16 material, section heads in the title font with SoftOutline16, and the
// title column's labels at 22 (checked in a test copy, build 25846389; cm_look.c says which window shows what).
#pragma once
#include "dsl_sdk.h"

/* the kit's table with the game look: text / button / toggle / value_row style the texts of the widget they make
   (LanguageSetting.GetFont / GetMaterial: the language's own faces). Every other call is the kit's. NULL = no kit. */
const DSLUi *cm_look_ui(const DSLHost *host, DSLPlugin *self);
/* a button of the Units Laboratory page in the title column: its label as the stock title column's (size 22) */
void cm_look_title_button(DSLObj button);
