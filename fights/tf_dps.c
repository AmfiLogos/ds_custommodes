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
 * game for every building. The Fix live-hp (ds_modloader, "Live building HP") calls UpdateBuildingEntityUI and
 * SetBuildingStatus again on every change; the two hooks here put the DPS Check's number form (and fit) on what it draws. */
#include "tf_dps.h"
#include "tf_worlds.h"
#include "cm_win.h"
#include "cm_il.h"
#include "mp_api.h"
#include "i18n/ds_i18n.h"
#include "cm_prefs.h"
#include "tf_meter.h"
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
static unsigned nworld;                                  /* world loads seen (log lines once per world) */
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
    static void *k_das; static cm_m m_das;                 /* the game's DASText keeps its own copy (_text): its SetText */
    if (!k_das) { k_das = cm_il_class("Refactor.UI", "DASText"); m_das = cm_il_method("Refactor.UI", "DASText", "SetText", 1, NULL); }
    if (k_das && m_das.fn && (x_obj_class(t) == k_das || (x_subclass && x_subclass(x_obj_class(t), k_das, false)))) {
        CM_CALL(m_das, void, Obj, Obj, const void *)(t, cm_il_str(s), m_das.mi); return;
    }
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

/* ---------------------------------------------------------------- switches (cm_prefs) */
static const char *const SW_KEY[TFD_SW_N] = { "dps_speech", "dps_heal_numbers", "dps_chat_log", "dps_start_first_hit", "dps_effects" };
static const int SW_DEF[TFD_SW_N] = { 0, 0, 1, 1, 1 };   /* owner 2026-10-25: speech + heal numbers off, log in chat + first hit + effects on */
static int sw_val[TFD_SW_N] = { -1, -1, -1, -1, -1 };
int tf_dps_switch(int w) {
    if (w < 0 || w >= TFD_SW_N) return 0;
    if (sw_val[w] < 0) { char v[8]; sw_val[w] = cm_pref_get(SW_KEY[w], v, sizeof v) ? SW_DEF[w] : atoi(v) != 0; }
    return sw_val[w];
}
void tf_dps_switch_set(int w, int on) {
    if (w < 0 || w >= TFD_SW_N) return;
    sw_val[w] = on != 0; cm_pref_set(SW_KEY[w], on ? "1" : "0");
}
const char *tf_dps_switch_label(int w) {
    return w == TFD_SW_SPEECH ? TR("Scarecrow speech") : w == TFD_SW_HEALNUM ? TR("Scarecrow heal numbers")
         : w == TFD_SW_CHATLOG ? TR("DPS log in chat") : w == TFD_SW_FIRSTHIT ? TR("Start on first hit")
         : w == TFD_SW_EFFECTS ? TR("Effects in log") : "";
}
static void sw_click(void *ud, DSLObj w) { int k = (int)(intptr_t)ud; tf_dps_switch_set(k, U->toggle_get(w)); }
float tf_dps_setup_rows(DSLObj parent, float y, float w) {   /* the DPS Check's switches in the Test Fights setup */
    static const int SWS[4] = { TFD_SW_SPEECH, TFD_SW_HEALNUM, TFD_SW_CHATLOG, TFD_SW_EFFECTS };
    static const char *const NM[4] = { "fights:r.dps_speech", "fights:r.dps_healnum", "fights:r.dps_chatlog", "fights:r.dps_effects" };
    float y0 = y;
    for (int i = 0; i < 4; i++) {
        int k = SWS[i];
        DSLObj t = U->toggle(parent, 0, y, w, tf_dps_switch_label(k), tf_dps_switch(k), sw_click, (void *)(intptr_t)k);
        if (t) cmw_named(t, CMW_TOGGLE, NM[i], sw_click, (void *)(intptr_t)k, tf_dps_switch_label(k));
        y += 36;
    }
    return y - y0;
}

/* ---------------------------------------------------------------- the game clock: TimeHandler._currentTick * TICK_INTERVAL
   (game speed and pause do not change Time.timeScale in this game; MainFlow._timeHandler; decomp 25846389) */
static void *k_mainflow; static int32_t f_mf_th = -1, f_th_tick = -1, f_th_iv = -1; static uintptr_t h_mf;
double tf_dps_clock(void) {
    Obj mf = cm_il_hget(h_mf);
    if (!cm_alive(mf)) { mf = k_mainflow ? cm_find(k_mainflow, false) : NULL; cm_il_hset(&h_mf, mf); }
    Obj th = mf && f_mf_th >= 0 ? FLD(mf, f_mf_th, Obj) : NULL;
    if (!th || f_th_tick < 0 || f_th_iv < 0) return 0;
    return (double)FLD(th, f_th_tick, int64_t) * (double)FLD(th, f_th_iv, float);
}

/* ---------------------------------------------------------------- the events (ONE source: chat log + DPS meter) */
static tf_hit *hitv; static size_t nhit, caphit;
#define MAXU 64
static tf_unit unitv[MAXU]; static int nunit;
static double repaired;
static int restarting;                                   /* tf_restart: the next DPS world starts with a "--- restart ---" line */
static int restart_line;                                 /* frames the world must show without the title before that line */
size_t tf_dps_hits(const tf_hit **out) { if (out) *out = hitv; return nhit; }
const tf_unit *tf_dps_unit(int i) { return i >= 0 && i < nunit ? &unitv[i] : NULL; }
int tf_dps_units(void) { return nunit; }
double tf_dps_repaired(void) { return repaired; }
int tf_dps_active(void) { return dps_on && !title_up; }
static long long game_tick(void) {
    Obj mf = cm_il_hget(h_mf), th = cm_alive(mf) && f_mf_th >= 0 ? FLD(mf, f_mf_th, Obj) : NULL;
    return th && f_th_tick >= 0 ? (long long)FLD(th, f_th_tick, int64_t) : 0;
}

/* the chat lines: the source's name as a header when it changes, then ONE indented line per source and game tick, its parts
   joined by " · " (owner, ask 10: "Fireball → Scarecrow: -227 crit · Heal → Silvarion, Prax: +50") */
