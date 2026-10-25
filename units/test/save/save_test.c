/* mu_save tests (no game). Run: custom/units/test/save/run.sh
 *   save_test list SAVE...            each save: list the units, build + card each one, one line per unit
 *   save_test rt BUILDS.jsonl TMPDIR  round trip: builder build -> wk_from_json -> pb_entry (the save writer) -> a save
 *                                     file holding all those units -> mu_save_units / mu_save_build_ex -> same unit?
 *   save_test ref SAVE REF.jsonl      the wiki builder's own save import (builder_import.js) of SAVE == this reader
 *   save_test worlds TMPDIR SAVE      My Units "World units" (mu_worlds.c): a world's NEWEST save is the one listed (a
 *                                     snapshot of an older save would be wrong), our own worlds / temp saves / test worlds
 *                                     never listed, game + modded folders apart; a snapshot of units of a real save (a copy)
 *                                     read back from the library file = the save's units, with their source; the save
 *                                     file's bytes unchanged
 * Exit 1 on any FAIL. */
#include "mu_save.h"
#include "mu_worlds.h"
#include "myunits.h"
#include "mu_card.h"
#include "pvp_build.h"
#include "pvp_wiki_tables.h"
#include "fe/fe_json.h"
#include "fe/fe_save.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <utime.h>
#include <sys/stat.h>

static int pass, failc;
#define CHECK(c, ...) do { if (c) pass++; else { failc++; printf("FAIL %s:%d ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)
static double now(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec + t.tv_nsec / 1e9; }

static int list_save(const char *path) {
    static mu_save_unit U[256]; char e[300];
    double t0 = now();
    int n = mu_save_units(path, U, 256, e, sizeof e);
    double t1 = now();
    const char *bn = strrchr(path, '/'); bn = bn ? bn + 1 : path;
    printf("== %s: %d units (parse + list %.0f ms)%s%s\n", bn, n, (t1 - t0) * 1000, n < 0 ? " ERROR " : "", n < 0 ? e : "");
    CHECK(n >= 0, "%s: %s", bn, e);
    int tot = 0;
    for (int i = 0; i < n; i++) {
        wk_build b; mu_save_extra x; mu_card c;
        int r = mu_save_build_ex(path, U[i].guid, &b, &x, e, sizeof e);
        CHECK(!r, "%s %s: %s", bn, U[i].guid, e); if (r) continue;
        CHECK(!mu_card_of(&b, &c), "card %s", U[i].guid);
        char *js = mu_card_json(&c); CHECK(js && js[0] == '{', "card json"); free(js);
        CHECK(!strcmp(U[i].race, WK_RACE[b.race]) && !strcmp(U[i].name, b.name), "list row = build (%s)", U[i].guid);
        printf("  %-36.36s lv %2d %-20s %-24s items %d skills %2d bar %d (card bar %d) insc %d souls %d unknown %d%s%s\n",
               U[i].name, U[i].level, U[i].race + 9, U[i].portrait[0] ? U[i].portrait : "-", c.neq, b.nlearned, x.nbar, c.nbar,
               c.ninsc, c.nsouls, x.unknown, x.unknown ? ": " : "", x.unknown_keys);
        tot++;
    }
    double t2 = now();
    printf("   %d units built + carded in %.0f ms (cached save)\n", tot, (t2 - t1) * 1000);
    CHECK(mu_save_build(path, "00000000-0000-0000-0000-00000000dead", &(wk_build){ 0 }, e, sizeof e) == -1, "unknown guid refused");
    return n;
}

