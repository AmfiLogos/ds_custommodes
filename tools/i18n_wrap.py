#!/usr/bin/env python3
"""i18n_wrap.py - the i18n-solo lane's text wrapping (TR / TRN) of the solo part, as exact edits.

  python3 tools/i18n_wrap.py [--check]     from the repo root

Each edit is (old text, new text) in one file. An edit whose new text is already there is skipped (done); an edit whose
old text is there exactly once is applied; anything else is reported (the source moved on: redo that edit by hand).
Kept so the wrapping can be run again on a newer main. Logic changes (cm_i18n.c, by-value fixes, game words) are in git.
"""
import os
import sys

EDITS = {}

EDITS["units/mu_window.c"] = [
('''#include "cm_il.h"
#include <stdio.h>''', '''#include "cm_il.h"
#include "i18n/ds_i18n.h"
#include <stdio.h>'''),
('''snprintf(m, sizeof m, "Could not save My Units: %s", W.lib ? e : W.err);''', '''snprintf(m, sizeof m, TR("Could not save My Units: %s"), W.lib ? e : W.err);'''),
('''set_msg("Reading the link...", MU_C_SUB);''', '''set_msg(TR("Reading the link..."), MU_C_SUB);'''),
('''set_msg("No wiki link on the clipboard. Paste one below.", MU_C_SUB);''', '''set_msg(TR("No wiki link on the clipboard. Paste one below."), MU_C_SUB);'''),
('''set_msg("That is not a wiki builder link. Copy the link from the wiki's Unit Builder.", MU_C_BAD);''', '''set_msg(TR("That is not a wiki builder link. Copy the link from the wiki's Unit Builder."), MU_C_BAD);'''),
('''set_msg("Saved to My Units.", MU_C_GOLD);''', '''set_msg(TR("Saved to My Units."), MU_C_GOLD);'''),
('''snprintf(m, sizeof m, "A team holds %d units at most.", MU_TEAM_MAX);''', '''snprintf(m, sizeof m, TRN("A team holds %d unit at most.", "A team holds %d units at most.", MU_TEAM_MAX), MU_TEAM_MAX);'''),
('''snprintf(nm, sizeof nm, "Team %zu", mu_team_count(W.lib) + 1);''', '''snprintf(nm, sizeof nm, TR("Team %zu"), mu_team_count(W.lib) + 1); dsi_utf8_fix_end(nm);'''),
('''snprintf(m, sizeof m, "Saved %d units as %s.", W.nticked, nm);''', '''snprintf(m, sizeof m, TRN("Saved %d unit as %s.", "Saved %d units as %s.", W.nticked), W.nticked, nm);'''),
('''set_msg("Type a name first.", MU_C_BAD);''', '''set_msg(TR("Type a name first."), MU_C_BAD);'''),
('''set_msg("Renamed.", MU_C_GOLD);''', '''set_msg(TR("Renamed."), MU_C_GOLD);'''),
('''snprintf(m, sizeof m, "Deleted %d unit%s.", n, n == 1 ? "" : "s");''', '''snprintf(m, sizeof m, TRN("Deleted %d unit.", "Deleted %d units.", n), n);'''),
('''snprintf(m, sizeof m, "Delete %d unit%s from My Units?\\nThey also leave their teams. Your saves are not changed.", W.nticked, W.nticked == 1 ? "" : "s");
    U->notice(m, "Delete", "Cancel", del_ok, NULL, NULL);''', '''snprintf(m, sizeof m, TRN("Delete %d unit from My Units?\\nIt also leaves its teams. Your saves are not changed.",
                              "Delete %d units from My Units?\\nThey also leave their teams. Your saves are not changed.", W.nticked), W.nticked);
    U->notice(m, TR("Delete"), TR("Cancel"), del_ok, NULL, NULL);'''),
('''set_msg("Team deleted. Its units stay in My Units.", MU_C_GOLD);''', '''set_msg(TR("Team deleted. Its units stay in My Units."), MU_C_GOLD);'''),
('''U->notice("Delete this team?\\nIts units stay in My Units.", "Delete team", "Cancel", delteam_ok, NULL, NULL);''', '''U->notice(TR("Delete this team?\\nIts units stay in My Units."), TR("Delete team"), TR("Cancel"), delteam_ok, NULL, NULL);'''),
('''    float y = 0; char t[200];
    snprintf(t, sizeof t, "%sAll units (%zu)%s", W.sel_team < 0 ? "<color=#D9BE8F>" : "", W.lib ? mu_count(W.lib) : (size_t)0, W.sel_team < 0 ? "</color>" : "");''',
'''    float y = 0; char t[200], all[160];
    snprintf(all, sizeof all, TR("All units (%zu)"), W.lib ? mu_count(W.lib) : (size_t)0);
    snprintf(t, sizeof t, "%s%s%s", W.sel_team < 0 ? "<color=#D9BE8F>" : "", all, W.sel_team < 0 ? "</color>" : "");'''),
('''        mu_text(W.right, 16, 16, W.rw - 32, 90, W.sel_team < 0
            ? "No units yet. On the wiki, open the Unit Builder, copy the link of a unit or a team, then press \\"Add from wiki link\\"."
            : "This team has no units.", 16, DSL_TXT_WRAP, MU_C_SUB);''', '''        mu_text(W.right, 16, 16, W.rw - 32, 90, W.sel_team < 0
            ? TR("No units yet. On the wiki, open the Unit Builder, copy the link of a unit or a team, then press \\"Add from wiki link\\".")
            : TR("This team has no units."), 16, DSL_TXT_WRAP, MU_C_SUB);'''),
('''char t[64]; snprintf(t, sizeof t, "%d selected", W.nticked);''', '''char t[64]; snprintf(t, sizeof t, TRN("%d selected", "%d selected", W.nticked), W.nticked);'''),
('''W.win = mu_window("My Units", 1100, 720, &w, &h);''', '''W.win = mu_window(TR("My Units"), 1100, 720, &w, &h);'''),
('''W.b_add = mu_button(W.win, x, y, 220, 40, "Add from wiki link", on_add, NULL); x += 230;
    U->tooltip(W.b_add, "Add from wiki link", "Copy a unit or team link in the wiki's Unit Builder, then press here: it is read from the clipboard.");''',
'''W.b_add = mu_button(W.win, x, y, 220, 40, TR("Add from wiki link"), on_add, NULL); x += 230;
    U->tooltip(W.b_add, TR("Add from wiki link"), TR("Copy a unit or team link in the wiki's Unit Builder, then press here: it is read from the clipboard."));'''),
('''W.b_new = mu_button(W.win, x, y, 150, 40, "New unit", on_new, NULL);''', '''W.b_new = mu_button(W.win, x, y, 150, 40, TR("New unit"), on_new, NULL);'''),
('''W.b_close = mu_button(W.win, w - 130, y, 110, 40, "Close", on_close, NULL);''', '''W.b_close = mu_button(W.win, w - 130, y, 110, 40, TR("Close"), on_close, NULL);'''),
('''"Paste a wiki builder link", "", on_box_add, NULL);
        W.box_add = mu_button(W.box, w - 40 - 250, 6, 120, 40, "Add", on_box_add, NULL);
        W.box_cancel = mu_button(W.box, w - 40 - 125, 6, 117, 40, "Cancel", on_box_cancel, NULL);''',
'''TR("Paste a wiki builder link"), "", on_box_add, NULL);
        W.box_add = mu_button(W.box, w - 40 - 250, 6, 120, 40, TR("Add"), on_box_add, NULL);
        W.box_cancel = mu_button(W.box, w - 40 - 125, 6, 117, 40, TR("Cancel"), on_box_cancel, NULL);'''),
('''"Save as team", on_save_team, NULL); bx += bw;
        W.b_rename = mu_button(W.bottom, bx, 2, bw - 6, 40, "Rename", on_rename, NULL); bx += bw;''',
'''TR("Save as team"), on_save_team, NULL); bx += bw;
        W.b_rename = mu_button(W.bottom, bx, 2, bw - 6, 40, TR("Rename"), on_rename, NULL); bx += bw;'''),
('''W.b_edit = mu_button(W.bottom, bx, 2, bw - 6, 40, "Edit", on_edit, NULL);''', '''W.b_edit = mu_button(W.bottom, bx, 2, bw - 6, 40, TR("Edit"), on_edit, NULL);'''),
('''W.b_delete = mu_button(W.bottom, bx, 2, bw - 6, 40, "Delete", on_delete, NULL); bx += bw;
        W.b_delteam = mu_button(W.bottom, bx, 2, bw - 6, 40, "Delete team", on_delteam, NULL); bx += bw;''',
'''W.b_delete = mu_button(W.bottom, bx, 2, bw - 6, 40, TR("Delete"), on_delete, NULL); bx += bw;
        W.b_delteam = mu_button(W.bottom, bx, 2, bw - 6, 40, TR("Delete team"), on_delteam, NULL); bx += bw;'''),
('''mu_text(W.rename_row, 8, 0, 70, 44, "Name", 14, DSL_TXT_LEFT, MU_C_SUB);
        W.rename_field = U->field(W.rename_row, 80, 2, w - 40 - 80 - 260, 40, "Unit name", "", on_rename_ok, NULL);
        W.b_rename_ok = mu_button(W.rename_row, w - 40 - 250, 2, 120, 40, "Save", on_rename_ok, NULL);
        mu_button(W.rename_row, w - 40 - 125, 2, 120, 40, "Cancel", on_rename_cancel, NULL);''',
'''mu_text(W.rename_row, 8, 0, 70, 44, TR("Name"), 14, DSL_TXT_LEFT, MU_C_SUB);
        W.rename_field = U->field(W.rename_row, 80, 2, w - 40 - 80 - 260, 40, TR("Unit name"), "", on_rename_ok, NULL);
        W.b_rename_ok = mu_button(W.rename_row, w - 40 - 250, 2, 120, 40, TR("Save"), on_rename_ok, NULL);
        mu_button(W.rename_row, w - 40 - 125, 2, 120, 40, TR("Cancel"), on_rename_cancel, NULL);'''),
('''int mu_window_is_open(void) { return W.open; }''', '''int mu_window_is_open(void) { return W.open; }
/* the language changed: the open window is built again (same state), as after a screen layer change */
void mu_window_relang(void) {
    if (!W.open || !W.win) return;
    U->destroy(W.win); W.win = NULL; mu_drawn_reset();
    if (build_ui()) { close_win(); return; }
    refresh();
}'''),
]