static char **lines; static size_t nl, capl, sent;      /* this fight's lines, oldest first; sent = given to the chat */
static unsigned long hits, n_crit, n_miss, n_ev_done; static double dmg_sum; static int last_unit = -1, nheads;
static unsigned long n_dmg_ev, n_heal_ev, n_abs_ev, n_eff_ev, n_eff_seen;   /* events by kind (test) */
static const mp_api_table *api; static int fb_use, mp_rc_last = 1000;
static int mp_ok(void) {
    return api && api->version >= 3 && api->size >= offsetof(mp_api_table, chat_local) + sizeof api->chat_local && api->chat_local;
}
#define NGRP 24
#define NPART 10
#define NTGT 6
typedef struct { int kind; char key[80], name[100]; int n, crits, misses, ntg; int tg[NTGT]; double amt[NTGT]; double over; } Part;
typedef struct { int unit; long long tick; int np; size_t ev_hi; Part p[NPART]; } Grp;
static Grp grp[NGRP]; static int ngrp;
static void finalize(int upto_all, long long now);
static void grp_lines(const Grp *g);
static int is_dummy_e(Obj e);
static void log_clear(void) {
    for (size_t i = 0; i < nl; i++) free(lines[i]);
    nl = sent = 0; hits = n_crit = n_miss = n_ev_done = 0; dmg_sum = 0; fb_use = 0; last_unit = -1; nheads = 0;
    n_dmg_ev = n_heal_ev = n_abs_ev = n_eff_ev = n_eff_seen = 0;
    nhit = 0; nunit = 0; repaired = 0; ngrp = 0;
    if (mp_ok()) api->chat_local(MOD_NAME, NULL, MP_CHATL_CLEAR);
}
static void add_line(const char *s) {
    if (nl == capl) { size_t nc = capl ? capl * 2 : 1024; char **p = realloc(lines, nc * sizeof *p); if (!p) return; lines = p; capl = nc; }
    size_t l = strlen(s); char *c = malloc(l + 1); if (!c) return;
    memcpy(c, s, l + 1); lines[nl++] = c;
}
static void esc(const char *in, char *out, size_t cap) {  /* names as typed: no rich-text tag from a name (as the chat's own) */
    size_t o = 0;
    for (; *in && o + 5 < cap; in++) { if (*in == '<') { memcpy(out + o, "<\xE2\x80\x8B", 4); o += 4; } else out[o++] = *in; }
    out[o] = 0;
}
static void cut(char *s, size_t chars) {                 /* at most `chars` characters (UTF-8), then "..." */
    size_t k = 0;
    for (char *p = s; *p; p++) if (((unsigned char)*p & 0xC0) != 0x80 && k++ == chars) { strcpy(p, "..."); return; }
}
#define C_GOLD "#D9BE8F"
#define C_SUB  "#A69085"
#define C_CRIT "#F1A33C"
#define C_HEAL "#7EC86B"
#define C_ABS  "#8FB8DE"
#define C_DMG  "#E8DCC8"
/* the skill / affecter that published it: key, and "basic" = SkillTableData.IsBasicAttackSkill of a Skill_Base publisher */
static void *k_skill; static int32_t f_sk_data = -1, f_skd_basic = -1;
static void pub_info(Obj data, int32_t f_exec, char *key, size_t n, int *basic) {
    key[0] = 0; *basic = 0;
    Obj ex = data && f_exec >= 0 ? FLD(data, f_exec, Obj) : NULL, ctx = ex && f_ex_ctx >= 0 ? FLD(ex, f_ex_ctx, Obj) : NULL,
        pub = ctx && f_ctx_pub >= 0 ? FLD(ctx, f_ctx_pub, Obj) : NULL;
    const void *vm = pub && x_vmethod && mi_pub_key ? x_vmethod(pub, mi_pub_key) : NULL;
    if (vm) cm_il_utf8(((Obj (*)(Obj, const void *))(*(void *const *)vm))(pub, vm), key, n);
    if (pub && k_skill && f_sk_data >= 0 && (x_obj_class(pub) == k_skill || (x_subclass && x_subclass(x_obj_class(pub), k_skill, false)))) {
        Obj d = FLD(pub, f_sk_data, Obj);                 /* Util.Sheet.SkillTableData */
        if (d && f_skd_basic < 0) f_skd_basic = cm_il_field(x_obj_class(d), "IsBasicAttackSkill");
        if (d && f_skd_basic >= 0) *basic = FLD(d, f_skd_basic, uint8_t) != 0;
    }
}
static void key_name(const char *key, int basic, char *out, size_t n) {   /* the game's name; "<name> (basic)" for a basic attack */
    char nm[96];
    if (key[0]) {
        char tk[180], en[96]; const char *u = strchr(key, '_');
        snprintf(tk, sizeof tk, "TEXTKEY_%s_NAME", key); snprintf(en, sizeof en, "%s", u ? u + 1 : key);   /* no text: the key's name part */
        cm_game_text(tk, en, nm, sizeof nm);
    } else cm_game_text("TEXTKEY_GAMEGUIDE_Attack_NAME", "Attack", nm, sizeof nm);
    if (basic) snprintf(out, n, TR("%s (basic)"), nm); else snprintf(out, n, "%s", nm);
}
static cm_m m_isplayer;
static int ent_of(Obj e) {                               /* the entity table: squad units, the scarecrow, others; names numbered */
    uint8_t g[16] = { 0 }; static void *k_ue; static int32_t f_ug = -1, f_bg = -1; static cm_m m_port;
    if (!k_ue) { k_ue = cm_il_class("Refactor", "UnitEntity"); f_ug = cm_il_field(k_ue, "<Guid>k__BackingField"); f_bg = cm_il_field(k_bld, "<Guid>k__BackingField");
                 m_port = cm_il_method("Refactor", "UnitEntity", "get_PortraitID", 0, NULL); }
    void *kc = e ? x_obj_class(e) : NULL;
    int is_unit = kc && k_ue && (kc == k_ue || (x_subclass && x_subclass(kc, k_ue, false)));
    int is_b = kc && k_bld && (kc == k_bld || (x_subclass && x_subclass(kc, k_bld, false)));
    if (is_unit && f_ug >= 0) memcpy(g, (char *)e + f_ug, 16); else if (is_b && f_bg >= 0) memcpy(g, (char *)e + f_bg, 16);
    for (int i = 0; i < nunit; i++) if (!memcmp(unitv[i].guid, g, 16)) return i;
    if (nunit == MAXU) return MAXU - 1;
    tf_unit *u = &unitv[nunit]; memset(u, 0, sizeof *u); memcpy(u->guid, g, 16);
    if (e && m_name.fn) cm_il_utf8(CM_CALL(m_name, Obj, Obj, const void *)(e, m_name.mi), u->name, sizeof u->name);   /* as the stock unit panel */
    if (e && (!u->name[0] || !strcmp(u->name, "NULL") || !strncmp(u->name, "TEXTKEY_", 8))) {   /* an item (a torch): the game's name of its key */
        int32_t fk = cm_il_field(kc, "<Key>k__BackingField"); char key[96] = "";
        if (fk >= 0) cm_il_utf8(FLD(e, fk, Obj), key, sizeof key);
        if (key[0]) key_name(key, 0, u->name, sizeof u->name);
    }
    if (!u->name[0]) snprintf(u->name, sizeof u->name, "?");
    int same = 1; for (int i = 0; i < nunit; i++) if (!strncmp(unitv[i].name, u->name, strlen(u->name)) && (unitv[i].name[strlen(u->name)] == 0 || unitv[i].name[strlen(u->name)] == ' ')) same++;
    if (same > 1) { size_t l = strlen(u->name); snprintf(u->name + l, sizeof u->name - l, " %d", same); }   /* "Rat", "Rat 2", ... */
    if (is_unit && m_port.fn) cm_il_utf8(CM_CALL(m_port, Obj, Obj, const void *)(e, m_port.mi), u->portrait, sizeof u->portrait);
    u->is_player = e && m_isplayer.fn && CM_CALL(m_isplayer, bool, Obj, const void *)(e, m_isplayer.mi);
    u->is_dummy = is_b && is_dummy_e(e);
    return nunit++;
}
static int involved(Obj user, Obj tgt) {                 /* a squad unit or the scarecrow takes part */
    return (tgt && is_dummy_e(tgt)) || (user && m_isplayer.fn && CM_CALL(m_isplayer, bool, Obj, const void *)(user, m_isplayer.mi))
        || (tgt && m_isplayer.fn && CM_CALL(m_isplayer, bool, Obj, const void *)(tgt, m_isplayer.mi));
}
/* one event: into the list (meter + tests) and into its source's group of this tick (chat) */
static void ev_add(int kind, Obj user, Obj tgt, const char *key, int basic, float amt, float over, int res) {
    if (nhit == caphit) { size_t nc = caphit ? caphit * 2 : 2048; tf_hit *p = realloc(hitv, nc * sizeof *p); if (!p) return; hitv = p; caphit = nc; }
    tf_hit *h = &hitv[nhit]; memset(h, 0, sizeof *h);
    h->t = tf_dps_clock(); h->tick = game_tick(); h->kind = kind; h->dmg = amt; h->over = over; h->res = res; h->basic = basic;
    h->unit = ent_of(user ? user : tgt); h->target = ent_of(tgt); h->on_dummy = unitv[h->target].is_dummy;
    snprintf(h->skill_key, sizeof h->skill_key, "%s", key);
    if (key[0]) key_name(key, basic, h->skill, sizeof h->skill);
    else snprintf(h->skill, sizeof h->skill, "%s", unitv[h->unit].name);   /* no skill / affecter behind it: the source itself (an item) */
    nhit++;
    if (kind == TFK_DMG && h->on_dummy) { hits++; dmg_sum += amt; n_crit += res == TFD_CRIT; n_miss += res == TFD_MISS; }
    n_dmg_ev += kind == TFK_DMG; n_heal_ev += kind == TFK_HEAL; n_abs_ev += kind == TFK_ABSORB; n_eff_ev += kind == TFK_EFFECT;
    if (kind == TFK_EFFECT && !tf_dps_switch(TFD_SW_EFFECTS)) return;   /* "Effects in log" off: not in the chat */
    Grp *g = NULL;
    for (int i = 0; i < ngrp && !g; i++) if (grp[i].unit == h->unit && grp[i].tick == h->tick) g = &grp[i];
    if (!g) {
        if (ngrp == NGRP) finalize(1, 0);
        g = &grp[ngrp++]; memset(g, 0, sizeof *g); g->unit = h->unit; g->tick = h->tick;
    }
    g->ev_hi = nhit;
    Part *p = NULL;
    for (int i = 0; i < g->np && !p; i++) if (g->p[i].kind == kind && !strcmp(g->p[i].key, key) && !strcmp(g->p[i].name, h->skill)) p = &g->p[i];
    if (!p) {
        if (g->np == NPART) { grp_lines(g); g->np = 0; }          /* a full line: out now, the tick goes on in a new one */
        p = &g->p[g->np++]; memset(p, 0, sizeof *p); p->kind = kind;
        snprintf(p->key, sizeof p->key, "%s", key); snprintf(p->name, sizeof p->name, "%s", h->skill);
    }
    p->n++; p->crits += res == TFD_CRIT; p->misses += res == TFD_MISS || res == TFD_IMMUNE; p->over += over;
    int t = 0; while (t < p->ntg && p->tg[t] != h->target) t++;
    if (t == p->ntg && p->ntg < NTGT) { p->tg[p->ntg] = h->target; p->amt[p->ntg] = 0; p->ntg++; }
    if (t < NTGT) p->amt[t] += amt;
}
/* a part: "<source>[ ×n] → <targets>: -X[ crit]" (heal "+X", in green), "<shield> (<owner>): absorbed X", "<effect> → <targets>" */
static size_t part_text(const Part *p, char *o, size_t cap) {
    char nm[220], e[220]; size_t k = 0;
    snprintf(nm, sizeof nm, "%s", p->name); cut(nm, 28); esc(nm, e, sizeof e);
    const char *col = p->kind == TFK_HEAL ? C_HEAL : p->kind == TFK_ABSORB ? C_ABS : p->kind == TFK_EFFECT ? C_SUB : C_DMG;
    if (p->kind == TFK_ABSORB) {
        char ow[200]; esc(unitv[p->tg[0]].name, ow, sizeof ow);
        double tot = 0; for (int i = 0; i < p->ntg; i++) tot += p->amt[i];
        return (size_t)snprintf(o, cap, "<color=%s>%s (%s): %s %.0f</color>", col, e, ow, TR("absorbed"), tot);
    }
    k += (size_t)snprintf(o + k, cap - k, "<color=%s>%s", col, e);
    if (p->n > 1 && p->kind != TFK_EFFECT) k += (size_t)snprintf(o + k, cap - k, " \xC3\x97%d", p->n);   /* ×n */
    k += (size_t)snprintf(o + k, cap - k, " \xC2\xBB ");                                                 /* » (the chat's fonts have no →) */
    int same = 1; for (int i = 1; i < p->ntg; i++) if ((long long)(p->amt[i] + 0.5) != (long long)(p->amt[0] + 0.5)) same = 0;
    for (int i = 0; i < p->ntg && k < cap; i++) {
        char tn[200]; esc(unitv[p->tg[i]].name, tn, sizeof tn);
        k += (size_t)snprintf(o + k, cap - k, "%s%s", i ? ", " : "", tn);
        if (p->kind == TFK_EFFECT) continue;
        int last = i == p->ntg - 1;
        if (!same || last) {
            if (p->kind == TFK_DMG && p->misses == p->n) k += (size_t)snprintf(o + k, cap - k, ": %s", cm_gw("TEXTKEY_VFXExecution_Miss", "Miss"));
            else k += (size_t)snprintf(o + k, cap - k, ": %s%.0f", p->kind == TFK_HEAL ? "+" : "-", p->amt[i]);
        }
    }
    if (p->kind == TFK_HEAL && p->over >= 0.5) k += (size_t)snprintf(o + k, cap - k, TR(" (%.0f over)"), p->over);
    k += (size_t)snprintf(o + k, cap - k, "</color>");
    if (p->kind == TFK_DMG && p->crits) {
        char w[48]; cm_game_text("TEXTKEY_VFXExecution_Critical", "Crit", w, sizeof w);
        if (p->n == 1) k += (size_t)snprintf(o + k, cap - k, " <color=" C_CRIT ">%s</color>", w);
        else k += (size_t)snprintf(o + k, cap - k, " <color=" C_CRIT ">(%d %s)</color>", p->crits, w);
    }
    if (p->kind == TFK_DMG && p->misses && p->misses < p->n)
        k += (size_t)snprintf(o + k, cap - k, " <color=" C_SUB ">(%d %s)</color>", p->misses, cm_gw("TEXTKEY_VFXExecution_Miss", "Miss"));
    return k < cap ? k : cap - 1;
}
#define LINE_MAX_B 380                                   /* the chat's local line holds 400 bytes */
static void grp_lines(const Grp *g) {
    char t[1200], e[200];
    if (g->unit != last_unit) {                          /* a header line when another source acts */
        esc(unitv[g->unit].name, e, sizeof e);
        snprintf(t, sizeof t, "<b><color=" C_GOLD ">%s</color></b>", e); add_line(t); nheads++;
        last_unit = g->unit;
    }
    size_t o = (size_t)snprintf(t, sizeof t, "<indent=5%%>"); int any = 0;
    for (int i = 0; i < g->np; i++) {
        char pt[700]; size_t pl = part_text(&g->p[i], pt, sizeof pt);
        if (any && o + 3 + pl > LINE_MAX_B) { add_line(t); o = (size_t)snprintf(t, sizeof t, "<indent=5%%>"); any = 0; }   /* too long: the next line */
        o += (size_t)snprintf(t + o, sizeof t - o, "%s%s", any ? " \xC2\xB7 " : "", pt); any = 1;              /* " · " */
    }
    if (any) add_line(t);
    n_ev_done = g->ev_hi > n_ev_done ? g->ev_hi : n_ev_done;
}
static void finalize(int all, long long now) {           /* the groups of finished ticks become lines, in the order they began */
    int k = 0;
    for (int i = 0; i < ngrp; i++) {
        if (all || grp[i].tick < now) grp_lines(&grp[i]);
        else grp[k++] = grp[i];
    }
    ngrp = k;
}
/* DamageExecutionLog: ResultType, _user, _target, TotalDamage (the floating number shows TotalDamage "F0"), shield before /
   after (ActualSieldDamage = absorbed) */
