/* tf_dps.c - see tf_dps.h. Game names checked in the 25846389 dump (Mac and Windows script.json); every address by name,
 * every field by name (Mac and Windows offsets differ). Hook sites (one owner each; none taken by another mod):
 *   Refactor.UI.EntitySummaryUIView.UpdateBuildingEntityUI(StatusReader)  the panel's durability for the shown building
 *   Refactor.Combat.ExecutionProcess.DamageSimulator.Simulate(...)        every damage the game works out (one per hit)
 *   UnityEngine.Input.get_mouseScrollDelta                                 only without the multiplayer mod: 0 for the game
 *                                                                          while the pointer is over our own log panel
 * Decomp 25846389 (what the stock panel does): EntitySummaryUIPresenter.RefreshSummary fills a building's panel once
 * (InitBuildingUI, then UpdateBuildingEntityUI -> EntitySummaryBuildingStatusUI.SetDurabilityUI: fillAmount, and the
 * two texts as ((int)value).ToString()). Nothing else sets them: UpdateOnStatusChanged refreshes UNITS only (their
 * UpdateUnitStatusUI), so the durability of ANY building stays as it was when the panel was filled. That is the stock
 * game for every building; here only the DPS Check's panel follows its building. */
#include "tf_dps.h"
#include "tf_worlds.h"
#include "cm_win.h"
#include "cm_il.h"
#include "mp_api.h"
#include "i18n/ds_i18n.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#define DPS_MP_IMAGE "multiplayer.dll"
static void *mp_image(void) { return (void *)GetModuleHandleA(DPS_MP_IMAGE); }
static void *mp_sym(void *h, const char *n) { return h ? (void *)GetProcAddress((HMODULE)h, n) : NULL; }
static void *test_sym(const char *n) { (void)n; return NULL; }   /* the test driver is Mac only */
#else
#include <dlfcn.h>
#define DPS_MP_IMAGE "@rpath/multiplayer.dylib"
static void *mp_image(void) { return dlopen(DPS_MP_IMAGE, RTLD_NOLOAD | RTLD_LOCAL); }
static void *mp_sym(void *h, const char *n) { return h ? dlsym(h, n) : NULL; }
static void *test_sym(const char *n) { return dlsym(RTLD_DEFAULT, n); }
#endif

#define DUMMY_KEY "BUILDING_DungeonScareCrow"   /* the wiki's DPS target (pipeline/gen/pages_builder.py _DPS_TARGET_BUILDING) */
#define MOD_NAME "custom-modes"
enum { ST_DURABILITY = 69, ST_MAX_DURABILITY = 70 };                     /* Refactor.StatType */
enum { RES_NORMAL, RES_CRIT, RES_GUARD, RES_MISS, RES_IMMUNE };          /* Refactor.Combat.ExecutionDataResult */
#define FLD(o, off, T) (*(T *)((char *)(o) + (off)))
typedef struct { float x, y; } V2;
typedef struct { float x, y, z; } V3;

static const DSLHost *H; static const DSLUi *U;
static int dps_on;                                       /* the world loaded last is a DPS Check save */
static int title_up = 1;                                 /* the title menu shows (checked every 30 frames) */

/* ---------------------------------------------------------------- il2cpp by name */
static void *(*x_obj_class)(Obj); static const void *(*x_vmethod)(Obj, const void *); static bool (*x_subclass)(void *, void *, bool);
static void *k_bld;
static int32_t f_log_user = -1, f_log_target = -1, f_log_res = -1, f_log_total = -1, f_dd_exec = -1, f_ex_ctx = -1, f_ctx_pub = -1,
               f_b_key = -1, f_v_bsui = -1, f_v_bpanel = -1, f_v_guid = -1, f_s_cur = -1, f_s_max = -1;
static cm_m m_name, m_getval, m_setdur, m_settext, m_wrap, m_fs_get, m_fs_set, m_prefw, m_act_h, m_act_s, m_rich, m_inrect, m_mpos;
static const void *mi_pub_key, *mi_settext;

static bool go_on(Obj comp_or_go, int hierarchy) {       /* GameObject.activeInHierarchy / activeSelf */
    if (!cm_alive(comp_or_go)) return false;
    Obj g = x_obj_class(comp_or_go) == cm_il_class("UnityEngine", "GameObject") ? comp_or_go : cm_go(comp_or_go);
    cm_m m = hierarchy ? m_act_h : m_act_s;
    return g && m.fn && CM_CALL(m, bool, Obj, const void *)(g, m.mi);
}
static float bstat(Obj sr, int type) { return sr && m_getval.fn ? CM_CALL(m_getval, float, Obj, int, const void *)(sr, type, m_getval.mi) : -1; }
static void tmp_text(Obj t, const char *s) {             /* TMP_Text.text (virtual: the object's own override) */
    if (!cm_alive(t)) return;
    const void *vm = x_vmethod && mi_settext ? x_vmethod(t, mi_settext) : NULL;
    if (vm) ((void (*)(Obj, Obj, const void *))(*(void *const *)vm))(t, cm_il_str(s), vm);
    else if (m_settext.fn) CM_CALL(m_settext, void, Obj, Obj, const void *)(t, cm_il_str(s), m_settext.mi);
}
static float tmp_pw(Obj t) { return cm_alive(t) && m_prefw.fn ? CM_CALL(m_prefw, float, Obj, const void *)(t, m_prefw.mi) : -1; }
static float tmp_fs(Obj t) { return cm_alive(t) && m_fs_get.fn ? CM_CALL(m_fs_get, float, Obj, const void *)(t, m_fs_get.mi) : -1; }
static void tmp_fs_set(Obj t, float v) { if (cm_alive(t) && m_fs_set.fn) CM_CALL(m_fs_set, void, Obj, float, const void *)(t, v, m_fs_set.mi); }

