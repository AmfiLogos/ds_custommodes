/* tf_ui.c - the Test Fights window (tf.h). Texts are the wiki panel's own (builder.js renderSaveGen), so the game and
 * the wiki say the same thing. The fight choices, defaults and limits come from sg_panel (the wiki's data), never typed
 * here. */
#include "tf.h"
#include "cm_win.h"
#include "cm_work.h"
#include "tf_worlds.h"
#include "sg_js.h"
#include "sg_cache.h"
#include "myunits.h"
#include "cm_home.h"
#include "../fe/fe_json.h"
#include "cm_il.h"
#include "mu_ui.h"
#include "cm_i18n.h"
#include "cm_prefs.h"
#include "cm_gtip.h"
#include "tf_card.h"
#include <stdint.h>
#include "i18n/ds_i18n.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define WIN_W 1040.0f
#define WIN_H 720.0f
#define BODY_H (WIN_H - CMW_BAND - 1)
#define PANEL_H (BODY_H - 110 - foot_x)     /* the two lists: shorter while a long status message needs more lines */
#define LX 24.0f
#define LW 360.0f
#define RX 408.0f
#define RW 608.0f
#define GOLD 0xD9BE8FFFu
#define SUB 0xA69085FFu
#define WARN 0xEA9F3DFFu
#define MAIN 0xE8DCC8FFu
#define SCRIPTS_MAX_AGE (6 * 3600)          /* the wiki's scripts are asked again after 6 h (offline = the cached copy) */

/* ------------------------------------------------------------------ the panel (sg_panel JSON) */
#define MAXL 128
typedef struct { char id[24], n[96], key[80], tn[96]; int abyss; } Boss;      /* tn: the name shown (game text of key) */
#define KIT_IT 10
typedef struct { char id[24], n[96]; int slots; double w; int on; int nit; char ik[KIT_IT][80], in_[KIT_IT][96], itn[KIT_IT][96]; int iq[KIT_IT]; } Kit;   /* items: key, wiki name, game name, count */
typedef struct { char k[80], n[96], tn[96]; } Mon;
static struct {
    int ok; char tag[32], site[32];
    int max_squad;
    char modes[6][16]; int nmodes;
    Boss bosses[8]; int nbosses;
    char combat[8][24], mgmt[8][24], mgmt_for[8][24], label[8][32]; int ncombat, nmgmt;
    Kit kits[8]; int nkits; char insc_n[96]; char miss[8][160]; int nmiss;
    Mon *mons; int nmons;
    int msat_keys[12], nmsat, msat_def;
    char dummy_n[96]; double dummy_dur;
    char def_mode[16], def_boss[24], def_combat[24];
    char dummy_k[80], insc_k[80], dummy_tn[96], insc_tn[96], tlabel[8][64];   /* game names (names_refresh) */
    char regen_k[80], regen_n[96], regen_tn[96]; double regen_ps;            /* the dummy's self repair (wiki dummy.regen) */
    unsigned names_gen;                                                     /* dsi_generation() + 1 they are for */
} P;

static void unesc(const char *s, char *o, size_t n) {           /* raw JSON string text -> UTF-8 */
    size_t k = 0;
    for (; s && *s && k + 4 < n; s++) {
        if (*s != '\\') { o[k++] = *s; continue; }
        s++;
        if (*s == 'u' && isxdigit((unsigned char)s[1]) && isxdigit((unsigned char)s[2]) && isxdigit((unsigned char)s[3]) && isxdigit((unsigned char)s[4])) {
            char hx[5] = { s[1], s[2], s[3], s[4], 0 }; unsigned c = (unsigned)strtoul(hx, NULL, 16); s += 4;
            if (c < 0x80) o[k++] = (char)c;
            else if (c < 0x800) { o[k++] = (char)(0xC0 | c >> 6); o[k++] = (char)(0x80 | (c & 63)); }
            else { o[k++] = (char)(0xE0 | c >> 12); o[k++] = (char)(0x80 | (c >> 6 & 63)); o[k++] = (char)(0x80 | (c & 63)); }
        } else if (*s == 'n') o[k++] = '\n';
        else if (*s == 't') o[k++] = ' ';
        else if (*s) o[k++] = *s;
        else break;
    }
    o[k] = 0;
    dsi_utf8_fix_end(o);                                          /* the cap may fall inside a character */
}
static void jtxt(const jv *v, char *o, size_t n) { o[0] = 0; if (v && v->t == JN_STR) { char *r = jstrdup_raw(v); unesc(r, o, n); free(r); } }
static void jesc(const char *s, char *o, size_t n) {           /* UTF-8 -> JSON string body */
    size_t k = 0;
    for (; s && *s && k + 7 < n; s++) {
        unsigned char c = (unsigned char)*s;
        if (c == '"' || c == '\\') { o[k++] = '\\'; o[k++] = (char)c; }
        else if (c < 0x20) k += (size_t)snprintf(o + k, n - k, "\\u%04x", c);
        else o[k++] = (char)c;
    }
    o[k] = 0;
}
static int panel_parse(const char *json) {
    char e[200]; jv *r = jparse(json, strlen(json), e, sizeof e);
    if (!r) { cmw_log("tf: panel JSON: %s", e); return -1; }
    free(P.mons); memset(&P, 0, sizeof P);
    jtxt(jget(r, "tag"), P.tag, sizeof P.tag); jtxt(jget(r, "siteVer"), P.site, sizeof P.site);
    P.max_squad = (int)jnumv(jget(r, "maxSquad")); if (P.max_squad <= 0) P.max_squad = 4;
    jv *a = jget(r, "modes");
    for (size_t i = 0; a && i < a->cnt && P.nmodes < 6; i++) jtxt(a->v[i], P.modes[P.nmodes++], 16);
    a = jget(r, "bosses");
    for (size_t i = 0; a && i < a->cnt && P.nbosses < 8; i++) {
        Boss *b = &P.bosses[P.nbosses++];
        jtxt(jget(a->v[i], "id"), b->id, sizeof b->id); jtxt(jget(a->v[i], "n"), b->n, sizeof b->n); b->abyss = jtruthy(jget(a->v[i], "abyss"));
        jtxt(jget(a->v[i], "key"), b->key, sizeof b->key);
    }
    jv *labels = jget(r, "diffLabels"), *mf = jget(r, "mgmtFor");
    a = jget(r, "combatTiers");
    for (size_t i = 0; a && i < a->cnt && P.ncombat < 8; i++) {
        jtxt(a->v[i], P.combat[P.ncombat], 24);
        jtxt(jget(labels, P.combat[P.ncombat]), P.label[P.ncombat], 32);
        jtxt(jget(mf, P.combat[P.ncombat]), P.mgmt_for[P.ncombat], 24);
        if (!P.label[P.ncombat][0]) snprintf(P.label[P.ncombat], 32, "%s", P.combat[P.ncombat]);
        P.ncombat++;
    }
    a = jget(r, "mgmtTiers");
    for (size_t i = 0; a && i < a->cnt && P.nmgmt < 8; i++) jtxt(a->v[i], P.mgmt[P.nmgmt++], 24);
    jv *dk = jget(jget(r, "defaults"), "kits");
    a = jget(r, "kits");
    for (size_t i = 0; a && i < a->cnt && P.nkits < 8; i++) {
        Kit *k = &P.kits[P.nkits++];
        jtxt(jget(a->v[i], "id"), k->id, sizeof k->id); jtxt(jget(a->v[i], "n"), k->n, sizeof k->n);
        k->slots = (int)jnumv(jget(a->v[i], "slots")); k->w = jnumv(jget(a->v[i], "w"));
        k->on = jtruthy(jget(dk, k->id));
        jv *it = jget(a->v[i], "items");                              /* the wiki shows the first 4 item icons before the name */
        for (size_t j = 0; it && j < it->cnt && k->nit < KIT_IT; j++) {
            jtxt(jget(it->v[j], "k"), k->ik[k->nit], sizeof k->ik[0]); jtxt(jget(it->v[j], "n"), k->in_[k->nit], sizeof k->in_[0]);
            k->iq[k->nit] = (int)jnumv(jget(it->v[j], "qty"));
            if (k->ik[k->nit][0]) k->nit++;
        }
    }
    jtxt(jget(jget(r, "kitInsc"), "n"), P.insc_n, sizeof P.insc_n); jtxt(jget(jget(r, "kitInsc"), "k"), P.insc_k, sizeof P.insc_k);
    a = jget(r, "kitMiss");
    for (size_t i = 0; a && i < a->cnt && P.nmiss < 8; i++) {
        char k[80], why[80]; jtxt(jget(a->v[i], "k"), k, sizeof k); jtxt(jget(a->v[i], "why"), why, sizeof why);
        snprintf(P.miss[P.nmiss++], 160, TR("Left out of the kits: %s (%s)."), k, why); dsi_utf8_fix_end(P.miss[P.nmiss - 1]);
    }
    a = jget(r, "enemies");
    if (a && a->cnt) {
        P.mons = calloc(a->cnt, sizeof *P.mons);
        for (size_t i = 0; P.mons && i < a->cnt; i++) { Mon *m = &P.mons[P.nmons++]; jtxt(jget(a->v[i], "k"), m->k, sizeof m->k); jtxt(jget(a->v[i], "n"), m->n, sizeof m->n); }
    }
    jv *ms = jget(r, "msat"); a = jget(ms, "keys");
    for (size_t i = 0; a && i < a->cnt && P.nmsat < 12; i++) P.msat_keys[P.nmsat++] = (int)jnumv(a->v[i]);
    P.msat_def = (int)jnumv(jget(ms, "def"));
    jv *d = jget(r, "dummy");
    if (d && d->t == JN_OBJ) { jtxt(jget(d, "n"), P.dummy_n, sizeof P.dummy_n); jtxt(jget(d, "k"), P.dummy_k, sizeof P.dummy_k); P.dummy_dur = jnumv(jget(d, "dur"));
                                    jv *rg = jget(d, "regen"); jtxt(jget(rg, "k"), P.regen_k, sizeof P.regen_k); jtxt(jget(rg, "n"), P.regen_n, sizeof P.regen_n); P.regen_ps = jnumv(jget(rg, "perSec")); }
    jv *df = jget(r, "defaults");
    jtxt(jget(df, "mode"), P.def_mode, sizeof P.def_mode); jtxt(jget(df, "boss"), P.def_boss, sizeof P.def_boss);
    jtxt(jget(df, "combat"), P.def_combat, sizeof P.def_combat);
    jfree(r);
    P.ok = P.nmodes > 0 && P.ncombat > 0;
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
    gname(P.regen_k, P.regen_n, P.regen_tn, sizeof P.regen_tn);
    gname(P.insc_k, P.insc_n, P.insc_tn, sizeof P.insc_tn);
    for (int i = 0; i < P.nkits; i++) for (int j = 0; j < P.kits[i].nit; j++) gname(P.kits[i].ik[j], P.kits[i].in_[j], P.kits[i].itn[j], sizeof P.kits[i].itn[j]);
    for (int i = 0; i < P.ncombat; i++) {                            /* TEXTKEY_CreateCampaign_<tier>Difficulty (tags removed) */
        char k[96]; snprintf(k, sizeof k, "TEXTKEY_CreateCampaign_%sDifficulty", P.combat[i]);
        cm_game_text(k, P.label[i], P.tlabel[i], sizeof P.tlabel[i]);
    }
}

