/* tf_worlds.c - see tf_worlds.h */
#include "tf_worlds.h"
#include "tf_dps.h"
#include "cm_win.h"
#include "../fe/fe_json.h"
#include "cm_il.h"
#include <ctype.h>
#include "i18n/ds_i18n.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <windows.h>
#include <direct.h>
#define mkdir_(p) _mkdir(p)
#else
#include <dirent.h>
#define mkdir_(p) mkdir(p, 0755)
#endif

#define NW 64
static char W[NW][33]; static int nw, loaded;             /* test world GUIDs: 32 lowercase hex, no dashes; newest last */

static void norm(const char *g, char out[33]) {
    int k = 0;
    for (; g && *g && k < 32; g++) if (isxdigit((unsigned char)*g)) out[k++] = (char)tolower((unsigned char)*g);
    out[k] = 0;
}
static int reg_path(char *p, size_t n) {
    char d[1024]; if (cmw_game_data(d, sizeof d)) return -1;
    snprintf(p, n, "%s/Mods", d); mkdir_(p);
    snprintf(p, n, "%s/Mods/custom-modes", d); mkdir_(p);
    snprintf(p, n, "%s/Mods/custom-modes/test_worlds.json", d);
    return 0;
}
static void reg_load(void) {                             /* once the game's data folder is known (first frame) */
    if (loaded) return;
    char p[1100]; nw = 0; if (reg_path(p, sizeof p)) return;
    loaded = 1;
    char e[200]; jv *r = jload(p, e, sizeof e); if (!r) return;
    jv *a = jget(r, "worlds");
    for (size_t i = 0; a && a->t == JN_ARR && i < a->cnt && nw < NW; i++) {
        char *s = jstrdup_raw(a->v[i]); norm(s, W[nw]); free(s);
        if (strlen(W[nw]) == 32) nw++;
    }
    jfree(r);
}
static void reg_save(void) {
    char p[1100], t[1120]; if (reg_path(p, sizeof p)) return;
    snprintf(t, sizeof t, "%s.tmp", p);
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
int tfw_is_test(const char *guid, const char *save_name) {
    reg_load();
    char g[33]; norm(guid, g);
    for (int i = 0; i < nw; i++) {
        if (g[0] && !strcmp(g, W[i])) return 1;
        if (save_name && strstr(save_name, W[i])) return 1;   /* "<team>_<fight>__AutoSave_<guid hex>_<n>" */
    }
    return 0;
}

/* the save files of an older test world: every .json in Saves whose name carries its GUID */
static void remove_world_files(const char *saves, const char *g) {
#ifdef _WIN32
    char pat[1200]; snprintf(pat, sizeof pat, "%s\\*%s*.json", saves, g);
    WIN32_FIND_DATAA fd; HANDLE h = FindFirstFileA(pat, &fd);
    if (h == INVALID_HANDLE_VALUE) return;
    do { char f[1400]; snprintf(f, sizeof f, "%s\\%s", saves, fd.cFileName); if (DeleteFileA(f)) cmw_log("tf: old test world file removed: %s", fd.cFileName); } while (FindNextFileA(h, &fd));
    FindClose(h);
#else
    DIR *d = opendir(saves); if (!d) return;
    struct dirent *e;
    while ((e = readdir(d))) {
        size_t l = strlen(e->d_name);
        if (l < 6 || strcmp(e->d_name + l - 5, ".json") || !strstr(e->d_name, g)) continue;
        char f[1400]; snprintf(f, sizeof f, "%s/%s", saves, e->d_name);
        if (!remove(f)) cmw_log("tf: old test world file removed: %s", e->d_name);
    }
    closedir(d);
#endif
}

static int world_has_files(const char *saves, const char *g) {
#ifdef _WIN32
    char pat[1200]; snprintf(pat, sizeof pat, "%s\\*%s*.json", saves, g);
    WIN32_FIND_DATAA fd; HANDLE h = FindFirstFileA(pat, &fd);
    if (h == INVALID_HANDLE_VALUE) return 0;
    FindClose(h); return 1;
#else
    DIR *d = opendir(saves); if (!d) return 0;
    struct dirent *e; int found = 0;
    while (!found && (e = readdir(d))) found = strstr(e->d_name, g) != NULL;
    closedir(d); return found;
#endif
}

static char *keep; static size_t keep_n;                            /* the last test world's start save (Restart test) */
static int in_test;
int tfw_write(const char *save, size_t n, char *name_out, size_t name_n, char *err, size_t errn) {
    reg_load();
    jv *s = jparse(save, n, err, errn); if (!s) return -1;
    jv *h = jget(s, "CampaignSaveHeader");
    char *name = h ? jstrdup_raw(jget(h, "SaveName")) : NULL, *guid = h ? jstrdup_raw(jget(h, "CampaignGuid")) : NULL;
    char *fixed = NULL; size_t fixed_n = 0;
    if (tfw_consecration_fix(s)) { fixed = jdump(s, 0, &fixed_n); if (fixed) { save = fixed; n = fixed_n; } }   /* no "Lock Consecration" box */
    jfree(s);
    char g[33]; norm(guid, g); free(guid);
    if (!name || !name[0] || strlen(g) != 32 || strpbrk(name, "/\\:") || strstr(name, "..")) {
        snprintf(err, errn, "%s", TR("The save that was made has no usable name.")); free(name); free(fixed); return -1;
    }
    char saves[1100], p[1400], t[1420];
    if (cmw_saves_dir(saves, sizeof saves)) { snprintf(err, errn, "%s", TR("The game's save folder is not known.")); free(name); free(fixed); return -1; }
    mkdir_(saves);
    /* older test worlds go first (only the newest one is kept); their GUIDs stay listed until the title removes them all
       (a test world loaded now, left by Restart test, may still write a save after this) */
    for (int i = 0; i < nw; i++) remove_world_files(saves, W[i]);
    if (nw == NW) { memmove(W[0], W[1], sizeof W[0] * (NW - 1)); nw--; }
    snprintf(W[nw++], 33, "%s", g); reg_save();                        /* listed BEFORE the file exists: never visible */
    snprintf(p, sizeof p, "%s/%s.json", saves, name); snprintf(t, sizeof t, "%s.tmp", p);
    FILE *f = fopen(t, "wb");
    if (!f || fwrite(save, 1, n, f) != n) { if (f) fclose(f); snprintf(err, errn, "%s", TR("The test world could not be written into the Saves folder.")); free(name); free(fixed); return -1; }
    fclose(f);
#ifdef _WIN32
    if (!MoveFileExA(t, p, MOVEFILE_REPLACE_EXISTING)) { snprintf(err, errn, "%s", TR("The test world could not be written into the Saves folder.")); free(name); free(fixed); return -1; }
#else
    if (rename(t, p)) { snprintf(err, errn, "%s", TR("The test world could not be written into the Saves folder.")); free(name); free(fixed); return -1; }
#endif
    snprintf(name_out, name_n, "%s", name); free(name);
    if (save != keep) { char *k = malloc(n); if (k) { memcpy(k, save, n); free(keep); keep = k; keep_n = n; } }   /* for Restart test */
    free(fixed);
    cmw_log("tf: test world %s written (%zu bytes)", name_out, n);
    return 0;
}
/* Restart test (owner 2026-10-25, every Test Fights world): the save the Start wrote, written and loaded again by the same
   path as Start (tfw_write, tfw_loading, the game's LoadCampaignRequested): the same map, squad, difficulty and kits, from
   a fresh state, without the title */
int tf_restart_ready(void) { return keep && in_test; }
int tf_restart(void) {
    if (!tf_restart_ready()) return -1;
    char *c = malloc(keep_n), name[300], err[300]; if (!c) return -1;
    memcpy(c, keep, keep_n);
    int r = tfw_write(c, keep_n, name, sizeof name, err, sizeof err);
    free(c);
    if (r) { cmw_log("tf: Restart test: %s", err); return -1; }
    tfw_loading(); tf_dps_restarting();
    r = cmw_load_campaign(name);
    cmw_log("tf: Restart test: %s %s", name, r ? "NOT queued" : "queued");
    return r;
}

/* Leaving a test world: back on the title, its saves are removed (the game's own Remove is switched off in this build:
 * CampaignSaveDeletionService.IsAvailable returns false), then the game refreshes its save list and the title's Continue,
 * which otherwise points at the test world, and the player's records are set back (og_restore, below). Not while our own
 * load is starting: from Start (tfw_loading, loading 1) until the game has read the test world (CampaignSaveLoader.TryLoad,
 * loading 2) and the title is gone. A load from inside a world (Restart test) passes through the title scene first
 * (LoadCampaign: MoveSceneToTitle, then TryLoad), so "the title is gone" alone would remove the world it is loading.
 * Checked every 30 frames. */
static int loading, frames;
static int load_seen;                                             /* TryLoad hooked (below); in_test: a test world is loaded */
static void og_restore(void);
void tfw_loading(void) { loading = 1; }
static void tick(void *ud) {
    (void)ud;
    if (++frames % 30) return;
    int title = cmw_title_shown();
    if ((loading == 2 || (loading && !load_seen)) && !title) loading = 0;
    if (loading || !title) return;
    if (in_test) { og_restore(); in_test = 0; }
    reg_load();
    if (!nw) return;
    char saves[1100]; if (cmw_saves_dir(saves, sizeof saves)) return;
    int changed = 0;
    for (int i = 0; i < nw; i++) if (world_has_files(saves, W[i])) { remove_world_files(saves, W[i]); changed = 1; }
    nw = 0; reg_save();
    if (changed) cmw_saves_changed();
}
static int f_show(void *ud, const char *guid, const char *save_name, int toggle_on) {
    (void)ud; (void)toggle_on;
    return !tfw_is_test(guid, save_name);
}
/* ---------------------------------------------------------------- wiki-made saves: no "Lock Consecration" box */
/* SaveLoadHelper.DeserializeCampaignSaveDataSafely(string json, JsonSerializerSettings, string saveFilePath), static: every
   campaign load reads the file text and hands it here. A wiki-made save gets the fixed text; anything else goes through. */
/* it returns Refactor.CampaignSaveData, a STRUCT of 0x3B0 bytes (dump: last field StoragePolicySaveData at 0x3A8, one
   reference): returned through the caller's result pointer (arm64 x8, x64 hidden first argument), so the hook is typed
   with the same struct and the size is checked against the runtime before hooking */
typedef struct { uint8_t b[0x3B0]; } CampaignSaveData;
typedef CampaignSaveData (*deser_fn)(Obj json, Obj settings, Obj path, const void *mi);
static deser_fn o_deser;
static Obj fix_text(Obj json, const char *stem) {
    int32_t len = json ? CM_AT(json, 0x10, int32_t) : 0;
    if (len <= 0 || len > 512 * 1024 * 1024 / 3) return NULL;
    size_t cap = (size_t)len * 3 + 1; char *u = malloc(cap); if (!u) return NULL;
    cm_il_utf8(json, u, cap);
    char e[200]; jv *s = jparse(u, strlen(u), e, sizeof e); free(u);
    if (!s) { cmw_log("tf: wiki save %s: not read (%s), left as it is", stem, e); return NULL; }
    Obj r = NULL;
    if (tfw_consecration_fix(s)) {
        size_t n = 0; char *t = jdump(s, 0, &n);
        if (t) { r = cm_il_str(t); free(t); }
        cmw_log("tf: wiki save %s: Lock Consecration set off and acknowledged at load%s", stem, r ? "" : " FAILED (string)");
    }
    jfree(s);
    return r;
}
static CampaignSaveData h_deser(Obj json, Obj settings, Obj path, const void *mi) {
    char p[1100]; cm_il_utf8(path, p, sizeof p);
    const char *b = p; for (const char *c = p; *c; c++) if (*c == '/' || *c == '\\') b = c + 1;
    char stem[300]; snprintf(stem, sizeof stem, "%s", b);
    size_t l = strlen(stem); if (l > 5 && !strcmp(stem + l - 5, ".json")) stem[l - 5] = 0;
    if (tfw_wiki_made(stem)) { Obj f = fix_text(json, stem); if (f) json = f; }
    tf_dps_world(stem);                                  /* the DPS Check's panel and hit log: on in its world only */
    return o_deser(json, settings, path, mi);
}

/* ---------------------------------------------------------------- the title never shows a test world */
/* (owner 2026-10-25: after a test the title's background was the test world's base.) The title takes its background and
   its Continue save from the game's save list, newest first, and a test world's saves are on that list until the tick
   above removes them (or until the next start when the game was closed inside a fight). Both pickers skip them, so the
   title shows and continues the player's own last world, as if no test had run:
   - background: TitleBasePreviewSaveSelector.Select tries each save, newest first, with TryLoadCampaignSaveData(reader,
     header, out data, out reason) and takes the first that loads; for a test world it answers false (as for a save it
     cannot read), so Select goes on to the next world (none left: the default title scene);
   - Continue: LatestCampaignSaveSelector.TrySelect(reader, out header, refresh) sorts its _candidateHeaders newest first
     and takes the first whose file exists (reader.IsExistFile); when that is a test world, the next save of that list
     that is no test world and whose file exists is taken instead; none = no Continue.
   CampaignSaveHeader (a struct of more than 16 bytes) is passed by reference on arm64 and x64; its fields by name. Both
   methods have direct callers on Mac and Windows (not inlined away). Nothing is written: the title only reads. */
static int32_t hd_guid = -1, hd_name = -1, off_cand = -1, off_items = -1, off_size = -1; static uint32_t hd_size;
static void *k_csl; static void *(*x_obj_class)(Obj);
static uint8_t (*f_exist)(Obj self, Obj name, const void *mi);
static void guid_hex(const uint8_t *g, char out[33]) {           /* System.Guid as ToString("N") */
    uint32_t a; uint16_t b, c; memcpy(&a, g, 4); memcpy(&b, g + 4, 2); memcpy(&c, g + 6, 2);
    snprintf(out, 33, "%08x%04x%04x%02x%02x%02x%02x%02x%02x%02x%02x", a, b, c, g[8], g[9], g[10], g[11], g[12], g[13], g[14], g[15]);
}
static int hdr_test(const uint8_t *h, char *name, size_t nn) {
    char g[33]; guid_hex(h + hd_guid, g);
    name[0] = 0; cm_il_utf8(*(Obj const *)(h + hd_name), name, nn);
    cmw_dirs_now();                                                 /* the list of test worlds lives in the game's data folder */
    return tfw_is_test(g, name[0] ? name : NULL);
}
typedef uint8_t (*tryload_fn)(Obj self, Obj reader, const uint8_t *hdr, void *data, Obj *reason, const void *mi);
static tryload_fn o_tryload;
static uint8_t h_tryload(Obj self, Obj reader, const uint8_t *hdr, void *data, Obj *reason, const void *mi) {
    char n[300];
    if (hdr && hdr_test(hdr, n, sizeof n)) { cmw_log("tf: title background: test world %s skipped", n); return 0; }
    return o_tryload(self, reader, hdr, data, reason, mi);
}
typedef uint8_t (*trysel_fn)(Obj self, Obj reader, uint8_t *out, uint8_t refresh, const void *mi);
static trysel_fn o_trysel;
static uint8_t h_trysel(Obj self, Obj reader, uint8_t *out, uint8_t refresh, const void *mi) {
    uint8_t r = o_trysel(self, reader, out, refresh, mi);
    char n[300], t[300];
    if (!r || !out || !hdr_test(out, t, sizeof t)) return r;
    Obj list = self && off_cand >= 0 ? CM_AT(self, off_cand, Obj) : NULL;
    if (list && off_items < 0) { void *k = x_obj_class(list); off_items = cm_il_field(k, "_items"); off_size = cm_il_field(k, "_size"); }
    Obj items = list && off_items >= 0 && off_size >= 0 ? CM_AT(list, off_items, Obj) : NULL;
    int32_t cnt = items ? CM_AT(list, off_size, int32_t) : 0;
    int ok = items && f_exist && reader && x_obj_class(reader) == k_csl;
    for (int32_t i = 0; ok && i < cnt; i++) {
        const uint8_t *h = (const uint8_t *)items + 0x20 + (size_t)i * hd_size;   /* array data after klass, monitor, bounds, length */
        Obj s = CM_AT(h, hd_name, Obj);
        if (!s || CM_AT(s, 0x10, int32_t) <= 0 || hdr_test(h, n, sizeof n) || !f_exist(reader, s, NULL)) continue;
        memcpy(out, h, hd_size);
        cmw_log("tf: title Continue: test world %s skipped, %s instead", t, n);
        return 1;
    }
    memset(out, 0, hd_size);
    cmw_log("tf: title Continue: test world %s skipped, no other world%s", t, ok ? "" : " (save list not read)");
    return 0;
}
static void title_init(const DSLHost *host, DSLPlugin *self) {
    uint32_t (*vsize)(void *, uint32_t *) = host->il2cpp_fn ? (uint32_t (*)(void *, uint32_t *))host->il2cpp_fn("il2cpp_class_value_size") : NULL;
    *(void **)&x_obj_class = host->il2cpp_fn ? host->il2cpp_fn("il2cpp_object_get_class") : NULL;
    void *k_hdr = cm_il_class("Refactor", "CampaignSaveHeader"), *k_sel = cm_il_class("Refactor.UI", "LatestCampaignSaveSelector");
    k_csl = cm_il_class("Refactor", "CampaignSaveLoader");
    int32_t g0 = k_hdr ? cm_il_field(k_hdr, "CampaignGuid") : -1, n0 = k_hdr ? cm_il_field(k_hdr, "SaveName") : -1;   /* value type: offsets count the object header */
    hd_size = vsize && k_hdr ? vsize(k_hdr, NULL) : 0;
    if (g0 >= 0 && n0 > g0) { hd_guid = 0; hd_name = n0 - g0; }
    off_cand = k_sel ? cm_il_field(k_sel, "_candidateHeaders") : -1;
    *(void **)&f_exist = host->find_method_address ? host->find_method_address("Refactor", "CampaignSaveLoader", "IsExistFile", 1) : NULL;
    if (hd_name < 0 || hd_size < (uint32_t)hd_name + 8 || hd_size % 8 || !x_obj_class || !k_csl) {
        cmw_log("tf: CampaignSaveHeader not read (guid %d name %d size %#x): the title may show test worlds", g0, n0, hd_size); return;
    }
    int r1 = host->hook_method ? host->hook_method(self, "Refactor", "TitleBasePreviewSaveSelector", "TryLoadCampaignSaveData", 4, (void *)h_tryload, (void **)&o_tryload) : -1;
    int r2 = host->hook_method && off_cand >= 0 && f_exist ? host->hook_method(self, "Refactor.UI", "LatestCampaignSaveSelector", "TrySelect", 3, (void *)h_trysel, (void **)&o_trysel) : -1;
    cmw_log("tf: the title skips test worlds: background %s (%d), Continue %s (%d)", r1 ? "hook refused" : "on", r1, r2 ? "hook refused" : "on", r2);
}

/* ---------------------------------------------------------------- a test leaves the player's records as they were */
/* OutGame.json is what the game remembers across worlds: tutorials, the items obtained, credits seen, Downfall unlocked.
   A test world records into it like any world (test 2026-10-25: the squad's gear went into the items obtained). So:
   - while a test world is loaded the game's writes of it (SaveLoadHelper.SaveOutGameRecord) are held: the file stays
     byte-equal, also when the game is closed or crashes inside the test (nothing to put back at the next start);
   - when a test world loads, the records in memory are copied (OutGameRecordContainer.CreateSaveData, what the game
     writes); when the test is left (the title, or another world loads) they are set back from that copy: each record
     container's constructor again, on the same objects, so everything that holds them keeps working.
   CampaignSaveLoader.TryLoad(name, out data, out reason), called for every world the game loads, tells which world it is.
   UserCampaignLog.json is left as it is: a test changes only its PlayerApplicationMinute (minutes the game was on), as
   any session does. */
typedef struct { Obj tut, items, credits, unlock; } OutGameSaveData;   /* Refactor.Main.OutGameSaveData: 4 references */
static int32_t off_mf_og = -1, off_og_tut = -1, off_og_items = -1, off_og_cred = -1, off_og_unl = -1, off_unl_on = -1, off_unld_on = -1;
static void *k_mf; static cm_m m_og_create, m_tut_ctor, m_items_ctor, m_cred_ctor;
static uintptr_t (*x_hnew)(Obj, bool); static Obj (*x_hget)(uintptr_t); static void (*x_hfree)(uintptr_t);
static uintptr_t snap[4]; static int og_ok;
static Obj og_records(void) {
    Obj mf = k_mf ? cm_find(k_mf, false) : NULL;
    return mf && off_mf_og >= 0 ? CM_AT(mf, off_mf_og, Obj) : NULL;
}
static void og_snapshot(void) {
    if (!og_ok) return;
    Obj rc = og_records();
    if (!rc || !m_og_create.fn || !x_hnew) { cmw_log("tf: player's records not copied (container %p): a test may change them", rc); return; }
    OutGameSaveData d = CM_CALL(m_og_create, OutGameSaveData, Obj, const void *)(rc, m_og_create.mi);
    Obj o[4] = { d.tut, d.items, d.credits, d.unlock };
    for (int i = 0; i < 4; i++) snap[i] = o[i] ? x_hnew(o[i], false) : 0;   /* strong: our copy lives until the test ends */
    cmw_log("tf: test world: the player's records copied, OutGame.json writes held");
}
static void og_restore(void) {
    if (!snap[0] && !snap[1] && !snap[2] && !snap[3]) return;
    Obj rc = og_records(), d[4];
    for (int i = 0; i < 4; i++) { d[i] = snap[i] ? x_hget(snap[i]) : NULL; }
    Obj tut = rc && off_og_tut >= 0 ? CM_AT(rc, off_og_tut, Obj) : NULL, items = rc && off_og_items >= 0 ? CM_AT(rc, off_og_items, Obj) : NULL;
    Obj cred = rc && off_og_cred >= 0 ? CM_AT(rc, off_og_cred, Obj) : NULL, unl = rc && off_og_unl >= 0 ? CM_AT(rc, off_og_unl, Obj) : NULL;
    int ok = 0;
    if (tut && d[0] && m_tut_ctor.fn) { CM_CALL(m_tut_ctor, void, Obj, Obj, const void *)(tut, d[0], m_tut_ctor.mi); ok++; }
    if (items && d[1] && m_items_ctor.fn) { CM_CALL(m_items_ctor, void, Obj, Obj, const void *)(items, d[1], m_items_ctor.mi); ok++; }
    if (cred && d[2] && m_cred_ctor.fn) { CM_CALL(m_cred_ctor, void, Obj, Obj, const void *)(cred, d[2], m_cred_ctor.mi); ok++; }
    if (unl && d[3] && off_unl_on >= 0 && off_unld_on >= 0) { CM_AT(unl, off_unl_on, uint8_t) = CM_AT(d[3], off_unld_on, uint8_t); ok++; }
    for (int i = 0; i < 4; i++) if (snap[i]) { x_hfree(snap[i]); snap[i] = 0; }
    cmw_log("tf: test left: the player's records set back (%d of 4)", ok);
}
typedef void (*saveog_fn)(OutGameSaveData d, const void *mi);
static saveog_fn o_saveog;
static void h_saveog(OutGameSaveData d, const void *mi) {
    static int said;
    if (in_test && og_ok) { if (!said++) cmw_log("tf: OutGame.json write held (test world)"); return; }
    said = 0;
    o_saveog(d, mi);
}
typedef uint8_t (*load_fn)(Obj self, Obj name, void *data, Obj *reason, const void *mi);
static load_fn o_load;
static uint8_t h_load(Obj self, Obj name, void *data, Obj *reason, const void *mi) {
    char n[300]; cm_il_utf8(name, n, sizeof n);
    cmw_dirs_now();
    if (n[0] && tfw_is_test(NULL, n)) {
        if (!in_test) og_snapshot();
        in_test = 1; loading = 2;                                   /* the game has read the test world */
    } else {
        if (in_test) og_restore();
        in_test = 0; loading = 0;
    }
    return o_load(self, name, data, reason, mi);
}
static void og_init(const DSLHost *host, DSLPlugin *self) {
    *(void **)&x_hnew = host->il2cpp_fn ? host->il2cpp_fn("il2cpp_gchandle_new") : NULL;
    *(void **)&x_hget = host->il2cpp_fn ? host->il2cpp_fn("il2cpp_gchandle_get_target") : NULL;
    *(void **)&x_hfree = host->il2cpp_fn ? host->il2cpp_fn("il2cpp_gchandle_free") : NULL;
    k_mf = cm_il_class("Refactor.Main", "MainFlow");
    void *k_og = cm_il_class("Refactor.Main", "OutGameRecordContainer"), *k_unl = cm_il_class("Refactor.Main", "DifficultyUnlockRecordContainer");
    void *k_unld = cm_il_class("Refactor.Main", "DifficultyUnlockSaveData"), *k_ogd = cm_il_class("Refactor.Main", "OutGameSaveData");
    off_mf_og = k_mf ? cm_il_field(k_mf, "_outGameRecord") : -1;
    if (k_og) { off_og_tut = cm_il_field(k_og, "_tutorialContainer"); off_og_items = cm_il_field(k_og, "_itemObtainedContainer");
                off_og_cred = cm_il_field(k_og, "_creditRollRecordContainer"); off_og_unl = cm_il_field(k_og, "_difficultyUnlockRecordContainer"); }
    off_unl_on = k_unl ? cm_il_field(k_unl, "_isDownfallUnlocked") : -1;
    off_unld_on = k_unld ? cm_il_field(k_unld, "IsDownfallUnlocked") : -1;
    m_og_create = cm_il_method("Refactor.Main", "OutGameRecordContainer", "CreateSaveData", 0, NULL);
    m_tut_ctor = cm_il_method("Refactor.Main", "TutorialContainer", ".ctor", 1, NULL);
    m_items_ctor = cm_il_method("Refactor.Main", "ItemObtainedContainer", ".ctor", 1, NULL);
    m_cred_ctor = cm_il_method("Refactor.Main", "CreditRollRecordContainer", ".ctor", 1, NULL);
    /* OutGameSaveData = TutorialSaveData, ItemObtainedSaveData, CreditRollSaveData, DifficultyUnlockSaveData, in that order */
    int32_t f0 = k_ogd ? cm_il_field(k_ogd, "TutorialSaveData") : -1, f3 = k_ogd ? cm_il_field(k_ogd, "DifficultyUnlockSaveData") : -1;
    int lay = f0 >= 0 && f3 - f0 == 3 * (int32_t)sizeof(Obj) && k_ogd && cm_il_field(k_ogd, "ItemObtainedSaveData") - f0 == (int32_t)sizeof(Obj)
              && cm_il_field(k_ogd, "CreditRollSaveData") - f0 == 2 * (int32_t)sizeof(Obj);
    /* the world loads first: they also end our own load's wait (loading 2), with or without the records */
    int r1 = host->hook_method ? host->hook_method(self, "Refactor", "CampaignSaveLoader", "TryLoad", 3, (void *)h_load, (void **)&o_load) : -1;
    load_seen = !r1;
    og_ok = lay && off_mf_og >= 0 && off_og_tut >= 0 && off_og_items >= 0 && off_og_cred >= 0 && off_og_unl >= 0 && m_og_create.fn
            && m_tut_ctor.fn && m_items_ctor.fn && m_cred_ctor.fn && x_hnew && x_hget && x_hfree;
    int r2 = og_ok && load_seen && host->hook_method ? host->hook_method(self, "Refactor", "SaveLoadHelper", "SaveOutGameRecord", 1, (void *)h_saveog, (void **)&o_saveog) : -1;
    if (r2) og_ok = 0;
    cmw_log("tf: world loads %s (%d); a test leaves OutGame.json as it was: %s (%d; layout %d, MainFlow %d, ctors %p %p %p)", r1 ? "hook refused" : "seen", r1,
            og_ok ? "on" : "OFF", r2, lay, off_mf_og, m_tut_ctor.fn, m_items_ctor.fn, m_cred_ctor.fn);
}

int tfw_init(const DSLHost *host, DSLPlugin *self) {
    static const DSLLoadFilter F = { f_show, NULL, NULL, NULL };
    int r = dsl_loadlist_filter(host, self, &F);
    cmw_tick_add(tick, NULL);
    cmw_log("tf: load-list filter for test worlds %s", r ? "NOT registered" : "on");
    cm_il_init(host, self);
    title_init(host, self);
    og_init(host, self);
    uint32_t (*vsize)(void *, uint32_t *) = host->il2cpp_fn ? (uint32_t (*)(void *, uint32_t *))host->il2cpp_fn("il2cpp_class_value_size") : NULL;
    void *k_csd = cm_il_class("Refactor", "CampaignSaveData");
    uint32_t sz = vsize && k_csd ? vsize(k_csd, NULL) : 0;
    if (sz != sizeof(CampaignSaveData)) { cmw_log("tf: CampaignSaveData is %#x bytes, not %#zx: wiki-made save fix off", sz, sizeof(CampaignSaveData)); return r; }
    int hr = host->hook_method ? host->hook_method(self, "Refactor", "SaveLoadHelper", "DeserializeCampaignSaveDataSafely", 3, (void *)h_deser, (void **)&o_deser) : -1;
    cmw_log("tf: wiki-made saves load without the Lock Consecration box: %s (%d)", hr ? "hook refused" : "on", hr);
    if (!hr) tf_dps_init(host, self);                    /* knows a DPS Check world from this hook */
    return r;
}
