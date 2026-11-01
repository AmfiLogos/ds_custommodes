/* cm_prefs.c - see cm_prefs.h */
#include "cm_prefs.h"
#include "../fe/fe_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <windows.h>
#include <direct.h>
#define mkdir_(p) _mkdir(p)
#else
#define mkdir_(p) mkdir(p, 0755)
#endif

#ifndef CM_PREFS_NO_GAME
int cmw_game_data(char *out, size_t n);                  /* cm_win.c */
#endif

#define NP 64                                            /* Test Fights keeps every choice (owner 2026-11-01): ~20 keys */
#define VMAX 600                                         /* a World units pick list: 4 x (world guid / unit guid) */
static struct { char k[48], v[VMAX]; } pv[NP];
static int np, loaded, batch, dirty;
static char file[1100];

static int path(void) {
    if (file[0]) return 0;
#ifndef CM_PREFS_NO_GAME
    char d[1024]; if (cmw_game_data(d, sizeof d)) return -1;
    snprintf(file, sizeof file, "%s/Mods", d); mkdir_(file);
    snprintf(file, sizeof file, "%s/Mods/custom-modes", d); mkdir_(file);
    snprintf(file, sizeof file, "%s/Mods/custom-modes/settings.json", d);
    return 0;
#else
    return -1;
#endif
}
void cm_prefs_use_file(const char *p) { snprintf(file, sizeof file, "%s", p ? p : ""); loaded = 0; np = 0; }

static void load(void) {
    if (loaded || path()) return;
    loaded = 1; np = 0;
    char e[200]; jv *r = jload(file, e, sizeof e); if (!r) return;
    jv *o = jget(r, "values");
    for (size_t i = 0; o && o->t == JN_OBJ && i < o->cnt && np < NP; i++) {
        if (o->v[i]->t != JN_STR) continue;
        char *s = jstrdup_raw(o->v[i]);
        snprintf(pv[np].k, sizeof pv[np].k, "%.*s", (int)o->kn[i], o->k[i]);
        snprintf(pv[np].v, sizeof pv[np].v, "%s", s); free(s);
        np++;
    }
    jfree(r);
}
static void save(void) {
    if (batch) { dirty = 1; return; }
    dirty = 0;
    if (path()) return;
    char t[1120]; snprintf(t, sizeof t, "%s.tmp", file);
    FILE *f = fopen(t, "wb"); if (!f) return;
    fprintf(f, "{\"format\":\"custom-modes-settings\",\"version\":1,\"values\":{");
    for (int i = 0; i < np; i++) fprintf(f, "%s\"%s\":\"%s\"", i ? "," : "", pv[i].k, pv[i].v);   /* keys and values: plain ids */
    fprintf(f, "}}\n");
    if (fclose(f)) return;
#ifdef _WIN32
    MoveFileExA(t, file, MOVEFILE_REPLACE_EXISTING);
#else
    rename(t, file);
#endif
}
static int plain(const char *s) {                         /* ids only: no quote, backslash or control character */
    for (; s && *s; s++) if (*s == '"' || *s == '\\' || (unsigned char)*s < 0x20) return 0;
    return 1;
}
int cm_pref_get(const char *key, char *out, size_t n) {
    load();
    for (int i = 0; i < np; i++) if (!strcmp(pv[i].k, key)) { snprintf(out, n, "%s", pv[i].v); return 0; }
    return -1;
}
void cm_pref_set(const char *key, const char *value) {
    if (!key || !plain(key) || (value && !plain(value)) || strlen(key) >= sizeof pv[0].k || (value && strlen(value) >= sizeof pv[0].v)) return;
    load();
    int at = -1; for (int i = 0; i < np; i++) if (!strcmp(pv[i].k, key)) at = i;
    if (!value) { if (at < 0) return; memmove(&pv[at], &pv[at + 1], (size_t)(np - at - 1) * sizeof pv[0]); np--; }
    else if (at >= 0) { if (!strcmp(pv[at].v, value)) return; snprintf(pv[at].v, sizeof pv[at].v, "%s", value); }
    else if (np < NP) { snprintf(pv[np].k, sizeof pv[np].k, "%s", key); snprintf(pv[np].v, sizeof pv[np].v, "%s", value); np++; }
    else return;
    save();
}
void cm_prefs_batch(int on) {
    if (on) { batch++; return; }
    if (batch > 0 && --batch == 0 && dirty) save();
}
/* free text (a unit name, a search) as a plain value: '%' + 2 hex digits for '%', quote, backslash, ',', '|', '/' and
   control characters (the separators the callers use in their lists) */
void cm_pref_enc(const char *s, char *out, size_t n) {
    static const char hx[] = "0123456789ABCDEF"; size_t k = 0;
    for (; s && *s && k + 4 < n; s++) {
        unsigned char c = (unsigned char)*s;
        if (c == '%' || c == '"' || c == '\\' || c == ',' || c == '|' || c == '/' || c < 0x20) { out[k++] = '%'; out[k++] = hx[c >> 4]; out[k++] = hx[c & 15]; }
        else out[k++] = (char)c;
    }
    if (n) out[k < n ? k : n - 1] = 0;
}
static int hexv(char c) { return c >= '0' && c <= '9' ? c - '0' : c >= 'A' && c <= 'F' ? c - 'A' + 10 : c >= 'a' && c <= 'f' ? c - 'a' + 10 : -1; }
void cm_pref_dec(const char *s, char *out, size_t n) {
    size_t k = 0;
    for (; s && *s && k + 1 < n; s++) {
        int a = *s == '%' ? hexv(s[1]) : -1, b = a >= 0 ? hexv(s[2]) : -1;
        if (a >= 0 && b >= 0) { out[k++] = (char)(a << 4 | b); s += 2; }
        else out[k++] = *s;
    }
    if (n) out[k] = 0;
}
/* a bigger kept value in its own file next to settings.json: <dir>/<name> (temp + rename) */
static int blob_path(const char *name, char *out, size_t n) {
    if (path() || !name || strchr(name, '/') || strchr(name, '\\')) return -1;
    snprintf(out, n, "%s", file); char *e = strrchr(out, '/'); if (!e) return -1;
    snprintf(e + 1, n - (size_t)(e + 1 - out), "%s", name);
    return 0;
}
int cm_pref_blob_set(const char *name, const char *data) {
    char f[1100], t[1120]; if (blob_path(name, f, sizeof f)) return -1;
    snprintf(t, sizeof t, "%s.tmp", f);
    FILE *o = fopen(t, "wb"); if (!o) return -1;
    size_t n = data ? strlen(data) : 0;
    if (fwrite(data ? data : "", 1, n, o) != n) { fclose(o); return -1; }
    if (fclose(o)) return -1;
#ifdef _WIN32
    return MoveFileExA(t, f, MOVEFILE_REPLACE_EXISTING) ? 0 : -1;
#else
    return rename(t, f) ? -1 : 0;
#endif
}
char *cm_pref_blob_get(const char *name) {
    char f[1100]; if (blob_path(name, f, sizeof f)) return NULL;
    FILE *i = fopen(f, "rb"); if (!i) return NULL;
    fseek(i, 0, SEEK_END); long n = ftell(i); fseek(i, 0, SEEK_SET);
    char *b = n >= 0 && n < (8L << 20) ? malloc((size_t)n + 1) : NULL;
    if (b && fread(b, 1, (size_t)n, i) != (size_t)n) { free(b); b = NULL; }
    fclose(i);
    if (b) b[n] = 0;
    return b;
}