static int32_t f_log_shb = -1, f_log_sha = -1;
static void on_damage(Obj data, Obj log) {
    Obj tgt = FLD(log, f_log_target, Obj), user = FLD(log, f_log_user, Obj);
    if (!tgt || !involved(user, tgt)) return;
    char key[80]; int basic; pub_info(data, f_dd_exec, key, sizeof key, &basic);
    int res = FLD(log, f_log_res, int32_t);
    float sh = f_log_shb >= 0 && f_log_sha >= 0 ? FLD(log, f_log_shb, float) - FLD(log, f_log_sha, float) : 0;
    ev_add(TFK_DMG, user, tgt, key, basic, FLD(log, f_log_total, float) - (sh > 0 ? sh : 0), 0, res);   /* the absorbed part is no damage */
    if (sh > 0.0f) ev_add(TFK_ABSORB, user, tgt, "STATUS_Barrier", 0, sh, 0, TFD_NORMAL);   /* StatType Barrier (10) */
}
typedef Obj (*sim_fn)(Obj self, Obj data, float last, float cur, Obj *apply, const void *mi);
static sim_fn o_sim, o_ssim, o_hsim, o_asim;
static Obj h_sim(Obj self, Obj data, float last, float cur, Obj *apply, const void *mi) {
    Obj log = o_sim(self, data, last, cur, apply, mi);
    if (dps_on && log && data) on_damage(data, log);
    return log;
}
/* heals: HealExecutionLog TargetHealth_Before / _After (health total); the part past max health = over */
static void *k_hed; static int32_t f_hd_exec = -1, f_hl_user = -1, f_hl_tgt = -1, f_hl_b = -1, f_hl_a = -1;
static Obj h_hsim(Obj self, Obj data, float last, float cur, Obj *apply, const void *mi) {
    Obj log = o_hsim(self, data, last, cur, apply, mi);
    if (dps_on && log && data && f_hl_b >= 0 && f_hl_a >= 0) {
        Obj user = FLD(log, f_hl_user, Obj), tgt = FLD(log, f_hl_tgt, Obj);
        if (tgt && involved(user, tgt)) {
            char key[80]; int basic; pub_info(data, f_hd_exec, key, sizeof key, &basic);
            float b = FLD(log, f_hl_b, float), a = FLD(log, f_hl_a, float);   /* the log: max health, max health + what was added */
            if (a - b >= 0.5f) ev_add(TFK_HEAL, user, tgt, key, basic, a - b, 0, TFD_NORMAL);   /* a heal that added nothing is no heal */
        }
    }
    return log;
}
/* effects put on: AffecterExecutionLog AffecterKey, _user, _target; only the game's named ones (a text for the key) and only
   once the fight runs (a unit's own traits arrive with the world) */
