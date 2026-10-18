// cm_mp.c - Custom Modes, the part that needs multiplayer (custom-modes-mp.dylib): Foreign Expedition (fe/), PvP (pvp/)
// and the units code they share (units/). Loaded by custom-modes.dylib (cm_module.c) only when multiplayer is active; it
// links two-level to @rpath/multiplayer.dylib (the image already loaded: the loader's module or the rig's dsrig.dylib)
// and registers everything through the Multiplayer API (src/mp_api.h). Multiplayer never calls this code by name.
// Registration order = the order the code ran in when it was compiled into multiplayer: FE first, then PvP.
#include "dsl_sdk.h"
#include "mp_api.h"
#include "mpmenu_kit.h"
#include "rig.h"
#include "net/dsnet.h"
#include "fe/fe_json.h"
#define DSI_IMPLEMENTATION                               /* this image's string tables (fe/cm_mp_text.h); the solo part has its own */
#include "fe/cm_mp_text.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

static const DSLHost *H __attribute__((unused));
static DSLPlugin *ME __attribute__((unused));

#ifndef CM_SAME_IMAGE
/* hooks of this module go through the loader's engine as Custom Modes' own (the rig host maps them to rig_hook).
   CM_SAME_IMAGE (Windows: one DLL with multiplayer): multiplayer's own rig_hook is used. */
int rig_hook(uintptr_t rva, void *repl, void **orig, const char *who) {
    int r = H && H->hook_rva ? H->hook_rva(ME, rv(rva), repl, orig, who) : -1;   /* rv: rig.h, the running game version */
    if (r) rig_log("%s: hook %#lx refused (custom-modes)", who, (unsigned long)rva);
    return r;
}
#endif

/* Foreign Expedition (fe/fe_game.c, fe/fe_menu.c) */
void fe_on_start(const uint8_t *payload, size_t len);
int fe_join_point_save(const char *path, const char *save_name);
int fe_is_protected(const char *stem);
void fe_poll(void);
int fe_recruit_allowed(void);
void fe_note_loot(const uint8_t item[16], int player);
char *fe_exec(const char *cmd, const char *line);
int fe_menu_start(int death, int xp, int loot, int recruit, char *err, size_t errn);
const mp_menu_hooks *fe_menu_hooks(void);
/* PvP (pvp/pvp_game.c, pvp/pvp_ui.c, units/pvp_units.c) */
void pvp_on_units_start(const uint8_t *payload, size_t len);
int pvp_units_join_point(const char *path);
void pvp_sim_tick(void);
int pvp_freeze_now(void);
uint64_t pvp_fp_mix(void);
size_t pvp_block_size(void);
size_t pvp_block_get(uint8_t *out, size_t cap);
void pvp_block_set(const uint8_t *p, size_t n);
int pvp_save_override(const uint8_t **det, size_t *dl, const uint8_t **json, size_t *jn);
int pvp_unit_tag(const uint8_t g[16], char *out, int cap);
uint64_t pvp_tag_sig(void);
int pvp_portal_blocked(void);
void pvp_frame(void);
char *pvp_exec(const char *cmd, const char *line);
void pvp_install(void);
const mp_menu_hooks *pvp_menu_hooks(void);

static const mp_api_table *A;
/* host choices (mp_api register_host_choice2, lane mpfix): Foreign Expedition = the neutral hub (fe/fe_hub.c; the menu's
   own Expedition Rules panel, open), PvP = PvP v1 on the starting map (pvp/pvp_ui.c; its rules panel). Each page's Start
   builds + loads its world; the lobby's Start starts the expedition / the match. */