EDITS["units/mu_bring.c"] = [
('''#include "cm_il.h"
#include <stdio.h>''', '''#include "cm_il.h"
#include "i18n/ds_i18n.h"
#include <stdio.h>'''),
('''    if (!o) snprintf(out, cap, "Save");''', '''    if (!o) snprintf(out, cap, "%s", TR("Save"));'''),
('''        if (!B.su[i].name[0]) snprintf(B.su[i].name, sizeof B.su[i].name, "Unit");''', '''        if (!B.su[i].name[0]) snprintf(B.su[i].name, sizeof B.su[i].name, "%s", TR("Unit"));'''),
('''char m[200]; snprintf(m, sizeof m, "You can bring %d unit%s at most. Untick one first.", B.max, B.max == 1 ? "" : "s");''',
 '''char m[200]; snprintf(m, sizeof m, TRN("You can bring %d unit at most. Untick one first.", "You can bring %d units at most. Untick one first.", B.max), B.max);'''),
('''if (left) snprintf(m, sizeof m, "Ticked %d of the %d units of %s: you can bring %d at most.", added + had, n, nm, B.max);
        else snprintf(m, sizeof m, "Ticked the %d units of %s.", n, nm);''',
 '''if (left) snprintf(m, sizeof m, TR("Ticked %d of the %d units of %s: you can bring %d at most."), added + had, n, nm, B.max);
        else snprintf(m, sizeof m, TRN("Ticked the %d unit of %s.", "Ticked the %d units of %s.", n), n, nm);'''),
('''snprintf(m, sizeof m, "Saved to My Units. Not ticked: you can bring %d at most.", B.max); set_msg(m, MU_C_BAD); }
        else set_msg("Saved to My Units and ticked.", MU_C_GOLD);''',
 '''snprintf(m, sizeof m, TR("Saved to My Units. Not ticked: you can bring %d at most."), B.max); set_msg(m, MU_C_BAD); }
        else set_msg(TR("Saved to My Units and ticked."), MU_C_GOLD);'''),
('''    char t[80];
    if (B.tab_save) { snprintf(t, sizeof t, "%sFrom a save%s", B.tab == MU_SRC_SAVE ? "<color=#D9BE8F>" : "", B.tab == MU_SRC_SAVE ? "</color>" : ""); U->set_text(B.tab_save, t); }
    if (B.tab_lib) { snprintf(t, sizeof t, "%sMy Units%s", B.tab == MU_SRC_LIBRARY ? "<color=#D9BE8F>" : "", B.tab == MU_SRC_LIBRARY ? "</color>" : ""); U->set_text(B.tab_lib, t); }''',
 '''    char t[200];
    if (B.tab_save) { snprintf(t, sizeof t, "%s%s%s", B.tab == MU_SRC_SAVE ? "<color=#D9BE8F>" : "", TR("From a save"), B.tab == MU_SRC_SAVE ? "</color>" : ""); U->set_text(B.tab_save, t); }
    if (B.tab_lib) { snprintf(t, sizeof t, "%s%s%s", B.tab == MU_SRC_LIBRARY ? "<color=#D9BE8F>" : "", TR("My Units"), B.tab == MU_SRC_LIBRARY ? "</color>" : ""); U->set_text(B.tab_lib, t); }'''),
('''char body[64]; snprintf(body, sizeof body, "%s%s", B.saves[i].when[0] ? "Saved " : "", B.saves[i].when);''',
 '''char body[160] = ""; if (B.saves[i].when[0]) snprintf(body, sizeof body, TR("Saved %s"), B.saves[i].when);'''),
('''"No saves found.", 15, DSL_TXT_WRAP, MU_C_SUB);''', '''TR("No saves found."), 15, DSL_TXT_WRAP, MU_C_SUB);'''),
('''        snprintf(t, sizeof t, "All units (%zu)", B.lib ? mu_count(B.lib) : (size_t)0);''', '''        snprintf(t, sizeof t, TR("All units (%zu)"), B.lib ? mu_count(B.lib) : (size_t)0);'''),
('''left_row(y, t, B.sel_team == (int)i, (int)i, nm, "Pick this team: its units are ticked.");''', '''left_row(y, t, B.sel_team == (int)i, (int)i, nm, TR("Pick this team: its units are ticked."));'''),
('''        if (!B.nsaves) empty = "No saves to bring units from.";''', '''        if (!B.nsaves) empty = TR("No saves to bring units from.");'''),
('''        else if (!n) empty = "This save has no units.";''', '''        else if (!n) empty = TR("This save has no units.");'''),
('''else if (!n) empty = B.sel_team < 0 ? "No units in My Units yet. Add some in My Units (home page), or press \\"New unit\\"." : "This team has no units.";''',
 '''else if (!n) empty = B.sel_team < 0 ? TR("No units in My Units yet. Add some in My Units (home page), or press \\"New unit\\".") : TR("This team has no units.");'''),
('''e[0] ? e : "This unit cannot be shown."''', '''e[0] ? e : TR("This unit cannot be shown.")'''),
('''char t[80]; snprintf(t, sizeof t, "Picked %d / %d", B.npicks, B.max);''', '''char t[120]; snprintf(t, sizeof t, TR("Picked %d / %d"), B.npicks, B.max);'''),
('''req->title && *req->title ? req->title : "Bring Units");''', '''req->title && *req->title ? req->title : TR("Bring Units")); dsi_utf8_fix_end(B.title);'''),
('''    snprintf(B.sub, sizeof B.sub, "%s", req->subtitle ? req->subtitle : "");''', '''    snprintf(B.sub, sizeof B.sub, "%s", req->subtitle ? req->subtitle : ""); dsi_utf8_fix_end(B.sub);'''),
('''        B.tab_save = mu_button(B.win, x, y, 170, 40, "From a save", on_tab, (void *)(intptr_t)MU_SRC_SAVE); x += 176;
        B.tab_lib = mu_button(B.win, x, y, 170, 40, "My Units", on_tab, (void *)(intptr_t)MU_SRC_LIBRARY); x += 180;''',
 '''        B.tab_save = mu_button(B.win, x, y, 170, 40, TR("From a save"), on_tab, (void *)(intptr_t)MU_SRC_SAVE); x += 176;
        B.tab_lib = mu_button(B.win, x, y, 170, 40, TR("My Units"), on_tab, (void *)(intptr_t)MU_SRC_LIBRARY); x += 180;'''),
('''B.tab == MU_SRC_SAVE ? "From a save" : "My Units", 19, DSL_TXT_LEFT, MU_C_GOLD);''', '''B.tab == MU_SRC_SAVE ? TR("From a save") : TR("My Units"), 19, DSL_TXT_LEFT, MU_C_GOLD);'''),
('''        B.b_new = mu_button(B.win, x, y, 140, 40, "New unit", on_new, NULL); x += 150;
        U->tooltip(B.b_new, "New unit", "Make a unit in the Unit Builder. When you save it, it goes to My Units and is ticked here.");''',
 '''        B.b_new = mu_button(B.win, x, y, 140, 40, TR("New unit"), on_new, NULL); x += 150;
        U->tooltip(B.b_new, TR("New unit"), TR("Make a unit in the Unit Builder. When you save it, it goes to My Units and is ticked here."));'''),
('''        B.b_cancel = mu_button(bot, bw - 290, 2, 140, 40, "Cancel", on_cancel, NULL);
        B.b_bring = mu_button(bot, bw - 140, 2, 140, 40, "Bring", on_bring, NULL);''',
 '''        B.b_cancel = mu_button(bot, bw - 290, 2, 140, 40, TR("Cancel"), on_cancel, NULL);
        B.b_bring = mu_button(bot, bw - 140, 2, 140, 40, TR("Bring"), on_bring, NULL);'''),
('''int  mu_bring_is_open(void) { return B.open; }''', '''int  mu_bring_is_open(void) { return B.open; }
/* the language changed: the open window is built again (same state), as after a screen layer change. The title and
   subtitle are the caller's (already in the language they were given in). */
void mu_bring_relang(void) {
    if (!B.open || !B.win) return;
    U->destroy(B.win); B.win = NULL; mu_drawn_reset();
    if (build_ui()) { finish(0); return; }
    refresh();
}'''),
]

EDITS["units/mu_init.c"] = [
('''#include "fe/fe_json.h"''', '''#include "fe/fe_json.h"
#include "cm_i18n.h"'''),
('''size_t mu_bring_test_state(char *out, size_t cap);''', '''size_t mu_bring_test_state(char *out, size_t cap);
void mu_window_relang(void);
void mu_bring_relang(void);
static void units_relang(void) { mu_window_relang(); mu_bring_relang(); }'''),
('''    cm_home_add("my-units", "My Units", NULL, 10, mu_window_open);''', '''    cm_home_add("my-units", TR_NOOP("My Units"), NULL, 10, mu_window_open);
    cm_i18n_on_change(units_relang);'''),
('''mu_bring_req r = { .title = "Bring Units", .subtitle = "Test: pick your units",''', '''mu_bring_req r = { .title = TR("Bring Units"), .subtitle = "Test: pick your units",'''),
]

