/* wk_export - "Open in Wiki Builder" (owner 2026-11-01, PLAN_v013 item 16: "export a team to the wiki ... to actually be
 * able to modify them, edit them on the wiki and so that they become visible in the build gallery"; exported builds
 * publish at once to the Build Gallery "just like for other builds", with a toggle to make them private).
 * The ENCODER of the wiki builder's team code (the inverse of pvp_wiki.c wk_decode; builder.js encodeTeam / dsbEncBuild:
 * blob = ver, fp hi, fp lo, unit count, units as LEB128; written as v1 = the raw blob, which every builder reads), the
 * export link, and the system browser. Pure C (+ the platform open in wk_open_url).
 * Link contract (the wiki side, dungeonsettlersdb builder boot, "from=game"; ext/nvexport/PLAN.md):
 *   https://dungeonsettlers.wiki[/<lang>]/builder?team=<code>&from=game&pub=<1|0>&tn=<team name>&gx=<16 hex>
 * The builder imports the team into the reader's roster as one new team, sets its Build Gallery switch from pub and
 * publishes it at once when public; the same gx again (same units) only opens that team and applies pub.
 * Tests: units/test/mu_test.c (round trip), units/test/save/run.sh "export" (C code -> builder decode == the builder's
 * own save import), units/test/export_test.py (in game). */
#pragma once
#include <stddef.h>
#include "pvp_wiki.h"
#include "wk_lang.h"   /* wk_wiki_lang(): the export link's language prefix */

#define WKX_HOST "https://dungeonsettlers.wiki"

/* the team code of n builds (1..WK_MAX_UNITS), as builder.js encodeTeam writes it. 0 ok, -1 (bad input / no room) */
int wk_encode(const wk_build *b, int n, char *out, size_t outn);
/* the same with each unit's name as raw UTF-8 text (names[i] NULL = b[i].name): a wk_build name has no '"' or '\\' (they
   became spaces), the game's own name keeps them, as the builder does. names NULL = all from the builds */
int wk_encode_named(const wk_build *b, int n, const char *const *names, char *out, size_t outn);
/* the export link (contract above). lang = the wiki prefix ("fr", "ko"...; NULL / "" / "en" = the English root);
   names as wk_encode_named; team_name NULL / "" = none. 0 ok, -1 no room / bad builds */
int wk_export_url(const wk_build *b, int n, const char *const *names, const char *team_name, int pub, const char *lang, char *out, size_t outn);
/* the builder's own tables as the wiki serves them now (WKX_CODEC_PATH: the "var D = {...}" lists builder.js dsbIdx
   indexes) compared with ours, key for key (+ portraits per race, item bonus ranges). 0 = the same: a link made here loads
   exactly these units on the wiki; -1 + err (the first difference) = the wiki's data changed since this version was built */
#define WKX_CODEC_PATH "/dbdata/gallery_codec.js"
int wkx_codec_check(const char *text, size_t n, char *err, size_t errn);
/* opens url in the system browser (Mac /usr/bin/open, Windows ShellExecuteW; never waits on the browser). Only https
   links of dungeonsettlers.wiki are opened. 0 started, -1 not */
int wk_open_url(const char *url);
