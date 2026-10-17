/* gal_ui.c - the Gallery window (Custom Modes page > Gallery; owner 2026-10-15: "integrate the build gallery ... they can
 * choose in the gallery build units or teams they want to select"). The wiki's public Build Gallery, read-only:
 * Teams / Units, Most popular / Newest, the wiki's filters (Races, Weapon trees, Complete only) -> a card -> "Save to My
 * Units" (myunits.h) or "Test Fight" (tf.h). Texts are the gallery page's own (site/builds, C.UI).
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
#include <ctype.h>
#ifndef _WIN32
#include <strings.h>
#endif
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define WIN_W 1040.0f
#define WIN_H 720.0f
#define BODY_H (WIN_H - CMW_BAND - 1)
#define GOLD 0xD9BE8FFFu
#define SUB 0xA69085FFu
#define WARN 0xEA9F3DFFu
#define MAIN 0xE8DCC8FFu
#define CARD_W 484.0f
#define CARD_H 196.0f
#define MAXIT (GAL_PAGE_SIZE * (GAL_MAX_PAGE + 1))
#define SEARCH_Y 116.0f                                /* the search row, under the filters */
#define GRID_Y 166.0f

static const DSLUi *U;
static DSLObj body, grid_holder, w_status, w_more;
static gal_words *words; static int words_tried;
static struct { int chars, recent, race, tree, complete; } q = { 0, 0, 0, 0, 1 };
static gal_item *items; static int nitems, page, more, loading, gen;   /* gen: a filter change makes older answers stale */
static char status[600]; static int status_warn;
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
typedef struct { cm_job j; int gen, page; gal_query q; char race[64], tree[64], by[20]; gal_page pg; gal_words *w; char err[300]; int ok; } PageJob;
static int visible(const gal_item *it);
static void page_run(cm_job *j) {
    PageJob *b = (PageJob *)j;
    if (!words && !words_tried) b->w = gal_words_fetch(b->err, sizeof b->err);
    const char *r1[1] = { b->race }, *t1[1] = { b->tree };
    if (b->race[0]) { b->q.races[0] = r1[0]; b->q.nraces = 1; }
    if (b->tree[0]) { b->q.trees[0] = t1[0]; b->q.ntrees = 1; }
    if (b->by[0]) b->q.by = b->by;
    b->ok = gal_fetch_page(&b->q, &b->pg, b->err, sizeof b->err) == 0;
}
static void paint(void);
static void page_done(cm_job *j) {
    PageJob *b = (PageJob *)j;
    if (b->w) { words = b->w; words_tried = 1; }
    else if (!words) words_tried = 1;
    if (b->gen == gen) {
        loading = 0;
        if (b->ok) {
            if (b->page == 0) nitems = 0;
            for (int i = 0; i < b->pg.n && nitems < MAXIT; i++) items[nitems++] = b->pg.items[i];
            more = b->pg.more && page < GAL_MAX_PAGE; page = b->page;
            status[0] = 0;
            int shown = 0; for (int i = 0; i < nitems; i++) shown += visible(&items[i]);
            if (s_name[0] && shown < SEARCH_MIN && more) {                /* a name search: look further down the list */
                gal_page_free(&b->pg); free(b);
                void fetch_more_search(void); fetch_more_search();
                paint(); return;
            }
            if (!nitems) { snprintf(status, sizeof status, "%s", TR("No public builds match these filters yet.")); status_warn = 0; }
            else if (!shown) { snprintf(status, sizeof status, "%s", TR("No public builds match this search.")); status_warn = 0; }
        } else { snprintf(status, sizeof status, "%s", TR("Could not load the gallery. Try again later.")); status_warn = 1; cmw_log("gal: %s", b->err); }
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
    loading = 1; snprintf(status, sizeof status, "%s", TR("Loading builds...")); status_warn = 0;
    cm_work_post(&b->j);
}
void fetch_more_search(void) {
    if (page + 1 > GAL_MAX_PAGE || nitems + GAL_PAGE_SIZE > MAXIT) { more = 0; loading = 0; status[0] = 0; return; }
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
        if (ci_has(gal_word(words, "bg", g->bg), s_name) || ci_has(gal_word(words, "races", g->race), s_name)) return 1;
        for (int z = 0; z < g->ntrees; z++) if (ci_has(gal_word(words, "trees", g->trees[z]), s_name)) return 1;
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
    for (int i = 0; i < n; i++) if (!strcasecmp(gal_word(words, "bg", k[i]), t)) return 1;
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
        n = 0; for (int i = 0; i < nrace; i++) { snprintf(nb[n], 64, "%s", gal_word(words, "races", race_keys[i])); names[n] = nb[n]; n++; }
        int r = take_word(t, names, n); if (r >= 0) q.race = r + 1;
        n = 0; for (int i = 0; i < ntree; i++) { snprintf(nb[n], 64, "%s", gal_word(words, "trees", tree_keys[i])); names[n] = nb[n]; n++; }
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
typedef struct { cm_job j; char id[32]; int test; gal_item it; char *blob, *builds; char err[400]; int ok; } ItemJob;
static void title_of(const gal_item *it, char *o, size_t n) { gal_item_title(it, words, o, n); }
extern sg_engine *tf_engine_for_gallery(char *err, size_t errn);        /* tf_ui.c: the engine, opened on this thread */
static void item_run(cm_job *j) {
    ItemJob *b = (ItemJob *)j;
    if (gal_fetch_item(b->id, &b->it, &b->blob, b->err, sizeof b->err) || !b->blob) { if (!b->err[0]) snprintf(b->err, sizeof b->err, "%s", TR("Could not load this team. Try again later.")); return; }
    if (!b->test) { b->ok = 1; return; }
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
static void test_click(void *ud, DSLObj w) { (void)w; card_action((int)(intptr_t)ud, 1); }

/* ------------------------------------------------------------------ drawing */
static void kind_click(void *ud, DSLObj w) { (void)w; q.chars = (int)(intptr_t)ud; fetch(0); paint(); }
static void sort_click(void *ud, DSLObj w) { (void)w; q.recent = (int)(intptr_t)ud; fetch(0); paint(); }
static void race_change(void *ud, DSLObj w) { (void)ud; q.race = U->value_get(w); fetch(0); paint(); }
static void tree_change(void *ud, DSLObj w) { (void)ud; q.tree = U->value_get(w); fetch(0); paint(); }
static void complete_change(void *ud, DSLObj w) { (void)ud; q.complete = U->toggle_get(w); fetch(0); paint(); }
static void more_click(void *ud, DSLObj w) { (void)ud; (void)w; if (more && !loading) { fetch(page + 1); paint(); } }

/* a card = 5 widgets (panel, title, one text block, 2 buttons): the kit holds at most 1024 live widgets */
static void card(DSLObj p, int i, float x, float y) {
    const gal_item *it = &items[i]; char t[300], nm[48];
    cmw_flat(U->panel(p, x, y, CARD_W, CARD_H, 0), 0x2D2A31FF);
    title_of(it, t, sizeof t);
    snprintf(nm, sizeof nm, "gallery:c%d.title", i);
    cmw_named(U->text(p, x + 14, y + 8, CARD_W - 28, 26, t, 18, DSL_TXT_LEFT, GOLD), CMW_TEXT, nm, NULL, NULL, t);
    char au[160], body_t[1400]; size_t k = 0; gal_author_name(it, words, au, sizeof au);
    k += (size_t)snprintf(body_t + k, sizeof body_t - k, TR("by %s  -  Opens %d  -  Copies %d  -  Likes %d"), au, it->opens, it->copies, it->likes);
    if (k >= sizeof body_t) k = sizeof body_t - 1;
    for (int u = 0; u < it->nunits && u < 4 && k < sizeof body_t; u++) {
        const gal_unit *g = &it->units[u]; char m[96], tr[120] = ""; size_t q2 = 0;
        if (it->team) gal_member_name(it, u, words, m, sizeof m); else snprintf(m, sizeof m, "%s", gal_word(words, "races", g->race));
        for (int z = 0; z < g->ntrees && z < 3; z++) q2 += (size_t)snprintf(tr + q2, sizeof tr - q2, "%s%s", z ? ", " : "", gal_word(words, "trees", g->trees[z])[0] ? gal_word(words, "trees", g->trees[z]) : g->trees[z]);
        char ml[240]; snprintf(ml, sizeof ml, TR("%s  Lv %d"), m, g->lvl);
        k += (size_t)snprintf(body_t + k, sizeof body_t - k, "\n%s%s%s", ml, tr[0] ? "  -  " : "", tr);
        if (k >= sizeof body_t) k = sizeof body_t - 1;
    }
    if (it->nunits > 4 && k < sizeof body_t) snprintf(body_t + k, sizeof body_t - k, "\n+%d", it->nunits - 4);
    dsi_utf8_fix_end(body_t);
    U->text(p, x + 14, y + 36, CARD_W - 28, CARD_H - 36 - 50, body_t, 14, DSL_TXT_LEFT, MAIN);
    snprintf(nm, sizeof nm, "gallery:c%d.save", i);
    cmw_named(U->button(p, x + 14, y + CARD_H - 46, 220, 38, TR("Save to My Units"), save_click, (void *)(intptr_t)i), CMW_BUTTON, nm, save_click, (void *)(intptr_t)i, TR("Save to My Units"));
    snprintf(nm, sizeof nm, "gallery:c%d.test", i);
    cmw_named(U->button(p, x + CARD_W - 14 - 180, y + CARD_H - 46, 180, 38, TR("Test Fight"), test_click, (void *)(intptr_t)i), CMW_BUTTON, nm, test_click, (void *)(intptr_t)i, TR("Test Fight"));
}
static void paint(void) {
    if (!cmw_is_open("gallery") || !body) return;
    cmw_forget("gallery:c"); cmw_forget("gallery:f.");
    if (grid_holder) U->destroy(grid_holder);
    grid_holder = cmw_flat(U->panel(body, 0, 0, WIN_W, BODY_H - 70, 0), 0x00000000);
    cm_raycast(grid_holder, false);                                  /* the search row under it (made once) keeps its clicks */
    /* filters (rebuilt with the grid: the race / tree lists arrive with the words) */
    const char *kinds[2] = { TR("Teams"), TR("Units") }, *sorts[2] = { TR("Most popular"), TR("Newest") };
    for (int s = 0; s < 2; s++) {
        char nm[40]; snprintf(nm, sizeof nm, "gallery:f.kind%d", s);
        cmw_named(U->button(grid_holder, 24 + s * 134, 12, 128, 38, kinds[s], kind_click, (void *)(intptr_t)s), CMW_BUTTON, nm, kind_click, (void *)(intptr_t)s, kinds[s]);
        if (q.chars == s) cmw_flat(U->panel(grid_holder, 24 + s * 134, 52, 128, 3, 0), GOLD);
        snprintf(nm, sizeof nm, "gallery:f.sort%d", s);
        cmw_named(U->button(grid_holder, 310 + s * 164, 12, 158, 38, sorts[s], sort_click, (void *)(intptr_t)s), CMW_BUTTON, nm, sort_click, (void *)(intptr_t)s, sorts[s]);
        if (q.recent == s) cmw_flat(U->panel(grid_holder, 310 + s * 164, 52, 158, 3, 0), GOLD);
    }
    cmw_named(U->toggle(grid_holder, 660, 13, 356, TR("Complete only"), q.complete, complete_change, NULL), CMW_TOGGLE, "gallery:f.complete", complete_change, NULL, TR("Complete only"));
    if (words) {
        nrace = gal_word_keys(words, "races", race_keys, 16); ntree = gal_word_keys(words, "trees", tree_keys, 40);
        const char *rl[17] = { TR("Any") }, *tl[41] = { TR("Any") };
        static char rn[16][64], tn[40][64];
        for (int i = 0; i < nrace; i++) { snprintf(rn[i], 64, "%s", gal_word(words, "races", race_keys[i])); rl[i + 1] = rn[i]; }
        for (int i = 0; i < ntree; i++) { snprintf(tn[i], 64, "%s", gal_word(words, "trees", tree_keys[i])); tl[i + 1] = tn[i]; }
        cmw_named(U->value_row(grid_holder, 24, 64, 488, TR("Races"), rl, nrace + 1, q.race, race_change, NULL), CMW_VALUE, "gallery:f.race", race_change, NULL, rl[q.race]);
        cmw_named(U->value_row(grid_holder, 528, 64, 488, TR("Weapon trees"), tl, ntree + 1, q.tree, tree_change, NULL), CMW_VALUE, "gallery:f.tree", tree_change, NULL, tl[q.tree]);
    }
    DSLObj sc = U->scroll(grid_holder, 24, GRID_Y, WIN_W - 36, BODY_H - 70 - GRID_Y - 8);
    float y = 0; int k = 0;
    for (int i = 0; i < nitems; i++) {
        if (!visible(&items[i])) continue;                             /* a name search shows only the builds that match */
        float x = (k % 2) * (CARD_W + 12);
        card(sc, i, x, y);
        if (k % 2) y += CARD_H + 12;
        k++;
    }
    if (k % 2) y += CARD_H + 12;
    U->scroll_height(sc, y);
    if (w_status) U->destroy(w_status);                               /* the kit has no colour setter: a problem shows orange */
    w_status = cmw_named(U->text(body, 24, BODY_H - 58, WIN_W - 300, 52, status, 15, DSL_TXT_LEFT | DSL_TXT_WRAP, status_warn ? WARN : MAIN), CMW_TEXT, "gallery:status", NULL, NULL, status);
    U->set_active(w_more, more);
    U->set_enabled(w_more, more && !loading);
}
static void closed(void *ud) { (void)ud; body = grid_holder = w_status = w_more = w_field = w_by = NULL; cmw_forget("gallery:"); }
void gal_open(void) {
    U = cmw_ui(); if (!U) return;
    if (!items) items = calloc(MAXIT, sizeof *items);
    if (!items) return;
    body = cmw_open("gallery", TR("Wiki Build Gallery"), WIN_W, WIN_H, closed, NULL);
    if (!body) return;
    grid_holder = NULL;
    /* the search row (made once per window, so typing is never cut by a page that arrives) */
    s_gen++; s_name[0] = s_by[0] = s_by_name[0] = 0;
    w_field = cmw_named(U->field(body, 24, SEARCH_Y, 520, 40, TR("Search: name, author, race, background or weapon"), "", search_click, NULL),
                        CMW_FIELD, "gallery:s.field", search_click, NULL, "");
    cmw_named(U->button(body, 552, SEARCH_Y, 150, 40, TR("Search"), search_click, NULL), CMW_BUTTON, "gallery:s.go", search_click, NULL, TR("Search"));
    cmw_named(U->button(body, 708, SEARCH_Y, 110, 40, TR("Clear"), clear_click, NULL), CMW_BUTTON, "gallery:s.clear", clear_click, NULL, TR("Clear"));
    w_by = cmw_named(U->text(body, 828, SEARCH_Y, WIN_W - 24 - 828, 40, "", 14, DSL_TXT_LEFT, SUB), CMW_TEXT, "gallery:s.by", NULL, NULL, "");
    cmw_flat(U->panel(body, 24, BODY_H - 66, WIN_W - 48, 1, 0), 0x633D33FF);
    w_status = NULL;
    w_more = cmw_named(U->button(body, WIN_W - 24 - 220, BODY_H - 58, 220, 46, TR("Show more"), more_click, NULL), CMW_BUTTON, "gallery:more", more_click, NULL, TR("Show more"));
    fetch(0);
    paint();
}

/* mu_link's worker thread: the builder of a gallery item (a /build/<id> short link), named as the wiki names it. 1 found */
static int gal_link_author(const char *id, char *out, size_t n) {
    gal_item *it = calloc(1, sizeof *it); char *blob = NULL, e[300]; int ok = 0;
    if (n) out[0] = 0;
    if (it && !gal_fetch_item(id, it, &blob, e, sizeof e)) {
        gal_words *w = it->au_pseudo ? gal_words_fetch(e, sizeof e) : NULL;   /* a pseudonym is made of the wiki's words */
        gal_author_name(it, w, out, n); gal_words_free(w);
        ok = out[0] != 0;
    }
    free(blob); free(it);
    return ok;
}
static void gal_relang(void) { if (cmw_is_open("gallery")) gal_open(); }   /* the language changed: drawn again */
static void test_state(char *out, size_t n) {
    size_t k = (size_t)snprintf(out, n, "\"gal\":{\"items\":%d,\"loading\":%d,\"more\":%d,\"words\":%d,\"status\":\"%.300s\",\"ids\":[", nitems, loading, more, words != NULL, status);
    for (int i = 0; i < nitems && i < 24 && k + 40 < n; i++) k += (size_t)snprintf(out + k, n - k, "%s\"%s\"", i ? "," : "", items[i].id);
    snprintf(out + k, n - k, "]}");
}

__attribute__((visibility("default"))) void cm_gallery_init(const DSLHost *host, DSLPlugin *self) {
    if (cmw_init(host, self)) return;
    if (cm_work_start()) { cmw_log("gal: worker thread did not start: Gallery is off"); return; }
    cmw_test_opener("gallery", gal_open);
    cmw_test_state(test_state);
    cm_home_add("gallery", TR_NOOP("Wiki Build Gallery"), NULL, 30, gal_open);
    mul_gallery_author = gal_link_author;                        /* My Units' "Add from wiki link": a /build/<id> link's builder */
    cm_i18n_on_change(gal_relang);
}