static int32_t f_al_user = -1, f_al_tgt = -1, f_al_key = -1, f_ad_exec = -1;
static Obj h_asim(Obj self, Obj data, float last, float cur, Obj *apply, const void *mi) {
    Obj log = o_asim(self, data, last, cur, apply, mi);
    if (dps_on && !title_up && n_dmg_ev && log && data && f_al_key >= 0) {   /* once the fight runs (any damage) */
        Obj user = FLD(log, f_al_user, Obj), tgt = FLD(log, f_al_tgt, Obj);
        char key[80]; cm_il_utf8(FLD(log, f_al_key, Obj), key, sizeof key);
        char tk[160], tx[96]; snprintf(tk, sizeof tk, "TEXTKEY_%s_NAME", key);
        if (tgt && key[0] && involved(user, tgt) && !cm_gettext(tk, tx, sizeof tx)) { n_eff_seen++; ev_add(TFK_EFFECT, user, tgt, key, 0, 0, 0, FLD(log, f_log_res, int32_t)); }
    }
    return log;
}
static int is_dummy_e(Obj e) {                           /* the DPS Check's scarecrow (a BuildingEntity with its key) */
    if (!e || !k_bld) return 0;
    void *kc = x_obj_class(e);
    if (kc != k_bld && !(x_subclass && x_subclass(kc, k_bld, false))) return 0;
    char key[64]; cm_il_utf8(FLD(e, f_b_key, Obj), key, sizeof key);
    return !strcmp(key, DUMMY_KEY);
}
static int is_dummy(Obj e) { return is_dummy_e(e); }
/* its self repair: StatusSimulator.Simulate of StatusExecutionData Stat Durability (69) on it; the log's Amount is what
   was added (the game caps it at max - durability). In the chat (a heal of the scarecrow) only with its heal switch on. */