/* the wiki's battle kit titles (builder data.js kits[].n, English only): keys of the shared i18n table, shown with TR(n).
   A title not listed here shows in English. The kit items use the game's own item names (names_refresh). */
static const char *const kit_titles[] __attribute__((used)) = { TR_NOOP("Aid Kit"), TR_NOOP("Basic Camping Kit"), TR_NOOP("Basic Supply Kit"), TR_NOOP("Potion Buff Kit") };

/* ------------------------------------------------------------------ choices (the wiki panel's `st`) */
#define MAXMON 16
static struct {
    int mode, boss, combat, mgmt, mgmt_own, msat, kit[8], kit_insc;
    struct { char k[80]; int n; } mon[MAXMON]; int nmon; char monq[80]; int mon_count;
    int src;                                    /* 0 My Units, 1 Gallery */
    char ids[8][17]; int nids; char team[96];   /* My Units picks; team = the picked team's name ("" = single units) */
    int team_i;                                 /* the picked team's place in My Units, -1 = single units */
    char *gal_builds; char gal_title[160]; char gal_names[8][96]; int gal_n;
    int seeded;
} st;
static int idx_of(char (*list)[24], int n, const char *v) { for (int i = 0; i < n; i++) if (!strcmp(list[i], v)) return i; return 0; }
static void seed_choices(void) {
    if (st.seeded) return;
    st.seeded = 1;
    st.mode = 0; for (int i = 0; i < P.nmodes; i++) if (!strcmp(P.modes[i], P.def_mode)) st.mode = i;
    st.boss = 0; for (int i = 0; i < P.nbosses; i++) if (!strcmp(P.bosses[i].id, P.def_boss)) st.boss = i;
    st.combat = idx_of(P.combat, P.ncombat, P.def_combat[0] ? P.def_combat : "Normal");
    st.mgmt_own = 0;
    st.msat = 0; for (int i = 0; i < P.nmsat; i++) if (P.msat_keys[i] == P.msat_def) st.msat = i;
    for (int i = 0; i < P.nkits; i++) st.kit[i] = P.kits[i].on;
    st.kit_insc = 1; st.mon_count = 3; st.team_i = -1;
    /* the difficulty chosen last time (owner v0.1.11 #3: "choosing Hard stays Hard"), by tier id: a tier this game data
       no longer has falls back to the wiki's default */
    char v[96];
    if (!cm_pref_get("fights.combat", v, sizeof v)) for (int i = 0; i < P.ncombat; i++) if (!strcmp(P.combat[i], v)) st.combat = i;
    if (!cm_pref_get("fights.mgmt_own", v, sizeof v)) st.mgmt_own = !strcmp(v, "1");
    if (!cm_pref_get("fights.mgmt", v, sizeof v)) for (int i = 0; i < P.nmgmt; i++) if (!strcmp(P.mgmt[i], v)) st.mgmt = i;
    if (st.mgmt_own && st.mgmt >= P.nmgmt) st.mgmt_own = 0;
}
static void diff_save(void) {
    if (st.combat < P.ncombat) cm_pref_set("fights.combat", P.combat[st.combat]);
    cm_pref_set("fights.mgmt_own", st.mgmt_own ? "1" : "0");
    if (st.mgmt_own && st.mgmt < P.nmgmt) cm_pref_set("fights.mgmt", P.mgmt[st.mgmt]);
}
static int mgmt_index(void) {
    if (st.mgmt_own) return st.mgmt;
    const char *m = P.mgmt_for[st.combat]; int i = idx_of(P.mgmt, P.nmgmt, m);
    return i;
}
static const char *mode_id(void) { return st.mode < P.nmodes ? P.modes[st.mode] : "dps"; }
static int is_fight(void) { return !strcmp(mode_id(), "boss") || !strcmp(mode_id(), "arena"); }

/* ------------------------------------------------------------------ the engine on the worker thread */
static sg_engine *E; static sgc C; static int engine_state;       /* 0 none, 1 opening, 2 ready, 3 failed */
static char eng_msg[600], status[1200]; static int status_warn, busy;
static int eng_mism;                                               /* the failure is the game build: Try again cannot help */
static float foot_x;                                               /* the status box's height over its 70 (paint_status) */
static char eng_why[600];                                           /* the engine error behind eng_msg: log only */
typedef struct { cm_job j; char *in, *out, *file; size_t file_n; char err[600]; int problems; char status[200]; } Job;

/* worker thread: the engine, opened once (scripts from the cache, refreshed after SCRIPTS_MAX_AGE) */
static int eng_ensure(char *err, size_t errn, char *st_out, size_t sn) {
    if (E) { if (st_out) snprintf(st_out, sn, "running"); return 0; }
    eng_why[0] = 0;
    char root[1100], d[1024];
    if (cmw_game_data(d, sizeof d)) { snprintf(err, errn, "%s", TR("The game's data folder is not known.")); return -1; }
    snprintf(root, sizeof root, "%s/Mods/custom-modes/wiki", d);
    sgc_init(&C, root);
    sg_sources src; char e[400] = "", s2[200] = "";
    if (sgc_scripts(&C, SCRIPTS_MAX_AGE, &src, s2, sizeof s2, e, sizeof e)) {
        snprintf(err, errn, TR("Test Fights needs the wiki's files once (dungeonsettlers.wiki), and they could not be downloaded: %s"), e);
        return -1;
    }
    E = sg_open(&src, sgc_template, &C, NULL, e, sizeof e);
    sgc_sources_free(&src);
    if (!E) { snprintf(err, errn, "%s", TR("Test Fights could not start the wiki's fight maker. Try again later.")); snprintf(eng_why, sizeof eng_why, "%s", e); return -1; }
    if (st_out) snprintf(st_out, sn, "%s", s2);
    return 0;
}
sg_engine *tf_engine_for_gallery(char *err, size_t errn) { return eng_ensure(err, errn, NULL, 0) ? NULL : E; }
static void job_open_run(cm_job *j) {
    Job *b = (Job *)j; char e[400] = "";
    if (eng_ensure(b->err, sizeof b->err, b->status, sizeof b->status)) return;
    b->out = sg_panel(E, e, sizeof e);
    if (!b->out) { snprintf(b->err, sizeof b->err, "%s", TR("Test Fights could not start the wiki's fight maker. Try again later.")); snprintf(eng_why, sizeof eng_why, "%s", e); }
}
static void paint_all(void);
static void job_open_done(cm_job *j) {
    Job *b = (Job *)j;
    if (b->out && !panel_parse(b->out)) {
        engine_state = 2; eng_msg[0] = 0; eng_mism = 0;
        const char *gb = cmw_host()->game_build;
        if (gb && P.tag[0] && strcmp(gb, P.tag)) {
            snprintf(eng_msg, sizeof eng_msg, TR("The wiki's Test Fights files are for game build %s, and this game is build %s. Test Fights opens again when both match."), P.tag, gb);
            engine_state = 3; eng_mism = 1;
        }
        seed_choices();
        cmw_log("tf: engine ready (%s), wiki data %s, %d modes", b->status, P.tag, P.nmodes);
    } else {
        engine_state = 3; eng_mism = 0;
        snprintf(eng_msg, sizeof eng_msg, "%s", b->err[0] ? b->err : TR("The wiki's save generator gave no answer."));
        cmw_log("tf: engine failed: %s%s%s", eng_msg, eng_why[0] ? " | " : "", eng_why); eng_why[0] = 0;
    }
    free(b->in); free(b->out); free(b);
    paint_all();
}
static void engine_open(void) {
    if (engine_state == 1 || engine_state == 2) return;
    engine_state = 1;
    Job *b = calloc(1, sizeof *b); b->j.run = job_open_run; b->j.done = job_open_done;
    cm_work_post(&b->j);
}