/* 1234567 -> "1,234,567" with the language's separator (as the Test Fights card: fights/tf_ui.c fmt_n). Whole numbers as the
   stock panel shows them: (int)value, cut toward zero. */
static void fmt_sep(double v, char *o, size_t n) {
    const char *c = dsi_code(), *sep = !strcmp(c, "de") || !strcmp(c, "es") || !strcmp(c, "pt-BR") ? "."
                                      : !strcmp(c, "fr") || !strcmp(c, "ru") ? "\xC2\xA0" : ",";
    char d[40]; snprintf(d, sizeof d, "%lld", (long long)v); size_t l = strlen(d), k = 0, sl = strlen(sep);
    for (size_t i = 0; i < l && k + sl + 1 < n; i++) { if (i && d[i - 1] != '-' && (l - i) % 3 == 0) { memcpy(o + k, sep, sl); k += sl; } o[k++] = d[i]; }
    o[k] = 0;
}

/* ---------------------------------------------------------------- the inspect panel's durability */
static uintptr_t h_view, h_sr;                           /* the panel and the StatusReader of the building it shows */
static uint8_t shown_g[16]; static float shown_c = -1, shown_m = -1;
static int live = 1;                                     /* test switch: 0 = the stock panel (no follow) */
static struct { uintptr_t h; float orig; int al; } fsz[4];   /* the two texts' own font size and alignment (before ours) */
static cm_m m_align, m_align_set;
static int orig_slot(Obj t) {
    for (int i = 0; i < 4; i++) if (fsz[i].h && cm_il_hget(fsz[i].h) == t) return i;
    float s = tmp_fs(t); if (s <= 0) return -1;
    for (int i = 0; i < 4; i++) if (!fsz[i].h || !cm_alive(cm_il_hget(fsz[i].h))) {
        cm_il_hset(&fsz[i].h, t); fsz[i].orig = s; fsz[i].al = m_align.fn ? CM_CALL(m_align, int, Obj, const void *)(t, m_align.mi) : 0; return i;
    }
    return -1;
}
static float orig_size(Obj t) { int i = orig_slot(t); return i >= 0 ? fsz[i].orig : -1; }
static void tmp_align(Obj t, int al) {
    if (cm_alive(t) && m_align.fn && m_align_set.fn && CM_CALL(m_align, int, Obj, const void *)(t, m_align.mi) != al) CM_CALL(m_align_set, void, Obj, int, const void *)(t, al, m_align_set.mi);
}
/* The room a one-line text has inside its parent (the bar's "Amount" box: "<now>" "/" "<max>" side by side), in the
   text's own units: a left-aligned text may run on to the box's right edge, a right-aligned one back to its left edge
   (the text grows that way, away from the "/"); any other alignment keeps to its own rect. */
typedef struct { float x, y, w, h; } RF;
static cm_m m_tp, m_rect;
static float wx(Obj tr, float x) { return m_tp.fn ? CM_CALL(m_tp, V3, Obj, V3, const void *)(tr, (V3){ x, 0, 0 }, m_tp.mi).x : 0; }
static float room(Obj t) {
    Obj tr = cm_tr(t), pr = cm_parent(tr);
    if (!m_rect.fn || !m_tp.fn || !cm_alive(tr)) return -1;
    RF r = CM_CALL(m_rect, RF, Obj, const void *)(tr, m_rect.mi);
    int al = m_align.fn ? (CM_CALL(m_align, int, Obj, const void *)(t, m_align.mi) & 0xFF) : 0;   /* 1 left, 2 center, 4 right */
    if (!cm_alive(pr) || r.w <= 0 || (al != 1 && al != 4)) return r.w;
    RF q = CM_CALL(m_rect, RF, Obj, const void *)(pr, m_rect.mi);
    float l = wx(tr, r.x), rr = wx(tr, r.x + r.w), per = (rr - l) / r.w;   /* world units per text unit */
    if (per <= 0) return r.w;
    float edge = al == 1 ? wx(pr, q.x + q.w) - l : rr - wx(pr, q.x);
    return edge / per - 2;
}
/* one line each (no wrap). A number that fits its own box at the game's size stays as the game shows it (its alignment,
   its size). A longer one ("1,000,000" in a box made for 6 digits) grows away from the "/": "<now>" right-aligned, "<max>"
   left-aligned, into the bar's free room; when even that is too narrow, both shrink by the same factor (never under 60 %). */