/* ---- round trip */
static int same_set(const int *a, int na, const int *b, int nb) {
    if (na != nb) return 0;
    for (int i = 0; i < na; i++) { int f = 0; for (int j = 0; j < nb; j++) if (a[i] == b[j]) f = 1; if (!f) return 0; }
    return 1;
}
static void key_set(const char **out, int *n, const char *k) { for (int i = 0; i < *n; i++) if (!strcmp(out[i], k)) return; out[(*n)++] = k; }
static int same_keys(const char **a, int na, const char **b, int nb) {
    if (na != nb) return 0;
    for (int i = 0; i < na; i++) { int f = 0; for (int j = 0; j < nb; j++) if (!strcmp(a[i], b[j])) f = 1; if (!f) return 0; }
    return 1;
}
static int tree_insc_key(const char *k) {
    for (int t = 0; t < WK_TREE_N; t++) {
        char a[96], b[96]; snprintf(a, sizeof a, "AFFECTER_%sInscription", WK_TREE[t]);
        size_t L = strlen(WK_TREE[t]); snprintf(b, sizeof b, "AFFECTER_%.*sInscription", (int)(L > 5 && !strcmp(WK_TREE[t] + L - 5, "Magic") ? L - 5 : L), WK_TREE[t]);
        if (!strcmp(a, k) || !strcmp(b, k)) return 1;
    }
    return 0;
}
static int pcmp(const void *x, const void *y) { return strcmp(*(const char *const *)x, *(const char *const *)y); }
static int passcmp(const void *x, const void *y) { return strcmp(*(const char *const *)x, *(const char *const *)y); }   /* first field = skill */
static void card_sort(mu_card *c) {
    qsort(c->bar, (size_t)c->nbar, sizeof c->bar[0], pcmp); qsort(c->acq, (size_t)c->nacq, sizeof c->acq[0], pcmp);
    qsort(c->insc, (size_t)c->ninsc, sizeof c->insc[0], pcmp); qsort(c->souls, (size_t)c->nsouls, sizeof c->souls[0], pcmp);
    qsort(c->pass, (size_t)c->npass, sizeof c->pass[0], passcmp);
}
static int round_trip(const char *builds_path, const char *tmp) {
    char e[400];
    FILE *f = fopen(builds_path, "r"); CHECK(f, "open %s", builds_path); if (!f) return 0;
    static wk_build A[400]; static char ug[400][40]; static jv *bars[400]; int na = 0, nskip = 0, nrace = 0;
    jv *save = jobj(), *hdr = jobj(), *ecd = jobj(), *ucs = jobj(), *base = jobj(), *pl = jarr(), *ecs = jobj(), *ec = jobj(), *pu = jobj();
    jset(hdr, "Version", jstrv("DS_B.0.4.29")); jset(save, "CampaignSaveHeader", hdr);
    jset(base, "PlayerGuidList", pl); jset(ucs, "Base", base); jset(ecd, "UnitContainerSaveData", ucs); jset(save, "EntityContainerSaveData", ecd);
    jset(ecs, "EntityComponents", ec); jset(save, "EntityComponentSaveData", ecs); jset(save, "PlayerUnitsSaveData", pu);
    char line[1 << 16];
    while (fgets(line, sizeof line, f) && na < 400) {
        if (line[0] != '{') continue;
        jv *j = jparse(line, strlen(line), e, sizeof e); if (!j) { nskip++; continue; }
        if (wk_from_json(j, &A[na], e, sizeof e)) { jfree(j); nskip++; nrace += !strncmp(e, "race ", 5); if (getenv("SHOW_SKIPS")) printf("SKIP %s\n", e); continue; }
        jfree(j);
        char notes[2000];
        jv *ent = pb_entry(&A[na], "test", notes, sizeof notes, e, sizeof e);
        if (!ent) { nskip++; if (getenv("SHOW_SKIPS")) printf("SKIP %s\n", e); continue; }                                    /* a build the save writer refuses (race with no look, ...) */
        jv *cl = jget(ent, "closure"), *u = jget(cl, "unit"), *items = jget(cl, "items");
        snprintf(ug[na], sizeof ug[na], "%.*s", (int)jget(u, "guid")->n, jget(u, "guid")->s);
        jpush(pl, jstrv(ug[na]));
        jset(ec, ug[na], jcopy(jget(u, "components")));
        for (size_t i = 0; items && i < items->cnt; i++) jset(ec, items->k[i], jcopy(jget(items->v[i], "components")));
        jv *pusd = jget(u, "pusd");
        for (size_t i = 0; pusd && i < pusd->cnt; i++) {
            if (!jget(pu, pusd->k[i])) jset(pu, pusd->k[i], jobj());
            jset(jget(pu, pusd->k[i]), ug[na], jcopy(pusd->v[i]));
        }
        bars[na] = jcopy(jpath(pusd, "QuickSlots.QuickSlots"));
        jfree(ent); na++;
    }
    fclose(f);
    char sp[1200]; snprintf(sp, sizeof sp, "%s/rt_save.json", tmp);
    { size_t L; char *txt = jdump(save, 0, &L); FILE *o = fopen(sp, "wb"); fwrite(txt, 1, L, o); fclose(o); free(txt); }
    jfree(save);
    mu_save_cache_clear();
    static mu_save_unit U[400];
    int n = mu_save_units(sp, U, 400, e, sizeof e);
    CHECK(n == na, "round trip: %d units listed of %d written (%s)", n, na, e);
    CHECK(na > 0 && nskip == nrace, "round trip: %d builds written, %d refused, only %d of them the generator's bogus races", na, nskip, nrace);
    int ok_all = 0, card_same = 0, maj_same = 0, tags_cmp = 0, tags_skip = 0;
    for (int i = 0; i < na; i++) {
        wk_build B; mu_save_extra x; const wk_build *a = &A[i];
        if (mu_save_build_ex(sp, ug[i], &B, &x, e, sizeof e)) { CHECK(0, "rt build %s: %s", ug[i], e); continue; }
        int bad = 0;
#define RT(c, ...) do { if (!(c)) { bad++; CHECK(0, __VA_ARGS__); } else pass++; } while (0)
        RT(B.race == a->race, "unit %d race %s != %s", i, WK_RACE[B.race], WK_RACE[a->race]);
        RT(B.bg == a->bg, "unit %d bg", i);
        RT(B.lvl == a->lvl, "unit %d lvl %d != %d", i, B.lvl, a->lvl);
        RT(!memcmp(B.tal, a->tal, sizeof a->tal), "unit %d talents", i);
        RT(!memcmp(B.eq, a->eq, sizeof B.eq), "unit %d worn items", i);
        RT(same_set(B.learned, B.nlearned, a->learned, a->nlearned), "unit %d learned %d vs %d", i, B.nlearned, a->nlearned);
        RT(same_set(B.trees, B.ntrees, a->trees, a->ntrees), "unit %d inscription trees", i);
        RT(same_set(B.souls, B.nsouls, a->souls, a->nsouls), "unit %d souls", i);
        RT(B.hide_head == a->hide_head, "unit %d hide head", i);
        {   /* traits: the unit's traits + acquired ones, by key (an acquired key that is also a trait reads back as a trait) */
            const char *ka[160], *kb[160]; int nka = 0, nkb = 0;
            const int *ai = a->has_saved_inds ? a->saved_inds : a->inds; int nai = a->has_saved_inds ? a->nsaved_inds : a->ninds;
            for (int k = 0; k < nai; k++) key_set(ka, &nka, WK_IND[ai[k]]);
            for (int k = 0; k < a->nacq; k++) key_set(ka, &nka, WK_ACQ[a->acq[k]]);
            for (int k = 0; k < B.nsaved_inds; k++) key_set(kb, &nkb, WK_IND[B.saved_inds[k]]);
            for (int k = 0; k < B.nacq; k++) key_set(kb, &nkb, WK_ACQ[B.acq[k]]);
            RT(same_keys(ka, nka, kb, nkb), "unit %d traits %d vs %d", i, nka, nkb);
        }
        {   /* held inscriptions (not tree ones) */
            const char *ka[160], *kb[160]; int nka = 0, nkb = 0;
            for (int k = 0; k < a->ninsc; k++) if (!tree_insc_key(WK_INSC[a->insc[k]])) key_set(ka, &nka, WK_INSC[a->insc[k]]);
            for (int k = 0; k < a->nsaved_insc; k++) if (!tree_insc_key(WK_INSC[a->saved_insc[k]])) key_set(ka, &nka, WK_INSC[a->saved_insc[k]]);
            for (int k = 0; k < B.ninsc; k++) key_set(kb, &nkb, WK_INSC[B.insc[k]]);
            RT(same_keys(ka, nka, kb, nkb), "unit %d inscriptions %d vs %d", i, nka, nkb);
        }
        {   /* portrait: the sprite pb_entry wrote */
            int pa = a->portrait >= 0 && a->portrait < WK_PORTRAITS_N[a->race] ? a->portrait : 0;
            RT(B.portrait == pa, "unit %d portrait %d vs %d", i, B.portrait, pa);
        }
        if (a->name[0]) {   /* wk_from_json turns '"' into ' ' and then trims: a name read twice loses those edge spaces */
            const char *an = a->name; size_t al = strlen(an); while (*an == ' ') { an++; al--; } while (al && an[al - 1] == ' ') al--;
            RT(strlen(B.name) == al && !strncmp(B.name, an, al), "unit %d name '%s' vs '%s'", i, B.name, a->name);
        }
        else RT(!strncmp(B.name, "TEXTKEY_PLAYERNAME_", 19), "unit %d default name key '%s'", i, B.name);
        RT(!strcmp(U[i].guid, ug[i]), "unit %d list order", i);
        /* tags: comparable when the slot's bonuses have distinct stats (a save item keeps ONE value per stat) */
        for (int s = 0; s < WK_SLOT_N; s++) {
            if (a->eq[s] < 0 || !a->ntag[s]) continue;
            int distinct = 1;
            for (int p = 0; p < a->ntag[s]; p++) for (int q = p + 1; q < a->ntag[s]; q++)
                if (a->tag[s][p].idx == a->tag[s][q].idx) distinct = 0;
            if (!distinct) { tags_skip++; continue; }
            int okt = B.ntag[s] == a->ntag[s];
            for (int p = 0; okt && p < a->ntag[s]; p++) { int f = 0; for (int q = 0; q < B.ntag[s]; q++) if (B.tag[s][q].idx == a->tag[s][p].idx && fabsf(B.tag[s][q].v - a->tag[s][p].v) < 1e-3f) f = 1; okt = f; }
            RT(okt, "unit %d slot %s tags %d vs %d", i, WK_SLOT[s], B.ntag[s], a->ntag[s]); tags_cmp++;
        }
        {   /* the bar written == the bar read */
            int nb = 0; jv *bw = bars[i];
            for (size_t k = 0; bw && k < bw->cnt; k++) if (bw->v[k]->t == JN_STR) { if (nb < x.nbar && jstreq(bw->v[k], x.bar[nb])) nb++; else nb = -100; }
            RT(nb == x.nbar && x.unknown == 0, "unit %d bar %d read, unknown %d (%s)", i, x.nbar, x.unknown, x.unknown_keys);
        }
        {   /* informational: same card, same stored majors */
            wk_build a2 = *a; a2.portrait = a->portrait >= 0 && a->portrait < WK_PORTRAITS_N[a->race] ? a->portrait : WK_PORTRAITS_N[a->race] ? 0 : -1;   /* the face pb_entry wrote */
            mu_card ca, cb; mu_card_of(&a2, &ca); mu_card_of(&B, &cb);
            ca.lvl = cb.lvl; snprintf(ca.name, sizeof ca.name, "%s", cb.name);
            card_sort(&ca); card_sort(&cb);       /* the save keeps skills per tree and holders in its own order */
            char *ja = mu_card_json(&ca), *jb = mu_card_json(&cb); int cs = !strcmp(ja, jb); card_same += cs; CHECK(cs, "unit %d: the card differs (SHOW_CARD_DIFF=1 shows it)", i);
            static int shown; if (!cs && getenv("SHOW_CARD_DIFF") && shown++ < 4) printf("CARD %d\n link: %s\n save: %s\n", i, ja, jb);
            free(ja); free(jb);
            double ma[6], mbv[6]; pb_stored_majors(a, ma); pb_stored_majors(&B, mbv); int m6 = 1;
            for (int m = 0; m < 6; m++) if (fabs(ma[m] - mbv[m]) > 0.0051) m6 = 0;   /* wk_from_json keeps majorBase to 1/100 (the link's step) */
            maj_same += m6; CHECK(m6, "unit %d: stored majors differ", i);
            static int mshown; if (!m6 && getenv("SHOW_CARD_DIFF") && mshown++ < 3) printf("MAJ %d link %g %g %g %g %g %g save %g %g %g %g %g %g (lvl %d, nman %d)\n", i,
                ma[0], ma[1], ma[2], ma[3], ma[4], ma[5], mbv[0], mbv[1], mbv[2], mbv[3], mbv[4], mbv[5], a->lvl, a->nman);
        }
        ok_all += !bad;
        jfree(bars[i]);
    }
    printf("== round trip %s: %d units written (%d builds skipped: refused by wk_from_json / pb_entry), %d identical, "
           "tags compared %d (skipped %d: same bonus twice), same card %d, same stored majors %d\n",
           builds_path, na, nskip, ok_all, tags_cmp, tags_skip, card_same, maj_same);
    return na;
}

