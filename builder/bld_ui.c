// bld_ui.c - the Unit Builder window (owner AFTER_MVP 4: "Unit Builder drawn in the game's menus"; 2026-10-15: "we want
// to allow people to create custom units inside of the multiplayer"; "when you create a unit, make sure that it's saved
// inside of the game, so that if I want to use the same unit multiple times inside of a hub, I can").
// Opened from the title's Custom Modes page ("Unit Builder"), from My Units ("New unit", "Edit") and from Bring Units
// ("New unit"). A game-style window (the character sheet's look): name, level, then Presets / Race / Background / Traits /
// Level-ups / Skills / Gear / Inscriptions on the left (search and filters above the lists), the live sheet on the right. Every choice comes from the wiki builder's own data and
// rules (bld_js.h model on the bld_work thread): the window shows what the model lists and sends each click to it.
// Save = the build into My Units (myunits.h mu_add_build; Edit replaces that unit's build in place, its teams keep it).
// A unit made here lives only in the My Units library file: it never goes into a regular save (Bring Units hands out
// copies; the FE guard refuses library units in a world's results).
#include "bld.h"
#include "bld_js.h"
#include "bld_work.h"
#include "mu_ui.h"
#include "myunits.h"
#include "mu_look.h"
#include "bld_filter.h"
#include "cm_home.h"
#include "cm_il.h"
#include "sg_cache.h"
#include "i18n/ds_i18n.h"
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <ctype.h>
#include <time.h>

#define SCRIPTS_MAX_AGE (6 * 3600)          /* same as Test Fights: the wiki's scripts are asked again after 6 h */
enum { SEC_RACE, SEC_BG, SEC_TRAITS, SEC_SKILLS, SEC_GEAR, SEC_INSC, SEC_LEVEL, SEC_PRESETS, SEC_N };
static const char *const SEC_NAME[SEC_N] = { TR_NOOP("Race"), TR_NOOP("Background"), TR_NOOP("Traits"), TR_NOOP("Skills"), TR_NOOP("Gear"), TR_NOOP("Inscriptions"),
                                             TR_NOOP("Level-ups"), TR_NOOP("Presets") };   /* English: also the test names */
static const int NAV_ORDER[SEC_N] = { SEC_PRESETS, SEC_RACE, SEC_BG, SEC_TRAITS, SEC_LEVEL, SEC_SKILLS, SEC_GEAR, SEC_INSC };   /* top to bottom */
static const char *sec_label(int i) { return i == SEC_SKILLS ? cm_gw("TEXTKEY_GAMEGUIDE_SkillsAbility_NAME", SEC_NAME[i]) : TR(SEC_NAME[i]); }
enum { ENG_NONE, ENG_OPENING, ENG_READY, ENG_FAILED };

static const DSLUi *U;

/* ------------------------------------------------------------------ the model on the worker thread */
static bld_engine *E;                      /* worker thread only */
static int eng;                            /* main thread's view of the engine */
static char eng_msg[700];
static char eng_why[600];                                           /* the engine error behind eng_msg: log only */

typedef struct { bld_job j; int kind; char *in, *out, *build, *link; char err[700]; char root[1100]; char game_build[32]; } Job;
enum { J_OPEN = 1, J_LOAD, J_ACT, J_SAVE };

static int eng_ensure(Job *b) {
    if (E) return 0;
    eng_why[0] = 0;
    sgc c; sgc_init(&c, b->root);
    sg_sources src; char e[500] = "", st[200] = "";
    if (sgc_scripts(&c, SCRIPTS_MAX_AGE, &src, st, sizeof st, e, sizeof e)) {
        snprintf(b->err, sizeof b->err, TR("The Unit Builder needs the wiki's builder files once (dungeonsettlers.wiki), and they could not be downloaded: %s"), e);
        return -1;
    }
    E = bld_open(src.data_js, src.data_n, src.builder_js, src.builder_n, NULL, e, sizeof e);
    sgc_sources_free(&src);
    if (!E) { snprintf(b->err, sizeof b->err, "%s", TR("The wiki's Unit Builder could not start. Try again later.")); snprintf(eng_why, sizeof eng_why, "%s", e); return -1; }
    char *tag = bld_call(E, "tag", NULL, e, sizeof e), t[64] = "";
    if (tag) { sscanf(tag, "\"%63[^\"]", t); free(tag); }
    if (b->game_build[0] && t[0] && strcmp(t, b->game_build)) {
        snprintf(b->err, sizeof b->err, TR("The wiki's Unit Builder files are for game build %s, and this game is build %s. The Unit Builder opens again when both match."), t, b->game_build);
        bld_close(E); E = NULL;
        return -1;
    }
    return 0;
}
static void job_run(bld_job *j) {
    Job *b = (Job *)j; char e[700] = "";
    if (eng_ensure(b)) return;
    if (b->kind == J_LOAD) b->out = bld_call(E, "load", b->in ? b->in : "null", e, sizeof e);
    else if (b->kind == J_ACT) b->out = bld_call(E, "act", b->in, e, sizeof e);
    else if (b->kind == J_SAVE) {
        if (b->in) { char *s = bld_call(E, "act", b->in, e, sizeof e); free(s); }
        b->build = bld_call(E, "build", NULL, e, sizeof e);
        b->link = b->build ? bld_call(E, "link", NULL, e, sizeof e) : NULL;
        b->out = b->link ? bld_call(E, "state", NULL, e, sizeof e) : NULL;
    } else return;
    if (!b->out) snprintf(b->err, sizeof b->err, "%s", e[0] ? e : TR("no answer"));
}
static void job_done(bld_job *j);
static void post(int kind, const char *in) {
    Job *b = calloc(1, sizeof *b); if (!b) return;
    b->j.run = job_run; b->j.done = job_done; b->kind = kind; b->in = in ? strdup(in) : NULL;
    char d[1024] = "";
    { cm_m m = cm_il_method("UnityEngine", "Application", "get_persistentDataPath", 0, NULL);
      if (m.fn) cm_il_utf8(CM_CALL(m, Obj, const void *)(m.mi), d, sizeof d); }
    snprintf(b->root, sizeof b->root, "%s/Mods/custom-modes/wiki", d);
    { const DSLHost *h = cm_host(); snprintf(b->game_build, sizeof b->game_build, "%s", h && h->game_build ? h->game_build : ""); }
    if (eng != ENG_READY) eng = ENG_OPENING;
    bld_work_post(&b->j);
}

/* ------------------------------------------------------------------ the window */
#define REGMAX 900
static struct {
    int open, section, open_slot;          /* open_slot: gear slot whose item list shows (-1 none) */
    char unit_id[17], lib_name[96];        /* editing this My Units unit ("" = a new unit) */
    void (*saved)(const char *, void *); void *ud;
    DSLObj win, f_name, v_lvl, status, b_save, b_cancel, nav[SEC_N], mid, mid_holder, sheet, bar, f_search;
    float w, h, lw, lh, scale, mx, my, mw, mh, sx, bar_h, scroll_h, lx;   /* lx: the Level row's x */
    int bar_sec, bar_slot, search_wait;    /* the filter bar drawn for (section, open slot); ticks left before a search redraws */
    int lvl_n;                             /* values in v_lvl */
    jv *st;                                /* the model's last state */
    char msg[800]; uint32_t msg_col;
    int saving, take_name;                 /* take_name: the next load names the unit (a gallery build) */
    int dirty; char name0[96];             /* a change since the window opened (Cancel asks); the name field as loaded */
    struct { char name[120]; DSLObj o; } reg[REGMAX]; int nreg;
} W = { .open_slot = -1 };

static void refresh(void);
static int build_ui(void);
static void reg(const char *name, DSLObj o) { if (o && W.nreg < REGMAX) { snprintf(W.reg[W.nreg].name, sizeof W.reg[W.nreg].name, "%s", name); W.reg[W.nreg++].o = o; } }
static void set_msg(const char *m, uint32_t col) {
    snprintf(W.msg, sizeof W.msg, "%s", m ? m : ""); W.msg_col = col;
    if (W.status) U->set_text(W.status, W.msg);
}
static void sraw(const jv *v, char *out, size_t n) {          /* a JSON string's text (escapes resolved for \" \\ \n) */
    if (!n) return; out[0] = 0;
    if (!v || v->t != JN_STR) return;
    size_t k = 0;
    for (size_t i = 0; i < v->n && k + 1 < n; i++) {
        char c = v->s[i];
        if (c == '\\' && i + 1 < v->n) {
            char d = v->s[++i];
            if (d == 'n') c = '\n'; else if (d == 't') c = ' ';
            else if (d == 'u' && i + 4 < v->n) {                 /* \uXXXX -> UTF-8 (BMP) */
                unsigned u = 0; sscanf(v->s + i + 1, "%4x", &u); i += 4;
                if (u < 0x80) c = (char)u;
                else if (u < 0x800) { if (k + 2 >= n) break; out[k++] = (char)(0xC0 | (u >> 6)); c = (char)(0x80 | (u & 0x3F)); }
                else { if (k + 3 >= n) break; out[k++] = (char)(0xE0 | (u >> 12)); out[k++] = (char)(0x80 | ((u >> 6) & 0x3F)); c = (char)(0x80 | (u & 0x3F)); }
            } else c = d;
        }
        out[k++] = c;
    }
    out[k] = 0;
}
static void jesc(char *o, size_t cap, const char *s) {
    size_t n = 0;
    for (; *s && n + 3 < cap; s++) { if (*s == '"' || *s == '\\') o[n++] = '\\'; if ((unsigned char)*s < 0x20) continue; o[n++] = *s; }
    o[n] = 0;
}
static int num(const jv *o, const char *k) { return (int)jnumv(jget(o, k)); }
static int truthy(const jv *o, const char *k) { return jtruthy(jget(o, k)); }

static void close_win(void) {
    if (!W.open) return;
    W.open = 0;
    if (W.win) U->destroy(W.win);
    W.win = W.mid = W.mid_holder = W.sheet = W.bar = W.f_search = NULL;
    W.nreg = 0;
    if (W.st) { jfree(W.st); W.st = NULL; }
    W.saved = NULL; W.ud = NULL;
}
static void discard_ok(void *ud, DSLObj w) { (void)ud; (void)w; if (W.open) { mu_log("builder: closed without saving (changes discarded)"); close_win(); } }
static void on_cancel(void *ud, DSLObj w) {      /* unsaved changes: the game's notice asks first (owner 2026-10-21) */
    (void)ud; (void)w;
    char nm[96] = ""; if (W.f_name) U->field_get(W.f_name, nm, sizeof nm);
    if (W.dirty || strcmp(nm, W.name0)) { U->notice(TR("Discard your changes?"), TR("Discard"), TR("Keep editing"), discard_ok, NULL, NULL); return; }
    mu_log("builder: closed without saving"); close_win();
}

/* a click: the action JSON goes to the model, the answer redraws */
static void act(const char *json) {
    if (!W.open || eng != ENG_READY || W.saving) return;
    W.dirty = 1;
    post(J_ACT, json);
}
static void act_k(const char *t, const char *key) {
    char a[400], k[300]; jesc(k, sizeof k, key ? key : "");
    snprintf(a, sizeof a, "{\"t\":\"%s\",\"k\":\"%s\"}", t, k); act(a);
}

/* ------------------------------------------------------------------ Save */
static void finish_save(Job *b) {
    char e[500] = "", m[900];
    jv *lib = mu_load(e, sizeof e);
    jv *bv = b->build ? jparse(b->build, strlen(b->build), e, sizeof e) : NULL;
    char name[96] = ""; if (W.f_name) U->field_get(W.f_name, name, sizeof name);
    char *p = name; while (*p == ' ') p++;
    size_t L = strlen(p); while (L && p[L - 1] == ' ') p[--L] = 0;
    char id[17] = ""; int rc = -1;
    if (!lib) snprintf(m, sizeof m, TR("Could not read My Units: %s"), e);
    else if (!bv) { snprintf(m, sizeof m, "%s", TR("This build could not be read.")); mu_log("builder: build could not be read: %s", e); }
    else if (W.unit_id[0] && mu_find(lib, W.unit_id)) {
        /* Edit: that unit keeps its id (its teams keep it); its build and name are replaced */
        wk_build wb;
        if (wk_from_json(bv, &wb, e, sizeof e)) snprintf(m, sizeof m, TR("This unit cannot be saved: %s"), e);
        else {
            jv *u = (jv *)mu_find(lib, W.unit_id);
            jset(u, "build", jcopy(bv));
            if (*p) mu_rename(lib, W.unit_id, p);
            jset(u, "updated", jnum_i((long long)time(NULL) * 1000));
            memcpy(id, W.unit_id, 17); rc = 0;
        }
    } else if (mu_add_build(lib, bv, *p ? p : NULL, id, e, sizeof e)) snprintf(m, sizeof m, TR("This unit cannot be saved: %s"), e);
    else rc = 0;
    if (!rc && mu_save(lib, e, sizeof e)) { snprintf(m, sizeof m, TR("Could not save My Units: %s"), e); rc = -1; }
    if (lib) jfree(lib);
    if (bv) jfree(bv);
    W.saving = 0;
    if (rc) { set_msg(m, MU_C_BAD); mu_log("builder: save failed: %s", m); refresh(); return; }
    mu_log("builder: saved unit %s (%s) %s", id, W.unit_id[0] ? "edited" : "new", b->link ? b->link : "");
    void (*cb)(const char *, void *) = W.saved; void *ud = W.ud;
    close_win();
    if (cb) cb(id, ud);
}
static void on_save(void *ud, DSLObj w) {
    (void)ud; (void)w;
    if (!W.open || eng != ENG_READY || W.saving) return;
    char name[96] = "", k[200], a[300]; if (W.f_name) U->field_get(W.f_name, name, sizeof name);
    jesc(k, sizeof k, name);
    snprintf(a, sizeof a, "{\"t\":\"name\",\"v\":\"%s\"}", k);
    W.saving = 1; set_msg(TR("Saving..."), MU_C_SUB);
    post(J_SAVE, name[0] ? a : NULL);
}
static void on_name(void *ud, DSLObj w) {        /* Enter in the name field: the build's name follows */
    (void)ud; (void)w;
    char name[96] = "", k[200], a[300]; U->field_get(W.f_name, name, sizeof name);
    if (!name[0]) return;
    jesc(k, sizeof k, name); snprintf(a, sizeof a, "{\"t\":\"name\",\"v\":\"%s\"}", k); act(a);
}