static void *k_sed; static int32_t f_sd_tgt = -1, f_sd_stat = -1, f_sl_amt = -1;
static Obj h_ssim(Obj self, Obj data, float last, float cur, Obj *apply, const void *mi) {
    Obj log = o_ssim(self, data, last, cur, apply, mi);
    if (dps_on && log && data && x_obj_class(data) == k_sed && FLD(data, f_sd_stat, int32_t) == ST_DURABILITY && is_dummy(FLD(data, f_sd_tgt, Obj))) {
        if (f_sl_amt < 0) f_sl_amt = cm_il_field(x_obj_class(log), "<Amount>k__BackingField");
        float a = f_sl_amt >= 0 ? FLD(log, f_sl_amt, float) : 0;
        if (a > 0) {
            repaired += a;
            if (tf_dps_switch(TFD_SW_HEALNUM)) { Obj d = FLD(data, f_sd_tgt, Obj); ev_add(TFK_HEAL, d, d, "AFFECTER_SelfRepair", 0, a, 0, TFD_NORMAL); }
        }
    }
    return log;
}
/* speech bubbles and heal numbers over the scarecrow: their view iterators (AffecterFX_SelfRepair.OnFXCall,
   StatusResultFX.HandleFX) end at once ("<>1__state" = -1): nothing is shown, the sim is untouched */
static unsigned long n_speech_shown, n_speech_hidden, n_heal_shown, n_heal_hidden;
static void it_end(Obj it) {
    if (!it) return;
    int32_t f = cm_il_field(x_obj_class(it), "<>1__state");
    if (f >= 0) FLD(it, f, int32_t) = -1;
}
static void *k_fxd; static int32_t f_fx_user = -1;
static Obj (*o_fxcall)(Obj self, Obj data, const void *mi);
static Obj h_fxcall(Obj self, Obj data, const void *mi) {
    Obj it = o_fxcall(self, data, mi);
    if (dps_on && it && data && x_obj_class(data) == k_fxd && is_dummy(FLD(data, f_fx_user, Obj))) {
        if (!tf_dps_switch(TFD_SW_SPEECH)) { it_end(it); n_speech_hidden++; } else n_speech_shown++;
    }
    return it;
}
static Obj (*o_statfx)(Obj self, Obj data, const void *mi);
static Obj h_statfx(Obj self, Obj data, const void *mi) {
    Obj it = o_statfx(self, data, mi);
    if (dps_on && it && data && x_obj_class(data) == k_sed && FLD(data, f_sd_stat, int32_t) == ST_DURABILITY && is_dummy(FLD(data, f_sd_tgt, Obj))) {
        if (!tf_dps_switch(TFD_SW_HEALNUM)) { it_end(it); n_heal_hidden++; } else n_heal_shown++;
    }
    return it;
}

