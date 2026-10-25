/* tf_meter.c - see tf_meter.h. Reads the hits of tf_dps (the same events as the chat log), draws with the loader's kit
 * (DSLUi) on its screen layer. Game classes by name (dump 25846389): Refactor.UI.PlayerOwnedPanelView._scrollRect (the
 * resource list), Refactor.UI.EntitySummaryUIView and its panels (the unit panel). Nothing here changes the game. */
#include "tf_meter.h"
#include "tf_dps.h"
#include "cm_win.h"
#include "cm_il.h"
#include "cm_prefs.h"
#include "mu_ui.h"
#include "i18n/ds_i18n.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef _WIN32
#include <dlfcn.h>
#endif

#define FLD(o, off, T) (*(T *)((char *)(o) + (off)))
typedef struct { float x, y, z; } V3;
typedef struct { float x, y; } V2;
typedef struct { float r, g, b, a; } Col;

#define GOLD 0xD9BE8FFFu
#define SUB 0xA69085FFu
#define MAIN 0xE8DCC8FFu
#define BODY 0x222025F2u
#define BAND 0x2D2A31FFu
#define COPPER 0xB6835AFFu
#define TRACK 0x161419FFu
#define SKROW 0x2B2830FFu

/* the panel, in its own units (drawn x its scale) */
#define MW 360.f
#define BAR 32.f
#define PAD 8.f
#define RW (MW - 2 * PAD)
#define ROW_H 34.f
#define SK_H 22.f
#define LINE_H 20.f
#define TOG_H 38.f
#define Y_BTN (BAR + 8)                       /* Start Stop Reset */
#define Y_FIRST (Y_BTN + 36)                  /* Start on first hit */
#define Y_STATS (Y_FIRST + 40)                /* 6 lines */
#define Y_HEAD (Y_STATS + 6 * LINE_H + 4)     /* the rows' column names */
#define Y_ROWS (Y_HEAD + 20)
#define TAIL (6 + 4 * TOG_H + 2 + 32 + PAD)   /* under the rows: the four switches, Restart test */
#define H_FIXED (Y_ROWS + TAIL)
#define MIN_ROWS 2
/* columns inside a row (RW wide) */
#define C_NAME 38.f
#define W_NAME 92.f
#define C_DMG 132.f
#define W_DMG 62.f
#define C_HEAL 196.f                          /* healing (ask 10): never counted as damage */
#define W_HEAL 50.f
#define C_DPS 248.f
#define W_DPS 46.f
#define C_SH 296.f
#define W_SH 42.f
/* the small panel of a test world that is not a DPS Check */
#define SW 250.f
#define SH (BAR + 8 + 32 + 8)
/* the multiplayer chat's idle lines (and tf_dps' own log panel): x >= 130, the lowest 400 units of the layer */
#define CHAT_X 130.f
#define CHAT_H 400.f

static const DSLHost *H; static const DSLUi *U;
static unsigned fr;

/* ================================================================ the measurement */
typedef struct { char id[96], name[96]; double dmg, heal; unsigned hits; } Sk;
typedef struct { double dmg, heal; unsigned hits; Sk *sk; int nsk, csk; } Un;
typedef struct { size_t a, b; } Span;                     /* counted hit indices [a, b); b = SIZE_MAX while open */
static struct {
    int running, armed;
    double t0, acc;                                      /* clock at the last (re)start; measured seconds before it */
    double rep0, rep_acc;                                /* tf_dps_repaired at the last (re)start; repaired before it */
    double total; unsigned hits;
    double heal, absorbed;                               /* healing done by the squad; damage shields absorbed (not damage) */
    size_t scan; int units_seen;                         /* next hit to look at; units the fight had */
    Un *u; int nu;
    double *ct; float *cd; size_t *cx; size_t nc, cc;    /* counted hits: measured time, damage, hit index */
    Span *sp; int nsp, csp;
} M;
static int m_dirty = 1, m_layout = 1, open_unit = -1, was_act;

static void m_zero(int arm) {
    for (int i = 0; i < M.nu; i++) free(M.u[i].sk);
    free(M.u); free(M.ct); free(M.cd); free(M.cx); free(M.sp);
    size_t scan = M.scan; int seen = M.units_seen;
    memset(&M, 0, sizeof M);
    M.scan = scan; M.units_seen = seen; M.armed = arm != 0;
    open_unit = -1; m_dirty = m_layout = 1;
}
static double m_elapsed(void) {
    double d = M.running ? tf_dps_clock() - M.t0 : 0;
    return M.acc + (d > 0 ? d : 0);
}
static double m_div(double el) { return el < 1 ? 1 : el; }
static double m_repaired(void) {
    double d = M.running ? tf_dps_repaired() - M.rep0 : 0;
    return M.rep_acc + (d > 0 ? d : 0);
}
static double m_dps5(double el) {                        /* the counted hits of the last min(5, el) measured seconds */
    double from = el - 5, s = 0;
    for (size_t k = M.nc; k-- > 0 && M.ct[k] > from; ) s += M.cd[k];
    double w = el < 5 ? el : 5;
    return el > 0 ? s / (w < 1 ? 1 : w) : 0;
}
static Un *m_unit(int i) {
    if (i < 0 || i > 100000) return NULL;
    if (i >= M.nu) {
        Un *p = realloc(M.u, (size_t)(i + 1) * sizeof *p); if (!p) return NULL;
        memset(p + M.nu, 0, (size_t)(i + 1 - M.nu) * sizeof *p); M.u = p; M.nu = i + 1;
    }
    return &M.u[i];
}
static Sk *m_skill(Un *u, const tf_hit *h) {               /* by its key, named as the hit */
    char id[96]; snprintf(id, sizeof id, "%s", h->skill_key[0] ? h->skill_key : h->skill);
    Sk *s = NULL;
    for (int k = 0; k < u->nsk && !s; k++) if (!strcmp(u->sk[k].id, id)) s = &u->sk[k];
    if (!s) {
        if (u->nsk == u->csk) { int nc = u->csk ? u->csk * 2 : 8; Sk *p = realloc(u->sk, (size_t)nc * sizeof *p); if (!p) return NULL; u->sk = p; u->csk = nc; }
        s = &u->sk[u->nsk++]; memset(s, 0, sizeof *s); snprintf(s->id, sizeof s->id, "%s", id);
    }
    snprintf(s->name, sizeof s->name, "%s", h->skill[0] ? h->skill : h->skill_key[0] ? h->skill_key : "?");
    return s;
}
static void m_span_open(size_t i) {
    if (M.nsp == M.csp) { int nc = M.csp ? M.csp * 2 : 8; Span *p = realloc(M.sp, (size_t)nc * sizeof *p); if (!p) return; M.sp = p; M.csp = nc; }
    M.sp[M.nsp].a = i; M.sp[M.nsp].b = SIZE_MAX; M.nsp++;
}
static void m_span_close(size_t n) { if (M.nsp && M.sp[M.nsp - 1].b == SIZE_MAX) M.sp[M.nsp - 1].b = n; }
static void m_start(size_t i, double t) {                /* from hit i on, the timer from t */
    M.running = 1; M.armed = 0; M.t0 = t; M.rep0 = tf_dps_repaired();
    m_span_open(i); m_dirty = 1;
}
static int counts_dmg(const tf_hit *h) { return h->kind == TFK_DMG && h->on_dummy; }   /* the DPS Check's measure */
static void m_count(size_t i, const tf_hit *h) {
    const tf_unit *src = tf_dps_unit(h->unit);
    if (h->kind == TFK_ABSORB) { M.absorbed += h->dmg; m_dirty = 1; return; }
    if (h->kind == TFK_HEAL && src && src->is_player) {  /* a squad unit's heal: its own column, never damage */
        Un *u = m_unit(h->unit);
        if (u) { if (!u->hits && !u->heal) m_layout = 1; u->heal += h->dmg; Sk *s = m_skill(u, h); if (s) s->heal += h->dmg; }
        M.heal += h->dmg; m_dirty = 1; return;
    }
    if (!counts_dmg(h)) return;
    double d = (double)h->dmg, mt = M.acc + (h->t - M.t0);
    if (mt < M.acc) mt = M.acc;
    M.total += d; M.hits++;
    Un *u = m_unit(h->unit);
    if (u) {
        if (!u->hits) m_layout = 1;                      /* a new row */
        u->dmg += d; u->hits++;
        Sk *s = m_skill(u, h); if (s) { s->dmg += d; s->hits++; }
    }
    if (M.nc == M.cc) {
        size_t nc = M.cc ? M.cc * 2 : 1024;
        double *a = realloc(M.ct, nc * sizeof *a); if (a) M.ct = a;
        float *b = realloc(M.cd, nc * sizeof *b); if (b) M.cd = b;
        size_t *c = realloc(M.cx, nc * sizeof *c); if (c) M.cx = c;
        if (!a || !b || !c) { m_dirty = 1; return; }
        M.cc = nc;
    }
    M.ct[M.nc] = mt; M.cd[M.nc] = h->dmg; M.cx[M.nc] = i; M.nc++;
    m_dirty = 1;
}
/* every frame: a new fight (the hit list or the units list started again) zeroes the meter; while a DPS Check shows, the
   new hits are counted (running), start the timer (armed: "Start on first hit"), or are passed over (stopped) */