EDITS["units/mu_ui.c"] = [
('''#include "cm_il.h"
#include <stdarg.h>''', '''#include "cm_il.h"
#include "i18n/ds_i18n.h"
#include <stdarg.h>'''),
('''/* ------------------------------------------------------------------ Compact Plus card */''', '''/* the card's weapon word (mu_card_tables.h MUC_ITEM_WLABEL, the wiki's English label): the game's own text of that item
   type when the game has one (TEXTKEY), else TR of the label */
static const char *weapon_word(const char *en) {
    static const struct { const char *en, *key; } W[] = {
        { TR_NOOP("Sword"), "TEXTKEY_Equipment_Sword_NAME" }, { TR_NOOP("Greatsword"), "TEXTKEY_Equipment_Greatsword_NAME" },
        { TR_NOOP("Spear"), "TEXTKEY_Equipment_Spear_NAME" }, { TR_NOOP("Blunt"), "TEXTKEY_Equipment_Blunt_NAME" },
        { TR_NOOP("Bow"), "TEXTKEY_Equipment_Bow_NAME" }, { TR_NOOP("Shield"), "TEXTKEY_Equipment_Shield_NAME" },
        { TR_NOOP("Dagger"), "TEXTKEY_Equipment_Dagger_NAME" }, { TR_NOOP("Fire Staff"), "TEXTKEY_Equipment_FireStaff_NAME" },
        { TR_NOOP("Water Staff"), "TEXTKEY_Equipment_WaterStaff_NAME" }, { TR_NOOP("Nature Staff"), "TEXTKEY_Equipment_NatureStaff_NAME" },
        { TR_NOOP("Main Accessory"), "TEXTKEY_EquipmentType_Accessory1" }, { TR_NOOP("Sub Accessory"), "TEXTKEY_EquipmentType_Accessory2" },
        { TR_NOOP("Body Armor"), "TEXTKEY_EquipmentType_Body" }, { TR_NOOP("Torch"), "TEXTKEY_ITEM_Torch_NAME" },
        { TR_NOOP("Arrow"), "TEXTKEY_Tooltip_RequiredAmmoAmmo_Arrow" },
        { TR_NOOP("Helmet"), NULL }, { TR_NOOP("Hood"), NULL }, { TR_NOOP("Axe"), NULL } };
    for (size_t i = 0; i < sizeof W / sizeof *W; i++) if (!strcmp(W[i].en, en)) return cm_gw(W[i].key, W[i].en);
    return TR(en);
}

/* ------------------------------------------------------------------ Compact Plus card */'''),
('''char lv[32]; snprintf(lv, sizeof lv, "Lv %d", level > 0 ? level : c->lvl);''', '''char lv[64]; snprintf(lv, sizeof lv, TR("Lv %d"), level > 0 ? level : c->lvl);'''),
('''else snprintf(nm, sizeof nm, "%s", c->name[0] ? c->name : "Unit");''', '''else snprintf(nm, sizeof nm, "%s", c->name[0] ? c->name : TR("Unit"));'''),
('''    snprintf(line, sizeof line, "%s%s%s%s%s", rn, bg[0] ? "  \\xC2\\xB7  " : "", bg, c->weapon ? "  \\xC2\\xB7  " : "", c->weapon ? c->weapon : "");''',
 '''    snprintf(line, sizeof line, "%s%s%s%s%s", rn, bg[0] ? "  \\xC2\\xB7  " : "", bg, c->weapon ? "  \\xC2\\xB7  " : "", c->weapon ? weapon_word(c->weapon) : "");
    dsi_utf8_fix_end(line);'''),
]

EDITS["units/mu_link.c"] = [
('''        snprintf(res.msg, sizeof res.msg, "%s", *t ? "That is not a wiki builder link. Copy a team or unit link on the wiki builder, or paste it here."
                                                   : "Copy a team or unit link on the wiki builder, or paste it here.");''',
 '''        snprintf(res.msg, sizeof res.msg, "%s", *t ? TR("That is not a wiki builder link. Copy a team or unit link on the wiki builder, or paste it here.")
                                                   : TR("Copy a team or unit link on the wiki builder, or paste it here."));'''),
('''state = MUL_FAILED; snprintf(res.msg, sizeof res.msg, "Could not add to My Units: %s", e);''', '''state = MUL_FAILED; snprintf(res.msg, sizeof res.msg, TR("Could not add to My Units: %s"), e);'''),
('''                if (w_n == 1) snprintf(res.msg, sizeof res.msg, "Added 1 unit to My Units: %s", nm);
                else snprintf(res.msg, sizeof res.msg, "Added %d units to My Units as the team %s", w_n, nm);''',
 '''                if (w_n == 1) snprintf(res.msg, sizeof res.msg, TR("Added 1 unit to My Units: %s"), nm);
                else snprintf(res.msg, sizeof res.msg, TRN("Added %d unit to My Units as the team %s", "Added %d units to My Units as the team %s", w_n), w_n, nm);'''),
]

EDITS["units/pvp_wiki.c"] = [
('''        if (ix < 0) { fail(E, "%s %.*s is not in this game version", what,''', '''        if (ix < 0) { fail(E, TR("%s %.*s is not in this game version"), TR(what),'''),
('''if (!j || j->t != JN_OBJ) { fail(&E, "not a unit build"); return -1; }''', '''if (!j || j->t != JN_OBJ) { fail(&E, "%s", TR("not a unit build")); return -1; }'''),
('''if (b->race < 0) { fail(&E, "race %.*s is not in this game version",''', '''if (b->race < 0) { fail(&E, TR("race %.*s is not in this game version"),'''),
('''if (b->bg < 0) { fail(&E, "background %.*s is not in this game version",''', '''if (b->bg < 0) { fail(&E, TR("background %.*s is not in this game version"),'''),
('''if (it < 0) { fail(&E, "item %.*s is not in this game version",''', '''if (it < 0) { fail(&E, TR("item %.*s is not in this game version"),'''),
('''b->ninds, b->inds, "trait") ||''', '''b->ninds, b->inds, TR_NOOP("trait")) ||'''),
('''b->ntrees, b->trees, "skill tree") ||''', '''b->ntrees, b->trees, TR_NOOP("skill tree")) ||'''),
('''b->nlearned, b->learned, "skill") ||''', '''b->nlearned, b->learned, TR_NOOP("skill")) ||'''),
('''b->ninsc, b->insc, "inscription")) return -1;''', '''b->ninsc, b->insc, TR_NOOP("inscription"))) return -1;'''),
('''b->nsouls, b->souls, "soul stone") ||''', '''b->nsouls, b->souls, TR_NOOP("soul stone")) ||'''),
('''b->nacq, b->acq, "acquired trait")) return -1;''', '''b->nacq, b->acq, TR_NOOP("acquired trait"))) return -1;'''),
]

# wk_http.c: the "network down, do not try the other backend" test compared the error TEXT ("timed out", "no internet",
# curl's "Timeout"/"resolve"): now a flag the backends set, so the texts can be translated
EDITS["units/wk_http.c"] = [
('''static const char *backend = "";''', '''static const char *backend = "";
static int net_down __attribute__((unused));   /* the last backend saw no network (timeout, no name resolution): set by the backends */'''),
('''    if (r) { snprintf(err, errn, "%s", strerr ? strerr(r) : "curl error"); cleanup(c); return -1; }''',
 '''    if (r) { net_down = r == 28 || r == 6 || r == 5; snprintf(err, errn, "%s", strerr ? strerr(r) : "curl error"); cleanup(c); return -1; }   /* curl's OPERATION_TIMEDOUT, COULDNT_RESOLVE_HOST / _PROXY */'''),
('''    if (!x.done) { snprintf(err, errn, "timed out"); goto out; }''', '''    if (!x.done) { net_down = 1; snprintf(err, errn, "%s", TR("timed out")); goto out; }'''),
('''        snprintf(err, errn, "%s (error %ld)", code == -1009 || code == 8 || code == 12 ? "no internet" : "connection failed", code);''',
 '''        net_down = code == -1009 || code == 8 || code == 12;
        snprintf(err, errn, net_down ? TR("no internet (error %ld)") : TR("connection failed (error %ld)"), code);'''),
('''        int r = use_curl ? curl_get''', '''        net_down = 0;
        int r = use_curl ? curl_get'''),
('''        if (strstr(e1, "timed out") || strstr(e1, "no internet") || strstr(e1, "Timeout") || strstr(e1, "resolve")) break;''',
 '''        if (net_down) break;'''),
('''        snprintf(err, errn, e == ERROR_WINHTTP_TIMEOUT ? "timed out" : e == ERROR_WINHTTP_NAME_NOT_RESOLVED ? "no internet (name not resolved)" : e == ERROR_WINHTTP_CANNOT_CONNECT ? "could not connect" : "connection error %lu", e);''',
 '''        snprintf(err, errn, e == ERROR_WINHTTP_TIMEOUT ? TR("timed out") : e == ERROR_WINHTTP_NAME_NOT_RESOLVED ? TR("no internet (name not resolved)") : e == ERROR_WINHTTP_CANNOT_CONNECT ? TR("could not connect") : TR("connection error %lu"), e);'''),
]

EDITS["units/mu_link.c"].insert(0, ('''#include "wk_link.h"
#include <stdio.h>''', '''#include "wk_link.h"
#include "i18n/ds_i18n.h"
#include <stdio.h>'''))
EDITS["units/pvp_wiki.c"].insert(0, ('''#include "pvp_wiki.h"
#include "pvp_wiki_tables.h"''', '''#include "pvp_wiki.h"
#include "pvp_wiki_tables.h"
#include "i18n/ds_i18n.h"'''))

