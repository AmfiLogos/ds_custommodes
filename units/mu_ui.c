// mu_ui.c - shared pieces of the My Units and Bring Units windows (mu_ui.h).
#include "mu_ui.h"
#include "cm_il.h"
#include "cm_look.h"
#include "mu_look.h"
#include "i18n/ds_i18n.h"
#include <stdarg.h>
#include <stddef.h>
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
typedef struct { float x, y; } V2s;
static void set_scale(DSLObj p, float sc) {
    static cm_m ls; if (!ls.fn) ls = cm_il_method("UnityEngine", "Transform", "set_localScale", 1, NULL);
    if (ls.fn && p) CM_CALL(ls, void, Obj, V3s, const void *)(p, (V3s){ sc, sc, 1 }, ls.mi);
}
/* anchors, pivot, size and position of a RectTransform (a = anchorMin, b = anchorMax) */
static void rt_set(DSLObj rt, V2s a, V2s b, V2s pivot, V2s size, V2s pos) {
    static cm_m amin, amax, piv, sz, ap;
    if (!amin.fn) { amin = cm_il_method("UnityEngine", "RectTransform", "set_anchorMin", 1, NULL); amax = cm_il_method("UnityEngine", "RectTransform", "set_anchorMax", 1, NULL);
                    piv = cm_il_method("UnityEngine", "RectTransform", "set_pivot", 1, NULL); sz = cm_il_method("UnityEngine", "RectTransform", "set_sizeDelta", 1, NULL);
                    ap = cm_il_method("UnityEngine", "RectTransform", "set_anchoredPosition", 1, NULL); }
    if (!rt || !amin.fn || !amax.fn || !piv.fn || !sz.fn || !ap.fn) return;
    CM_CALL(amin, void, Obj, V2s, const void *)(rt, a, amin.mi); CM_CALL(amax, void, Obj, V2s, const void *)(rt, b, amax.mi);
    CM_CALL(piv, void, Obj, V2s, const void *)(rt, pivot, piv.mi); CM_CALL(sz, void, Obj, V2s, const void *)(rt, size, sz.mi);
    CM_CALL(ap, void, Obj, V2s, const void *)(rt, pos, ap.mi);
}
DSLObj mu_backdrop(uint32_t rgba) {
    if (!U) return NULL;
    DSLObj b = U->panel(NULL, 0, 0, 100, 100, 0);
    if (!b) return NULL;
    cm_set_sprite(b, NULL, rgba);
    cm_raycast(b, true);                                               /* takes the clicks of what is under it */
    rt_set(b, (V2s){ 0, 0 }, (V2s){ 1, 1 }, (V2s){ 0.5f, 0.5f }, (V2s){ 0, 0 }, (V2s){ 0, 0 });   /* the whole layer, also after a resize */
    return b;
}
void mu_centre_scaled(DSLObj w, float bw, float bh) {
    rt_set(w, (V2s){ 0.5f, 0.5f }, (V2s){ 0.5f, 0.5f }, (V2s){ 0.5f, 0.5f }, (V2s){ bw, bh }, (V2s){ 0, 0 });
    set_scale(w, mu_ui_scale());                                       /* KIT-SCALE: ours; no dsl_scale_follow on this box */
}
/* UX U12: the title's corner buttons (Roadmap, Wishlist, Discord: TitleMenuView._roadmapButton, ...) are drawn on a canvas
   above the kit's screen layer: over our window's corner, not dimmed, and clickable through the backdrop. While one of our
   windows is open they are hidden; when the last one closes, the ones hidden here come back (the game's own state else) */