/* ---- the builder's own import of the same save (builder_import.js) vs this reader */
static void ref_save(const char *save, const char *ref) {
    char e[400], line[1 << 16]; FILE *f = fopen(ref, "r"); CHECK(f, "open %s", ref); if (!f) return;
    int n = 0, same = 0, port = 0, acq = 0;
    while (fgets(line, sizeof line, f)) {
        jv *j = jparse(line, strlen(line), e, sizeof e); if (!j) continue;
        char g[64]; snprintf(g, sizeof g, "%.*s", (int)jget(j, "guid")->n, jget(j, "guid")->s);
        wk_build R, M; int rr = wk_from_json(jget(j, "build"), &R, e, sizeof e); jfree(j);
        CHECK(!rr, "builder import of %s: %s", g, e); if (rr) continue;
        int mr = mu_save_build(save, g, &M, e, sizeof e); CHECK(!mr, "reader %s: %s", g, e); if (mr) continue;
        n++;
        if (M.portrait != R.portrait) { port++; printf("  portrait %s: builder %s, save UnitPortraitID %s\n", g,
            R.portrait >= 0 ? WK_PORTRAITS[R.race][R.portrait] : "-", M.portrait >= 0 ? WK_PORTRAITS[M.race][M.portrait] : "-"); M.portrait = R.portrait; }
        if (M.nacq != R.nacq) { acq++; printf("  acquired traits %s: +%d the builder import drops\n", g, M.nacq - R.nacq); M.nacq = R.nacq; }
        char *a = wk_json(&R, 1), *b = wk_json(&M, 1); int ok = !strcmp(a, b); same += ok;
        CHECK(ok, "%s: reader != builder import\n builder: %s\n reader:  %s", g, a, b); free(a); free(b);
    }
    fclose(f);
    CHECK(n > 0, "builder import vs reader, %s: 0 units compared", save);
    const char *bn = strrchr(save, '/');
    printf("== builder import vs reader, %s: %d units, %d identical (after %d portrait + %d acquired-trait differences, by design)\n", bn ? bn + 1 : save, n, same, port, acq);
}

