// mu_ui.c - shared pieces of the My Units and Bring Units windows (mu_ui.h).
#include "mu_ui.h"
#include "cm_il.h"
#include "cm_look.h"
#include "mu_look.h"
#include "i18n/ds_i18n.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static const DSLHost *H; static DSLPlugin *ME; static const DSLUi *U;
const DSLUi *mu_kit(void) { return U; }
const DSLHost *mu_host(void) { return H; }
void mu_log(const char *fmt, ...) {
    if (!H) return;
    char b[1200]; va_list ap; va_start(ap, fmt); vsnprintf(b, sizeof b, fmt, ap); va_end(ap);
    H->log(ME, "%s", b);
}
void mu_ui_init(const DSLHost *host, DSLPlugin *self) { H = host; ME = self; U = cm_look_ui(host, self); cm_il_init(host, self); }

/* ------------------------------------------------------------------ test record */
#define DMAX 2048
static mu_drawn dr[DMAX]; static int ndr; static DSLObj cards[256]; static int ncards, in_card = -1;
/* in_card: the card being drawn (its index), -1 = a window widget (checked for its own box only) */
static void rec(DSLObj o, int is_text) { if (o && ndr < DMAX) { dr[ndr].o = o; dr[ndr].card = in_card; dr[ndr].is_text = is_text; ndr++; } }
int mu_drawn_list(const mu_drawn **out) { *out = dr; return ndr; }
DSLObj mu_drawn_card(int i) { return i >= 0 && i < ncards ? cards[i] : NULL; }
void mu_drawn_reset(void) { ndr = 0; ncards = 0; in_card = -1; }
DSLObj mu_button(DSLObj parent, float x, float y, float w, float h, const char *label, dsl_ui_cb cb, void *ud) {
    if (!U) return NULL;
    DSLObj b = U->button(parent, x, y, w, h, label, cb, ud);
    if (b) { cm_text_ellipsis(b); rec(b, 1); }
    return b;
}

/* ------------------------------------------------------------------ texts, icons, names */
float mu_text_size(float size) {
    const char *c = dsi_code();
    return size < 14 && (!strcmp(c, "ja") || !strcmp(c, "ko") || !strncmp(c, "zh", 2)) ? 14 : size;
}
DSLObj mu_text(DSLObj parent, float x, float y, float w, float h, const char *s, float size, int flags, uint32_t rgba) {
    if (!U) return NULL;
    float fs = mu_text_size(size);
    if (fs != size && !(flags & DSL_TXT_WRAP) && h < fs * 1.6f) { y -= (fs * 1.6f - h) / 2; h = fs * 1.6f; }   /* a line taller than its box is cut */
    size = fs;
    DSLObj t = U->text(parent, x, y, w, h, s ? s : "", size, flags, rgba);
    if (t && !(flags & DSL_TXT_WRAP)) cm_text_ellipsis(t);
    if (t) cm_raycast(t, false);
    rec(t, 1);
    return t;
}
void mu_name_of(const char *key, char *out, size_t cap) {
    if (!cap) return; out[0] = 0;
    if (!key || !*key) return;
    char k[160]; snprintf(k, sizeof k, "TEXTKEY_%s_NAME", key);
    if (!cm_gettext(k, out, cap)) return;
    const char *p = key; if (!strncmp(p, "AFFECTER_", 9)) p += 9;
    snprintf(out, cap, "%s", p);
    for (char *c = out; *c; c++) if (*c == '_') *c = ' ';
}
DSLObj mu_icon(DSLObj parent, float x, float y, float s, int kind, const char *key, int dim, const char *tip) {
    if (!U) return NULL;
    Obj sp = cm_icon(kind, key);
    DSLObj p = U->panel(parent, x, y, s, s, sp ? 0xFFFFFFFFu : 0x1E1C21FFu);
    if (!p) return NULL;
    if (sp) cm_set_sprite(p, sp, dim ? 0xFFFFFF59u : 0xFFFFFFFFu);
    cm_raycast(p, false);                                              /* clicks go to what is under the icon */
    /* an item / skill / trait / inscription: the game's own hover card (mu_tip.c); else the kit's name box */
    if (tip && *tip && !(sp && mu_tip_game(p, kind, key))) U->tooltip(p, tip, sp ? " " : key);
    rec(p, 0);
    return p;
}

