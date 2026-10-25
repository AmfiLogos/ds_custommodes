/* mu_worlds - the player's worlds for My Units' "World units" and the snapshot (see mu_worlds.h). Pure C. */
#include "mu_worlds.h"
#include "mu_save.h"
#include "pvp_wiki.h"
#include "fe/fe_json.h"
#include "i18n/ds_i18n.h"
#include <ctype.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <windows.h>
#define W_MTIME(st) ((long long)(st).st_mtime)
#else
#define W_MTIME(st) ((long long)(st).st_mtimespec.tv_sec)
#endif

void mu_game_saves_dir(char *out, size_t n) {
    if (!n) return;
    out[0] = 0;
#ifdef _WIN32
    wchar_t w[1024]; DWORD k = GetEnvironmentVariableW(L"DSML_GAME_SAVES", w, 1024);   /* UTF-8, as myunits.c DSML_DATA */
    if (k && k < 1024 && !WideCharToMultiByte(CP_UTF8, 0, w, -1, out, (int)n, NULL, NULL)) out[0] = 0;
#else
    const char *e = getenv("DSML_GAME_SAVES"); if (e && *e) snprintf(out, n, "%s", e);
#endif
}
static int hex32_at(const char *p) {                                    /* 32 hex digits, then no 33rd */
    for (int k = 0; k < 32; k++) if (!isxdigit((unsigned char)p[k])) return 0;
    return !isxdigit((unsigned char)p[32]);
}
static int our_guid(const char *g) {                                     /* hub (running / template) and PvP worlds */
    static const char *const pre[] = { "fe5ea00", "fe5eb", "b45e0" };
    for (size_t i = 0; i < sizeof pre / sizeof *pre; i++) if (!strncmp(g, pre[i], strlen(pre[i]))) return 1;
    return 0;
}
int mu_world_save_name(const char *f) {
    static const char *const tmp[] = { "mp_", "MPFE_", "MPHub_", "MPPvP_" };
    for (size_t i = 0; i < sizeof tmp / sizeof *tmp; i++) if (!strncmp(f, tmp[i], strlen(tmp[i]))) return 1;
    for (const char *p = strchr(f, '_'); p; p = strchr(p + 1, '_')) {   /* every "_<32 hex>" token */
        if (!hex32_at(p + 1)) continue;
        char g[33]; for (int k = 0; k < 32; k++) g[k] = (char)tolower((unsigned char)p[1 + k]); g[32] = 0;
        if (our_guid(g)) return 1;
    }
    return 0;
}
static void norm_guid(const char *s, size_t n, char out[40]) {
    int k = 0;
    for (size_t i = 0; i < n && k < 32; i++) if (isxdigit((unsigned char)s[i])) out[k++] = (char)tolower((unsigned char)s[i]);
    out[k] = 0;
}
/* "<clan>__AutoSave_<32 hex>_<n>": the world's guid from the name (no file read) */
static int name_guid(const char *f, char out[40]) {
    const char *p = strstr(f, "__AutoSave_");
    if (!p || !hex32_at(p + 11)) { out[0] = 0; return 0; }
    norm_guid(p + 11, 32, out); return 1;
}
static void raw_txt(char *o, size_t n, const jv *v) {
    if (v && v->t == JN_STR) snprintf(o, n, "%.*s", (int)v->n, v->s); else if (n) o[0] = 0;
    for (char *p = o; *p; p++) if (*p == '\\' || *p == '"') *p = ' ';  /* raw JSON text: no escapes on screen */
}

/* the CampaignSaveHeader object of a save: found as text and parsed alone (a save is 1-5 MB; the header sits in the
   middle, after the entity data), so a list of worlds never parses whole saves */