/* ---- World units + snapshot */
static void put_save(const char *dir, const char *name, const char *guid, const char *clan, const char *when, long long mt) {
    char p[1200]; snprintf(p, sizeof p, "%s/%s.json", dir, name);
    FILE *f = fopen(p, "wb"); if (!f) { CHECK(0, "write %s", p); return; }
    /* the game's layout: entity data first, the header in the middle */
    fprintf(f, "{\n  \"EntityContainerSaveData\": {\"UnitContainerSaveData\": {}},\n  \"Note\": \"a \\\"CampaignSaveHeader\\\" text in a string\",\n");
    if (guid) fprintf(f, "  \"CampaignSaveHeader\": {\"CampaignGuid\": \"%s\", \"ClanName\": \"%s\", \"SaveName\": \"%s\", \"SaveTime\": \"%s\", \"Nested\": {\"a\": \"}\"}},\n", guid, clan, name, when);
    fprintf(f, "  \"EntityComponentSaveData\": {\"EntityComponents\": {}}\n}\n");
    fclose(f);
    struct utimbuf u = { (time_t)mt, (time_t)mt }; utime(p, &u);
}
static int skip_test(const char *guid, const char *save) { (void)save; return !strcmp(guid, "55555555555555555555555555555555"); }
static unsigned char *bytes(const char *p, size_t *n) {
    FILE *f = fopen(p, "rb"); if (!f) return NULL; fseek(f, 0, SEEK_END); long L = ftell(f); fseek(f, 0, SEEK_SET);
    unsigned char *b = malloc((size_t)L + 1); *n = fread(b, 1, (size_t)L, f); fclose(f); return b;
}
static void worlds(const char *tmp, const char *real) {
    char g[1100], m[1100]; snprintf(g, sizeof g, "%s/w/game", tmp); snprintf(m, sizeof m, "%s/w/mod", tmp);
    char w0[1100]; snprintf(w0, sizeof w0, "%s/w", tmp); mkdir(tmp, 0755); mkdir(w0, 0755); mkdir(g, 0755); mkdir(m, 0755);
    long long T = (long long)time(NULL) - 1000;
    const char *G1 = "11111111222233334444555555555555", *G1D = "11111111-2222-3333-4444-555555555555";
    put_save(g, "ClanA__AutoSave_11111111222233334444555555555555_0", G1D, "Clan A", "2026-10-20 10:00:00", T - 100);
    put_save(g, "ClanA__AutoSave_11111111222233334444555555555555_1", G1D, "Clan A", "2026-10-21 11:00:00", T);
    put_save(g, "My own save", "22222222-2222-2222-2222-222222222222", "Clan B", "2026-10-19 09:30:00", T - 50);
    put_save(g, "mp_sync", "33333333333333333333333333333333", "Temp", "2026-10-22 00:00:00", T + 10);
    put_save(g, "ForeignExpeditionHub__AutoSave_fe5ea00094e9478e909cfb74cbcad20e_0", "fe5ea000-94e9-478e-909c-fb74cbcad20e", "Hub", "2026-10-22 00:00:00", T + 10);
    put_save(g, "Hub copy", "fe5eb000-94e9-478e-909c-fb74cbcad20e", "Hub", "2026-10-22 00:00:00", T + 10);  /* our world, no guid in the name */
    put_save(g, "SaveHeaderCache", "44444444444444444444444444444444", "Cache", "2026-10-22 00:00:00", T + 10);
    put_save(g, "Broken__AutoSave_66666666666666666666666666666666_0", NULL, NULL, NULL, T + 20);       /* no header */
    put_save(m, "PvPBase__AutoSave_b45e0553a2054c6a9fe3bb03fc5d2094_0", "b45e0553-a205-4c6a-9fe3-bb03fc5d2094", "PvP", "2026-10-22 00:00:00", T + 10);
    put_save(m, "ClanA__AutoSave_11111111222233334444555555555555_0", G1D, "Clan A", "2026-10-18 08:00:00", T - 200);
    put_save(m, "TeamX_DPS Check_Normal__AutoSave_55555555555555555555555555555555_0", "55555555-5555-5555-5555-555555555555", "Test", "2026-10-22 00:00:00", T + 30);
    const char *dirs[2] = { g, m };
    mu_world w[16]; int n = mu_worlds_list(dirs, 2, w, 16, skip_test);
    for (int i = 0; i < n; i++) printf("  world %d: group %d %s | %s | %s | %s\n", i, w[i].group, w[i].name, w[i].save, w[i].guid, w[i].when);
    CHECK(n == 3, "worlds: 3 listed (got %d): our hub / PvP worlds, temp saves, the header cache, a test world and a file with no header left out", n);
    if (n == 3) {
        CHECK(w[0].group == 0 && !strcmp(w[0].name, "Clan A") && strstr(w[0].save, "_1") && !strcmp(w[0].when, "2026-10-21 11:00") && !strcmp(w[0].guid, G1),
              "worlds: the game world's NEWEST save is listed (%s, %s)", w[0].save, w[0].when);
        CHECK(w[1].group == 0 && !strcmp(w[1].name, "Clan B") && !strcmp(w[1].save, "My own save"), "worlds: a save with any name is found by its header (%s)", w[1].name);
        CHECK(w[2].group == 1 && !strcmp(w[2].name, "Clan A") && !strcmp(w[2].when, "2026-10-18 08:00"), "worlds: the same world in the modded folder is its own row (%d %s)", w[2].group, w[2].when);
    }
    /* snapshot of a real save (a copy) */
    char sd[1100], sp[1300], lp[1300], e[400]; snprintf(sd, sizeof sd, "%s/w/snap", tmp); mkdir(sd, 0755);
    const char *bn = strrchr(real, '/'); bn = bn ? bn + 1 : real;
    snprintf(sp, sizeof sp, "%s/%s", sd, bn);
    size_t n0 = 0, n1 = 0; unsigned char *b0 = bytes(real, &n0);
    CHECK(b0 != NULL, "snapshot: read %s", real); if (!b0) return;
    { FILE *f = fopen(sp, "wb"); fwrite(b0, 1, n0, f); fclose(f); }
    const char *sdirs[1] = { sd };
    mu_world sw; int ns = mu_worlds_list(sdirs, 1, &sw, 1, NULL);
    CHECK(ns == 1, "snapshot: the real save is one world (%d)", ns); if (ns != 1) { free(b0); return; }
    printf("  real world: %s | %s | %s\n", sw.name, sw.when, sw.guid);
    static mu_save_unit U[256]; int nu = mu_save_units(sp, U, 256, e, sizeof e);
    CHECK(nu >= 2, "snapshot: the save has 2+ units (%d) %s", nu, e); if (nu < 2) { free(b0); return; }
    snprintf(lp, sizeof lp, "%s/w/library.json", tmp); setenv("DSMP_UNITS_LIB", lp, 1);
    jv *lib = mu_load(e, sizeof e); if (!lib) { CHECK(0, "snapshot: library: %s", e); free(b0); return; }
    int pick[2] = { 0, nu - 1 }; char ids[2][17];
    for (int k = 0; k < 2; k++) CHECK(!mu_snapshot(lib, &sw, U[pick[k]].guid, k ? NULL : "Snap One", ids[k], e, sizeof e), "snapshot of %s: %s", U[pick[k]].guid, e);
    CHECK(!mu_save(lib, e, sizeof e), "snapshot: library saved: %s", e);
    jfree(lib); lib = mu_load(e, sizeof e); if (!lib) { CHECK(0, "snapshot: reload: %s", e); free(b0); return; }
    CHECK(mu_count(lib) == 2, "snapshot: 2 units in the library file (%zu)", mu_count(lib));
    for (int k = 0; k < 2; k++) {
        wk_build a, b; mu_source s;
        int ra = mu_save_build(sp, U[pick[k]].guid, &a, e, sizeof e), rb = mu_build_of(lib, ids[k], &b, e, sizeof e);
        CHECK(!ra && !rb, "snapshot %d: builds (%s)", k, e); if (ra || rb) continue;
        if (k == 0) CHECK(!strcmp(b.name, "Snap One"), "snapshot 0: the given name (%s)", b.name);
        a.name[0] = b.name[0] = 0;
        CHECK(wk_hash(&a, 1) == wk_hash(&b, 1), "snapshot %d: the library unit = the save's unit (race %d/%d lvl %d/%d learned %d/%d)", k, a.race, b.race, a.lvl, b.lvl, a.nlearned, b.nlearned);
        CHECK(mu_source_of(lib, ids[k], &s) && !strcmp(s.world, sw.name) && !strcmp(s.campaign, sw.guid) && !strcmp(s.save, sw.save)
              && !strcmp(s.save_time, sw.when) && !strcmp(s.unit, U[pick[k]].guid) && s.taken > 0, "snapshot %d: source (%s / %s / %s / %s)", k, s.world, s.save, s.save_time, s.unit);
        char line[400]; mu_source_line(&s, line, sizeof line);
        CHECK(strstr(line, sw.name) && strstr(line, sw.when), "snapshot %d: card line \"%s\"", k, line);
    }
    jfree(lib);
    unsigned char *b1 = bytes(sp, &n1);
    CHECK(b1 && n1 == n0 && !memcmp(b0, b1, n0), "snapshot: the save file is unchanged");
    free(b0); free(b1);
}

int main(int argc, char **argv) {
    if (argc >= 3 && !strcmp(argv[1], "list")) for (int i = 2; i < argc; i++) list_save(argv[i]);
    else if (argc >= 4 && !strcmp(argv[1], "rt")) round_trip(argv[2], argv[3]);
    else if (argc >= 4 && !strcmp(argv[1], "ref")) ref_save(argv[2], argv[3]);
    else if (argc >= 4 && !strcmp(argv[1], "worlds")) worlds(argv[2], argv[3]);
    else { fprintf(stderr, "usage: save_test list SAVE... | save_test rt BUILDS.jsonl TMPDIR\n"); return 2; }
    mu_save_cache_clear();
    printf("%d passed, %d failed\n", pass, failc);
    return failc ? 1 : 0;
}