static float fit_scale;
static void fit(Obj tc, Obj tm) {
    Obj t[2] = { tc, tm }; float need[2] = { 0 }, own[2] = { 0 }; int slot[2], wide = 0;
    for (int i = 0; i < 2; i++) {
        if (!cm_alive(t[i]) || (slot[i] = orig_slot(t[i])) < 0) return;
        if (m_wrap.fn) CM_CALL(m_wrap, void, Obj, int, const void *)(t[i], 0, m_wrap.mi);     /* TextWrappingModes.NoWrap */
        float o = fsz[slot[i]].orig, now = tmp_fs(t[i]), pw = tmp_pw(t[i]);
        need[i] = now > 0 && pw > 0 ? pw * o / now : 0;  /* the text's width at the game's size */
        if (cm_rect_size(cm_tr(t[i]), &own[i], NULL)) own[i] = 0;
        if (need[i] > own[i] + 0.5f) wide = 1;
    }
    float s = 1;
    for (int i = 0; i < 2; i++) {
        int al = fsz[slot[i]].al;
        tmp_align(t[i], wide ? (al & ~0xFF) | (i == 0 ? 4 : 1) : al);   /* TextAlignmentOptions: horizontal bits 4 right, 1 left */
        float rw = wide ? room(t[i]) : own[i];
        if (need[i] > rw && rw > 0 && rw / need[i] < s) s = rw / need[i];
    }
    if (s < 0.6f) s = 0.6f;
    fit_scale = s;
    for (int i = 0; i < 2; i++) { float o = fsz[slot[i]].orig; if (tmp_fs(t[i]) != o * s) tmp_fs_set(t[i], o * s); }
}
static void show(Obj view, float c, float m, int game) {
    Obj bs = f_v_bsui >= 0 ? FLD(view, f_v_bsui, Obj) : NULL;
    if (!cm_alive(bs) || f_s_cur < 0 || f_s_max < 0) return;
    if (game && m_setdur.fn) CM_CALL(m_setdur, void, Obj, float, float, const void *)(bs, c, m, m_setdur.mi);   /* bar + texts, stock */
    Obj tc = FLD(bs, f_s_cur, Obj), tm = FLD(bs, f_s_max, Obj);
    char a[48], b[48]; fmt_sep(c, a, sizeof a); fmt_sep(m, b, sizeof b);
    tmp_text(tc, a); tmp_text(tm, b);
    fit(tc, tm);
}
static void (*o_upd)(Obj view, Obj sr, const void *mi);
static void h_upd(Obj view, Obj sr, const void *mi) {
    o_upd(view, sr, mi);
    if (!dps_on || !view || !sr || f_v_guid < 0) return;
    cm_il_hset(&h_view, view); cm_il_hset(&h_sr, sr);
    memcpy(shown_g, (char *)view + f_v_guid, 16);
    shown_c = bstat(sr, ST_DURABILITY); shown_m = bstat(sr, ST_MAX_DURABILITY);
    show(view, shown_c, shown_m, 0);
}
static void live_tick(void) {
    Obj view = cm_il_hget(h_view), sr = cm_il_hget(h_sr);
    if (!live || !sr || !cm_alive(view) || !go_on(view, 1)) return;
    Obj bp = f_v_bpanel >= 0 ? FLD(view, f_v_bpanel, Obj) : NULL;
    if (!go_on(bp, 0) || memcmp((char *)view + f_v_guid, shown_g, 16)) return;   /* another kind of entity / building now */
    float c = bstat(sr, ST_DURABILITY), m = bstat(sr, ST_MAX_DURABILITY);
    if (c == shown_c && m == shown_m) return;
    shown_c = c; shown_m = m;
    show(view, c, m, 1);
}

/* ---------------------------------------------------------------- the log */
static char **lines; static size_t nl, capl, sent;      /* this fight's lines, oldest first; sent = given to the chat */
static unsigned long hits, n_crit, n_miss; static double dmg_sum;
static const mp_api_table *api; static int fb_use, mp_rc_last = 1000;
static void log_clear(void) {
    for (size_t i = 0; i < nl; i++) free(lines[i]);
    nl = sent = 0; hits = n_crit = n_miss = 0; dmg_sum = 0; fb_use = 0;
    if (api && api->version >= 3 && api->size >= offsetof(mp_api_table, chat_local) + sizeof api->chat_local && api->chat_local)
        api->chat_local(MOD_NAME, NULL, MP_CHATL_CLEAR);
}
static void add_line(const char *s) {
    if (nl == capl) { size_t nc = capl ? capl * 2 : 1024; char **p = realloc(lines, nc * sizeof *p); if (!p) return; lines = p; capl = nc; }
    size_t l = strlen(s); char *c = malloc(l + 1); if (!c) return;
    memcpy(c, s, l + 1); lines[nl++] = c;
}
static void pub_name(Obj data, char *out, size_t n) {    /* the skill / affecter that dealt it, in the game's language */
    out[0] = 0;
    Obj ex = data && f_dd_exec >= 0 ? FLD(data, f_dd_exec, Obj) : NULL, ctx = ex && f_ex_ctx >= 0 ? FLD(ex, f_ex_ctx, Obj) : NULL,
        pub = ctx && f_ctx_pub >= 0 ? FLD(ctx, f_ctx_pub, Obj) : NULL;
    const void *vm = pub && x_vmethod && mi_pub_key ? x_vmethod(pub, mi_pub_key) : NULL;
    char key[128] = ""; if (vm) cm_il_utf8(((Obj (*)(Obj, const void *))(*(void *const *)vm))(pub, vm), key, sizeof key);
    if (!key[0]) return;
    char tk[180], en[128]; const char *u = strchr(key, '_');
    snprintf(tk, sizeof tk, "TEXTKEY_%s_NAME", key); snprintf(en, sizeof en, "%s", u ? u + 1 : key);   /* no text: the key's name part */
    cm_game_text(tk, en, out, n);
}
/* DamageExecutionLog: ResultType, _user, _target, TotalDamage (the floating number shows TotalDamage "F0") */
static void on_hit(Obj data, Obj log) {
    Obj tgt = FLD(log, f_log_target, Obj);
    if (!tgt) return;
    void *kc = x_obj_class(tgt);
    if (kc != k_bld && !(x_subclass && x_subclass(kc, k_bld, false))) return;
    char key[64]; cm_il_utf8(FLD(tgt, f_b_key, Obj), key, sizeof key);
    if (strcmp(key, DUMMY_KEY)) return;
    int res = FLD(log, f_log_res, int32_t); float v = FLD(log, f_log_total, float);
    Obj user = FLD(log, f_log_user, Obj);
    char who[96] = "", what[128], word[48] = "", num[32] = "", t[400];
    if (user && m_name.fn) cm_il_utf8(CM_CALL(m_name, Obj, Obj, const void *)(user, m_name.mi), who, sizeof who);
    pub_name(data, what, sizeof what);
    if (res == RES_CRIT) cm_game_text("TEXTKEY_VFXExecution_Critical", "Crit", word, sizeof word);
    else if (res == RES_GUARD) cm_game_text("TEXTKEY_VFXExecution_Guard", "Guard", word, sizeof word);
    else if (res == RES_MISS) cm_game_text("TEXTKEY_VFXExecution_Miss", "Miss", word, sizeof word);
    else if (res == RES_IMMUNE) cm_game_text("TEXTKEY_VFXExecution_Immune", "Immune", word, sizeof word);
    if (res != RES_MISS && res != RES_IMMUNE) snprintf(num, sizeof num, "%.0f", (double)v);
    size_t o = (size_t)snprintf(t, sizeof t, "%s:", who[0] ? who : "?");
    const char *parts[3] = { what, num, word };
    for (int i = 0; i < 3 && o < sizeof t; i++) if (parts[i][0]) o += (size_t)snprintf(t + o, sizeof t - o, " %s", parts[i]);
    hits++; dmg_sum += v; n_crit += res == RES_CRIT; n_miss += res == RES_MISS;
    add_line(t);
}
typedef Obj (*sim_fn)(Obj self, Obj data, float last, float cur, Obj *apply, const void *mi);
static sim_fn o_sim;
static Obj h_sim(Obj self, Obj data, float last, float cur, Obj *apply, const void *mi) {
    Obj log = o_sim(self, data, last, cur, apply, mi);
    if (dps_on && log && data) on_hit(data, log);
    return log;
}

