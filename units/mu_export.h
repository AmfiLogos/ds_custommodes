/* mu_export - the "Open in Wiki Builder" button's work off the game's thread (owner 2026-11-01, PLAN_v013 item 16):
 * one GET of the wiki's builder tables (wk_export.h WKX_CODEC_PATH, cm_http: dungeonsettlers.wiki only, 8 s at most),
 * the link written against THOSE tables (wkx_export_url: every index re-mapped by key, so a wiki data update does not
 * stop the button; lead task 9), then the browser (wk_open_url). OUTDATED only when the wiki's link format itself changed
 * or a unit's race is gone. One job at a time; the caller polls each frame (mux_poll) and shows the message. */
#pragma once
#include <stddef.h>
#include "pvp_wiki.h"

enum { MUX_IDLE = 0, MUX_BUSY, MUX_OPENED, MUX_OUTDATED, MUX_OFFLINE, MUX_NO_BROWSER, MUX_NO_LINK };

/* starts the job for these n builds (1..WK_MAX_UNITS, our table indexes; names / team name / language as wk_export_url;
   all copied). MUX_BUSY, or the current state when a job still runs / the input is bad */
int mux_start(const wk_build *b, int n, const char *const *names, const char *team_name, int pub, const char *lang);
/* every frame: BUSY while the job runs; once, when it ends: OPENED / OUTDATED / OFFLINE / NO_BROWSER with detail (the
   check's first difference, the network error); IDLE otherwise */
int mux_poll(char *detail, size_t n);
/* after the job's end: the link it opened (or tried; "" = none), how many parts of the units the wiki no longer has (left
   out), the wiki's data fingerprint (-1 = not read) */
const char *mux_url(void);
int mux_dropped(void);
int mux_fp(void);