int mu_world_header(const char *path, mu_world *w) {
    FILE *f = fopen(path, "rb"); if (!f) return -1;
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    if (n <= 0 || n > 512L * 1024 * 1024) { fclose(f); return -1; }
    char *b = malloc((size_t)n + 1); if (!b) { fclose(f); return -1; }
    size_t got = fread(b, 1, (size_t)n, f); fclose(f); b[got] = 0;
    static const char key[] = "\"CampaignSaveHeader\"";
    char *h = NULL;
    for (char *p = b; (p = strstr(p, key)); p += sizeof key - 1) {     /* the key, a colon, an object */
        char *q = p + sizeof key - 1; while (*q == ' ' || *q == '\t' || *q == '\r' || *q == '\n') q++;
        if (*q != ':') continue;
        q++; while (*q == ' ' || *q == '\t' || *q == '\r' || *q == '\n') q++;
        if (*q == '{') { h = q; break; }
    }
    int rc = -1;
    if (h) {
        int depth = 0, str = 0; char *e = h;
        for (; *e; e++) {
            if (str) { if (*e == '\\' && e[1]) e++; else if (*e == '"') str = 0; continue; }
            if (*e == '"') str = 1;
            else if (*e == '{') depth++;
            else if (*e == '}' && --depth == 0) break;
        }
        char pe[200];
        jv *o = *e ? jparse(h, (size_t)(e - h + 1), pe, sizeof pe) : NULL;
        jv *g = o ? jget(o, "CampaignGuid") : NULL;
        if (g && g->t == JN_STR) {
            norm_guid(g->s, g->n, w->guid);
            raw_txt(w->name, sizeof w->name, jget(o, "ClanName"));
            char st[64]; raw_txt(st, sizeof st, jget(o, "SaveTime"));    /* "2026-10-24 23:52:45" (the game's local time) */
            if (strlen(st) >= 16 && st[4] == '-' && st[10] == ' ') snprintf(w->when, sizeof w->when, "%.16s", st);
            else w->when[0] = 0;
            rc = strlen(w->guid) == 32 ? 0 : -1;
        }
        jfree(o);
    }
    free(b);
    return rc;
}

typedef struct { char path[1024], file[256], guid[40]; long long mt; int group; } fent;
static int by_time(const void *a, const void *b) {
    long long x = ((const fent *)a)->mt, y = ((const fent *)b)->mt;
    return x < y ? 1 : x > y ? -1 : 0;
}
int mu_worlds_list(const char *const *dirs, int ndirs, mu_world *out, int max, mu_world_skip_fn skip) {
    int cap = 2048, nf = 0, n = 0;
    fent *fs = malloc(sizeof *fs * (size_t)cap); if (!fs) return 0;
    for (int d = 0; d < ndirs; d++) {
        if (!dirs[d] || !dirs[d][0]) continue;
        int dup = 0; for (int k = 0; k < d; k++) if (dirs[k] && !strcmp(dirs[k], dirs[d])) dup = 1;
        if (dup) continue;                                               /* one folder listed once */
        DIR *dp = opendir(dirs[d]); if (!dp) continue;
        for (struct dirent *e; (e = readdir(dp)) && nf < cap; ) {
            size_t l = strlen(e->d_name);
            if (l < 6 || l >= sizeof fs[0].file || strcmp(e->d_name + l - 5, ".json") || strstr(e->d_name, "SaveHeaderCache") || mu_world_save_name(e->d_name)) continue;
            fent *x = &fs[nf];
            if ((size_t)snprintf(x->path, sizeof x->path, "%s/%s", dirs[d], e->d_name) >= sizeof x->path) continue;
            struct stat st; if (stat(x->path, &st) || !S_ISREG(st.st_mode)) continue;
            snprintf(x->file, sizeof x->file, "%.*s", (int)(l - 5), e->d_name);
            name_guid(x->file, x->guid); x->mt = W_MTIME(st); x->group = d; nf++;
        }
        closedir(dp);
    }
    qsort(fs, (size_t)nf, sizeof *fs, by_time);                          /* newest first: the first file of a world is its newest */
    for (int i = 0; i < nf && n < max; i++) {
        fent *x = &fs[i];
        int seen = 0;
        if (x->guid[0]) for (int k = 0; k < n && !seen; k++) seen = !strcmp(out[k].guid, x->guid);
        if (seen) continue;
        mu_world w; memset(&w, 0, sizeof w);
        if (mu_world_header(x->path, &w)) continue;                      /* not a readable save: an older file may stand in */
        if (x->guid[0] && strcmp(w.guid, x->guid)) snprintf(w.guid, sizeof w.guid, "%s", x->guid);
        for (int k = 0; k < n && !seen; k++) seen = !strcmp(out[k].guid, w.guid);
        if (seen || our_guid(w.guid) || (skip && skip(w.guid, x->file))) continue;
        snprintf(w.path, sizeof w.path, "%s", x->path); snprintf(w.save, sizeof w.save, "%s", x->file);
        if (!w.name[0]) snprintf(w.name, sizeof w.name, "%s", x->file);
        if (!w.when[0]) { time_t t = (time_t)x->mt; struct tm *tm = localtime(&t); if (tm) strftime(w.when, sizeof w.when, "%Y-%m-%d %H:%M", tm); }
        w.group = x->group; w.mtime = x->mt;
        out[n++] = w;
    }
    free(fs);
    return n;
}