/* ---------------------------------------------------------------- into the chat, or our own panel */
static int mp_ok(void) {
    return api && api->version >= 3 && api->size >= offsetof(mp_api_table, chat_local) + sizeof api->chat_local && api->chat_local;
}
static void flush(void) {
    if (sent >= nl || fb_use) return;
    if (!mp_ok()) { fb_use = 1; cmw_log("tf: DPS log: no multiplayer chat (mp_api %u): our own log panel", api ? api->version : 0); return; }
    while (sent < nl) {
        int rc = api->chat_local(MOD_NAME, lines[sent], MP_CHATL_LINE);
        if (rc != MP_CHAT_SHOWN) {
            if (rc != mp_rc_last) cmw_log("tf: DPS log: the chat said %d (%s): our own log panel", rc, rc == MP_CHAT_OFF ? "Chat: Off" : "refused");
            mp_rc_last = rc; fb_use = 1; return;
        }
        sent++;
    }
}
#define FB_LINES 10
#define FB_W 470.0f
#define FB_H 252.0f
static DSLObj fb_root, fb_txt; static size_t fb_n = (size_t)-1; static long fb_off, fb_off_shown = -1; static int fb_on;
static V2 (*o_wheel)(const void *mi); static int wheel_over;
static V2 h_wheel(const void *mi) { V2 d = o_wheel(mi); return wheel_over ? (V2){ 0, 0 } : d; }
static void fb_build(void) {
    if (!U) return;
    fb_root = cmw_flat(U->panel(NULL, 0, 0, FB_W, FB_H, 0), 0x222025E6u);
    if (!fb_root) return;
    float lw = 0, lh = 0; if (cm_rect_size(cm_parent(fb_root), &lw, &lh) || lh < 300) lh = 1080;
    U->place(fb_root, 130, lh - 130 - FB_H, FB_W, FB_H);   /* bottom-left above the build bar, where the multiplayer chat sits */
    cmw_flat(U->panel(fb_root, 0, 0, FB_W, 30, 0), 0x2D2A31FFu);
    cmw_flat(U->panel(fb_root, 0, 30, FB_W, 1, 0), 0xB6835AFFu);
    U->text(fb_root, 12, 0, FB_W - 24, 30, TR("DPS Check"), 15, DSL_TXT_LEFT, 0xD9BE8FFFu);
    fb_txt = U->text(fb_root, 12, 36, FB_W - 24, FB_H - 42, "", 15, DSL_TXT_LEFT, 0xE8DCC8FFu);
    Obj t = cm_text_of(fb_txt); if (t && m_rich.fn) CM_CALL(m_rich, void, Obj, bool, const void *)(t, false, m_rich.mi);   /* names as typed */
    cm_text_ellipsis(fb_txt);
    cm_raycast(fb_root, true);
    cmw_log("tf: DPS log panel built");
}
static void fb_tick(void) {
    int want = fb_use && nl > 0 && !title_up;
    if (!want) { if (fb_root && fb_on) { U->set_active(fb_root, 0); fb_on = 0; } wheel_over = 0; return; }
    if (!fb_root) fb_build();
    if (!fb_root) return;
    if (!fb_on) { U->set_active(fb_root, 1); fb_on = 1; }
    long maxo = nl > FB_LINES ? (long)(nl - FB_LINES) : 0;
    if (fb_n != (size_t)-1 && nl > fb_n && fb_off > 0) fb_off += (long)(nl - fb_n);   /* scrolled up: the same lines stay */
    { V3 mp = m_mpos.fn ? CM_CALL(m_mpos, V3, const void *)(m_mpos.mi) : (V3){ -1, -1, 0 };
      wheel_over = m_inrect.fn && CM_CALL(m_inrect, bool, Obj, V2, Obj, const void *)(fb_root, (V2){ mp.x, mp.y }, NULL, m_inrect.mi);
      static cm_m mw; if (!o_wheel && !mw.fn) mw = cm_il_method("UnityEngine", "Input", "get_mouseScrollDelta", 0, NULL);
      V2 d = !wheel_over ? (V2){ 0, 0 } : o_wheel ? o_wheel(NULL) : mw.fn ? CM_CALL(mw, V2, const void *)(mw.mi) : (V2){ 0, 0 };
      fb_off += d.y > 0 ? 3 : d.y < 0 ? -3 : 0; }
    if (fb_off > maxo) fb_off = maxo;
    if (fb_off < 0) fb_off = 0;
    if (nl == fb_n && fb_off == fb_off_shown) return;     /* drawn once per frame at most, only when something changed */
    fb_n = nl; fb_off_shown = fb_off;
    size_t end = nl - (size_t)fb_off, beg = end > FB_LINES ? end - FB_LINES : 0, o = 0; char t[FB_LINES * 210];
    for (size_t i = beg; i < end && o < sizeof t; i++) o += (size_t)snprintf(t + o, sizeof t - o, "%s%s", i > beg ? "\n" : "", lines[i]);
    U->set_text(fb_txt, t);
}

