/* tf_wikisave.c - wiki-made saves (tf_worlds.h): the name test and the Lock Consecration fields; which saves are test
 * worlds (the mark, the list, the record). Pure C (fe_json + stdio only), so fights/test/wikisave_test.c checks it without
 * the game. */
#include "tf_worlds.h"
#include "../fe/fe_json.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#endif

static int has_tok(const char *s, size_t n, const char *tok) {
    size_t l = strlen(tok);
    for (size_t i = 0; i + l <= n; i++) if (!memcmp(s + i, tok, l)) return 1;
    return 0;
}
int tfw_wiki_made(const char *name) {
    if (!name) return 0;
    const char *a = strstr(name, "__AutoSave_");
    if (!a || a == name) return 0;
    const char *h = a + 11; int k = 0;
    while (k < 32 && isxdigit((unsigned char)h[k])) k++;
    if (k != 32 || h[32] != '_' || !isdigit((unsigned char)h[33])) return 0;
    for (const char *d = h + 33; *d; d++) if (!isdigit((unsigned char)*d)) return 0;
    size_t n = (size_t)(a - name);                                   /* the stem: what the generator put before it */
    if (!strncmp(name, "BossRush-", 9) || !strncmp(name, "Arena-", 6) || !strncmp(name, "DPS Check_", 10) || !strncmp(name, "FinishedBase_", 13)) return 1;
    return has_tok(name, n, "_BossRush-") || has_tok(name, n, "_Arena-") || has_tok(name, n, "_DPS Check_") ||
           has_tok(name, n, "_FinishedBase_") || has_tok(name, n, "DSBuilder+") || has_tok(name, n, "DSBuilder_");
}
/* a DPS Check save (tf_dps.h): wiki-made and its stem is "DPS Check_<difficulty>" or "<team>_DPS Check_<difficulty>" */
int tfw_dps_made(const char *name) {
    if (!tfw_wiki_made(name)) return 0;
    const char *a = strstr(name, "__AutoSave_");
    return !strncmp(name, "DPS Check_", 10) || has_tok(name, (size_t)(a - name), "_DPS Check_");
}
int tfw_consecration_fix(jv *save) {
    jv *css = jpath(save, "CampaignSettingSaveData.CampaignStartSettingSaveData");
    if (!css || css->t != JN_OBJ) return 0;
    int ch = 0;
    if (!jget(css, "LockConsecrationEnabled")) { jset(css, "LockConsecrationEnabled", jbool(0)); ch = 1; }   /* absent: the stock default */
    if (!jis(jget(css, "LockConsecrationNoticeAcknowledged"), JN_TRUE)) { jset(css, "LockConsecrationNoticeAcknowledged", jbool(1)); ch = 1; }
    return ch;
}

/* ---------------------------------------------------------------- test worlds known forever (tf_worlds.h) */
static void norm(const char *g, char out[33]) {                      /* 32 lowercase hex, no dashes; "" when fewer */
    int k = 0;
    for (; g && *g && k < 32; g++) if (isxdigit((unsigned char)*g)) out[k++] = (char)tolower((unsigned char)*g);
    out[k] = 0;
}
static int marked(const char *g) { return strlen(g) == 32 && !strncmp(g, TFW_GUID_MARK, 8) && g[12] == '0'; }
int tfw_guid_marked(const char *guid) { char g[33]; norm(guid, g); return marked(g); }
static int name_marked(const char *s) {                              /* a run of exactly 32 hex digits that is marked */
    for (const char *p = s; p && *p; p++) {
        if (!isxdigit((unsigned char)*p) || (p > s && isxdigit((unsigned char)p[-1]))) continue;
        size_t k = 0; while (k < 33 && isxdigit((unsigned char)p[k])) k++;
        if (k == 32) { char g[33]; norm(p, g); if (marked(g)) return 1; }
    }
    return 0;
}