/* ------------------------------------------------------------------ Start */
static void job_gen_run(cm_job *j) {
    Job *b = (Job *)j; char e[600] = "";
    if (!E) { snprintf(b->err, sizeof b->err, "%s", TR("The save generator is not running.")); return; }
    sg_result r; memset(&r, 0, sizeof r);
    if (sg_generate(E, b->in, &r, e, sizeof e)) { snprintf(b->err, sizeof b->err, "%s", e); return; }
    b->problems = r.problems;
    if (r.problems) {                                                 /* the wiki would not hand the file out: neither do we */
        char e2[200]; jv *m = jparse(r.meta, strlen(r.meta), e2, sizeof e2);
        jv *pr = m ? jget(m, "problems") : NULL;
        size_t k = (size_t)snprintf(b->err, sizeof b->err, TRN("%d problem found - the test world was NOT made.", "%d problems found - the test world was NOT made.", r.problems), r.problems);
        for (size_t i = 0; pr && i < pr->cnt && i < 4 && k < sizeof b->err; i++) {
            char t[300]; jtxt(pr->v[i], t, sizeof t); k += (size_t)snprintf(b->err + k, sizeof b->err - k, " %s", t);
        }
        jfree(m);
    } else { b->file = r.file; b->file_n = r.file_n; r.file = NULL; }
    sg_result_free(&r);
}
static void job_gen_done(cm_job *j) {
    Job *b = (Job *)j; char name[300] = "", e[300] = "";
    busy = 0;
    if (!b->file) { snprintf(status, sizeof status, "%s", b->err[0] ? b->err : TR("The save generator gave no answer.")); status_warn = 1; }
    else if (tfw_write(b->file, b->file_n, name, sizeof name, e, sizeof e)) { snprintf(status, sizeof status, "%s", e); status_warn = 1; }
    else if (tfw_loading(), cmw_load_campaign(name)) { snprintf(status, sizeof status, "%s", TR("The game did not take the load. Try again from the title screen.")); status_warn = 1; }
    else {
        snprintf(status, sizeof status, "%s", TR("Loading...")); status_warn = 0;
        free(b->in); free(b->file); free(b);
        cmw_close("gallery"); cmw_close("fights");                    /* the game's loading screen takes over */
        return;
    }
    cmw_log("tf: Start failed: %s", status);
    free(b->in); free(b->file); free(b);
    paint_all();
}
static jv *lib;                                                      /* My Units library (mu_load) while the window is open */
static char *builds_json(char *err, size_t errn) {
    if (st.src == 1) {
        if (!st.gal_builds) { snprintf(err, errn, "%s", TR("Pick a build in the Wiki Build Gallery first.")); return NULL; }
        return strdup(st.gal_builds);
    }
    if (!st.nids) { snprintf(err, errn, "%s", TR("Pick at least one unit.")); return NULL; }
    size_t cap = 1024, k = 0; char *o = malloc(cap); o[k++] = '[';
    for (int i = 0; i < st.nids && i < P.max_squad; i++) {
        const jv *u = lib ? mu_find(lib, st.ids[i]) : NULL; jv *b = u ? jget(u, "build") : NULL;
        size_t n = 0; char *t = b ? jdump(b, 0, &n) : NULL;
        if (!t) { free(o); snprintf(err, errn, "%s", TR("A picked unit is no longer in My Units.")); return NULL; }
        if (k + n + 4 > cap) { cap = (k + n + 4) * 2; o = realloc(o, cap); }
        if (i) o[k++] = ',';
        memcpy(o + k, t, n); k += n; free(t);
    }
    o[k++] = ']'; o[k] = 0;
    return o;
}
static void start_click(void *ud, DSLObj w) {
    (void)ud; (void)w;
    if (busy || engine_state != 2) return;
    char e[300] = "";
    char *builds = builds_json(e, sizeof e);
    if (!builds) { snprintf(status, sizeof status, "%s", e); status_warn = 1; paint_all(); return; }
    if (!strcmp(mode_id(), "arena") && !st.nmon) { free(builds); snprintf(status, sizeof status, "%s", TR("Add at least one monster to the room.")); status_warn = 1; paint_all(); return; }
    size_t cap = strlen(builds) + 4096; char *o = malloc(cap); size_t k = 0;
    char team[300]; jesc(st.src == 1 ? st.gal_title : st.team, team, sizeof team);
    k += (size_t)snprintf(o + k, cap - k, "{\"mode\":\"%s\",\"boss\":\"%s\",\"combat\":\"%s\",\"mgmt\":\"%s\",\"kitInsc\":%s,\"team\":\"%s\",\"kits\":[",
                          mode_id(), P.nbosses ? P.bosses[st.boss].id : "", P.combat[st.combat], P.mgmt[mgmt_index()], st.kit_insc ? "true" : "false", team);
    int first = 1;
    for (int i = 0; i < P.nkits; i++) if (st.kit[i]) { k += (size_t)snprintf(o + k, cap - k, "%s\"%s\"", first ? "" : ",", P.kits[i].id); first = 0; }
    k += (size_t)snprintf(o + k, cap - k, "],\"monsters\":[");
    for (int i = 0; i < st.nmon; i++) k += (size_t)snprintf(o + k, cap - k, "%s{\"k\":\"%s\",\"n\":%d}", i ? "," : "", st.mon[i].k, st.mon[i].n);
    k += (size_t)snprintf(o + k, cap - k, "]");
    if (is_fight() && P.nbosses && P.bosses[st.boss].abyss && P.nmsat) k += (size_t)snprintf(o + k, cap - k, ",\"msat\":%d", P.msat_keys[st.msat]);
    k += (size_t)snprintf(o + k, cap - k, ",\"builds\":%s}", builds);
    free(builds);
    Job *b = calloc(1, sizeof *b); b->j.run = job_gen_run; b->j.done = job_gen_done; b->in = o;
    busy = 1; snprintf(status, sizeof status, "%s", TR("Building your save file...")); status_warn = 0;
    cmw_log("tf: Start %s", mode_id());
    cm_work_post(&b->j);
    paint_all();
}

/* ------------------------------------------------------------------ drawing */
static const DSLUi *U;
static DSLObj body, left, right_sc, right, w_status, w_start, w_retry, w_div;
static float lines_h(const char *s, float w, float size) {           /* height of a wrapped text (rough: 0.5 em per char) */
    float per = w / (size * 0.5f); int n = 1; float c = 0;
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) { if ((*p & 0xC0) == 0x80) continue; if (*p == '\n' || ++c > per) { n++; c = 0; } }
    return n * size * 1.3f + 4;
}
/* the text's real height at width w (TMP GetPreferredValues: CJK lines and long words wrap as the game draws them), so
   the box grows to its text and nothing below is drawn over it; 0 = not known */
static float pref_h(DSLObj t, float w) {
    static cm_m m; static int tried;
    if (!tried) { tried = 1; m = cm_il_method("TMPro", "TMP_Text", "GetPreferredValues", 2, "System.Single,System.Single"); }
    Obj tm = t ? cm_text_of(t) : NULL;
    if (!m.fn || !tm) return 0;
    typedef struct { float x, y; } V2f;
    V2f v = CM_CALL(m, V2f, Obj, float, float, const void *)(tm, w, 100000.f, m.mi);
    return v.y > 0 && v.y < 5000 ? v.y : 0;
}
static float txt(DSLObj parent, const char *name, float x, float y, float w, const char *s, float size, uint32_t col) {
    float h = lines_h(s, w, size);
    DSLObj t = U->text(parent, x, y, w, h, s, size, DSL_TXT_LEFT | DSL_TXT_WRAP, col);
    float ph = pref_h(t, w);
    if (ph > 0) { h = ph + 4; U->place(t, x, y, w, h); }
    if (name) cmw_named(t, CMW_TEXT, name, NULL, NULL, s);
    return h;
}
static float head(DSLObj parent, float x, float y, float w, const char *s) {
    U->text(parent, x, y, w, 31, s, 19, DSL_TXT_LEFT | DSL_TXT_TITLE, GOLD);   /* a one-line box >= 1.6 x the size */
    return 32;
}

