/* cm_prefs - the Custom Modes settings the windows remember between sessions (owner v0.1.11: "choosing Hard stays Hard
 * the next time the window opens and after a restart"). One small JSON object of strings:
 *   <game data>/Mods/custom-modes/settings.json   { "format": "custom-modes-settings", "version": 1, "values": { key: "text" } }
 * Main thread. Read once (after the game's data folder is known), written at each change (temp + rename). */
#pragma once
#include <stddef.h>

int  cm_pref_get(const char *key, char *out, size_t n);   /* 0 = found (out filled), -1 = none */
void cm_pref_set(const char *key, const char *value);     /* NULL value = remove */
/* test / tool: read and write a given file (the game uses <game data>/Mods/custom-modes/settings.json) */
void cm_prefs_use_file(const char *path);