#define NW 64
static char dir[1024], W[NW][33]; static int nw, loaded;            /* listed: newest last */
static char (*R)[33]; static int nr, rcap;                           /* recorded: every test world this install made */
void tfw_reg_dir(const char *d) {
    if (!d || !strcmp(d, dir)) return;
    snprintf(dir, sizeof dir, "%s", d); loaded = 0; nw = 0; nr = 0;
}
static int recorded(const char *g) { for (int i = 0; i < nr; i++) if (!strcmp(R[i], g)) return 1; return 0; }
static void rec_mem(const char *g) {
    if (nr == rcap) { int c = rcap ? rcap * 2 : 64; void *p = realloc(R, sizeof *R * (size_t)c); if (!p) return; R = p; rcap = c; }
    snprintf(R[nr++], 33, "%s", g);
}
static void record(const char *g) {                                  /* appended, never rewritten: nothing can drop it */
    if (recorded(g)) return;
    rec_mem(g);
    char p[1100]; snprintf(p, sizeof p, "%s/test_worlds_made.txt", dir);
    FILE *t = fopen(p, "rb"); int fresh = !t; if (t) fclose(t);
    FILE *f = fopen(p, "ab"); if (!f) return;
    if (fresh) fprintf(f, "# Units Laboratory: every test world (campaign guid) Test Fights made on this computer. Load lists and the\n"
                          "# Steam Cloud backup leave their saves out. Kept forever; one guid per line.\n");
    fprintf(f, "%s\n", g); fclose(f);
}
static void reg_save(void) {
    char p[1100], t[1120]; snprintf(p, sizeof p, "%s/test_worlds.json", dir); snprintf(t, sizeof t, "%s.tmp", p);
    FILE *f = fopen(t, "wb"); if (!f) return;
    fprintf(f, "{\"format\":\"custom-modes-test-worlds\",\"version\":1,\"worlds\":[");
    for (int i = 0; i < nw; i++) fprintf(f, "%s\"%s\"", i ? "," : "", W[i]);
    fprintf(f, "]}\n"); fclose(f);
#ifdef _WIN32
    MoveFileExA(t, p, MOVEFILE_REPLACE_EXISTING);
#else
    rename(t, p);
#endif
}
static void reg_load(void) {
    if (loaded || !dir[0]) return;
    loaded = 1; nw = 0; nr = 0;
    char p[1100]; snprintf(p, sizeof p, "%s/test_worlds_made.txt", dir);
    FILE *f = fopen(p, "rb");
    if (f) { char l[256], g[33]; while (fgets(l, sizeof l, f)) if (l[0] != '#') { norm(l, g); if (strlen(g) == 32) rec_mem(g); } fclose(f); }
    snprintf(p, sizeof p, "%s/test_worlds.json", dir);
    char e[200]; jv *r = jload(p, e, sizeof e); if (!r) return;
    jv *a = jget(r, "worlds");
    for (size_t i = 0; a && a->t == JN_ARR && i < a->cnt && nw < NW; i++) {
        char *s = jstrdup_raw(a->v[i]); norm(s, W[nw]); free(s);
        if (strlen(W[nw]) == 32) record(W[nw++]);                    /* listed by an older version (no record then) */
    }
    jfree(r);
}
const char *tfw_listed(int i) { reg_load(); return i >= 0 && i < nw ? W[i] : NULL; }
void tfw_title_clear(void) { reg_load(); nw = 0; if (dir[0]) reg_save(); }

int tfw_take(jv *save, char *name_out, size_t name_n) {
    jv *h = jget(save, "CampaignSaveHeader");
    char *name = h ? jstrdup_raw(jget(h, "SaveName")) : NULL, *guid = h ? jstrdup_raw(jget(h, "CampaignGuid")) : NULL, g[33];
    norm(guid, g); free(guid);
    if (!name || !name[0] || strlen(g) != 32 || strpbrk(name, "/\\:") || strstr(name, "..") || strlen(name) >= name_n) { free(name); return -1; }
    if (!marked(g)) {                                                /* the generator's random guid gets the mark */
        char old[33], d[37], *at = NULL; memcpy(old, g, 33);
        memcpy(g, TFW_GUID_MARK, 8); g[12] = '0';
        snprintf(d, sizeof d, "%.8s-%.4s-%.4s-%.4s-%.12s", g, g + 8, g + 12, g + 16, g + 20);
        jset(h, "CampaignGuid", jstrv(d));
        jv *c = jget(save, "ClanSaveData");
        if (c && c->t == JN_OBJ && jget(c, "CampaignGuid")) jset(c, "CampaignGuid", jstrv(d));
        for (char *p = name; *p && !at; p++) if (!strncasecmp(p, old, 32)) at = p;   /* "..__AutoSave_<guid hex>_0" */
        if (at) { memcpy(at, g, 32); jset(h, "SaveName", jstrv(name)); }
    }
    reg_load();
    int i = 0; while (i < nw && strcmp(W[i], g)) i++;
    if (i == nw) {
        if (nw == NW) { memmove(W[0], W[1], sizeof W[0] * (NW - 1)); nw--; }
        snprintf(W[nw++], 33, "%s", g);
    }
    if (dir[0]) { reg_save(); record(g); }
    snprintf(name_out, name_n, "%s", name); free(name);
    return 0;
}

int tfw_known(const char *guid, const char *save_name) {
    char g[33], n[512] = ""; norm(guid, g);
    if (marked(g) || name_marked(save_name)) return 1;
    for (size_t i = 0; save_name && save_name[i] && i + 1 < sizeof n; i++) { n[i] = (char)tolower((unsigned char)save_name[i]); n[i + 1] = 0; }
    reg_load();
    for (int i = 0; i < nw; i++) if ((g[0] && !strcmp(g, W[i])) || (n[0] && strstr(n, W[i]))) return 1;
    for (int i = 0; i < nr; i++) if ((g[0] && !strcmp(g, R[i])) || (n[0] && strstr(n, R[i]))) return 1;
    return 0;
}
