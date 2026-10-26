/* saveunits_test - Test Fights' World units (owner 2026-10-25: "Test fights should allow to select directly people from one
 * of your current saves"): a unit picked from a save enters the test fight with the save's level, gear and skills.
 * The picks go the way the game's Start does (tf_ui.c save_builds_json: each unit by mu_save_build_obj, in pick order)
 * through the wiki's save generator (sg_js, QuickJS, the same scripts and templates); the test world it writes is read
 * back with mu_save and each of its units is compared with mu_save_build_ex of the source save.
 * Fails when: a picked unit is missing from the fight or another unit takes its place; its level, race, background,
 * talents, worn items or their rolls, learned skills, skill trees, inscriptions, soul stones or name differ from the save;
 * its skill bar is not the save's bar (the save's skills the build has, in the save's order); the generator reports a
 * problem; the source save changes.
 * Bag and effects (owner 2026-10-25), with the builds the game sends (mu_save_fight_build, glue sg_glue.js carry):
 *  - its bag in the fight is not its bag in the save, slot by slot (key, amount, AutoFixed, the item entity's components);
 *  - effects switch off: any affecter from the save shows up that the build alone does not give (the fight's holders
 *    differ from the plain build's);
 *  - effects switch on: an affecter of the save is missing or has another Stack / Duration / InitialDuration, or a
 *    difficulty holder is not the fight's own;
 *  - battle kits on: an item of the save's bag or a kit item is missing;
 *  - a key the game does not know (the test's known() refuses one) is not left out and counted.
 * Auto Skill (owner 2026-10-25: "Auto skill was also activated on my units despite not turning it on"):
 *  - a unit from a save (fight build) does not keep the save's Auto Skill bit (BehaviourOption 1) or a per-skill auto-use;
 *    each save also runs as a copy with every unit's bit and every auto-use flipped (autoflip_*), so both ways are checked;
 *  - a stored build (no save: My Units, Gallery, Builder) does not get Auto Skill on (0.1.36: else it never casts a skill).
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

/* one test world from these builds (kits: JSON array text, aff: the effects switch); NULL + a FAIL when the generator
   gives no file or a problem */
