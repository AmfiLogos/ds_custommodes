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
    int res;
    if (cmh_get(CM_WIKI WKX_CODEC_PATH, MUX_TIMEOUT_MS, MUX_MAX_BYTES, &r, e, sizeof e)) { res = MUX_OFFLINE; snprintf(w_detail, sizeof w_detail, "%s", e); }
    else if (r.status != 200) { res = MUX_OFFLINE; snprintf(w_detail, sizeof w_detail, "HTTP %d", r.status); }
    else if (wkx_codec_check(r.body, r.n, w_detail, sizeof w_detail)) res = MUX_OUTDATED;
    else res = wk_open_url(url) ? MUX_NO_BROWSER : MUX_OPENED;
    cmh_free(&r);
    w_res = res;
    __atomic_store_n(&w_done, 1, __ATOMIC_RELEASE);
    return 0;
}
int mux_start(const char *u) {
    if (state == MUX_BUSY) return state;
    snprintf(url, sizeof url, "%s", u ? u : "");
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