static long long now_ms(void) {
    struct timespec ts; timespec_get(&ts, TIME_UTC);
    return (long long)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}
jv *mu_save_build_obj(const char *path, const char *guid, char *err, size_t errn) {
    struct { wk_build b; mu_save_extra x; } *t = calloc(1, sizeof *t); char e[400];   /* large: on the heap */
    if (!t) { snprintf(err, errn, "internal: out of memory"); return NULL; }
    if (mu_save_build_ex(path, guid, &t->b, &t->x, e, sizeof e)) { snprintf(err, errn, "%s", e); free(t); return NULL; }
    mu_save_extra x = t->x; char *js = wk_json(&t->b, 1); char pe[200] = "";
    free(t);
    jv *arr = js ? jparse(js, strlen(js), pe, sizeof pe) : NULL; free(js);
    if (!arr || arr->t != JN_ARR || arr->cnt != 1) { snprintf(err, errn, "internal: unit JSON: %s", pe); jfree(arr); return NULL; }
    jv *o = arr->v[0]; arr->v[0] = NULL; arr->cnt = 0;                   /* taken out of the array */
    jfree(arr);
    /* the unit's own skill bar from the save (wk_build has no field for it) as the builder's own bar pick `quick`: the save
       generator writes it as the unit's bar, keeping only the skills the build has (builder.js quickFor) */
    if (x.nbar) { jv *q = jarr(); for (int i = 0; i < x.nbar && i < 8; i++) jpush(q, jstrv(x.bar[i])); jset(o, "quick", q); }
    return o;
}
int mu_snapshot(jv *lib, const mu_world *w, const char *guid, const char *name, char id_out[17], char *err, size_t errn) {
    jv *bj = mu_save_build_obj(w->path, guid, err, errn);
    if (!bj) return -1;
    char id[17];
    int r = mu_add_build(lib, bj, name, id, err, errn);
    jfree(bj);
    if (r) return -1;
    mu_source s; memset(&s, 0, sizeof s);
    snprintf(s.world, sizeof s.world, "%s", w->name); snprintf(s.campaign, sizeof s.campaign, "%s", w->guid);
    snprintf(s.save, sizeof s.save, "%s", w->save); snprintf(s.save_time, sizeof s.save_time, "%s", w->when);
    snprintf(s.unit, sizeof s.unit, "%s", guid); s.taken = now_ms();
    mu_set_source(lib, id, &s);
    if (id_out) memcpy(id_out, id, 17);
    return 0;
}
int mu_source_forms(const mu_source *s, char forms[4][200]) {
    char taken[24] = "", b[80];
    time_t t = (time_t)(s->taken / 1000); struct tm *tm = s->taken > 0 ? localtime(&t) : NULL;
    if (tm) strftime(taken, sizeof taken, "%Y-%m-%d %H:%M", tm);
    mu_source_line(s, forms[0], sizeof forms[0]);
    snprintf(b, sizeof b, TR("snapshot %s"), taken);
    snprintf(forms[1], sizeof forms[1], "%s  \xC2\xB7  %s", s->world, b);
    snprintf(forms[2], sizeof forms[2], "%s  \xC2\xB7  %s", s->world, taken);
    snprintf(forms[3], sizeof forms[3], "%s  \xC2\xB7  %s", s->world, taken[0] ? taken + 5 : "");
    for (int i = 1; i < 4; i++) dsi_utf8_fix_end(forms[i]);
    return 4;
}
void mu_source_line(const mu_source *s, char *out, size_t n) {
    char taken[24] = "", a[80], b[80];
    time_t t = (time_t)(s->taken / 1000); struct tm *tm = s->taken > 0 ? localtime(&t) : NULL;
    if (tm) strftime(taken, sizeof taken, "%Y-%m-%d %H:%M", tm);
    snprintf(a, sizeof a, TR("save %s"), s->save_time);
    snprintf(b, sizeof b, TR("snapshot %s"), taken);
    snprintf(out, n, "%s  \xC2\xB7  %s  \xC2\xB7  %s", s->world, a, b);
    dsi_utf8_fix_end(out);
}