# Test Fights: our texts through TR; game words (boss / monster / dummy / inscription names, difficulty tiers, Combat,
# Management, Magic Saturation) from the game's TextKeyTable by the keys the wiki's panel JSON carries
EDITS["fights/tf_ui.c"] = [
('''#include "../fe/fe_json.h"
#include <stdint.h>''', '''#include "../fe/fe_json.h"
#include "cm_il.h"
#include "cm_i18n.h"
#include <stdint.h>'''),
('''typedef struct { char id[24], n[96]; int abyss; } Boss;
typedef struct { char id[24], n[96]; int slots; double w; int on; } Kit;
typedef struct { char k[80], n[96]; } Mon;''', '''typedef struct { char id[24], n[96], key[80], tn[96]; int abyss; } Boss;      /* tn: the name shown (game text of key) */
typedef struct { char id[24], n[96]; int slots; double w; int on; } Kit;
typedef struct { char k[80], n[96], tn[96]; } Mon;'''),
('''    char dummy_n[96]; double dummy_dur;
    char def_mode[16], def_boss[24], def_combat[24];
} P;''', '''    char dummy_n[96]; double dummy_dur;
    char def_mode[16], def_boss[24], def_combat[24];
    char dummy_k[80], insc_k[80], dummy_tn[96], insc_tn[96], tlabel[8][64];   /* game names (names_refresh) */
    unsigned names_gen;                                                     /* dsi_generation() + 1 they are for */
} P;'''),
('''        else if (*s) o[k++] = *s;
        else break;
    }
    o[k] = 0;
}
static void jtxt(''', '''        else if (*s) o[k++] = *s;
        else break;
    }
    o[k] = 0;
    dsi_utf8_fix_end(o);                                          /* the cap may fall inside a character */
}
static void jtxt('''),
('''jtxt(jget(a->v[i], "n"), b->n, sizeof b->n); b->abyss = jtruthy(jget(a->v[i], "abyss"));''',
 '''jtxt(jget(a->v[i], "n"), b->n, sizeof b->n); b->abyss = jtruthy(jget(a->v[i], "abyss"));
        jtxt(jget(a->v[i], "key"), b->key, sizeof b->key);'''),
('''    jtxt(jget(jget(r, "kitInsc"), "n"), P.insc_n, sizeof P.insc_n);''', '''    jtxt(jget(jget(r, "kitInsc"), "n"), P.insc_n, sizeof P.insc_n); jtxt(jget(jget(r, "kitInsc"), "k"), P.insc_k, sizeof P.insc_k);'''),
('''        snprintf(P.miss[P.nmiss++], 160, "Left out of the kits: %s (%s).", k, why);''', '''        snprintf(P.miss[P.nmiss++], 160, TR("Left out of the kits: %s (%s)."), k, why); dsi_utf8_fix_end(P.miss[P.nmiss - 1]);'''),
('''    if (d && d->t == JN_OBJ) { jtxt(jget(d, "n"), P.dummy_n, sizeof P.dummy_n); P.dummy_dur = jnumv(jget(d, "dur")); }''',
 '''    if (d && d->t == JN_OBJ) { jtxt(jget(d, "n"), P.dummy_n, sizeof P.dummy_n); jtxt(jget(d, "k"), P.dummy_k, sizeof P.dummy_k); P.dummy_dur = jnumv(jget(d, "dur")); }'''),
('''    P.ok = P.nmodes > 0 && P.ncombat > 0;
    return P.ok ? 0 : -1;
}''', '''    P.ok = P.nmodes > 0 && P.ncombat > 0;
    return P.ok ? 0 : -1;
}
/* the game's name of a wiki key (TEXTKEY_<key>_NAME), else the wiki's English name */
static void gname(const char *key, const char *en, char *out, size_t n) {
    char k[160];
    if (key && *key) { snprintf(k, sizeof k, "TEXTKEY_%s_NAME", key); cm_game_text(k, en, out, n); }
    else snprintf(out, n, "%s", en);
}
/* the names shown, in the current language: once per language (main thread, at draw time) */
static void names_refresh(void) {
    if (P.names_gen == dsi_generation() + 1) return;
    P.names_gen = dsi_generation() + 1;
    for (int i = 0; i < P.nbosses; i++) gname(P.bosses[i].key, P.bosses[i].n, P.bosses[i].tn, sizeof P.bosses[i].tn);
    for (int i = 0; i < P.nmons; i++) gname(P.mons[i].k, P.mons[i].n, P.mons[i].tn, sizeof P.mons[i].tn);
    gname(P.dummy_k, P.dummy_n, P.dummy_tn, sizeof P.dummy_tn);
    gname(P.insc_k, P.insc_n, P.insc_tn, sizeof P.insc_tn);
    for (int i = 0; i < P.ncombat; i++) {                            /* TEXTKEY_CreateCampaign_<tier>Difficulty (tags removed) */
        char k[96]; snprintf(k, sizeof k, "TEXTKEY_CreateCampaign_%sDifficulty", P.combat[i]);
        cm_game_text(k, P.label[i], P.tlabel[i], sizeof P.tlabel[i]);
    }
}'''),
('''snprintf(eng_msg, sizeof eng_msg, "The wiki's Test Fights files are for game build %s, and this game is build %s. Test Fights opens again when both match.", P.tag, gb);''',
 '''snprintf(eng_msg, sizeof eng_msg, TR("The wiki's Test Fights files are for game build %s, and this game is build %s. Test Fights opens again when both match."), P.tag, gb);'''),
('''        snprintf(eng_msg, sizeof eng_msg, "%s", b->err[0] ? b->err : "The wiki's save generator gave no answer.");''',
 '''        snprintf(eng_msg, sizeof eng_msg, "%s", b->err[0] ? b->err : TR("The wiki's save generator gave no answer."));'''),
('''        size_t k = (size_t)snprintf(b->err, sizeof b->err, "%d problem(s) found - the test world was NOT made.", r.problems);''',
 '''        size_t k = (size_t)snprintf(b->err, sizeof b->err, TRN("%d problem found - the test world was NOT made.", "%d problems found - the test world was NOT made.", r.problems), r.problems);'''),
('''    if (!b->file) { snprintf(status, sizeof status, "%s", b->err[0] ? b->err : "The save generator gave no answer."); status_warn = 1; }''',
 '''    if (!b->file) { snprintf(status, sizeof status, "%s", b->err[0] ? b->err : TR("The save generator gave no answer.")); status_warn = 1; }'''),
('''snprintf(status, sizeof status, "The game did not take the load. Try again from the title screen."); status_warn = 1; }''',
 '''snprintf(status, sizeof status, "%s", TR("The game did not take the load. Try again from the title screen.")); status_warn = 1; }'''),
('''        snprintf(status, sizeof status, "Loading..."); status_warn = 0;''', '''        snprintf(status, sizeof status, "%s", TR("Loading...")); status_warn = 0;'''),
('''snprintf(status, sizeof status, "Add at least one monster to the room."); status_warn = 1;''', '''snprintf(status, sizeof status, "%s", TR("Add at least one monster to the room.")); status_warn = 1;'''),
('''busy = 1; snprintf(status, sizeof status, "Building your save file..."); status_warn = 0;''', '''busy = 1; snprintf(status, sizeof status, "%s", TR("Building your save file...")); status_warn = 0;'''),
('''snprintf(status, sizeof status, "An expedition holds %d units: untick one first.", P.max_squad); status_warn = 1; }''',
 '''snprintf(status, sizeof status, TRN("An expedition holds %d unit: untick one first.", "An expedition holds %d units: untick one first.", P.max_squad), P.max_squad); status_warn = 1; }'''),
('''    float y = head(left, 0, 0, LW, "Squad");
    const char *src_l[2] = { "My Units", "Gallery" };''', '''    float y = head(left, 0, 0, LW, TR("Squad"));
    const char *src_l[2] = { TR("My Units"), TR("Gallery") };'''),
('''        if (!lib) { txt(left, "fights:l.note", 0, y, LW, "My Units could not be read.", 15, WARN); return; }''',
 '''        if (!lib) { txt(left, "fights:l.note", 0, y, LW, TR("My Units could not be read."), 15, WARN); return; }'''),
('''"My Units is empty. Add units there (wiki link, Unit Builder) or pick a build in the Gallery.", 15, SUB); return; }''',
 '''TR("My Units is empty. Add units there (wiki link, Unit Builder) or pick a build in the Gallery."), 15, SUB); return; }'''),
('''snprintf(lab, sizeof lab, "%s  Lv %d", in.name, in.lvl);''', '''snprintf(lab, sizeof lab, TR("%s  Lv %d"), in.name, in.lvl);'''),
('''"Pick a team or a unit in the Gallery, then press Test Fight there.", 15, SUB) + 8;''', '''TR("Pick a team or a unit in the Gallery, then press Test Fight there."), 15, SUB) + 8;'''),
('''        cmw_named(U->button(left, 0, y, LW, 40, "Open the Gallery", gallery_open_click, NULL), CMW_BUTTON, "fights:l.gallery", gallery_open_click, NULL, "Open the Gallery");''',
 '''        cmw_named(U->button(left, 0, y, LW, 40, TR("Open the Gallery"), gallery_open_click, NULL), CMW_BUTTON, "fights:l.gallery", gallery_open_click, NULL, TR("Open the Gallery"));'''),
('''static const char *mon_name(const char *k) { for (int i = 0; i < P.nmons; i++) if (!strcmp(P.mons[i].k, k)) return P.mons[i].n; return k; }''',
 '''static const char *mon_name(const char *k) { for (int i = 0; i < P.nmons; i++) if (!strcmp(P.mons[i].k, k)) return P.mons[i].tn[0] ? P.mons[i].tn : P.mons[i].n; return k; }'''),
('''        const char *m = engine_state == 3 ? eng_msg : "Getting the wiki's Test Fights files...";''', '''        const char *m = engine_state == 3 ? eng_msg : TR("Getting the wiki's Test Fights files...");'''),
('''    right_sc = cmw_flat(U->panel(body, RX, 16, RW, BODY_H - 110, 0), 0x00000000);   /* holder: one destroy removes it all */''',
 '''    names_refresh();
    right_sc = cmw_flat(U->panel(body, RX, 16, RW, BODY_H - 110, 0), 0x00000000);   /* holder: one destroy removes it all */'''),
('''    y += head(right, 0, y, w, "What do you want");''', '''    y += head(right, 0, y, w, TR("What do you want"));'''),
('''        ml[i] = !strcmp(P.modes[i], "dps") ? "DPS Check" : !strcmp(P.modes[i], "boss") ? "Boss fight" : !strcmp(P.modes[i], "arena") ? "Custom fight" : "Finished base";
    cmw_named(U->value_row(right, 0, y, w, "Test", ml,''', '''        ml[i] = !strcmp(P.modes[i], "dps") ? TR("DPS Check") : !strcmp(P.modes[i], "boss") ? TR("Boss fight") : !strcmp(P.modes[i], "arena") ? TR("Custom fight") : TR("Finished base");
    cmw_named(U->value_row(right, 0, y, w, TR("Test"), ml,'''),
('''snprintf(md, sizeof md, "A practice %s with %s durability that taunts your units. Hit it to see how much damage your squad does.", P.dummy_n, dur); }
    else if (!strcmp(mode_id(), "boss")) snprintf(md, sizeof md, "Your squad standing at a boss door, boss alive and never beaten.");
    else if (!strcmp(mode_id(), "arena")) snprintf(md, sizeof md, "The same boss room with the boss taken out and the monsters you choose put in instead.");
    else snprintf(md, sizeof md, "A fully built base with all research done. Your squad walks in ready to play.");''',
 '''snprintf(md, sizeof md, TR("A practice %s with %s durability that taunts your units. Hit it to see how much damage your squad does."), P.dummy_tn, dur); }
    else if (!strcmp(mode_id(), "boss")) snprintf(md, sizeof md, "%s", TR("Your squad standing at a boss door, boss alive and never beaten."));
    else if (!strcmp(mode_id(), "arena")) snprintf(md, sizeof md, "%s", TR("The same boss room with the boss taken out and the monsters you choose put in instead."));
    else snprintf(md, sizeof md, "%s", TR("A fully built base with all research done. Your squad walks in ready to play."));'''),
('''        y += head(right, 0, y, w, !strcmp(mode_id(), "boss") ? "Which fight" : "Which room");
        const char *bl[8]; for (int i = 0; i < P.nbosses; i++) bl[i] = P.bosses[i].n;
        cmw_named(U->value_row(right, 0, y, w, "Boss", bl,''', '''        y += head(right, 0, y, w, !strcmp(mode_id(), "boss") ? TR("Which fight") : TR("Which room"));
        const char *bl[8]; for (int i = 0; i < P.nbosses; i++) bl[i] = P.bosses[i].tn[0] ? P.bosses[i].tn : P.bosses[i].n;
        cmw_named(U->value_row(right, 0, y, w, TR("Boss"), bl,'''),
('''            char lab[12][24]; const char *ll[12];
            for (int i = 0; i < P.nmsat; i++) { snprintf(lab[i], sizeof lab[i], P.msat_keys[i] ? "Day %d" : "Off", P.msat_keys[i]); ll[i] = lab[i]; }
            cmw_named(U->value_row(right, 0, y, w, "Magic Saturation", ll,''', '''            char lab[12][48]; const char *ll[12];
            for (int i = 0; i < P.nmsat; i++) { snprintf(lab[i], sizeof lab[i], P.msat_keys[i] ? TR("Day %d") : TR("Off"), P.msat_keys[i]); dsi_utf8_fix_end(lab[i]); ll[i] = lab[i]; }
            cmw_named(U->value_row(right, 0, y, w, cm_gw("TEXTKEY_AFFECTER_MagicSaturation_NAME", TR_NOOP("Magic Saturation")), ll,'''),
('''"The save turns Magic Saturation on and writes the day you pick. Only Abyss monsters get it.", 14, SUB) + 6;''', '''TR("The save turns Magic Saturation on and writes the day you pick. Only Abyss monsters get it."), 14, SUB) + 6;'''),
('''"The boss is removed from that room and your monsters take its place.", 14, SUB) + 6;
            cmw_named(U->field(right, 0, y, w - 220, 40, "Search monsters by name...",''', '''TR("The boss is removed from that room and your monsters take its place."), 14, SUB) + 6;
            cmw_named(U->field(right, 0, y, w - 220, 40, TR("Search monsters by name..."),'''),
('''cmw_named(U->value_row(right, w - 212, y - 2, 212, "How many", cnt,''', '''cmw_named(U->value_row(right, w - 212, y - 2, 212, TR("How many"), cnt,'''),
('''                if (!fold_has(P.mons[i].n, st.monq)) continue;''', '''                const char *mn = P.mons[i].tn[0] ? P.mons[i].tn : P.mons[i].n;
                if (!fold_has(mn, st.monq) && !fold_has(P.mons[i].n, st.monq)) continue;   /* the shown name or the English one */'''),
('''                cmw_named(U->button(right, bx, y, w / 2 - 2, 34, P.mons[i].n, mon_add, (void *)(intptr_t)i), CMW_BUTTON, nm, mon_add, (void *)(intptr_t)i, P.mons[i].n);''',
 '''                cmw_named(U->button(right, bx, y, w / 2 - 2, 34, mn, mon_add, (void *)(intptr_t)i), CMW_BUTTON, nm, mon_add, (void *)(intptr_t)i, mn);'''),
('''            if (!shown) y += txt(right, NULL, 12, y, w - 12, "No monster matches.", 14, SUB);''', '''            if (!shown) y += txt(right, NULL, 12, y, w - 12, TR("No monster matches."), 14, SUB);'''),
('''char lab[160], nm[40]; snprintf(lab, sizeof lab, "%s x%d", mon_name(st.mon[i].k), st.mon[i].n);''', '''char lab[160], nm[40]; snprintf(lab, sizeof lab, TR("%s x%d"), mon_name(st.mon[i].k), st.mon[i].n);'''),
('''cmw_named(U->button(right, w - 120, y, 120, 34, "Remove", mon_del, (void *)(intptr_t)i), CMW_BUTTON, nm, mon_del, (void *)(intptr_t)i, "Remove");''',
 '''cmw_named(U->button(right, w - 120, y, 120, 34, TR("Remove"), mon_del, (void *)(intptr_t)i), CMW_BUTTON, nm, mon_del, (void *)(intptr_t)i, TR("Remove"));'''),
('''    y += head(right, 0, y, w, "Difficulty");
    const char *cl[8], *gl[8];
    for (int i = 0; i < P.ncombat; i++) cl[i] = P.label[i];
    for (int i = 0; i < P.nmgmt; i++) { gl[i] = P.mgmt[i]; for (int c = 0; c < P.ncombat; c++) if (!strcmp(P.combat[c], P.mgmt[i])) gl[i] = P.label[c]; }
    cmw_named(U->value_row(right, 0, y, w, "Combat", cl,''', '''    y += head(right, 0, y, w, TR("Difficulty"));
    const char *cl[8], *gl[8];
    for (int i = 0; i < P.ncombat; i++) cl[i] = P.tlabel[i][0] ? P.tlabel[i] : P.label[i];
    for (int i = 0; i < P.nmgmt; i++) { gl[i] = P.mgmt[i]; for (int c = 0; c < P.ncombat; c++) if (!strcmp(P.combat[c], P.mgmt[i])) gl[i] = cl[c]; }
    cmw_named(U->value_row(right, 0, y, w, cm_gw("TEXTKEY_SaveLoad_CombatDifficulty", TR_NOOP("Combat")), cl,'''),
('''DSLObj mg = cmw_named(U->value_row(right, 0, y, w, "Management", gl,''', '''DSLObj mg = cmw_named(U->value_row(right, 0, y, w, cm_gw("TEXTKEY_SaveLoad_ManagementDifficulty", TR_NOOP("Management")), gl,'''),
('''    cmw_named(U->toggle(right, 0, y, w, "Choose management difficulty separately", st.mgmt_own, mgmt_own_change, NULL), CMW_TOGGLE, "fights:r.mgmtown", mgmt_own_change, NULL, "Choose management difficulty separately");''',
 '''    cmw_named(U->toggle(right, 0, y, w, TR("Choose management difficulty separately"), st.mgmt_own, mgmt_own_change, NULL), CMW_TOGGLE, "fights:r.mgmtown", mgmt_own_change, NULL, TR("Choose management difficulty separately"));'''),
('''snprintf(t, sizeof t, "Left off, the economy matches the combat difficulty (%s).", gl[mi]);''', '''snprintf(t, sizeof t, TR("Left off, the economy matches the combat difficulty (%s)."), gl[mi]);'''),
('''        y += head(right, 0, y, w, "Battle kits");
        y += txt(right, NULL, 12, y, w - 12, "Each ticked kit is packed into the personal backpack of every character going in, so the squad walks in carrying it.", 14, SUB) + 4;''',
 '''        y += head(right, 0, y, w, TR("Battle kits"));
        y += txt(right, NULL, 12, y, w - 12, TR("Each ticked kit is packed into the personal backpack of every character going in, so the squad walks in carrying it."), 14, SUB) + 4;'''),
('''snprintf(lab, sizeof lab, "%s  (%d slot(s), %g weight)", P.kits[i].n, P.kits[i].slots, P.kits[i].w);''',
 '''snprintf(lab, sizeof lab, TRN("%s  (%d slot, %g weight)", "%s  (%d slots, %g weight)", P.kits[i].slots), P.kits[i].n, P.kits[i].slots, P.kits[i].w);'''),
('''        if (!any) snprintf(t, sizeof t, "No kit ticked: the squad walks in with only its gear.");
        else snprintf(t, sizeof t, "%d slot(s) and %g weight per character.", sl, (double)((long long)(wt * 100 + 0.5)) / 100);''',
 '''        if (!any) snprintf(t, sizeof t, "%s", TR("No kit ticked: the squad walks in with only its gear."));
        else snprintf(t, sizeof t, TRN("%d slot and %g weight per character.", "%d slots and %g weight per character.", sl), sl, (double)((long long)(wt * 100 + 0.5)) / 100);'''),
('''            char lab[200]; snprintf(lab, sizeof lab, "Grant %s on custom saves", P.insc_n);''', '''            char lab[200]; snprintf(lab, sizeof lab, TR("Grant %s on custom saves"), P.insc_tn[0] ? P.insc_tn : P.insc_n);'''),
('''"Added by default so every unit has enough carry weight for the battle kits.", 14, SUB);
            if (st.kit_insc) snprintf(t, sizeof t, "Every character gains %s so the load is carryable.", P.insc_n);
            else snprintf(t, sizeof t, "Inscription off. Units may be overweight if you keep the kits packed.");''',
 '''TR("Added by default so every unit has enough carry weight for the battle kits."), 14, SUB);
            if (st.kit_insc) snprintf(t, sizeof t, TR("Every character gains %s so the load is carryable."), P.insc_tn[0] ? P.insc_tn : P.insc_n);
            else snprintf(t, sizeof t, "%s", TR("Inscription off. Units may be overweight if you keep the kits packed."));'''),
('''    snprintf(sq, sizeof sq, "Squad: %d / %d", n, P.max_squad ? P.max_squad : 4);''', '''    snprintf(sq, sizeof sq, TR("Squad: %d / %d"), n, P.max_squad ? P.max_squad : 4);'''),
('''    cmw_set_text(w_start, busy ? "Working..." : "Start");''', '''    cmw_set_text(w_start, busy ? TR("Working...") : TR("Start"));'''),
('''    body = cmw_open("fights", "Test Fights", WIN_W, WIN_H, closed, NULL);''', '''    body = cmw_open("fights", TR("Test Fights"), WIN_W, WIN_H, closed, NULL);'''),
('''w_start = cmw_named(U->button(body, WIN_W - 24 - 220, BODY_H - 72, 220, 52, "Start", start_click, NULL), CMW_BUTTON, "fights:start", start_click, NULL, "Start");
    if (engine_state == 3) cmw_named(U->button(body, WIN_W - 24 - 220 - 180, BODY_H - 66, 170, 40, "Try again", retry_click, NULL), CMW_BUTTON, "fights:retry", retry_click, NULL, "Try again");''',
 '''w_start = cmw_named(U->button(body, WIN_W - 24 - 220, BODY_H - 72, 220, 52, TR("Start"), start_click, NULL), CMW_BUTTON, "fights:start", start_click, NULL, TR("Start"));
    if (engine_state == 3) cmw_named(U->button(body, WIN_W - 24 - 220 - 180, BODY_H - 66, 170, 40, TR("Try again"), retry_click, NULL), CMW_BUTTON, "fights:retry", retry_click, NULL, TR("Try again"));'''),
('''void tf_open(void) { build_window(); }''', '''void tf_open(void) { build_window(); }
static void tf_relang(void) { if (cmw_is_open("fights")) build_window(); }   /* the language changed: drawn again */'''),
('''    cm_home_add("test-fights", "Test Fights", NULL, 40, tf_open);''', '''    cm_home_add("test-fights", TR_NOOP("Test Fights"), NULL, 40, tf_open);
    cm_i18n_on_change(tf_relang);'''),
]

