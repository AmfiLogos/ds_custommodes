/* bld_test - test hooks for the in-game Unit Builder test (ext/builder13/test/builder_test.py), called by the test driver's
 * "call" command (ext/units13/driver: dlsym in custom-modes). Nothing calls them in play.
 *   bld_test_open(id or "")      the Unit Builder, new unit or Edit (a saved() callback records the id)
 *   bld_test_saved               the id the last saved() callback got ("" none)
 *   bld_test_list(kind)          keys the window lists now ("key,on,locked;...")
 *   bld_test_act(json)           one click by its action JSON (the level row) */
#include "bld.h"
#include <stdio.h>
#include <string.h>

#define EXPORT __attribute__((visibility("default")))
static char saved_id[17];
static void on_saved(const char *id, void *ud) { (void)ud; snprintf(saved_id, sizeof saved_id, "%s", id ? id : ""); }
EXPORT int bld_test_open(const char *s, char *out, size_t n) {
    saved_id[0] = 0; cm_builder_open(s && *s ? s : NULL, on_saved, NULL);
    snprintf(out, n, "%d", bld_is_open()); return bld_is_open() ? 0 : -1;
}
EXPORT int bld_test_saved(const char *s, char *out, size_t n) { (void)s; snprintf(out, n, "%s", saved_id); return saved_id[0] ? 0 : 1; }
int bld_test_keys(const char *kind, char *out, size_t n);
int bld_test_act_json(const char *json);
EXPORT int bld_test_list(const char *s, char *out, size_t n) { return bld_test_keys(s, out, n); }
EXPORT int bld_test_act(const char *s, char *out, size_t n) { int rc = bld_test_act_json(s); snprintf(out, n, "%d", rc); return rc; }