int fe_world_start(int kind, char *err, size_t errn);
static int fe_page_start(char *err, size_t errn) { return fe_world_start(0, err, errn); }
static int fe_lobby_start(char *err, size_t errn) { return fe_menu_start(M.set.fe_death, M.set.fe_xp, M.set.fe_loot, M.set.fe_recruit, err, errn); }
void pvp_v1_page_draw(void *panel);
int pvp_v1_page_start(char *err, size_t errn);
int pvp_v1_lobby_start(char *err, size_t errn);
static void fe_ui_flags(void) {                          /* hub with Recruiting Off: Guild Contact hidden, popup 8 not offered */
    int off = !fe_recruit_allowed();
    A->ui_set_flag(MPUI_HIDE_GUILD_CONTACT, off);
    A->ui_set_flag(MPUI_NO_RECRUIT_POPUP, off);
}
int pvp_v1_world(void);                                  /* fe/fe_hub.c */
int pvp_v1_keep_prefix(const char *path);
static int pvp_world_build(const char *path, const char *save_name) {
    (void)save_name;
    int v1 = pvp_v1_world(), r = pvp_units_join_point(path);
    if (r == 1 && v1) pvp_v1_keep_prefix(path);            /* the PvP v1 world stays one with the brought units in */
    return r;
}

/* ---- player text (fe/cm_mp_text.h). Host choice texts are copied by multiplayer when registered: registered again
   (same id = replaced in place) when the language changes. Labels <= 47 bytes, tips <= 299 (mp_api.h): cut at a
   character boundary, never inside one. */
static int choice_reg(const char *id, const char *label, const char *tip, void (*draw)(void *), int (*page)(char *, size_t), int (*lobby)(char *, size_t)) {
    char l[48], t[300];
    snprintf(l, sizeof l, "%s", label); dsi_utf8_fix_end(l);
    snprintf(t, sizeof t, "%s", tip); dsi_utf8_fix_end(t);
    mp_host_choice c = { id, l, t, draw, page, lobby };
    return A->register_host_choice2(&c);
}
static int choice_fe(void) {
    return choice_reg("foreign-expedition", TR("Foreign Expedition"),
        TR("Everyone brings units into a neutral hub with everything unlocked. Nobody's save changes until they bring the results home."),
        NULL, fe_page_start, fe_lobby_start);
}
static int choice_pvp(void) {
    return choice_reg("pvp", TR("PvP"),
        TR("Everyone brings units onto the starting map and the teams fight each other. A world made only for the match: nobody's save changes."),
        pvp_v1_page_draw, pvp_v1_page_start, pvp_v1_lobby_start);
}
/* lang/ next to this dylib (one folder for both Custom Modes dylibs) */
static void lang_dir(void) {
    char d[1200] = "";
#ifdef _WIN32
    HMODULE m = NULL;
    if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCSTR)(void *)lang_dir, &m)) {
        wchar_t w[600]; DWORD k = GetModuleFileNameW(m, w, 600);      /* UTF-8, as ds_i18n.h opens it (_wfopen) */
        if (!k || k >= 600 || WideCharToMultiByte(CP_UTF8, 0, w, -1, d, (int)sizeof d, NULL, NULL) <= 0) d[0] = 0;
    }
    char *sl = strrchr(d, '\\');
#else
    Dl_info di; if (dladdr((void *)lang_dir, &di) && di.dli_fname) snprintf(d, sizeof d, "%s", di.dli_fname);
    char *sl = strrchr(d, '/');
#endif
    if (!sl) { rig_log("custom-modes-mp: no folder for lang/: English only"); return; }
    snprintf(sl + 1, sizeof d - (size_t)(sl + 1 - d), "lang");
    dsi_set_dir(d);
}
/* Refactor.Setting.LanguageSetting.GetCurrentLanguage (static int32, no args), resolved by name (Mac and Windows), read
   every frame (a static read); English until the game is up. A change can only happen on the title screen. */
