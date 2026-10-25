/* saveunits_test - Test Fights' World units (owner 2026-10-25: "Test fights should allow to select directly people from one
 * of your current saves"): a unit picked from a save enters the test fight with the save's level, gear and skills.
 * The picks go the way the game's Start does (tf_ui.c save_builds_json: each unit by mu_save_build_obj, in pick order)
 * through the wiki's save generator (sg_js, QuickJS, the same scripts and templates); the test world it writes is read
 * back with mu_save and each of its units is compared with mu_save_build_ex of the source save.
 * Fails when: a picked unit is missing from the fight or another unit takes its place; its level, race, background,
 * talents, worn items or their rolls, learned skills, skill trees, inscriptions, soul stones or name differ from the save;
 * its skill bar is not the save's bar (the save's skills the build has, in the save's order); the generator reports a
 * problem; the source save changes.
 * usage: saveunits_test <builder dir> <templates dir> <out dir> SAVE...   (each SAVE is read only; pass copies) */
#include "../sg_js.h"
#include "../../fe/fe_json.h"
#include "../../units/mu_save.h"
#include "../../units/mu_worlds.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int pass, failc;
#define CHECK(c, ...) do { if (c) pass++; else { failc++; printf("FAIL "); printf(__VA_ARGS__); printf("\n"); } } while (0)

static char *slurp(const char *path, size_t *n) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END); long L = ftell(f); fseek(f, 0, SEEK_SET);
    char *b = malloc((size_t)L + 1);
    if (b && fread(b, 1, (size_t)L, f) != (size_t)L) { free(b); b = NULL; }
    if (b) { b[L] = 0; if (n) *n = (size_t)L; }
    fclose(f);
    return b;
}
static const char *g_tpl;
static int tpl_from_dir(void *u, const char *file, char **text, size_t *len, char *err, size_t errn) {
    (void)u;
    char p[2048]; snprintf(p, sizeof p, "%s/%s", g_tpl, file);
    *text = slurp(p, len);
    if (!*text) { snprintf(err, errn, "no file %s", p); return -1; }
    return 0;
}
static int cmp_int(const void *a, const void *b) { return *(const int *)a - *(const int *)b; }
static int same_set(const int *a, int na, const int *b, int nb) {
    if (na != nb) return 0;
    int x[256], y[256]; if (na > 256) return 0;
    memcpy(x, a, sizeof *a * (size_t)na); memcpy(y, b, sizeof *b * (size_t)nb);
    qsort(x, (size_t)na, sizeof *x, cmp_int); qsort(y, (size_t)nb, sizeof *y, cmp_int);
    return !memcmp(x, y, sizeof *x * (size_t)na);
}
static int same_gear(const wk_build *a, const wk_build *b, char *why, size_t n) {
    for (int s = 0; s < WK_SLOTS; s++) {
        if (a->eq[s] != b->eq[s]) { snprintf(why, n, "slot %s: %s vs %s", wk_slot_name(s), a->eq[s] >= 0 ? wk_item_key(a->eq[s]) : "-", b->eq[s] >= 0 ? wk_item_key(b->eq[s]) : "-"); return 0; }
        if (a->ntag[s] != b->ntag[s]) { snprintf(why, n, "slot %s: %d rolls vs %d", wk_slot_name(s), a->ntag[s], b->ntag[s]); return 0; }
        for (int t = 0; t < a->ntag[s]; t++)
            if (a->tag[s][t].idx != b->tag[s][t].idx || a->tag[s][t].v != b->tag[s][t].v) { snprintf(why, n, "slot %s: roll %d differs", wk_slot_name(s), t); return 0; }
    }
    return 1;
}

/* the save's bar as the fight should get it: its skills that the generated unit's build has, in the save's order (the
   builder's quickFor drops a skill the build cannot use, e.g. Emergency Escape) */
static int bar_ok(const mu_save_extra *src, const mu_save_extra *gen, char *why, size_t n) {
    int k = 0;
    for (int i = 0; i < src->nbar && k < gen->nbar; i++) if (!strcmp(src->bar[i], gen->bar[k])) k++;
    size_t o = 0; why[0] = 0;
    for (int i = 0; i < src->nbar && o + 70 < n; i++) o += (size_t)snprintf(why + o, n - o, "%s%s", i ? "," : "save: ", src->bar[i]);
    for (int i = 0; i < gen->nbar && o + 70 < n; i++) o += (size_t)snprintf(why + o, n - o, "%s%s", i ? "," : " | fight: ", gen->bar[i]);
    return k == gen->nbar && gen->nbar > 0;
}