static int nshell; static uintptr_t h_icon[3];
static void title_icons(int show) {
    static void *k_tmv; static int32_t off[3] = { -2, -2, -2 }; static cm_m act_self;
    static const char *const F[3] = { "_roadmapButton", "_wishlistButton", "_discordButton" };
    if (show) {
        for (int i = 0; i < 3; i++) { Obj g = cm_il_hget(h_icon[i]); if (g && cm_alive(g)) cm_set_active(g, true); cm_il_hset(&h_icon[i], NULL); }
        return;
    }
    if (!k_tmv) { k_tmv = cm_il_class("Refactor.UI", "TitleMenuView"); act_self = cm_il_method("UnityEngine", "GameObject", "get_activeSelf", 0, NULL); }
    Obj v = k_tmv ? cm_find(k_tmv, false) : NULL;
    for (int i = 0; v && i < 3; i++) {
        if (off[i] == -2) off[i] = cm_il_field(k_tmv, F[i]);
        Obj b = off[i] >= 0 ? CM_AT(v, off[i], Obj) : NULL, g = b && cm_alive(b) ? cm_go(b) : NULL;
        if (g && act_self.fn && CM_CALL(act_self, bool, Obj, const void *)(g, act_self.mi)) { cm_set_active(g, false); cm_il_hset(&h_icon[i], g); }
    }
}
/* the red X: the kit's copy of the game's close button (dsl_close_x, kit bdcb603+); its click runs the close that `win`
   registered with esc_push, the same action as Escape (every caller pushes the window in the frame it opens it). NULL (an
   older kit, or the game's button not found): a kit button "X" */