static void job_done(bld_job *j) {
    Job *b = (Job *)j;
    if (!b->out && !E) {
        eng = ENG_FAILED; snprintf(eng_msg, sizeof eng_msg, "%s", b->err[0] ? b->err : TR("The wiki's Unit Builder gave no answer."));
        mu_log("builder: engine failed: %s%s%s", eng_msg, eng_why[0] ? " | " : "", eng_why); eng_why[0] = 0;
        W.saving = 0;
        if (W.open) refresh();
    } else {
        eng = ENG_READY;
        if (!b->out) { char m[800]; snprintf(m, sizeof m, "%s", b->err); set_msg(m, MU_C_BAD); mu_log("builder: %s", b->err); W.saving = 0; }
        else if (W.open) {
            char e[200]; jv *s = jparse(b->out, strlen(b->out), e, sizeof e);
            if (s) { if (W.st) jfree(W.st); W.st = s; }
            if (b->kind == J_SAVE) finish_save(b);
            else if (!bld_work_busy()) {                 /* more clicks queued: draw the last answer only */
                if (b->kind == J_LOAD && W.f_name) { char nm[96]; sraw(jget(W.st, "name"), nm, sizeof nm); if (!W.lib_name[0] || W.take_name) U->field_set(W.f_name, nm); W.take_name = 0;
                                                     U->field_get(W.f_name, W.name0, sizeof W.name0); }
                refresh();
            }
        }
    }
    free(b->in); free(b->out); free(b->build); free(b->link); free(b);
}

/* ------------------------------------------------------------------ the game's look */
/* The window is drawn like the game's character sheet (EntityStatusUI, uiprefab bundle of build 25846389): the "Box"
   frame with one flat fill, "Contents" panels under lists and stat groups, "Header" strips under section titles,
   "Devider" lines, and the sheet's "Toggle_Normal" / "Toggle_Activated" frames for the section tabs. The sprites are
   the game's own, found by name among the loaded sprites; one not loaded yet = a flat colour of the same tone. */
enum { SP_CONTENTS, SP_HEADER, SP_DIV, SP_TOG, SP_TOG_ON, SP_N };
static const char *const SPR_NAME[SP_N] = { "Contents", "Header", "Devider", "Toggle_Normal", "Toggle_Activated" };
static uintptr_t spr_h[SP_N];
static Obj spr(int k) {
    Obj o = spr_h[k] ? cm_il_hget(spr_h[k]) : NULL;
    if (cm_alive(o)) return o;
    static time_t last; time_t now = time(NULL);
    if (now - last < 5) return NULL;                     /* not loaded: look again at most every 5 s */
    last = now;
    static cm_m all, name; if (!all.fn) { all = cm_il_method("UnityEngine", "Resources", "FindObjectsOfTypeAll", 1, "System.Type"); name = cm_il_method("UnityEngine", "Object", "get_name", 0, NULL); }
    if (!all.fn || !name.fn) return NULL;
    Obj arr = CM_CALL(all, Obj, Obj, const void *)(cm_il_type_obj(cm_il_class("UnityEngine", "Sprite")), all.mi);
    size_t n = arr ? CM_AT(arr, 0x18, uintptr_t) : 0; int found = 0;
    for (size_t i = 0; i < n; i++) {
        Obj s = CM_AT((char *)arr + 0x20, i * 8, Obj); char nm[64];
        cm_il_utf8(CM_CALL(name, Obj, Obj, const void *)(s, name.mi), nm, sizeof nm);
        for (int j = 0; j < SP_N; j++) if (!strcmp(nm, SPR_NAME[j]) && !cm_alive(spr_h[j] ? cm_il_hget(spr_h[j]) : NULL)) { cm_il_hset(&spr_h[j], s); found++; }
    }
    mu_log("builder: look: %d of %zu loaded sprites matched", found, n);
    o = spr_h[k] ? cm_il_hget(spr_h[k]) : NULL;
    return cm_alive(o) ? o : NULL;
}
/* sprite (or NULL) + tint on a kit widget's Image, stretched (these sprites have no 9-slice border except Contents) */
static void look(DSLObj o, Obj sprite, uint32_t rgba) {
    if (!o) return;
    cm_set_sprite(o, sprite, rgba);
    static cm_m asp; if (!asp.fn) asp = cm_il_method("UnityEngine.UI", "Image", "set_preserveAspect", 1, NULL);
    static void *kim; if (!kim) kim = cm_il_class("UnityEngine.UI", "Image");
    Obj im = kim ? cm_comp(o, kim) : NULL;
    if (im && asp.fn) CM_CALL(asp, void, Obj, bool, const void *)(im, false, asp.mi);
}
/* a decorative panel: the game sprite k, else the flat colour; never a click target */
static DSLObj deco(DSLObj parent, float x, float y, float w, float h, int k, uint32_t flat) {
    DSLObj p = U->panel(parent, x, y, w, h, 0);
    if (!p) return NULL;
    Obj s = k >= 0 ? spr(k) : NULL;
    look(p, s, s ? 0xFFFFFFFFu : flat);
    cm_raycast(p, false);
    return p;
}
/* a kit button's frame (its first Image child: the TitleButton clone's background) drawn with a game sprite */
static int btn_look(DSLObj b, Obj sprite) {
    if (!b || !sprite) return -1;
    static void *kim; if (!kim) kim = cm_il_class("UnityEngine.UI", "Image");
    for (int i = 0, n = cm_child_count(b); i < n; i++) {
        Obj c = cm_child(b, i);
        if (kim && cm_comp(c, kim)) { look(c, sprite, 0xFFFFFFFFu); return 0; }
    }
    return -1;
}
#define C_FILL     0x2D2A31FFu       /* the Box sprite's inner tone (45,42,49): the whole window */
#define C_CONTENTS 0x24212773u       /* "Contents": (36,33,39) at 45 % */
#define C_DIV      0x4A3F3CFFu
static DSLObj clear_holder(DSLObj parent, float x, float y, float w, float h) {   /* an invisible box for children */
    DSLObj p = U->panel(parent, x, y, w, h, 0);
    if (p) { look(p, NULL, 0x00000000u); cm_raycast(p, false); }
    return p;
}

/* ------------------------------------------------------------------ texts */
/* a text that keeps its box: too wide, it shrinks down to 0.75 of its size, then ends in "..." */
static DSLObj text_fit(DSLObj p, float x, float y, float w, float h, const char *s, float size, int flags, uint32_t col) {
    DSLObj t = mu_text(p, x, y, w, h, s, size, flags, col);
    float pw = t ? cm_text_preferred_w(t) : -1;
    if (t && pw > w && w > 0) {
        float ns = size * w / pw; if (ns < size * 0.75f) ns = size * 0.75f;
        U->destroy(t); t = mu_text(p, x, y, w, h, s, ns, flags, col);
    }
    return t;
}
/* the game's name of a key (TEXTKEY_<key>_NAME), else the wiki's */
static void game_name(const char *key, const char *wiki, char *out, size_t n) {
    char tk[200]; snprintf(tk, sizeof tk, "TEXTKEY_%s_NAME", key);
    cm_game_text(key && *key ? tk : NULL, wiki, out, n);
    if (!out[0]) snprintf(out, n, "%s", wiki);
}
static const char *stat_word(const char *k, const char *wiki) { char tk[120]; snprintf(tk, sizeof tk, "TEXTKEY_STATUS_%s_NAME", k); return cm_gw(tk, wiki); }
static const char *rarity_word(const char *r) { char tk[80]; snprintf(tk, sizeof tk, "TEXTKEY_RarityType_%s", r); return cm_gw(tk, r); }
static const char *tier_word(const char *t, const char *wiki) { char tk[80]; snprintf(tk, sizeof tk, "TEXTKEY_MajorStat_%s_NAME", t); return cm_gw(tk, wiki); }
static const char *weapon_family(const char *sub, const char *wiki) {     /* item type: the game's equipment word */
    char tk[80]; snprintf(tk, sizeof tk, "TEXTKEY_Equipment_%s_NAME", sub); return cm_gw(tk, wiki && *wiki ? wiki : sub);
}
static const char *slot_word(const char *slot, const char *wiki) { char tk[80]; snprintf(tk, sizeof tk, "TEXTKEY_EquipmentType_%s", slot); return cm_gw(tk, wiki); }

/* ------------------------------------------------------------------ filters (owner 2026-10-17: search and filters like the wiki's builder) */
enum { F_BG_W, F_BG_A, F_BG_B, F_GEAR_TYPE, F_GEAR_RAR, F_SK_TYPE, F_SK_STATE, F_IN_RAR, F_IN_CAT, F_IN_TREE, F_FACE, F_N };
#define FV 48
static struct { bld_pick pk[F_N]; char val[F_N][FV][64]; int n[F_N]; char q[SEC_N][96]; } FL;   /* pk: the chosen values (bld_filter.h) */
static void fl_reset(void) { memset(&FL, 0, sizeof FL); }
static int fl_add(int f, const char *k) {                  /* a value of filter f (deduplicated); its index */
    for (int i = 0; i < FL.n[f]; i++) if (!strcmp(FL.val[f][i], k)) return i;
    if (FL.n[f] >= FV) return -1;
    snprintf(FL.val[f][FL.n[f]], sizeof FL.val[f][0], "%s", k); return FL.n[f]++;
}
static int fl_pass(int f, const char *k) { return bld_pick_any(&FL.pk[f], k); }
static int ci_has(const char *hay, const char *q) {       /* case-insensitive (ASCII) substring */
    if (!q || !*q) return 1;
    if (!hay) return 0;
    size_t nq = strlen(q);
    for (const char *h = hay; *h; h++) {
        size_t i = 0;
        while (i < nq && h[i] && ((unsigned char)h[i] < 128 ? (char)tolower((unsigned char)h[i]) : h[i]) == ((unsigned char)q[i] < 128 ? (char)tolower((unsigned char)q[i]) : q[i])) i++;
        if (i == nq) return 1;
    }
    return 0;
}
static int q_match(const char *a, const char *b, const char *c) {
    const char *q = FL.q[W.section]; while (*q == ' ') q++;
    if (!*q) return 1;
    char t[96]; snprintf(t, sizeof t, "%s", q); size_t L = strlen(t); while (L && t[L - 1] == ' ') t[--L] = 0;
    return ci_has(a, t) || ci_has(b, t) || ci_has(c, t);
}

