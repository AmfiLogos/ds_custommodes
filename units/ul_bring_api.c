/* ul_bring_api.c - Units Laboratory for the multiplayer mod's Bring Units (owner 2026-10-20, PLAN_v021 E3): when a player
 * has Units Laboratory, Bring (PvP and Foreign Expedition) also lists his My Units and offers "Build a unit" (the Unit
 * Builder in game); the brought unit carries its FULL data (the entry {closure, manifest} of pb_entry, as JSON text), so
 * the other players never need Units Laboratory. Multiplayer finds this table at run time (dlsym / GetProcAddress
 * "ul_bring_api_get"); a missing module or symbol = no My Units source, no button. Text crosses the module border, never
 * a jv (each module has its own fe_json copy). Main thread only. */
#include "myunits.h"
#include "../builder/bld.h"
#include "cm_il.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#define UL_EXPORT __declspec(dllexport)
#else
#define UL_EXPORT __attribute__((visibility("default")))
#endif

typedef struct ul_bring_api {
    int version;                                                                       /* 1 */
    int   (*count)(void);                                                              /* My Units (reads the library) */
    int   (*info)(int i, char id[17], char name[64], char race[64], char portrait[96], int *level);   /* 0 ok */
    char *(*entry_json)(const char *id, const char *player_uid, char *err, size_t errn);   /* malloc'd {closure, manifest} or NULL + err */
    void  (*free_str)(char *s);
    int   (*open_builder)(void (*saved)(const char *unit_id, void *ud), void *ud);     /* 0 = the Unit Builder opened */
    char *(*build_json)(const char *id);                                               /* malloc'd builder JSON of a unit (its card), or NULL */
} ul_bring_api;

static jv *lib;
static void data_dir(void) {
    static int done; if (done) return; done = 1;
    char p[1200] = ""; cm_m m = cm_il_method("UnityEngine", "Application", "get_persistentDataPath", 0, NULL);
    if (m.fn) { cm_il_utf8(CM_CALL(m, Obj, const void *)(m.mi), p, sizeof p); if (p[0]) mu_set_data_dir(p); }
}
static int api_count(void) {
    data_dir();
    char err[300]; jv *l = mu_load(err, sizeof err);
    if (!l) return 0;
    jfree(lib); lib = l;
    return (int)mu_count(lib);
}
static int api_info(int i, char id[17], char name[64], char race[64], char portrait[96], int *level) {
    mu_info x; if (!lib || i < 0 || mu_info_at(lib, (size_t)i, &x)) return -1;
    memcpy(id, x.id, 17); memcpy(name, x.name, 64); memcpy(race, x.race, 64); memcpy(portrait, x.portrait, 96); *level = x.lvl;
    return 0;
}
static char *api_entry_json(const char *id, const char *player_uid, char *err, size_t errn) {
    if (!lib && api_count() <= 0) { snprintf(err, errn, "%s", "My Units is empty"); return NULL; }
    char ids[1][17]; snprintf(ids[0], 17, "%s", id ? id : "");
    char notes[600];
    jv *en = mu_entries(lib, ids, 1, player_uid ? player_uid : "", notes, sizeof notes, err, errn);
    if (!en) { api_count(); if (lib) en = mu_entries(lib, ids, 1, player_uid ? player_uid : "", notes, sizeof notes, err, errn); }   /* a unit saved since: read again */
    if (!en || !en->cnt) { jfree(en); return NULL; }
    size_t n = 0; char *s = jdump(en->v[0], 0, &n);
    jfree(en);
    return s;
}
static void api_free(char *s) { free(s); }
static int api_open_builder(void (*saved)(const char *unit_id, void *ud), void *ud) {
    if (bld_is_open()) return -1;
    cm_builder_open(NULL, saved, ud);
    return bld_is_open() ? 0 : -1;
}
static char *api_build_json(const char *id) {
    if (!lib) api_count();
    const jv *u = lib ? mu_find(lib, id) : NULL; if (!u) { api_count(); u = lib ? mu_find(lib, id) : NULL; }
    const jv *b = u ? jget(u, "build") : NULL; if (!b) return NULL;
    size_t n = 0; return jdump(b, 0, &n);
}
static const ul_bring_api API = { 1, api_count, api_info, api_entry_json, api_free, api_open_builder, api_build_json };
UL_EXPORT const ul_bring_api *ul_bring_api_get(int version) { return version <= API.version ? &API : NULL; }