DSLObj mu_close_x(DSLObj parent, float x, float y, float s, DSLObj win, dsl_ui_cb cb, void *ud) {
    DSLObj b = dsl_close_x(U, parent, x, y, s, win);
    return b ? b : U->button(parent, x, y, s, s, "X", cb, ud);
}
/* our own frame on an older kit: its Escape (Back first, then the Close) and its X (the Close) */
static void shell_esc(void *ud) { mu_shell *s = ud; if (!s || !s->win) return; if (s->back_fn && s->back_fn(s->ud)) return; if (s->close) s->close(s->ud); }
static void shell_x(void *ud, DSLObj w) { (void)w; mu_shell *s = ud; if (s && s->win && s->close) s->close(s->ud); }
/* the screen layer's size (a probe panel: the layer before a window is made); 1920 x 1080 when it cannot be read */
static void layer_size(float *lw, float *lh) {
    *lw = 0; *lh = 0;
    DSLObj pr = U ? U->panel(NULL, 0, 0, 10, 10, 0) : NULL;
    if (!pr || cm_rect_size(cm_parent(pr), lw, lh) || *lw < 100 || *lh < 100) { *lw = 1920; *lh = 1080; }
    if (pr) U->destroy(pr);
}
float mu_shell_room_h(void) { float lw, lh; layer_size(&lw, &lh); return (lh - 40) / mu_ui_scale(); }   /* fit 1's own test */
DSLObj mu_shell_open(mu_shell *s, const char *title, float want_w, float want_h, int fit, float *w, float *h,
                     void (*close)(void *ud), int (*back)(void *ud), void *ud) {
    memset(s, 0, sizeof *s);
    if (!U) return NULL;
    s->close = close; s->back_fn = back; s->ud = ud;
    float lw, lh; layer_size(&lw, &lh);                                 /* the room on the screen layer */
    /* the game's UI scale like the game's own windows: drawn at its size x scale, never cut by the screen layer: fit 0 =
       the room it may take (in its own units) is the layer / scale minus a margin; fit 1 = its size stays, the scale goes
       down until it fits */
    float sc = mu_ui_scale(), ww = want_w, hh = want_h;                  /* KIT-SCALE: ours (DSL_WIN_OWN_SCALE) */
    if (!fit) { float aw = lw / sc - 40, ah = lh / sc - 40; if (ww > aw) ww = aw; if (hh > ah) hh = ah; }
    else { if (sc * ww > lw - 40) sc = (lw - 40) / ww; if (sc * hh > lh - 40) sc = (lh - 40) / hh; }
    if (nshell++ == 0) title_icons(0);
    DSLWinStd o; memset(&o, 0, sizeof o);
    o.size = sizeof o; o.kind = DSL_WIN_MODAL; o.w = ww; o.h = hh; o.title = title; o.close = close; o.back = back; o.ud = ud;
    o.flags = DSL_WIN_OWN_SCALE; o.band_h = MU_SHELL_BAND;
    DSLWinParts parts; memset(&parts, 0, sizeof parts);
    DSLObj p = close ? dsl_window_std(U, &o, &parts) : NULL;
    if (p) {
        s->std = 1; s->esc_ok = 1; s->back = parts.backdrop; s->x = parts.close_x;
        /* the template's backdrop must take every click: kits d529f52 .. a4408f1 left its clicks off (dsl_ui_flat; a click
           beside the window reached the title page, ux_test U1); 6e6071a+ turns them on itself, this is then a no-op */
        if (s->back) cm_raycast(s->back, true);
    }
    else {                                                             /* an older kit: our own frame, the template's layout */
        s->back = mu_backdrop(0x0000009Eu);
        p = U->panel(NULL, 0, 0, ww, hh, 0);
        if (!p) { if (s->back) U->destroy(s->back); s->back = NULL; if (--nshell == 0) title_icons(1); return NULL; }
        rt_set(p, (V2s){ 0.5f, 0.5f }, (V2s){ 0.5f, 0.5f }, (V2s){ 0.5f, 0.5f }, (V2s){ ww, hh }, (V2s){ 0, 0 });
        mu_plate(p, 4, 4, ww - 8, hh - 8, MU_C_BODY);
        mu_plate(p, 4, 4, ww - 8, MU_SHELL_BAND - 4, MU_C_BAND);
        mu_plate(p, 4, MU_SHELL_BAND, ww - 8, 1, MU_C_COPPER);
        DSLObj t = U->text(p, 70, 4, ww - 140, MU_SHELL_BAND - 4, title, 30, DSL_TXT_CENTER | DSL_TXT_TITLE, 0);
        if (t) { cm_text_ellipsis(t); cm_raycast(t, false); }
        s->esc_ok = dsl_esc_push(U, p, shell_esc, s, 0) == 0;
        float xs = 38;
        s->x = mu_close_x(p, ww - 24 - xs, (MU_SHELL_BAND - xs) / 2, xs, p, shell_x, s);
        dsl_rules_own(U, p, DSL_RULE_SCALE);                           /* KIT-SCALE: UI Scale is ours (the UI-RULE guard) */
    }
    if (sc > 0.1f) set_scale(p, sc);
    s->win = p;
    if (w) *w = ww;
    if (h) *h = hh;
    return p;
}
void mu_shell_close(mu_shell *s) {
    if (!U || !s || !s->win) return;
    if (s->std) dsl_window_close(U, s->win);                           /* window, backdrop and its Escape entry */
    else { U->destroy(s->win); if (s->back) U->destroy(s->back); }
    memset(s, 0, sizeof *s);
    if (nshell > 0 && --nshell == 0) title_icons(1);
}
int mu_shell_count(void) { return nshell; }

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
static char note_shown[400]; static DSLObj note_obj;
const char *mu_card_note_shown(void) { return note_shown; }
DSLObj mu_card_note_obj(void) { return note_obj; }
static void note_row(DSLObj card, float x, float y, float room, const char *const *forms, int n, const char *tip) {
    int k = n - 1;
    for (int i = 0; i < n; i++) { float pw = text_w(card, forms[i], 13); if (pw >= 0 && pw <= room) { k = i; break; } }
    DSLObj h = mu_plate(card, x, y, room, 22, 0);                     /* a holder that takes the hover (the text lets it through); a
                                                                         click on it goes up to the card's button (My Units) */
    note_obj = h;
    if (h && tip && *tip) { cm_raycast(h, true); U->tooltip(h, TR("Snapshot"), tip); }
    mu_text(h ? h : card, h ? 0 : x, h ? 0 : y, room, 22, forms[k], 13, DSL_TXT_RIGHT, MU_C_SUB);
    snprintf(note_shown, sizeof note_shown, "%s", forms[k]);
}
float mu_card_h(int flags) { return flags & MU_CARD_COMPACT ? MU_CARD_H_COMPACT : MU_CARD_H; }
DSLObj mu_card_draw(DSLObj parent, float x, float y, float w, const mu_card *c, const char *name, int level,
                    const char *portrait_key, const char *const *bar, int nbar, const char *const *teams, int nteams,
                    const char *const *note, int nnote, const char *note_tip) {
    return mu_card_draw_ex(parent, x, y, w, c, name, level, portrait_key, bar, nbar, teams, nteams, note, nnote, note_tip, 0);
}
DSLObj mu_card_draw_ex(DSLObj parent, float x, float y, float w, const mu_card *c, const char *name, int level,
                       const char *portrait_key, const char *const *bar, int nbar, const char *const *teams, int nteams,
                       const char *const *note, int nnote, const char *note_tip, int flags) {
    teams_shown[0] = 0; note_shown[0] = 0; note_obj = NULL;
    if (!U || !c) return NULL;
    int compact = (flags & MU_CARD_COMPACT) != 0;
    if (compact) { teams = NULL; nteams = 0; note = NULL; nnote = 0; }
    DSLObj card = U->panel(parent, x, y, w, mu_card_h(flags), 0);     /* the game's own panel (a dark tint drew it black) */
    if (!card) return NULL;
    cm_raycast(card, false);
    if (ncards < 256) { cards[ncards] = card; in_card = ncards++; }
    /* avatar + level */
    const char *face = portrait_key && *portrait_key ? portrait_key : c->avatar;
    char rn[96]; mu_name_of(c->race, rn, sizeof rn);
    float ay = compact ? 6 : 8;
    if (face && cm_icon(CM_ICON_PORTRAIT, face)) mu_icon(card, 8, ay, 72, CM_ICON_PORTRAIT, face, 0, NULL);
    else mu_icon(card, 8, ay, 72, CM_ICON_AFFECTER, c->race, 0, rn);
    char lv[64]; snprintf(lv, sizeof lv, TR("Lv %d"), level > 0 ? level : c->lvl);
    mu_text(card, 4, compact ? 78 : 82, 80, compact ? 22 : 24, lv, 14, DSL_TXT_CENTER, MU_C_GOLD);
    /* the in-game character (race, face, worn gear), drawn by the game (mu_look.c), top right; a narrow card (Test Fights'
       squad list, about 300 wide) leaves it out: the name and the icon rows need the room, the portrait shows the face */
    float lookw = 0;
    if (w >= MU_CARD_NARROW) { mu_look lk; memset(&lk, 0, sizeof lk);
      snprintf(lk.race, sizeof lk.race, "%s", c->race ? c->race : ""); snprintf(lk.portrait, sizeof lk.portrait, "%s", face ? face : "");
      for (int i = 0; i < c->neq && lk.ngear < MU_LOOK_GEAR; i++) snprintf(lk.gear[lk.ngear++], sizeof lk.gear[0], "%s", c->eq[i].item ? c->eq[i].item : "");
      if (mu_look_sprite(card, w - 92, 8, 84, 84, &lk)) lookw = 92; }
    /* name, race / background / weapon */
    float x0 = 90, tw = w - x0 - 10 - lookw;
    char nm[128];
    if (name && *name) snprintf(nm, sizeof nm, "%s", name);
    else snprintf(nm, sizeof nm, "%s", c->name[0] ? c->name : TR("Unit"));
    DSLObj nt = mu_text(card, x0, compact ? 4 : 5, tw, compact ? 26 : 28, nm, 17, DSL_TXT_LEFT, MU_C_MAIN);   /* clear of the frame's top edge (it pressed on it) */
    float rx = x0, ry = 31;                                            /* the race line's place: its own row, or after the name */
    if (((teams && nteams > 0) || compact) && nt) {                    /* the teams / race line after the name: the name keeps up to 55 % */
        float pw = cm_text_preferred_w(nt), nw = tw * 0.55f;
        if (pw >= 0 && pw + 2 < nw) nw = pw + 2;
        U->place(nt, x0, compact ? 4 : 5, nw, compact ? 26 : 28);
        if (compact) { rx = x0 + nw + 10; ry = 7; }                    /* on the name's baseline (13 under 17) */
        else teams_row(card, x0 + nw + 14, 7, tw - nw - 14, teams, nteams);
    }
    char line[300], bg[96] = ""; if (c->bg) mu_name_of(c->bg, bg, sizeof bg);
    snprintf(line, sizeof line, "%s%s%s%s%s", rn, bg[0] ? "  \xC2\xB7  " : "", bg, c->weapon ? "  \xC2\xB7  " : "", c->weapon ? weapon_word(c->weapon) : "");
    dsi_utf8_fix_end(line);
    /* one size on every card: a line too long for 13 drops its weapon word (the weapon is the first worn-item icon below)
       instead of the kit shrinking each card to its own size and cutting the weapon with "..." (RU) */
    float rw = note && nnote > 0 ? tw * 0.6f : x0 + tw - rx;           /* with a note: the race line keeps up to 60 % */
    if (c->weapon) {
        DSLObj m = U->text(card, rx, ry, tw, 22, line, mu_text_size(13), DSL_TXT_LEFT, MU_C_SUB); float pw = m ? cm_text_preferred_w(m) : -1;
        if (m) U->destroy(m);
        if (pw > rw) { snprintf(line, sizeof line, "%s%s%s", rn, bg[0] ? "  \xC2\xB7  " : "", bg); dsi_utf8_fix_end(line); }
    }
    DSLObj rt = mu_text(card, rx, ry, rw, 22, line, 13, DSL_TXT_LEFT, MU_C_SUB);
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
    float r0 = compact ? 32 : 54;                                      /* the three icon rows, 25 apart */
    icon_row(card, x0, r0, w - 8 - lookw, 24, ic, gr, n);
    /* row B: branches ACTIVE | PASSIVE (dim = nothing learned in it) | banked milestones */
    n = 0;
    for (int i = 0; i < c->nactive; i++) { ic[n] = (IconRef){ CM_ICON_TREE, c->active[i].tree, !c->active[i].invested }; gr[n++] = 0; }
    for (int i = 0; i < c->npassive; i++) { ic[n] = (IconRef){ CM_ICON_TREE, c->passive[i].tree, !c->passive[i].invested }; gr[n++] = 1; }
    for (int i = 0; i < c->nstacks; i++) { ic[n] = (IconRef){ CM_ICON_SKILL, c->stacks[i].skill, !c->stacks[i].learned }; gr[n++] = 2; }
    icon_row(card, x0, r0 + 25, w - 8 - lookw, 24, ic, gr, n);
    /* row C: passive skills | inscriptions | soul stones | traits acquired in play */
    n = 0;
    for (int i = 0; i < c->npass && n < MUC_MAX; i++) { ic[n] = (IconRef){ CM_ICON_SKILL, c->pass[i].skill, 0 }; gr[n++] = 0; }
    for (int i = 0; i < c->ninsc && n < MUC_MAX * 2; i++) { ic[n] = (IconRef){ CM_ICON_AFFECTER, c->insc[i], 0 }; gr[n++] = 1; }
    for (int i = 0; i < c->nsouls && n < MUC_MAX * 2 + WK_MAX_LIST; i++) { ic[n] = (IconRef){ CM_ICON_AFFECTER, c->souls[i], 0 }; gr[n++] = 2; }
    for (int i = 0; i < c->nacq && n < MUC_MAX * 3; i++) { ic[n] = (IconRef){ CM_ICON_AFFECTER, c->acq[i], 0 }; gr[n++] = 3; }
    icon_row(card, x0, r0 + 50, w - 8 - lookw, 20, ic, gr, n);
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