static void m_track(int on) {
    const tf_hit *h = NULL; size_t n = tf_dps_hits(&h); int nu = tf_dps_units();
    if (n < M.scan || nu < M.units_seen) {
        m_zero(tf_dps_switch(TFD_SW_FIRSTHIT)); M.scan = 0; M.units_seen = 0;
        cmw_forget("dps:unit");
    }
    if (nu > M.units_seen) M.units_seen = nu;
    if (on && !was_act && !M.running && !M.nsp) M.armed = tf_dps_switch(TFD_SW_FIRSTHIT) != 0;   /* the fight's first frame */
    was_act = on;
    if (!on || !h) return;
    for (size_t i = M.scan; i < n; i++) {
        if (!M.running && M.armed && counts_dmg(&h[i])) m_start(i, h[i].t);
        if (M.running) m_count(i, &h[i]);
    }
    M.scan = n;
}

/* ================================================================ il2cpp by name: screen rects, scale, colours */
static Obj (*x_arr_new)(void *, uintptr_t); static void *(*x_obj_class)(Obj); static const void *(*x_vmethod)(Obj, const void *);
static cm_m m_corners, m_inparent, m_rootc, m_mode, m_cam, m_w2s, m_scale, m_euler, m_go_tr, m_act_h, m_getpos, m_size;
static const void *mi_color;
static void *k_v3, *k_canvas, *k_go, *k_pov, *k_esv, *k_eiv;
static int32_t f_pov_sr = -1, f_esv[16]; static int n_esv;
static int il_ok;
static void il_init(void) {
    if (il_ok) return;
    il_ok = 1;
    *(void **)&x_arr_new = H->il2cpp_fn("il2cpp_array_new");
    *(void **)&x_obj_class = H->il2cpp_fn("il2cpp_object_get_class");
    *(void **)&x_vmethod = H->il2cpp_fn("il2cpp_object_get_virtual_method");
    m_corners = cm_il_method("UnityEngine", "RectTransform", "GetWorldCorners", 1, NULL);
    m_inparent = cm_il_method("UnityEngine", "Component", "GetComponentInParent", 1, "System.Type");
    m_rootc = cm_il_method("UnityEngine", "Canvas", "get_rootCanvas", 0, NULL);
    m_mode = cm_il_method("UnityEngine", "Canvas", "get_renderMode", 0, NULL);
    m_cam = cm_il_method("UnityEngine", "Canvas", "get_worldCamera", 0, NULL);
    m_w2s = cm_il_method("UnityEngine", "RectTransformUtility", "WorldToScreenPoint", 2, "UnityEngine.Camera,UnityEngine.Vector3");
    m_scale = cm_il_method("UnityEngine", "Transform", "set_localScale", 1, NULL);
    m_getpos = cm_il_method("UnityEngine", "RectTransform", "get_anchoredPosition", 0, NULL);
    m_size = cm_il_method("UnityEngine", "RectTransform", "set_sizeDelta", 1, NULL);
    m_euler = cm_il_method("UnityEngine", "Transform", "set_localEulerAngles", 1, NULL);
    m_go_tr = cm_il_method("UnityEngine", "GameObject", "get_transform", 0, NULL);
    m_act_h = cm_il_method("UnityEngine", "GameObject", "get_activeInHierarchy", 0, NULL);
    void *k_gr = cm_il_class("UnityEngine.UI", "Graphic");
    mi_color = k_gr ? H->find_method(k_gr, "set_color", 1) : NULL;
    k_v3 = cm_il_class("UnityEngine", "Vector3"); k_canvas = cm_il_class("UnityEngine", "Canvas"); k_go = cm_il_class("UnityEngine", "GameObject");
    k_pov = cm_il_class("Refactor.UI", "PlayerOwnedPanelView"); k_esv = cm_il_class("Refactor.UI", "EntitySummaryUIView");
    k_eiv = cm_il_class("Refactor.UI", "EntityInspectionView");
    f_pov_sr = k_pov ? cm_il_field(k_pov, "_scrollRect") : -1;
    static const char *const parts[] = { "_imgPortrait", "_txtName", "_playerStatusUI", "_buildingStatusUI", "_itemStatusUI", "_optionUI",
                                         "_affecetUI", "_toggleButtons", "_unitPanel", "_onlyPlayerPanel", "_buildingPanel", "_itemPanel", "_renameButton" };
    for (size_t i = 0; k_esv && i < sizeof parts / sizeof parts[0]; i++) { int32_t o = cm_il_field(k_esv, parts[i]); if (o >= 0) f_esv[n_esv++] = o; }
    cmw_log("tf: DPS meter: resource list %s (scroll %d), unit panel %s (%d parts), rects %s", k_pov ? "known" : "NOT FOUND", f_pov_sr,
            k_esv ? "known" : "NOT FOUND", n_esv, m_corners.fn && x_arr_new && k_v3 ? "ok" : "MISSING");
}
static Obj rt_of(Obj o) {                                /* a component's or a GameObject's transform */
    if (!cm_alive(o)) return NULL;
    if (x_obj_class && k_go && x_obj_class(o) == k_go) return m_go_tr.fn ? CM_CALL(m_go_tr, Obj, Obj, const void *)(o, m_go_tr.mi) : NULL;
    return cm_tr(o);
}
static int go_on(Obj o) {                                /* activeInHierarchy */
    Obj g = !cm_alive(o) ? NULL : x_obj_class && k_go && x_obj_class(o) == k_go ? o : cm_go(o);
    return g && m_act_h.fn && CM_CALL(m_act_h, bool, Obj, const void *)(g, m_act_h.mi);
}
/* screen rect in pixels {x0, y0, x1, y1}, origin bottom left (overlay canvas: world = pixels; camera canvas: through its
   camera), as the kit's own. 0 ok */
