/* mu_export - the "Open in Wiki Builder" button's work off the game's thread (owner 2026-11-01, PLAN_v013 item 16):
 * one GET of the wiki's builder tables (wk_export.h WKX_CODEC_PATH, cm_http: dungeonsettlers.wiki only, 8 s at most),
 * wkx_codec_check against this version's tables, and only when they are the same the browser opens the link (wk_open_url).
 * A link built on other tables would load other items / skills on the wiki, so it never goes out. One job at a time; the
 * caller polls each frame (mux_poll) and shows the message. */
#pragma once
#include <stddef.h>

enum { MUX_IDLE = 0, MUX_BUSY, MUX_OPENED, MUX_OUTDATED, MUX_OFFLINE, MUX_NO_BROWSER };

/* starts the job for this link (copied). MUX_BUSY, or the current state when a job still runs */
int mux_start(const char *url);
/* every frame: BUSY while the job runs; once, when it ends: OPENED / OUTDATED / OFFLINE / NO_BROWSER with detail (the
   check's first difference, the network error); IDLE otherwise */
int mux_poll(char *detail, size_t n);