static void lang_poll(void) {
    static void *mi; static int64_t next;
    if (!mi) {
        if (frames < next || scene() < 1) return;
        next = frames + 120;
        void *k = class_by_name("Refactor.Setting", "LanguageSetting");
        mi = k ? F(0x6F8C1C, void *(*)(void *, const char *, int))(k, "GetCurrentLanguage", 0) : NULL;   /* il2cpp_class_get_method_from_name */
        if (!mi) return;
    }
    int v = ((int32_t (*)(const void *))(*(void **)mi))(mi);
    if (dsi_set_lang(v)) {
        rig_log("custom-modes-mp: language %d (%s), %d entries, %d dropped", v, dsi_code(), dsi_loaded(), dsi_dropped());
        choice_fe(); choice_pvp();
    }
}
unsigned cmt_gen(void) { return dsi_generation(); }
void cmt_set(void *t, const char *s) {
    if (!s) s = "";
    char b[4096]; size_t n = strlen(s);
    if (n >= sizeof b) { tmp_set(t, s); return; }
    memcpy(b, s, n + 1); dsi_utf8_fix_end(b); tmp_set(t, b);
}
const char *cmt_game(const char *key, const char *en) {
    static struct { char key[80], val[200]; } c[24]; static int nc; static unsigned gen; static char spill[200];
    if (!key || !*key) return en;
    if (gen != dsi_generation()) { gen = dsi_generation(); nc = 0; }
    for (int i = 0; i < nc; i++) if (!strcmp(c[i].key, key)) return c[i].val[0] ? c[i].val : en;
    char v[200] = "";
    Obj s = F(0x80EBAC, Obj (*)(Obj, void *))(u_str(key), NULL);            /* TextKeyExtensions.GetText (current language) */
    if (s) utf16_to_utf8(s, v, sizeof v);
    if (!strncmp(v, "TextKey not found", 17) || !strcmp(v, key)) v[0] = 0;
    char *slot = spill;
    if (nc < 24 && strlen(key) < sizeof c[0].key) { snprintf(c[nc].key, sizeof c[nc].key, "%s", key); slot = c[nc++].val; }
    snprintf(slot, 200, "%s", v);
    return slot[0] ? slot : en;
}
void cmt_level(char *out, size_t cap, int level) {     /* the unit stat's own word (TEXTKEY_STATUS_Level_NAME) */
    snprintf(out, cap, "%s %d", cmt_game("TEXTKEY_STATUS_Level_NAME", "Level"), level); dsi_utf8_fix_end(out);
}
void cmt_day(char *out, size_t cap, int day) {         /* the save list's own format "Day{0}" (TEXTKEY_SaveLoad_Date) */
    const char *f = cmt_game("TEXTKEY_SaveLoad_Date", ""), *p = strstr(f, "{0}");
    if (!p) { snprintf(out, cap, "%s %d", cmt_game("TEXTKEY_Common_Day", "Day"), day); dsi_utf8_fix_end(out); return; }
    snprintf(out, cap, "%.*s%d%s", (int)(p - f), f, day, p + 3); dsi_utf8_fix_end(out);
}
/* messages made in English by code that stays English (fe/fe_save.c: pure C, tested on its own, and one of its texts is
   compared by value in fe_game.c), translated only for display. Patterns hold at most two %s. */
static const char *const msg_pat[] = {
    TR_NOOP("a unit is not in this save's clan"),
    TR_NOOP("an item of one unit is missing from this save"),
    TR_NOOP("a unit comes from %s, the hub is %s"),
    TR_NOOP("this save is not the hub of that run"),
    TR_NOOP("these results are damaged"),
    TR_NOOP("Already brought home on %s."),
    TR_NOOP("These results are not for your units."),
    TR_NOOP("this save is not the campaign the units came from"),
    TR_NOOP("Made with %s; the expedition ran on %s."),
    TR_NOOP("These results are already in this save."),
    TR_NOOP("the new save failed its safety check, so it was not written (details in the log)"),
    TR_NOOP("could not write the new save (a save with that name may exist already)"),
    TR_NOOP("These units came from My Units or a wiki link: nothing to bring home (your saves are not changed)."),
    TR_NOOP("no results for this run"),
    TR_NOOP("the home save is gone"),
    TR_NOOP("backup failed: nothing applied"),
    TR_NOOP("The results could not be applied."),
};
static int msg_match(const char *pat, const char *m, char a[2][400]) {    /* 1 = m is pat with its %s filled (args in a) */
    const char *s1 = strstr(pat, "%s"), *s2 = s1 ? strstr(s1 + 2, "%s") : NULL;
    if (!s1) return !strcmp(pat, m);
    size_t l0 = (size_t)(s1 - pat), lm = strlen(m);
    if (strncmp(m, pat, l0)) return 0;
    const char *tail = s2 ? s2 + 2 : s1 + 2, *mid = s2 ? s1 + 2 : NULL; size_t lt = strlen(tail), lmid = s2 ? (size_t)(s2 - mid) : 0;
    if (lm < l0 + lt || strcmp(m + lm - lt, tail)) return 0;
    const char *b = m + l0, *e = m + lm - lt;                               /* the args' part */
    if (!s2) { if ((size_t)(e - b) >= 400) return 0; snprintf(a[0], 400, "%.*s", (int)(e - b), b); return 1; }
    const char *x = NULL;
    for (const char *q = b; q + lmid <= e; q++) if (!strncmp(q, mid, lmid)) { x = q; break; }
    if (!x || (size_t)(x - b) >= 400 || (size_t)(e - x - lmid) >= 400) return 0;
    snprintf(a[0], 400, "%.*s", (int)(x - b), b); snprintf(a[1], 400, "%.*s", (int)(e - x - lmid), x + lmid);
    return 1;
}
const char *cmt_msg(const char *en, char *out, size_t cap) {
    if (!en || !cap) return en ? en : "";
    char a[2][400] = { "", "" };
    for (size_t i = 0; i < sizeof msg_pat / sizeof msg_pat[0]; i++)
        if (msg_match(msg_pat[i], en, a)) { snprintf(out, cap, TR(msg_pat[i]), a[0], a[1]); dsi_utf8_fix_end(out); return out; }
    snprintf(out, cap, "%s", en); dsi_utf8_fix_end(out);
    return out;
}