static uintptr_t h_arr;
static int scr_rect(Obj o, float r[4]) {
    Obj rt = rt_of(o);
    if (!rt || !m_corners.fn || !x_arr_new || !k_v3) return -1;
    Obj arr = cm_il_hget(h_arr);
    if (!arr) { arr = x_arr_new(k_v3, 4); if (!arr) return -1; h_arr = cm_il_hnew(arr); }
    CM_CALL(m_corners, void, Obj, Obj, const void *)(rt, arr, m_corners.mi);
    V3 c[4]; memcpy(c, (char *)arr + 0x20, sizeof c);
    Obj cv = m_inparent.fn && k_canvas ? CM_CALL(m_inparent, Obj, Obj, Obj, const void *)(rt, cm_il_type_obj(k_canvas), m_inparent.mi) : NULL;
    Obj root = cm_alive(cv) && m_rootc.fn ? CM_CALL(m_rootc, Obj, Obj, const void *)(cv, m_rootc.mi) : NULL;
    Obj cam = cm_alive(root) && m_mode.fn && m_cam.fn && CM_CALL(m_mode, int32_t, Obj, const void *)(root, m_mode.mi) != 0
            ? CM_CALL(m_cam, Obj, Obj, const void *)(root, m_cam.mi) : NULL;
    for (int i = 0; i < 4; i++) if (cm_alive(cam) && m_w2s.fn) { V2 s = CM_CALL(m_w2s, V2, Obj, V3, const void *)(cam, c[i], m_w2s.mi); c[i].x = s.x; c[i].y = s.y; }
    r[0] = r[2] = c[0].x; r[1] = r[3] = c[0].y;
    for (int i = 1; i < 4; i++) { if (c[i].x < r[0]) r[0] = c[i].x; if (c[i].x > r[2]) r[2] = c[i].x; if (c[i].y < r[1]) r[1] = c[i].y; if (c[i].y > r[3]) r[3] = c[i].y; }
    return r[2] > r[0] && r[3] > r[1] ? 0 : -1;
}
static void set_scale(DSLObj o, float s) { if (o && m_scale.fn) CM_CALL(m_scale, void, Obj, V3, const void *)((Obj)o, (V3){ s, s, 1 }, m_scale.mi); }
/* MOVE RULE (owner 2026-10-25, kit `movable`): both panels move with a right-click drag, stay on the screen and keep their
   place ("custom-modes.dps_meter" / ".tf_restart"). Until the player moves one (or the kit puts a kept place back), it keeps
   finding its own place (under the resource list, right of an open Inspect window); once its position reads back other
   than where we put it, only its size and scale change from then on (the kit pivot is its top-left: a size change keeps
   its place). UI Scale stays ours (rules_own DSL_RULE_SCALE: the scale shrinks when its room is short). */
static int moved_m, moved_s, mov_m, mov_s;
static int was_moved(DSLObj o, float x, float y) {
    if (!o || !m_getpos.fn || x < 0) return 0;
    V2 p = CM_CALL(m_getpos, V2, Obj, const void *)((Obj)o, m_getpos.mi);
    return (p.x - x > 0.5f || x - p.x > 0.5f) || (-p.y - y > 0.5f || y + p.y > 0.5f);
}
static void resize(DSLObj o, float w, float h) { if (o && m_size.fn) CM_CALL(m_size, void, Obj, V2, const void *)((Obj)o, (V2){ w, h }, m_size.mi); }
static void make_movable(DSLObj o, const char *key, int *done) {
    if (*done || !o) return;
    *done = 1; dsl_rules_own(U, o, DSL_RULE_SCALE);
    int r = dsl_movable(U, o, key);
    cmw_log("tf: %s: right-drag move %s (%d)", key, r ? "not in this kit" : "on", r);
}
static void txt_color(DSLObj w, uint32_t c) {            /* Graphic.color through the text's own override */
    Obj t = w ? cm_text_of(w) : NULL;
    const void *vm = t && x_vmethod && mi_color ? x_vmethod(t, mi_color) : NULL;
    if (vm) ((void (*)(Obj, Col, const void *))(*(void *const *)vm))(t, (Col){ (c >> 24) / 255.f, (c >> 16 & 255) / 255.f, (c >> 8 & 255) / 255.f, (c & 255) / 255.f }, vm);
}
static void chevron(DSLObj b, int open) {               /* the fold arrow (the kit's "›"): pointing down while open */
    Obj t = b ? cm_text_of(b) : NULL;
    if (t && m_euler.fn) CM_CALL(m_euler, void, Obj, V3, const void *)(cm_tr(t), (V3){ 0, 0, open ? -90.f : 0 }, m_euler.mi);
}

/* ================================================================ widgets */
static DSLObj plate(DSLObj p, float x, float y, float w, float h, uint32_t c) {   /* a flat colour, clicks pass through */
    DSLObj o = U->panel(p, x, y, w, h, c ? c : 0x01u);
    if (o) { cm_set_sprite(o, NULL, c ? c : 0x01u); cm_raycast(o, false); }
    return o;
}
static DSLObj txt(DSLObj p, float x, float y, float w, float h, const char *s, float size, int flags, uint32_t c) {
    float fs = mu_text_size(size);                        /* Japanese / Korean / Chinese never under 14 */
    if (fs != size && h < fs * 1.6f) { y -= (fs * 1.6f - h) / 2; h = fs * 1.6f; }
    DSLObj t = U->text(p, x, y, w, h, s ? s : "", fs, flags, c);
    if (t) { cm_text_ellipsis(t); cm_raycast(t, false); }
    return t;
}
static void put(DSLObj o, float x, float y, float w, float h) { if (o) U->place(o, x, y, w, h); }
static void act(DSLObj o, int on) { if (o) U->set_active(o, on); }
static void set_if(DSLObj t, char *cache, size_t n, const char *s) {   /* only a changed text is written */
    if (!t || !strcmp(cache, s)) return;
    snprintf(cache, n, "%s", s); U->set_text(t, s);
}
static void named_if(DSLObj t, char *cache, size_t n, const char *s) {
    if (!t || !strcmp(cache, s)) return;
    snprintf(cache, n, "%s", s); cmw_set_text(t, s);
}
/* numbers as the Test Fights card writes them: 1,000,000 / 1.000.000 / 1 000 000 (the language's separator) */
static int dec_comma(void) { const char *c = dsi_code(); return !strcmp(c, "de") || !strcmp(c, "es") || !strcmp(c, "pt-BR") || !strcmp(c, "fr") || !strcmp(c, "ru"); }
static void fmt_n(double v, char *o, size_t n) {
    const char *c = dsi_code(), *sep = !strcmp(c, "de") || !strcmp(c, "es") || !strcmp(c, "pt-BR") ? "."
                                      : !strcmp(c, "fr") || !strcmp(c, "ru") ? "\xC2\xA0" : ",";
    char d[40]; snprintf(d, sizeof d, "%.0f", v); size_t l = strlen(d), k = 0, sl = strlen(sep);
    for (size_t i = 0; i < l && k + sl + 1 < n; i++) { if (i && d[i - 1] != '-' && (l - i) % 3 == 0) { memcpy(o + k, sep, sl); k += sl; } o[k++] = d[i]; }
    o[k] = 0;
}
static void fmt_rate(double v, char *o, size_t n) {      /* a DPS: whole numbers, one decimal under 10 */
    if (v > 0 && v < 9.95) { snprintf(o, n, "%.1f", v); if (dec_comma()) for (char *p = o; *p; p++) if (*p == '.') *p = ','; }
    else fmt_n(v, o, n);
}
static void fmt_time(double s, char *o, size_t n) {
    long t = s > 0 ? (long)s : 0;
    if (t >= 3600) snprintf(o, n, "%ld:%02ld:%02ld", t / 3600, t / 60 % 60, t % 60);
    else snprintf(o, n, "%ld:%02ld", t / 60, t % 60);
}
static void fmt_share(double part, double total, char *o, size_t n) {
    double p = total > 0 ? part * 100 / total : 0;
    if (p > 0 && p < 1) snprintf(o, n, "<1%%"); else snprintf(o, n, "%.0f%%", p);
}