/* left: the squad */
static uintptr_t h_keep[2];
static void src_click(void *ud, DSLObj w) { (void)w; st.src = (int)(intptr_t)ud; cm_il_hset(&h_keep[0], NULL); paint_all(); }
static int picked_at(const char *id) { for (int k = 0; k < st.nids; k++) if (!strcmp(st.ids[k], id)) return k; return -1; }
static void unpick(int at) { memmove(st.ids[at], st.ids[at + 1], (size_t)(st.nids - at - 1) * 17); st.nids--; }
static void squad_full_warn(void) { snprintf(status, sizeof status, TRN("An expedition holds %d unit: untick one first.", "An expedition holds %d units: untick one first.", P.max_squad), P.max_squad); status_warn = 1; }
/* the save keeps a team's name only when the squad is exactly that team (owner v0.1.12: units are picked one by one) */
static void team_sync(void) {
    st.team[0] = 0; st.team_i = -1;
    size_t nt = lib ? mu_team_count(lib) : 0;
    for (size_t i = 0; i < nt && st.nids; i++) {
        char ids[MU_TEAM_MAX][17], name[96]; int n = mu_team_at(lib, i, name, sizeof name, ids), all = n == st.nids;
        for (int k = 0; k < n && all; k++) all = picked_at(ids[k]) >= 0;
        if (all) { snprintf(st.team, sizeof st.team, "%s", name); st.team_i = (int)i; return; }
    }
}
/* the team's tick: select all / none of its units (owner v0.1.12 #1: no more "the whole team at once" by default) */
static void team_click(void *ud, DSLObj w) {
    (void)w; size_t i = (size_t)(intptr_t)ud; char ids[MU_TEAM_MAX][17]; char name[96];
    int n = lib ? mu_team_at(lib, i, name, sizeof name, ids) : 0, in = 0;
    for (int k = 0; k < n; k++) in += picked_at(ids[k]) >= 0;
    status[0] = 0; status_warn = 0;
    if (n && in == n) { for (int k = 0; k < n; k++) { int at = picked_at(ids[k]); if (at >= 0) unpick(at); } }
    else for (int k = 0; k < n; k++) {
        if (picked_at(ids[k]) >= 0) continue;
        if (st.nids >= P.max_squad) { squad_full_warn(); break; }
        snprintf(st.ids[st.nids++], 17, "%s", ids[k]);
    }
    team_sync(); paint_all();
}
static void unit_toggle(void *ud, DSLObj w) {
    size_t i = (size_t)(intptr_t)ud; mu_info in; if (!lib || mu_info_at(lib, i, &in)) return;
    int at = picked_at(in.id);
    status[0] = 0; status_warn = 0;
    if (at >= 0) unpick(at);
    else if (st.nids < P.max_squad) snprintf(st.ids[st.nids++], 17, "%s", in.id);
    else { U->toggle_set(w, 0); squad_full_warn(); }
    team_sync(); paint_all();
}
static void gallery_open_click(void *ud, DSLObj w);
/* a My Units unit by id: its info and its place in the library. 0 ok */
static int unit_info_by_id(const char *id, mu_info *out, size_t *at) {
    size_t n = lib ? mu_count(lib) : 0;
    for (size_t i = 0; i < n; i++) if (!mu_info_at(lib, i, out) && !strcmp(out->id, id)) { if (at) *at = i; return 0; }
    return -1;
}
/* the builder's name of a team saved from the Wiki Build Gallery (library.json teams[i].author, lane cmunits). 1 = has one */
static int team_author(size_t i, char *out, size_t n) {
    jv *t = jget(lib, "teams"); out[0] = 0;
    if (!t || t->t != JN_ARR || i >= t->cnt) return 0;
    jtxt(jget(t->v[i], "author"), out, n);
    return out[0] != 0;
}
/* ------------------------------------------------------------------ scroll thumbs: the kit's scroll lists have no bar, so a
   long list showed no sign of more below (owner v0.1.11: "the battle kit list is cut at the bottom"). A thin gold bar on
   the right edge shows where the list is and that it goes on; it follows the wheel every frame. */
#define NTHUMB 2
static struct { uintptr_t h_ct, h_bar; float x, y, vh, ch; } th[NTHUMB];
static float thumb_h(int i) { float bh = th[i].vh * th[i].vh / th[i].ch; return bh < 24 ? 24 : bh; }
static void thumb_add(int k, DSLObj parent, DSLObj content, float x, float y, float vh, float ch) {   /* k: 0 squad list, 1 fight list */
    cm_il_hset(&th[k].h_ct, NULL); cm_il_hset(&th[k].h_bar, NULL);
    if (ch <= vh + 1) return;
    DSLObj track = cmw_flat(U->panel(parent, x, y, 4, vh, 0), 0x3A3540FFu);
    if (track) cm_raycast(track, false);
    th[k].x = x; th[k].y = y; th[k].vh = vh; th[k].ch = ch;
    DSLObj bar = cmw_flat(U->panel(parent, x, y, 4, thumb_h(k), 0), GOLD);
    if (bar) cm_raycast(bar, false);
    cm_il_hset(&th[k].h_ct, content); cm_il_hset(&th[k].h_bar, bar);
}
static void thumb_tick(void *ud) {
    (void)ud;
    static cm_m pos; static void *k_sr; static int tried;
    if (!tried) { tried = 1; k_sr = cm_il_class("UnityEngine.UI", "ScrollRect"); pos = cm_il_method("UnityEngine.UI", "ScrollRect", "get_verticalNormalizedPosition", 0, NULL); }
    if (!pos.fn || !k_sr) return;
    for (int i = 0; i < NTHUMB; i++) {
        Obj ct = cm_il_hget(th[i].h_ct), bar = cm_il_hget(th[i].h_bar);
        if (!cm_alive(ct) || !cm_alive(bar)) continue;
        Obj sr = cm_comp(cm_parent(ct), k_sr); if (!sr) continue;
        float np = CM_CALL(pos, float, Obj, const void *)(sr, pos.mi);
        if (np < 0) np = 0;
        if (np > 1) np = 1;
        float bh = thumb_h(i);
        U->place(bar, th[i].x, th[i].y + (1 - np) * (th[i].vh - bh), 4, bh);
    }
}
/* every click draws the lists again: they keep their place (it was back at the top after each tick). The content's
   offset (anchored position, top-left) is read before the old list goes and given to the new one, within its length. */
typedef struct { float x, y; } V2k;
static cm_m m_apos_get, m_apos_set;
static float keep_get(int k) {
    static int tried;
    if (!tried) { tried = 1; m_apos_get = cm_il_method("UnityEngine", "RectTransform", "get_anchoredPosition", 0, NULL); m_apos_set = cm_il_method("UnityEngine", "RectTransform", "set_anchoredPosition", 1, NULL); }
    Obj c = cm_il_hget(h_keep[k]);
    if (!cm_alive(c) || !m_apos_get.fn) return 0;
    V2k p = CM_CALL(m_apos_get, V2k, Obj, const void *)(c, m_apos_get.mi);
    return p.y > 0 ? p.y : 0;
}
static void keep_set(int k, DSLObj content, float y, float max) {   /* max: content height - view height */
    cm_il_hset(&h_keep[k], content);
    if (y > max) y = max;
    if (y > 0 && content && m_apos_set.fn) CM_CALL(m_apos_set, void, Obj, V2k, const void *)(content, (V2k){ 0, y }, m_apos_set.mi);
}
/* a one-line text cut with "..." at its own size so it fits w (the kit would shrink it instead: the team row's name was
   12.5 pt next to 16 pt unit rows) */