/* ------------------------------------------------------------------ drawing helpers */
/* a button with a game icon on its left and a one-line label (never spills: ellipsis); state colours the label */
enum { ST_OFF, ST_ON, ST_LOCKED };
static DSLObj icon_button(DSLObj parent, float x, float y, float w, float h, int kind, const char *key, const char *label,
                          const char *sub, int state, dsl_ui_cb cb, void *ud) {
    DSLObj b = mu_button(parent, x, y, w, h, "", cb, ud);
    if (!b) return NULL;
    if (state == ST_ON) {                                   /* chosen: a gold edge and a faint gold wash (flat, no sprite) */
        DSLObj wash = U->panel(b, 3, 3, w - 6, h - 6, 0); if (wash) { cm_set_sprite(wash, NULL, 0xD9BE8F26u); cm_raycast(wash, false); }
        DSLObj bar = U->panel(b, 3, 3, 4, h - 6, 0); if (bar) { cm_set_sprite(bar, NULL, MU_C_GOLD); cm_raycast(bar, false); }
    }
    float s = h - 12 > 40 ? 40 : h - 12, tx = 10;
    if (kind) { mu_icon(b, 10, (h - s) / 2, s, kind, key, state == ST_LOCKED, NULL); tx = 10 + s + 8; }
    uint32_t col = state == ST_ON ? MU_C_GOLD : state == ST_LOCKED ? MU_C_SUB : MU_C_MAIN;
    if (sub && *sub) {   /* boxes tall enough for the CJK faces' line height (a line that does not fit is not drawn) */
        text_fit(b, tx, 1, w - tx - 6, h * 0.56f, label, 15, DSL_TXT_LEFT, col);
        mu_text(b, tx, h * 0.5f, w - tx - 6, h * 0.5f - 1, sub, 12, DSL_TXT_LEFT, MU_C_SUB);
    } else text_fit(b, tx, 0, w - tx - 6, h, label, 15, DSL_TXT_LEFT, col);
    if (state == ST_LOCKED) U->set_enabled(b, 0);
    return b;
}
/* a small text button that grows with its label (filters, tiers, quick actions); on = gold like a chosen entry */
static float label_w(const char *s, float size) {          /* a first guess, the label is measured after */
    float w = 0; for (const unsigned char *p = (const unsigned char *)s; *p; p++) { if ((*p & 0xC0) == 0x80) continue; w += *p < 0x80 ? size * 0.56f : size * 1.05f; }
    return w;
}
static DSLObj chip(DSLObj p, float x, float y, float w, float h, const char *label, int on, dsl_ui_cb cb, void *ud, float *used) {
    int grow = w <= 0;
    if (w <= 0) w = label_w(label, 14) + 26;
    DSLObj b = mu_button(p, x, y, w, h, "", cb, ud);
    if (!b) { if (used) *used = w; return NULL; }
    DSLObj t = mu_text(b, 8, 0, w - 16, h, label, 14, DSL_TXT_CENTER, on ? MU_C_GOLD : MU_C_MAIN);
    float pw = t ? cm_text_preferred_w(t) : -1;
    if (t && pw > w - 16 && grow) {                          /* the box grows to its label; a fixed cell (talent tier,
                                                                elixir +/-) keeps its size and its label shrinks / ends in "..." */
        w = pw + 18;
        U->place(b, x, y, w, h); U->place(t, 8, 0, w - 16, h);
    }
    if (on && btn_look(b, spr(SP_TOG_ON))) {                 /* chosen: the sheet's active tab frame, else a gold wash */
        DSLObj wash = U->panel(b, 3, 3, w - 6, h - 6, 0);
        if (wash) { cm_set_sprite(wash, NULL, 0xD9BE8F33u); cm_raycast(wash, false); cm_set_sibling(wash, 0); }
    }
    if (used) *used = w;
    return b;
}
/* a section title on the game's "Header" strip (character sheet: "Combat Stat") */
static float heading(DSLObj p, float y, float w, const char *t) {
    deco(p, 0, y + 4, w, 26, SP_HEADER, C_CONTENTS);
    text_fit(p, 10, y, w - 20, 34, t, 16, DSL_TXT_LEFT, MU_C_MAIN);
    return y + 38;
}
/* a wrapped note; its box grows to its text (long DE / RU / FR lines, a narrow window), so it never runs over the next row */
static float note(DSLObj p, float y, float w, const char *t, float h) {
    DSLObj o = mu_text(p, 6, y, w - 12, h, t, 14, DSL_TXT_WRAP, MU_C_SUB);
    float th = o && U->size >= offsetof(DSLUi, text_height) + sizeof U->text_height && U->text_height ? U->text_height(o, w - 12) : 0;
    if (th > h - 2) { h = th + 4; U->place(o, 6, y, w - 12, h); }
    return y + h + 4;
}
/* key carried by a widget callback: index into the state's array for that section */
#define UD(i) ((void *)(intptr_t)(i))
#define IDX(ud) ((int)(intptr_t)(ud))
static const jv *arr(const char *k) { jv *a = W.st ? jget(W.st, k) : NULL; return a && a->t == JN_ARR ? a : NULL; }
static const jv *arr_in(const jv *o, const char *k) { jv *a = o ? jget(o, k) : NULL; return a && a->t == JN_ARR ? a : NULL; }
static float LW;                                             /* the list's width (the scroll content) */
static char REGP = '+';                                      /* test names: '+' the list, '=' the filter bar, '~' the sheet */
static void reg_forget(char c) { int k = 0; for (int i = 0; i < W.nreg; i++) if (W.reg[i].name[0] != c) W.reg[k++] = W.reg[i]; W.nreg = k; }
static void regp(const char *name, DSLObj o) { char rn[160]; snprintf(rn, sizeof rn, "%c%s", REGP, name); reg(rn, o); }

/* ------------------------------------------------------------------ clicks */
static void click_list(const char *list, const char *t, void *ud) {
    const jv *a = arr(list); int i = IDX(ud);
    if (!a || i < 0 || (size_t)i >= a->cnt) return;
    char k[200]; sraw(jget(a->v[i], "k"), k, sizeof k); act_k(t, k);
}
static void on_race(void *ud, DSLObj w) { (void)w; click_list("races", "race", ud); }
static void on_bg(void *ud, DSLObj w) { (void)w; click_list("bgs", "bg", ud); }
static void on_ind(void *ud, DSLObj w) { (void)w; click_list("inds", "ind", ud); }
static void on_tree(void *ud, DSLObj w) { (void)w; click_list("treeCands", "tree", ud); }
static void on_insc(void *ud, DSLObj w) { (void)w; click_list("inscs", "insc", ud); }
static void on_acq(void *ud, DSLObj w) { (void)w; click_list("acq", "acq", ud); }
static void on_soul(void *ud, DSLObj w) { (void)w; click_list("souls", "soul", ud); }
static void on_face(void *ud, DSLObj w) {
    (void)w; int i = IDX(ud);
    if (i < 0) { act("{\"t\":\"portrait\",\"k\":null}"); return; }
    click_list("portraits", "portrait", ud);
}
static void on_skill(void *ud, DSLObj w) {
    (void)w; int v = IDX(ud), t = v >> 16, r = (v >> 4) & 0xFFF, c = v & 15;
    const jv *trees = arr("trees"); if (!trees || (size_t)t >= trees->cnt) return;
    jv *rows = jget(trees->v[t], "rows"); if (!rows || (size_t)r >= rows->cnt) return;
    jv *row = rows->v[r]; if (!row || row->t != JN_ARR || (size_t)c >= row->cnt) return;
    char k[200]; sraw(jget(row->v[c], "k"), k, sizeof k); act_k("skill", k);
}
static void on_slot(void *ud, DSLObj w) { (void)w; W.open_slot = W.open_slot == IDX(ud) ? -1 : IDX(ud); FL.pk[F_GEAR_TYPE].n = 0; refresh(); }
static void on_item(void *ud, DSLObj w) {
    (void)w; int v = IDX(ud), s = v >> 12, i = (v & 0xFFF) - 1;
    const jv *eq = arr("eq"); if (!eq || (size_t)s >= eq->cnt) return;
    char slot[40], k[200] = "", kk[300], a[400]; sraw(jget(eq->v[s], "slot"), slot, sizeof slot);
    jv *items = jget(eq->v[s], "items");
    if (i >= 0) { if (!items || (size_t)i >= items->cnt) return; sraw(jget(items->v[i], "k"), k, sizeof k); }
    jesc(kk, sizeof kk, k);
    if (i >= 0) snprintf(a, sizeof a, "{\"t\":\"eq\",\"slot\":\"%s\",\"k\":\"%s\"}", slot, kk);
    else snprintf(a, sizeof a, "{\"t\":\"eq\",\"slot\":\"%s\",\"k\":null}", slot);
    W.open_slot = -1;
    act(a);
}
static void on_roll(void *ud, DSLObj w) {
    int v = IDX(ud), s = v >> 4, r = v & 15;
    const jv *eq = arr("eq"); if (!eq || (size_t)s >= eq->cnt) return;
    char slot[40], a[200]; sraw(jget(eq->v[s], "slot"), slot, sizeof slot);
    snprintf(a, sizeof a, "{\"t\":\"roll\",\"slot\":\"%s\",\"r\":%d,\"i\":%d}", slot, r, U->value_get(w) - 1);
    act(a);
}
/* level-ups: offering, stat gain, talents, elixirs */
static void on_offer(void *ud, DSLObj w) { (void)w; const jv *a = arr_in(jget(W.st, "lv"), "offers"); int i = IDX(ud); if (!a || (size_t)i >= a->cnt) return; char k[60]; sraw(jget(a->v[i], "k"), k, sizeof k); char j[120]; snprintf(j, sizeof j, "{\"t\":\"offer\",\"v\":\"%s\"}", k); act(j); }
static const char *const GAIN[3] = { "min", "avg", "max" };
static void on_gain(void *ud, DSLObj w) { (void)w; char j[80]; snprintf(j, sizeof j, "{\"t\":\"gain\",\"v\":\"%s\"}", GAIN[IDX(ud) % 3]); act(j); }
static void on_tal(void *ud, DSLObj w) {
    (void)w; int v = IDX(ud), m = v >> 8, t = v & 255;
    const jv *tl = arr("tal"), *ti = arr("tiers"); if (!tl || !ti || (size_t)m >= tl->cnt || (size_t)t >= ti->cnt) return;
    char mk[40], tk[40], j[160]; sraw(jget(tl->v[m], "m"), mk, sizeof mk); sraw(jget(ti->v[t], "k"), tk, sizeof tk);
    snprintf(j, sizeof j, "{\"t\":\"tal\",\"m\":\"%s\",\"v\":\"%s\"}", mk, tk); act(j);
}
static void on_tal_all(void *ud, DSLObj w) {
    (void)w; int i = IDX(ud);
    if (i < 0) { act("{\"t\":\"talRandom\"}"); return; }
    char j[120]; snprintf(j, sizeof j, "{\"t\":\"talAll\",\"v\":\"%s\"}", i == 0 ? "Moderate" : i == 1 ? "Genius" : "Poor"); act(j);
}
static void on_elix(void *ud, DSLObj w) {
    (void)w; int v = IDX(ud), i = v >> 1, up = v & 1;
    const jv *e = arr("elix"); if (!e || (size_t)i >= e->cnt) return;
    char m[40], j[120]; sraw(jget(e->v[i], "m"), m, sizeof m);
    snprintf(j, sizeof j, "{\"t\":\"elix\",\"m\":\"%s\",\"v\":%d}", m, num(e->v[i], "uses") + (up ? 1 : -1)); act(j);
}
/* presets: a whole unit from the Wiki Build Gallery (its picker hands back one unit's wiki build; lane cmgallery) */
extern void cm_gallery_pick(void (*picked)(const char *build_json, void *ud), void *ud) MU_WEAK_IMPORT;
static void gal_picked(const char *build_json, void *ud) {
    (void)ud;
    if (!W.open || !build_json || !*build_json) return;
    W.take_name = 1; W.open_slot = -1; W.dirty = 1;
    set_msg(TR("Loaded from the Wiki Build Gallery. Change anything, then Save."), MU_C_SUB);
    post(J_LOAD, build_json);
}
static void on_gal(void *ud, DSLObj w) { (void)ud; (void)w; if (cm_gallery_pick && W.open && eng == ENG_READY) cm_gallery_pick(gal_picked, NULL); }
/* presets: gear quick sets */
static void on_weapon(void *ud, DSLObj w) { (void)w; const jv *a = arr_in(jget(W.st, "sets"), "weapons"); int i = IDX(ud); if (!a || (size_t)i >= a->cnt) return; char k[60], j[120]; sraw(jget(a->v[i], "k"), k, sizeof k); snprintf(j, sizeof j, "{\"t\":\"weapon\",\"v\":\"%s\"}", k); act(j); }
static void on_shield(void *ud, DSLObj w) { (void)ud; char j[60]; snprintf(j, sizeof j, "{\"t\":\"shield\",\"v\":%d}", U->toggle_get(w) ? 1 : 0); act(j); }
static void on_qset(void *ud, DSLObj w) { (void)w; char j[60]; snprintf(j, sizeof j, "{\"t\":\"qset\",\"i\":%d}", IDX(ud)); act(j); }
/* a filter chip or value row: ud = filter << 8 | value + 1 (0 = all) */
static void on_chip(void *ud, DSLObj w) {
    (void)w; int f = IDX(ud) >> 8, i = (IDX(ud) & 255) - 1;
    if (f < 0 || f >= F_N) return;
    if (i < 0 || i >= FL.n[f]) FL.pk[f].n = 0;                 /* "All" */
    else bld_pick_toggle(&FL.pk[f], FL.val[f][i]);            /* several values at once */
    W.bar_sec = -1;                                      /* the bar shows the chip states: draw it again */
    refresh();
}

/* a single-choice value row "Label  < Any >" over filter f's values (the wiki's bonus select: one stat) */
static void on_vrow(void *ud, DSLObj w) {
    int f = IDX(ud), i = U->value_get(w) - 1;
    if (f < 0 || f >= F_N) return;
    FL.pk[f].n = 0;
    if (i >= 0 && i < FL.n[f]) bld_pick_toggle(&FL.pk[f], FL.val[f][i]);
    refresh();                                           /* the row shows its own value: only the list changes */
}
static void filter_row(DSLObj p, float x, float y, float w, const char *label, int f, const char *const *shown) {
    const char *vals[FV + 1]; int cur = 0;
    vals[0] = TR("Any");
    for (int i = 0; i < FL.n[f]; i++) { vals[i + 1] = shown && shown[i] ? shown[i] : FL.val[f][i]; if (bld_pick_has(&FL.pk[f], FL.val[f][i])) cur = i + 1; }
    DSLObj v = U->value_row(p, x, y, w, label, vals, FL.n[f] + 1, cur, on_vrow, UD(f));
    char rn[40]; snprintf(rn, sizeof rn, "filter:%d", f); regp(rn, v);
}
/* chips laid out left to right, a new line when the next one does not fit (each is measured: no box cuts its label) */
typedef struct { DSLObj p; float x0, x, y, w, h; } Flow;
static void flow_init(Flow *f, DSLObj p, float x0, float y, float w, float h) { f->p = p; f->x0 = f->x = x0; f->y = y; f->w = w; f->h = h; }
static void flow_fit(Flow *f, DSLObj o, float used) {
    if (f->x > f->x0 && f->x + used > f->w) { f->x = f->x0; f->y += f->h + 6; if (o) U->place(o, f->x, f->y, used, f->h); }
}
static DSLObj flow_chip(Flow *f, const char *label, int on, dsl_ui_cb cb, void *ud) {
    float used; DSLObj b = chip(f->p, f->x, f->y, 0, f->h, label, on, cb, ud, &used);
    flow_fit(f, b, used); f->x += used + 6;
    return b;
}
static void flow_label(Flow *f, const char *label) {
    if (f->x > f->x0) f->x += 10;
    DSLObj t = mu_text(f->p, f->x, f->y, 400, f->h, label, 14, DSL_TXT_LEFT, MU_C_SUB);
    float pw = t ? cm_text_preferred_w(t) : label_w(label, 14); if (pw < 0) pw = 0;
    flow_fit(f, t, pw + 2);
    if (t) U->place(t, f->x, f->y, pw + 2, f->h);
    f->x += pw + 10;
}
static float flow_end(const Flow *f) { return f->y + f->h + 6; }
/* the chips of filter f: "Label: [All] [a] [b] ..." over its values (FL.val) */
static void chip_group(Flow *fl, const char *label, int f, const char *const *shown) {
    if (label && *label) flow_label(fl, label);
    DSLObj b = flow_chip(fl, TR("All"), !FL.pk[f].n, on_chip, UD(f << 8));
    { char rn[60]; snprintf(rn, sizeof rn, "chip:%d:all", f); regp(rn, b); }
    for (int i = 0; i < FL.n[f]; i++) {
        b = flow_chip(fl, shown && shown[i] ? shown[i] : FL.val[f][i], bld_pick_has(&FL.pk[f], FL.val[f][i]), on_chip, UD((f << 8) | (i + 1)));
        char rn[120]; snprintf(rn, sizeof rn, "chip:%d:%s", f, FL.val[f][i]); regp(rn, b);
    }
}
static float chip_row(DSLObj p, float y, float w, const char *label, int f, const char *const *shown) {
    Flow fl; flow_init(&fl, p, 6, y, w - 6, 32); chip_group(&fl, label, f, shown); return flow_end(&fl);
}

