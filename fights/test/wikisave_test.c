/* wikisave_test - the breaks this guards: a save the PLAYER made gets its Lock Consecration choice changed (it must never
 * be touched), or a save the wiki's generator made still opens the "Lock Consecration Results" box at load.
 * Names: real ones the generator writes (savegen.js finish(): team slug, fight, difficulty slug) and real player names. */
#include "../tf_worlds.h"
#include "../../fe/fe_json.h"
#include <stdio.h>
#include <string.h>

static int fails;
#define CHECK(c, ...) do { if (!(c)) { fails++; printf("FAIL "); printf(__VA_ARGS__); printf("\n"); } } while (0)
#define G "_AutoSave_0123456789abcdef0123456789abcdef"

int main(void) {
    static const char *wiki[] = {
        "DPS Check_Normal_" G "_0", "Team-1_DPS Check_Devastation_" G "_0", "BossRush-GiantStoneGolem_Very-Hard_" G "_0",
        "Shield-Greatsword-Fellowship_BossRush-ChainKnightSentinel_Hard_" G "_0", "Arena-3xAcidSlogel-2xBlueGoblin_Easy_" G "_0",
        "FinishedBase_Normal_" G "_0", "MyClan_DSBuilder+2units_AsIs_" G "_0", "DSBuilder_1units-Li DPSCheck Downfall_" G "_0" };
    static const char *player[] = {
        "ElvenExpedition_" G "_0", "ElvenExpedition_" G "_12", "Test Squad_" G "_3", "Shield & Greatsword Fellowship_" G "_1",
        "BossRush_" G "_0", "My manual save", "Arena_" G "_0", "DPS Check", "BossRush-GiantStoneGolem_Hard_AutoSave_0123_0",
        "Team_BossRush-Golem_Hard_" G "_0x", "" };
    for (size_t i = 0; i < sizeof wiki / sizeof *wiki; i++) CHECK(tfw_wiki_made(wiki[i]), "wiki name not known: %s", wiki[i]);
    for (size_t i = 0; i < sizeof player / sizeof *player; i++) CHECK(!tfw_wiki_made(player[i]), "player name taken for a wiki save: %s", player[i]);
    CHECK(!tfw_wiki_made(NULL), "NULL name");

    /* the generator's templates: both fields false (the box shows) -> off + acknowledged, once */
    const char *tpl = "{\"CampaignSaveHeader\":{},\"CampaignSettingSaveData\":{\"CampaignStartSettingSaveData\":{\"IronModeEnabled\":false,"
                      "\"LockConsecrationEnabled\":false,\"LockConsecrationNoticeAcknowledged\":false}}}";
    char e[200]; jv *s = jparse(tpl, strlen(tpl), e, sizeof e);
    CHECK(s && tfw_consecration_fix(s) == 1, "template not fixed");
    jv *css = jpath(s, "CampaignSettingSaveData.CampaignStartSettingSaveData");
    CHECK(jis(jget(css, "LockConsecrationEnabled"), JN_FALSE) && jis(jget(css, "LockConsecrationNoticeAcknowledged"), JN_TRUE), "template fields");
    CHECK(tfw_consecration_fix(s) == 0, "a fixed save changed again");
    jfree(s);
    /* an older template without the fields: added as the stock default */
    const char *old = "{\"CampaignSettingSaveData\":{\"CampaignStartSettingSaveData\":{\"IronModeEnabled\":false}}}";
    s = jparse(old, strlen(old), e, sizeof e); css = jpath(s, "CampaignSettingSaveData.CampaignStartSettingSaveData");
    CHECK(tfw_consecration_fix(s) == 1 && jis(jget(css, "LockConsecrationEnabled"), JN_FALSE) && jis(jget(css, "LockConsecrationNoticeAcknowledged"), JN_TRUE), "old template");
    jfree(s);
    /* a builder save made from the player's own world (DSBuilder+) keeps the player's own choice (lock on) */
    const char *mine = "{\"CampaignSettingSaveData\":{\"CampaignStartSettingSaveData\":{\"LockConsecrationEnabled\":true,\"LockConsecrationNoticeAcknowledged\":false}}}";
    s = jparse(mine, strlen(mine), e, sizeof e); css = jpath(s, "CampaignSettingSaveData.CampaignStartSettingSaveData");
    tfw_consecration_fix(s);
    CHECK(jis(jget(css, "LockConsecrationEnabled"), JN_TRUE), "the player's lock choice was changed");
    jfree(s);
    printf("wikisave_test: %s (%d failed)\n", fails ? "FAIL" : "PASS", fails);
    return fails != 0;
}