EDITS["fights/cm_win.c"] = [
('''#include "cm_home.h"
#include <stdarg.h>''', '''#include "cm_home.h"
#include "i18n/ds_i18n.h"
#include <stdarg.h>'''),
('''    U->tooltip(x, NULL, "Close (Esc)");''', '''    U->tooltip(x, NULL, TR("Close (Esc)"));'''),
]

EDITS["fights/sg_cache.c"] = [
('''        snprintf(err, errn, "%s", r.status == 200 ? "a file from the wiki arrived incomplete" : "the wiki is missing a file for this fight");''',
 '''        snprintf(err, errn, "%s", r.status == 200 ? TR("a file from the wiki arrived incomplete") : TR("the wiki is missing a file for this fight"));'''),
]
EDITS["fights/cm_http.c"] = [
('''        else snprintf(err, errn, "%s", strerr ? strerr(e) : "connection error");''', '''        else snprintf(err, errn, "%s", strerr ? strerr(e) : TR("connection error"));'''),
]
EDITS["units/wk_link.c"] = [
('''        snprintf(err, errn, "This short link needs the internet (%s). Or open it on the wiki and copy the long link.", e[0] ? e : "no answer");''',
 '''        snprintf(err, errn, TR("This short link needs the internet (%s). Or open it on the wiki and copy the long link."), e[0] ? e : TR("no answer"));'''),
]

