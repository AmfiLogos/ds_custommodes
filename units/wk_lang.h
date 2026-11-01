/* wk_lang - the wiki's language prefix for a game language code (dsi_code()): ONE table for every wiki link the mod
 * makes (the Build Gallery words, units/gallery; the export link, units/wk_export.c). The wiki's mirrors are
 * dungeonsettlersdb pipeline/gen/common.py SITE_LANGS. "" = the English root, or a language the wiki does not have. */
#pragma once
#include <string.h>
static inline const char *wk_wiki_lang(const char *game_lang) {
    static const struct { const char *game, *wiki; } L[] = {
        { "fr", "fr" }, { "de", "de" }, { "ja", "ja" }, { "ko", "ko" }, { "ru", "ru" }, { "zh-Hans", "zh" }, { "zh-Hant", "zht" } };
    for (size_t i = 0; game_lang && i < sizeof L / sizeof *L; i++) if (!strcmp(L[i].game, game_lang)) return L[i].wiki;
    return "";
}