/* ------------------------------------------------------------------ the window */
/* the game's UI scale (Settings > UI scale, UIScaleManager.Scale: 0.75 .. 1.4); 1 when unknown */
float mu_ui_scale(void) {
    static cm_m inst, get; if (!inst.fn) { inst = cm_il_method("Refactor", "UIScaleManager", "get_Instance", 0, NULL); get = cm_il_method("Refactor", "UIScaleManager", "get_Scale", 0, NULL); }
    Obj m = inst.fn ? CM_CALL(inst, Obj, const void *)(inst.mi) : NULL;
    float s = cm_alive(m) && get.fn ? CM_CALL(get, float, Obj, const void *)(m, get.mi) : 1.f;
    return s >= 0.5f && s <= 2.f ? s : 1.f;
}
/* a plain colour rect (no sprite): the stock "Box" sprite stretched over a large area shows its own darker footer band */
DSLObj mu_plate(DSLObj parent, float x, float y, float w, float h, uint32_t rgba) {
    if (!U) return NULL;
    DSLObj p = U->panel(parent, x, y, w, h, rgba ? rgba : 0x01u);
    if (p) { cm_set_sprite(p, NULL, rgba); cm_raycast(p, false); }
    return p;
}
typedef struct { float x, y, z; } V3s;
DSLObj mu_window(const char *title, float want_w, float want_h, float *w, float *h) {
    if (!U) return NULL;
    DSLObj p = U->panel(NULL, 0, 0, want_w, want_h, 0);
    if (!p) return NULL;
    float lw = 0, lh = 0;
    if (cm_rect_size(cm_parent(p), &lw, &lh) || lw < 100 || lh < 100) { lw = 1920; lh = 1080; }
    /* the window follows the game's UI scale like the game's own windows: drawn at its size x scale, so the room it may
       take (in its own units) is the layer / scale */
    float sc = mu_ui_scale(), aw = lw / sc - 40, ah = lh / sc - 40;
    float ww = want_w > aw ? aw : want_w, hh = want_h > ah ? ah : want_h;
    U->place(p, (lw - ww * sc) / 2, (lh - hh * sc) / 2, ww, hh);
    { static cm_m ls; if (!ls.fn) ls = cm_il_method("UnityEngine", "Transform", "set_localScale", 1, NULL);
      if (ls.fn) CM_CALL(ls, void, Obj, V3s, const void *)(p, (V3s){ sc, sc, 1 }, ls.mi); }
    mu_plate(p, 6, 6, ww - 12, hh - 12, MU_C_BODY);                    /* one body colour inside the frame (the lobby's #222025) */
    U->text(p, 20, 10, ww - 40, 40, title, 26, DSL_TXT_CENTER | DSL_TXT_TITLE, 0);
    if (w) *w = ww;
    if (h) *h = hh;
    return p;
}