static void ellipsis_fit(DSLObj t, const char *s, float w) {
    if (!t || cm_text_preferred_w(t) <= w) return;
    char b[200]; int n = (int)strlen(s), lo = 0, hi = n;                    /* the longest prefix (whole UTF-8 chars) that fits */
    while (lo < hi) {
        int m = (lo + hi + 1) / 2; while (m > 0 && ((unsigned char)s[m] & 0xC0) == 0x80) m--;
        if (m <= lo) break;
        snprintf(b, sizeof b, "%.*s...", m, s); U->set_text(t, b);
        if (cm_text_preferred_w(t) <= w) lo = m; else hi = m - 1;
    }
    while (lo > 0 && s[lo - 1] == ' ') lo--;
    snprintf(b, sizeof b, "%.*s...", lo, s); U->set_text(t, b);
}
static void paint_left(void) {
    cmw_forget("fights:l.");
    float ky = keep_get(0); cm_il_hset(&h_keep[0], NULL);
    if (left) U->destroy(left);
    left = cmw_flat(U->panel(body, LX, 16, LW, PANEL_H, 0), 0x00000000);
    float y = head(left, 0, 0, LW, TR("Squad"));
    const char *src_l[2] = { TR("My Units"), TR("Wiki Build Gallery") };
    DSLObj sb[2]; float sx[2] = { 0, LW / 2 + 4 }, sw[2] = { LW / 2 - 4, LW / 2 - 4 }, need[2];
    for (int s = 0; s < 2; s++) {
        char nm[40]; snprintf(nm, sizeof nm, "fights:l.src%d", s);
        sb[s] = cmw_named(U->button(left, sx[s], y, sw[s], 38, src_l[s], src_click, (void *)(intptr_t)s), CMW_BUTTON, nm, src_click, (void *)(intptr_t)s, src_l[s]);
        float pw = sb[s] ? cm_text_preferred_w(sb[s]) : -1; need[s] = pw > 0 ? pw + 28 : sw[s];   /* the label measured + the frame */
    }
    /* a label too long for half the row (JA "Wikiビルドギャラリー" touched the frame): each tab as wide as its label, the
       rest shared */
    if ((need[0] > sw[0] || need[1] > sw[1]) && need[0] + need[1] <= LW - 8) {
        float extra = (LW - 8 - need[0] - need[1]) / 2;
        sw[0] = need[0] + extra; sw[1] = need[1] + extra; sx[1] = sw[0] + 8;
        for (int s = 0; s < 2; s++) if (sb[s]) U->place(sb[s], sx[s], y, sw[s], 38);
    }
    cmw_flat(U->panel(left, sx[st.src], y + 40, sw[st.src], 3, 0), GOLD);
    y += 52;
    float lh = PANEL_H - y;
    if (st.src == 0) {
        if (!lib) { txt(left, "fights:l.note", 0, y, LW, TR("My Units could not be read."), 15, WARN); return; }
        DSLObj sc = U->scroll(left, 0, y, LW, lh); float sy = 0;
        size_t nt = mu_team_count(lib), nu = mu_count(lib);
        if (!nt && !nu) { txt(sc, "fights:l.note", 0, 0, LW - 12, TR("My Units is empty. Add units there (wiki link, Unit Builder) or pick a build in the Wiki Build Gallery."), 15, SUB); return; }
        static char in_team[512]; memset(in_team, 0, sizeof in_team);   /* library units shown under a team */
        float uw = LW - 16;
        /* owner v0.1.12 #1: every unit has its own tick; a team's header is a tick too, a shortcut that adds or removes all
           its units (on = all of them in the squad). The line above the list names the units in the squad. */
        {
            char sq[700]; size_t k = 0; sq[0] = 0;
            for (int q = 0; q < st.nids; q++) { mu_info in; if (unit_info_by_id(st.ids[q], &in, NULL)) continue;
                                                k += (size_t)snprintf(sq + k, sizeof sq - k, "%s%s", k ? ", " : "", in.name); if (k >= sizeof sq) break; }
            char line[900];
            if (sq[0]) { snprintf(line, sizeof line, TR("In the squad: %s"), sq); dsi_utf8_fix_end(line); }
            else snprintf(line, sizeof line, "%s", TR("Tick the units to send in. A team's tick adds or removes all its units."));
            sy += txt(sc, "fights:l.picked", 0, sy, uw, line, 15, sq[0] ? GOLD : SUB) + 8;
        }
        for (size_t i = 0; i < nt; i++) {
            char name[96], ids[MU_TEAM_MAX][17], lab[200], nm[40], au[96];
            int n = mu_team_at(lib, i, name, sizeof name, ids), nin = 0;
            for (int k = 0; k < n; k++) nin += picked_at(ids[k]) >= 0;
            mu_info lead; int has_lead = n > 0 && !unit_info_by_id(ids[0], &lead, NULL);
            DSLObj ib = U->panel(sc, 0, sy, 40, 40, 0x1E1C21FFu);   /* the team icon: its first unit's portrait (as My Units), centred on the tick row */
            Obj sp = has_lead ? cm_icon(CM_ICON_PORTRAIT, lead.portrait) : NULL;
            if (!sp && has_lead) sp = cm_icon(CM_ICON_AFFECTER, lead.race);
            if (ib && sp) cm_set_sprite(ib, sp, 0xFFFFFFFFu);
            if (ib) cm_raycast(ib, false);
            snprintf(lab, sizeof lab, "%s%s  (%d/%d)%s", nin ? "<color=#D9BE8F>" : "", name, nin, n, nin ? "</color>" : "");
            snprintf(nm, sizeof nm, "fights:l.team%zu", i);
            /* the name on one line at the unit rows' size (ends in "..."), the count apart before the tick box: as one kit
               label it wrapped to two smaller lines with "(0/4)" on the border */
            DSLObj tg = cmw_named(U->toggle(sc, 46, sy + 2, uw - 46, "", n && nin == n, team_click, (void *)(intptr_t)i), CMW_TOGGLE, nm, team_click, (void *)(intptr_t)i, lab);
            if (tg) { char c[24]; snprintf(c, sizeof c, "%d/%d", nin, n);
                      /* the count measured, right-aligned 9 before the tick box; the name gets the row up to 10 before it */
                      float ce = uw - 46 - 44; DSLObj ct = mu_text(tg, ce - 46, 6, 46, 24, c, 15, DSL_TXT_RIGHT, nin ? GOLD : SUB);
                      float cw = ct ? cm_text_preferred_w(ct) : -1; if (cw <= 0 || cw > 46) cw = 46;
                      ellipsis_fit(mu_text(tg, 12, 6, ce - cw - 10 - 12, 24, name, 16, DSL_TXT_LEFT, nin ? GOLD : MAIN), name, ce - cw - 10 - 12); }
            sy += 48;
            if (team_author(i, au, sizeof au)) {
                char by[160]; snprintf(by, sizeof by, TR("by %s"), au); dsi_utf8_fix_end(by);
                snprintf(nm, sizeof nm, "fights:l.team%zu.by", i);
                sy += txt(sc, nm, 46, sy - 2, uw - 46, by, 14, SUB);
            }
            for (int k = 0; k < n; k++) {
                mu_info in; size_t ui;
                if (unit_info_by_id(ids[k], &in, &ui)) continue;
                if (ui < sizeof in_team) in_team[ui] = 1;
                int on = picked_at(in.id) >= 0;
                snprintf(lab, sizeof lab, TR("%s  Lv %d"), in.name, in.lvl); snprintf(nm, sizeof nm, "fights:l.team%zu.u%d", i, k);
                cmw_named(U->toggle(sc, 68, sy, uw - 68, lab, on, unit_toggle, (void *)(intptr_t)ui), CMW_TOGGLE, nm, unit_toggle, (void *)(intptr_t)ui, lab);
                sy += 40;
            }
            sy += 10;
        }
        int head_done = 0;
        for (size_t i = 0; i < nu; i++) {
            mu_info in; if (mu_info_at(lib, i, &in)) continue;
            if (i < sizeof in_team && in_team[i]) continue;
            if (nt && !head_done) { sy += txt(sc, "fights:l.solo", 0, sy, uw, TR("Units not in a team"), 16, GOLD) + 2; head_done = 1; }
            int on = picked_at(in.id) >= 0;
            char lab[160], nm[40]; snprintf(lab, sizeof lab, TR("%s  Lv %d"), in.name, in.lvl); snprintf(nm, sizeof nm, "fights:l.unit%zu", i);
            cmw_named(U->toggle(sc, 0, sy, uw, lab, on, unit_toggle, (void *)(intptr_t)i), CMW_TOGGLE, nm, unit_toggle, (void *)(intptr_t)i, lab);
            sy += 40;
        }
        U->scroll_height(sc, sy + 8);
        keep_set(0, sc, ky, sy + 8 - lh);
        thumb_add(0, left, sc, LW - 5, y, lh, sy + 8);
    } else {
        if (st.gal_builds) {
            y += txt(left, "fights:l.galtitle", 0, y, LW, st.gal_title, 17, MAIN);
            for (int i = 0; i < st.gal_n; i++) y += txt(left, NULL, 12, y, LW - 12, st.gal_names[i], 15, SUB);
            y += 8;
        } else y += txt(left, "fights:l.note", 0, y, LW, TR("Pick a team or a unit in the Wiki Build Gallery, then press Test Fight there."), 15, SUB) + 8;
        cmw_named(U->button(left, 0, y, LW, 40, TR("Open the Wiki Build Gallery"), gallery_open_click, NULL), CMW_BUTTON, "fights:l.gallery", gallery_open_click, NULL, TR("Open the Wiki Build Gallery"));
    }
}

/* right: the fight */
static void mode_change(void *ud, DSLObj w) { (void)ud; st.mode = U->value_get(w); status[0] = 0; paint_all(); }
static void boss_change(void *ud, DSLObj w) { (void)ud; st.boss = U->value_get(w); paint_all(); }
static void msat_change(void *ud, DSLObj w) { (void)ud; st.msat = U->value_get(w); }
static void combat_change(void *ud, DSLObj w) { (void)ud; st.combat = U->value_get(w); diff_save(); paint_all(); }
static void mgmt_change(void *ud, DSLObj w) { (void)ud; st.mgmt = U->value_get(w); diff_save(); }
static void mgmt_own_change(void *ud, DSLObj w) { (void)ud; int cur = mgmt_index(); st.mgmt_own = U->toggle_get(w); st.mgmt = cur; diff_save(); paint_all(); }
static void kit_change(void *ud, DSLObj w) { st.kit[(int)(intptr_t)ud] = U->toggle_get(w); paint_all(); }
static void insc_change(void *ud, DSLObj w) { (void)ud; st.kit_insc = U->toggle_get(w); paint_all(); }
static void monq_submit(void *ud, DSLObj w) { (void)ud; U->field_get(w, st.monq, sizeof st.monq); paint_all(); }
static void moncount_change(void *ud, DSLObj w) { (void)ud; st.mon_count = U->value_get(w) + 1; }
static const char *mon_name(const char *k);
static void mon_add(void *ud, DSLObj w) {
    (void)w; Mon *m = &P.mons[(int)(intptr_t)ud]; int n = st.mon_count < 1 ? 1 : st.mon_count > 30 ? 30 : st.mon_count;
    /* the room list is below the search results, often out of sight: the status line says what the click did */
    int at = -1; for (int i = 0; i < st.nmon; i++) if (!strcmp(st.mon[i].k, m->k)) at = i;
    if (at >= 0) st.mon[at].n = st.mon[at].n + n > 30 ? 30 : st.mon[at].n + n;
    else if (st.nmon < MAXMON) { at = st.nmon++; snprintf(st.mon[at].k, sizeof st.mon[0].k, "%s", m->k); st.mon[at].n = n; }
    if (at >= 0) { snprintf(status, sizeof status, TR("Added to the room: %s x%d."), mon_name(m->k), st.mon[at].n); status_warn = 0; }
    else { snprintf(status, sizeof status, "%s", TR("The room is full. Remove a monster first.")); status_warn = 1; }
    dsi_utf8_fix_end(status);
    paint_all();
}
static void mon_del(void *ud, DSLObj w) {
    (void)w; int i = (int)(intptr_t)ud; if (i < 0 || i >= st.nmon) return;
    memmove(&st.mon[i], &st.mon[i + 1], (size_t)(st.nmon - i - 1) * sizeof st.mon[0]); st.nmon--; paint_all();
}
static const char *mon_name(const char *k) { for (int i = 0; i < P.nmons; i++) if (!strcmp(P.mons[i].k, k)) return P.mons[i].tn[0] ? P.mons[i].tn : P.mons[i].n; return k; }
static int fold_has(const char *hay, const char *q) {
    if (!*q) return 1;
    size_t n = strlen(q);
    for (const char *h = hay; *h; h++) { size_t i = 0; while (i < n && h[i] && tolower((unsigned char)h[i]) == tolower((unsigned char)q[i])) i++; if (i == n) return 1; }
    return 0;
}
static int dec_comma(void) { const char *c = dsi_code(); return !strcmp(c, "de") || !strcmp(c, "es") || !strcmp(c, "pt-BR") || !strcmp(c, "fr") || !strcmp(c, "ru"); }
static const char *fmt_g(double v, char *o, size_t n) {             /* %g with the language's decimal mark (RU "107,5") */
    snprintf(o, n, "%g", v);
    if (dec_comma()) for (char *p = o; *p; p++) if (*p == '.') *p = ',';
    return o;
}
static void fmt_n(double v, char *o, size_t n) {                     /* 1000000 -> "1,000,000" (fmtN), the language's separator */
    const char *c = dsi_code(), *sep = !strcmp(c, "de") || !strcmp(c, "es") || !strcmp(c, "pt-BR") ? "."
                                      : !strcmp(c, "fr") || !strcmp(c, "ru") ? "\xC2\xA0" : ",";   /* 1.000.000 / 1 000 000 (no-break space) */
    char d[40]; snprintf(d, sizeof d, "%.0f", v); size_t l = strlen(d), k = 0, sl = strlen(sep);
    for (size_t i = 0; i < l && k + sl + 1 < n; i++) { if (i && d[i - 1] != '-' && (l - i) % 3 == 0) { memcpy(o + k, sep, sl); k += sl; } o[k++] = d[i]; }
    o[k] = 0;
}
/* owner v0.1.11 #2: the wiki's boss preview (art, name, key stats) for the chosen boss, in game style, from the game's own
   data (UnitTable) and sprites (the unit's Body sprite); the stat names are the game's. Returns its height. */