/* ---------------------------------------------------------------- frame */
static void tick(void *ud) {
    (void)ud;
    static unsigned fr;
    if (!dps_on) { if (fb_root && fb_on) { U->set_active(fb_root, 0); fb_on = 0; } return; }
    if (fr++ % 30 == 0) title_up = cmw_title_shown();
    if (!title_up) { flush(); live_tick(); }
    fb_tick();
}

void tf_dps_world(const char *stem) {
    int on = tfw_dps_made(stem);
    if (on || dps_on) cmw_log("tf: %s: DPS Check log and panel %s", stem, on ? "on" : "off");
    if (on) { log_clear(); fb_off = 0; fb_n = (size_t)-1; }
    cm_il_hset(&h_view, NULL); cm_il_hset(&h_sr, NULL); shown_c = shown_m = -1;
    dps_on = on;
}

int tf_dps_init(const DSLHost *host, DSLPlugin *self) {
    H = host; U = cmw_ui();
    if (!cm_il_init(host, self)) { cmw_log("tf: DPS Check: il2cpp not ready, off"); return -1; }
    *(void **)&x_obj_class = H->il2cpp_fn("il2cpp_object_get_class");
    *(void **)&x_vmethod = H->il2cpp_fn("il2cpp_object_get_virtual_method");
    *(void **)&x_subclass = H->il2cpp_fn("il2cpp_class_is_subclass_of");
    const char *RC = "Refactor.Combat", *RU = "Refactor.UI", *TM = "TMPro";
    void *k_log = cm_il_class(RC, "DamageExecutionLog"), *k_dd = cm_il_class(RC, "DamageExecutionData"), *k_ex = cm_il_class(RC, "Execution"),
         *k_ctx = cm_il_class(RC, "ExecutionContext"), *k_pub = cm_il_class(RC, "IExecutionPublisher"),
         *k_view = cm_il_class(RU, "EntitySummaryUIView"), *k_bs = cm_il_class(RU, "EntitySummaryBuildingStatusUI"), *k_tmp = cm_il_class(TM, "TMP_Text");
    k_bld = cm_il_class("Refactor", "BuildingEntity");
    f_log_user = cm_il_field(k_log, "_user"); f_log_target = cm_il_field(k_log, "_target");
    f_log_res = cm_il_field(k_log, "<ResultType>k__BackingField"); f_log_total = cm_il_field(k_log, "<TotalDamage>k__BackingField");
    f_dd_exec = cm_il_field(k_dd, "<BaseExecution>k__BackingField"); f_ex_ctx = cm_il_field(k_ex, "<Context>k__BackingField");
    f_ctx_pub = cm_il_field(k_ctx, "<Publisher>k__BackingField"); f_b_key = cm_il_field(k_bld, "<Key>k__BackingField");
    f_v_bsui = cm_il_field(k_view, "_buildingStatusUI"); f_v_bpanel = cm_il_field(k_view, "_buildingPanel"); f_v_guid = cm_il_field(k_view, "_currentBuildingGuid");
    f_s_cur = cm_il_field(k_bs, "_txtCurrentDurability"); f_s_max = cm_il_field(k_bs, "_txtMaxDurability");
    mi_pub_key = k_pub ? H->find_method(k_pub, "get_Key", 0) : NULL;
    mi_settext = k_tmp ? H->find_method(k_tmp, "set_text", 1) : NULL;
    m_name = cm_il_method("Refactor", "EntityDisplayNameExtensions", "GetDisplayName", 1, "Refactor.IEntity");
    m_getval = cm_il_method("Refactor", "StatusReader", "GetValue", 1, NULL);
    m_setdur = cm_il_method(RU, "EntitySummaryBuildingStatusUI", "SetDurabilityUI", 2, NULL);
    m_settext = cm_il_method(TM, "TMP_Text", "set_text", 1, NULL);
    m_wrap = cm_il_method(TM, "TMP_Text", "set_textWrappingMode", 1, NULL);
    m_fs_get = cm_il_method(TM, "TMP_Text", "get_fontSize", 0, NULL); m_fs_set = cm_il_method(TM, "TMP_Text", "set_fontSize", 1, NULL);
    m_prefw = cm_il_method(TM, "TMP_Text", "get_preferredWidth", 0, NULL); m_rich = cm_il_method(TM, "TMP_Text", "set_richText", 1, NULL);
    m_act_h = cm_il_method("UnityEngine", "GameObject", "get_activeInHierarchy", 0, NULL);
    m_act_s = cm_il_method("UnityEngine", "GameObject", "get_activeSelf", 0, NULL);
    m_inrect = cm_il_method("UnityEngine", "RectTransformUtility", "RectangleContainsScreenPoint", 3, "UnityEngine.RectTransform,UnityEngine.Vector2,UnityEngine.Camera");
    m_mpos = cm_il_method("UnityEngine", "Input", "get_mousePosition", 0, NULL);
    m_tp = cm_il_method("UnityEngine", "Transform", "TransformPoint", 1, "UnityEngine.Vector3");
    m_rect = cm_il_method("UnityEngine", "RectTransform", "get_rect", 0, NULL); m_align = cm_il_method(TM, "TMP_Text", "get_alignment", 0, NULL);
    m_align_set = cm_il_method(TM, "TMP_Text", "set_alignment", 1, NULL);
    int fields_ok = f_log_user >= 0 && f_log_target >= 0 && f_log_res >= 0 && f_log_total >= 0 && f_b_key >= 0 && k_bld && x_obj_class;
    int panel_ok = f_v_bsui >= 0 && f_v_bpanel >= 0 && f_v_guid >= 0 && f_s_cur >= 0 && f_s_max >= 0 && m_getval.fn && m_setdur.fn && m_act_h.fn && m_act_s.fn;
    cmw_log("tf: DPS Check: log fields %s (dd %d ex %d ctx %d pub %p), panel fields %s", fields_ok ? "ok" : "MISSING", f_dd_exec, f_ex_ctx, f_ctx_pub, mi_pub_key, panel_ok ? "ok" : "MISSING");
    if (panel_ok) {
        int r = H->hook_method(self, "Refactor.UI", "EntitySummaryUIView", "UpdateBuildingEntityUI", 1, (void *)h_upd, (void **)&o_upd);
        cmw_log("tf: DPS Check: the inspect panel's durability %s (%d)", r ? "hook refused" : "on", r);
    }
    if (fields_ok) {
        int r = H->hook_method(self, "Refactor.Combat.ExecutionProcess", "DamageSimulator", "Simulate", 4, (void *)h_sim, (void **)&o_sim);
        cmw_log("tf: DPS Check: hit log %s (%d)", r ? "hook refused" : "on", r);
    }
    if (H->module_loaded && H->module_loaded("multiplayer")) {
        void *mp = mp_image(); mp_api_get_fn get = (mp_api_get_fn)mp_sym(mp, "mp_api_get");
        api = get ? get(MP_API_VERSION) : NULL;
        if (!api && get) api = get(2);                     /* an older multiplayer: our own panel */
        cmw_log("tf: DPS Check: multiplayer chat %s (mp_api %u)", mp_ok() ? "ok" : "not there", api ? api->version : 0);
    } else {                                              /* only then: the multiplayer mod owns this site when it runs */
        int r = H->hook_method(self, "UnityEngine", "Input", "get_mouseScrollDelta", 0, (void *)h_wheel, (void **)&o_wheel);
        cmw_log("tf: DPS Check: no multiplayer mod: the log panel's wheel %s (%d)", r ? "hook refused" : "on", r);
    }
    cmw_tick_add(tick, NULL);
    return 0;
}

