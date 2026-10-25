/* tf_meter - the DPS meter of a DPS Check world (owner 2026-10-25, ask 4, and the "Restart test" button of ask 6).
 * A panel on the left of the screen, in the lab's flat style, shown while tf_dps_active() is 1 (a DPS Check world is
 * loaded and shown). Its title bar folds it (remembered: cm_pref "dps_meter_folded").
 *   - Start / Stop / Reset, and "Start on first hit" (tf_dps_switch TFD_SW_FIRSTHIT, default on).
 *     Start: the timer runs from now (tf_dps_clock). Stop: the numbers freeze; hits after Stop are not counted; Start
 *     after Stop goes on with the same measurement. Reset: back to zero and stopped; with "Start on first hit" on, the
 *     next hit starts the timer at that hit's time (also at the start of every fight).
 *   - The measurement is a set of spans of the hit list (tf_dps_hits): every hit of a span is counted once, in index
 *     order, into the squad's total, its unit's sum and its skill's sum (double sums of the hits' float damage). Misses
 *     and immune hits count as hits with 0 damage.
 *   - Lines: time measured (m:ss), damage dealt, squad DPS over the measured time, squad DPS over the last 5 game
 *     seconds, and on its own line what the scarecrow repaired while measured (tf_dps_repaired differences).
 *     DPS = damage / max(1 s, time); the last 5 s = the counted hits of the last min(5, time) measured seconds, divided
 *     by max(1 s, that window).
 *   - One row per unit with counted hits, by damage: portrait, name, damage, DPS, share, a thin bar. A click opens the
 *     unit's damage per skill (skill name as stored in the hit, damage, DPS, hits); a second click closes it.
 *   - The switches "Scarecrow speech", "Scarecrow heal numbers", "DPS log in chat" (tf_dps_switch_set), and "Restart
 *     test" (tf_restart, enabled while tf_restart_ready). In a test world that is not a DPS Check, only a small panel
 *     with "Restart test".
 * Placement: never over the game's resource list (top left: Refactor.UI.PlayerOwnedPanelView, its ScrollRect) nor the
 * unit panel (bottom left: Refactor.UI.EntitySummaryUIView) nor the multiplayer chat's idle lines (x >= 130, the lowest
 * 400 units of the kit's 1080-high layer), found at run time every 30 frames (fixed numbers when not found). The panel
 * goes under the resource list, or beside it when that leaves more room; it is drawn at the game's UI scale, smaller
 * when it would not fit (never under 0.5); the unit list scrolls when its rows do not fit.
 * Texts are written at most every 10 frames and only when they changed; widgets are made once and kept.
 * Test names (cm_win registry): dps:fold, dps:start, dps:stop, dps:reset, dps:firsthit, dps:speech, dps:healnum,
 * dps:chatlog, dps:restart (the shown panel's), dps:unit<i> (the row of tf_dps_unit(i): a click opens / closes its
 * skills), texts dps:status, dps:time, dps:total, dps:dps, dps:dps5, dps:repaired. */
#pragma once
#include <stddef.h>

int tf_meter_init(void);                 /* once, after cmw_init: registers the frame tick. 0 ok */

/* test (exported): "state" -> JSON {shown, small, folded, running, armed, elapsed, total, dps, dps5, repaired, base,
 * hits, spans:[[from,to],...], units:[{i, name, dmg, hits, dps, share, skills:[{skill, dmg, hits}]}], rect:[screen x0,
 * y0, x1, y1], game_rects:{resources, unitpanel}, place:{...}}; "counted [from]" -> JSON [[hit index, damage], ...] of
 * the counted hits (from the from-th counted hit). Screen rects: pixels, origin bottom left (dsl_ui_test_rect). 0 ok */
int tf_meter_test(const char *s, char *out, size_t cap);