/* ---------------------------------------------------------------- the panel */
typedef struct { DSLObj b, por, name, dmg, heal, dps, sh, fill; int unit, on, por_ok, open; unsigned por_try; float y, fw;
                 char tn[96], td[40], th[40], tp[40], ts[16]; } Row;
typedef struct { DSLObj p, name, dmg, heal, dps, hits; int on; float y; char tn[96], td[40], tl[40], tp[40], th[24]; } SRow;
#define NSW 4
#define NVAL 6
static DSLObj p_root, p_body, p_fold, p_status, b_start, b_stop, b_reset, t_first, t_sw[NSW], b_restart, l_val[NVAL], sc_rows, l_tail;
static DSLObj s_root, s_btn;
static char c_status[96], c_val[NVAL][48];
static Row *rows; static int nrows; static SRow *srows; static int nsrows;
static float rows_h = -1;
static int folded, prefs_read, mode, en_start = -1, en_stop = -1, en_rst = -1, st_kind = -1, s_en = -1;
static unsigned gen, last_ref;
static const int sw_of[NSW] = { TFD_SW_SPEECH, TFD_SW_HEALNUM, TFD_SW_CHATLOG, TFD_SW_EFFECTS };
static const char *const sw_name[NSW] = { "dps:speech", "dps:healnum", "dps:chatlog", "dps:effects" };
static const char *const val_name[NVAL] = { "dps:time", "dps:total", "dps:dps", "dps:dps5", "dps:repaired", "dps:absorbed" };

static void refresh(void); static void place_now(void);
static int title_up = 1;
static int cur_act(void) { return !title_up && tf_dps_active(); }
static void refresh_now(void) { m_dirty = 1; if (mode == 1) { refresh(); place_now(); last_ref = fr; } }

static void start_click(void *ud, DSLObj w) {
    (void)ud; (void)w; m_track(cur_act());          /* the hits before the click belong to before it */
    if (!M.running) m_start(M.scan, tf_dps_clock());
    refresh_now();
}
static void stop_click(void *ud, DSLObj w) {
    (void)ud; (void)w; m_track(cur_act());
    if (M.running) {
        double d = tf_dps_clock() - M.t0, r = tf_dps_repaired() - M.rep0;
        M.acc += d > 0 ? d : 0; M.rep_acc += r > 0 ? r : 0; M.running = 0; m_span_close(M.scan);
    }
    M.armed = 0;
    refresh_now();
}
static void reset_click(void *ud, DSLObj w) {
    (void)ud; (void)w; m_track(cur_act());         /* every hit so far is left behind */
    m_zero(tf_dps_switch(TFD_SW_FIRSTHIT));
    cmw_forget("dps:unit");
    refresh_now();
}
static void first_change(void *ud, DSLObj w) {
    (void)ud; int on = U->toggle_get(w);
    tf_dps_switch_set(TFD_SW_FIRSTHIT, on);
    if (!M.running && !M.nsp) M.armed = on;               /* an empty measurement waits for the first hit (or not) */
    else if (!on) M.armed = 0;
    refresh_now();
}
static void sw_change(void *ud, DSLObj w) { tf_dps_switch_set((int)(intptr_t)ud, U->toggle_get(w)); }
static void restart_click(void *ud, DSLObj w) {
    (void)ud; (void)w;
    if (!tf_restart_ready()) { cmw_log("tf: DPS meter: Restart test: no test world to restart"); return; }
    int r = tf_restart();
    cmw_log("tf: DPS meter: Restart test %s (%d)", r ? "refused" : "queued", r);
}
static void unit_toggle(int k) { open_unit = open_unit == k ? -1 : k; m_layout = 1; refresh_now(); }
static void unit_click(void *ud, DSLObj w) { (void)w; unit_toggle((int)(intptr_t)ud); }
static void row_click(void *ud, DSLObj w) { (void)w; int s = (int)(intptr_t)ud; if (s >= 0 && s < nrows && rows[s].unit >= 0) unit_toggle(rows[s].unit); }
static void fold_click(void *ud, DSLObj w);
static void fold_apply(void) {                           /* the arrow, its test name, the body */
    chevron(p_fold, !folded);
    cmw_named(p_fold, CMW_BUTTON, "dps:fold", fold_click, NULL, folded ? "folded" : "open");
    act(p_body, !folded);
}
static void fold_click(void *ud, DSLObj w) {
    (void)ud; (void)w; folded = !folded;
    cm_pref_set("dps_meter_folded", folded ? "1" : "0");
    fold_apply(); m_layout = 1;
    refresh_now();
}

static void forget_all(void) {
    cmw_forget("dps:");
    free(rows); rows = NULL; nrows = 0; free(srows); srows = NULL; nsrows = 0;
    p_root = p_body = p_fold = p_status = b_start = b_stop = b_reset = t_first = b_restart = sc_rows = l_tail = NULL;
    memset(t_sw, 0, sizeof t_sw); memset(l_val, 0, sizeof l_val);
    c_status[0] = 0; memset(c_val, 0, sizeof c_val); rows_h = -1;
    en_start = en_stop = en_rst = st_kind = s_en = -1;
    s_root = s_btn = NULL;
}
static void destroy_all(void) {                          /* a language change: made again at the next show */
    if (p_root && cm_alive(p_root)) U->destroy(p_root);
    if (s_root && cm_alive(s_root)) U->destroy(s_root);
    forget_all(); mode = 0; mov_m = mov_s = moved_m = moved_s = 0;
}

