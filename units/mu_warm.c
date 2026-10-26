/* mu_warm - the icons of the wiki data loaded a few per tick while the Units Laboratory page is open, before a window
 * needs them. Foctopus 2026-10-20: "selecting anything in the wiki builder and character builder window freezes the game
 * with a stutter". Each icon a window shows for the first time is a synchronous ResourceLoader load (the frame waits on
 * the asset), and a section of the Unit Builder or a Gallery page shows dozens at once. Here the ~470 icon keys of the
 * wiki data (races, backgrounds, traits, inscriptions, soul stones, acquired traits, trees, skills, items) go into
 * cm_icon's cache at most ~3 ms of loading per 50 ms tick; a key already cached costs nothing. Main thread only.
 * At the title only (frame-time audit 2026-10-26, mp.md #6: it went on in a world, ~3 ms every 50 ms): in a world or while
 * a scene loads it waits, and goes on when the title shows again. */
#include "pvp_wiki_tables.h"
#include "cm_il.h"
#include "cm_win.h"
#include "mu_ui.h"
#include <string.h>
#ifdef _WIN32
#include <windows.h>
static double now_ms(void) { LARGE_INTEGER f, c; QueryPerformanceFrequency(&f); QueryPerformanceCounter(&c); return (double)c.QuadPart * 1000.0 / (double)f.QuadPart; }
#else
#include <time.h>
static double now_ms(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec * 1000.0 + t.tv_nsec / 1e6; }
#endif

#define BUDGET_MS 3.0
static const struct { int kind; const char *const *keys; int n; } SETS[] = {
    { CM_ICON_AFFECTER, WK_RACE, WK_RACE_N }, { CM_ICON_AFFECTER, WK_BG, WK_BG_N }, { CM_ICON_AFFECTER, WK_IND, WK_IND_N },
    { CM_ICON_SKILL, WK_SKILL, WK_SKILL_N }, { CM_ICON_TREE, WK_TREE, WK_TREE_N }, { CM_ICON_ITEM, WK_ITEM, WK_ITEM_N },
    { CM_ICON_AFFECTER, WK_INSC, WK_INSC_N }, { CM_ICON_AFFECTER, WK_SOUL, WK_SOUL_N }, { CM_ICON_AFFECTER, WK_ACQ, WK_ACQ_N },
};
#define NSETS ((int)(sizeof SETS / sizeof *SETS))
static int set_i, key_i, done, on, loads;
static double spent, t_start;

static void warm_tick(void) {
    if (done || !cmw_title_shown() || !cm_icon_ready()) return;         /* a world / a load; the game's loader not up yet */
    double t0 = now_ms();
    if (!t_start) t_start = t0;
    while (set_i < NSETS) {
        if (key_i >= SETS[set_i].n) { set_i++; key_i = 0; continue; }
        int kind = SETS[set_i].kind; const char *k = SETS[set_i].keys[key_i++];
        if (cm_icon_cached(kind, k)) continue;
        cm_icon(kind, k); loads++;
        if (now_ms() - t0 >= BUDGET_MS) { spent += now_ms() - t0; return; }
    }
    spent += now_ms() - t0;
    done = 1;
    mu_log("icons: %d loaded ahead in %.0f ms of ticks over %.1f s", loads, spent, (now_ms() - t_start) / 1000.0);
    mu_tick_remove(warm_tick);
}
void mu_warm_start(void) {
    if (on || done) return;
    on = !mu_tick_add(warm_tick);
}