/* ------------------------------------------------------------------ the filter bar (above the list; it stays while
   the list redraws, so typing in its search box never loses the focus) */
static void on_search(void *ud, DSLObj w) { (void)ud; (void)w; W.search_wait = 1; }   /* Enter: filter now */
static float search_box(DSLObj p, float y, float w) {
    char ph[160]; snprintf(ph, sizeof ph, "%s", cm_gw("TEXTKEY_ConsumePolicyUI_Search", "Search..."));
    W.f_search = U->field(p, 6, y, w - 12 > 420 ? 420 : w - 12, 36, ph, FL.q[W.section], on_search, NULL);
    regp("search", W.f_search);
    return y + 42;
}
static float bar_bg(DSLObj p, float y, float w) {
    /* filter values from the state: the trees and bonus stats the listed backgrounds give */
    FL.n[F_BG_W] = FL.n[F_BG_A] = FL.n[F_BG_B] = 0;
    const jv *a = arr("bgs"); jv *tn = W.st ? jget(W.st, "treeNames") : NULL;
    for (size_t i = 0; a && i < a->cnt; i++) {
        const jv *x;
        if ((x = arr_in(a->v[i], "tw"))) for (size_t j = 0; j < x->cnt; j++) { char k[64]; sraw(x->v[j], k, sizeof k); fl_add(F_BG_W, k); }
        if ((x = arr_in(a->v[i], "ta"))) for (size_t j = 0; j < x->cnt; j++) { char k[64]; sraw(x->v[j], k, sizeof k); fl_add(F_BG_A, k); }
        if ((x = arr_in(a->v[i], "bon"))) for (size_t j = 0; j < x->cnt; j++) { char k[64]; sraw(x->v[j], k, sizeof k); fl_add(F_BG_B, k); }
    }
    static char names[3][FV][120]; const char *shown[3][FV];
    for (int f = 0; f < 3; f++) for (int i = 0; i < FL.n[F_BG_W + f]; i++) {
        const char *k = FL.val[F_BG_W + f][i];
        if (f < 2) { jv *t = tn ? jget(tn, k) : NULL; sraw(t ? jget(t, "n") : NULL, names[f][i], sizeof names[f][i]); if (!names[f][i][0]) snprintf(names[f][i], sizeof names[f][i], "%s", k); }
        else {                                           /* the game's stat name, its [Shared_...] link marks removed */
            const char *sw = stat_word(k, k); size_t o = 0;
            for (const char *c = sw; *c && o + 1 < sizeof names[f][i]; c++) {
                if (*c == '[') { const char *e = strchr(c, ']'); if (e) { c = e; continue; } }
                names[f][i][o++] = *c;
            }
            while (o && names[f][i][o - 1] == ' ') o--;
            names[f][i][o] = 0;
        }
        shown[f][i] = names[f][i];
    }
    y = search_box(p, y, w);
    /* chips, several at once: a background must grant every chosen tree (the wiki's tree chips); one bonus stat (the wiki's bonus select) */
    y = chip_row(p, y, w, TR("Weapon trees"), F_BG_W, shown[0]);
    y = chip_row(p, y, w, TR("Auxiliary trees"), F_BG_A, shown[1]);
    filter_row(p, 6, y, w * 0.6f, TR("Bonus"), F_BG_B, shown[2]);
    return y + 46;
}
static float bar_gear(DSLObj p, float y, float w) {
    const jv *eq = arr("eq"); if (!eq || W.open_slot < 0 || (size_t)W.open_slot >= eq->cnt) return y;
    const jv *items = arr_in(eq->v[W.open_slot], "items");
    FL.n[F_GEAR_TYPE] = FL.n[F_GEAR_RAR] = 0;
    for (size_t i = 0; items && i < items->cnt; i++) { char s[64]; sraw(jget(items->v[i], "sub"), s, sizeof s); if (s[0]) fl_add(F_GEAR_TYPE, s); }
    static const char *const RAR[] = { "Common", "Uncommon", "Rare", "Legendary" };   /* RarityType order */
    for (int r = 0; r < 4; r++) for (size_t i = 0; items && i < items->cnt; i++) { char s[64]; sraw(jget(items->v[i], "r"), s, sizeof s); if (!strcmp(s, RAR[r])) { fl_add(F_GEAR_RAR, s); break; } }
    static char tw[FV][120], rw[FV][120]; const char *ts[FV], *rs[FV];
    for (int i = 0; i < FL.n[F_GEAR_TYPE]; i++) { snprintf(tw[i], sizeof tw[i], "%s", weapon_family(FL.val[F_GEAR_TYPE][i], FL.val[F_GEAR_TYPE][i])); ts[i] = tw[i]; }
    for (int i = 0; i < FL.n[F_GEAR_RAR]; i++) { snprintf(rw[i], sizeof rw[i], "%s", rarity_word(FL.val[F_GEAR_RAR][i])); rs[i] = rw[i]; }
    y = search_box(p, y, w);
    if (FL.n[F_GEAR_TYPE] > 1) y = chip_row(p, y, w, TR("Type"), F_GEAR_TYPE, ts);
    if (FL.n[F_GEAR_RAR] > 1) y = chip_row(p, y, w, cm_gw("TEXTKEY_RarityType_Rarity", "Rarity"), F_GEAR_RAR, rs);
    return y;
}
static float bar_skills(DSLObj p, float y, float w) {
    FL.n[F_SK_TYPE] = FL.n[F_SK_STATE] = 0;
    fl_add(F_SK_TYPE, "active"); fl_add(F_SK_TYPE, "passive"); fl_add(F_SK_STATE, "learned"); fl_add(F_SK_STATE, "can");
    const char *ts[] = { TR("Active"), TR("Passive") }, *ss[] = { TR("Learned"), TR("Can learn") };
    jv *sp = jget(W.st, "sp"); char t[300];
    snprintf(t, sizeof t, TR("Main SP left %d of %d    Sub SP left %d of %d"), num(sp, "main"), num(sp, "mainMax"), num(sp, "sub"), num(sp, "subMax"));
    y = note(p, y, w, t, 22);
    y = search_box(p, y, w);
    Flow fl; flow_init(&fl, p, 6, y, w - 6, 32);
    chip_group(&fl, NULL, F_SK_TYPE, ts); fl.x += 24; chip_group(&fl, NULL, F_SK_STATE, ss);
    return flow_end(&fl);
}
static float bar_insc(DSLObj p, float y, float w) {
    jv *in = jget(W.st, "insc"); char t[600];
    snprintf(t, sizeof t, TR("One inscription per level: %d by level %d. %d spent on extra skill trees, %d on inscriptions, so %d free."),
             num(in, "slots"), num(W.st, "lvl"), num(in, "trees"), num(in, "used"), num(in, "free"));
    y = note(p, y, w, t, 40);
    FL.n[F_IN_RAR] = FL.n[F_IN_CAT] = 0;
    const jv *a = arr("inscs");
    static const char *const RAR[] = { "Common", "Uncommon", "Rare", "Legendary" };
    for (int r = 0; r < 4; r++) for (size_t i = 0; a && i < a->cnt; i++) { char s[64]; sraw(jget(a->v[i], "r"), s, sizeof s); if (!strcmp(s, RAR[r])) { fl_add(F_IN_RAR, s); break; } }
    for (size_t i = 0; a && i < a->cnt; i++) { char s[64]; sraw(jget(a->v[i], "cat"), s, sizeof s); if (s[0]) fl_add(F_IN_CAT, s); }
    static char rw[FV][120]; const char *rs[FV], *cs[FV];
    for (int i = 0; i < FL.n[F_IN_RAR]; i++) { snprintf(rw[i], sizeof rw[i], "%s", rarity_word(FL.val[F_IN_RAR][i])); rs[i] = rw[i]; }
    for (int i = 0; i < FL.n[F_IN_CAT]; i++) {                       /* the wiki's inscription categories */
        const char *c = FL.val[F_IN_CAT][i];
        cs[i] = !strcmp(c, "attack") ? TR("Attack") : !strcmp(c, "special") ? TR("Special") : !strcmp(c, "survival") ? TR("Survival") : c;
    }
    y = search_box(p, y, w);
    Flow fl; flow_init(&fl, p, 6, y, w - 6, 32);
    chip_group(&fl, cm_gw("TEXTKEY_RarityType_Rarity", "Rarity"), F_IN_RAR, rs);
    chip_group(&fl, TR("Kind"), F_IN_CAT, cs);
    return flow_end(&fl);
}
static void draw_bar(void) {
    if (W.bar) { U->destroy(W.bar); W.bar = NULL; }
    reg_forget('=');
    REGP = '=';
    W.f_search = NULL;
    float w = W.mw - 16;
    W.bar = clear_holder(W.win, W.mx + 8, W.my + 6, w, 400);
    if (!W.bar) { W.bar_h = 0; return; }
    float y = heading(W.bar, 0, w, sec_label(W.section));
    if (eng == ENG_READY && W.st) switch (W.section) {
        case SEC_BG: y = bar_bg(W.bar, y, w); break;
        case SEC_TRAITS: y = search_box(W.bar, y, w); break;
        case SEC_SKILLS: y = bar_skills(W.bar, y, w); break;
        case SEC_GEAR: y = bar_gear(W.bar, y, w); break;
        case SEC_INSC: y = bar_insc(W.bar, y, w); break;
        default: break;
    }
    U->place(W.bar, W.mx + 8, W.my + 6, w, y);
    W.bar_h = y + 8;
    W.bar_sec = W.section; W.bar_slot = W.section == SEC_GEAR ? W.open_slot : -2;
    REGP = '+';
}

/* ------------------------------------------------------------------ sections */
/* a grid of icon buttons from one of the state's lists (only the entries the search and pass() keep); returns the y below */
typedef struct { const char *list; int kind; int cols; float h; dsl_ui_cb cb; const char *rprefix; int locked_when_full; int (*pass)(const jv *o); const char *sub_key; } Grid;
static float grid(DSLObj p, float y, const Grid *g, int full) {
    const jv *a = arr(g->list); if (!a) return y;
    float cw = (LW - 8 - (g->cols - 1) * 8) / g->cols; int shown = 0;
    for (size_t i = 0; i < a->cnt; i++) {
        jv *o = a->v[i]; char k[200], wn[200], n[200], d[1200], rn[220], sub[300] = "";
        sraw(jget(o, "k"), k, sizeof k); sraw(jget(o, "n"), wn, sizeof wn); sraw(jget(o, "d"), d, sizeof d);
        if (g->sub_key) { char s[200]; sraw(jget(o, g->sub_key), s, sizeof s); if (s[0]) snprintf(sub, sizeof sub, TR("from %s"), s); }
        game_name(k, wn, n, sizeof n);
        if (!q_match(n, wn, d) || (g->pass && !g->pass(o))) continue;
        int on = truthy(o, "on"), ok = jget(o, "ok") ? truthy(o, "ok") : 1;
        if (g->locked_when_full && !on && !full) ok = 0;
        float x = 4 + (shown % g->cols) * (cw + 8), yy = y + (shown / g->cols) * (g->h + 6);
        shown++;
        DSLObj b = icon_button(p, x, yy, cw, g->h, g->kind, k, n, sub, on ? ST_ON : ok ? ST_OFF : ST_LOCKED, g->cb, UD(i));
        if (b) {
            char tr[300] = ""; sraw(jget(o, "trees"), tr, sizeof tr);
            char trl[400] = ""; if (tr[0]) snprintf(trl, sizeof trl, TR("Skill trees: %s"), tr);
            char body[1600]; snprintf(body, sizeof body, "%s%s%s", d, trl[0] && d[0] ? "\n" : "", trl);
            if (body[0]) U->tooltip(b, n, body);
            snprintf(rn, sizeof rn, "+%s:%s", g->rprefix, k); reg(rn, b);
        }
    }
    if (!shown) return note(p, y, LW, TR("Nothing matches the search and filters."), 24);
    return y + ((shown + g->cols - 1) / g->cols) * (g->h + 6);
}