/* ---------------------------------------------------------------- tests (the units test driver's "call") */
static Obj find_all(void *k, int32_t *n) {               /* Object.FindObjectsOfType(Type, false): Il2CppArray */
    static cm_m m; if (!m.fn) m = cm_il_method("UnityEngine", "Object", "FindObjectsOfType", 2, "System.Type,System.Boolean");
    Obj a = m.fn && k ? CM_CALL(m, Obj, Obj, bool, const void *)(cm_il_type_obj(k), false, m.mi) : NULL;
    *n = a ? (int32_t)FLD(a, 0x18, uintptr_t) : 0;
    return a;
}
static int tmp_lines(Obj t) {                            /* TMP_Text.textInfo.lineCount after a mesh update */
    static cm_m fm, ti; static int32_t f_lc = -1;
    if (!fm.fn) { fm = cm_il_method("TMPro", "TMP_Text", "ForceMeshUpdate", 2, NULL); ti = cm_il_method("TMPro", "TMP_Text", "get_textInfo", 0, NULL);
                  f_lc = cm_il_field(cm_il_class("TMPro", "TMP_TextInfo"), "lineCount"); }
    if (!cm_alive(t) || !fm.fn || !ti.fn || f_lc < 0) return -1;
    CM_CALL(fm, void, Obj, bool, bool, const void *)(t, true, false, fm.mi);
    Obj inf = CM_CALL(ti, Obj, Obj, const void *)(t, ti.mi);
    return inf ? FLD(inf, f_lc, int32_t) : -1;
}
static int (*t_rect)(DSLObj, float *);
static size_t text_json(char *o, size_t n, const char *name, Obj t) {
    char s[2400] = ""; static cm_m gt; if (!gt.fn) gt = cm_il_method("TMPro", "TMP_Text", "get_text", 0, NULL);
    if (cm_alive(t) && gt.fn) cm_il_utf8(CM_CALL(gt, Obj, Obj, const void *)(t, gt.mi), s, sizeof s);
    for (char *p = s; *p; p++) if (*p == '"' || *p == '\\') *p = '\''; else if (*p == '\n') *p = '|';
    float r[4] = { 0 }, rw = 0, rh = 0; if (t_rect && cm_alive(t)) t_rect(cm_tr(t), r); cm_rect_size(cm_tr(t), &rw, &rh);
    int al = cm_alive(t) && m_align.fn ? CM_CALL(m_align, int, Obj, const void *)(t, m_align.mi) : -1;
    typedef struct { V3 c, e; } Bounds;                  /* TMP_Text.textBounds: the drawn text, the text's own space */
    static cm_m tb; if (!tb.fn) tb = cm_il_method("TMPro", "TMP_Text", "get_textBounds", 0, NULL);
    float ink0 = 0, ink1 = 0;
    if (cm_alive(t) && tb.fn) { Bounds b = CM_CALL(tb, Bounds, Obj, const void *)(t, tb.mi); ink0 = wx(cm_tr(t), b.c.x - b.e.x); ink1 = wx(cm_tr(t), b.c.x + b.e.x); }
    return (size_t)snprintf(o, n, "\"%s\":{\"text\":\"%s\",\"lines\":%d,\"size\":%.2f,\"orig\":%.2f,\"pw\":%.1f,\"rw\":%.1f,\"rh\":%.1f,\"room\":%.1f,\"al\":%d,\"ink\":[%.4f,%.4f],\"screen\":[%.0f,%.0f,%.0f,%.0f]}",
                            name, s, tmp_lines(t), tmp_fs(t), cm_alive(t) ? orig_size(t) : 0, tmp_pw(t), rw, rh, cm_alive(t) ? room(t) : -1, al, ink0, ink1, r[0], r[1], r[2], r[3]);
}
/* s: "state" | "live 0|1" | "fb 0|1" (our own log panel instead of the chat) | "fb_state" | "fit <now> <max>" (the shown panel at those values, until the durability changes) | "pos" (building objects on screen)
      | "layout" (the panel's status part: every child's name, screen rect, text) */
