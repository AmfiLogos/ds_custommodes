/* tf_wikisave.c - wiki-made saves (tf_worlds.h): the name test and the Lock Consecration fields. Pure C (fe_json only),
 * so fights/test/wikisave_test.c checks it without the game. */
#include "tf_worlds.h"
#include "../fe/fe_json.h"
#include <ctype.h>
#include <string.h>

static int has_tok(const char *s, size_t n, const char *tok) {
    size_t l = strlen(tok);
    for (size_t i = 0; i + l <= n; i++) if (!memcmp(s + i, tok, l)) return 1;
    return 0;
}
int tfw_wiki_made(const char *name) {
    if (!name) return 0;
    const char *a = strstr(name, "__AutoSave_");
    if (!a || a == name) return 0;
    const char *h = a + 11; int k = 0;
    while (k < 32 && isxdigit((unsigned char)h[k])) k++;
    if (k != 32 || h[32] != '_' || !isdigit((unsigned char)h[33])) return 0;
    for (const char *d = h + 33; *d; d++) if (!isdigit((unsigned char)*d)) return 0;
    size_t n = (size_t)(a - name);                                   /* the stem: what the generator put before it */
    if (!strncmp(name, "BossRush-", 9) || !strncmp(name, "Arena-", 6) || !strncmp(name, "DPS Check_", 10) || !strncmp(name, "FinishedBase_", 13)) return 1;
    return has_tok(name, n, "_BossRush-") || has_tok(name, n, "_Arena-") || has_tok(name, n, "_DPS Check_") ||
           has_tok(name, n, "_FinishedBase_") || has_tok(name, n, "DSBuilder+") || has_tok(name, n, "DSBuilder_");
}
int tfw_consecration_fix(jv *save) {
    jv *css = jpath(save, "CampaignSettingSaveData.CampaignStartSettingSaveData");
    if (!css || css->t != JN_OBJ) return 0;
    int ch = 0;
    if (!jget(css, "LockConsecrationEnabled")) { jset(css, "LockConsecrationEnabled", jbool(0)); ch = 1; }   /* absent: the stock default */
    if (!jis(jget(css, "LockConsecrationNoticeAcknowledged"), JN_TRUE)) { jset(css, "LockConsecrationNoticeAcknowledged", jbool(1)); ch = 1; }
    return ch;
}
