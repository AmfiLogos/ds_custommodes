/* gal_ui.c - the Gallery window (Custom Modes page > Gallery; owner 2026-10-15: "integrate the build gallery ... they can
 * choose in the gallery build units or teams they want to select"). The wiki's public Build Gallery, read-only:
 * Teams / Units, Most popular / Newest, the wiki's filters (Races, Weapon trees, Complete only), the wiki's two views
 * (Compact Plus: each unit's portrait, gear, trees, inscriptions, traits and skills as game icons; List: one line a
 * unit) -> a card -> Details (every unit in full, with the game's hover cards) / "Save to My Units" (myunits.h) /
 * "Test Fight" (tf.h). Owner 2026-10-17: "the gallery view looks bad ... add the Compact Plus view ... you cannot click
 * a build to preview it". Texts are the gallery page's own (site/builds, C.UI) or the game's (TEXTKEY).
 * Links are read by lane units' decoder (wk_link.h, the one rule for old links); the Test Fight squad is decoded by the
 * wiki's own builder (sg_decode_blob), so the fight is the one the wiki's "Test this team" makes. */
#include "gallery.h"
#include "../fights/cm_win.h"
#include "../fights/cm_work.h"
#include "../fights/tf.h"
#include "../fights/sg_js.h"
#include "myunits.h"
#include "cm_home.h"
#include "wk_link.h"
#include "../fe/fe_json.h"
#include "cm_i18n.h"
#include "cm_il.h"
#include "mu_link.h"
#include "mu_ui.h"
#include "mu_look.h"
#include "../fights/cm_prefs.h"
#include <ctype.h>
#include <stddef.h>
#ifndef _WIN32
#include <strings.h>
#endif
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static float WW = 1040, WH = 720, BH = 720 - CMW_BAND - 1;   /* window, body height (win_size) */
#define GOLD 0xD9BE8FFFu
#define SUB 0xA69085FFu
#define WARN 0xEA9F3DFFu
#define MAIN 0xE8DCC8FFu
#define MAXIT (GAL_PAGE_SIZE * (GAL_MAX_PAGE + 1))
#define SEARCH_Y 112.0f                                /* the search row, under the filters */
#define GRID_Y 162.0f

static const DSLUi *U;
static DSLObj body, grid_holder, w_status, w_prev, w_next, w_page, w_search;
static int view;                                       /* 0 = Compact Plus (the wiki's default), 1 = List; remembered */
static int pv = -1; static char pv_id[32];             /* the build shown in Details (-1 none), by id: items move when page 0 loads again */
static int nvis, vis[GAL_PAGE_SIZE * (GAL_MAX_PAGE + 1)], vpage, want_next;   /* the builds shown, the page, ">" waits for a load */
static gal_words *words; static int words_tried; static char words_lang[12];   /* words_lang: dsi_code() of words */
static struct { int chars, recent, race, tree, complete; } q = { 0, 0, 0, 0, 1 };
static gal_item *items; static int nitems, page, more, loading, gen;   /* gen: a filter change makes older answers stale */
static char status[600]; static int status_warn;
/* the picker door (cm_gallery_pick): the gallery opened for the Unit Builder; NULL = a normal window */
static void (*pick_cb)(const char *build_json, void *ud); static void *pick_ud;
static const char *race_keys[16], *tree_keys[40]; static int nrace, ntree;
/* search (owner 2026-10-17: "add a search bar (search by name, author, and if cheap by class/weapon)"). A race or weapon
   tree word in the text sets the Races / Weapon trees filter (the server filters); the rest is an author (the wiki's
   Find box: /api/gallery?authors=) or, when no author has that name, a filter on the loaded builds: name, author,
   background (class), race and tree words. */
static DSLObj w_field, w_by;
static char s_name[128];                       /* the local filter, "" = none */
static char s_by[20], s_by_name[120];          /* the author filter (aid), "" = none */
static int s_gen;                              /* a new search makes an older author answer stale */
#define SEARCH_MIN 6                           /* the local filter loads more pages until this many builds show */

/* ------------------------------------------------------------------ worker jobs */
typedef struct { cm_job j; int gen, page; gal_query q; char race[64], tree[64], by[20], lang[12]; gal_page pg; gal_words *w; char err[300]; int ok; } PageJob;
static int visible(const gal_item *it);
static void page_run(cm_job *j) {
    PageJob *b = (PageJob *)j;
    if (b->lang[0]) b->w = gal_words_fetch_lang(b->lang, b->err, sizeof b->err);   /* none yet, or another language */
    const char *r1[1] = { b->race }, *t1[1] = { b->tree };
    if (b->race[0]) { b->q.races[0] = r1[0]; b->q.nraces = 1; }
    if (b->tree[0]) { b->q.trees[0] = t1[0]; b->q.ntrees = 1; }
    if (b->by[0]) b->q.by = b->by;
    b->ok = gal_fetch_page(&b->q, &b->pg, b->err, sizeof b->err) == 0;
}
static void paint(void);
static void paint_status(void);
static int per_page(void);
static void page_done(cm_job *j) {
    PageJob *b = (PageJob *)j;
    if (b->w) { gal_words_free(words); words = b->w; snprintf(words_lang, sizeof words_lang, "%s", b->lang); }
    words_tried = 1;
    if (b->gen == gen) {
        loading = 0;
        if (b->ok) {
            if (b->page == 0) nitems = 0;
            for (int i = 0; i < b->pg.n && nitems < MAXIT; i++) items[nitems++] = b->pg.items[i];
            if (pv >= 0) { pv = -1; for (int i = 0; i < nitems; i++) if (!strcmp(items[i].id, pv_id)) { pv = i; break; } }
            more = b->pg.more && page < GAL_MAX_PAGE; page = b->page;
            status[0] = 0;                                                 /* picking: the hint stays until a state replaces it */
            if (pick_cb) { snprintf(status, sizeof status, "%s", TR("Choose a build, open its Details, then press \"Use this unit\".")); status_warn = 0; }
            int shown = 0; for (int i = 0; i < nitems; i++) shown += visible(&items[i]);
            if (s_name[0] && shown < SEARCH_MIN && more) {                /* a name search: look further down the list */
                gal_page_free(&b->pg); free(b);
                void fetch_more_search(void); fetch_more_search();
                paint(); return;
            }
            if (want_next) {                                               /* ">" on the last page: the next one, once loaded */
                int nv = 0; for (int i = 0; i < nitems; i++) nv += visible(&items[i]);
                if (nv > (vpage + 1) * per_page()) vpage++;
                want_next = 0;
            }
            if (!nitems) { snprintf(status, sizeof status, "%s", TR("No public builds match these filters yet.")); status_warn = 0; }
            else if (!shown) { snprintf(status, sizeof status, "%s", TR("No public builds match this search.")); status_warn = 0; }
        } else { snprintf(status, sizeof status, "%s", TR("Could not load the gallery. Try again later.")); status_warn = 1; want_next = 0; cmw_log("gal: %s", b->err); }
    }
    gal_page_free(&b->pg); free(b);
    paint();
}
static void fetch(int pg) {
    if (pg == 0) gen++;
    PageJob *b = calloc(1, sizeof *b); b->j.run = page_run; b->j.done = page_done; b->gen = gen; b->page = pg;
    b->q.chars = q.chars; b->q.recent = q.recent; b->q.incomplete = !q.complete; b->q.page = pg;
    if (q.race > 0) snprintf(b->race, sizeof b->race, "%s", race_keys[q.race - 1]);
    if (q.tree > 0) snprintf(b->tree, sizeof b->tree, "%s", tree_keys[q.tree - 1]);
    snprintf(b->by, sizeof b->by, "%s", s_by);
    if ((!words && !words_tried) || (words && strcmp(words_lang, dsi_code()))) snprintf(b->lang, sizeof b->lang, "%s", dsi_code());
    loading = 1; snprintf(status, sizeof status, "%s", TR("Loading builds...")); status_warn = 0;
    cm_work_post(&b->j);
}
void fetch_more_search(void) {
    if (page + 1 > GAL_MAX_PAGE || nitems + GAL_PAGE_SIZE > MAXIT) {                 /* all loaded: say when none matched */
        more = 0; loading = 0; status[0] = 0;
        int shown = 0; for (int i = 0; i < nitems; i++) shown += visible(&items[i]);
        if (!shown) { snprintf(status, sizeof status, "%s", TR("No public builds match this search.")); status_warn = 0; }
        return;
    }
    fetch(page + 1);
}