static int g_aff, g_noknow;                                      /* the builds' effects switch; refuse one key (known()) */
static int known_t(int kind, const char *key) { return !(g_noknow && !strcmp(key, kind ? "AFFECTER_OatPorridge" : "ITEM_OatPorridge")); }
static char *gen(sg_engine *e, const char *src, const char *out, const char *tag, const char *team, const int *pick, int np,
                 const mu_save_unit *su, int fight, const char *kits, int *si, int *sa) {
    char err[2048] = "";
    jv *opts = jobj(), *arr = jarr();
    jset(opts, "mode", jstrv("dps")); jset(opts, "combat", jstrv("Normal")); jset(opts, "kits", jparse(kits, strlen(kits), err, sizeof err));
    jset(opts, "kitInsc", jbool(0)); jset(opts, "team", jstrv(team));   /* raw save text, quotes already out (mu_world_header) */
    for (int k = 0; k < np; k++) {
        jv *b = fight ? mu_save_fight_build(src, su[pick[k]].guid, g_aff, g_noknow ? known_t : NULL, si, sa, err, sizeof err)
                      : mu_save_build_obj(src, su[pick[k]].guid, err, sizeof err);
        CHECK(b != NULL, "%s: unit %s as a build: %s", src, su[pick[k]].guid, err);
        if (!b) { jfree(opts); jfree(arr); return NULL; }
        jpush(arr, b);
    }
    jset(opts, "builds", arr);
    char *oj = jdump(opts, 0, NULL); jfree(opts);
    sg_result r; memset(&r, 0, sizeof r);
    int rc = sg_generate(e, oj, &r, err, sizeof err); free(oj);
    CHECK(!rc && !r.problems, "%s [%s]: the generator makes the test world (%s) %s", src, tag, err, r.meta ? r.meta : "");
    if (rc || r.problems) { if (!rc) sg_result_free(&r); return NULL; }
    const char *bn = strrchr(src, '/'); bn = bn ? bn + 1 : src;
    char gp[2048]; snprintf(gp, sizeof gp, "%s/fight_%s_%s", out, tag, bn);
    FILE *f = fopen(gp, "wb"); if (f) { fwrite(r.file, 1, r.file_n, f); fclose(f); }
    sg_result_free(&r);
    return strdup(gp);
}
static const jv *holder(const jv *hs, const char *key) {
    for (size_t i = 0; hs && i < hs->cnt; i++) if (jstreq(jget(hs->v[i], "Key"), key)) return hs->v[i];
    return NULL;
}
static int is_diff(const char *k) { return !strncmp(k, "AFFECTER_PlayerDifficulty", 25) || !strncmp(k, "AFFECTER_PlayerManagementDifficulty", 35); }

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
    g_aff = 0; g_noknow = 0;
    char *gp = gen(e, src, out, "plain", w.name, pick, np, su, 0, "[]", NULL, NULL);
    if (!gp) { free(b0); return; }
    const char *bn = strrchr(src, '/'); bn = bn ? bn + 1 : src;
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
    /* bag + effects: the builds the game sends (switch off, switch on, kits on, one key the game does not know) */
    char *gb[4] = { 0 }; int si[4] = { 0 }, sa[4] = { 0 };
    const char *tags[4] = { "bag", "aff", "kits", "unknown" }, *kits[4] = { "[]", "[]", "[\"basic\"]", "[]" };
    for (int c = 0; c < 4; c++) { g_aff = c == 1 || c == 3; g_noknow = c == 3; gb[c] = gen(e, src, out, tags[c], w.name, pick, np, su, 1, kits[c], &si[c], &sa[c]); }
    int want_si = 0, want_sa = 0;
    for (int k = 0; k < np && k < ng; k++) {
        const mu_save_unit *s0 = &su[pick[k]];
        jv *sbag = NULL, *saff = NULL; char e1[400] = "";
        if (mu_save_unit_carry(src, s0->guid, &sbag, &saff, e1, sizeof e1)) { CHECK(0, "%s unit %d: source carry: %s", bn, k, e1); continue; }
        for (size_t q = 0; q < sbag->cnt; q++) want_si += jstreq(jget(sbag->v[q], "key"), "ITEM_OatPorridge");
        want_sa += holder(saff, "AFFECTER_OatPorridge") != NULL;
        for (int c = 0; c < 4; c++) {
            if (!gb[c]) continue;
            static mu_save_unit cu[64]; int nc = mu_save_units(gb[c], cu, 64, err, sizeof err);
            jv *fbag = NULL, *faff = NULL, *pbag = NULL, *paff = NULL; char e2[400] = "";
            if (nc != np || mu_save_unit_carry(gb[c], cu[k].guid, &fbag, &faff, e2, sizeof e2) || mu_save_unit_carry(gp, gu[k].guid, &pbag, &paff, e2, sizeof e2)) {
                CHECK(0, "%s [%s] unit %d: read back (%d units) %s", bn, tags[c], k, nc, e2); jfree(fbag); jfree(faff); jfree(pbag); jfree(paff); continue;
            }
            if (c == 0 || c == 1) {                                      /* the bag, slot by slot */
                int same = fbag->cnt == sbag->cnt; char why[300] = "";
                for (size_t q = 0; same && q < sbag->cnt; q++) {
                    const jv *x = sbag->v[q], *y = fbag->v[q];
                    same = jequal(jget(x, "slot"), jget(y, "slot")) && jequal(jget(x, "key"), jget(y, "key")) && jequal(jget(x, "amount"), jget(y, "amount"))
                           && jequal(jget(x, "fixed"), jget(y, "fixed")) && jequal(jget(x, "comps"), jget(y, "comps"));
                    if (!same) { char *kx = jstrdup_raw(jget(x, "key")), *ky = jstrdup_raw(jget(y, "key")); snprintf(why, sizeof why, "slot %zu: %s vs %s", q, kx, ky); free(kx); free(ky); }
                }
                CHECK(same, "%s [%s] unit %d (%s): its bag = the save's bag (%zu vs %zu items) %s", bn, tags[c], k, s0->name, sbag->cnt, fbag->cnt, why);
            }
            if (c == 0) {                                                /* Auto Skill: the save's bit and per-skill auto-use; a stored build: on */
                int so = 0, fo = 0, po = 0; jv *sau = NULL, *fau = NULL; char e3[400] = "", why[300] = "";
                if (mu_save_unit_auto(src, s0->guid, &so, &sau, e3, sizeof e3) || mu_save_unit_auto(gb[c], cu[k].guid, &fo, &fau, e3, sizeof e3)
                    || mu_save_unit_auto(gp, gu[k].guid, &po, NULL, e3, sizeof e3)) CHECK(0, "%s [auto] unit %d: read %s", bn, k, e3);
                else {
                    int same = 0, off = 0;
                    for (size_t q = 0; q < fau->cnt; q++) {
                        const jv *sv = jget(sau, fau->k[q]);
                        if (!sv || (sv->t != JN_TRUE && sv->t != JN_FALSE)) continue;
                        if (jtruthy(sv) == jtruthy(fau->v[q])) same++; else if (!why[0]) snprintf(why, sizeof why, " (%s: %d in the save)", fau->k[q], jtruthy(sv));
                        off += !jtruthy(sv);
                    }
                    CHECK((fo & 1) == (so & 1), "%s [auto] unit %d (%s): Auto Skill %s as in the save (Option %d in the save, %d in the fight)", bn, k, s0->name, so & 1 ? "on" : "off", so, fo);
                    CHECK(!why[0] && (same > 0 || !sau->cnt), "%s [auto] unit %d (%s): per-skill auto-use as in the save (%d skills, %d off in the save)%s", bn, k, s0->name, same, off, why);
                    CHECK(po & 1, "%s [plain] unit %d (%s): a stored build gets Auto Skill on (Option %d)", bn, k, s0->name, po);
                }
                jfree(sau); jfree(fau);
            }
            if (c == 0) {                                                /* switch off: the plain build's holders, nothing from the save */
                int same = faff->cnt == paff->cnt;
                for (size_t q = 0; same && q < paff->cnt; q++) { char *kk = jstrdup_raw(jget(paff->v[q], "Key")); same = jequal(paff->v[q], holder(faff, kk)); free(kk); }
                CHECK(same, "%s [bag] unit %d (%s): effects off = only the build's own holders (%zu vs %zu)", bn, k, s0->name, faff->cnt, paff->cnt);
            }
            if (c == 1) {                                                /* switch on: every save holder as is, difficulty the fight's */
                int ok = 1; char why[300] = "";
                for (size_t q = 0; q < saff->cnt && ok; q++) {
                    char *kk = jstrdup_raw(jget(saff->v[q], "Key"));
                    if (!is_diff(kk)) { const jv *h = holder(faff, kk); ok = h && jequal(jget(h, "Stack"), jget(saff->v[q], "Stack")) && jequal(jget(h, "Duration"), jget(saff->v[q], "Duration"))
                                                                && jequal(jget(h, "InitialDuration"), jget(saff->v[q], "InitialDuration"));
                                        if (!ok) snprintf(why, sizeof why, "%s", kk); }
                    free(kk);
                }
                for (size_t q = 0; q < faff->cnt && ok; q++) {
                    char *kk = jstrdup_raw(jget(faff->v[q], "Key"));
                    if (is_diff(kk)) { ok = !strcmp(kk, "AFFECTER_PlayerDifficultyNormal") || !strcmp(kk, "AFFECTER_PlayerManagementDifficultyNormal"); if (!ok) snprintf(why, sizeof why, "%s", kk); }
                    free(kk);
                }
                CHECK(ok, "%s [aff] unit %d (%s): effects on = the save's holders as they are (%zu in the save, %zu in the fight) %s", bn, k, s0->name, saff->cnt, faff->cnt, why);
            }
            if (c == 2) {                                                /* kits on: the save's bag and the kit both there */
                int ok = 1;
                for (size_t q = 0; q < sbag->cnt && ok; q++) {
                    ok = 0; for (size_t r = 0; r < fbag->cnt && !ok; r++) ok = jequal(jget(fbag->v[r], "key"), jget(sbag->v[q], "key")) && jequal(jget(fbag->v[r], "amount"), jget(sbag->v[q], "amount"));
                }
                int kit = 0; for (size_t r = 0; r < fbag->cnt; r++) kit |= jstreq(jget(fbag->v[r], "key"), "ITEM_LumaLog");
                CHECK(ok && kit, "%s [kits] unit %d (%s): the save's bag and the Basic kit (%zu items)", bn, k, s0->name, fbag->cnt);
            }
            if (c == 3) {                                                /* a key the game does not know: left out */
                int left = 1; for (size_t r = 0; r < fbag->cnt; r++) left &= !jstreq(jget(fbag->v[r], "key"), "ITEM_OatPorridge");
                left &= !holder(faff, "AFFECTER_OatPorridge") || holder(paff, "AFFECTER_OatPorridge") != NULL;
                CHECK(left, "%s [unknown] unit %d (%s): a key the game does not know is left out", bn, k, s0->name);
            }
            jfree(fbag); jfree(faff); jfree(pbag); jfree(paff);
        }
        jfree(sbag); jfree(saff);
    }
    CHECK(si[3] == want_si && sa[3] == want_sa, "%s [unknown]: the left-out keys are counted (items %d of %d, effects %d of %d)", bn, si[3], want_si, sa[3], want_sa);
    for (int c = 0; c < 4; c++) free(gb[c]);
    free(gp);
    size_t n1 = 0; char *b1 = slurp(src, &n1);
    CHECK(b1 && n1 == n0 && !memcmp(b0, b1, n0), "%s: the source save is unchanged", src);
    free(b0); free(b1);
}