static int build_full(void) {
    il_init();
    p_root = cmw_flat(U->panel(NULL, 0, 0, MW, 400, 0), BODY);
    if (!p_root) return -1;
    dsl_esc_push(U, p_root, NULL, NULL, DSL_ESC_HUD);                 /* a HUD panel (fold, no Close): never takes Escape (loader NOTES "Escape") */
    plate(p_root, 0, 0, MW, BAR, BAND);
    plate(p_root, 0, BAR, MW, 1, COPPER);
    p_fold = U->button(p_root, 4, 3, 26, 26, "\xE2\x80\xBA", fold_click, NULL);
    txt(p_root, 36, 0, 150, BAR, TR("DPS Check"), 17, DSL_TXT_LEFT | DSL_TXT_TITLE, GOLD);
    p_status = cmw_named(txt(p_root, 190, 0, MW - 190 - PAD, BAR, "", 13, DSL_TXT_RIGHT, SUB), CMW_TEXT, "dps:status", NULL, NULL, "");
    p_body = plate(p_root, 0, 0, MW, 400, 0);             /* everything under the title bar: hidden while folded */
    float bw = (RW - 12) / 3;
    const char *ls = cm_gw("TEXTKEY_Common_Start", TR_NOOP("Start"));
    b_start = cmw_named(U->button(p_body, PAD, Y_BTN, bw, 30, ls, start_click, NULL), CMW_BUTTON, "dps:start", start_click, NULL, ls);
    b_stop = cmw_named(U->button(p_body, PAD + bw + 6, Y_BTN, bw, 30, TR("Stop"), stop_click, NULL), CMW_BUTTON, "dps:stop", stop_click, NULL, TR("Stop"));
    b_reset = cmw_named(U->button(p_body, PAD + 2 * (bw + 6), Y_BTN, bw, 30, TR("Reset"), reset_click, NULL), CMW_BUTTON, "dps:reset", reset_click, NULL, TR("Reset"));
    const char *lf = tf_dps_switch_label(TFD_SW_FIRSTHIT);
    t_first = cmw_named(U->toggle(p_body, PAD, Y_FIRST, RW, lf, tf_dps_switch(TFD_SW_FIRSTHIT), first_change, NULL), CMW_TOGGLE, "dps:firsthit", first_change, NULL, lf);
    const char *lab[NVAL] = { cm_gw("TEXTKEY_Shared_Time_NAME", TR_NOOP("Time")), cm_gw("TEXTKEY_STATUS_DamageDealtBonus_NAME", TR_NOOP("Damage Dealt")),
                              TR("DPS"), TR("DPS (Last 5 s)"), TR("Repaired by the Scarecrow"), TR("Absorbed") };
    for (int i = 0; i < NVAL; i++) {
        float y = Y_STATS + (float)i * LINE_H;
        txt(p_body, PAD, y, RW - 112, LINE_H, lab[i], 14, DSL_TXT_LEFT, SUB);
        l_val[i] = cmw_named(txt(p_body, PAD + RW - 110, y, 110, LINE_H, "", 15, DSL_TXT_RIGHT, i == 2 ? GOLD : MAIN), CMW_TEXT, val_name[i], NULL, NULL, "");
    }
    txt(p_body, PAD + C_NAME, Y_HEAD, W_NAME, 18, TR("Unit"), 12, DSL_TXT_LEFT, SUB);
    txt(p_body, PAD + C_DMG, Y_HEAD, W_DMG, 18, TR("Damage"), 12, DSL_TXT_RIGHT, SUB);
    txt(p_body, PAD + C_HEAL, Y_HEAD, W_HEAL, 18, TR("Healing"), 12, DSL_TXT_RIGHT, SUB);
    txt(p_body, PAD + C_DPS, Y_HEAD, W_DPS, 18, TR("DPS"), 12, DSL_TXT_RIGHT, SUB);
    txt(p_body, PAD + C_SH, Y_HEAD, W_SH, 18, TR("Share"), 12, DSL_TXT_RIGHT, SUB);
    plate(p_body, PAD, Y_ROWS - 2, RW, 1, COPPER);
    sc_rows = U->scroll(p_body, PAD, Y_ROWS, RW, MIN_ROWS * ROW_H);
    l_tail = plate(p_body, PAD, Y_ROWS, RW, 1, COPPER);
    for (int k = 0; k < NSW; k++) {
        const char *l = tf_dps_switch_label(sw_of[k]);
        t_sw[k] = cmw_named(U->toggle(p_body, PAD, Y_ROWS, RW, l, tf_dps_switch(sw_of[k]), sw_change, (void *)(intptr_t)sw_of[k]),
                            CMW_TOGGLE, sw_name[k], sw_change, (void *)(intptr_t)sw_of[k], l);
    }
    b_restart = U->button(p_body, PAD, Y_ROWS, RW, 32, TR("Restart Test Floor"), restart_click, NULL);
    cm_raycast(p_root, true);                             /* the panel takes the clicks over it */
    if (!prefs_read) { char v[8]; if (!cm_pref_get("dps_meter_folded", v, sizeof v)) folded = v[0] == '1'; prefs_read = 1; }
    fold_apply();
    m_dirty = m_layout = 1;
    cmw_log("tf: DPS meter built%s", sc_rows ? "" : " (no scroll list)");
    return 0;
}
static int build_small(void) {
    il_init();
    s_root = cmw_flat(U->panel(NULL, 0, 0, SW, SH, 0), BODY);
    if (!s_root) return -1;
    dsl_esc_push(U, s_root, NULL, NULL, DSL_ESC_HUD);                 /* a HUD panel (no Close) */
    plate(s_root, 0, 0, SW, BAR, BAND);
    plate(s_root, 0, BAR, SW, 1, COPPER);
    txt(s_root, PAD + 4, 0, SW - 2 * PAD - 4, BAR, TR("Test Fights"), 17, DSL_TXT_LEFT | DSL_TXT_TITLE, GOLD);
    s_btn = U->button(s_root, PAD, BAR + 8, SW - 2 * PAD, 32, TR("Restart Test Floor"), restart_click, NULL);
    cm_raycast(s_root, true);
    return 0;
}