EDITS["gallery/gal_ui.c"] = [
('''#include "../fe/fe_json.h"
#include <stdint.h>''', '''#include "../fe/fe_json.h"
#include "cm_i18n.h"
#include <stdint.h>'''),
('''            if (!nitems) { snprintf(status, sizeof status, "No public builds match these filters yet."); status_warn = 0; }
        } else { snprintf(status, sizeof status, "Could not load the gallery. Try again later."); status_warn = 1; cmw_log("gal: %s", b->err); }''',
 '''            if (!nitems) { snprintf(status, sizeof status, "%s", TR("No public builds match these filters yet.")); status_warn = 0; }
        } else { snprintf(status, sizeof status, "%s", TR("Could not load the gallery. Try again later.")); status_warn = 1; cmw_log("gal: %s", b->err); }'''),
('''    loading = 1; snprintf(status, sizeof status, "Loading builds..."); status_warn = 0;''', '''    loading = 1; snprintf(status, sizeof status, "%s", TR("Loading builds...")); status_warn = 0;'''),
('''{ if (!b->err[0]) snprintf(b->err, sizeof b->err, "Could not load this team. Try again later."); return; }''', '''{ if (!b->err[0]) snprintf(b->err, sizeof b->err, "%s", TR("Could not load this team. Try again later.")); return; }'''),
('''            snprintf(nb[nn], 96, "%s  Lv %d", m, b->it.units[i].lvl); names[nn] = nb[nn]; nn++;''', '''            snprintf(nb[nn], 96, TR("%s  Lv %d"), m, b->it.units[i].lvl); dsi_utf8_fix_end(nb[nn]); names[nn] = nb[nn]; nn++;'''),
('''        if (!r) { snprintf(status, sizeof status, "Saved to My Units: %s", title); status_warn = 0; }
        else { snprintf(status, sizeof status, "%s", e[0] ? e : "My Units could not be written."); status_warn = 1; }''',
 '''        if (!r) { snprintf(status, sizeof status, TR("Saved to My Units: %s"), title); status_warn = 0; }
        else { snprintf(status, sizeof status, "%s", e[0] ? e : TR("My Units could not be written.")); status_warn = 1; }'''),
('''    snprintf(status, sizeof status, test ? "Opening the build..." : "Saving..."); status_warn = 0;''', '''    snprintf(status, sizeof status, "%s", test ? TR("Opening the build...") : TR("Saving...")); status_warn = 0;'''),
('''    k += (size_t)snprintf(body_t + k, sizeof body_t - k, "by %s  -  Opens %d  -  Copies %d  -  Likes %d", au, it->opens, it->copies, it->likes);''',
 '''    k += (size_t)snprintf(body_t + k, sizeof body_t - k, TR("by %s  -  Opens %d  -  Copies %d  -  Likes %d"), au, it->opens, it->copies, it->likes);
    if (k >= sizeof body_t) k = sizeof body_t - 1;'''),
('''        k += (size_t)snprintf(body_t + k, sizeof body_t - k, "\\n%s  Lv %d%s%s", m, g->lvl, tr[0] ? "  -  " : "", tr);''',
 '''        char ml[240]; snprintf(ml, sizeof ml, TR("%s  Lv %d"), m, g->lvl);
        k += (size_t)snprintf(body_t + k, sizeof body_t - k, "\\n%s%s%s", ml, tr[0] ? "  -  " : "", tr);
        if (k >= sizeof body_t) k = sizeof body_t - 1;'''),
('''    U->text(p, x + 14, y + 36, CARD_W - 28, CARD_H - 36 - 50, body_t, 14, DSL_TXT_LEFT, MAIN);''', '''    dsi_utf8_fix_end(body_t);
    U->text(p, x + 14, y + 36, CARD_W - 28, CARD_H - 36 - 50, body_t, 14, DSL_TXT_LEFT, MAIN);'''),
('''"Save to My Units", save_click, (void *)(intptr_t)i), CMW_BUTTON, nm, save_click, (void *)(intptr_t)i, "Save to My Units");''',
 '''TR("Save to My Units"), save_click, (void *)(intptr_t)i), CMW_BUTTON, nm, save_click, (void *)(intptr_t)i, TR("Save to My Units"));'''),
('''"Test Fight", test_click, (void *)(intptr_t)i), CMW_BUTTON, nm, test_click, (void *)(intptr_t)i, "Test Fight");''',
 '''TR("Test Fight"), test_click, (void *)(intptr_t)i), CMW_BUTTON, nm, test_click, (void *)(intptr_t)i, TR("Test Fight"));'''),
('''    static const char *kinds[2] = { "Teams", "Units" }, *sorts[2] = { "Most popular", "Newest" };''', '''    const char *kinds[2] = { TR("Teams"), TR("Units") }, *sorts[2] = { TR("Most popular"), TR("Newest") };'''),
('''    cmw_named(U->toggle(grid_holder, 660, 13, 356, "Complete only", q.complete, complete_change, NULL), CMW_TOGGLE, "gallery:f.complete", complete_change, NULL, "Complete only");''',
 '''    cmw_named(U->toggle(grid_holder, 660, 13, 356, TR("Complete only"), q.complete, complete_change, NULL), CMW_TOGGLE, "gallery:f.complete", complete_change, NULL, TR("Complete only"));'''),
('''        const char *rl[17] = { "Any" }, *tl[41] = { "Any" };''', '''        const char *rl[17] = { TR("Any") }, *tl[41] = { TR("Any") };'''),
('''cmw_named(U->value_row(grid_holder, 24, 64, 488, "Races", rl,''', '''cmw_named(U->value_row(grid_holder, 24, 64, 488, TR("Races"), rl,'''),
('''cmw_named(U->value_row(grid_holder, 528, 64, 488, "Weapon trees", tl,''', '''cmw_named(U->value_row(grid_holder, 528, 64, 488, TR("Weapon trees"), tl,'''),
('''    body = cmw_open("gallery", "Gallery", WIN_W, WIN_H, closed, NULL);''', '''    body = cmw_open("gallery", TR("Gallery"), WIN_W, WIN_H, closed, NULL);'''),
('''w_more = cmw_named(U->button(body, WIN_W - 24 - 220, BODY_H - 58, 220, 46, "Show more", more_click, NULL), CMW_BUTTON, "gallery:more", more_click, NULL, "Show more");''',
 '''w_more = cmw_named(U->button(body, WIN_W - 24 - 220, BODY_H - 58, 220, 46, TR("Show more"), more_click, NULL), CMW_BUTTON, "gallery:more", more_click, NULL, TR("Show more"));'''),
('''static void test_state(char *out, size_t n) {
    size_t k = (size_t)snprintf(out, n, "\\"gal\\":{''', '''static void gal_relang(void) { if (cmw_is_open("gallery")) gal_open(); }   /* the language changed: drawn again */
static void test_state(char *out, size_t n) {
    size_t k = (size_t)snprintf(out, n, "\\"gal\\":{'''),
('''    cm_home_add("gallery", "Gallery", NULL, 30, gal_open);''', '''    cm_home_add("gallery", TR_NOOP("Gallery"), NULL, 30, gal_open);
    cm_i18n_on_change(gal_relang);'''),
]
EDITS["gallery/gallery.c"] = [
('''        if (k + 1 < n) o[k++] = c;
    }
    o[k] = 0;
}''', '''        if (k + 1 < n) o[k++] = c;
    }
    o[k] = 0;
    dsi_utf8_fix_end(o);                        /* raw UTF-8 bytes are copied one by one: the cap may fall inside one */
}'''),
('''        snprintf(err, errn, r->status == 404 ? "this build is no longer in the gallery" : "the Gallery did not answer (error %d)", r->status);''',
 '''        snprintf(err, errn, r->status == 404 ? TR("this build is no longer in the gallery") : TR("the Gallery did not answer (error %d)"), r->status);'''),
]