static void fmt_stat(int i, float v, char *o, size_t n) {
    char t[40];
    if (i == TFC_ASPD) fmt_g((double)((long long)(v * 100 + 0.5)) / 100, o, n);
    else if (i == TFC_CRIT) { fmt_g((double)((long long)(v * 10 + 0.5)) / 10, t, sizeof t); snprintf(o, n, "%s%%", t); }
    else { fmt_n(v, t, sizeof t); snprintf(o, n, "%s", t); }
}
/* the DPS check's card, as the boss card (owner v0.1.12): the wiki's DPS check picture (builder SG_MAPS.dps,
   assets/img/tour/sg-dps-map.png: the boss room with the scarecrow, game art only), copied into the mod at build time as
   art/dps_check.png (build_module.sh; never in git, never downloaded), the dummy's game name, its durability and its self repair. */
static float dps_card(DSLObj parent, float y, float w) {
    const float H = 156, A = 132;
    DSLObj card = cmw_flat(U->panel(parent, 0, y, w, H, 0), 0x2D2A31FFu);
    if (!card) return 0;
    cmw_flat(U->panel(card, 0, 0, w, 1, 0), 0xB6835AFFu);
    cmw_flat(U->panel(card, 0, H - 1, w, 1, 0), 0x633D33FFu);
    DSLObj box = cmw_flat(U->panel(card, 12, 12, A, A, 0), 0x1E1C21FFu);
    char png[1300]; snprintf(png, sizeof png, "%s/art/dps_check.png", cm_mod_dir());
#ifdef _WIN32
    int have = cm_mod_dir()[0] != 0;                                    /* a UTF-8 path: the kit opens it, fopen would not */
#else
    FILE *f = cm_mod_dir()[0] ? fopen(png, "rb") : NULL; int have = f != NULL;
    if (f) fclose(f);
#endif
    DSLObj art = have && box && U->image_png ? U->image_png(box, 4, 4, A - 8, A - 8, png, 1) : NULL;
    if (art) { cm_raycast(art, false); cmw_named(art, CMW_TEXT, "fights:r.dpsart", NULL, NULL, "art/dps_check.png"); }
    else cmw_log("tf: DPS check card: no picture at %s", png);
    float x = 12 + A + 16, cw = w - x - 12;
    const char *nm = P.dummy_tn[0] ? P.dummy_tn : P.dummy_n;
    DSLObj nmt = U->text(card, x, 10, cw, 32, nm, 20, DSL_TXT_LEFT | DSL_TXT_TITLE, GOLD);
    if (nmt) { cm_text_ellipsis(nmt); cmw_named(nmt, CMW_TEXT, "fights:r.dpsname", NULL, NULL, nm); }
    char lab[2][96], val[2][48]; int n = 0;
    snprintf(lab[n], sizeof lab[n], "%s", cm_gw("TEXTKEY_STATUS_MaxDurability_NAME", "Max Durability")); fmt_n(P.dummy_dur, val[n], sizeof val[n]); n++;
    if (P.regen_ps > 0) {
        char ps[40]; fmt_n(P.regen_ps, ps, sizeof ps);
        snprintf(lab[n], sizeof lab[n], "%s", P.regen_tn[0] ? P.regen_tn : P.regen_n); snprintf(val[n], sizeof val[n], TR("%s/s"), ps); n++;
    }
    char stats[400]; size_t k = 0;
    for (int i = 0; i < n; i++) {                                      /* the boss card's rows: label left, value right */
        float cy = 46 + (float)i * 24;
        DSLObj l = U->text(card, x, cy, cw - 130, 24, lab[i], 14, DSL_TXT_LEFT, SUB);
        if (l) cm_text_ellipsis(l);
        U->text(card, x + cw - 130, cy, 120, 24, val[i], 15, DSL_TXT_RIGHT, MAIN);
        k += (size_t)snprintf(stats + k, sizeof stats - k, "%s%s=%s", i ? ";" : "", lab[i], val[i]);
    }
    cmw_named(card, CMW_TEXT, "fights:r.dpscard", NULL, NULL, stats);
    return H;
}
static float boss_card(DSLObj parent, float y, float w, const Boss *b) {
    const float H = 156, A = 132;
    DSLObj card = cmw_flat(U->panel(parent, 0, y, w, H, 0), 0x2D2A31FFu);
    if (!card) return 0;
    cmw_flat(U->panel(card, 0, 0, w, 1, 0), 0xB6835AFFu);
    cmw_flat(U->panel(card, 0, H - 1, w, 1, 0), 0x633D33FFu);
    DSLObj box = cmw_flat(U->panel(card, 12, 12, A, A, 0), 0x1E1C21FFu);
    Obj sp = tfc_unit_art(b->key);                                   /* cut to its visible pixels: it fills the box */
    DSLObj art = sp && box ? U->panel(box, 4, 4, A - 8, A - 8, 0xFFFFFFFFu) : NULL;
    if (art) { cm_set_sprite(art, sp, 0xFFFFFFFFu); cm_raycast(art, false); }
    float x = 12 + A + 16, cw = w - x - 12;
    DSLObj nmt = U->text(card, x, 10, cw, 32, b->tn[0] ? b->tn : b->n, 20, DSL_TXT_LEFT | DSL_TXT_TITLE, GOLD);
    if (nmt) { cm_text_ellipsis(nmt); cmw_named(nmt, CMW_TEXT, "fights:r.bossname", NULL, NULL, b->tn[0] ? b->tn : b->n); }
    float v[TFC_N];
    if (tfc_unit_stats(b->key, v)) { txt(card, "fights:r.bossstats", x, 46, cw, TR("The game's data for this boss is not loaded yet."), 14, SUB); return H; }
    /* the wiki unit card's rows: HP, attacks, attack speed, critical chance, resistances, movement speed */
    char stats[1200]; size_t k = 0;
    float colw = cw / 2, ry = 46;
    for (int i = 0; i < TFC_N; i++) {
        char key[96], val[40]; snprintf(key, sizeof key, "TEXTKEY_STATUS_%s_NAME", tfc_field[i]);
        const char *lab = cm_gw(key, tfc_field[i]);
        fmt_stat(i, v[i], val, sizeof val);
        float cx = x + (i % 2) * colw, cy = ry + (float)(i / 2) * 24;
        DSLObj l = U->text(card, cx, cy, colw - 76, 24, lab, 14, DSL_TXT_LEFT, SUB);   /* clear of the value's box */
        if (l) cm_text_ellipsis(l);
        U->text(card, cx + colw - 74, cy, 64, 24, val, 15, DSL_TXT_RIGHT, MAIN);
        if (k + 100 < sizeof stats) k += (size_t)snprintf(stats + k, sizeof stats - k, "%s%s=%s", i ? ";" : "", tfc_field[i], val);
    }
    cmw_named(card, CMW_TEXT, "fights:r.bosscard", NULL, NULL, stats);
    return H;
}
static void paint_right(void) {
    cmw_forget("fights:r.");
    float ky = keep_get(1); cm_il_hset(&h_keep[1], NULL);
    if (right_sc) U->destroy(right_sc);
    right_sc = NULL; right = NULL;
    float y = 0, w = RW - 16;
    if (engine_state != 2) {
        right_sc = cmw_flat(U->panel(body, RX, 16, RW, PANEL_H, 0), 0x00000000); right = right_sc;
        const char *m = engine_state == 3 ? eng_msg : TR("Getting the wiki's Test Fights files...");
        txt(right, "fights:r.engine", 0, 0, w, m, 16, engine_state == 3 ? WARN : SUB);
        return;
    }
    names_refresh();
    right_sc = cmw_flat(U->panel(body, RX, 16, RW, PANEL_H, 0), 0x00000000);   /* holder: one destroy removes it all */
    DSLObj vp = U->scroll(right_sc, 0, 0, RW - 10, PANEL_H);
    right = vp;
    /* What do you want */
    y += head(right, 0, y, w, TR("What to test"));
    const char *ml[6]; char md[600] = "";
    for (int i = 0; i < P.nmodes; i++)
        ml[i] = !strcmp(P.modes[i], "dps") ? TR("DPS Check") : !strcmp(P.modes[i], "boss") ? TR("Boss fight") : !strcmp(P.modes[i], "arena") ? TR("Custom fight") : TR("Finished base");
    cmw_named(U->value_row(right, 0, y, w, TR("Test"), ml, P.nmodes, st.mode, mode_change, NULL), CMW_VALUE, "fights:r.mode", mode_change, NULL, ml[st.mode]);
    y += 50;
    if (!strcmp(mode_id(), "dps")) { char dur[40]; fmt_n(P.dummy_dur, dur, sizeof dur); snprintf(md, sizeof md, TR("A practice %s with %s durability that taunts your units. Hit it to see how much damage your squad does."), P.dummy_tn, dur); }
    else if (!strcmp(mode_id(), "boss")) snprintf(md, sizeof md, "%s", TR("Your squad standing at a boss door, boss alive and never beaten."));
    else if (!strcmp(mode_id(), "arena")) snprintf(md, sizeof md, "%s", TR("The same boss room with the boss taken out and the monsters you choose put in instead."));
    else snprintf(md, sizeof md, "%s", TR("A fully built base with all research done. Your squad walks in ready to play."));
    y += txt(right, "fights:r.modedesc", 12, y, w - 12, md, 15, SUB) + 10;
    if (!strcmp(mode_id(), "dps")) y += dps_card(right, y, w) + 10;
    /* Which fight / Which room */
    if (is_fight() && P.nbosses) {
        y += head(right, 0, y, w, !strcmp(mode_id(), "boss") ? TR("Which fight") : TR("Which room"));
        const char *bl[8]; for (int i = 0; i < P.nbosses; i++) bl[i] = P.bosses[i].tn[0] ? P.bosses[i].tn : P.bosses[i].n;
        cmw_named(U->value_row(right, 0, y, w, TR("Boss"), bl, P.nbosses, st.boss, boss_change, NULL), CMW_VALUE, "fights:r.boss", boss_change, NULL, bl[st.boss]);
        y += 50;
        if (!strcmp(mode_id(), "boss")) y += boss_card(right, y, w, &P.bosses[st.boss]) + 10;
        if (P.bosses[st.boss].abyss && P.nmsat) {
            char lab[12][48]; const char *ll[12];
            for (int i = 0; i < P.nmsat; i++) { snprintf(lab[i], sizeof lab[i], P.msat_keys[i] ? TR("Day %d") : TR("Off"), P.msat_keys[i]); dsi_utf8_fix_end(lab[i]); ll[i] = lab[i]; }
            cmw_named(U->value_row(right, 0, y, w, cm_gw("TEXTKEY_AFFECTER_MagicSaturation_NAME", TR_NOOP("Magic Saturation")), ll, P.nmsat, st.msat, msat_change, NULL), CMW_VALUE, "fights:r.msat", msat_change, NULL, ll[st.msat]);
            y += 50;
            y += txt(right, NULL, 12, y, w - 12, TR("The save turns Magic Saturation on and writes the day you pick. Only Abyss monsters get it."), 14, SUB) + 6;
        }
        if (!strcmp(mode_id(), "arena")) {
            y += txt(right, NULL, 12, y, w - 12, TR("The boss is removed from that room and your monsters take its place."), 14, SUB) + 6;
            cmw_named(U->field(right, 0, y, w - 220, 40, TR("Search monsters by name..."), st.monq, monq_submit, NULL), CMW_FIELD, "fights:r.monq", monq_submit, NULL, st.monq);
            static const char *cnt[30]; static char cb[30][4];
            for (int i = 0; i < 30; i++) { snprintf(cb[i], 4, "%d", i + 1); cnt[i] = cb[i]; }
            cmw_named(U->value_row(right, w - 212, y - 2, 212, TR("How many"), cnt, 30, st.mon_count - 1, moncount_change, NULL), CMW_VALUE, "fights:r.moncount", moncount_change, NULL, cnt[st.mon_count - 1]);
            y += 48;
            int shown = 0;
            for (int i = 0; i < P.nmons && shown < 12; i++) {
                const char *mn = P.mons[i].tn[0] ? P.mons[i].tn : P.mons[i].n;
                if (!fold_has(mn, st.monq) && !fold_has(P.mons[i].n, st.monq)) continue;   /* the shown name or the English one */
                char nm[40]; snprintf(nm, sizeof nm, "fights:r.hit%d", shown);
                float bx = (shown % 2) * (w / 2 + 2);
                cmw_named(U->button(right, bx, y, w / 2 - 2, 34, mn, mon_add, (void *)(intptr_t)i), CMW_BUTTON, nm, mon_add, (void *)(intptr_t)i, mn);
                if (shown % 2) y += 38;
                shown++;
            }
            if (shown % 2) y += 38;
            if (!shown) y += txt(right, NULL, 12, y, w - 12, TR("No monster matches."), 14, SUB);
            y += 6;
            for (int i = 0; i < st.nmon; i++) {
                char lab[160], nm[40]; snprintf(lab, sizeof lab, TR("%s x%d"), mon_name(st.mon[i].k), st.mon[i].n);
                txt(right, NULL, 12, y + 6, w - 140, lab, 16, MAIN);
                snprintf(nm, sizeof nm, "fights:r.mondel%d", i);
                cmw_named(U->button(right, w - 120, y, 120, 34, TR("Remove"), mon_del, (void *)(intptr_t)i), CMW_BUTTON, nm, mon_del, (void *)(intptr_t)i, TR("Remove"));
                y += 38;
            }
            y += 8;
        }
    }
    /* Difficulty */
    y += head(right, 0, y, w, TR("Difficulty"));
    const char *cl[8], *gl[8];
    for (int i = 0; i < P.ncombat; i++) cl[i] = P.tlabel[i][0] ? P.tlabel[i] : P.label[i];
    for (int i = 0; i < P.nmgmt; i++) { gl[i] = P.mgmt[i]; for (int c = 0; c < P.ncombat; c++) if (!strcmp(P.combat[c], P.mgmt[i])) gl[i] = cl[c]; }
    cmw_named(U->value_row(right, 0, y, w, cm_gw("TEXTKEY_SaveLoad_CombatDifficulty", TR_NOOP("Combat")), cl, P.ncombat, st.combat, combat_change, NULL), CMW_VALUE, "fights:r.combat", combat_change, NULL, cl[st.combat]);
    y += 50;
    int mi = mgmt_index();
    DSLObj mg = cmw_named(U->value_row(right, 0, y, w, cm_gw("TEXTKEY_SaveLoad_ManagementDifficulty", TR_NOOP("Management")), gl, P.nmgmt, mi, mgmt_change, NULL), CMW_VALUE, "fights:r.mgmt", mgmt_change, NULL, gl[mi]);
    if (!st.mgmt_own) U->set_enabled(mg, 0);
    y += 50;
    cmw_named(U->toggle(right, 0, y, w, TR("Choose management difficulty separately"), st.mgmt_own, mgmt_own_change, NULL), CMW_TOGGLE, "fights:r.mgmtown", mgmt_own_change, NULL, TR("Choose management difficulty separately"));
    y += 42;
    if (!st.mgmt_own) { char t[200]; snprintf(t, sizeof t, TR("Left off, the economy matches the combat difficulty (%s)."), gl[mi]); y += txt(right, NULL, 12, y, w - 12, t, 14, SUB); }
    y += 10;
    /* Battle kits */
    if (P.nkits) {
        y += head(right, 0, y, w, TR("Battle kits"));
        y += txt(right, NULL, 12, y, w - 12, TR("Each ticked kit is packed into the personal backpack of every character going in, so the squad walks in carrying it."), 14, SUB) + 4;
        int sl = 0, any = 0; double wt = 0;
        for (int i = 0; i < P.nkits; i++) {
            char lab[200], nm[40], g[32]; snprintf(lab, sizeof lab, TRN("%s  (%d slot, %s weight)", "%s  (%d slots, %s weight)", P.kits[i].slots), TR(P.kits[i].n), P.kits[i].slots, fmt_g(P.kits[i].w, g, sizeof g));
            snprintf(nm, sizeof nm, "fights:r.kit%d", i);
            /* owner v0.1.11 #4: the kit's header icons at the start of the row, as the wiki's kit chip (builder.js paintSup:
               the first 4 items' icons), drawn from the game's own item sprites */
            float ix = 0; int ni = P.kits[i].nit < 4 ? P.kits[i].nit : 4;
            for (int j = 0; j < ni; j++) {
                Obj sp = cm_icon(CM_ICON_ITEM, P.kits[i].ik[j]);
                DSLObj ic = U->panel(right, ix, y + 4, 28, 28, sp ? 0xFFFFFFFFu : 0x1E1C21FFu);   /* centred on the 36-high row */
                if (ic && sp) cm_set_sprite(ic, sp, 0xFFFFFFFFu);
                if (ic) cm_raycast(ic, false);
                ix += 30;
            }
            if (ni) ix += 6;
            DSLObj kt = cmw_named(U->toggle(right, ix, y, w - ix, lab, st.kit[i], kit_change, (void *)(intptr_t)i), CMW_TOGGLE, nm, kit_change, (void *)(intptr_t)i, lab);
            if (kt && P.kits[i].nit) {                                   /* the wiki chip's hover: the kit item by item */
                char tb[900]; size_t k = 0; tb[0] = 0;
                for (int j = 0; j < P.kits[i].nit && k + 120 < sizeof tb; j++)
                    k += (size_t)snprintf(tb + k, sizeof tb - k, "%s%s x%d", j ? "\n" : "", P.kits[i].itn[j][0] ? P.kits[i].itn[j] : P.kits[i].in_[j], P.kits[i].iq[j]);
                U->tooltip(kt, TR(P.kits[i].n), tb);
            }
            y += 40;
            if (st.kit[i]) { sl += P.kits[i].slots; wt += P.kits[i].w; any = 1; }
        }
        char t[300];
        if (!any) snprintf(t, sizeof t, "%s", TR("No kit ticked: the squad walks in with only its gear."));
        else { char g[32]; snprintf(t, sizeof t, TRN("%d slot and %s weight per character.", "%d slots and %s weight per character.", sl), sl, fmt_g((double)((long long)(wt * 100 + 0.5)) / 100, g, sizeof g)); }
        y += txt(right, NULL, 12, y, w - 12, t, 14, SUB) + 4;
        if (any && P.insc_n[0]) {
            char lab[200]; snprintf(lab, sizeof lab, TR("Grant %s on custom saves"), P.insc_tn[0] ? P.insc_tn : P.insc_n);
            /* owner v0.1.11 #5 (and the wiki's own rule, builder.js paintSup): the inscription's icon first; hovering it shows
               the game's own hover card for that inscription */
            Obj sp = P.insc_k[0] ? cm_icon(CM_ICON_AFFECTER, P.insc_k) : NULL;
            DSLObj ic = cmw_named(U->panel(right, 0, y + 1, 34, 34, sp ? 0xFFFFFFFFu : 0x1E1C21FFu), CMW_TEXT, "fights:r.inscicon", NULL, NULL, P.insc_k);
            if (ic && sp) cm_set_sprite(ic, sp, 0xFFFFFFFFu);
            if (ic && P.insc_k[0]) cm_gtip_add(ic, CM_GTIP_AFFECTER, P.insc_k);
            cmw_named(U->toggle(right, 40, y, w - 40, lab, st.kit_insc, insc_change, NULL), CMW_TOGGLE, "fights:r.insc", insc_change, NULL, lab);
            y += 42;
            y += txt(right, NULL, 12, y, w - 12, TR("Added by default so every unit has enough carry weight for the battle kits."), 14, SUB);
            if (st.kit_insc) snprintf(t, sizeof t, TR("Every character gains %s so the load is carryable."), P.insc_tn[0] ? P.insc_tn : P.insc_n);
            else snprintf(t, sizeof t, "%s", TR("Inscription off. Units may be overweight if you keep the kits packed."));
            y += txt(right, NULL, 12, y, w - 12, t, 14, SUB);
        }
        for (int i = 0; i < P.nmiss; i++) y += txt(right, NULL, 12, y, w - 12, P.miss[i], 14, WARN);
    }
    U->scroll_height(vp, y + 16);
    keep_set(1, vp, ky, y + 16 - PANEL_H);
    thumb_add(1, right_sc, vp, RW - 5, 0, PANEL_H, y + 16);
}
static void paint_status(void) {
    char sq[400]; int n = st.src == 1 ? (st.gal_builds ? st.gal_n : 0) : st.nids;
    if (n > P.max_squad && P.max_squad) n = P.max_squad;
    snprintf(sq, sizeof sq, TR("Squad: %d / %d"), n, P.max_squad ? P.max_squad : 4);
    char all[1700], shown[1740]; snprintf(all, sizeof all, "%s%s%s", sq, status[0] ? "\n" : "", status);
    snprintf(shown, sizeof shown, "%s%s%s%s%s", sq, status[0] ? "\n" : "", status_warn ? "<color=#EA9F3D>" : "", status, status_warn ? "</color>" : "");
    if (w_status) U->destroy(w_status);                               /* only the problem shows orange, not the squad count */
    /* Try again only while the files failed and a new try can help (not for a game build that differs); the status box
       stops before it (they overlapped). A long message (problems found) grows the box upward, the lists get shorter:
       it never runs past the window's bottom. */
    int retry = engine_state == 3 && !eng_mism;
    float sw = (retry ? WIN_W - 24 - 220 - 180 : WIN_W - 24 - 220) - 12 - 24;
    w_status = cmw_named(U->text(body, 24, BODY_H - 76, sw, 70, shown, 15, DSL_TXT_LEFT | DSL_TXT_WRAP, MAIN), CMW_TEXT, "fights:status", NULL, NULL, all);
    float ph = pref_h(w_status, sw), sh = ph + 4 > 70 ? ph + 4 : 70;
    if (sh > 230) sh = 230;                                           /* past that the kit sizes the text down */
    foot_x = sh - 70;
    if (foot_x > 0) U->place(w_status, 24, BODY_H - 76 - foot_x, sw, sh);
    if (w_div) U->place(w_div, 24, BODY_H - 84 - foot_x, WIN_W - 48, 1);
    if (w_retry) U->set_active(w_retry, retry);
    U->set_enabled(w_start, engine_state == 2 && !busy);
    cmw_set_text(w_start, busy ? TR("Working...") : TR("Start"));
}
static void paint_all(void) {
    if (!cmw_is_open("fights") || !body) return;
    paint_status(); paint_left(); paint_right();                     /* the status first: its height sets the lists' */
}
static void closed(void *ud) {
    (void)ud; body = left = right = right_sc = w_status = w_start = w_retry = w_div = NULL;
    cm_il_hset(&h_keep[0], NULL); cm_il_hset(&h_keep[1], NULL); foot_x = 0;
    if (lib) { jfree(lib); lib = NULL; }
    cmw_forget("fights:");
}
static void retry_click(void *ud, DSLObj w) { (void)ud; (void)w; if (engine_state == 3) { engine_state = 0; engine_open(); paint_all(); } }
static void build_window(void) {
    U = cmw_ui(); if (!U) return;
    body = cmw_open("fights", TR("Test Fights"), WIN_W, WIN_H, closed, NULL);
    if (!body) return;
    left = right = right_sc = NULL;
    char e[300]; if (lib) jfree(lib);
    lib = mu_load(e, sizeof e);
    if (!lib) cmw_log("tf: My Units: %s", e);
    cm_il_hset(&h_keep[0], NULL); cm_il_hset(&h_keep[1], NULL); foot_x = 0;
    w_div = cmw_flat(U->panel(body, 24, BODY_H - 84, WIN_W - 48, 1, 0), 0x633D33FF);
    w_status = NULL;
    w_start = cmw_named(U->button(body, WIN_W - 24 - 220, BODY_H - 72, 220, 52, TR("Start"), start_click, NULL), CMW_BUTTON, "fights:start", start_click, NULL, TR("Start"));
    /* always made, shown by paint_status: the files often fail after the window opened (it was missing then) */
    w_retry = cmw_named(U->button(body, WIN_W - 24 - 220 - 180, BODY_H - 66, 170, 40, TR("Try again"), retry_click, NULL), CMW_BUTTON, "fights:retry", retry_click, NULL, TR("Try again"));
    engine_open();
    paint_all();
}
void tf_open(void) { build_window(); }
static void tf_relang(void) { if (cmw_is_open("fights")) build_window(); }   /* the language changed: drawn again */
void tf_open_with_builds(const char *builds_json, const char *title, const char *const *names, int n) {
    free(st.gal_builds); st.gal_builds = builds_json ? strdup(builds_json) : NULL;
    snprintf(st.gal_title, sizeof st.gal_title, "%s", title ? title : "");
    st.gal_n = 0;
    for (int i = 0; i < n && i < 8; i++) snprintf(st.gal_names[st.gal_n++], 96, "%s", names[i] ? names[i] : "");
    st.src = 1; status[0] = 0;
    build_window();
}
extern void gal_open(void);
static void gallery_open_click(void *ud, DSLObj w) { (void)ud; (void)w; gal_open(); }