static float draw_race(DSLObj p, float y) {
    Grid g = { "races", CM_ICON_AFFECTER, 2, 56, on_race, "race", 0, NULL, NULL };
    y = grid(p, y, &g, 1) + 10;
    /* portrait (renderPortrait): the race's faces, a gender filter when it has both */
    const jv *a = arr("portraits");
    y = heading(p, y, LW, TR("Portrait"));
    if (!a || !a->cnt) return note(p, y, LW, TR("No portraits for this race in the wiki's data yet."), 24);
    int male = 0, female = 0;
    for (size_t i = 0; i < a->cnt; i++) { char g2[16]; sraw(jget(a->v[i], "g"), g2, sizeof g2); male |= !strcmp(g2, "male"); female |= !strcmp(g2, "female"); }
    FL.n[F_FACE] = 0;
    if (male && female) {
        fl_add(F_FACE, "male"); fl_add(F_FACE, "female");
        const char *gs[] = { TR("Male"), TR("Female") };
        y = chip_row(p, y, LW, NULL, F_FACE, gs);
    }
    float s = 72; int cols = (int)((LW - 8) / (s + 8)); if (cols < 1) cols = 1;
    int n = 0;
    DSLObj none = chip(p, 4, y, s, s, TR("None"), !jget(W.st, "portrait") || jget(W.st, "portrait")->t != JN_STR, on_face, UD(-1), NULL);
    reg("+face:none", none);
    n++;
    for (size_t i = 0; i < a->cnt; i++) {
        char k[120], g2[16]; sraw(jget(a->v[i], "k"), k, sizeof k); sraw(jget(a->v[i], "g"), g2, sizeof g2);
        if (!bld_pick_any(&FL.pk[F_FACE], g2) && strcmp(g2, "other")) continue;
        float x = 4 + (n % cols) * (s + 8), yy = y + (n / cols) * (s + 8); n++;
        DSLObj b = mu_button(p, x, yy, s, s, "", on_face, UD(i));
        if (!b) continue;
        int on = truthy(a->v[i], "on");
        if (on) { DSLObj wash = U->panel(b, 2, 2, s - 4, s - 4, 0); if (wash) { cm_set_sprite(wash, NULL, 0xD9BE8F40u); cm_raycast(wash, false); } }
        mu_icon(b, 6, 6, s - 12, CM_ICON_PORTRAIT, k, 0, NULL);
        char rn[160]; snprintf(rn, sizeof rn, "+face:%s", k); reg(rn, b);
    }
    return y + ((n + cols - 1) / cols) * (s + 8);
}
static int bg_pass(const jv *o) { return bld_bg_pass(o, &FL.pk[F_BG_W], &FL.pk[F_BG_A], &FL.pk[F_BG_B]); }
static float draw_bg(DSLObj p, float y) {
    y = note(p, y, LW, TR("The background decides the unit's skill trees."), 22);
    Grid g = { "bgs", CM_ICON_AFFECTER, 2, 48, on_bg, "bg", 0, bg_pass, NULL };
    return grid(p, y, &g, 1);
}
static float draw_traits(DSLObj p, float y) {
    const jv *a = arr("inds"); int on = 0;
    for (size_t i = 0; a && i < a->cnt; i++) on += truthy(a->v[i], "on");
    char t[160]; snprintf(t, sizeof t, TR("Traits (%d of %d)"), on, num(W.st, "indCap"));
    y = note(p, y, LW, t, 22);
    Grid g = { "inds", CM_ICON_AFFECTER, 2, 48, on_ind, "ind", 0, NULL, NULL };
    y = grid(p, y, &g, 1) + 10;
    /* acquired in play (renderAcqTraits) and the soul stone (renderSouls) */
    y = heading(p, y, LW, TR("Acquired traits"));
    y = note(p, y, LW, TR("Traits a unit gains in play by using an item. Add the ones this unit has taken: the stats include them."), 40);
    Grid ga = { "acq", CM_ICON_AFFECTER, 2, 48, on_acq, "acq", 0, NULL, "from" };
    y = grid(p, y, &ga, 1) + 10;
    y = heading(p, y, LW, cm_gw("TEXTKEY_TraitType_Soul", "Soul Trait"));
    y = note(p, y, LW, TR("A unit holds one soul stone: picking another replaces it."), 22);
    Grid gs = { "souls", CM_ICON_AFFECTER, 2, 48, on_soul, "soul", 0, NULL, "from" };
    return grid(p, y, &gs, 1);
}
static float draw_level(DSLObj p, float y) {
    jv *lv = jget(W.st, "lv"); char t[700], u[400];
    /* level: XP and mana stones to reach it (renderLevel) */
    snprintf(t, sizeof t, TR("XP to reach level %d: %d"), num(W.st, "lvl"), num(lv, "xp"));
    y = note(p, y, LW, t, 22);
    const jv *st = arr_in(lv, "stones"); size_t o = 0; u[0] = 0;
    for (size_t i = 0; st && i < st->cnt; i++) {
        char k[80], wn[120], n[160]; sraw(jget(st->v[i], "k"), k, sizeof k); sraw(jget(st->v[i], "n"), wn, sizeof wn); game_name(k, wn, n, sizeof n);
        o += (size_t)snprintf(u + o, o < sizeof u ? sizeof u - o : 0, "%s%d %s", i ? "  \xC2\xB7  " : "", num(st->v[i], "c"), n);
    }
    dsi_utf8_fix_end(u);
    snprintf(t, sizeof t, TR("Mana stones spent to reach it: %s"), u[0] ? u : "-");
    y = note(p, y, LW, t, 40) + 6;
    /* offering */
    y = heading(p, y, LW, TR("Offering at each level-up"));
    const jv *of = arr_in(lv, "offers");
    Flow fo; flow_init(&fo, p, 6, y, LW - 6, 36);
    for (size_t i = 0; of && i < of->cnt; i++) {
        char k[60], wn[80], lab[200], tk[120]; sraw(jget(of->v[i], "k"), k, sizeof k); sraw(jget(of->v[i], "n"), wn, sizeof wn);
        snprintf(tk, sizeof tk, "TEXTKEY_Shared_LevelUp%sOffering_NAME", k);
        double fs = jnumv(jget(of->v[i], "stats")), fi = jnumv(jget(of->v[i], "stones"));
        char a[24], b[24]; snprintf(a, sizeof a, "%g", fs); snprintf(b, sizeof b, "%g", fi);
        snprintf(lab, sizeof lab, TR("%s: stats x%s, mana stones x%s"), cm_gw(tk, wn), a, b);
        DSLObj c = flow_chip(&fo, lab, truthy(of->v[i], "on"), on_offer, UD(i));
        char rn[80]; snprintf(rn, sizeof rn, "+offer:%s", k); reg(rn, c);
    }
    y = flow_end(&fo);
    y = note(p, y, LW, TR("A bigger offering multiplies the stats each level-up gives and the mana stones it costs."), 22) + 6;
    /* stat gain per level */
    y = heading(p, y, LW, TR("Stat gain per level"));
    char gain[16]; sraw(jget(lv, "gain"), gain, sizeof gain);
    const char *gl[3] = { TR("worst"), TR("average"), TR("best") };
    Flow fg; flow_init(&fg, p, 6, y, LW - 6, 36);
    for (int i = 0; i < 3; i++) {
        DSLObj c = flow_chip(&fg, gl[i], !strcmp(gain, GAIN[i]), on_gain, UD(i));
        char rn[40]; snprintf(rn, sizeof rn, "+gain:%s", GAIN[i]); reg(rn, c);
    }
    y = flow_end(&fg) + 2;
    const jv *ti = arr("tiers");
    { char lo[16] = "", hi[16] = "", mn[80] = "Moderate";
      for (size_t i = 0; ti && i < ti->cnt; i++) if (truthy(ti->v[i], "base")) { snprintf(lo, sizeof lo, "%d", num(ti->v[i], "lo")); snprintf(hi, sizeof hi, "%d", num(ti->v[i], "hi")); sraw(jget(ti->v[i], "k"), mn, sizeof mn); }
      snprintf(t, sizeof t, TR("Each level-up adds a random amount to every stat, inside the range of its talent tier (%s: %s to %s per level). Worst takes the lowest amount at every level, average the middle, best the highest."),
               tier_word(mn, mn), lo, hi);
      y = note(p, y, LW, t, 60) + 6; }
    /* talents (renderTalents) */
    y = heading(p, y, LW, cm_gw("TEXTKEY_Shared_Talent_NAME", "Talent"));
    { const char *al[4] = { NULL, NULL, NULL, TR("Randomize") }; char a0[120], a1[120], a2[120];
      snprintf(a0, sizeof a0, TR("All %s"), tier_word("Moderate", "Moderate")); snprintf(a1, sizeof a1, TR("All %s"), tier_word("Genius", "Genius")); snprintf(a2, sizeof a2, TR("All %s"), tier_word("Poor", "Poor"));
      al[0] = a0; al[1] = a1; al[2] = a2;
      Flow fa; flow_init(&fa, p, 6, y, LW - 6, 34);
      for (int i = 0; i < 4; i++) { DSLObj c = flow_chip(&fa, al[i], 0, on_tal_all, UD(i == 3 ? -1 : i)); char rn[40]; snprintf(rn, sizeof rn, "+talall:%d", i); reg(rn, c); }
      y = flow_end(&fa) + 2; }
    const jv *tl = arr("tal");
    float nw = 150, cw = ti && ti->cnt ? (LW - nw - 12 - (ti->cnt - 1) * 6) / ti->cnt : 100;
    for (size_t m = 0; tl && m < tl->cnt; m++) {
        char mk[40], mn[80], cur[40]; sraw(jget(tl->v[m], "m"), mk, sizeof mk); sraw(jget(tl->v[m], "n"), mn, sizeof mn); sraw(jget(tl->v[m], "t"), cur, sizeof cur);
        text_fit(p, 6, y, nw - 8, 34, stat_word(mk, mn), 15, DSL_TXT_LEFT, MU_C_MAIN);
        for (size_t i = 0; ti && i < ti->cnt; i++) {
            char tk[40], tn[80]; sraw(jget(ti->v[i], "k"), tk, sizeof tk); sraw(jget(ti->v[i], "n"), tn, sizeof tn);
            DSLObj c = chip(p, nw + i * (cw + 6), y, cw, 34, tier_word(tk, tn), !strcmp(cur, tk), on_tal, UD(((int)m << 8) | (int)i), NULL);
            if (c) { char tip[200]; snprintf(tip, sizeof tip, TR("+%d to +%d per level"), num(ti->v[i], "lo"), num(ti->v[i], "hi")); U->tooltip(c, tier_word(tk, tn), tip);
                     char rn[120]; snprintf(rn, sizeof rn, "+tal:%s:%s", mk, tk); reg(rn, c); }
        }
        y += 40;
    }
    { jv *ts = jget(W.st, "talSum");
      snprintf(t, sizeof t, TR("Talent sum: %d. A recruit rolls a sum from %d to %d."), num(ts, "v"), num(ts, "lo"), num(ts, "hi"));
      y = note(p, y, LW, t, 22) + 6; }
    /* stat elixirs (renderElixirs) */
    const jv *ex = arr("elix");
    if (ex && ex->cnt) {
        y = heading(p, y, LW, TR("Stat elixirs"));
        y = note(p, y, LW, TR("Each elixir a unit drinks raises one stat for good. Count the ones this unit has taken."), 22);
        for (size_t i = 0; i < ex->cnt; i++) {
            char k[80], wn[120], n[160], m[40], sn[80], line[300];
            sraw(jget(ex->v[i], "k"), k, sizeof k); sraw(jget(ex->v[i], "n"), wn, sizeof wn); sraw(jget(ex->v[i], "m"), m, sizeof m); sraw(jget(ex->v[i], "sn"), sn, sizeof sn);
            game_name(k, wn, n, sizeof n);
            int uses = num(ex->v[i], "uses"), per = num(ex->v[i], "per");
            mu_icon(p, 6, y + 3, 32, CM_ICON_ITEM, k, 0, n);
            snprintf(line, sizeof line, TR("%s  (+%d %s per use)"), n, per, stat_word(m, sn));
            text_fit(p, 46, y, LW - 46 - 230, 38, line, 14, DSL_TXT_LEFT, MU_C_MAIN);
            float bx = LW - 220;
            DSLObj mi = chip(p, bx, y + 2, 40, 34, "-", 0, on_elix, UD((int)i << 1), NULL);
            char c[24]; snprintf(c, sizeof c, "%d", uses);
            mu_text(p, bx + 44, y, 40, 38, c, 16, DSL_TXT_CENTER, uses ? MU_C_GOLD : MU_C_MAIN);
            DSLObj pl = chip(p, bx + 88, y + 2, 40, 34, "+", 0, on_elix, UD(((int)i << 1) | 1), NULL);
            if (uses) { snprintf(c, sizeof c, "= +%d", uses * per); mu_text(p, bx + 134, y, 80, 38, c, 14, DSL_TXT_LEFT, MU_C_GOLD); }
            char rn[80]; snprintf(rn, sizeof rn, "+elix-:%s", m); reg(rn, mi); snprintf(rn, sizeof rn, "+elix+:%s", m); reg(rn, pl);
            y += 42;
        }
    }
    return y;
}
static int sk_pass(const jv *nd) {
    char sn[160], wn[160], d[1200], k[160];
    sraw(jget(nd, "k"), k, sizeof k); sraw(jget(nd, "n"), wn, sizeof wn); sraw(jget(nd, "d"), d, sizeof d); game_name(k, wn, sn, sizeof sn);
    if (!q_match(sn, wn, d)) return 0;
    if (!fl_pass(F_SK_TYPE, truthy(nd, "p") ? "passive" : "active")) return 0;
    char why[200]; sraw(jget(nd, "why"), why, sizeof why);
    if (FL.pk[F_SK_STATE].n && !(bld_pick_has(&FL.pk[F_SK_STATE], "learned") && truthy(nd, "on"))
                            && !(bld_pick_has(&FL.pk[F_SK_STATE], "can") && !truthy(nd, "on") && !why[0])) return 0;
    return 1;
}
static float draw_skills(DSLObj p, float y) {
    const jv *trees = arr("trees");
    if (!trees || !trees->cnt) return note(p, y, LW, TR("Pick a background first. It decides which skill trees this unit has."), 24);
    float cw = (LW - 8 - 16) / 3, ch = 52; int any = 0;
    for (size_t ti = 0; ti < trees->cnt; ti++) {
        jv *tr = trees->v[ti]; char k[100], n[160], head[300];
        jv *rows = jget(tr, "rows"); int vis = 0;
        for (size_t r = 0; rows && r < rows->cnt; r++) { jv *row = rows->v[r];
            for (size_t c = 0; row && row->t == JN_ARR && c < row->cnt; c++) if (row->v[c] && row->v[c]->t == JN_OBJ && sk_pass(row->v[c])) vis++; }
        if (!vis) continue;
        any = 1;
        sraw(jget(tr, "k"), k, sizeof k); sraw(jget(tr, "n"), n, sizeof n);
        int tw = truthy(tr, "w"), ti_ins = truthy(tr, "ins");
        snprintf(head, sizeof head, tw ? (ti_ins ? TR("%s  -  weapon tree, Main SP, from an inscription") : TR("%s  -  weapon tree, Main SP"))
                                       : (ti_ins ? TR("%s  -  auxiliary tree, Sub SP, from an inscription") : TR("%s  -  auxiliary tree, Sub SP")), n);
        deco(p, 0, y + 4, LW, 26, SP_HEADER, C_CONTENTS);
        mu_icon(p, 6, y + 2, 28, CM_ICON_TREE, k, 0, NULL);
        text_fit(p, 40, y, LW - 50, 32, head, 15, DSL_TXT_LEFT, MU_C_MAIN);
        y += 38;
        for (size_t r = 0; rows && r < rows->cnt; r++) {
            jv *row = rows->v[r]; int drawn = 0;
            for (size_t c = 0; row && row->t == JN_ARR && c < row->cnt && c < 3; c++) {
                jv *nd = row->v[c]; if (!nd || nd->t != JN_OBJ || !sk_pass(nd)) continue;
                char sk[160], wn[160], sn[160], why[300], d[1200], sub[300], tip[1700];
                sraw(jget(nd, "k"), sk, sizeof sk); sraw(jget(nd, "n"), wn, sizeof wn); sraw(jget(nd, "why"), why, sizeof why); sraw(jget(nd, "d"), d, sizeof d);
                game_name(sk, wn, sn, sizeof sn);
                int on = truthy(nd, "on"), lv = num(nd, "lv");
                char lvs[64] = "", cost[80], costl[80];
                if (lv > 1) { snprintf(lvs, sizeof lvs, "  \xC2\xB7  "); snprintf(lvs + 6, sizeof lvs - 6, TR("Lv %d"), lv); }
                /* the cell is a third of the list (~150 for its text): its line says "2 SP" (the tree's heading says Main or
                   Sub SP, the gold frame says learned); the full words are in its tooltip */
                snprintf(cost, sizeof cost, TR("%d SP"), num(nd, "cost"));
                snprintf(costl, sizeof costl, truthy(tr, "w") ? TR("%d main SP") : TR("%d sub SP"), num(nd, "cost"));
                snprintf(sub, sizeof sub, "%s%s%s%s", cost, lvs, truthy(nd, "p") ? "  \xC2\xB7  " : "", truthy(nd, "p") ? TR("passive") : "");
                dsi_utf8_fix_end(sub);
                DSLObj b = icon_button(p, 4 + c * (cw + 8), y, cw, ch, CM_ICON_SKILL, sk, sn, sub, on ? ST_ON : why[0] ? ST_LOCKED : ST_OFF,
                                       on_skill, UD(((int)ti << 16) | ((int)r << 4) | (int)c));
                drawn = 1;
                if (b) {
                    char lk[400] = ""; if (why[0]) snprintf(lk, sizeof lk, TR("Locked: %s"), why);
                    char hd[200]; snprintf(hd, sizeof hd, "%s%s%s", on ? TR("Learned") : "", on ? "  \xC2\xB7  " : "", costl);
                    snprintf(tip, sizeof tip, "%s\n%s%s%s", hd, lk, lk[0] && d[0] ? "\n" : "", d);
                    U->tooltip(b, sn, tip);
                    char rn[200]; snprintf(rn, sizeof rn, "+skill:%s", sk); reg(rn, b);
                }
            }
            if (drawn) y += ch + 6;
        }
        y += 8;
    }
    if (!any) y = note(p, y, LW, TR("Nothing matches the search and filters."), 24);
    return y;
}
static int item_pass(const jv *it) {
    char s[64]; sraw(jget(it, "sub"), s, sizeof s); if (!fl_pass(F_GEAR_TYPE, s)) return 0;
    sraw(jget(it, "r"), s, sizeof s); return fl_pass(F_GEAR_RAR, s);
}
static float draw_gear(DSLObj p, float y) {
    const jv *eq = arr("eq"); if (!eq) return y;
    float lab = 150;
    for (size_t s = 0; s < eq->cnt; s++) {
        jv *e = eq->v[s]; char slot[40], label[60], k[160], wn[160], n[160], blocked[300], rn[200];
        sraw(jget(e, "slot"), slot, sizeof slot); sraw(jget(e, "label"), label, sizeof label); sraw(jget(e, "k"), k, sizeof k);
        sraw(jget(e, "n"), wn, sizeof wn); sraw(jget(e, "blocked"), blocked, sizeof blocked);
        game_name(k, wn, n, sizeof n);
        text_fit(p, 6, y, lab - 10, 48, slot_word(slot, label), 14, DSL_TXT_LEFT, MU_C_SUB);
        if (blocked[0] && !k[0]) { mu_text(p, lab, y, LW - lab - 4, 48, blocked, 13, DSL_TXT_WRAP, MU_C_SUB); y += 54; continue; }
        DSLObj b = icon_button(p, lab, y, LW - lab - 4, 48, k[0] ? CM_ICON_ITEM : 0, k, k[0] ? n : TR("(empty)"),
                               W.open_slot == (int)s ? TR("choose below") : TR("click to change"), k[0] ? ST_ON : ST_OFF, on_slot, UD(s));
        snprintf(rn, sizeof rn, "+slot:%s", slot); reg(rn, b);
        y += 54;
        /* the item's random rolls (which stats rolled; the value is the middle of the range, like the wiki's picker) */
        jv *rolls = jget(e, "rolls"), *tags = jget(e, "tags");
        if (rolls && rolls->t == JN_ARR && tags && tags->t == JN_ARR && tags->cnt) {
            const char *vals[64]; char store[64][120]; int nv = 0;
            vals[nv++] = TR("(none)");
            for (size_t i = 0; i < tags->cnt && nv < 64; i++) { sraw(tags->v[i], store[nv], sizeof store[nv]); vals[nv] = store[nv]; nv++; }
            for (size_t r = 0; r < rolls->cnt && r < 8; r++) {
                char lb[80]; snprintf(lb, sizeof lb, TR("Roll %zu"), r + 1); dsi_utf8_fix_end(lb);
                DSLObj v = U->value_row(p, lab, y, LW - lab - 4, lb, vals, nv, num(rolls->v[r], "i") + 1, on_roll, UD(((int)s << 4) | (int)r));
                snprintf(rn, sizeof rn, "+roll:%s:%zu", slot, r); reg(rn, v);
                y += 46;
            }
        }
        if (W.open_slot == (int)s) {
            jv *items = jget(e, "items");
            float cw = (LW - 8 - 8) / 2; size_t cell = 0;
            DSLObj be = icon_button(p, 4, y, cw, 44, 0, "", TR("(empty)"), NULL, ST_OFF, on_item, UD(((int)s << 12) | 0));
            reg("+item:", be); cell++;
            for (size_t i = 0; items && i < items->cnt; i++) {
                jv *it = items->v[i]; char ik[160], iwn[160], in[160], sub[300];
                sraw(jget(it, "k"), ik, sizeof ik); sraw(jget(it, "n"), iwn, sizeof iwn);
                game_name(ik, iwn, in, sizeof in);
                if (!q_match(in, iwn, NULL) || !item_pass(it)) continue;
                char sb[64], rr[40]; sraw(jget(it, "sub"), sb, sizeof sb); sraw(jget(it, "r"), rr, sizeof rr);
                snprintf(sub, sizeof sub, "%s%s%s%s%s%s%s", sb[0] ? weapon_family(sb, sb) : "", rr[0] ? "  \xC2\xB7  " : "", rr[0] ? rarity_word(rr) : "", truthy(it, "th") ? "  \xC2\xB7  " : "", truthy(it, "th") ? TR("two-handed") : "",
                         truthy(it, "ob") ? "" : "  \xC2\xB7  ", truthy(it, "ob") ? "" : TR("not obtainable"));
                dsi_utf8_fix_end(sub);
                DSLObj ib = icon_button(p, 4 + (cell % 2) * (cw + 8), y + (cell / 2) * 50, cw, 44, CM_ICON_ITEM, ik, in, sub,
                                        !strcmp(ik, k) ? ST_ON : ST_OFF, on_item, UD(((int)s << 12) | (int)(i + 1)));
                snprintf(rn, sizeof rn, "+item:%s", ik); reg(rn, ib);
                cell++;
            }
            y += ((cell + 1) / 2) * 50 + 8;
            if (cell == 1) y = note(p, y, LW, TR("Nothing matches the search and filters."), 24);
        }
    }
    return y;
}
static int insc_pass(const jv *o) { char s[64]; sraw(jget(o, "r"), s, sizeof s); if (!fl_pass(F_IN_RAR, s)) return 0; sraw(jget(o, "cat"), s, sizeof s); return fl_pass(F_IN_CAT, s); }
static int tree_pass(const jv *o) { return fl_pass(F_IN_TREE, truthy(o, "w") ? "w" : "a"); }
static float draw_insc(DSLObj p, float y) {
    jv *in = jget(W.st, "insc"); int freen = num(in, "free");
    FL.n[F_IN_TREE] = 0; fl_add(F_IN_TREE, "w"); fl_add(F_IN_TREE, "a");
    y = heading(p, y, LW, TR("Extra skill trees"));
    const char *trs[] = { TR("Weapon trees"), TR("Auxiliary trees") };
    y = chip_row(p, y, LW, NULL, F_IN_TREE, trs);
    Grid gt = { "treeCands", CM_ICON_TREE, 3, 44, on_tree, "tree", 1, tree_pass, NULL };
    y = grid(p, y, &gt, freen > 0) + 8;
    y = heading(p, y, LW, TR("Inscriptions"));
    Grid gi = { "inscs", CM_ICON_AFFECTER, 2, 48, on_insc, "insc", 1, insc_pass, NULL };
    return grid(p, y, &gi, freen > 0);
}
static float draw_presets(DSLObj p, float y) {
    jv *sets = jget(W.st, "sets");
    y = heading(p, y, LW, TR("Gear quick sets"));
    y = note(p, y, LW, TR("Pick a weapon type, then a material set: the set brings that weapon and the armour of its tier. Change any slot in Gear after."), 40);
    const jv *wt = arr_in(sets, "weapons");
    Flow fw; flow_init(&fw, p, 6, y, LW - 6, 34);
    for (size_t i = 0; wt && i < wt->cnt; i++) {
        char k[40], wn[60]; sraw(jget(wt->v[i], "k"), k, sizeof k); sraw(jget(wt->v[i], "n"), wn, sizeof wn);
        const char *lab = weapon_family(k, wn);
        DSLObj c = flow_chip(&fw, lab, truthy(wt->v[i], "on"), on_weapon, UD(i));
        if (c) { U->tooltip(c, lab, truthy(wt->v[i], "th") ? TR("Two-handed: no shield.") : TR("One-handed.")); char rn[60]; snprintf(rn, sizeof rn, "+weapon:%s", k); reg(rn, c); }
    }
    y = flow_end(&fw);
    int two = truthy(sets, "twoHanded");
    DSLObj tg = U->toggle(p, 6, y, LW - 12 > 520 ? 520 : LW - 12, TR("Add a shield (one-handed weapons)"), truthy(sets, "shield"), on_shield, NULL);   /* 320 cut the label */
    if (tg) { U->set_enabled(tg, !two); reg("+shield", tg); }
    y += 44;
    const jv *ls = arr_in(sets, "list");
    float cw = (LW - 8 - 8) / 2;
    for (size_t i = 0; ls && i < ls->cnt; i++) {
        jv *s = ls->v[i]; char n[80], ic[120], line[600] = ""; size_t o = 0;
        sraw(jget(s, "n"), n, sizeof n); sraw(jget(s, "icon"), ic, sizeof ic);
        const jv *its = arr_in(s, "items");
        if (!ic[0] && its && its->cnt) sraw(jget(its->v[0], "k"), ic, sizeof ic);   /* a set without a weapon: its first piece */
        for (size_t j = 0; its && j < its->cnt; j++) {
            char k[120], wn[120], gn[160]; sraw(jget(its->v[j], "k"), k, sizeof k); sraw(jget(its->v[j], "n"), wn, sizeof wn); game_name(k, wn, gn, sizeof gn);
            o += (size_t)snprintf(line + o, o < sizeof line ? sizeof line - o : 0, "%s%s", j ? ", " : "", gn);
        }
        dsi_utf8_fix_end(line);
        DSLObj b = icon_button(p, 4 + (i % 2) * (cw + 8), y + (i / 2) * 56, cw, 50, CM_ICON_ITEM, ic, n, line, ST_OFF, on_qset, UD(i));
        if (b) { U->tooltip(b, n, line); char rn[60]; snprintf(rn, sizeof rn, "+qset:%zu", i); reg(rn, b); }
    }
    y += ((ls ? ls->cnt : 0) + 1) / 2 * 56 + 10;
    y = heading(p, y, LW, TR("Whole units"));
    DSLObj gb = chip(p, 6, y, 0, 40, TR("Start from a gallery build"), 0, on_gal, NULL, NULL);
    if (gb) { reg("+gallery", gb); if (!cm_gallery_pick) U->set_enabled(gb, 0);
              U->tooltip(gb, TR("Start from a gallery build"), TR("Opens the Wiki Build Gallery. The unit you choose there fills this window.")); }
    return note(p, y + 48, LW, TR("Or save a gallery build to My Units, then press Edit there."), 24);
}