EDITS["builder/bld_init.c"] = [
('''    if (cm_home_add("unit-builder", "Unit Builder", NULL, 20, open_home))''', '''    if (cm_home_add("unit-builder", TR_NOOP("Unit Builder"), NULL, 20, open_home))'''),
('''#include "mu_ui.h"
''', '''#include "mu_ui.h"
#include "cm_i18n.h"
'''),
('''    cm_test_register("bld", bld_test_find, bld_test_state);''', '''    cm_test_register("bld", bld_test_find, bld_test_state);
    cm_i18n_on_change(bld_relang);'''),
]
EDITS["builder/bld.h"] = [
('''void bld_close_window(void);''', '''void bld_close_window(void);
void bld_relang(void);                   /* the language changed: an open window is built again (cm_i18n_on_change) */'''),
]
EDITS["builder/bld_ui.c"] = [
('''#include "sg_cache.h"
#include <stdio.h>''', '''#include "sg_cache.h"
#include "i18n/ds_i18n.h"
#include <stdio.h>'''),
('''static const char *const SEC_NAME[SEC_N] = { "Race", "Background", "Traits", "Skills", "Gear", "Inscriptions" };''',
 '''static const char *const SEC_NAME[SEC_N] = { TR_NOOP("Race"), TR_NOOP("Background"), TR_NOOP("Traits"), TR_NOOP("Skills"), TR_NOOP("Gear"), TR_NOOP("Inscriptions") };   /* English: also the test names */
static const char *sec_label(int i) { return i == SEC_SKILLS ? cm_gw("TEXTKEY_GAMEGUIDE_SkillsAbility_NAME", SEC_NAME[i]) : TR(SEC_NAME[i]); }'''),
('''        snprintf(b->err, sizeof b->err, "The Unit Builder needs the wiki's builder files once (dungeonsettlers.wiki), and they could not be downloaded: %s", e);''',
 '''        snprintf(b->err, sizeof b->err, TR("The Unit Builder needs the wiki's builder files once (dungeonsettlers.wiki), and they could not be downloaded: %s"), e);'''),
('''    if (!E) { snprintf(b->err, sizeof b->err, "The wiki's Unit Builder could not start. Try again later.");''', '''    if (!E) { snprintf(b->err, sizeof b->err, "%s", TR("The wiki's Unit Builder could not start. Try again later."));'''),
('''        snprintf(b->err, sizeof b->err, "The wiki's Unit Builder files are for game build %s, and this game is build %s. The Unit Builder opens again when both match.", t, b->game_build);''',
 '''        snprintf(b->err, sizeof b->err, TR("The wiki's Unit Builder files are for game build %s, and this game is build %s. The Unit Builder opens again when both match."), t, b->game_build);'''),
('''    if (!b->out) snprintf(b->err, sizeof b->err, "%s", e[0] ? e : "no answer");''', '''    if (!b->out) snprintf(b->err, sizeof b->err, "%s", e[0] ? e : TR("no answer"));'''),
('''    if (!lib) snprintf(m, sizeof m, "Could not read My Units: %s", e);
    else if (!bv) { snprintf(m, sizeof m, "This build could not be read.");''', '''    if (!lib) snprintf(m, sizeof m, TR("Could not read My Units: %s"), e);
    else if (!bv) { snprintf(m, sizeof m, "%s", TR("This build could not be read."));'''),
('''        if (wk_from_json(bv, &wb, e, sizeof e)) snprintf(m, sizeof m, "This unit cannot be saved: %s", e);''', '''        if (wk_from_json(bv, &wb, e, sizeof e)) snprintf(m, sizeof m, TR("This unit cannot be saved: %s"), e);'''),
('''    } else if (mu_add_build(lib, bv, *p ? p : NULL, id, e, sizeof e)) snprintf(m, sizeof m, "This unit cannot be saved: %s", e);''', '''    } else if (mu_add_build(lib, bv, *p ? p : NULL, id, e, sizeof e)) snprintf(m, sizeof m, TR("This unit cannot be saved: %s"), e);'''),
('''    if (!rc && mu_save(lib, e, sizeof e)) { snprintf(m, sizeof m, "Could not save My Units: %s", e); rc = -1; }''', '''    if (!rc && mu_save(lib, e, sizeof e)) { snprintf(m, sizeof m, TR("Could not save My Units: %s"), e); rc = -1; }'''),
('''    W.saving = 1; set_msg("Saving...", MU_C_SUB);''', '''    W.saving = 1; set_msg(TR("Saving..."), MU_C_SUB);'''),
('''        eng = ENG_FAILED; snprintf(eng_msg, sizeof eng_msg, "%s", b->err[0] ? b->err : "The wiki's Unit Builder gave no answer.");''', '''        eng = ENG_FAILED; snprintf(eng_msg, sizeof eng_msg, "%s", b->err[0] ? b->err : TR("The wiki's Unit Builder gave no answer."));'''),
('''            char body[1600]; snprintf(body, sizeof body, "%s%s%s%s", d, tr[0] && d[0] ? "\\n" : "", tr[0] ? "Skill trees: " : "", tr);''',
 '''            char trl[400] = ""; if (tr[0]) snprintf(trl, sizeof trl, TR("Skill trees: %s"), tr);
            char body[1600]; snprintf(body, sizeof body, "%s%s%s", d, trl[0] && d[0] ? "\\n" : "", trl);'''),
('''    y = heading(p, y, "Race");''', '''    y = heading(p, y, TR("Race"));'''),
('''    y = heading(p, y, "Background");
    y = note(p, y, "The background decides the unit's skill trees.", 22);''', '''    y = heading(p, y, TR("Background"));
    y = note(p, y, TR("The background decides the unit's skill trees."), 22);'''),
('''    char t[120]; snprintf(t, sizeof t, "Traits (%d of %d)", on, num(W.st, "indCap"));''', '''    char t[160]; snprintf(t, sizeof t, TR("Traits (%d of %d)"), on, num(W.st, "indCap"));'''),
('''    jv *sp = jget(W.st, "sp"); char t[200];
    snprintf(t, sizeof t, "Main SP left %d of %d    Sub SP left %d of %d", num(sp, "main"), num(sp, "mainMax"), num(sp, "sub"), num(sp, "subMax"));
    y = heading(p, y, "Skills");''', '''    jv *sp = jget(W.st, "sp"); char t[300];
    snprintf(t, sizeof t, TR("Main SP left %d of %d    Sub SP left %d of %d"), num(sp, "main"), num(sp, "mainMax"), num(sp, "sub"), num(sp, "subMax"));
    y = heading(p, y, sec_label(SEC_SKILLS));'''),
('''    if (!trees || !trees->cnt) return note(p, y, "Pick a background first. It decides which skill trees this unit has.", 24);''', '''    if (!trees || !trees->cnt) return note(p, y, TR("Pick a background first. It decides which skill trees this unit has."), 24);'''),
('''        snprintf(head, sizeof head, "%s  -  %s%s", n, truthy(tr, "w") ? "weapon tree, Main SP" : "auxiliary tree, Sub SP", truthy(tr, "ins") ? ", from an inscription" : "");''',
 '''        int tw = truthy(tr, "w"), ti_ins = truthy(tr, "ins");
        snprintf(head, sizeof head, tw ? (ti_ins ? TR("%s  -  weapon tree, Main SP, from an inscription") : TR("%s  -  weapon tree, Main SP"))
                                       : (ti_ins ? TR("%s  -  auxiliary tree, Sub SP, from an inscription") : TR("%s  -  auxiliary tree, Sub SP")), n);'''),
('''                char sk[160], sn[160], why[300], d[1200], sub[120], tip[1700];''', '''                char sk[160], sn[160], why[300], d[1200], sub[300], tip[1700];'''),
('''                char lvs[24] = ""; if (lv > 1) snprintf(lvs, sizeof lvs, "  \\xC2\\xB7  Lv %d", lv);
                snprintf(sub, sizeof sub, "%s%d %s SP%s%s", on ? "learned  \\xC2\\xB7  " : "", num(nd, "cost"), truthy(tr, "w") ? "main" : "sub", lvs, truthy(nd, "p") ? "  \\xC2\\xB7  passive" : "");''',
 '''                char lvs[64] = "", cost[80];
                if (lv > 1) { snprintf(lvs, sizeof lvs, "  \\xC2\\xB7  "); snprintf(lvs + 6, sizeof lvs - 6, TR("Lv %d"), lv); }
                snprintf(cost, sizeof cost, truthy(tr, "w") ? TR("%d main SP") : TR("%d sub SP"), num(nd, "cost"));
                snprintf(sub, sizeof sub, "%s%s%s%s%s%s", on ? TR("learned") : "", on ? "  \\xC2\\xB7  " : "", cost, lvs,
                         truthy(nd, "p") ? "  \\xC2\\xB7  " : "", truthy(nd, "p") ? TR("passive") : "");
                dsi_utf8_fix_end(sub);'''),
('''                    snprintf(tip, sizeof tip, "%s%s%s%s", why[0] ? "Locked: " : "", why, why[0] && d[0] ? "\\n" : "", d);''',
 '''                    char lk[400] = ""; if (why[0]) snprintf(lk, sizeof lk, TR("Locked: %s"), why);
                    snprintf(tip, sizeof tip, "%s%s%s", lk, lk[0] && d[0] ? "\\n" : "", d);'''),
('''    y = heading(p, y, "Gear");''', '''    y = heading(p, y, TR("Gear"));'''),
('''        DSLObj b = icon_button(p, 120, y, W.mw - 150, 48, k[0] ? CM_ICON_ITEM : 0, k, k[0] ? n : "(empty)",
                               W.open_slot == (int)s ? "choose below" : "click to change", k[0] ? ST_ON : ST_OFF, on_slot, UD(s));''',
 '''        DSLObj b = icon_button(p, 120, y, W.mw - 150, 48, k[0] ? CM_ICON_ITEM : 0, k, k[0] ? n : TR("(empty)"),
                               W.open_slot == (int)s ? TR("choose below") : TR("click to change"), k[0] ? ST_ON : ST_OFF, on_slot, UD(s));'''),
('''            vals[nv++] = "(none)";''', '''            vals[nv++] = TR("(none)");'''),
('''                char lab[40]; snprintf(lab, sizeof lab, "Roll %zu", r + 1);''', '''                char lab[80]; snprintf(lab, sizeof lab, TR("Roll %zu"), r + 1); dsi_utf8_fix_end(lab);'''),
('''            DSLObj be = icon_button(p, 4, y, cw, 44, 0, "", "(empty)", NULL, ST_OFF, on_item, UD(((int)s << 12) | 0));''', '''            DSLObj be = icon_button(p, 4, y, cw, 44, 0, "", TR("(empty)"), NULL, ST_OFF, on_item, UD(((int)s << 12) | 0));'''),
('''                jv *it = items->v[i]; char ik[160], in[160], sub[120];''', '''                jv *it = items->v[i]; char ik[160], in[160], sub[300];'''),
('''                snprintf(sub, sizeof sub, "%s%s%s%s%s", sb, rr[0] ? "  \\xC2\\xB7  " : "", rr, truthy(it, "th") ? "  \\xC2\\xB7  two-handed" : "", truthy(it, "ob") ? "" : "  \\xC2\\xB7  not obtainable");''',
 '''                snprintf(sub, sizeof sub, "%s%s%s%s%s%s%s", sb, rr[0] ? "  \\xC2\\xB7  " : "", rr, truthy(it, "th") ? "  \\xC2\\xB7  " : "", truthy(it, "th") ? TR("two-handed") : "",
                         truthy(it, "ob") ? "" : "  \\xC2\\xB7  ", truthy(it, "ob") ? "" : TR("not obtainable"));
                dsi_utf8_fix_end(sub);'''),
('''    jv *in = jget(W.st, "insc"); char t[400];
    int slots = num(in, "slots"), trees = num(in, "trees"), used = num(in, "used"), freen = num(in, "free");
    y = heading(p, y, "Inscriptions");
    snprintf(t, sizeof t, "One inscription per level: %d by level %d. %d spent on extra skill trees, %d on inscriptions, so %d free.",''',
 '''    jv *in = jget(W.st, "insc"); char t[600];
    int slots = num(in, "slots"), trees = num(in, "trees"), used = num(in, "used"), freen = num(in, "free");
    y = heading(p, y, TR("Inscriptions"));
    snprintf(t, sizeof t, TR("One inscription per level: %d by level %d. %d spent on extra skill trees, %d on inscriptions, so %d free."),'''),
('''    y = heading(p, y, "Extra skill trees");''', '''    y = heading(p, y, TR("Extra skill trees"));'''),
('''    y = heading(p, y, "Inscriptions");
    Grid gi''', '''    y = heading(p, y, TR("Inscriptions"));
    Grid gi'''),
('''mu_text(W.sheet, 14, y, sw - 120, 26, "Main SP left", 15,''', '''mu_text(W.sheet, 14, y, sw - 120, 26, TR("Main SP left"), 15,'''),
('''mu_text(W.sheet, 14, y, sw - 120, 26, "Sub SP left", 15,''', '''mu_text(W.sheet, 14, y, sw - 120, 26, TR("Sub SP left"), 15,'''),
('''mu_text(W.sheet, 14, y, sw - 120, 26, "Inscriptions free", 15,''', '''mu_text(W.sheet, 14, y, sw - 120, 26, TR("Inscriptions free"), 15,'''),
('''    else if (eng != ENG_READY || !W.st) y = note(p, y, "Loading the wiki's Unit Builder...", 30);''', '''    else if (eng != ENG_READY || !W.st) y = note(p, y, TR("Loading the wiki's Unit Builder..."), 30);'''),
('''        char t[80]; snprintf(t, sizeof t, "%s%s%s", i == W.section ? "<color=#D9BE8F>" : "", SEC_NAME[i], i == W.section ? "</color>" : "");''',
 '''        char t[300]; snprintf(t, sizeof t, "%s%s%s", i == W.section ? "<color=#D9BE8F>" : "", sec_label(i), i == W.section ? "</color>" : "");'''),
('''            W.v_lvl = U->value_row(W.win, 360, 56, 240, "Level", vals, n, lv - 1, on_lvl, NULL);''', '''            W.v_lvl = U->value_row(W.win, 360, 56, 240, TR("Level"), vals, n, lv - 1, on_lvl, NULL);'''),
('''    W.win = mu_window(W.unit_id[0] ? "Unit Builder - Edit" : "Unit Builder", 1240, 780, &w, &h);''', '''    W.win = mu_window(W.unit_id[0] ? TR("Unit Builder - Edit") : TR("Unit Builder"), 1240, 780, &w, &h);'''),
('''    mu_text(W.win, 20, y, 64, 40, "Name", 15, DSL_TXT_LEFT, MU_C_SUB);
    W.f_name = U->field(W.win, 84, y, 260, 40, "Unit name", W.lib_name, on_name, NULL);
    { const char *one[] = { "-" }; W.v_lvl = U->value_row(W.win, 360, y, 240, "Level", one, 1, 0, on_lvl, NULL); }
    W.b_save = mu_button(W.win, w - 280, y, 125, 40, "Save", on_save, NULL);
    W.b_cancel = mu_button(W.win, w - 145, y, 125, 40, "Cancel", on_cancel, NULL);''',
 '''    mu_text(W.win, 20, y, 64, 40, TR("Name"), 15, DSL_TXT_LEFT, MU_C_SUB);
    W.f_name = U->field(W.win, 84, y, 260, 40, TR("Unit name"), W.lib_name, on_name, NULL);
    { const char *one[] = { "-" }; W.v_lvl = U->value_row(W.win, 360, y, 240, TR("Level"), one, 1, 0, on_lvl, NULL); }
    W.b_save = mu_button(W.win, w - 280, y, 125, 40, TR("Save"), on_save, NULL);
    W.b_cancel = mu_button(W.win, w - 145, y, 125, 40, TR("Cancel"), on_cancel, NULL);'''),
('''    U->tooltip(W.b_save, "Save", "Saves this unit in My Units. Use it in Bring Units (Foreign Expedition, PvP) and Test Fights.");''',
 '''    U->tooltip(W.b_save, TR("Save"), TR("Saves this unit in My Units. Use it in Bring Units (Foreign Expedition, PvP) and Test Fights."));'''),
('''        W.nav[i] = mu_button(W.win, 20, ny + i * 48, 180, 42, SEC_NAME[i], on_nav, UD(i));''', '''        W.nav[i] = mu_button(W.win, 20, ny + i * 48, 180, 42, sec_label(i), on_nav, UD(i));'''),
('''static void win_poll(void) {             /* the kit's screen layer or the UI scale changed: build the window again */
    if (!W.open || !W.win) return;
    float lw = 0, lh = 0;
    if (!cm_rect_size(cm_parent(W.win), &lw, &lh) && lw >= 100 && lh >= 100
        && (fabsf(lw - W.lw) > 1 || fabsf(lh - W.lh) > 1 || fabsf(mu_ui_scale() - W.scale) > 0.001f)) {''',
 '''static int relang;                       /* the language changed: build the window again (bld_relang) */
void bld_relang(void) { relang = 1; }
static void win_poll(void) {             /* the kit's screen layer or the UI scale changed: build the window again */
    if (!W.open || !W.win) { relang = 0; return; }
    float lw = 0, lh = 0;
    if (relang || (!cm_rect_size(cm_parent(W.win), &lw, &lh) && lw >= 100 && lh >= 100
        && (fabsf(lw - W.lw) > 1 || fabsf(lh - W.lh) > 1 || fabsf(mu_ui_scale() - W.scale) > 0.001f))) {
        relang = 0;'''),
('''        } else set_msg(lib ? "That unit is not in My Units any more: a new unit starts here." : e, MU_C_BAD);''', '''        } else set_msg(lib ? TR("That unit is not in My Units any more: a new unit starts here.") : e, MU_C_BAD);'''),
]