/* ---------------------------------------------------------------- rows */
static Row *row_at(int k) {
    while (nrows <= k) {
        Row *p = realloc(rows, (size_t)(nrows + 1) * sizeof *p); if (!p) return NULL;
        rows = p; Row *r = &rows[nrows]; memset(r, 0, sizeof *r); r->unit = -1; r->y = -1; r->fw = -1;
        r->b = U->button(sc_rows, 0, 0, RW, ROW_H - 2, "", row_click, (void *)(intptr_t)nrows);
        if (!r->b) return NULL;
        r->por = U->panel(r->b, 4, 2, 28, 28, 0xFFFFFFFFu); if (r->por) { cm_raycast(r->por, false); act(r->por, 0); }
        r->name = txt(r->b, C_NAME, 1, W_NAME - 4, 24, "", 14, DSL_TXT_LEFT, MAIN);
        r->dmg = txt(r->b, C_DMG, 1, W_DMG, 24, "", 14, DSL_TXT_RIGHT, MAIN);
        r->heal = txt(r->b, C_HEAL, 1, W_HEAL, 24, "", 14, DSL_TXT_RIGHT, 0x7EC86BFFu);
        r->dps = txt(r->b, C_DPS, 1, W_DPS, 24, "", 14, DSL_TXT_RIGHT, MAIN);
        r->sh = txt(r->b, C_SH, 1, W_SH, 24, "", 14, DSL_TXT_RIGHT, GOLD);
        plate(r->b, C_NAME, 26, RW - C_NAME - 6, 3, TRACK);
        r->fill = plate(r->b, C_NAME, 26, 0, 3, COPPER);
        U->tooltip(r->b, NULL, TR("Click to show or hide the damage per skill."));
        r->on = 1;
        nrows++;
    }
    return &rows[k];
}
static SRow *srow_at(int k) {
    while (nsrows <= k) {
        SRow *p = realloc(srows, (size_t)(nsrows + 1) * sizeof *p); if (!p) return NULL;
        srows = p; SRow *q = &srows[nsrows]; memset(q, 0, sizeof *q); q->y = -1;
        q->p = plate(sc_rows, 0, 0, RW, SK_H, SKROW);
        if (!q->p) return NULL;
        q->name = txt(q->p, C_NAME + 8, 0, W_NAME - 10, SK_H, "", 13, DSL_TXT_LEFT, SUB);
        q->dmg = txt(q->p, C_DMG, 0, W_DMG, SK_H, "", 13, DSL_TXT_RIGHT, MAIN);
        q->heal = txt(q->p, C_HEAL, 0, W_HEAL, SK_H, "", 13, DSL_TXT_RIGHT, 0x7EC86BFFu);
        q->dps = txt(q->p, C_DPS, 0, W_DPS, SK_H, "", 13, DSL_TXT_RIGHT, MAIN);
        q->hits = txt(q->p, C_SH, 0, W_SH, SK_H, "", 13, DSL_TXT_RIGHT, SUB);
        q->on = 1;
        nsrows++;
    }
    return &srows[k];
}
static int by_dmg(const Un *u, int a, int b) {            /* 1 = a before b */
    return u[a].dmg != u[b].dmg ? u[a].dmg > u[b].dmg : u[a].hits != u[b].hits ? u[a].hits > u[b].hits : a < b;
}
static int sorted_units(int *ord, int cap) {
    int n = 0;
    for (int i = 0; i < M.nu && n < cap; i++) {
        if (!M.u[i].hits && !(M.u[i].heal > 0)) continue;
        int k = n++;
        while (k > 0 && by_dmg(M.u, i, ord[k - 1])) { ord[k] = ord[k - 1]; k--; }
        ord[k] = i;
    }
    return n;
}
static int sorted_skills(const Un *u, int *ord, int cap) {
    int n = 0;
    for (int i = 0; i < u->nsk && n < cap; i++) {
        int k = n++;
        while (k > 0 && (u->sk[i].dmg > u->sk[ord[k - 1]].dmg || (u->sk[i].dmg == u->sk[ord[k - 1]].dmg && u->sk[i].hits > u->sk[ord[k - 1]].hits))) { ord[k] = ord[k - 1]; k--; }
        ord[k] = i;
    }
    return n;
}
#define MAXU 256
#define MAXSK 128
static void rows_paint(double div) {
    if (!sc_rows) return;
    int ord[MAXU], n = sorted_units(ord, MAXU), si = 0; float y = 0; char b[64];
    for (int k = 0; k < n; k++) {
        Row *r = row_at(k); if (!r) break;
        int ui = ord[k]; const Un *u = &M.u[ui]; const tf_unit *t = tf_dps_unit(ui);
        if (!r->on) { act(r->b, 1); r->on = 1; }
        if (r->y != y) { put(r->b, 0, y, RW, ROW_H - 2); r->y = y; }
        if (r->unit != ui) {
            r->unit = ui; r->por_ok = 0; r->por_try = 0;
            char nm[32]; snprintf(nm, sizeof nm, "dps:unit%d", ui);
            cmw_named(r->b, CMW_BUTTON, nm, unit_click, (void *)(intptr_t)ui, t && t->name[0] ? t->name : "?");
        }
        if (!r->por_ok && r->por && (!r->por_try || fr - r->por_try > 60)) {   /* the portrait (again later while not loaded) */
            Obj sp = t && t->portrait[0] ? cm_icon(CM_ICON_PORTRAIT, t->portrait) : NULL;
            r->por_try = fr ? fr : 1;
            if (sp) cm_set_sprite(r->por, sp, 0xFFFFFFFFu);
            act(r->por, sp != NULL);
            r->por_ok = sp != NULL || !t || !t->portrait[0];
        }
        set_if(r->name, r->tn, sizeof r->tn, t && t->name[0] ? t->name : "?");
        if (r->open != (ui == open_unit)) { r->open = ui == open_unit; txt_color(r->name, r->open ? GOLD : MAIN); }
        fmt_n(u->dmg, b, sizeof b); set_if(r->dmg, r->td, sizeof r->td, b);
        if (u->heal > 0) fmt_n(u->heal, b, sizeof b); else b[0] = 0;
        set_if(r->heal, r->th, sizeof r->th, b);
        fmt_rate(u->dmg / div, b, sizeof b); set_if(r->dps, r->tp, sizeof r->tp, b);
        fmt_share(u->dmg, M.total, b, sizeof b); set_if(r->sh, r->ts, sizeof r->ts, b);
        float fw = M.total > 0 ? (float)((RW - C_NAME - 6) * (u->dmg / M.total)) : 0;
        if (fw < 0) fw = 0;
        if (r->fw < 0 || (fw - r->fw) * (fw - r->fw) >= 0.25f) { put(r->fill, C_NAME, 26, fw, 3); r->fw = fw; }
        y += ROW_H;
        if (ui != open_unit) continue;
        int so[MAXSK], ns = sorted_skills(u, so, MAXSK);
        for (int j = 0; j < ns; j++) {
            SRow *q = srow_at(si); if (!q) break;
            si++;
            const Sk *s = &u->sk[so[j]];
            if (!q->on) { act(q->p, 1); q->on = 1; }
            if (q->y != y) { put(q->p, 0, y, RW, SK_H - 1); q->y = y; }
            set_if(q->name, q->tn, sizeof q->tn, s->name);
            fmt_n(s->dmg, b, sizeof b); set_if(q->dmg, q->td, sizeof q->td, b);
            if (s->heal > 0) fmt_n(s->heal, b, sizeof b); else b[0] = 0;
            set_if(q->heal, q->tl, sizeof q->tl, b);
            fmt_rate(s->dmg / div, b, sizeof b); set_if(q->dps, q->tp, sizeof q->tp, b);
            snprintf(b, sizeof b, "\xC3\x97%u", s->hits); set_if(q->hits, q->th, sizeof q->th, b);
            y += SK_H;
        }
    }
    for (int k = n; k < nrows; k++) if (rows[k].on) { act(rows[k].b, 0); rows[k].on = 0; rows[k].unit = -1; }
    for (int k = si; k < nsrows; k++) if (srows[k].on) { act(srows[k].p, 0); srows[k].on = 0; }
    if (y != rows_h) { rows_h = y; U->scroll_height(sc_rows, y); }
}

/* ---------------------------------------------------------------- texts (at most every 10 frames, only what changed) */
static void refresh(void) {
    if (!p_root) return;
    m_dirty = 0;
    double el = m_elapsed(), div = m_div(el);
    char b[64];
    fmt_time(el, b, sizeof b); named_if(l_val[0], c_val[0], sizeof c_val[0], b);
    fmt_n(M.total, b, sizeof b); named_if(l_val[1], c_val[1], sizeof c_val[1], b);
    fmt_rate(M.hits ? M.total / div : 0, b, sizeof b); named_if(l_val[2], c_val[2], sizeof c_val[2], b);
    fmt_rate(m_dps5(el), b, sizeof b); named_if(l_val[3], c_val[3], sizeof c_val[3], b);
    fmt_n(m_repaired(), b, sizeof b); named_if(l_val[4], c_val[4], sizeof c_val[4], b);
    fmt_n(M.absorbed, b, sizeof b); named_if(l_val[5], c_val[5], sizeof c_val[5], b);
    int kind = M.running ? 0 : M.armed ? 1 : 2;
    named_if(p_status, c_status, sizeof c_status, kind == 0 ? TR("Running") : kind == 1 ? TR("Waiting for the first hit") : TR("Stopped"));
    if (kind != st_kind) { txt_color(p_status, kind == 0 ? GOLD : SUB); st_kind = kind; }
    if (en_start != !M.running) U->set_enabled(b_start, en_start = !M.running);
    if (en_stop != (M.running || M.armed)) U->set_enabled(b_stop, en_stop = M.running || M.armed);
    if (!folded) rows_paint(div);
}