/* ------------------------------------------------------------------ the sheet (the character sheet's look) */
#define ROW_H 22
static float sheet_group(DSLObj p, float y, float sw, const char *title, int rows) {
    deco(p, 4, y + 3, sw - 8, 22, SP_HEADER, C_CONTENTS);
    text_fit(p, 12, y, sw - 24, 28, title, 15, DSL_TXT_LEFT, MU_C_MAIN);
    y += 30;
    deco(p, 4, y, sw - 8, rows * ROW_H + 6, SP_CONTENTS, C_CONTENTS);
    return y + 3;
}
static float sheet_row(DSLObj p, float y, float sw, const char *n, const char *v, const char *rkey) {
    /* the text boxes overlap the next row by 2 at each side: a 22 box under a 15 line is cut ("1.6 x the font size") */
    /* label up to the value (values are short: "15.2%", "34 / 34"); a game label much too long for it drops its trailing
       "(...)" (DE "Maximale Lebenspunkte (gesamt)" shrank far below the other rows) */
    float lw = sw - 14 - 64 - 14; char sh[120]; const char *pa = strrchr(n, '(');
    if (pa && pa > n + 2 && pa[-1] == ' ') {
        DSLObj t = mu_text(p, 14, y - 2, lw, ROW_H + 4, n, 14, DSL_TXT_LEFT, MU_C_SUB); float pw = t ? cm_text_preferred_w(t) : -1;
        if (t) U->destroy(t);
        if (pw > lw * 1.15f) { snprintf(sh, sizeof sh, "%.*s", (int)(pa - 1 - n), n); n = sh; }
    }
    text_fit(p, 14, y - 2, lw, ROW_H + 4, n, 14, DSL_TXT_LEFT, MU_C_SUB);
    DSLObj o = mu_text(p, sw - 78, y - 2, 64, ROW_H + 4, v, 15, DSL_TXT_RIGHT, MU_C_MAIN);
    if (rkey) { char rn[120]; snprintf(rn, sizeof rn, "~stat:%s", rkey); reg(rn, o); }
    return y + ROW_H;
}
/* the unit's avatar (portrait) and its in-game sprite (race, face, worn gear), both made by the game (mu_look.h) */
static float sheet_looks(DSLObj p, float y, float sw, float h) {
    mu_look lk; memset(&lk, 0, sizeof lk);
    sraw(jget(W.st, "race"), lk.race, sizeof lk.race); sraw(jget(W.st, "portrait"), lk.portrait, sizeof lk.portrait);
    const jv *eq = arr("eq");
    for (size_t i = 0; eq && i < eq->cnt && lk.ngear < MU_LOOK_GEAR; i++) { sraw(jget(eq->v[i], "k"), lk.gear[lk.ngear], sizeof lk.gear[0]); if (lk.gear[lk.ngear][0]) lk.ngear++; }
    deco(p, 4, y, sw - 8, h + 8, SP_CONTENTS, C_CONTENTS);
    DSLObj a = mu_look_avatar(p, 10, y + 4, h, &lk);
    if (!a) mu_icon(p, 10, y + 4, h, CM_ICON_AFFECTER, lk.race, 0, NULL);
    DSLObj s2 = mu_look_sprite(p, 18 + h, y + 4, sw - 30 - h, h, &lk);
    if (a) reg("~look:avatar", a);
    if (s2) reg("~look:sprite", s2);
    return y + h + 14;
}
static void thumb_set(int k, DSLObj content, float x, float y, float vh, float ch);
static void draw_sheet(void) {
    thumb_set(1, NULL, 0, 0, 0, 0);
    if (W.sheet) U->destroy(W.sheet);
    reg_forget('~');
    W.sheet = NULL;
    float sw = W.w - W.sx - 20;
    jv *sh = W.st ? jget(W.st, "sheet") : NULL;
    const jv *st = arr_in(sh, "stats"), *mj = arr_in(sh, "majors");
    /* its height: looks, the stat groups (title 33 + rows + 8), Points left (3 rows). UI scale 1.4 leaves ~600: the
       avatar gets smaller (down to 64), then the sheet scrolls; it never runs past the window's bottom */
    float ah = 100, rest = 14 + (st ? 41 + ROW_H * (float)st->cnt : 0) + (mj ? 41 + ROW_H * (float)mj->cnt : 0) + 36 + 3 * ROW_H;
    if (ah + rest > W.mh) { ah = W.mh - rest; if (ah < 64) ah = 64; if (ah > 100) ah = 100; }
    int scroll = W.st && ah + rest > W.mh + 0.5f;
    W.sheet = scroll ? U->scroll(W.win, W.sx, W.my, sw, W.mh) : clear_holder(W.win, W.sx, W.my, sw, W.mh);
    if (!W.sheet || !W.st) return;
    if (scroll) { sw -= 22; U->scroll_height(W.sheet, ah + rest); }                  /* room for the scroll bar */
    thumb_set(1, scroll ? W.sheet : NULL, W.sx + sw + 10, W.my, W.mh, ah + rest);
    float y = sheet_looks(W.sheet, 0, sw, ah);
    if (st) {
        y = sheet_group(W.sheet, y, sw, cm_gw("TEXTKEY_EntityStatusUI_CombatStat", "Combat Stat"), (int)st->cnt);
        for (size_t i = 0; i < st->cnt; i++) {
            char k[64], n[80], t[60]; sraw(jget(st->v[i], "k"), k, sizeof k); sraw(jget(st->v[i], "n"), n, sizeof n); sraw(jget(st->v[i], "t"), t, sizeof t);
            y = sheet_row(W.sheet, y, sw, stat_word(k, n), t, n);
        }
        y += 8;
    }
    if (mj) {
        y = sheet_group(W.sheet, y, sw, cm_gw("TEXTKEY_Shared_MajorStat_NAME", "Major Stat"), (int)mj->cnt);
        for (size_t i = 0; i < mj->cnt; i++) {
            char k[64], n[80], t[60]; sraw(jget(mj->v[i], "k"), k, sizeof k); sraw(jget(mj->v[i], "n"), n, sizeof n); sraw(jget(mj->v[i], "t"), t, sizeof t);
            y = sheet_row(W.sheet, y, sw, stat_word(k, n), t, n);
        }
        y += 8;
    }
    jv *sp = jget(W.st, "sp"), *in = jget(W.st, "insc"); char t[80];
    y = sheet_group(W.sheet, y, sw, TR("Points left"), 3);
    snprintf(t, sizeof t, "%d / %d", num(sp, "main"), num(sp, "mainMax")); y = sheet_row(W.sheet, y, sw, TR("Main SP left"), t, NULL);
    snprintf(t, sizeof t, "%d / %d", num(sp, "sub"), num(sp, "subMax")); y = sheet_row(W.sheet, y, sw, TR("Sub SP left"), t, NULL);
    snprintf(t, sizeof t, "%d / %d", num(in, "free"), num(in, "slots")); sheet_row(W.sheet, y, sw, TR("Inscriptions free"), t, NULL);
}