/* the card's weapon word (mu_card_tables.h MUC_ITEM_WLABEL, the wiki's English label): the game's own text of that item
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

/* ------------------------------------------------------------------ Compact Plus card */
typedef struct { int kind; const char *key; int dim; } IconRef;
/* one row of icon groups; groups are separated by a gap; the row ends with "+N" when the rest does not fit */
static void icon_row(DSLObj card, float x, float y, float right, float s, const IconRef *ic, const int *group, int n) {
    float step = s + 3, cx = x; int last = -1;
    for (int i = 0; i < n; i++) {
        if (last >= 0 && group[i] != last) cx += 8;
        last = group[i];
        int more = n - i - 1;
        if (cx + s > right - (more ? 30 : 0)) {
            char t[16]; snprintf(t, sizeof t, "+%d", n - i);
            mu_text(card, cx, y, right - cx > 30 ? right - cx : 30, s, t, 13, DSL_TXT_LEFT, MU_C_SUB);
            return;
        }
        char nm[160]; mu_name_of(ic[i].key, nm, sizeof nm);
        mu_icon(card, cx, y, s, ic[i].kind, ic[i].key, ic[i].dim, nm);
        cx += step;
    }
}
/* the width a one-line text takes at size (a measuring text, destroyed again) */
static float text_w(DSLObj parent, const char *s, float size) {
    DSLObj m = U->text(parent, 0, 0, 2000, 30, s, mu_text_size(size), DSL_TXT_LEFT, MU_C_SUB);
    float pw = m ? cm_text_preferred_w(m) : -1;
    if (m) U->destroy(m);
    return pw;
}
/* the unit's teams after its name: all names when they fit; else as many whole names as fit + " +N"; else the first
   name cut with "..." and " +N" at the right end (the count stays readable) */
static char teams_shown[600];
const char *mu_card_teams_shown(void) { return teams_shown; }
static void teams_row(DSLObj card, float x, float y, float room, const char *const *teams, int n) {
    char list[500], t[600], more[24];
    for (int k = n; k >= 1; k--) {
        size_t o = 0; list[0] = 0;
        for (int i = 0; i < k && o + 4 < sizeof list; i++) o += (size_t)snprintf(list + o, sizeof list - o, "%s%s", i ? ", " : "", teams[i]);
        if (k < n) snprintf(list + strlen(list), sizeof list - strlen(list), " +%d", n - k);
        snprintf(t, sizeof t, TRN("Team: %s", "Teams: %s", n), list); dsi_utf8_fix_end(t);
        float pw = text_w(card, t, 13);
        if (pw >= 0 && pw <= room) { mu_text(card, x, y, room, 26, t, 13, DSL_TXT_LEFT, MU_C_SUB); snprintf(teams_shown, sizeof teams_shown, "%s", t); return; }
    }
    /* not even one whole name: the first one cut, the count apart on the right */
    float mw = 0; more[0] = 0;
    if (n > 1) { snprintf(more, sizeof more, "+%d", n - 1); mw = text_w(card, more, 13) + 6; if (mw < 0 || mw > room / 2) mw = room / 2; }
    snprintf(t, sizeof t, TRN("Team: %s", "Teams: %s", n), teams[0]); dsi_utf8_fix_end(t);
    mu_text(card, x, y, room - mw, 26, t, 13, DSL_TXT_LEFT, MU_C_SUB);
    if (more[0]) mu_text(card, x + room - mw + 6, y, mw - 6, 26, more, 13, DSL_TXT_LEFT, MU_C_SUB);
    snprintf(teams_shown, sizeof teams_shown, "%s%s%s", t, more[0] ? " " : "", more);
}
/* the note on the right of the race line: the first form that fits the room, right-aligned before the character sprite;
   the hover card has the full line */