/* ================================================================ placement */
typedef struct { float x0, y0, x1, y1; } RU;              /* kit layer units, origin top left, y down */
static struct {
    int region, list_found, up_found, layer_found; float lw, lh, ds, s, x, y, ph, avail, bottom; RU list; float up_top;
    float rs[4], us[4], ls[4];
    int ins_found; float is[4]; float ins_x1;             /* the Inspect window (EntityInspectionView) while it shows */
} PL;
static uintptr_t h_pov, h_esv; static unsigned pov_try, esv_try;
static int list_rect(float r[4]) {                       /* the resource list's ScrollRect (the most it draws) */
    if (!k_pov) return -1;
    Obj v = cm_il_hget(h_pov);
    if (!cm_alive(v)) {
        if (pov_try && fr - pov_try < 120) return -1;
        pov_try = fr; v = cm_find(k_pov, false); cm_il_hset(&h_pov, v);
        if (!v) return -1;
    }
    if (!go_on(v)) return -1;
    Obj sr = f_pov_sr >= 0 ? FLD(v, f_pov_sr, Obj) : NULL;
    return scr_rect(cm_alive(sr) ? sr : v, r);
}
static int unit_rect(float r[4], const float ls[4]) {   /* the unit panel: all its parts (shown or not: it never jumps) */
    if (!k_esv) return -1;
    Obj v = cm_il_hget(h_esv);
    if (!cm_alive(v)) {
        if (esv_try && fr - esv_try < 120) return -1;
        esv_try = fr; v = cm_find(k_esv, true); cm_il_hset(&h_esv, v);
        if (!v) return -1;
    }
    float sw = ls[2] - ls[0], sh = ls[3] - ls[1]; int any = 0;
    for (int i = -1; i < n_esv; i++) {
        Obj o = i < 0 ? v : FLD(v, f_esv[i], Obj); float q[4];
        if (!cm_alive(o) || scr_rect(o, q)) continue;
        if (q[2] - q[0] > 0.7f * sw || q[3] - q[1] > 0.7f * sh) continue;   /* a screen-wide holder, not a panel */
        if (!any) memcpy(r, q, sizeof q);
        else { if (q[0] < r[0]) r[0] = q[0]; if (q[1] < r[1]) r[1] = q[1]; if (q[2] > r[2]) r[2] = q[2]; if (q[3] > r[3]) r[3] = q[3]; }
        any = 1;
    }
    return any ? 0 : -1;
}
static void to_units(const float s[4], RU *u) {
    float sx = PL.lw / (PL.ls[2] - PL.ls[0]), sy = PL.lh / (PL.ls[3] - PL.ls[1]);
    u->x0 = (s[0] - PL.ls[0]) * sx; u->x1 = (s[2] - PL.ls[0]) * sx;
    u->y0 = (PL.ls[3] - s[3]) * sy; u->y1 = (PL.ls[3] - s[1]) * sy;
}
/* every 30 frames: where the game's panels are (layer units) */
static void band_update(DSLObj any_root) {
    il_init();
    Obj layer = any_root ? cm_parent((Obj)any_root) : NULL;
    float lw = 0, lh = 0;
    if (!layer || cm_rect_size(layer, &lw, &lh) || lw < 100 || lh < 100) { lw = 1920; lh = 1080; }
    PL.lw = lw; PL.lh = lh; PL.ds = mu_ui_scale();
    PL.layer_found = layer && !scr_rect(layer, PL.ls);
    float ds = PL.ds;
    PL.list = (RU){ 0, 170 * ds, 120 * ds, 340 * ds };    /* not found: measured at 960x540, UI scale 1 */
    PL.up_top = lh - 215 * ds;
    PL.list_found = PL.layer_found && !list_rect(PL.rs);
    if (PL.list_found) to_units(PL.rs, &PL.list);
    PL.up_found = PL.layer_found && !unit_rect(PL.us, PL.ls);
    if (PL.up_found) { RU u; to_units(PL.us, &u); PL.up_top = u.y0; }
    { Obj iv = k_eiv ? cm_find(k_eiv, false) : NULL;      /* owner: the Inspect window must stay readable: the meter goes right of it */
      PL.ins_found = PL.layer_found && iv && go_on(iv) && !scr_rect(iv, PL.is);
      PL.ins_x1 = 0; if (PL.ins_found) { RU u; to_units(PL.is, &u); PL.ins_x1 = u.x1; } }
    float b = lh - CHAT_H;                                /* the panel always reaches past x 130: above the chat */
    PL.bottom = (PL.up_top < b ? PL.up_top : b) - 10;
}
/* the region and scale for a panel of min height need (own units): under the list (1) or beside it (2) */
static void region(float need, float w, float *x, float *y, float *s, float *avail) {
    float ds = PL.ds, y1 = PL.list.y1 + 10, a1 = PL.bottom - y1, x2 = PL.list.x1 + 10, y2 = PL.list.y0, a2 = PL.bottom - y2;
    float s1 = a1 / need, s2 = a2 / need;
    if (s1 > ds) s1 = ds;
    if (s2 > ds) s2 = ds;
    if (x2 + w * s2 > PL.lw - 8) s2 = (PL.lw - 8 - x2) / w;
    int r1 = s1 >= ds - 0.001f || s1 >= s2;
    PL.region = r1 ? 1 : 2;
    *x = r1 ? 8 : x2; *y = r1 ? y1 : y2; *avail = r1 ? a1 : a2; *s = r1 ? s1 : s2;
    if (PL.ins_x1 > 0 && *x < PL.ins_x1 + 10) {           /* right of the open Inspect window, smaller if the screen ends */
        *x = PL.ins_x1 + 10;
        if (*x + w * *s > PL.lw - 8) *s = (PL.lw - 8 - *x) / w;
    }
    if (*s < 0.5f) *s = 0.5f;
}
static void place_now(void) {
    if (mode == 1 && p_root) {
        float x, y, s, avail, rh = rows_h > MIN_ROWS * ROW_H ? rows_h : MIN_ROWS * ROW_H;
        region(H_FIXED + MIN_ROWS * ROW_H, MW, &x, &y, &s, &avail);
        float h = H_FIXED + rh, room = avail / s;
        if (h > room) h = room;
        if (h < H_FIXED + MIN_ROWS * ROW_H) h = H_FIXED + MIN_ROWS * ROW_H;
        float vh = h - H_FIXED, ty = Y_ROWS + vh + 6, ph = folded ? BAR + 1 : h;
        if (!moved_m && was_moved(p_root, PL.x, PL.y)) moved_m = 1;
        if (moved_m) { x = PL.x; y = PL.y; s = PL.ds; }      /* the player's place: only size and scale from here on */
        if (x != PL.x || y != PL.y || s != PL.s || ph != PL.ph || m_layout) {
            if (moved_m) resize(p_root, MW, ph); else put(p_root, x, y, MW, ph);
            set_scale(p_root, s);
            put(p_body, 0, 0, MW, h);
            put(sc_rows, PAD, Y_ROWS, RW, vh);
            put(l_tail, PAD, ty - 3, RW, 1);
            for (int k = 0; k < NSW; k++) put(t_sw[k], PAD, ty + (float)k * TOG_H, RW, 36);
            put(b_restart, PAD, ty + NSW * TOG_H + 2, RW, 32);
            PL.x = x; PL.y = y; PL.s = s; PL.ph = ph; PL.avail = avail; m_layout = 0;
        }
        make_movable(p_root, "custom-modes.dps_meter", &mov_m);
    } else if (mode == 2 && s_root) {
        float x, y, s, avail;
        region(SH, SW, &x, &y, &s, &avail);
        if (!moved_s && was_moved(s_root, PL.x, PL.y)) moved_s = 1;
        if (moved_s) { x = PL.x; y = PL.y; s = PL.ds; }
        if (x != PL.x || y != PL.y || s != PL.s) { if (!moved_s) put(s_root, x, y, SW, SH); set_scale(s_root, s); PL.x = x; PL.y = y; PL.s = s; PL.avail = avail; }
        make_movable(s_root, "custom-modes.tf_restart", &mov_s);
    }
}