/* a copy of save `src` in `out` (autoflip_<name>) with every clan unit's Auto Skill bit and every per-skill auto-use flipped:
   the owner's saves have Auto Skill off on every unit, so this is the "on stays on" way. NULL when it cannot be made. */
static char *flip_copy(const char *src, const char *out) {
    char err[400] = ""; jv *s = jload(src, err, sizeof err);
    static mu_save_unit su[256]; int nu = s ? mu_save_units(src, su, 256, err, sizeof err) : -1;
    jv *ec = s ? jpath(s, "EntityComponentSaveData.EntityComponents") : NULL;
    for (int i = 0; ec && i < nu; i++) {
        jv *cs = jget(ec, su[i].guid);
        for (size_t c = 0; cs && cs->t == JN_ARR && c < cs->cnt; c++) {
            jv *d = jget(cs->v[c], "Data"), *ty = jget(cs->v[c], "Type"), *au = jget(d, "AutoUse");
            if (ty && jnumv(ty) == 1 && d) jset(d, "Option", jnum_i((long long)jnumv(jget(d, "Option")) ^ 1));
            for (size_t q = 0; ty && jnumv(ty) == 8 && au && au->t == JN_OBJ && q < au->cnt; q++) {
                jv *v = jbool(!jtruthy(au->v[q])); jfree(au->v[q]); au->v[q] = v;
            }
        }
    }
    const char *bn = strrchr(src, '/'); bn = bn ? bn + 1 : src;
    char p[2048]; snprintf(p, sizeof p, "%s/autoflip_%s", out, bn);
    size_t n = 0; char *t = ec ? jdump(s, 2, &n) : NULL; FILE *f = t ? fopen(p, "wb") : NULL;
    int ok = f && fwrite(t, 1, n, f) == n; if (f) fclose(f);
    free(t); jfree(s);
    CHECK(ok, "%s: the Auto Skill flipped copy (%s)", src, err);
    return ok ? strdup(p) : NULL;
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
    for (int i = 4; i < argc; i++) {
        one_save(e, argv[i], argv[3]);
        char *fc = flip_copy(argv[i], argv[3]);
        if (fc) { one_save(e, fc, argv[3]); free(fc); }
    }
    sg_close(e); mu_save_cache_clear();
    printf("RESULT %s: %d passed, %d failed\n", failc ? "FAIL" : "PASS", pass, failc);
    return failc ? 1 : 0;
}
