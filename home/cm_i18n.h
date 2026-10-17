// cm_i18n.h - Custom Modes solo part (custom-modes.dylib): the string tables (i18n/ds_i18n.h: TR / TRN) and the game's
// language. cm_module.c calls cm_i18n_init first in dsl_plugin_init: the language files are <module dir>/lang/<code>.json,
// the first language is the one UserSetting.json holds, then the game's LanguageSetting.GetCurrentLanguage is read once
// per frame (cm_win.c frame tick). The language can change only on the title screen (Options): every part that drew text
// registers a cm_i18n_on_change callback that draws it again.
#pragma once
#include "dsl_sdk.h"
#include "i18n/ds_i18n.h"

#ifdef __cplusplus
extern "C" {
#endif

void cm_i18n_init(const DSLHost *host, DSLPlugin *self);
/* fn runs on the main thread between frames, after the language changed (TR already returns the new text) */
int  cm_i18n_on_change(void (*fn)(void));
/* the loader copies title button labels and tab titles once; 1 = it takes a second registration as an update
   (loader API 4+), so a part may register them again after a change */
int  cm_i18n_loader_relabels(void);

#ifdef __cplusplus
}
#endif
