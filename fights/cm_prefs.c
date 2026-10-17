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

#define NP 32
static struct { char k[48], v[96]; } pv[NP];
static int np, loaded;
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