/* ================================================================ frame */
static void show_mode(int m) {
    if (m == mode) return;
    act(p_root, m == 1);
    act(s_root, m == 2);
    mode = m; PL.x = PL.y = -1; m_layout = 1;
    if (m == 1 && b_restart) cmw_named(b_restart, CMW_BUTTON, "dps:restart", restart_click, NULL, TR("Restart Test Floor"));
    if (m == 2 && s_btn) cmw_named(s_btn, CMW_BUTTON, "dps:restart", restart_click, NULL, TR("Restart Test Floor"));
}
static void tick(void *ud) {
    (void)ud; fr++;
    if (!U) return;
    if (gen != dsi_generation()) { gen = dsi_generation(); if (p_root || s_root) destroy_all(); }
    if ((p_root && !cm_alive(p_root)) || (s_root && !cm_alive(s_root))) destroy_all();   /* the kit's layer went with a scene */
    if (fr % 30 == 1) title_up = cmw_title_shown();
    int dps = cur_act();
    m_track(dps);
    int want = title_up ? 0 : dps ? 1 : tf_restart_ready() ? 2 : 0;
    if (want == 1 && !p_root && build_full()) want = 0;
    if (want == 2 && !s_root && build_small()) want = 0;
    int was = mode;
    show_mode(want);
    if (!mode) return;
    if (mode != was || fr % 30 == 0) {
        band_update(mode == 1 ? p_root : s_root);
        int rr = tf_restart_ready();                      /* the switches may change in the Test Fights window too */
        if (mode == 1) {
            if (en_rst != rr) U->set_enabled(b_restart, en_rst = rr);
            if (U->toggle_get(t_first) != tf_dps_switch(TFD_SW_FIRSTHIT)) U->toggle_set(t_first, tf_dps_switch(TFD_SW_FIRSTHIT));
            for (int k = 0; k < NSW; k++) if (U->toggle_get(t_sw[k]) != tf_dps_switch(sw_of[k])) U->toggle_set(t_sw[k], tf_dps_switch(sw_of[k]));
        } else if (s_en != rr) U->set_enabled(s_btn, s_en = rr);
        if (mode == 1) refresh();
        place_now();
        last_ref = fr;
        return;
    }
    if (mode == 1 && fr - last_ref >= 10 && (M.running || m_dirty || m_layout)) { refresh(); place_now(); last_ref = fr; }
}

int tf_meter_init(void) {
    static int done;
    if (done) return 0;
    H = cmw_host(); U = cmw_ui();
    if (!H || !U) { cmw_log("tf: DPS meter: no widget kit, off"); return -1; }
    gen = dsi_generation();
    int r = cmw_tick_add(tick, NULL);
    if (r) { cmw_log("tf: DPS meter: no frame tick left (cm_win NTICK), off"); return -1; }
    done = 1;
    return 0;
}

/* ================================================================ test (the units test driver's "call") */
static size_t jesc(char *o, size_t n, const char *s) {
    size_t k = 0;
    for (; s && *s && k + 7 < n; s++) {
        unsigned char c = (unsigned char)*s;
        if (c == '"' || c == '\\') { o[k++] = '\\'; o[k++] = (char)c; }
        else if (c < 0x20) k += (size_t)snprintf(o + k, n - k, "\\u%04x", c);
        else o[k++] = (char)c;
    }
    if (n) o[k < n ? k : n - 1] = 0;
    return k;
}
#define ADD(...) do { if (o < cap) o += (size_t)snprintf(out + o, cap - o, __VA_ARGS__); } while (0)
__attribute__((visibility("default"))) int tf_meter_test(const char *s, char *out, size_t cap) {
    if (!s || !out || cap < 64) return -1;
    size_t o = 0; out[0] = 0;
    if (!strncmp(s, "counted", 7)) {
        size_t from = s[7] ? (size_t)atol(s + 8) : 0;
        ADD("{\"n\":%zu,\"from\":%zu,\"hits\":[", M.nc, from);
        size_t k = from;
        for (; k < M.nc && o + 64 < cap; k++) ADD("%s[%zu,%.9g]", k > from ? "," : "", M.cx[k], (double)M.cd[k]);
        ADD("],\"more\":%d}", k < M.nc);
        return o < cap ? 0 : -2;
    }
    il_init();
    double el = m_elapsed(), div = m_div(el);
    float r[4] = { 0 };
#ifndef _WIN32
    static int (*t_rect)(DSLObj, float *);
    if (!t_rect) *(void **)&t_rect = dlsym(RTLD_DEFAULT, "dsl_ui_test_rect");
    DSLObj shown = mode == 1 ? p_root : mode == 2 ? s_root : NULL;
    if (t_rect && shown) t_rect(shown, r);
#endif
    ADD("{\"shown\":%d,\"small\":%d,\"folded\":%d,\"running\":%d,\"armed\":%d,\"elapsed\":%.6f,\"total\":%.6f,\"dps\":%.6f,\"dps5\":%.6f,"
        "\"repaired\":%.3f,\"heal\":%.6f,\"absorbed\":%.6f,\"base\":%zu,\"scan\":%zu,\"hits\":%u,\"open\":%d,\"moved\":[%d,%d],\"restart_cut\":[%d,%d],\"spans\":[",
        mode == 1, mode == 2, folded, M.running, M.armed, el, M.total, M.hits ? M.total / div : 0, m_dps5(el), m_repaired(), M.heal, M.absorbed,
        M.nsp ? M.sp[0].a : M.scan, M.scan, M.hits, open_unit, moved_m, moved_s,
        b_restart ? cm_text_truncated(b_restart) : -1, s_btn ? cm_text_truncated(s_btn) : -1);
    for (int i = 0; i < M.nsp; i++) ADD("%s[%zu,%zu]", i ? "," : "", M.sp[i].a, M.sp[i].b == SIZE_MAX ? M.scan : M.sp[i].b);
    ADD("],\"units\":[");
    int ord[MAXU], n = sorted_units(ord, MAXU);
    for (int k = 0; k < n && o + 600 < cap; k++) {
        const Un *u = &M.u[ord[k]]; const tf_unit *t = tf_dps_unit(ord[k]); char e[200];
        jesc(e, sizeof e, t ? t->name : "");
        ADD("%s{\"i\":%d,\"name\":\"%s\",\"dmg\":%.6f,\"heal\":%.6f,\"hits\":%u,\"dps\":%.6f,\"share\":%.6f,\"skills\":[", k ? "," : "", ord[k], e, u->dmg, u->heal, u->hits,
            u->dmg / div, M.total > 0 ? u->dmg * 100 / M.total : 0);
        for (int j = 0; j < u->nsk && o + 300 < cap; j++) { jesc(e, sizeof e, u->sk[j].name);
            ADD("%s{\"skill\":\"%s\",\"dmg\":%.6f,\"heal\":%.6f,\"hits\":%u}", j ? "," : "", e, u->sk[j].dmg, u->sk[j].heal, u->sk[j].hits); }
        ADD("]}");
    }
    ADD("],\"rect\":[%.0f,%.0f,%.0f,%.0f],\"game_rects\":{", r[0], r[1], r[2], r[3]);
    if (PL.list_found) ADD("\"resources\":[%.0f,%.0f,%.0f,%.0f]", PL.rs[0], PL.rs[1], PL.rs[2], PL.rs[3]); else ADD("\"resources\":null");
    if (PL.up_found) ADD(",\"unitpanel\":[%.0f,%.0f,%.0f,%.0f]", PL.us[0], PL.us[1], PL.us[2], PL.us[3]); else ADD(",\"unitpanel\":null");
    if (PL.ins_found) ADD(",\"inspect\":[%.0f,%.0f,%.0f,%.0f]", PL.is[0], PL.is[1], PL.is[2], PL.is[3]); else ADD(",\"inspect\":null");
    ADD("},\"place\":{\"region\":%d,\"x\":%.1f,\"y\":%.1f,\"s\":%.3f,\"ui_scale\":%.3f,\"layer\":[%.0f,%.0f],\"layer_screen\":[%.0f,%.0f,%.0f,%.0f],"
        "\"list\":[%.1f,%.1f,%.1f,%.1f],\"unit_top\":%.1f,\"bottom\":%.1f,\"rows_h\":%.1f}}",
        PL.region, PL.x, PL.y, PL.s, PL.ds, PL.lw, PL.lh, PL.ls[0], PL.ls[1], PL.ls[2], PL.ls[3],
        PL.list.x0, PL.list.y0, PL.list.x1, PL.list.y1, PL.up_top, PL.bottom, rows_h);
    return o < cap ? 0 : -2;
}
