/* mu_export - see mu_export.h */
#include "mu_export.h"
#include "wk_export.h"
#include "../fights/cm_http.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <pthread.h>
#endif

#define MUX_TIMEOUT_MS 8000
#define MUX_MAX_BYTES (4u << 20)

static int state = MUX_IDLE;                 /* the caller's thread only */
static char url[16384];
static wk_build jb[WK_MAX_UNITS];            /* the job's copy (the worker reads, the caller waits for w_done) */
static char jnames[WK_MAX_UNITS][96], jteam[200], jlang[16];
static int jn, jpub, w_dropped, w_fp;
static volatile int w_done, w_res;
static char w_detail[400];
static int w_thread;
#ifdef _WIN32
static HANDLE th;
static DWORD WINAPI work(LPVOID u) {
#else
static pthread_t th;
static void *work(void *u) {
#endif
    (void)u;
    cmh_resp r; memset(&r, 0, sizeof r); char e[300] = "";
    int res; wkx_live *x = NULL;
    const char *np[WK_MAX_UNITS]; for (int i = 0; i < WK_MAX_UNITS; i++) np[i] = jnames[i];
    url[0] = 0; w_dropped = 0; w_fp = -1;
    if (cmh_get(CM_WIKI WKX_CODEC_PATH, MUX_TIMEOUT_MS, MUX_MAX_BYTES, &r, e, sizeof e)) { res = MUX_OFFLINE; snprintf(w_detail, sizeof w_detail, "%s", e); }
    else if (r.status != 200) { res = MUX_OFFLINE; snprintf(w_detail, sizeof w_detail, "HTTP %d", r.status); }
    else if (!(x = wkx_live_load(r.body, r.n, w_detail, sizeof w_detail))) res = MUX_OUTDATED;
    else {
        /* the link is written against the tables the wiki serves NOW (re-mapped by key): a wiki data update does not
           stop the button; what the wiki no longer has is left out and counted */
        int rc = wkx_export_url(x, jb, jn, np, jteam, jpub, jlang, url, sizeof url, &w_dropped);
        w_fp = wkx_live_fp(x);
        if (rc == -2) { res = MUX_OUTDATED; snprintf(w_detail, sizeof w_detail, "a unit's race is no longer on the wiki"); url[0] = 0; }
        else if (rc) { res = MUX_NO_LINK; url[0] = 0; }
        else res = wk_open_url(url) ? MUX_NO_BROWSER : MUX_OPENED;
    }
    wkx_live_free(x);
    cmh_free(&r);
    w_res = res;
    __atomic_store_n(&w_done, 1, __ATOMIC_RELEASE);
    return 0;
}
int mux_start(const wk_build *b, int n, const char *const *names, const char *team_name, int pub, const char *lang) {
    if (state == MUX_BUSY) return state;
    if (!b || n < 1 || n > WK_MAX_UNITS) return state;
    memcpy(jb, b, sizeof *b * (size_t)n); jn = n; jpub = pub ? 1 : 0;
    for (int i = 0; i < WK_MAX_UNITS; i++) snprintf(jnames[i], sizeof jnames[i], "%s", i < n && names && names[i] ? names[i] : i < n ? b[i].name : "");
    snprintf(jteam, sizeof jteam, "%s", team_name ? team_name : "");
    snprintf(jlang, sizeof jlang, "%s", lang ? lang : "");
    w_done = 0; w_res = 0; w_detail[0] = 0;
#ifdef _WIN32
    th = CreateThread(NULL, 0, work, NULL, 0, NULL);
    int ok = th != NULL;
#else
    int ok = pthread_create(&th, NULL, work, NULL) == 0;
#endif
    w_thread = ok;
    state = MUX_BUSY;
    if (!ok) work(NULL);                                                 /* no thread: here (may wait up to 8 s) */
    return state;
}
const char *mux_url(void) { return state == MUX_BUSY ? "" : url; }
int mux_dropped(void) { return state == MUX_BUSY ? 0 : w_dropped; }
int mux_fp(void) { return state == MUX_BUSY ? -1 : w_fp; }
int mux_poll(char *detail, size_t n) {
    if (state != MUX_BUSY) return MUX_IDLE;
    if (!__atomic_load_n(&w_done, __ATOMIC_ACQUIRE)) return MUX_BUSY;
    if (w_thread) {
#ifdef _WIN32
        WaitForSingleObject(th, INFINITE); CloseHandle(th);
#else
        pthread_join(th, NULL);
#endif
    }
    if (detail && n) snprintf(detail, n, "%s", w_detail);
    state = MUX_IDLE;
    return w_res;
}
