/* mu_link - one-click "Add from wiki link" (see mu_link.h). */
#include "mu_link.h"
#include "myunits.h"
#include "wk_link.h"
#include "i18n/ds_i18n.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <pthread.h>
#endif

static int state = MUL_IDLE;           /* written by the caller's thread only */
static mul_result res;
static char text[16384];
static wk_build units[WK_MAX_UNITS];
static volatile int w_done, w_n, w_code;   /* worker -> caller, read after the join */
static char w_err[400];
static char w_gid[48], w_author[120];   /* a /build/<id> link: the gallery build and its builder (mul_gallery_author) */
static int w_thread;                    /* a worker thread to join */
int (*mul_gallery_author)(const char *gallery_id, char *out, size_t n);
/* the id of a /build/<id> short link in t ("" = none) */
static void gallery_id_of(const char *t, char *out, size_t n) {
    const char *p = strstr(t, "/build/"); size_t k = 0;
    if (n) out[0] = 0;
    if (!p) return;
    for (p += 7; *p && k + 1 < n; p++) {
        char c = *p;
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-')) break;
        out[k++] = c;
    }
    out[k] = 0;
}
#ifdef _WIN32
static HANDLE th;
static DWORD WINAPI work(LPVOID u) {
#else
static pthread_t th;
static void *work(void *u) {
#endif
    (void)u;
    int code = 0;
    int n = wkl_resolve(text, units, WK_MAX_UNITS, MUL_TIMEOUT_MS, &code, w_err, sizeof w_err);
    gallery_id_of(text, w_gid, sizeof w_gid);
    if (n > 0 && w_gid[0] && mul_gallery_author && !mul_gallery_author(w_gid, w_author, sizeof w_author)) w_author[0] = 0;
    w_n = n; w_code = code;
    __atomic_store_n(&w_done, 1, __ATOMIC_RELEASE);
    return 0;
}

static int start(const char *t) {
    if (state == MUL_BUSY) return state;
    memset(&res, 0, sizeof res);
    while (*t == ' ' || *t == '\t' || *t == '\r' || *t == '\n') t++;
    if (!*t || !wkl_looks_like_link(t)) {
        state = MUL_NEED_BOX; res.state = state;
        snprintf(res.msg, sizeof res.msg, "%s", *t ? TR("That is not a wiki builder link. Copy a team or unit link on the wiki builder, or paste it here.")
                                                   : TR("Copy a team or unit link on the wiki builder, or paste it here."));
        return state;
    }
    snprintf(text, sizeof text, "%s", t);
    w_done = 0; w_n = 0; w_code = 0; w_err[0] = 0; w_gid[0] = w_author[0] = 0;
#ifdef _WIN32
    th = CreateThread(NULL, 0, work, NULL, 0, NULL);
    int ok = th != NULL;
#else
    int ok = pthread_create(&th, NULL, work, NULL) == 0;
#endif
    w_thread = ok;
    if (!ok) work(NULL);                                                 /* no thread: read here (a short link may block) */
    state = MUL_BUSY; res.state = state;
    return state;
}

int mul_click(void) {
    if (state == MUL_BUSY) return state;
    static char clip[16384];
    if (mu_clip_text(clip, sizeof clip)) clip[0] = 0;
    return start(clip);
}
int mul_submit(const char *t) { return start(t ? t : ""); }

int mul_poll(mul_result *r) {
    if (state == MUL_BUSY && __atomic_load_n(&w_done, __ATOMIC_ACQUIRE)) {
        if (w_thread) {
#ifdef _WIN32
            WaitForSingleObject(th, INFINITE); CloseHandle(th);
#else
            pthread_join(th, NULL);
#endif
        }
        memset(&res, 0, sizeof res);
        if (w_n < 1) { state = MUL_FAILED; res.code = w_code; snprintf(res.msg, sizeof res.msg, "%s", w_err); }
        else {
            char e[400];
            jv *lib = mu_load(e, sizeof e);
            if (!lib) { state = MUL_FAILED; snprintf(res.msg, sizeof res.msg, "%s", e); }
            else if (mu_add_team(lib, units, w_n, NULL, res.ids, e, sizeof e) < 0
                     || (w_author[0] && mu_team_set_source(lib, mu_team_count(lib) - 1, w_author, w_gid), 0)
                     || mu_save(lib, e, sizeof e)) {
                state = MUL_FAILED; snprintf(res.msg, sizeof res.msg, TR("Could not add to My Units: %s"), e);
            } else {
                state = MUL_DONE; res.n = w_n;
                size_t nt = mu_team_count(lib); char nm[200] = "", ids[MU_TEAM_MAX][17];
                if (nt) { mu_team_id_at(lib, nt - 1, res.team_id); mu_team_at(lib, nt - 1, nm, sizeof nm, ids); }
                if (w_n == 1) snprintf(res.msg, sizeof res.msg, TR("Added 1 unit to My Units: %s"), nm);
                else snprintf(res.msg, sizeof res.msg, TRN("Added %d unit to My Units as the team %s", "Added %d units to My Units as the team %s", w_n), w_n, nm);
            }
            jfree(lib);
        }
        res.state = state;
    }
    if (r) { *r = res; r->state = state; }
    return state;
}
int mul_reset(void) {
    if (state == MUL_BUSY) return state;
    state = MUL_IDLE; memset(&res, 0, sizeof res);
    return state;
}