/* scroll thumbs (as Test Fights): the kit's scroll lists have no bar, so the last portrait row looked cut at the window's
   bottom. A thin gold bar in the list's right margin shows there is more; it follows the wheel (win_poll). 0 mid, 1 sheet */
static struct { uintptr_t ct, track, bar; float x, y, vh, ch; } TH[2];
static float th_h(int k) { float bh = TH[k].vh * TH[k].vh / TH[k].ch; return bh < 24 ? 24 : bh; }
static void thumb_set(int k, DSLObj content, float x, float y, float vh, float ch) {
    Obj o;
    if (cm_alive(o = cm_il_hget(TH[k].track))) U->destroy(o);
    if (cm_alive(o = cm_il_hget(TH[k].bar))) U->destroy(o);
    cm_il_hset(&TH[k].ct, NULL); cm_il_hset(&TH[k].track, NULL); cm_il_hset(&TH[k].bar, NULL);
    if (!content || !W.win || ch <= vh + 1) return;
    TH[k].x = x; TH[k].y = y; TH[k].vh = vh; TH[k].ch = ch;
    DSLObj t = clear_holder(W.win, x, y, 4, vh), b = clear_holder(W.win, x, y, 4, th_h(k));
    look(t, NULL, 0x3A3540FFu); look(b, NULL, MU_C_GOLD);
    cm_il_hset(&TH[k].ct, content); cm_il_hset(&TH[k].track, t); cm_il_hset(&TH[k].bar, b);
}
static void thumb_tick(void) {
    static cm_m pos; static void *k_sr; static int tried;
    if (!tried) { tried = 1; k_sr = cm_il_class("UnityEngine.UI", "ScrollRect"); pos = cm_il_method("UnityEngine.UI", "ScrollRect", "get_verticalNormalizedPosition", 0, NULL); }
    for (int k = 0; k < 2 && pos.fn && k_sr; k++) {
        Obj ct = cm_il_hget(TH[k].ct), bar = cm_il_hget(TH[k].bar);
        if (!cm_alive(ct) || !cm_alive(bar)) continue;
        Obj sr = cm_comp(cm_parent(ct), k_sr); if (!sr) continue;
        float np = CM_CALL(pos, float, Obj, const void *)(sr, pos.mi), bh = th_h(k);
        if (np < 0) np = 0;
        if (np > 1) np = 1;
        U->place(bar, TH[k].x, TH[k].y + (1 - np) * (TH[k].vh - bh), 4, bh);
    }
}
static void draw_mid(void) {
    float top = W.my + W.bar_h, h = W.mh - W.bar_h - 4;
    thumb_set(0, NULL, 0, 0, 0, 0);
    if (W.mid && fabsf(h - W.scroll_h) > 0.5f) { U->destroy(W.mid); W.mid = W.mid_holder = NULL; }
    if (!W.mid) { W.mid = U->scroll(W.win, W.mx + 4, top, W.mw - 8, h); W.mid_holder = NULL; W.scroll_h = h; }
    if (!W.mid) return;
    if (W.mid_holder) U->destroy(W.mid_holder);
    LW = W.mw - 8 - 22;                                   /* room for the scroll bar */
    W.mid_holder = clear_holder(W.mid, 0, 0, LW, 10);
    if (!W.mid_holder) return;
    DSLObj p = W.mid_holder; float y = 4;
    if (eng == ENG_FAILED) y = note(p, y, LW, eng_msg, 140);
    else if (eng != ENG_READY || !W.st) y = note(p, y, LW, TR("Loading the wiki's Unit Builder..."), 30);
    else switch (W.section) {
        case SEC_RACE: y = draw_race(p, y); break;
        case SEC_BG: y = draw_bg(p, y); break;
        case SEC_TRAITS: y = draw_traits(p, y); break;
        case SEC_SKILLS: y = draw_skills(p, y); break;
        case SEC_GEAR: y = draw_gear(p, y); break;
        case SEC_LEVEL: y = draw_level(p, y); break;
        case SEC_PRESETS: y = draw_presets(p, y); break;
        default: y = draw_insc(p, y); break;
    }
    U->place(p, 0, 0, LW, y + 12);
    U->scroll_height(W.mid, y + 12);
    thumb_set(0, W.mid, W.mx + 4 + W.mw - 8 - 12, top, h, y + 12);
}

static void on_lvl(void *ud, DSLObj w) { (void)ud; char a[64]; snprintf(a, sizeof a, "{\"t\":\"lvl\",\"v\":%d}", U->value_get(w) + 1); act(a); }
static void on_nav(void *ud, DSLObj w) {
    (void)w; int s = IDX(ud); if (s < 0 || s >= SEC_N) return;
    if (W.section != s) { W.section = s; W.open_slot = -1; if (W.mid) { U->destroy(W.mid); W.mid = W.mid_holder = NULL; } W.bar_sec = -1; }
    refresh();
}
static void nav_look(void) {
    for (int i = 0; i < SEC_N; i++) if (W.nav[i]) {
        int on = i == W.section;
        btn_look(W.nav[i], spr(on ? SP_TOG_ON : SP_TOG));
        char t[300]; snprintf(t, sizeof t, "%s%s%s", on ? "<color=#D9BE8F>" : "", sec_label(i), on ? "</color>" : "");
        U->set_text(W.nav[i], t);
    }
}