# ---- rules: a string literal in these calls is wrapped in TR(...) (error texts written into an err buffer)
LIT = r'"(?:[^"\\\n]|\\.)*"'
RULES = [
    # (files, regex with groups (prefix, literal), the literal is a printf format)
    (["units/mu_save.c", "units/myunits.c", "units/pvp_build.c", "units/wk_link.c", "units/wk_http.c",
      "fights/cm_http.c", "fights/sg_cache.c", "fights/tf_worlds.c", "fights/tf_ui.c", "gallery/gallery.c"],
     r'(snprintf\(\s*(?:err|e|b->err)\s*,\s*(?:errn|sizeof e|sizeof b->err)\s*,\s*)(' + LIT + r')', True),
    (["units/wk_link.c"], r'(set_err\(code, WKL_[A-Z_]+, err, errn, )(' + LIT + r')', False),   # a message, not a format
]
NEVER = {'"%s"', '"%s: %s"', '"%s (%s)"', '"internal: unit JSON: %s"', '"internal: closure JSON: %s"', '"bad template name %.80s"'}


def apply_rules(root, report):
    import re
    for files, rx, fmt in RULES:
        for f in files:
            p = os.path.join(root, f)
            s = open(p).read()
            def sub(m):
                lit = m.group(2)
                if lit in NEVER or not re.search(r'[A-Za-z]{3}', lit):
                    return m.group(0)
                if fmt and not re.search(r'%[^%]', lit.replace('%%', '')):    # no arguments: never a non-literal format alone
                    return m.group(1) + '"%s", TR(' + lit + ')'
                return m.group(1) + 'TR(' + lit + ')'
            n = re.sub(rx, sub, s)
            if n != s:
                if 'i18n/ds_i18n.h' not in n:
                    n = n.replace('#include <stdio.h>', '#include "i18n/ds_i18n.h"\n#include <stdio.h>', 1)
                open(p, 'w').write(n)
                report.append(f"{f}: rule applied")


def main():
    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    check = "--check" in sys.argv
    report, bad = [], 0
    for f, edits in EDITS.items():
        p = os.path.join(root, f)
        s = open(p).read()
        done = applied = 0
        for old, new in edits:
            if old == new:
                continue
            if new in s:
                done += 1
            elif s.count(old) == 1:
                if not check:
                    s = s.replace(old, new)
                applied += 1
            else:
                bad += 1
                report.append(f"{f}: NOT FOUND: {old[:100]!r}")
        if not check:
            open(p, "w").write(s)
        report.append(f"{f}: {applied} applied, {done} already done")
    if not check:
        apply_rules(root, report)
    print("\n".join(report))
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