__attribute__((visibility("default"))) int cm_mp_start(const DSLHost *host, DSLPlugin *self, const mp_api_table *api) {
    H = host; ME = self; A = api;
    lang_dir();
    fe_log_hook = rig_log;                                               /* fe/fe_json.h: the detail behind short save messages */
    if (!api || api->size < sizeof(mp_api_table)) { rig_log("custom-modes: multiplayer API too old (size %u): Foreign Expedition and PvP off", api ? api->size : 0); return 1; }
    int bad = 0;
    /* Foreign Expedition */
    bad |= api->register_sim_cmd(DSNET_FE_START, fe_on_start);
    bad |= api->register_world_build("Foreign Expedition hub build", fe_join_point_save);
    bad |= api->register_save_keep(fe_is_protected);
    bad |= api->register_frame(MP_FRAME_SESSION, fe_poll);
    bad |= api->register_frame(MP_FRAME_ALWAYS, fe_ui_flags);
    bad |= api->register_order_filter(fe_recruit_allowed, fe_note_loot);
    bad |= api->register_rig_cmd("fe_", fe_exec);
    bad |= choice_fe();
    bad |= api->register_menu(fe_menu_hooks());
    /* PvP */
    bad |= api->register_sim_cmd(DSNET_FE_START, pvp_on_units_start);         /* own units use the same command; each acts on its own run id */
    bad |= api->register_tick(pvp_sim_tick, pvp_freeze_now);
    bad |= api->register_fingerprint(pvp_fp_mix);
    { mp_joinpoint jp = { .tag = { 'P', 'V', 'P', 'B' }, .size = pvp_block_size, .write = pvp_block_get, .read = pvp_block_set, .override_save = pvp_save_override };
      bad |= api->register_joinpoint(&jp); }
    bad |= api->register_world_build("PvP own-units world build", pvp_world_build);
    bad |= api->register_frame(MP_FRAME_ALWAYS, pvp_frame);
    bad |= api->register_rig_cmd("pvp_", pvp_exec);
    bad |= api->register_unit_tag(pvp_unit_tag, pvp_tag_sig);
    bad |= api->register_teams_portal(pvp_portal_blocked);
    bad |= api->register_menu(pvp_menu_hooks());
    bad |= choice_pvp();
    bad |= api->register_frame(MP_FRAME_ALWAYS, lang_poll);   /* the game's language, live (title screen Options) */
    pvp_install();                                       /* hostility hooks */
    rig_log("custom-modes: Foreign Expedition + PvP registered with multiplayer (API %u)%s", api->version, bad ? " (a registration was refused)" : "");
    return bad ? 2 : 0;
}