static void refresh(void) {
    if (!W.open || !W.win) return;
    reg_forget('+');
    nav_look();
    int ready = eng == ENG_READY && W.st;
    if (W.v_lvl && ready) {
        int mx = num(W.st, "maxLvl"), lv = num(W.st, "lvl");
        if (mx != W.lvl_n) {                                  /* the level values come from the data (consts.maxLevel) */
            char store[100][8]; const char *vals[100]; int n = mx > 0 && mx <= 100 ? mx : 12;
            for (int i = 0; i < n; i++) { snprintf(store[i], sizeof store[i], "%d", i + 1); vals[i] = store[i]; }
            U->destroy(W.v_lvl);
            W.v_lvl = U->value_row(W.win, W.lx, 54, 240, TR("Level"), vals, n, lv - 1, on_lvl, NULL);
            W.lvl_n = n;
        } else U->value_set(W.v_lvl, lv - 1);
    }
    reg_forget('-'); reg("-lvl", W.v_lvl);
    if (W.b_save) U->set_enabled(W.b_save, ready && !W.saving);
    if (W.v_lvl) U->set_enabled(W.v_lvl, ready);
    /* the filter bar: drawn again when the section, the open gear slot or a chip changed, or when the state it shows
       (SP left, inscription count) may have changed: never while its search box is being typed in */
    int slot_key = W.section == SEC_GEAR ? W.open_slot : -2;
    if (W.bar_sec != W.section || W.bar_slot != slot_key || !W.bar || (ready && W.section != SEC_TRAITS && W.section != SEC_BG)) {
        if (W.f_search) U->field_get(W.f_search, FL.q[W.section], sizeof FL.q[W.section]);
        draw_bar();
    }
    draw_mid();
    draw_sheet();
    if (W.status) U->set_text(W.status, W.msg);
}

static int build_ui(void) {
    float w, h;
    W.win = mu_window(W.unit_id[0] ? TR("Unit Builder - Edit") : TR("Unit Builder"), 1240, 780, &w, &h);
    if (!W.win) { mu_log("builder: kit not ready"); return -1; }
    W.w = w; W.h = h; W.mid = W.mid_holder = W.sheet = W.bar = W.f_search = NULL; W.nreg = 0; W.lvl_n = 0; W.bar_sec = -1; W.scroll_h = 0;
    W.lw = W.lh = 0; cm_rect_size(cm_parent(W.win), &W.lw, &W.lh); W.scale = mu_ui_scale();
    /* one fill over the Box sprite (it holds a lighter block that ended mid-window when stretched); its copper edge stays */
    { DSLObj fill = U->panel(W.win, 5, 8, w - 10, h - 16, 0);
      if (fill) { look(fill, NULL, C_FILL); cm_raycast(fill, false); cm_set_sibling(fill, 0); } }
    float y = 56;
    /* the top row from the window's real width (mu_window clamps it): a narrow window (UI scale 1.4 on a 16:10 or 4:3
       screen) takes the room for the message from the name box, never from the buttons */
    float fw = 260, need = 240 - (w - 902); if (need > 0) fw -= need > 100 ? 100 : need;
    W.lx = 84 + fw + 16; float stx = W.lx + 240 + 12;
    text_fit(W.win, 20, y, 64, 40, TR("Name"), 15, DSL_TXT_LEFT, MU_C_SUB);
    W.f_name = U->field(W.win, 84, y, fw, 40, TR("Unit name"), W.lib_name, on_name, NULL);
    { const char *one[] = { "-" }; W.v_lvl = U->value_row(W.win, W.lx, y - 2, 240, TR("Level"), one, 1, 0, on_lvl, NULL); }   /* 44 high: centred on the 40 row */
    W.b_save = mu_button(W.win, w - 280, y, 125, 40, TR("Save"), on_save, NULL);
    W.b_cancel = mu_button(W.win, w - 145, y, 125, 40, TR("Cancel"), on_cancel, NULL);
    /* the message: up to three lines (y 50..104, above the lists) so a gallery load or a save error is read whole */
    W.status = mu_text(W.win, stx, y - 6, w - 280 - stx - 10, 54, W.msg, 14, DSL_TXT_LEFT, MU_C_SUB);
    U->tooltip(W.b_save, TR("Save"), TR("Saves this unit in My Units. Use it in Bring Units (Foreign Expedition, PvP) and Test Fights."));
    reg("name", W.f_name); reg("save", W.b_save); reg("cancel", W.b_cancel); reg("status", W.status); reg("win", W.win);
    W.my = 108; W.mh = h - W.my - 20; W.mx = 212;
    float ny = W.my, nh = 40;
    if ((nh + 6) * SEC_N > W.mh) nh = W.mh / SEC_N - 6;
    for (int j = 0; j < SEC_N; j++) {
        int i = NAV_ORDER[j];
        W.nav[i] = mu_button(W.win, 20, ny + j * (nh + 6), 180, nh, sec_label(i), on_nav, UD(i));
        char rn[40]; snprintf(rn, sizeof rn, "nav:%s", SEC_NAME[i]); reg(rn, W.nav[i]);
    }
    float sheet_w = w >= 1100 ? 300 : 250;
    W.sx = w - 20 - sheet_w; W.mw = W.sx - 12 - W.mx;
    /* the list column: one "Contents" panel from the top of the tabs to the bottom (the same tone as the sheet's groups) */
    deco(W.win, W.mx, W.my, W.mw, W.mh, SP_CONTENTS, C_CONTENTS);
    return 0;
}

static int relang;                       /* the language changed: build the window again (bld_relang) */
void bld_relang(void) { relang = 1; }
static void win_poll(void) {             /* the kit's screen layer or the UI scale changed: build the window again */
    if (!W.open || !W.win) { relang = 0; return; }
    thumb_tick();
    /* the search box: the list follows what is typed (a short pause first, so a word is one redraw) */
    if (W.f_search) {
        char q[96] = ""; U->field_get(W.f_search, q, sizeof q);
        if (strcmp(q, FL.q[W.section])) { snprintf(FL.q[W.section], sizeof FL.q[W.section], "%s", q); W.search_wait = 6; }
        else if (W.search_wait > 0 && --W.search_wait == 0) { reg_forget('+'); draw_mid(); }
    }
    float lw = 0, lh = 0;
    if (relang || (!cm_rect_size(cm_parent(W.win), &lw, &lh) && lw >= 100 && lh >= 100
        && (fabsf(lw - W.lw) > 1 || fabsf(lh - W.lh) > 1 || fabsf(mu_ui_scale() - W.scale) > 0.001f))) {
        relang = 0;
        char name[96] = ""; if (W.f_name) U->field_get(W.f_name, name, sizeof name);
        if (W.f_search) U->field_get(W.f_search, FL.q[W.section], sizeof FL.q[W.section]);
        mu_log("builder: screen layer %.0fx%.0f -> %.0fx%.0f, window built again", W.lw, W.lh, lw, lh);
        U->destroy(W.win); W.win = NULL;
        if (build_ui()) { close_win(); return; }
        if (name[0]) U->field_set(W.f_name, name);
        refresh();
    }
}

void cm_builder_open(const char *unit_id_or_null, void (*saved)(const char *unit_id, void *ud), void *ud) {
    U = mu_kit(); if (!U) return;
    if (W.open) close_win();
    if (bld_work_start()) { mu_log("builder: worker thread did not start"); return; }
    { static int polling; if (!polling) polling = !mu_tick_add(win_poll); }
    W.section = SEC_RACE; W.open_slot = -1; W.msg[0] = 0; W.saving = 0; W.dirty = 0; W.name0[0] = 0;
    W.unit_id[0] = 0; W.lib_name[0] = 0; W.saved = saved; W.ud = ud;
    fl_reset();
    { char p[1200] = ""; cm_m m = cm_il_method("UnityEngine", "Application", "get_persistentDataPath", 0, NULL);
      if (m.fn) { cm_il_utf8(CM_CALL(m, Obj, const void *)(m.mi), p, sizeof p); if (p[0]) mu_set_data_dir(p); } }
    char *bj = NULL;
    if (unit_id_or_null && *unit_id_or_null) {
        char e[400]; jv *lib = mu_load(e, sizeof e);
        const jv *u = lib ? mu_find(lib, unit_id_or_null) : NULL;
        jv *b = u ? jget(u, "build") : NULL;
        if (b) {
            snprintf(W.unit_id, sizeof W.unit_id, "%s", unit_id_or_null);
            sraw(jget(u, "name"), W.lib_name, sizeof W.lib_name);
            bj = jdump(b, 0, NULL);
        } else set_msg(lib ? TR("That unit is not in My Units any more: a new unit starts here.") : e, MU_C_BAD);
        if (lib) jfree(lib);
    }
    W.saved = saved; W.ud = ud; snprintf(W.name0, sizeof W.name0, "%s", W.lib_name);   /* the field's first text */
    if (build_ui()) { free(bj); return; }
    W.open = 1;
    if (eng == ENG_FAILED) eng = ENG_NONE;              /* try again (the wiki may answer now) */
    post(J_LOAD, bj ? bj : "null");
    free(bj);
    refresh();
    mu_log("builder: window open (%s)", W.unit_id[0] ? W.unit_id : "new unit");
}

int bld_is_open(void) { return W.open; }
void bld_close_window(void) { close_win(); }

/* ------------------------------------------------------------------ tests (ext/units13/driver "bld:<name>") */
DSLObj bld_test_find(const char *n) {
    if (!W.open) return NULL;
    for (int i = 0; i < W.nreg; i++) {
        const char *rn = W.reg[i].name; if (*rn == '+' || *rn == '=' || *rn == '~' || *rn == '-') rn++;
        if (!strcmp(rn, n)) return W.reg[i].o;
    }
    return NULL;
}
size_t bld_test_state(char *out, size_t cap) {
    char m[900], nm[200] = "", race[100] = "", bg[100] = "", em[800];
    jesc(m, sizeof m, W.msg); jesc(em, sizeof em, eng_msg);
    if (W.st) { char t[200]; sraw(jget(W.st, "name"), t, sizeof t); jesc(nm, sizeof nm, t); sraw(jget(W.st, "race"), race, sizeof race); sraw(jget(W.st, "bg"), bg, sizeof bg); }
    int learned = 0; const jv *trees = arr("trees");
    for (size_t t = 0; trees && t < trees->cnt; t++) { jv *rows = jget(trees->v[t], "rows");
        for (size_t r = 0; rows && r < rows->cnt; r++) for (size_t c = 0; rows->v[r]->t == JN_ARR && c < rows->v[r]->cnt; c++) learned += jtruthy(jget(rows->v[r]->v[c], "on")); }
    return (size_t)snprintf(out, cap, "{\"open\":%d,\"engine\":%d,\"busy\":%d,\"saving\":%d,\"section\":%d,\"edit\":\"%s\",\"msg\":\"%s\",\"eng_msg\":\"%s\","
                            "\"name\":\"%s\",\"race\":\"%s\",\"bg\":\"%s\",\"lvl\":%d,\"learned\":%d,\"widgets\":%d,\"w\":%.0f,\"h\":%.0f}",
                            W.open, eng, bld_work_busy(), W.saving, W.section, W.unit_id, m, em, nm, race, bg, W.st ? num(W.st, "lvl") : 0, learned,
                            W.nreg, W.w, W.h);
}
/* test: keys the window lists now, "key,on,locked;..." for kind races|bgs|inds|treeCands|inscs|acq|souls|portraits|skills|items:<Slot> */
int bld_test_keys(const char *kind, char *out, size_t n) {
    size_t o = 0; out[0] = 0;
    if (!W.st) return -1;
    if (!strcmp(kind, "skills")) {
        const jv *trees = arr("trees");
        for (size_t t = 0; trees && t < trees->cnt; t++) { jv *rows = jget(trees->v[t], "rows");
            for (size_t r = 0; rows && r < rows->cnt; r++) for (size_t c = 0; rows->v[r]->t == JN_ARR && c < rows->v[r]->cnt; c++) {
                jv *nd = rows->v[r]->v[c]; if (!nd || nd->t != JN_OBJ) continue;
                char k[160], why[200]; sraw(jget(nd, "k"), k, sizeof k); sraw(jget(nd, "why"), why, sizeof why);
                if (o + 200 < n) o += (size_t)snprintf(out + o, n - o, "%s,%d,%d;", k, jtruthy(jget(nd, "on")), why[0] != 0);
            } }
        return 0;
    }
    const jv *a = NULL;
    if (!strncmp(kind, "items:", 6)) {
        const jv *eq = arr("eq");
        for (size_t s = 0; eq && s < eq->cnt; s++) { char sl[40]; sraw(jget(eq->v[s], "slot"), sl, sizeof sl); if (!strcmp(sl, kind + 6)) a = jget(eq->v[s], "items"); }
    } else a = arr(kind);
    for (size_t i = 0; a && a->t == JN_ARR && i < a->cnt; i++) {
        char k[160]; sraw(jget(a->v[i], "k"), k, sizeof k);
        int ok = jget(a->v[i], "ok") ? jtruthy(jget(a->v[i], "ok")) : 1;
        if (o + 200 < n) o += (size_t)snprintf(out + o, n - o, "%s,%d,%d;", k, jtruthy(jget(a->v[i], "on")), !ok);
    }
    return 0;
}
/* test: one click by JSON (the level row's arrows are not clickable by the driver) */
int bld_test_act_json(const char *json) { if (!W.open || eng != ENG_READY) return -1; act(json); return 0; }
