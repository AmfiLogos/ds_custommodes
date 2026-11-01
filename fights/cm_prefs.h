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
/* several sets written as one file write (the Test Fights window saves all its choices at each change):
   cm_prefs_batch(1) ... cm_prefs_batch(0) */
void cm_prefs_batch(int on);
/* free text in a value: encode before cm_pref_set, decode after cm_pref_get ('%XX' for '%', quote, backslash, ',', '|',
   '/' and control characters) */
void cm_pref_enc(const char *s, char *out, size_t n);
void cm_pref_dec(const char *s, char *out, size_t n);
/* a bigger kept value (a Gallery build) in its own file next to settings.json; get = malloc'd text or NULL */
int   cm_pref_blob_set(const char *name, const char *data);
char *cm_pref_blob_get(const char *name);