static char note_shown[400];
const char *mu_card_note_shown(void) { return note_shown; }
static void note_row(DSLObj card, float x, float y, float room, const char *const *forms, int n, const char *tip) {
    int k = n - 1;
    for (int i = 0; i < n; i++) { float pw = text_w(card, forms[i], 13); if (pw >= 0 && pw <= room) { k = i; break; } }
    DSLObj h = mu_plate(card, x, y, room, 22, 0);                     /* a holder that takes the hover (the text lets it through) */
    if (h && tip && *tip) { cm_raycast(h, true); U->tooltip(h, TR("Snapshot"), tip); }
    mu_text(h ? h : card, h ? 0 : x, h ? 0 : y, room, 22, forms[k], 13, DSL_TXT_RIGHT, MU_C_SUB);
    snprintf(note_shown, sizeof note_shown, "%s", forms[k]);
}
DSLObj mu_card_draw(DSLObj parent, float x, float y, float w, const mu_card *c, const char *name, int level,
                    const char *portrait_key, const char *const *bar, int nbar, const char *const *teams, int nteams,
                    const char *const *note, int nnote, const char *note_tip) {
    teams_shown[0] = 0; note_shown[0] = 0;
    if (!U || !c) return NULL;
    DSLObj card = U->panel(parent, x, y, w, MU_CARD_H, 0);            /* the game's own panel (a dark tint drew it black) */
    if (!card) return NULL;
    cm_raycast(card, false);
    if (ncards < 256) { cards[ncards] = card; in_card = ncards++; }
    /* avatar + level */
    const char *face = portrait_key && *portrait_key ? portrait_key : c->avatar;
    char rn[96]; mu_name_of(c->race, rn, sizeof rn);
    if (face && cm_icon(CM_ICON_PORTRAIT, face)) mu_icon(card, 8, 8, 72, CM_ICON_PORTRAIT, face, 0, NULL);
    else mu_icon(card, 8, 8, 72, CM_ICON_AFFECTER, c->race, 0, rn);
    char lv[64]; snprintf(lv, sizeof lv, TR("Lv %d"), level > 0 ? level : c->lvl);
    mu_text(card, 4, 82, 80, 24, lv, 14, DSL_TXT_CENTER, MU_C_GOLD);
    /* the in-game character (race, face, worn gear), drawn by the game (mu_look.c), top right */
    float lookw = 0;
    { mu_look lk; memset(&lk, 0, sizeof lk);
      snprintf(lk.race, sizeof lk.race, "%s", c->race ? c->race : ""); snprintf(lk.portrait, sizeof lk.portrait, "%s", face ? face : "");
      for (int i = 0; i < c->neq && lk.ngear < MU_LOOK_GEAR; i++) snprintf(lk.gear[lk.ngear++], sizeof lk.gear[0], "%s", c->eq[i].item ? c->eq[i].item : "");
      if (mu_look_sprite(card, w - 92, 8, 84, 84, &lk)) lookw = 92; }
    /* name, race / background / weapon */
    float x0 = 90, tw = w - x0 - 10 - lookw;
    char nm[128];
    if (name && *name) snprintf(nm, sizeof nm, "%s", name);
    else snprintf(nm, sizeof nm, "%s", c->name[0] ? c->name : TR("Unit"));
    DSLObj nt = mu_text(card, x0, 5, tw, 28, nm, 17, DSL_TXT_LEFT, MU_C_MAIN);   /* clear of the frame's top edge (it pressed on it) */
    if (teams && nteams > 0 && nt) {                                   /* the teams after the name: the name keeps up to 55 % */
        float pw = cm_text_preferred_w(nt), nw = tw * 0.55f;
        if (pw >= 0 && pw + 2 < nw) nw = pw + 2;
        U->place(nt, x0, 5, nw, 28);
        teams_row(card, x0 + nw + 14, 7, tw - nw - 14, teams, nteams);
    }
    char line[300], bg[96] = ""; if (c->bg) mu_name_of(c->bg, bg, sizeof bg);
    snprintf(line, sizeof line, "%s%s%s%s%s", rn, bg[0] ? "  \xC2\xB7  " : "", bg, c->weapon ? "  \xC2\xB7  " : "", c->weapon ? weapon_word(c->weapon) : "");
    dsi_utf8_fix_end(line);
    /* one size on every card: a line too long for 13 drops its weapon word (the weapon is the first worn-item icon below)
       instead of the kit shrinking each card to its own size and cutting the weapon with "..." (RU) */
    float rw = note && nnote > 0 ? tw * 0.6f : tw;                     /* with a note: the race line keeps up to 60 % */
    if (c->weapon) {
        DSLObj m = U->text(card, x0, 31, tw, 22, line, mu_text_size(13), DSL_TXT_LEFT, MU_C_SUB); float pw = m ? cm_text_preferred_w(m) : -1;
        if (m) U->destroy(m);
        if (pw > rw) { snprintf(line, sizeof line, "%s%s%s", rn, bg[0] ? "  \xC2\xB7  " : "", bg); dsi_utf8_fix_end(line); }
    }
    DSLObj rt = mu_text(card, x0, 31, rw, 22, line, 13, DSL_TXT_LEFT, MU_C_SUB);
    if (note && nnote > 0) {
        float pw = rt ? cm_text_preferred_w(rt) : -1;
        if (pw >= 0 && pw + 2 < rw) { rw = pw + 2; U->place(rt, x0, 31, rw, 22); }
        note_row(card, x0 + rw + 16, 31, tw - rw - 16, note, nnote, note_tip);
    }
    /* row A: worn items | skill bar */
    IconRef ic[MUC_MAX * 3]; int gr[MUC_MAX * 3], n = 0;
    for (int i = 0; i < c->neq && n < MUC_MAX; i++) { ic[n] = (IconRef){ CM_ICON_ITEM, c->eq[i].item, 0 }; gr[n++] = 0; }
    int nb = bar ? nbar : c->nbar;
    for (int i = 0; i < nb && n < MUC_MAX * 2; i++) { const char *k = bar ? bar[i] : c->bar[i]; if (k && *k) { ic[n] = (IconRef){ CM_ICON_SKILL, k, 0 }; gr[n++] = 1; } }
    icon_row(card, x0, 54, w - 8 - lookw, 24, ic, gr, n);
    /* row B: branches ACTIVE | PASSIVE (dim = nothing learned in it) | banked milestones */
    n = 0;
    for (int i = 0; i < c->nactive; i++) { ic[n] = (IconRef){ CM_ICON_TREE, c->active[i].tree, !c->active[i].invested }; gr[n++] = 0; }
    for (int i = 0; i < c->npassive; i++) { ic[n] = (IconRef){ CM_ICON_TREE, c->passive[i].tree, !c->passive[i].invested }; gr[n++] = 1; }
    for (int i = 0; i < c->nstacks; i++) { ic[n] = (IconRef){ CM_ICON_SKILL, c->stacks[i].skill, !c->stacks[i].learned }; gr[n++] = 2; }
    icon_row(card, x0, 79, w - 8 - lookw, 24, ic, gr, n);
    /* row C: passive skills | inscriptions | soul stones | traits acquired in play */
    n = 0;
    for (int i = 0; i < c->npass && n < MUC_MAX; i++) { ic[n] = (IconRef){ CM_ICON_SKILL, c->pass[i].skill, 0 }; gr[n++] = 0; }
    for (int i = 0; i < c->ninsc && n < MUC_MAX * 2; i++) { ic[n] = (IconRef){ CM_ICON_AFFECTER, c->insc[i], 0 }; gr[n++] = 1; }
    for (int i = 0; i < c->nsouls && n < MUC_MAX * 2 + WK_MAX_LIST; i++) { ic[n] = (IconRef){ CM_ICON_AFFECTER, c->souls[i], 0 }; gr[n++] = 2; }
    for (int i = 0; i < c->nacq && n < MUC_MAX * 3; i++) { ic[n] = (IconRef){ CM_ICON_AFFECTER, c->acq[i], 0 }; gr[n++] = 3; }
    icon_row(card, x0, 104, w - 8 - lookw, 20, ic, gr, n);
    in_card = -1;
    return card;
}

/* ------------------------------------------------------------------ frame tick */
#define TKMAX 8
static void (*tks[TKMAX])(void); static int ntk, timer_on;
static void tick_all(void) { for (int i = 0; i < ntk; i++) if (tks[i]) tks[i](); }
int mu_tick_add(void (*fn)(void)) {
    for (int i = 0; i < ntk; i++) if (tks[i] == fn) return 0;
    if (ntk >= TKMAX) return -1;
    tks[ntk++] = fn;
    if (!timer_on) timer_on = mu_os_timer(tick_all, 50) == 0;
    return 0;
}
void mu_tick_remove(void (*fn)(void)) {
    for (int i = 0; i < ntk; i++) if (tks[i] == fn) { tks[i] = tks[--ntk]; return; }
}
