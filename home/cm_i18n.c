// cm_i18n.c - see cm_i18n.h. The one DSI_IMPLEMENTATION of custom-modes.dylib (the -mp part has its own: the symbols
// are hidden, each image keeps its own tables).
#define DSI_IMPLEMENTATION
#include "cm_i18n.h"
#include "fights/cm_win.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const DSLHost *H; static DSLPlugin *ME;
#define NCB 16
static void (*cbs[NCB])(void); static int ncb;
int cm_i18n_on_change(void (*fn)(void)) {
    if (!fn) return -1;
    for (int i = 0; i < ncb; i++) if (cbs[i] == fn) return 0;
    if (ncb >= NCB) return -1;
    cbs[ncb++] = fn;
    return 0;
}
int cm_i18n_loader_relabels(void) { return H && H->api_version >= 4; }

/* the saved choice: GeneralSettingData.SaveCurrentLanguageType of the UserSetting.json the game reads, looked for where the
   loader looks (dsl_lang_early): $DSML_DATA, $DSRIG_DIR/data, then the folder above host->data_dir (<data>/Mods; it is the
   profile's folder when DSML_DATA is not set, so it can miss). 0 (English) when unknown. */
static int file_lang;
static char *read_file(const char *p) {
    FILE *f = fopen(p, "rb"); if (!f) return NULL;
    char *b = malloc(1 << 20); size_t n = b ? fread(b, 1, (1 << 20) - 1, f) : 0; fclose(f);
    if (b) b[n] = 0;
    return b;
}
static int read_usersetting(void) {
    char c[3][1200]; int nc = 0;
    const char *e = getenv("DSML_DATA");
    if (e && *e) snprintf(c[nc++], sizeof c[0], "%s/UserSetting.json", e);
    if ((e = getenv("DSRIG_DIR")) && *e) snprintf(c[nc++], sizeof c[0], "%s/data/UserSetting.json", e);
    const char *dd = DSL_HAS(H, data_dir) ? H->data_dir() : NULL;
    if (dd && *dd) {
        char *p = c[nc]; snprintf(p, sizeof c[0], "%s", dd);
        size_t L = strlen(p);
        while (L && (p[L - 1] == '/' || p[L - 1] == '\\')) p[--L] = 0;
        while (L && p[L - 1] != '/' && p[L - 1] != '\\') p[--L] = 0;   /* drop "Mods": keep the separator */
        if (L) { snprintf(p + L, sizeof c[0] - L, "UserSetting.json"); nc++; }
    }
    for (int k = 0; k < nc; k++) {
        char *b = read_file(c[k]); if (!b) continue;
        int v = dsi_lang_from_usersetting(b);
        free(b);
        H->log(ME, "i18n: saved language %d from %s", v, c[k]);
        return v;
    }
    H->log(ME, "i18n: no UserSetting.json found: English until the game's language is known");
    return 0;
}

/* Refactor.Setting.LanguageSetting.GetCurrentLanguage(): static int32, no arguments (a static read in the game) */
typedef int32_t (*lang_fn)(const void *method);
static lang_fn get_lang; static int resolved, settled; static unsigned polls;
static void resolve(void) {
    resolved = 1;
    void *k = H->find_class ? H->find_class("Refactor.Setting", "LanguageSetting") : NULL;
    void *mi = k && H->find_method ? H->find_method(k, "GetCurrentLanguage", 0) : NULL;
    void *fn = mi && H->method_address ? H->method_address(mi) : NULL;
    if (!fn) { H->log(ME, "i18n: LanguageSetting.GetCurrentLanguage not found: the language stays %s", dsi_code()); return; }
    void (*cinit)(void *) = H->il2cpp_fn ? (void (*)(void *))H->il2cpp_fn("il2cpp_runtime_class_init") : NULL;
    if (cinit) cinit(k);                                    /* its static fields exist before the first read */
    get_lang = (lang_fn)fn;
}
static void apply(int v, const char *from) {
    if (!dsi_set_lang(v)) return;
    H->log(ME, "i18n: language %s (from %s): %d texts, %d dropped", dsi_code(), from, dsi_loaded(), dsi_dropped());
    for (int i = 0; i < ncb; i++) cbs[i]();
}
static void lang_poll(void *ud) {
    (void)ud;
    if (!resolved) { if (!H->il2cpp_ready || !H->il2cpp_ready()) return; resolve(); }
    if (!get_lang) return;
    int v = get_lang(NULL);
    if (v < 0 || v >= DSI_LANGS) v = 0;
    /* while the game boots, the static is still 0 (English) until GeneralSetting loads the saved choice. Any other value
       is the loaded choice: follow it at once (UserSetting.json may be missing, or say another language than the game
       picked). A 0 against a saved other language waits for the title (loaded by then), ~30 s of frames at most. */
    if (!settled) { if (v == 0 && file_lang != 0 && ++polls < 1800 && !cmw_title_shown()) return; settled = 1; }
    apply(v, "the game");
}

#ifdef _WIN32
#include <windows.h>
/* Windows: the folder of this DLL in UTF-8 (ds_i18n.h opens with _wfopen; the loader's module_dir is an ANSI path), so
   both Custom Modes DLLs (cm_mp.c lang_dir does the same) find one mods/custom-modes/lang/ on any install path */
static const char *own_dir(char *out, size_t n) {
    HMODULE m = NULL; wchar_t w[600]; out[0] = 0;
    if (!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCSTR)(void *)own_dir, &m)) return NULL;
    DWORD k = GetModuleFileNameW(m, w, 600);
    if (!k || k >= 600 || WideCharToMultiByte(CP_UTF8, 0, w, -1, out, (int)n, NULL, NULL) <= 0) { out[0] = 0; return NULL; }
    char *sl = strrchr(out, '\\'); if (!sl) { out[0] = 0; return NULL; }
    *sl = 0;
    return out;
}
#endif

static char mod_dir[1200];
const char *cm_mod_dir(void) { return mod_dir; }
void cm_i18n_init(const DSLHost *host, DSLPlugin *self) {
    H = host; ME = self;
#ifdef _WIN32
    char own[1200]; const char *md = own_dir(own, sizeof own);
#else
    const char *md = host->api_version >= 2 && host->module_dir ? host->module_dir(self) : NULL;
#endif
    char d[1200] = "";
    if (md && *md) {
        snprintf(d, sizeof d, "%s", md);
        size_t L = strlen(d);
        while (L && (d[L - 1] == '/' || d[L - 1] == '\\')) d[--L] = 0;
        snprintf(mod_dir, sizeof mod_dir, "%s", d);
        snprintf(d + L, sizeof d - L, "/lang");
    }
    dsi_set_dir(d);
    file_lang = read_usersetting();
    apply(file_lang, "UserSetting.json");                   /* before the parts register their labels */
    if (cmw_init(host, self) || cmw_tick_add(lang_poll, NULL))
        host->log(self, "i18n: no frame tick: the language stays %s until the next start", dsi_code());
    host->log(self, "i18n: language files in %s", d[0] ? d : "(none)");
}