__attribute__((visibility("default"))) int tf_dps_test(const char *s, char *out, size_t cap) {
    if (!out || cap < 64) return -1;
    if (!t_rect) *(void **)&t_rect = test_sym("dsl_ui_test_rect");
    size_t o = 0; out[0] = 0;
    Obj view = cm_il_hget(h_view), bs = cm_alive(view) && f_v_bsui >= 0 ? FLD(view, f_v_bsui, Obj) : NULL;
    if (!strncmp(s, "live ", 5)) { live = atoi(s + 5); snprintf(out, cap, "{\"live\":%d}", live); return 0; }
    if (!strncmp(s, "fb ", 3)) { fb_use = atoi(s + 3); snprintf(out, cap, "{\"fb\":%d}", fb_use); return 0; }   /* our own panel, as with no chat */
    if (!strcmp(s, "fb_state")) {
        Obj t = fb_txt ? cm_text_of(fb_txt) : NULL;
        o += (size_t)snprintf(out + o, cap - o, "{\"fb_on\":%d,\"off\":%ld,\"lines\":%zu,\"over\":%d,", fb_on, fb_off, nl, wheel_over);
        o += text_json(out + o, cap - o, "text", t);
        float r[4] = { 0 }; if (t_rect && fb_root) t_rect(fb_root, r);
        snprintf(out + o, cap - o, ",\"rect\":[%.0f,%.0f,%.0f,%.0f]}", r[0], r[1], r[2], r[3]);
        return 0;
    }
    if (!strncmp(s, "fit ", 4)) {
        float c = 0, m = 0; if (sscanf(s + 4, "%f %f", &c, &m) != 2 || !cm_alive(bs)) return -2;
        show(view, c, m, 1);
        o += (size_t)snprintf(out + o, cap - o, "{\"scale\":%.3f,", fit_scale);
        o += text_json(out + o, cap - o, "cur", FLD(bs, f_s_cur, Obj)); o += (size_t)snprintf(out + o, cap - o, ",");
        o += text_json(out + o, cap - o, "max", FLD(bs, f_s_max, Obj));
        { float r[4] = { 0 }; Obj p = cm_parent(cm_tr(FLD(bs, f_s_cur, Obj))); if (t_rect && p) t_rect(p, r);
          o += (size_t)snprintf(out + o, cap - o, ",\"parent\":[%.0f,%.0f,%.0f,%.0f],\"boxes\":[", r[0], r[1], r[2], r[3]);
          static cm_m nm; if (!nm.fn) nm = cm_il_method("UnityEngine", "Object", "get_name", 0, NULL);
          for (int i = -1; p && i < cm_child_count(p) && o + 200 < cap; i++) {     /* -1: the parent itself, then each child: world x of its rect */
              Obj c = i < 0 ? p : cm_child(p, i); char n2[64] = ""; if (nm.fn) cm_il_utf8(CM_CALL(nm, Obj, Obj, const void *)(c, nm.mi), n2, sizeof n2);
              RF q = m_rect.fn ? CM_CALL(m_rect, RF, Obj, const void *)(c, m_rect.mi) : (RF){ 0 };
              o += (size_t)snprintf(out + o, cap - o, "%s{\"name\":\"%s\",\"x\":[%.4f,%.4f]}", i >= 0 ? "," : "", n2, wx(c, q.x), wx(c, q.x + q.w));
          }
          o += (size_t)snprintf(out + o, cap - o, "]}"); }
        return 0;
    }
    if (!strcmp(s, "layout")) {
        if (!cm_alive(bs)) return -2;
        void *k_tmp = cm_il_class("TMPro", "TMP_Text");
        Obj st[64]; int dep[64], ns = 0; st[ns] = cm_tr(bs); dep[ns++] = 0;
        o += (size_t)snprintf(out + o, cap - o, "[");
        static cm_m nm; if (!nm.fn) nm = cm_il_method("UnityEngine", "Object", "get_name", 0, NULL);
        int first = 1;
        while (ns && o + 600 < cap) {
            Obj t = st[--ns]; int d = dep[ns];
            char name[80] = ""; if (nm.fn) cm_il_utf8(CM_CALL(nm, Obj, Obj, const void *)(t, nm.mi), name, sizeof name);
            float r[4] = { 0 }; if (t_rect) t_rect(t, r);
            Obj tx = cm_comp(t, k_tmp);
            o += (size_t)snprintf(out + o, cap - o, "%s{\"d\":%d,\"name\":\"%s\",\"on\":%d,\"r\":[%.0f,%.0f,%.0f,%.0f]", first ? "" : ",", d, name, go_on(t, 1), r[0], r[1], r[2], r[3]);
            first = 0;
            if (tx) { o += (size_t)snprintf(out + o, cap - o, ","); o += text_json(out + o, cap - o, "tmp", tx); }
            o += (size_t)snprintf(out + o, cap - o, "}");
            for (int i = cm_child_count(t) - 1; i >= 0 && ns < 64 && d < 6; i--) { st[ns] = cm_child(t, i); dep[ns++] = d + 1; }
        }
        snprintf(out + o, cap - o, "]");
        return 0;
    }
    if (!strcmp(s, "pos")) {                             /* BuildingObject / UnitObject: name, PoolKey, screen point of its transform */
        static cm_m cmain, w2s, pos, nm;
        if (!cmain.fn) { cmain = cm_il_method("UnityEngine", "Camera", "get_main", 0, NULL); w2s = cm_il_method("UnityEngine", "Camera", "WorldToScreenPoint", 1, "UnityEngine.Vector3");
                         pos = cm_il_method("UnityEngine", "Transform", "get_position", 0, NULL); nm = cm_il_method("UnityEngine", "Object", "get_name", 0, NULL); }
        Obj cam = cmain.fn ? CM_CALL(cmain, Obj, const void *)(cmain.mi) : NULL;
        o += (size_t)snprintf(out + o, cap - o, "{\"b\":[");
        const char *kn[2] = { "BuildingObject", "UnitObject" }; int k = 0;
        for (int c = 0; c < 2; c++) {
            void *kb = cm_il_class("Refactor.View", kn[c]); int32_t n = 0, f_pk = cm_il_field(kb, "<PoolKey>k__BackingField"); Obj a = find_all(kb, &n);
            for (int32_t i = 0; a && i < n && o + 300 < cap; i++) {
                Obj b = FLD(a, 0x20 + 8 * i, Obj); if (!cm_alive(b)) continue;
                char pk[96] = "", gn[96] = ""; if (f_pk >= 0) cm_il_utf8(FLD(b, f_pk, Obj), pk, sizeof pk);
                Obj go = cm_go(b); if (go && nm.fn) cm_il_utf8(CM_CALL(nm, Obj, Obj, const void *)(go, nm.mi), gn, sizeof gn);
                for (char *q = pk; *q; q++) if (*q == '"' || *q == '\\') *q = '\''; for (char *q = gn; *q; q++) if (*q == '"' || *q == '\\') *q = '\'';
                V3 w = pos.fn ? CM_CALL(pos, V3, Obj, const void *)(cm_tr(b), pos.mi) : (V3){ 0 }, sp = { 0 };
                if (cam && w2s.fn) sp = CM_CALL(w2s, V3, Obj, V3, const void *)(cam, w, w2s.mi);
                o += (size_t)snprintf(out + o, cap - o, "%s{\"u\":%d,\"pk\":\"%s\",\"go\":\"%s\",\"on\":%d,\"w\":[%.2f,%.2f],\"s\":[%.1f,%.1f]}", k++ ? "," : "", c, pk, gn, go_on(b, 1), w.x, w.y, sp.x, sp.y);
            }
        }
        snprintf(out + o, cap - o, "]}");
        return 0;
    }
    /* state */
    Obj sr = cm_il_hget(h_sr), bp = cm_alive(view) && f_v_bpanel >= 0 ? FLD(view, f_v_bpanel, Obj) : NULL;
    o += (size_t)snprintf(out + o, cap - o, "{\"on\":%d,\"title\":%d,\"live\":%d,\"hits\":%lu,\"sum\":%.3f,\"crit\":%lu,\"miss\":%lu,\"lines\":%zu,\"sent\":%zu,\"mp\":%d,\"fb\":%d,\"fb_on\":%d,"
                            "\"view\":%d,\"view_on\":%d,\"bpanel_on\":%d,\"same\":%d,\"sr\":%d,\"dur\":%.3f,\"dmax\":%.3f,\"shown_c\":%.3f,",
                            dps_on, title_up, live, hits, dmg_sum, n_crit, n_miss, nl, sent, mp_ok(), fb_use, fb_on,
                            cm_alive(view), cm_alive(view) && go_on(view, 1), go_on(bp, 0), cm_alive(view) && f_v_guid >= 0 && !memcmp((char *)view + f_v_guid, shown_g, 16),
                            sr != NULL, sr ? bstat(sr, ST_DURABILITY) : -1, sr ? bstat(sr, ST_MAX_DURABILITY) : -1, shown_c);
    if (cm_alive(bs)) { o += text_json(out + o, cap - o, "cur", FLD(bs, f_s_cur, Obj)); o += (size_t)snprintf(out + o, cap - o, ",");
                        o += text_json(out + o, cap - o, "max", FLD(bs, f_s_max, Obj)); o += (size_t)snprintf(out + o, cap - o, ","); }
    o += (size_t)snprintf(out + o, cap - o, "\"last\":[");
    for (size_t i = nl > 8 ? nl - 8 : 0; i < nl && o + 300 < cap; i++) {
        char e[260]; size_t k = 0; for (const char *p = lines[i]; *p && k + 2 < sizeof e; p++) { if (*p == '"' || *p == '\\') e[k++] = '\\'; e[k++] = *p; } e[k] = 0;
        o += (size_t)snprintf(out + o, cap - o, "%s\"%s\"", i > (nl > 8 ? nl - 8 : 0) ? "," : "", e);
    }
    snprintf(out + o, cap - o, "]}");
    return 0;
}
/* the whole log (one line each) and the multiplayer chat's local lines (mpmenu_chat_local_dump), for the no-line-lost check */
__attribute__((visibility("default"))) int tf_dps_test_log(const char *s, char *out, size_t cap) {
    if (!out || !cap) return -1;
    size_t o = 0; out[0] = 0;
    if (!strcmp(s, "chat")) {
        size_t (*d)(char *, size_t) = (size_t (*)(char *, size_t))test_sym("mpmenu_chat_local_dump");
        return d ? (int)d(out, cap) : -404;
    }
    size_t from = (size_t)atol(s);
    for (size_t i = from; i < nl && o + strlen(lines[i]) + 2 < cap; i++) o += (size_t)snprintf(out + o, cap - o, "%s\n", lines[i]);
    return (int)o;
}