static void one_save(sg_engine *e, const char *src, const char *out) {
    char err[2048] = "";
    size_t n0 = 0; char *b0 = slurp(src, &n0);
    CHECK(b0 != NULL, "%s: read", src); if (!b0) return;
    static mu_save_unit su[256]; int nu = mu_save_units(src, su, 256, err, sizeof err);
    CHECK(nu >= 1, "%s: units (%d) %s", src, nu, err); if (nu < 1) { free(b0); return; }
    mu_world w; memset(&w, 0, sizeof w); mu_world_header(src, &w);
    /* up to 4 picks spread over the clan's units (first, last, two between), in that order */
    int pick[4], np = 0, want = nu < 4 ? nu : 4;
    for (int k = 0; k < want; k++) pick[np++] = want == 1 ? 0 : (int)((long)k * (nu - 1) / (want - 1));
    jv *opts = jobj(), *arr = jarr();
    jset(opts, "mode", jstrv("dps")); jset(opts, "combat", jstrv("Normal")); jset(opts, "kits", jarr());
    jset(opts, "kitInsc", jbool(0)); jset(opts, "team", jstrv(w.name));   /* raw save text, quotes already out (mu_world_header) */
    for (int k = 0; k < np; k++) {
        jv *b = mu_save_build_obj(src, su[pick[k]].guid, err, sizeof err);
        CHECK(b != NULL, "%s: unit %s as a build: %s", src, su[pick[k]].guid, err);
        if (!b) { jfree(opts); jfree(arr); free(b0); return; }
        jpush(arr, b);
    }
    jset(opts, "builds", arr);
    char *oj = jdump(opts, 0, NULL); jfree(opts);
    sg_result r; memset(&r, 0, sizeof r);
    int rc = sg_generate(e, oj, &r, err, sizeof err); free(oj);
    CHECK(!rc && !r.problems, "%s: the generator makes the test world (%s) %s", src, err, r.meta ? r.meta : "");
    if (rc || r.problems) { if (!rc) sg_result_free(&r); free(b0); return; }
    const char *bn = strrchr(src, '/'); bn = bn ? bn + 1 : src;
    char gp[2048]; snprintf(gp, sizeof gp, "%s/fight_%s", out, bn);
    FILE *f = fopen(gp, "wb"); if (f) { fwrite(r.file, 1, r.file_n, f); fclose(f); }
    sg_result_free(&r);
    static mu_save_unit gu[64]; int ng = mu_save_units(gp, gu, 64, err, sizeof err);
    CHECK(ng == np, "%s: the fight holds the %d picked units (%d) %s", src, np, ng, err);
    for (int k = 0; k < np && k < ng; k++) {
        static wk_build a, b; static mu_save_extra xa, xb; char ea[400] = "", eb[400] = "", why[1200] = "";
        const mu_save_unit *s = &su[pick[k]], *g = &gu[k];
        int ra = mu_save_build_ex(src, s->guid, &a, &xa, ea, sizeof ea), rb = mu_save_build_ex(gp, g->guid, &b, &xb, eb, sizeof eb);
        CHECK(!ra && !rb, "%s unit %d: read back (%s | %s)", bn, k, ea, eb); if (ra || rb) continue;
        int nit = 0; for (int q = 0; q < WK_SLOTS; q++) nit += a.eq[q] >= 0;
        printf("  %s unit %d: %s lvl %d (save %d) | %d items | %d skills | %d trees | %d inscriptions | bar %d of %d\n", bn, k, s->name, b.lvl, s->level,
               nit, a.nlearned, a.ntrees, a.ninsc, xb.nbar, xa.nbar);
        CHECK(!strcmp(s->name, g->name), "%s unit %d: the picked unit, in pick order (%s vs %s)", bn, k, s->name, g->name);
        CHECK(g->level == s->level && a.lvl == b.lvl, "%s unit %d (%s): level %d in the fight, %d in the save", bn, k, s->name, g->level, s->level);
        CHECK(a.race == b.race && a.bg == b.bg && !memcmp(a.tal, b.tal, sizeof a.tal), "%s unit %d (%s): race / background / talents", bn, k, s->name);
        CHECK(same_gear(&a, &b, why, sizeof why), "%s unit %d (%s): worn items and rolls: %s", bn, k, s->name, why);
        CHECK(same_set(a.learned, a.nlearned, b.learned, b.nlearned), "%s unit %d (%s): learned skills (%d vs %d)", bn, k, s->name, a.nlearned, b.nlearned);
        CHECK(same_set(a.trees, a.ntrees, b.trees, b.ntrees), "%s unit %d (%s): skill trees (%d vs %d)", bn, k, s->name, a.ntrees, b.ntrees);
        CHECK(same_set(a.insc, a.ninsc, b.insc, b.ninsc) && same_set(a.souls, a.nsouls, b.souls, b.nsouls), "%s unit %d (%s): inscriptions (%d vs %d) and soul stones (%d vs %d)",
              bn, k, s->name, a.ninsc, b.ninsc, a.nsouls, b.nsouls);
        CHECK(bar_ok(&xa, &xb, why, sizeof why), "%s unit %d (%s): the save's skill bar (%s)", bn, k, s->name, why);
        a.name[0] = b.name[0] = 0;
        if (wk_hash(&a, 1) != wk_hash(&b, 1)) printf("  note: %s unit %d: other build fields differ (wk_hash), see wk_json of both\n", bn, k);
    }
    size_t n1 = 0; char *b1 = slurp(src, &n1);
    CHECK(b1 && n1 == n0 && !memcmp(b0, b1, n0), "%s: the source save is unchanged", src);
    free(b0); free(b1);
}

int main(int argc, char **argv) {
    if (argc < 5) { fprintf(stderr, "usage: saveunits_test <builder dir> <templates dir> <out dir> SAVE...\n"); return 2; }
    g_tpl = argv[2];
    char p[2048], err[2048] = ""; sg_sources s = {0};
    snprintf(p, sizeof p, "%s/data.js", argv[1]);    s.data_js = slurp(p, &s.data_n);
    snprintf(p, sizeof p, "%s/builder.js", argv[1]); s.builder_js = slurp(p, &s.builder_n);
    snprintf(p, sizeof p, "%s/savegen.js", argv[1]); s.savegen_js = slurp(p, &s.savegen_n);
    sg_engine *e = s.data_js && s.builder_js && s.savegen_js ? sg_open(&s, tpl_from_dir, NULL, NULL, err, sizeof err) : NULL;
    if (!e) { fprintf(stderr, "engine: %s\n", err[0] ? err : "the wiki scripts are missing"); return 1; }
    for (int i = 4; i < argc; i++) one_save(e, argv[i], argv[3]);
    sg_close(e); mu_save_cache_clear();
    printf("RESULT %s: %d passed, %d failed\n", failc ? "FAIL" : "PASS", pass, failc);
    return failc ? 1 : 0;
}