/* ------------------------------------------------------------------ search */
/* case-insensitive (ASCII letters) substring; other bytes as they are (Japanese, Cyrillic names match exactly) */
static int ci_has(const char *hay, const char *needle) {
    size_t n = strlen(needle); if (!n) return 1;
    for (const char *h = hay; *h; h++) {
        size_t i = 0;
        while (i < n && h[i] && (h[i] == needle[i] || ((unsigned char)h[i] < 128 && (unsigned char)needle[i] < 128 &&
               tolower((unsigned char)h[i]) == tolower((unsigned char)needle[i])))) i++;
        if (i == n) return 1;
    }
    return 0;
}
static int visible(const gal_item *it) {
    if (!s_name[0]) return 1;
    char t[300]; gal_item_title(it, words, t, sizeof t); if (ci_has(t, s_name)) return 1;
    gal_author_name(it, words, t, sizeof t); if (ci_has(t, s_name)) return 1;
    for (int u = 0; u < it->nunits; u++) {
        const gal_unit *g = &it->units[u];
        if (ci_has(gal_name_word(words, "bg", g->bg), s_name) || ci_has(gal_name_word(words, "races", g->race), s_name)) return 1;
        for (int z = 0; z < g->ntrees; z++) if (ci_has(gal_name_word(words, "trees", g->trees[z]), s_name)) return 1;
    }
    return 0;
}
/* a whole word of the text equal to a word of the group (case-insensitive): its index, and the word is cut out of text */
static int take_word(char *text, const char *const *names, int n) {
    for (int i = 0; i < n; i++) {
        size_t L = strlen(names[i]); if (!L) continue;
        for (char *p = text; *p; p++) {
            if (p != text && p[-1] != ' ') continue;
            if (strncasecmp(p, names[i], L) || (p[L] && p[L] != ' ')) continue;
            memmove(p, p + L, strlen(p + L) + 1);
            return i;
        }
    }
    return -1;
}
static void trim(char *s) {
    char *a = s; while (*a == ' ') a++;
    memmove(s, a, strlen(a) + 1);
    size_t L = strlen(s); while (L && s[L - 1] == ' ') s[--L] = 0;
    for (char *p = s; *p; p++) while (p[0] == ' ' && p[1] == ' ') memmove(p, p + 1, strlen(p));
}
static int is_bg_word(const char *t) {
    const char *k[64]; int n = gal_word_keys(words, "bg", k, 64);
    for (int i = 0; i < n; i++) if (!strcasecmp(gal_name_word(words, "bg", k[i]), t)) return 1;
    return 0;
}
typedef struct { cm_job j; int gen; char text[128]; gal_item au[8]; int n; char err[300]; } FindJob;
static void find_run(cm_job *j) { FindJob *b = (FindJob *)j; b->n = gal_fetch_authors(b->text, b->au, 8, b->err, sizeof b->err); }
static void search_label(void) {
    if (!w_by) return;
    char t[200] = "";
    if (s_by[0]) snprintf(t, sizeof t, TR("Builds by %s"), s_by_name);
    dsi_utf8_fix_end(t); cmw_set_text(w_by, t);
}
static void find_done(cm_job *j) {
    FindJob *b = (FindJob *)j;
    if (b->gen == s_gen && cmw_is_open("gallery")) {
        int pick = -1; char nm[120];
        for (int i = 0; i < b->n && pick < 0; i++) { gal_author_name(&b->au[i], words, nm, sizeof nm); if (!strcasecmp(nm, b->text)) pick = i; }
        if (pick < 0 && b->n > 0) pick = 0;
        if (pick >= 0) { snprintf(s_by, sizeof s_by, "%s", b->au[pick].au_id); gal_author_name(&b->au[pick], words, s_by_name, sizeof s_by_name); s_name[0] = 0; }
        else { s_by[0] = 0; snprintf(s_name, sizeof s_name, "%s", b->text); }   /* no author by that name: names and words */
        if (b->n < 0) cmw_log("gal: author search: %s", b->err);
        search_label(); fetch(0); paint();
    }
    free(b);
}
static void search_go(const char *raw) {
    char t[128]; snprintf(t, sizeof t, "%s", raw ? raw : ""); trim(t);
    s_gen++; s_name[0] = s_by[0] = s_by_name[0] = 0;
    if (t[0] && words) {                                               /* race / weapon tree words: the server's filters */
        static char nb[40][64]; const char *names[40]; int n;          /* gal_word's buffers rotate: copies */
        nrace = gal_word_keys(words, "races", race_keys, 16); ntree = gal_word_keys(words, "trees", tree_keys, 40);
        n = 0; for (int i = 0; i < nrace; i++) { snprintf(nb[n], 64, "%s", gal_name_word(words, "races", race_keys[i])); names[n] = nb[n]; n++; }
        int r = take_word(t, names, n); if (r >= 0) q.race = r + 1;
        n = 0; for (int i = 0; i < ntree; i++) { snprintf(nb[n], 64, "%s", gal_name_word(words, "trees", tree_keys[i])); names[n] = nb[n]; n++; }
        int k = take_word(t, names, n); if (k >= 0) q.tree = k + 1;
        trim(t);
    }
    if (!t[0] || is_bg_word(t) || strlen(t) < 2) { snprintf(s_name, sizeof s_name, "%s", t); search_label(); fetch(0); paint(); return; }
    FindJob *b = calloc(1, sizeof *b); if (!b) return;
    b->j.run = find_run; b->j.done = find_done; b->gen = s_gen; snprintf(b->text, sizeof b->text, "%s", t);
    loading = 1; snprintf(status, sizeof status, "%s", TR("Searching...")); status_warn = 0;
    search_label(); cm_work_post(&b->j); paint();
}
static void search_click(void *ud, DSLObj w) { (void)ud; (void)w; char t[256] = ""; if (w_field) U->field_get(w_field, t, sizeof t); search_go(t); }
static void clear_click(void *ud, DSLObj w) {
    (void)ud; (void)w; if (w_field) U->field_set(w_field, "");
    q.race = q.tree = 0; search_go("");
}