/* ---------------------------------------------------------------- into the chat, or our own panel */
static void flush(void) {
    if (!tf_dps_switch(TFD_SW_CHATLOG)) { sent = nl; return; }   /* "DPS log in chat" off: nothing written (the meter counts on) */
    if (sent >= nl || fb_use) return;
    if (!mp_ok()) { fb_use = 1; cmw_log("tf: DPS log: no multiplayer chat (mp_api %u): our own log panel", api ? api->version : 0); return; }
    while (sent < nl) {
        int rc = api->chat_local(MOD_NAME, lines[sent], MP_CHATL_LINE | MP_CHATL_RICH);
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
    Obj t = cm_text_of(fb_txt); if (t && m_rich.fn) CM_CALL(m_rich, void, Obj, bool, const void *)(t, true, m_rich.mi);   /* our own tags; names escaped */
    cm_text_ellipsis(fb_txt);
    cm_raycast(fb_root, true);
    cmw_log("tf: DPS log panel built");
}
static void fb_tick(void) {
    int want = fb_use && nl > 0 && !title_up && tf_dps_switch(TFD_SW_CHATLOG);
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
    size_t end = nl - (size_t)fb_off, beg = end > FB_LINES ? end - FB_LINES : 0, o = 0; char t[FB_LINES * 420];
    for (size_t i = beg; i < end && o < sizeof t; i++) o += (size_t)snprintf(t + o, sizeof t - o, "%s%s", i > beg ? "\n" : "", lines[i]);
    U->set_text(fb_txt, t);
}

/* ---------------------------------------------------------------- the Inspect window (SubUI_EntityInspectionStatus):
   a building's Durability / Max Durability rows are written on open / selection change only (stock); the Fix live-hp
   writes them again on every change; in a DPS Check this hook puts the language's thousands form on them */
static uintptr_t h_ins; static float ins_c = -1, ins_m = -1; static int32_t f_ins_el = -1, f_el_amt = -1;
static void (*o_setbs)(Obj self, Obj sr, int quality, const void *mi);
static void ins_rows(Obj ui, float c, float m) {         /* _buildingElements: Dictionary<StatType, FinalStatElementUI> */
    Obj d = ui && f_ins_el >= 0 ? FLD(ui, f_ins_el, Obj) : NULL;
    if (!d) return;
    static int32_t f_ent = -1, f_cnt = -1; if (f_ent < 0) { f_ent = cm_il_field(x_obj_class(d), "_entries"); f_cnt = cm_il_field(x_obj_class(d), "_count"); }
    Obj ent = f_ent >= 0 ? FLD(d, f_ent, Obj) : NULL; int n = f_cnt >= 0 ? FLD(d, f_cnt, int32_t) : 0;
    for (int i = 0; ent && i < n; i++) {                 /* entry: hashCode +0, next +4, key +8, value +0x10; stride 0x18 */
        char *e = (char *)ent + 0x20 + (size_t)i * 0x18;
        int k = *(int32_t *)(e + 8); Obj el = *(Obj *)(e + 0x10);
        if (*(int32_t *)e < 0 || !el || (k != ST_DURABILITY && k != ST_MAX_DURABILITY)) continue;
        if (f_el_amt < 0) f_el_amt = cm_il_field(x_obj_class(el), "_txtAmount");
        char s[48]; fmt_sep(k == ST_DURABILITY ? c : m, s, sizeof s);
        if (f_el_amt >= 0) tmp_text(FLD(el, f_el_amt, Obj), s);
    }
}
/* the values: the bottom panel's StatusReader of the same selection when it has one (measured: the reader the window gets
   stayed at full durability while the building lost it), else the window's own */
static Obj ins_values(Obj sr) { Obj v = cm_il_hget(h_sr); return v ? v : sr; }
static void h_setbs(Obj self, Obj sr, int quality, const void *mi) {
    o_setbs(self, sr, quality, mi);
    if (!dps_on || !self || !sr) return;
    cm_il_hset(&h_ins, self);
    Obj v = ins_values(sr); ins_c = bstat(v, ST_DURABILITY); ins_m = bstat(v, ST_MAX_DURABILITY);
    ins_rows(self, ins_c, ins_m);
    static int said_world = -1; if (said_world != (int)nworld) { said_world = (int)nworld; cmw_log("tf: Inspect window: durability %.0f / %.0f (its own reader: %.0f)", ins_c, ins_m, bstat(sr, ST_DURABILITY)); }
}
double tf_dps_durability(double *max) {
    Obj sr = cm_il_hget(h_sr);
    if (max) *max = sr ? bstat(sr, ST_MAX_DURABILITY) : -1;
    return sr ? bstat(sr, ST_DURABILITY) : -1;
}

/* ---------------------------------------------------------------- frame */
static void tick(void *ud) {
    (void)ud;
    static unsigned fr;
    if (!dps_on) { if (fb_root && fb_on) { U->set_active(fb_root, 0); fb_on = 0; } return; }
    if (fr++ % 30 == 0) title_up = cmw_title_shown();
    if (restart_line) {                                  /* a restart passes through the title scene (the chat clears there) */
        if (cmw_title_shown()) restart_line = 30;
        else if (!--restart_line) add_line("<color=" C_SUB ">--- restart ---</color>");
    }
    if (!title_up) { finalize(0, game_tick()); flush(); }
    fb_tick();
}

static int force_any;                                    /* test: the log and meter in any wiki-made test world (a Custom fight) */
void tf_dps_world(const char *stem) {
    int on = tfw_dps_made(stem) || (force_any && tfw_wiki_made(stem)); nworld++;
    ngrp = 0;                                            /* a group left from the world before never reaches the new log */
    if (on || dps_on) cmw_log("tf: %s: DPS Check log and panel %s%s", stem, on ? "on" : "off", restarting ? " (restart)" : "");
    if (on) { log_clear(); fb_off = 0; fb_n = (size_t)-1; restart_line = restarting ? 30 : 0; restarting = 0; }   /* the line once the world shows */
                                                         /* (a restart also loads the title's background world first: not that one) */
    cm_il_hset(&h_view, NULL); cm_il_hset(&h_sr, NULL); shown_c = shown_m = -1;
    cm_il_hset(&h_ins, NULL); ins_c = ins_m = -1;
    dps_on = on;
}
void tf_dps_restarting(void) { restarting = dps_on; }

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
    k_mainflow = cm_il_class("Refactor.Main", "MainFlow"); f_mf_th = cm_il_field(k_mainflow, "_timeHandler");
    { void *k_th = cm_il_class("Refactor.Main", "TimeHandler"); f_th_tick = cm_il_field(k_th, "_currentTick"); f_th_iv = cm_il_field(k_th, "TICK_INTERVAL"); }
    k_sed = cm_il_class(RC, "StatusExecutionData"); f_sd_tgt = cm_il_field(k_sed, "<Target>k__BackingField"); f_sd_stat = cm_il_field(k_sed, "<Stat>k__BackingField");
    f_log_shb = cm_il_field(k_log, "<TargetShield_Before>k__BackingField"); f_log_sha = cm_il_field(k_log, "<TargetShield_After>k__BackingField");
    k_skill = cm_il_class("Refactor", "Skill_Base"); f_sk_data = cm_il_field(k_skill, "<Data>k__BackingField");
    f_skd_basic = cm_il_field(cm_il_class("Util.Sheet", "SkillTableData"), "IsBasicAttackSkill");
    m_isplayer = cm_il_method("Refactor", "EntityExtensions", "IsPlayerUnit", 1, NULL);
    k_hed = cm_il_class(RC, "HealExecutionData"); f_hd_exec = cm_il_field(k_hed, "<BaseExecution>k__BackingField");
    { void *k = cm_il_class(RC, "HealExecutionLog"); f_hl_user = cm_il_field(k, "_user"); f_hl_tgt = cm_il_field(k, "_target");
      f_hl_b = cm_il_field(k, "<TargetHealth_Before>k__BackingField"); f_hl_a = cm_il_field(k, "<TargetHealth_After>k__BackingField"); }
    { void *k = cm_il_class(RC, "AffecterExecutionLog"); f_al_user = cm_il_field(k, "_user"); f_al_tgt = cm_il_field(k, "_target"); f_al_key = cm_il_field(k, "<AffecterKey>k__BackingField");
      f_ad_exec = cm_il_field(cm_il_class(RC, "AffecterExecutionData"), "<BaseExecution>k__BackingField"); }
    cmw_log("tf: DPS Check: shield %d %d, skill data %d basic %d, player test %s, heal log %d %d %d %d, effect log %d %d %d",
            f_log_shb, f_log_sha, f_sk_data, f_skd_basic, m_isplayer.fn ? "ok" : "MISSING", f_hl_user, f_hl_tgt, f_hl_b, f_hl_a, f_al_user, f_al_tgt, f_al_key);
    if (f_hl_b >= 0 && f_hl_a >= 0 && f_hl_tgt >= 0) {
        int r = H->hook_method(self, "Refactor.Combat.ExecutionProcess", "HealSimulator", "Simulate", 4, (void *)h_hsim, (void **)&o_hsim);
        cmw_log("tf: DPS Check: heals in the log %s (%d)", r ? "hook refused" : "on", r);
    }
    if (f_al_key >= 0 && f_al_tgt >= 0) {
        int r = H->hook_method(self, "Refactor.Combat.ExecutionProcess", "AffecterSimulator", "Simulate", 4, (void *)h_asim, (void **)&o_asim);
        cmw_log("tf: DPS Check: effects in the log %s (%d)", r ? "hook refused" : "on", r);
    }
    k_fxd = cm_il_class(RC, "FXCallExecutionData"); f_fx_user = cm_il_field(k_fxd, "<User>k__BackingField");
    f_ins_el = cm_il_field(cm_il_class(RU, "SubUI_EntityInspectionStatus"), "_buildingElements");
    cmw_log("tf: DPS Check: clock fields %d %d %d, status %d %d, fx user %d, inspect %d", f_mf_th, f_th_tick, f_th_iv, f_sd_tgt, f_sd_stat, f_fx_user, f_ins_el);
    if (k_sed && f_sd_tgt >= 0 && f_sd_stat >= 0 && k_bld) {
        int r = H->hook_method(self, "Refactor.Combat.ExecutionProcess", "StatusSimulator", "Simulate", 4, (void *)h_ssim, (void **)&o_ssim);
        cmw_log("tf: DPS Check: self repair count %s (%d)", r ? "hook refused" : "on", r);
        r = H->hook_method(self, "Refactor.FX", "StatusResultFX", "HandleFX", 1, (void *)h_statfx, (void **)&o_statfx);
        cmw_log("tf: DPS Check: heal numbers switch %s (%d)", r ? "hook refused" : "on", r);
    }
    if (k_fxd && f_fx_user >= 0 && k_bld) {
        int r = H->hook_method(self, "Refactor.FX", "AffecterFX_SelfRepair", "OnFXCall", 1, (void *)h_fxcall, (void **)&o_fxcall);
        cmw_log("tf: DPS Check: speech switch %s (%d)", r ? "hook refused" : "on", r);
    }
    if (f_ins_el >= 0 && m_getval.fn) {
        int r = H->hook_method(self, "Refactor.UI", "SubUI_EntityInspectionStatus", "SetBuildingStatus", 2, (void *)h_setbs, (void **)&o_setbs);
        cmw_log("tf: DPS Check: the Inspect window's durability %s (%d)", r ? "hook refused" : "on", r);
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
    tf_meter_init();
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
static const char *ins_text(void) {                      /* the Inspect window's Durability row text now (test) */
    static char r[64]; r[0] = 0;
    Obj ui = cm_il_hget(h_ins), d = cm_alive(ui) && f_ins_el >= 0 ? FLD(ui, f_ins_el, Obj) : NULL;
    if (!d) return r;
    int32_t f_ent = cm_il_field(x_obj_class(d), "_entries"), f_cnt = cm_il_field(x_obj_class(d), "_count");
    Obj ent = f_ent >= 0 ? FLD(d, f_ent, Obj) : NULL; int n = f_cnt >= 0 ? FLD(d, f_cnt, int32_t) : 0;
    static cm_m gt; if (!gt.fn) gt = cm_il_method("TMPro", "TMP_Text", "get_text", 0, NULL);
    for (int i = 0; ent && i < n; i++) {
        char *e = (char *)ent + 0x20 + (size_t)i * 0x18; Obj el = *(Obj *)(e + 0x10);
        if (*(int32_t *)e < 0 || *(int32_t *)(e + 8) != ST_DURABILITY || !el || f_el_amt < 0 || !gt.fn) continue;
        cm_il_utf8(CM_CALL(gt, Obj, Obj, const void *)(FLD(el, f_el_amt, Obj), gt.mi), r, sizeof r);
    }
    for (char *q = r; *q; q++) if (*q == '"' || *q == '\\') *q = ' ';
    return r;
}
/* s: "state" | "force 0|1" (the log and meter in any wiki-made test world: a Custom fight) | "fb 0|1" (our own log panel instead of the chat) | "fb_state" | "fit <now> <max>" (the shown panel at those values, until the durability changes) | "pos" (building objects on screen)
      | "layout" (the panel's status part: every child's name, screen rect, text) */
__attribute__((visibility("default"))) int tf_dps_test(const char *s, char *out, size_t cap) {
    if (!out || cap < 64) return -1;
    if (!t_rect) *(void **)&t_rect = test_sym("dsl_ui_test_rect");
    size_t o = 0; out[0] = 0;
    Obj view = cm_il_hget(h_view), bs = cm_alive(view) && f_v_bsui >= 0 ? FLD(view, f_v_bsui, Obj) : NULL;
    if (!strncmp(s, "force ", 6)) { force_any = atoi(s + 6); snprintf(out, cap, "{\"force\":%d}", force_any); return 0; }
    if (!strncmp(s, "sw ", 3)) { int k = -1, v = 0; if (sscanf(s + 3, "%d %d", &k, &v) == 2) tf_dps_switch_set(k, v); snprintf(out, cap, "{\"sw\":%d}", tf_dps_switch(k)); return 0; }
    if (!strcmp(s, "vfx")) {                             /* speech bubbles and floating numbers on screen now */
        void *ksp = cm_il_class("Refactor.FX", "VFX_Speech"), *kdt = cm_il_class("Refactor.FX", "VFX_DataText"), *ktx = cm_il_class("TMPro", "TMP_Text");
        static cm_m gt; if (!gt.fn) gt = cm_il_method("TMPro", "TMP_Text", "get_text", 0, NULL);
        static cm_m inch; if (!inch.fn) inch = cm_il_method("UnityEngine", "Component", "GetComponentInChildren", 2, "System.Type,System.Boolean");
        int32_t n1 = 0, n2 = 0; Obj a1 = find_all(ksp, &n1), a2 = find_all(kdt, &n2); int on1 = 0;
        for (int32_t i = 0; a1 && i < n1; i++) on1 += go_on(FLD(a1, 0x20 + 8 * i, Obj), 1);
        o += (size_t)snprintf(out + o, cap - o, "{\"speech\":%d,\"texts\":[", on1);
        for (int32_t i = 0, k = 0; a2 && i < n2 && o + 100 < cap; i++) {
            Obj v = FLD(a2, 0x20 + 8 * i, Obj); if (!go_on(v, 1)) continue;
            Obj t = inch.fn && ktx ? CM_CALL(inch, Obj, Obj, Obj, bool, const void *)(v, cm_il_type_obj(ktx), false, inch.mi) : NULL;
            char x[64] = ""; if (cm_alive(t) && gt.fn) cm_il_utf8(CM_CALL(gt, Obj, Obj, const void *)(t, gt.mi), x, sizeof x);
            for (char *q = x; *q; q++) if (*q == '"' || *q == '\\' || (unsigned char)*q < 0x20) *q = ' ';
            o += (size_t)snprintf(out + o, cap - o, "%s\"%s\"", k++ ? "," : "", x);
        }
        snprintf(out + o, cap - o, "]}");
        return 0;
    }
    if (!strcmp(s, "summary")) {                         /* the bottom panel's buttons: name, text, screen rect (the Inspect toggle) */
        static void *kv; if (!kv) kv = cm_il_class("Refactor.UI", "EntitySummaryUIView");
        Obj v = kv ? cm_find(kv, false) : NULL; if (!v) return -2;
        void *k_tmp = cm_il_class("TMPro", "TMP_Text");
        static cm_m nm; if (!nm.fn) nm = cm_il_method("UnityEngine", "Object", "get_name", 0, NULL);
        static cm_m gt; if (!gt.fn) gt = cm_il_method("TMPro", "TMP_Text", "get_text", 0, NULL);
        Obj st[200]; int ns = 0, first = 1; st[ns++] = cm_tr(v);
        o += (size_t)snprintf(out + o, cap - o, "[");
        while (ns && o + 400 < cap) {
            Obj t = st[--ns];
            Obj tx = cm_comp(t, k_tmp);
            if (tx && go_on(t, 1)) {
                char n2[80] = "", x[80] = ""; if (nm.fn) cm_il_utf8(CM_CALL(nm, Obj, Obj, const void *)(t, nm.mi), n2, sizeof n2);
                if (gt.fn) cm_il_utf8(CM_CALL(gt, Obj, Obj, const void *)(tx, gt.mi), x, sizeof x);
                for (char *q = x; *q; q++) if (*q == '"' || *q == '\\' || (unsigned char)*q < 0x20) *q = ' ';
                Obj p = cm_parent(t); float r[4] = { 0 }; if (t_rect && p) t_rect(p, r);
                o += (size_t)snprintf(out + o, cap - o, "%s{\"name\":\"%s\",\"text\":\"%s\",\"r\":[%.0f,%.0f,%.0f,%.0f]}", first ? "" : ",", n2, x, r[0], r[1], r[2], r[3]);
                first = 0;
            }
            for (int i = cm_child_count(t) - 1; i >= 0 && ns < 200; i--) st[ns++] = cm_child(t, i);
        }
        snprintf(out + o, cap - o, "]");
        return 0;
    }
    if (!strcmp(s, "restart")) { snprintf(out, cap, "{\"rc\":%d}", tf_restart()); return 0; }
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
    o += (size_t)snprintf(out + o, cap - o, "{\"on\":%d,\"title\":%d,\"hits\":%lu,\"sum\":%.3f,\"crit\":%lu,\"miss\":%lu,\"lines\":%zu,\"sent\":%zu,\"mp\":%d,\"fb\":%d,\"fb_on\":%d,"
                            "\"view\":%d,\"view_on\":%d,\"bpanel_on\":%d,\"same\":%d,\"sr\":%d,\"dur\":%.3f,\"dmax\":%.3f,\"shown_c\":%.3f,",
                            dps_on, title_up, hits, dmg_sum, n_crit, n_miss, nl, sent, mp_ok(), fb_use, fb_on,
                            cm_alive(view), cm_alive(view) && go_on(view, 1), go_on(bp, 0), cm_alive(view) && f_v_guid >= 0 && !memcmp((char *)view + f_v_guid, shown_g, 16),
                            sr != NULL, sr ? bstat(sr, ST_DURABILITY) : -1, sr ? bstat(sr, ST_MAX_DURABILITY) : -1, shown_c);
    if (cm_alive(bs)) { o += text_json(out + o, cap - o, "cur", FLD(bs, f_s_cur, Obj)); o += (size_t)snprintf(out + o, cap - o, ",");
                        o += text_json(out + o, cap - o, "max", FLD(bs, f_s_max, Obj)); o += (size_t)snprintf(out + o, cap - o, ","); }
    o += (size_t)snprintf(out + o, cap - o, "\"repaired\":%.3f,\"heads\":%d,\"units\":%d,\"clock\":%.3f,\"speech_shown\":%lu,\"speech_hidden\":%lu,\"heal_shown\":%lu,\"heal_hidden\":%lu,"
                            "\"sw\":[%d,%d,%d,%d,%d],\"ins\":%d,\"ins_c\":%.3f,\"ins_txt\":\"%s\",\"restart_ready\":%d,"
                            "\"events\":%zu,\"ev_done\":%lu,\"ev\":{\"dmg\":%lu,\"heal\":%lu,\"absorb\":%lu,\"effect\":%lu,\"effect_seen\":%lu},",
                            repaired, nheads, nunit, tf_dps_clock(), n_speech_shown, n_speech_hidden, n_heal_shown, n_heal_hidden,
                            tf_dps_switch(0), tf_dps_switch(1), tf_dps_switch(2), tf_dps_switch(3), tf_dps_switch(4), cm_alive(cm_il_hget(h_ins)) && go_on(cm_il_hget(h_ins), 1), ins_c, ins_text(), tf_restart_ready(),
                            nhit, n_ev_done, n_dmg_ev, n_heal_ev, n_abs_ev, n_eff_ev, n_eff_seen);
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
    if (!strncmp(s, "chat", 4)) {                         /* "chat [from]": the chat's 2 count lines, then its local lines from `from` */
        size_t (*d)(char *, size_t) = (size_t (*)(char *, size_t))test_sym("mpmenu_chat_local_dump");
        if (!d) return -404;
        size_t big = 8u << 20; char *b = malloc(big); if (!b) return -1;
        d(b, big);
        long from = s[4] ? atol(s + 5) : 0; char *p = b; int nlines = 0;
        for (; *p && nlines < 2; p++) if (*p == '\n') nlines++;   /* past the two count lines */
        size_t head = (size_t)(p - b); if (head >= cap) head = cap - 1;
        memcpy(out, b, head); o = head;
        for (long k = 0; *p && k < from; p++) if (*p == '\n') k++;
        while (*p) { char *e = strchr(p, '\n'); size_t l = e ? (size_t)(e - p) + 1 : strlen(p); if (o + l + 1 >= cap) break; memcpy(out + o, p, l); o += l; p += l; }
        out[o] = 0; free(b);
        return (int)o;
    }
    if (!strncmp(s, "hits ", 5)) {                       /* "<index> <amount> <res> <unit> <kind> <tick> <target> <on_dummy> <basic> <key>|<name>" */
        for (size_t i = (size_t)atol(s + 5); i < nhit && o + 300 < cap; i++)
            o += (size_t)snprintf(out + o, cap - o, "%zu %.4f %d %d %d %lld %d %d %d %s|%s\n", i, (double)hitv[i].dmg, hitv[i].res, hitv[i].unit, hitv[i].kind,
                                  hitv[i].tick, hitv[i].target, hitv[i].on_dummy, hitv[i].basic, hitv[i].skill_key, hitv[i].skill);
        return (int)o;
    }
    if (!strcmp(s, "units")) {                           /* "<index> <is_player> <is_dummy> <name>" */
        for (int i = 0; i < nunit && o + 200 < cap; i++) o += (size_t)snprintf(out + o, cap - o, "%d %d %d %s\n", i, unitv[i].is_player, unitv[i].is_dummy, unitv[i].name);
        return (int)o;
    }
    size_t from = (size_t)atol(s);
    for (size_t i = from; i < nl && o + strlen(lines[i]) + 2 < cap; i++) o += (size_t)snprintf(out + o, cap - o, "%s\n", lines[i]);
    return (int)o;
}
