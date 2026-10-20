// cm_module.c - Units Laboratory as a loader module (custom-modes.dylib / .dll, mod id "custom-modes", scope client): a
// launcher mod with My Units, Unit Builder, Wiki Build Gallery and Test Fights. Since the split (owner 2026-10-18 /
// 2026-10-20) Foreign Expedition and PvP are part of the multiplayer mod: a multiplayer session never needs this module,
// and nothing of it touches a multiplayer world. The only link left: with multiplayer running, My Units' test commands
// ("mu_", units/mu_init.c) answer on multiplayer's test control socket (tests only; no game hook, no shared state).
// dsl_plugin_init (il2cpp up, widget kit there): the title home (home/cm_home.c, PLAN_V13 R4) and the parts, each
// through a weak entry point that exists only when its folder is in the build (custom/<folder>/sources.txt).
#include "dsl_sdk.h"
#include "mp_api.h"
#include "cm_home.h"
#include "cm_i18n.h"
#include <stdio.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#define CM_MP_IMAGE "multiplayer.dll"
static void *cm_loaded(const char *name) { return GetModuleHandleA(name); }
static void *cm_sym(void *h, const char *n) { return (void *)GetProcAddress((HMODULE)h, n); }
#else
#include <dlfcn.h>
#define CM_MP_IMAGE "@rpath/multiplayer.dylib"           /* the loaded multiplayer image, by install name */
static void *cm_loaded(const char *name) { return dlopen(name, RTLD_NOLOAD | RTLD_LOCAL); }
static void *cm_sym(void *h, const char *n) { return dlsym(h, n); }
#endif

DSL_PLUGIN();

/* solo parts (PLAN_V13 section 4): weak defaults here, the real entry point (in its folder's sources.txt) replaces it at
   link time. A default only logs "not in this build" (that lane is not merged yet). Weak definitions, not weak
   references: the Mac linker and the Windows (llvm-mingw) linker both take them without extra flags. */
typedef void cm_init_fn(const DSLHost *, DSLPlugin *);
#define CM_PART_DEFAULT(fn, what) \
    __attribute__((weak)) void fn(const DSLHost *h, DSLPlugin *s) { h->log(s, "%s: not in this build", what); }
CM_PART_DEFAULT(cm_units_init, "My Units")
CM_PART_DEFAULT(cm_builder_init, "Unit Builder")
CM_PART_DEFAULT(cm_gallery_init, "Gallery")
CM_PART_DEFAULT(cm_fights_init, "Test Fights")
/* My Units' rig commands ("mu_", units/mu_init.c) through multiplayer's test socket, when multiplayer runs */
__attribute__((weak)) char *mu_rig_exec(const char *cmd, const char *line) { (void)cmd; (void)line; return NULL; }

DSL_EXPORT int dsl_module_load(const DSLHost *host, DSLPlugin *self) {
    host->log(self, "Custom Modes loaded");
    return 0;
}

DSL_EXPORT int dsl_module_early(const DSLHost *host, DSLPlugin *self) {
    if (!host->module_loaded || !host->module_loaded("multiplayer")) return 0;
    void *mp = cm_loaded(CM_MP_IMAGE);                   /* test control only: My Units' "mu_" commands on its socket */
    mp_api_get_fn get = mp ? (mp_api_get_fn)cm_sym(mp, "mp_api_get") : NULL;
    const mp_api_table *api = get ? get(1) : NULL;
    if (api && api->register_rig_cmd) api->register_rig_cmd("mu_", mu_rig_exec);
    (void)self;
    return 0;
}

DSL_EXPORT int dsl_plugin_init(const DSLHost *host, DSLPlugin *self) {
    cm_i18n_init(host, self);                            /* the language first: the parts register translated labels */
    cm_home_init(host, self);
    cm_init_fn *parts[] = { cm_units_init, cm_builder_init, cm_gallery_init, cm_fights_init };
    for (size_t i = 0; i < sizeof parts / sizeof *parts; i++) parts[i](host, self);
    return 0;
}