static void test_state(char *out, size_t n) {
    snprintf(out, n, "\"tf\":{\"engine\":%d,\"busy\":%d,\"mode\":\"%s\",\"squad\":%d,\"src\":%d,\"status\":\"%.300s\",\"tag\":\"%s\",\"combat\":\"%s\",\"mgmt\":\"%s\",\"team\":%d}",
             engine_state, busy, P.ok ? mode_id() : "", st.src == 1 ? st.gal_n : st.nids, st.src, status, P.tag,
             P.ok && st.combat < P.ncombat ? P.combat[st.combat] : "", P.ok && P.nmgmt ? P.mgmt[mgmt_index()] : "", st.team_i);
    for (char *p = out; *p; p++) if (*p == '\n') *p = ' ';
}

/* ------------------------------------------------------------------ entry point (called by cm_module.c) */
__attribute__((visibility("default"))) void cm_fights_init(const DSLHost *host, DSLPlugin *self) {
    if (cmw_init(host, self)) return;
    if (cm_work_start()) { cmw_log("tf: worker thread did not start: Test Fights is off"); return; }
    tfw_init(host, self);
    cmw_test_opener("fights", tf_open);
    cmw_test_state(test_state);
    cmw_tick_add(thumb_tick, NULL);
    cm_home_add("test-fights", TR_NOOP("Test Fights"), NULL, 40, tf_open);
    cm_i18n_on_change(tf_relang);
}