/* a card's action: fetch the full item (its stored build), then save it or test it */
typedef struct { cm_job j; char id[32]; int test, unit; gal_item it; char *blob, *builds; char err[400]; int ok; } ItemJob;
static void title_of(const gal_item *it, char *o, size_t n) { gal_item_title(it, words, o, n); }
extern sg_engine *tf_engine_for_gallery(char *err, size_t errn);        /* tf_ui.c: the engine, opened on this thread */
static void item_run(cm_job *j) {
    ItemJob *b = (ItemJob *)j;
    if (gal_fetch_item(b->id, &b->it, &b->blob, b->err, sizeof b->err) || !b->blob) { if (!b->err[0]) snprintf(b->err, sizeof b->err, "%s", TR("Could not load this team. Try again later.")); return; }
    if (!b->test) { b->ok = 1; return; }                             /* test: 1 Test Fight, 2 pick (the builder's JSON) */
    sg_engine *e = tf_engine_for_gallery(b->err, sizeof b->err); if (!e) return;
    b->builds = sg_decode_blob(e, b->blob, b->err, sizeof b->err);
    b->ok = b->builds != NULL;
}
/* the one rule for links (lane units): the blob read by wk_link. 1 ok (n units), 0 refused (err) */
static int link_ok(const char *blob, wk_build *bs, int *n, char *err, size_t errn) {
    int code = 0; char t[8192];
    if (!strncmp(blob, "b:", 2)) snprintf(t, sizeof t, "#b=%s", blob + 2); else snprintf(t, sizeof t, "%s", blob);
    *n = wkl_decode_offline(t, bs, WK_MAX_UNITS, &code, err, errn);
    return *n > 0;
}
static void item_done(cm_job *j) {
    ItemJob *b = (ItemJob *)j; char title[200]; title_of(&b->it, title, sizeof title);
    wk_build *bs = calloc(WK_MAX_UNITS, sizeof *bs); int n = 0; char e[300] = "";
    if (!b->ok) { snprintf(status, sizeof status, "%s", b->err); status_warn = 1; }
    else if (b->test == 2) {                                           /* the picked unit's build, as the wiki builder wrote it */
        jv *arr = jparse(b->builds, strlen(b->builds), e, sizeof e);
        const jv *one = arr && arr->t == JN_ARR && b->unit >= 0 && (size_t)b->unit < arr->cnt ? arr->v[b->unit] : NULL;
        char *js = one ? jdump(one, 0, NULL) : NULL;
        if (js && pick_cb) {
            void (*cb)(const char *, void *) = pick_cb; void *ud = pick_ud;
            pick_cb = NULL; cb(js, ud);                                /* answered: closing the window is not a cancel */
            cmw_close("gallery");
        } else { snprintf(status, sizeof status, "%s", e[0] ? e : TR("Could not load this team. Try again later.")); status_warn = 1; }
        free(js); jfree(arr);
        free(bs); free(b->blob); free(b->builds); free(b);
        if (cmw_is_open("gallery")) paint();
        return;
    }
    else if (!link_ok(b->blob, bs, &n, e, sizeof e)) { snprintf(status, sizeof status, "%s", e); status_warn = 1; }
    else if (b->test) {
        const char *names[GAL_MAX_UNITS]; char nb[GAL_MAX_UNITS][96]; int nn = 0;
        for (int i = 0; i < b->it.nunits && nn < GAL_MAX_UNITS; i++) {
            char m[96]; if (b->it.team) gal_member_name(&b->it, i, words, m, sizeof m); else snprintf(m, sizeof m, "%s", title);
            snprintf(nb[nn], 96, TR("%s  Lv %d"), m, b->it.units[i].lvl); dsi_utf8_fix_end(nb[nn]); names[nn] = nb[nn]; nn++;
        }
        tf_open_with_builds(b->builds, title, names, nn);
        status[0] = 0;
    } else {
        jv *lib = mu_load(e, sizeof e); int r = -1;
        if (lib) {
            if (b->it.team) {
                r = mu_add_team(lib, bs, n, title, NULL, e, sizeof e) < 0 ? -1 : 0;
                char au[120]; gal_author_name(&b->it, words, au, sizeof au);   /* My Units shows "by <builder>" under the team */
                if (!r) mu_team_set_source(lib, mu_team_count(lib) - 1, au, b->it.id[0] ? b->it.id : b->id);
            } else {
                /* one character: the build JSON as stored ("b:" + base64), checked by mu_add_build (wk_from_json) */
                char t[8192]; snprintf(t, sizeof t, "%s", b->blob + 2);
                size_t L = strlen(t); for (size_t i = 0; i < L; i++) { if (t[i] == '-') t[i] = '+'; else if (t[i] == '_') t[i] = '/'; }
                static const char A[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
                char *out = malloc(L + 4); size_t o = 0; unsigned acc = 0; int bits = 0;
                for (size_t i = 0; i < L; i++) { const char *p = strchr(A, t[i]); if (!p || !t[i]) continue; acc = acc << 6 | (unsigned)(p - A); bits += 6; if (bits >= 8) { bits -= 8; out[o++] = (char)(acc >> bits & 255); } }
                out[o] = 0;
                jv *bj = jparse(out, o, e, sizeof e); free(out);
                char id[17];
                r = bj ? mu_add_build(lib, bj, title, id, e, sizeof e) : -1;
                jfree(bj);
            }
            if (!r) r = mu_save(lib, e, sizeof e);
            jfree(lib);
        }
        if (!r) { snprintf(status, sizeof status, TR("Saved to My Units: %s"), title); status_warn = 0; }
        else { snprintf(status, sizeof status, "%s", e[0] ? e : TR("My Units could not be written.")); status_warn = 1; }
    }
    free(bs); free(b->blob); free(b->builds); free(b);
    paint();
}
static void card_action(int i, int test) {
    if (i < 0 || i >= nitems) return;
    ItemJob *b = calloc(1, sizeof *b); b->j.run = item_run; b->j.done = item_done; b->test = test;
    snprintf(b->id, sizeof b->id, "%s", items[i].id);
    snprintf(status, sizeof status, "%s", test ? TR("Opening the build...") : TR("Saving...")); status_warn = 0;
    cm_work_post(&b->j);
    paint();
}
static void save_click(void *ud, DSLObj w) { (void)w; card_action((int)(intptr_t)ud, 0); }
/* pick mode: "Use this unit" in Details (ud = unit index; the item is the one shown) */
static void pick_click(void *ud, DSLObj w) {
    (void)w; if (pv < 0 || pv >= nitems || !pick_cb) return;
    ItemJob *b = calloc(1, sizeof *b); if (!b) return;
    b->j.run = item_run; b->j.done = item_done; b->test = 2; b->unit = (int)(intptr_t)ud;
    snprintf(b->id, sizeof b->id, "%s", items[pv].id);
    snprintf(status, sizeof status, "%s", TR("Opening the build...")); status_warn = 0;
    cm_work_post(&b->j);
    paint_status();
}
static void test_click(void *ud, DSLObj w) { (void)w; card_action((int)(intptr_t)ud, 1); }

/* ------------------------------------------------------------------ drawing */
/* Every widget of the page (filters, cards or the preview) is kept in trk and destroyed one by one when the page is drawn
   again: the kit frees a destroyed widget's slot at once, its children only at its next sweep (~1 s), and it holds 1024
   live widgets for every window. A Compact Plus page is ~500 widgets, so two pages in one second would not fit. */
#define TRK_MAX 1200
static DSLObj trk[TRK_MAX]; static int ntrk;
static unsigned char trk_text[TRK_MAX];
static DSLObj K(DSLObj o) { if (o && ntrk < TRK_MAX) { trk_text[ntrk] = 0; trk[ntrk++] = o; } return o; }
static void trk_clear(void) {
    for (int i = ntrk - 1; i >= 0; i--) { mu_tip_forget(trk[i]); U->destroy(trk[i]); }
    ntrk = 0;
}
/* one line of text (ellipsis when too long). The box is at least 1.6 x the font size high, centred on the asked box:
   TMP also cuts a line that is taller than its box (a 14 px line in a 20 px box was cut to "Lizardman ...") */
static DSLObj txt(DSLObj p, float x, float y, float w, float h, const char *s, float size, int flags, uint32_t c) {
    if (!(flags & DSL_TXT_WRAP) && h < size * 1.6f) { y -= (size * 1.6f - h) / 2; h = size * 1.6f; }
    DSLObj o = K(mu_text(p, x, y, w, h, s, size, flags, c));
    if (o && ntrk) trk_text[ntrk - 1] = 1;
    return o;
}
static int texts_cut(void) { int n = 0; for (int i = 0; i < ntrk; i++) n += trk_text[i] && cm_text_truncated(trk[i]) == 1; return n; }
static DSLObj flat(DSLObj p, float x, float y, float w, float h, uint32_t c) {
    DSLObj o = K(U->panel(p, x, y, w, h, 0)); if (o) { cmw_flat(o, c); cm_raycast(o, false); }
    return o;
}
/* the scroll thumb (as Test Fights): the kit's scroll lists have no bar, and a card is taller than the list (its buttons
   under the bottom edge looked cut). A thin gold bar at the list's right edge shows there is more; it follows the wheel. */
static uintptr_t th_ct, th_bar; static float th_x, th_y, th_vh, th_ch;
static float th_h(void) { float bh = th_vh * th_vh / th_ch; return bh < 24 ? 24 : bh; }
static void thumb_add(DSLObj parent, DSLObj content, float x, float y, float vh, float ch) {
    cm_il_hset(&th_ct, NULL); cm_il_hset(&th_bar, NULL);
    if (ch <= vh + 1) return;
    flat(parent, x, y, 4, vh, 0x3A3540FFu);
    th_x = x; th_y = y; th_vh = vh; th_ch = ch;
    cm_il_hset(&th_ct, content); cm_il_hset(&th_bar, flat(parent, x, y, 4, th_h(), GOLD));
}
static void thumb_tick(void *ud) {
    (void)ud;
    static cm_m pos; static void *k_sr; static int tried;
    if (!tried) { tried = 1; k_sr = cm_il_class("UnityEngine.UI", "ScrollRect"); pos = cm_il_method("UnityEngine.UI", "ScrollRect", "get_verticalNormalizedPosition", 0, NULL); }
    Obj ct = cm_il_hget(th_ct), bar = cm_il_hget(th_bar);
    if (!pos.fn || !k_sr || !cm_alive(ct) || !cm_alive(bar)) return;
    Obj sr = cm_comp(cm_parent(ct), k_sr); if (!sr) return;
    float np = CM_CALL(pos, float, Obj, const void *)(sr, pos.mi), bh = th_h();
    if (np < 0) np = 0;
    if (np > 1) np = 1;
    U->place(bar, th_x, th_y + (1 - np) * (th_vh - bh), 4, bh);
}

/* names in the game's language (the build's own title stays the wiki's, as on the wiki) */
static void game_name(const char *key, char *o, size_t n) { mu_name_of(key, o, n); }
/* gal_name_hook: race and background = the game's AFFECTER name; a weapon tree = the game's weapon type name
   (TEXTKEY_Equipment_<tree>_NAME: Sword, Bow ...); the other trees (Warrior, Fire Magic ...) have no short game name:
   the wiki's word in the player's language */
static _Thread_local int hook_off;                   /* the worker thread (gal_link_author): no game calls there */
/* the wiki has no gallery words in Spanish and Portuguese: the words with no short game name, ours */
static const struct { const char *group, *key, *es, *pt; } ESPT[] = {
    { "trees", "Berserker", "Berserker", "Berserker" }, { "trees", "Blitz", "Relámpago", "Relâmpago" },
    { "trees", "Burst", "Estallido", "Explosão" }, { "trees", "Execution", "Ejecución", "Execução" },
    { "trees", "FireMagic", "Magia de fuego", "Magia de fogo" }, { "trees", "Fortress", "Fortaleza", "Fortaleza" },
    { "trees", "Guardian", "Guardián", "Guardião" }, { "trees", "Harmony", "Armonía", "Harmonia" },
    { "trees", "NatureMagic", "Magia de la naturaleza", "Magia da natureza" }, { "trees", "Stake", "Estaca", "Estaca" },
    { "trees", "Tide", "Marea", "Maré" }, { "trees", "Warrior", "Guerrero", "Guerreiro" },
    { "trees", "WaterMagic", "Magia de agua", "Magia de água" },
    { "nouns", "vanguard", "Vanguardia", "Vanguarda" }, { "nouns", "warband", "Banda de guerra", "Bando de guerra" },
    { "nouns", "blades", "Hojas", "Lâminas" }, { "nouns", "circle", "Círculo", "Círculo" },
    { "nouns", "coven", "Aquelarre", "Coven" }, { "nouns", "conclave", "Cónclave", "Conclave" },
    { "nouns", "company", "Compañía", "Companhia" }, { "nouns", "order", "Orden", "Ordem" },
    { "nouns", "fellowship", "Hermandad", "Irmandade" },
    { "tpl", NULL, "{noun} {race} de {trees}", "{noun} {race} de {trees}" }, { "pair", NULL, "{a} y {b}", "{a} e {b}" },
    { "unit", NULL, "Colono", "Colono" } };
static const char *espt(const char *group, const char *key) {
    const char *c = dsi_code(); int es = !strcmp(c, "es"), pt = !strcmp(c, "pt-BR");
    if (!es && !pt) return NULL;
    for (size_t i = 0; i < sizeof ESPT / sizeof *ESPT; i++)
        if (!strcmp(ESPT[i].group, group) && (!ESPT[i].key ? !key : key && !strcmp(ESPT[i].key, key))) return es ? ESPT[i].es : ESPT[i].pt;
    return NULL;
}
static const char *name_hook(const char *group, const char *key) {
    if (hook_off) return NULL;
    if (!key) return espt(group, NULL);                                /* a template */
    static char ring[8][128]; static unsigned slot; char *o = ring[slot++ & 7]; o[0] = 0;
    if (!strcmp(group, "races") || !strcmp(group, "bg")) { char k[128]; snprintf(k, sizeof k, "TEXTKEY_%s_NAME", key); if (cm_gettext(k, o, 128)) o[0] = 0; }
    else if (!strcmp(group, "trees")) { char k[128]; snprintf(k, sizeof k, "TEXTKEY_Equipment_%s_NAME", key); if (cm_gettext(k, o, 128)) o[0] = 0; }
    return o[0] ? o : espt(group, key);
}
static const char *tree_name(const char *t) {
    char k[96]; snprintf(k, sizeof k, "TEXTKEY_SKILLTREE_%s_NAME", t);
    const char *w = gal_name_word(words, "trees", t);
    return cm_gw(k, *w ? w : t);
}
static const char *stat_name(const char *key) {
    char k[96]; snprintf(k, sizeof k, "TEXTKEY_STATUS_%s_NAME", key);
    return cm_gw(k, key);
}
static void member_name(const gal_item *it, int u, char *o, size_t n) {
    char rn[96]; game_name(it->units[u].race, rn, sizeof rn);
    if (!rn[0]) gal_member_name(it, u, words, o, n);
    else if (it->team) snprintf(o, n, "%s %d", rn, u + 1);
    else snprintf(o, n, "%s", rn);
    dsi_utf8_fix_end(o);
}
static const char *const MAJ[6] = { "Strength", "Constitution", "WillPower", "Intelligence", "Agility", "Perception" };
static uint32_t talent_colour(const char *t) {
    if (!strcmp(t, "Genius")) return 0xF2C64EFFu;
    if (!strcmp(t, "Exceptional")) return 0xC9D3DCFFu;
    if (!strcmp(t, "Outstanding")) return 0xD08A55FFu;
    if (!strcmp(t, "Poor")) return 0xE06A5AFFu;
    return MAIN;
}

/* one row of game icons in groups (a gap between groups). wrap = go on below; else "+N" when the rest does not fit.
   Returns the height used. Icons with a game card (item, skill, trait) show it on hover; a tree shows its name. */
typedef struct { int kind; const char *key; int dim; } Ic;
static float icons(DSLObj p, float x, float y, float right, float s, const Ic *ic, const int *grp, int n, int wrap, const char *test_name) {
    float cx = x, cy = y, step = s + 4; int last = -1;
    for (int i = 0; i < n; i++) {
        if (last >= 0 && grp[i] != last) cx += 10;
        last = grp[i];
        int rest = n - i - 1;
        if (cx + s > right - (!wrap && rest ? 34 : 0)) {
            if (wrap) { cx = x; cy += step; }
            else { char t[16]; snprintf(t, sizeof t, "+%d", n - i); txt(p, cx, cy, 34, s, t, 13, DSL_TXT_LEFT, SUB); break; }
        }
        char nm[160];
        if (ic[i].kind == CM_ICON_TREE) snprintf(nm, sizeof nm, "%s", tree_name(ic[i].key)); else game_name(ic[i].key, nm, sizeof nm);
        DSLObj o = K(mu_icon(p, cx, cy, s, ic[i].kind, ic[i].key, ic[i].dim, nm));
        if (test_name && i == 0) cmw_named(o, CMW_TEXT, test_name, NULL, NULL, ic[i].key);
        cx += step;
    }
    return n ? cy - y + s : 0;
}
static int kind_of_key(const char *k) {
    if (!strncmp(k, "ITEM_", 5)) return CM_ICON_ITEM;
    if (!strncmp(k, "SKILL_", 6)) return CM_ICON_SKILL;
    if (!strncmp(k, "AFFECTER_", 9)) return CM_ICON_AFFECTER;
    return CM_ICON_TREE;
}

/* the item's info lines: "by X" + counts, then units / level / focus / date / incomplete */
static void counts_text(const gal_item *it, char *o, size_t n) {
    snprintf(o, n, "%s <color=#D9BE8F>%d</color>    %s <color=#D9BE8F>%d</color>    %s <color=#D9BE8F>%d</color>",
             TR("Copies"), it->copies, TR("Opens"), it->opens, TR("Likes"), it->likes);
    dsi_utf8_fix_end(o);
}
static void meta_text(const gal_item *it, char *o, size_t n) {
    size_t k = 0; o[0] = 0;
#define ADD(...) do { if (k < n) { if (k) k += (size_t)snprintf(o + k, n - k, "  \xC2\xB7  "); if (k < n) k += (size_t)snprintf(o + k, n - k, __VA_ARGS__); } } while (0)
    if (it->team) ADD(TRN("%d unit", "%d units", it->nunits), it->nunits);
    if (it->min_level > 0) ADD(TR("From Lv %d"), it->min_level);
    if (it->focus[0]) ADD(TR("Focus: %s"), stat_name(it->focus));
    if (strlen(it->created) >= 10) ADD(TR("Shared %.10s"), it->created);
    if (!it->complete) ADD("<color=#EA9F3D>%s</color>", TR("Incomplete"));
#undef ADD
    dsi_utf8_fix_end(o);
}
/* tags (game keys): icon + name chips; returns the height used */
static float tags_row(DSLObj p, float x, float y, float right, const gal_item *it) {
    if (!it->ntags) return 0;
    float cx = x;
    for (int i = 0; i < it->ntags; i++) {
        const char *k = it->tags[i]; int kind = kind_of_key(k);
        char nm[160]; if (kind == CM_ICON_TREE) snprintf(nm, sizeof nm, "%s", tree_name(k)); else game_name(k, nm, sizeof nm);
        if (cx + 60 > right) break;
        DSLObj chip = flat(p, cx, y, 40, 26, 0x3A353FFF);
        K(mu_icon(chip, 3, 3, 20, kind, k, 0, nm));
        DSLObj t = txt(chip, 27, 0, 300, 26, nm, 13, DSL_TXT_LEFT, MAIN);
        float tw = t ? cm_text_preferred_w(t) : 60; if (tw < 10) tw = 60;
        float cw = 27 + tw + 8; if (cx + cw > right) cw = right - cx;
        U->place(chip, cx, y, cw, 26); if (t) U->place(t, 27, 0, cw - 31, 26);
        cx += cw + 6;
    }
    return 32;
}
/* the portrait (else the race's icon) with the level under it */
/* the unit's two looks (owner 2026-10-17: "each unit shows both its in-game sprite and its avatar"), by the shared
   mu_look (units/mu_look.h, the game's own compositor): the avatar (else the race's icon), then the whole character
   with its race body and worn gear. Returns the width used. */
static int nsprites;                                  /* sprites drawn on the page (test) */
static float looks(DSLObj p, float x, float y, float s, const gal_unit *g) {
    mu_look l; memset(&l, 0, sizeof l);
    snprintf(l.race, sizeof l.race, "%s", g->race); snprintf(l.portrait, sizeof l.portrait, "%s", g->portrait);
    for (int i = 0; i < g->ngear && l.ngear < MU_LOOK_GEAR; i++) snprintf(l.gear[l.ngear++], sizeof l.gear[0], "%s", g->gear[i]);
    DSLObj a = K(mu_look_avatar(p, x, y, s, &l));
    if (!a) { char rn[96]; game_name(g->race, rn, sizeof rn); K(mu_icon(p, x, y, s, CM_ICON_AFFECTER, g->race, 0, rn)); }
    if (K(mu_look_sprite(p, x + s + 2, y, s, s, &l))) nsprites++;
    return 2 * s + 2;
}
static void face(DSLObj p, float x, float y, float s, const gal_unit *g) {
    float w = looks(p, x, y, s, g);
    if (g->lvl > 0) { char lv[32]; snprintf(lv, sizeof lv, TR("Lv %d"), g->lvl); txt(p, x, y + s + 1, w, 18, lv, 13, DSL_TXT_CENTER, GOLD); }
}
/* "Race · Background · Health 1967 · Physical Attack Power 163" */
static void crumb(const gal_unit *g, int stats, char *o, size_t n) {
    char rn[96], bg[96]; game_name(g->race, rn, sizeof rn); game_name(g->bg, bg, sizeof bg);
    size_t k = (size_t)snprintf(o, n, "%s%s%s", rn, bg[0] ? "  \xC2\xB7  " : "", bg);
    if (stats && k < n && g->hp) k += (size_t)snprintf(o + k, n - k, "  \xC2\xB7  %s %d", cm_gw("TEXTKEY_GAMEGUIDE_Health_NAME", "Health"), g->hp);
    if (stats && k < n && (g->pa || g->ma)) {
        int mag = g->ma > g->pa;
        k += (size_t)snprintf(o + k, n - k, "  \xC2\xB7  %s %d", stat_name(mag ? "MagicAttackPower" : "PhysicalAttackPower"), mag ? g->ma : g->pa);
    }
    dsi_utf8_fix_end(o);
}
/* the unit's icons by group: A = worn items | trees | inscriptions | soul stones | traits acquired in play;
   B = active skills | passive skills */
static int icons_a(const gal_unit *g, Ic *ic, int *gr) {
    int n = 0;
    for (int i = 0; i < g->ngear; i++) { ic[n] = (Ic){ CM_ICON_ITEM, g->gear[i], 0 }; gr[n++] = 0; }
    for (int i = 0; i < g->ntrees; i++) { ic[n] = (Ic){ CM_ICON_TREE, g->trees[i], 0 }; gr[n++] = 1; }
    for (int i = 0; i < g->ninsc; i++) { ic[n] = (Ic){ CM_ICON_AFFECTER, g->insc[i], 0 }; gr[n++] = 2; }
    for (int i = 0; i < g->nsouls; i++) { ic[n] = (Ic){ CM_ICON_AFFECTER, g->souls[i], 0 }; gr[n++] = 3; }
    for (int i = 0; i < g->nacq; i++) { ic[n] = (Ic){ CM_ICON_AFFECTER, g->acq[i], 0 }; gr[n++] = 4; }
    return n;
}
static int icons_b(const gal_unit *g, Ic *ic, int *gr, int which) {   /* which: -1 both, 0 active, 1 passive */
    int n = 0;
    for (int pas = 0; pas < 2; pas++) {
        if (which >= 0 && which != pas) continue;
        for (int i = 0; i < g->nskills; i++) if (gal_skill_passive(words, g->skills[i]) == pas) { ic[n] = (Ic){ CM_ICON_SKILL, g->skills[i], 0 }; gr[n++] = pas; }
    }
    return n;
}

/* ---- the list: cards */
static float CW;                                      /* card width (2 columns) */
static int ncards_drawn;
static int per_page(void) { return view == 0 ? (q.chars ? 6 : 4) : (q.chars ? 10 : 8); }
static void view_open(void *ud, DSLObj w) {
    (void)w; int i = (int)(intptr_t)ud;
    if (i >= 0 && i < nitems) { pv = i; snprintf(pv_id, sizeof pv_id, "%s", items[i].id); paint(); }
}
/* a card's Test Fight: its Details stay under the Test Fights window (a page of cards is ~500 of the kit's widgets) */
static void card_test_click(void *ud, DSLObj w) { view_open(ud, w); test_click(ud, w); }

/* Compact Plus unit (the wiki gallery's Compact+ card): portrait + in-game sprite + level | name, race / background / stats,
   items + trees + inscriptions + soul stones + acquired traits, then the skills (active | passive) */
#define CPU_H 96.f
static void unit_cp(DSLObj p, float x, float y, float w, const gal_item *it, int u) {
    const gal_unit *g = &it->units[u]; char t[400];
    face(p, x, y + 2, 50, g);
    float x0 = x + 110, right = x + w;
    /* name left, health / attack right on the name's line; race / background under it (all on one line, the German
       attack word was cut: "Magischer Angriffswert 3...") */
    char st[200] = "", rb[300]; crumb(g, 1, t, sizeof t); crumb(g, 0, rb, sizeof rb);
    if (strlen(t) > strlen(rb) + 6) snprintf(st, sizeof st, "%s", t + strlen(rb) + 6);   /* the stats part ("  ·  " + ...) */
    member_name(it, u, t, sizeof t);
    float nw = st[0] ? (right - x0) * 0.32f : right - x0;
    txt(p, x0, y, nw, 20, t, 15, DSL_TXT_LEFT, MAIN);
    if (st[0]) txt(p, x0 + nw + 6, y + 2, right - x0 - nw - 6, 18, st, 12, DSL_TXT_RIGHT, SUB);
    txt(p, x0, y + 20, right - x0, 18, rb, 12, DSL_TXT_LEFT, SUB);
    Ic ic[128]; int gr[128];
    int n = icons_a(g, ic, gr); icons(p, x0, y + 40, right, 24, ic, gr, n, 0, NULL);
    n = icons_b(g, ic, gr, -1); icons(p, x0, y + 68, right, 24, ic, gr, n, 0, NULL);
}
/* List unit: one line, portrait, name + level, race / background, trees */
#define LU_H 34.f
static void unit_list(DSLObj p, float x, float y, float w, const gal_item *it, int u) {
    const gal_unit *g = &it->units[u]; char t[300], nm[160];
    looks(p, x, y + 2, 30, g);
    member_name(it, u, nm, sizeof nm);
    snprintf(t, sizeof t, TR("%s  Lv %d"), nm, g->lvl); dsi_utf8_fix_end(t);
    float tx = x + 70, treex = x + w - 3 * 26;
    txt(p, tx, y, (treex - tx) * 0.5f - 6, LU_H, t, 14, DSL_TXT_LEFT, MAIN);
    crumb(g, 0, t, sizeof t);
    txt(p, tx + (treex - tx) * 0.5f, y + 8, (treex - tx) * 0.5f - 6, 18, t, 12, DSL_TXT_LEFT, SUB);   /* one line: a tall box wraps */
    Ic ic[8]; int gr[8], n = 0;
    for (int i = 0; i < g->ntrees && n < 3; i++) { ic[n] = (Ic){ CM_ICON_TREE, g->trees[i], 0 }; gr[n++] = 0; }
    icons(p, treex, y + 6, x + w + 4, 22, ic, gr, n, 0, NULL);
}
/* a card: panel (the game's panel sprite), click anywhere = Details, then title, author + counts, info, tags, units,
   Details / Save to My Units / Test Fight (above the units: always in view), units. Returns its height. */
static float card(DSLObj p, int i, float x, float y) {
    const gal_item *it = &items[i]; char t[600], nm[48]; float pad = 14, w = CW, iw = w - 2 * pad;
    DSLObj c = K(U->panel(p, x, y, w, 100, 0));
    if (!c) return 0;
    DSLObj hit = K(U->button(c, 0, 0, w, 100, "", view_open, (void *)(intptr_t)i));   /* under everything: a click opens Details */
    if (hit) cmw_flat(hit, 0x00000000);
    snprintf(nm, sizeof nm, "gallery:c%d.card", i); cmw_named(hit, CMW_BUTTON, nm, view_open, (void *)(intptr_t)i, "");
    title_of(it, t, sizeof t);
    snprintf(nm, sizeof nm, "gallery:c%d.title", i);
    cmw_named(txt(c, pad, 19, iw, 26, t, 19, DSL_TXT_LEFT, GOLD), CMW_TEXT, nm, NULL, NULL, t);
    char au[160], by[200]; gal_author_name(it, words, au, sizeof au);
    snprintf(by, sizeof by, TR("by %s"), au); dsi_utf8_fix_end(by);
    txt(c, pad, 45, iw * 0.42f, 20, au[0] ? by : "", 13, DSL_TXT_LEFT, SUB);
    counts_text(it, t, sizeof t);
    snprintf(nm, sizeof nm, "gallery:c%d.counts", i);
    cmw_named(txt(c, pad + iw * 0.42f, 45, iw * 0.58f, 20, t, 13, DSL_TXT_RIGHT, MAIN), CMW_TEXT, nm, NULL, NULL, t);
    float yy = 67;
    meta_text(it, t, sizeof t);
    if (t[0]) { txt(c, pad, yy, iw, 18, t, 12, DSL_TXT_LEFT, SUB); yy += 22; }
    yy += tags_row(c, pad, yy, w - pad, it);
    /* the buttons under the header, above the units: a team card is taller than the list, and at the card's foot the
       list's bottom edge cut them through their labels in the first view */
    int nb = pick_cb ? 1 : 3;                                          /* picking for the Unit Builder: Details only */
    float bw = (iw - 8 * (nb - 1)) / nb;
    const char *lb[3] = { TR("Details"), TR("Save to My Units"), TR("Test Fight") };
    dsl_ui_cb cb[3] = { view_open, save_click, card_test_click };
    const char *sfx[3] = { "view", "save", "test" };
    DSLObj bt[3] = { 0 }; float need[3], sum = 0; int over = 0;
    for (int b = 0; b < nb; b++) {
        snprintf(nm, sizeof nm, "gallery:c%d.%s", i, sfx[b]);
        bt[b] = cmw_named(K(mu_button(c, pad + b * (bw + 8), yy, bw, 38, lb[b], cb[b], (void *)(intptr_t)i)), CMW_BUTTON, nm, cb[b], (void *)(intptr_t)i, lb[b]);
        float pw = bt[b] ? cm_text_preferred_w(bt[b]) : -1;            /* the label measured + the frame */
        need[b] = pw > 0 ? pw + 28 : bw; sum += need[b]; over |= need[b] > bw;
    }
    /* a label longer than an equal third (a long translation): each button as wide as its label, the rest shared */
    if (over && sum <= iw - 8 * (nb - 1)) {
        float extra = (iw - 8 * (nb - 1) - sum) / nb, bx = pad;
        for (int b = 0; b < nb; b++) { if (bt[b]) U->place(bt[b], bx, yy, need[b] + extra, 38); bx += need[b] + extra + 8; }
    }
    yy += 38 + 6;
    flat(c, pad, yy + 2, iw, 1, 0x633D33FF); yy += 10;
    for (int u = 0; u < it->nunits; u++) {
        if (view == 0) { unit_cp(c, pad, yy, iw, it, u); yy += CPU_H + 4; }
        else { unit_list(c, pad, yy, iw, it, u); yy += LU_H + 2; }
    }
    yy += 10;
    U->place(c, x, y, w, yy);
    if (hit) U->place(hit, 0, 0, w, yy);
    ncards_drawn++;
    return yy;
}

/* ---- the preview (a build's Details): every unit with its gear, skills, trees, inscriptions and traits, the game's own
   hover cards on each icon; Save to My Units / Test Fight from there */
static void back_click(void *ud, DSLObj w) { (void)ud; (void)w; pv = -1; paint(); }
static int esc_back(void *ud) { if (pv < 0) return 0; back_click(ud, NULL); return 1; }   /* Escape in Details = its Back */
static float unit_full(DSLObj p, float x, float y, float w, const gal_item *it, int u) {
    const gal_unit *g = &it->units[u]; char t[900];
    DSLObj c = K(U->panel(p, x, y, w, 100, 0));
    if (!c) return 0;
    cm_raycast(c, false);
    face(c, 14, 14, 92, g);
    float x0 = 222, right = w - 14;
    member_name(it, u, t, sizeof t);
    txt(c, x0, 10, right - x0 - (pick_cb ? 230 : 0), 26, t, 20, DSL_TXT_LEFT, GOLD);
    if (pick_cb) { char nm[40]; snprintf(nm, sizeof nm, "gallery:p.pick%d", u);
                   cmw_named(K(mu_button(c, right - 220, 8, 220, 38, TR("Use this unit"), pick_click, (void *)(intptr_t)u)), CMW_BUTTON, nm, pick_click, (void *)(intptr_t)u, TR("Use this unit")); }
    crumb(g, 1, t, sizeof t);
    size_t k = strlen(t);
    for (int e = 0; e < g->nextra && k < sizeof t; e++) {
        char v[32]; double d = g->extra[e].v; if (d == (int)d) snprintf(v, sizeof v, "%d", (int)d); else snprintf(v, sizeof v, "%.1f", d);
        k += (size_t)snprintf(t + k, sizeof t - k, "  \xC2\xB7  %s %s%s", stat_name(g->extra[e].key), v, g->extra[e].pct ? "%" : "");
    }
    dsi_utf8_fix_end(t);
    txt(c, x0, 38, right - x0 - (pick_cb ? 230 : 0), 20, t, 14, DSL_TXT_LEFT, SUB);   /* clear of "Use this unit" */
    /* the six major stats; a talent colours the value (the game's talent gems) and is named after it */
    k = 0; t[0] = 0;
    for (int m = 0; m < 6 && k < sizeof t; m++) {
        if (!g->majors[m] && !g->talents[m][0]) continue;
        char sk[24]; snprintf(sk, sizeof sk, "%s", MAJ[m]);
        char tk[64] = ""; if (g->talents[m][0]) snprintf(tk, sizeof tk, "TEXTKEY_MajorStat_%s_NAME", g->talents[m]);
        uint32_t col = g->talents[m][0] ? talent_colour(g->talents[m]) : MAIN;
        k += (size_t)snprintf(t + k, sizeof t - k, "%s%s <color=#%06X>%d%s%s%s</color>", k ? "    " : "", stat_name(sk), col >> 8, g->majors[m],
                              tk[0] ? " (" : "", tk[0] ? cm_gw(tk, g->talents[m]) : "", tk[0] ? ")" : "");
    }
    dsi_utf8_fix_end(t);
    float yy = 60;
    if (t[0]) { DSLObj mt = txt(c, x0, yy, right - x0, 40, t, 14, DSL_TXT_LEFT | DSL_TXT_WRAP, MAIN);
                float pw = mt ? cm_text_preferred_w(mt) : -1, th = pw > right - x0 ? 40 : 20;   /* one line or two */
                if (mt) U->place(mt, x0, yy, right - x0, th); yy += th + 6; }
    if (yy < 134) yy = 134;                                            /* under the portrait and its level */
    /* sections: the game's own words where it has them */
    struct { const char *label; Ic ic[64]; int gr[64], n; const char *tname; } sec[6]; int ns = 0;
    memset(sec, 0, sizeof sec);
    sec[ns].label = cm_gw("TEXTKEY_ItemType_Equipment", "Equipment"); sec[ns].tname = u == 0 ? "gallery:p.gear0" : NULL;
    for (int i = 0; i < g->ngear && sec[ns].n < 64; i++) { sec[ns].ic[sec[ns].n] = (Ic){ CM_ICON_ITEM, g->gear[i], 0 }; sec[ns].gr[sec[ns].n++] = 0; } ns++;
    sec[ns].label = cm_gw("TEXTKEY_Tooltip_ActiveSkill", "Active Skill"); sec[ns].tname = u == 0 ? "gallery:p.skill0" : NULL;
    sec[ns].n = icons_b(g, sec[ns].ic, sec[ns].gr, 0); ns++;
    sec[ns].label = cm_gw("TEXTKEY_Tooltip_PassiveSkill", "Passive Skill");
    sec[ns].n = icons_b(g, sec[ns].ic, sec[ns].gr, 1); ns++;
    sec[ns].label = TR("Skill Trees");
    for (int i = 0; i < g->ntrees && sec[ns].n < 64; i++) { sec[ns].ic[sec[ns].n] = (Ic){ CM_ICON_TREE, g->trees[i], 0 }; sec[ns].gr[sec[ns].n++] = 0; } ns++;
    sec[ns].label = TR("Inscriptions"); sec[ns].tname = u == 0 ? "gallery:p.insc0" : NULL;
    for (int i = 0; i < g->ninsc && sec[ns].n < 64; i++) { sec[ns].ic[sec[ns].n] = (Ic){ CM_ICON_AFFECTER, g->insc[i], 0 }; sec[ns].gr[sec[ns].n++] = 0; } ns++;
    sec[ns].label = cm_gw("TEXTKEY_EntityAbilityUI_Traits", "Traits"); sec[ns].tname = u == 0 ? "gallery:p.trait0" : NULL;
    { Ic *ic = sec[ns].ic; int *gr = sec[ns].gr, n = 0;
      if (g->race[0]) { ic[n] = (Ic){ CM_ICON_AFFECTER, g->race, 0 }; gr[n++] = 0; }
      if (g->bg[0]) { ic[n] = (Ic){ CM_ICON_AFFECTER, g->bg, 0 }; gr[n++] = 0; }
      for (int i = 0; i < g->nsouls && n < 64; i++) { ic[n] = (Ic){ CM_ICON_AFFECTER, g->souls[i], 0 }; gr[n++] = 1; }
      for (int i = 0; i < g->nacq && n < 64; i++) { ic[n] = (Ic){ CM_ICON_AFFECTER, g->acq[i], 0 }; gr[n++] = 2; }
      sec[ns].n = n; } ns++;
    for (int s = 0; s < ns; s++) {
        if (!sec[s].n) continue;
        txt(c, 14, yy, 150, 34, sec[s].label, 14, DSL_TXT_LEFT, SUB);
        float h = icons(c, 170, yy + 1, right, 32, sec[s].ic, sec[s].gr, sec[s].n, 1, sec[s].tname);
        yy += (h > 34 ? h : 34) + 6;
    }
    yy += 8;
    U->place(c, x, y, w, yy);
    return yy;
}
static void preview(DSLObj p, const gal_item *it) {
    char t[600], au[160], by[200];
    K(cmw_named(mu_button(p, 24, 12, 120, 40, TR("Back"), back_click, NULL), CMW_BUTTON, "gallery:p.back", back_click, NULL, TR("Back")));
    float bx = WW - 24 - 180, sx = bx - 8 - 230;
    if (!pick_cb) {
        K(cmw_named(mu_button(p, bx, 12, 180, 40, TR("Test Fight"), test_click, (void *)(intptr_t)pv), CMW_BUTTON, "gallery:p.test", test_click, (void *)(intptr_t)pv, TR("Test Fight")));
        K(cmw_named(mu_button(p, sx, 12, 230, 40, TR("Save to My Units"), save_click, (void *)(intptr_t)pv), CMW_BUTTON, "gallery:p.save", save_click, (void *)(intptr_t)pv, TR("Save to My Units")));
    } else sx = WW - 24;
    title_of(it, t, sizeof t);
    cmw_named(txt(p, 160, 10, sx - 12 - 160, 30, t, 22, DSL_TXT_LEFT | DSL_TXT_TITLE, GOLD), CMW_TEXT, "gallery:p.title", NULL, NULL, t);
    gal_author_name(it, words, au, sizeof au); snprintf(by, sizeof by, TR("by %s"), au); dsi_utf8_fix_end(by);
    txt(p, 160, 38, sx - 12 - 160, 20, au[0] ? by : "", 14, DSL_TXT_LEFT, SUB);
    counts_text(it, t, sizeof t); txt(p, 24, 64, (WW - 48) * 0.45f, 22, t, 14, DSL_TXT_LEFT, MAIN);
    meta_text(it, t, sizeof t); txt(p, 24 + (WW - 48) * 0.45f, 64, (WW - 48) * 0.55f, 22, t, 13, DSL_TXT_RIGHT, SUB);
    float y = 92 + tags_row(p, 24, 92, WW - 24, it);
    DSLObj sc = K(U->scroll(p, 24, y, WW - 36, BH - 70 - y - 8));
    if (!sc) return;
    float yy = 0;
    for (int u = 0; u < it->nunits; u++) yy += unit_full(sc, 0, yy, WW - 48, it, u) + 10;   /* right edge = the buttons' */
    U->scroll_height(sc, yy);
    thumb_add(p, sc, WW - 18, y, BH - 70 - y - 8, yy);
}

/* ---- filters + the page of cards */
static void kind_click(void *ud, DSLObj w) { (void)w; q.chars = (int)(intptr_t)ud; vpage = 0; nitems = 0; fetch(0); paint(); }
static void sort_click(void *ud, DSLObj w) { (void)w; q.recent = (int)(intptr_t)ud; vpage = 0; nitems = 0; fetch(0); paint(); }
static void view_click(void *ud, DSLObj w) { (void)w; view = (int)(intptr_t)ud; vpage = 0; cm_pref_set("gallery.view", view ? "list" : "compactplus"); paint(); }
static void race_change(void *ud, DSLObj w) { (void)ud; q.race = U->value_get(w); vpage = 0; nitems = 0; fetch(0); paint(); }
static void tree_change(void *ud, DSLObj w) { (void)ud; q.tree = U->value_get(w); vpage = 0; nitems = 0; fetch(0); paint(); }
static void complete_change(void *ud, DSLObj w) { (void)ud; q.complete = U->toggle_get(w); vpage = 0; nitems = 0; fetch(0); paint(); }
static void pages_of(int *pages) { int pp = per_page(); *pages = nvis ? (nvis + pp - 1) / pp : 1; }
static void prev_click(void *ud, DSLObj w) { (void)ud; (void)w; if (vpage > 0) { vpage--; paint(); } }
static void next_click(void *ud, DSLObj w) {
    (void)ud; (void)w; int pages; pages_of(&pages);
    if (vpage + 1 < pages) { vpage++; paint(); }
    else if (more && !loading) { want_next = 1; fetch(page + 1); paint_status(); }
}
static void tab(DSLObj p, float x, float y, float w, const char *label, int on, dsl_ui_cb cb, int v, const char *name) {
    cmw_named(K(mu_button(p, x, y, w, 38, label, cb, (void *)(intptr_t)v)), CMW_BUTTON, name, cb, (void *)(intptr_t)v, label);
    if (on) flat(p, x + 4, y + 40, w - 8, 3, GOLD);
}
static void filters(DSLObj p) {
    const char *kinds[2] = { TR("Teams"), TR("Units") }, *sorts[2] = { TR("Most popular"), TR("Newest") }, *views[2] = { TR("Compact Plus"), TR("List") };
    char nm[40];
    for (int s = 0; s < 2; s++) {
        snprintf(nm, sizeof nm, "gallery:f.kind%d", s); tab(p, 24 + s * 126, 12, 120, kinds[s], q.chars == s, kind_click, s, nm);
        snprintf(nm, sizeof nm, "gallery:f.sort%d", s); tab(p, 292 + s * 162, 12, 156, sorts[s], q.recent == s, sort_click, s, nm);
        snprintf(nm, sizeof nm, "gallery:f.view%d", s); tab(p, WW - 24 - 2 * 168 + 6 + s * 168, 12, 162, views[s], view == s, view_click, s, nm);
    }
    float rw = (WW - 48 - 2 * 12 - 250) / 2;
    if (words) {
        nrace = gal_word_keys(words, "races", race_keys, 16); ntree = gal_word_keys(words, "trees", tree_keys, 40);
        const char *rl[17] = { TR("Any") }, *tl[41] = { TR("Any") };
        static char rn[16][96], tn[40][96];
        for (int i = 0; i < nrace; i++) { char g[96]; game_name(race_keys[i], g, sizeof g); snprintf(rn[i], 96, "%s", g[0] ? g : gal_name_word(words, "races", race_keys[i])); rl[i + 1] = rn[i]; }
        for (int i = 0; i < ntree; i++) { snprintf(tn[i], 96, "%s", gal_name_word(words, "trees", tree_keys[i])); tl[i + 1] = tn[i]; }
        cmw_named(K(U->value_row(p, 24, 62, rw, TR("Races"), rl, nrace + 1, q.race, race_change, NULL)), CMW_VALUE, "gallery:f.race", race_change, NULL, rl[q.race]);
        cmw_named(K(U->value_row(p, 24 + rw + 12, 62, rw, TR("Weapon trees"), tl, ntree + 1, q.tree, tree_change, NULL)), CMW_VALUE, "gallery:f.tree", tree_change, NULL, tl[q.tree]);
    }
    cmw_named(K(U->toggle(p, WW - 24 - 250, 63, 250, TR("Complete only"), q.complete, complete_change, NULL)), CMW_TOGGLE, "gallery:f.complete", complete_change, NULL, TR("Complete only"));
}
static void grid(DSLObj p) {
    nvis = 0; for (int i = 0; i < nitems; i++) if (visible(&items[i])) vis[nvis++] = i;   /* a name search shows only its matches */
    int pages; pages_of(&pages); if (vpage >= pages) vpage = pages - 1; if (vpage < 0) vpage = 0;
    DSLObj sc = K(U->scroll(p, 24, GRID_Y, WW - 36, BH - 70 - GRID_Y - 8));
    if (!sc) return;
    if (!nvis && status[0])                                            /* empty / loading / error: in the empty grid, not at the bottom */
        cmw_named(K(mu_text(sc, 0, 0, WW - 48, 160, status, 18, DSL_TXT_CENTER | DSL_TXT_WRAP, status_warn ? WARN : SUB)), CMW_TEXT, "gallery:empty", NULL, NULL, status);
    CW = (WW - 48 - 12) / 2;                                           /* the right column ends under the filters' right edge */
    float col[2] = { 0, 0 };                                           /* two columns, each card under the shorter one */
    for (int k = vpage * per_page(); k < nvis && k < (vpage + 1) * per_page(); k++) {
        int c = col[1] < col[0];
        col[c] += card(sc, vis[k], c * (CW + 12), col[c]) + 12;
    }
    U->scroll_height(sc, col[0] > col[1] ? col[0] : col[1]);
    thumb_add(p, sc, WW - 18, GRID_Y, BH - 70 - GRID_Y - 8, col[0] > col[1] ? col[0] : col[1]);
}
/* the bottom line: status, page n / m, < > (cheap: called while a page loads, without drawing the cards again) */
static void paint_status(void) {
    if (!cmw_is_open("gallery") || !body) return;
    if (w_status) U->destroy(w_status);                               /* the kit has no colour setter: a problem shows orange */
    const char *st = pv < 0 && !nvis ? "" : status;                   /* an empty grid shows it in its middle (grid) */
    w_status = cmw_named(mu_text(body, 24, BH - 58, WW - 24 - 24 - 290, 52, st, 15, DSL_TXT_LEFT | DSL_TXT_WRAP, status_warn ? WARN : MAIN), CMW_TEXT, "gallery:status", NULL, NULL, st);
    int pages; pages_of(&pages);
    char t[64]; snprintf(t, sizeof t, more ? TR("Page %d / %d+") : TR("Page %d / %d"), vpage + 1, pages); dsi_utf8_fix_end(t);
    cmw_set_text(w_page, pv < 0 ? t : "");
    U->set_active(w_prev, pv < 0); U->set_active(w_next, pv < 0);
    U->set_enabled(w_prev, vpage > 0);
    U->set_enabled(w_next, !loading && (vpage + 1 < pages || more));
}
static void paint(void) {
    if (!cmw_is_open("gallery") || !body) return;
    cmw_forget("gallery:c"); cmw_forget("gallery:f."); cmw_forget("gallery:p.");
    trk_clear(); ncards_drawn = nsprites = 0;
    grid_holder = K(cmw_flat(U->panel(body, 0, 0, WW, BH - 70, 0), 0x00000000));
    cm_raycast(grid_holder, false);                                  /* the search row under it (made once) keeps its clicks */
    if (pv >= nitems) pv = -1;
    if (w_search) U->set_active(w_search, pv < 0);
    if (pv >= 0) preview(grid_holder, &items[pv]);
    else { filters(grid_holder); grid(grid_holder); }
    paint_status();
}
static void closed(void *ud) {
    (void)ud; body = grid_holder = w_status = w_prev = w_next = w_page = w_field = w_by = w_search = NULL;
    if (pick_cb) { void (*cb)(const char *, void *) = pick_cb; pick_cb = NULL; cb(NULL, pick_ud); }   /* closed with no pick */
    ntrk = 0; pv = -1; cmw_forget("gallery:");
}
/* the window: as large as the screen allows at the game's UI scale (cmw_open draws it at that scale), up to 1200 x 820 */
static void win_size(void) {
    float lw = 0, lh = 0, sc = mu_ui_scale(); DSLObj p = U->panel(NULL, 0, 0, 10, 10, 0);
    if (p) { if (cm_rect_size(cm_parent(p), &lw, &lh)) lw = lh = 0; U->destroy(p); }
    lw /= sc; lh /= sc;
    WW = lw >= 1000 ? (lw - 60 < 1200 ? lw - 60 : 1200) : 1040;
    WH = lh >= 700 ? (lh - 60 < 820 ? lh - 60 : 820) : 720;
    BH = WH - CMW_BAND - 1;
}
void gal_open(void) {
    U = cmw_ui(); if (!U || !mu_kit()) return;
    gal_name_hook = name_hook;
    if (!items) items = calloc(MAXIT, sizeof *items);
    if (!items) return;
    { char v[32]; if (!cm_pref_get("gallery.view", v, sizeof v)) view = !strcmp(v, "list"); }
    win_size();
    body = cmw_open("gallery", TR("Wiki Build Gallery"), WW, WH, closed, NULL);
    cmw_set_back("gallery", esc_back, NULL);                          /* Escape in Details = Back first, then the window */
    if (!body) return;
    grid_holder = NULL; ntrk = 0; pv = -1; vpage = 0;
    /* the search row (made once per window, so typing is never cut by a page that arrives) */
    s_gen++; s_name[0] = s_by[0] = s_by_name[0] = 0;
    w_search = cmw_flat(U->panel(body, 0, SEARCH_Y, WW, 44, 0), 0x00000000);
    cm_raycast(w_search, false);
    w_field = cmw_named(U->field(w_search, 24, 0, 480, 40, TR("Search: name, author, race, background or weapon"), "", search_click, NULL),
                        CMW_FIELD, "gallery:s.field", search_click, NULL, "");
    cmw_named(U->button(w_search, 512, 0, 140, 40, TR("Search"), search_click, NULL), CMW_BUTTON, "gallery:s.go", search_click, NULL, TR("Search"));
    cmw_named(U->button(w_search, 658, 0, 110, 40, TR("Clear"), clear_click, NULL), CMW_BUTTON, "gallery:s.clear", clear_click, NULL, TR("Clear"));
    w_by = cmw_named(U->text(w_search, 778, 0, WW - 24 - 778, 40, "", 14, DSL_TXT_LEFT, SUB), CMW_TEXT, "gallery:s.by", NULL, NULL, "");
    cmw_flat(U->panel(body, 24, BH - 66, WW - 48, 1, 0), 0x633D33FF);
    w_status = NULL;
    w_prev = cmw_named(mu_button(body, WW - 24 - 270, BH - 56, 46, 44, "<", prev_click, NULL), CMW_BUTTON, "gallery:prev", prev_click, NULL, "<");
    w_page = cmw_named(mu_text(body, WW - 24 - 220, BH - 56, 174, 44, "", 15, DSL_TXT_CENTER, SUB), CMW_TEXT, "gallery:page", NULL, NULL, "");
    w_next = cmw_named(mu_button(body, WW - 24 - 46, BH - 56, 46, 44, ">", next_click, NULL), CMW_BUTTON, "gallery:next", next_click, NULL, ">");
    if (pick_cb) { snprintf(status, sizeof status, "%s", TR("Choose a build, open its Details, then press \"Use this unit\".")); status_warn = 0; }
    fetch(0);
    if (pick_cb) snprintf(status, sizeof status, "%s", TR("Choose a build, open its Details, then press \"Use this unit\"."));
    paint();
}
/* the picker door for the Unit Builder (lane cmbuilder, weak import): the gallery opens; "Use this unit" in a build's
   Details hands back that unit's build as the wiki builder writes it (JSON, one character), then the gallery closes.
   picked(NULL, ud) when the gallery is closed with no pick. Main thread. */
__attribute__((visibility("default"))) void cm_gallery_pick(void (*picked)(const char *build_json, void *ud), void *ud) {
    cmw_close("gallery");                                              /* an open gallery (an older pick: cancelled there) */
    pick_cb = picked; pick_ud = ud;
    gal_open();
    if (!cmw_is_open("gallery") && pick_cb) { pick_cb = NULL; if (picked) picked(NULL, ud); }
}

/* mu_link's worker thread: the builder of a gallery item (a /build/<id> short link), named as the wiki names it. 1 found */
static int gal_link_author(const char *id, char *out, size_t n) {
    gal_item *it = calloc(1, sizeof *it); char *blob = NULL, e[300]; int ok = 0;
    hook_off = 1;
    if (n) out[0] = 0;
    if (it && !gal_fetch_item(id, it, &blob, e, sizeof e)) {
        gal_words *w = it->au_pseudo ? gal_words_fetch(e, sizeof e) : NULL;   /* a pseudonym is made of the wiki's words */
        gal_author_name(it, w, out, n); gal_words_free(w);
        ok = out[0] != 0;
    }
    free(blob); free(it);
    return ok;
}
static void gal_relang(void) {                                         /* drawn again, a pick in progress kept */
    if (!cmw_is_open("gallery")) return;
    void (*cb)(const char *, void *) = pick_cb; pick_cb = NULL;
    cmw_close("gallery"); pick_cb = cb; gal_open();
}   /* the language changed: drawn again */
/* test: the picker door with a recording callback (what the Unit Builder would get) */
static int t_picks, t_cancels; static char t_pick[160];
static void test_picked(const char *js, void *ud) { (void)ud; if (js) { t_picks++; snprintf(t_pick, sizeof t_pick, "%.150s", js); } else t_cancels++; }
static void test_pick_open(void) { cm_gallery_pick(test_picked, NULL); }
static void test_state(char *out, size_t n) {
    char tip[400]; mu_tip_test_state(tip, sizeof tip);
    char pj[340], se[700]; size_t q = 0; for (const char *c = t_pick; *c && q + 3 < sizeof pj; c++) { if (*c == '"' || *c == '\\') pj[q++] = '\\'; pj[q++] = *c; } pj[q] = 0;
    q = 0; for (const char *c = status; *c && q + 3 < sizeof se; c++) { if ((unsigned char)*c < 0x20) continue; if (*c == '"' || *c == '\\') se[q++] = '\\'; se[q++] = *c; } se[q] = 0;
    size_t k = (size_t)snprintf(out, n, "\"gal\":{\"items\":%d,\"loading\":%d,\"more\":%d,\"words\":%d,\"status\":\"%.300s\","
                                "\"pick\":{\"mode\":%d,\"picks\":%d,\"cancels\":%d,\"json\":\"%s\"},\"sprites\":%d,\"cut\":%d,\"view\":%d,\"vpage\":%d,\"shown\":%d,\"cards\":%d,\"preview\":%d,\"widgets\":%d,\"win\":[%.0f,%.0f],%s,\"ids\":[",
                                nitems, loading, more, words != NULL, se, pick_cb != NULL, t_picks, t_cancels, pj, nsprites, texts_cut(), view, vpage, nvis, ncards_drawn, pv, ntrk, WW, WH, tip);
    for (int i = 0; i < nitems && i < 24 && k + 40 < n; i++) k += (size_t)snprintf(out + k, n - k, "%s\"%s\"", i ? "," : "", items[i].id);
    snprintf(out + k, n - k, "]}");
}

__attribute__((visibility("default"))) void cm_gallery_init(const DSLHost *host, DSLPlugin *self) {
    if (cmw_init(host, self)) return;
    if (cm_work_start()) { cmw_log("gal: worker thread did not start: Gallery is off"); return; }
    cmw_test_opener("gallery", gal_open);
    cmw_test_opener("gallery_pick", test_pick_open);
    cmw_test_state(test_state);
    cm_home_add("gallery", TR_NOOP("Wiki Build Gallery"), NULL, 30, gal_open);
    cmw_tick_add(thumb_tick, NULL);
    mul_gallery_author = gal_link_author;                        /* My Units' "Add from wiki link": a /build/<id> link's builder */
    cm_i18n_on_change(gal_relang);
}
