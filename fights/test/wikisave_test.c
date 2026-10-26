/* wikisave_test - the breaks this guards: a save the PLAYER made gets its Lock Consecration choice changed (it must never
 * be touched), or a save the wiki's generator made still opens the "Lock Consecration Results" box at load; the DPS Check
 * log and panel (tf_dps.c) switched on in another world (a boss fight, a player's save) or off in a DPS Check.
 * Names: real ones the generator writes (savegen.js finish(): team slug, fight, difficulty slug) and real player names.
 * Test worlds known forever (2026-10-26): once the title cleared the list, a test world was a normal save again: the
 * Load list showed it and the Steam Cloud backup uploaded it / another computer downloaded it (main: FAIL). Also guards
 * the other way: a player's world (a wiki download with the same kind of name) taken for a test world, so hidden. */
#include "../tf_worlds.h"
#include "../../fe/fe_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

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
    /* the DPS Check's panel and hit log (tf_dps.c) run only in a DPS Check world: on in the 2 DPS names, off in every other */
    for (size_t i = 0; i < sizeof wiki / sizeof *wiki; i++) CHECK(tfw_dps_made(wiki[i]) == (i < 2), "DPS Check world wrong for %s", wiki[i]);
    for (size_t i = 0; i < sizeof player / sizeof *player; i++) CHECK(!tfw_dps_made(player[i]), "player name taken for a DPS Check world: %s", player[i]);

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

    /* a Test Fights world: the generator's save (savegen.js finish(): random v4 guid in the header, the clan record and
       the name) through tfw_take as tfw_write does it, then the title clears the list (tf_worlds.c tick) */
    char d1[] = "/tmp/wikisave_test.XXXXXX", d2[] = "/tmp/wikisave_test.XXXXXX", d3[] = "/tmp/wikisave_test.XXXXXX", p[200];
    CHECK(mkdtemp(d1) && mkdtemp(d2) && mkdtemp(d3), "temp folders");
    tfw_reg_dir(d1);
    const char *gen = "{\"CampaignSaveHeader\":{\"CampaignGuid\":\"3c1f9a2e-5b7d-4e08-9a6c-0d2e4f6a8b1c\",\"SaveName\":"
                      "\"Elven-Expedition_DPS Check_Very-Hard__AutoSave_3c1f9a2e5b7d4e089a6c0d2e4f6a8b1c_0\"},"
                      "\"ClanSaveData\":{\"CampaignGuid\":\"3c1f9a2e-5b7d-4e08-9a6c-0d2e4f6a8b1c\"}}";
    char name[300] = "", g[40] = "", auto3[400];
    s = jparse(gen, strlen(gen), e, sizeof e);
    CHECK(s && tfw_take(s, name, sizeof name) == 0, "tfw_take refused the generator's save");
    char *hg = s ? jstrdup_raw(jpath(s, "CampaignSaveHeader.CampaignGuid")) : NULL, *cg = s ? jstrdup_raw(jpath(s, "ClanSaveData.CampaignGuid")) : NULL;
    snprintf(g, sizeof g, "%s", hg ? hg : ""); free(hg);
    CHECK(cg && !strcmp(cg, g), "clan record guid %s != header %s", cg ? cg : "-", g); free(cg);
    CHECK(tfw_guid_marked(g), "test world guid not marked: %s", g);
    char hex[33]; int k = 0; for (const char *c = g; *c && k < 32; c++) if (*c != '-') hex[k++] = *c; hex[k] = 0;
    CHECK(strstr(name, hex) && !strstr(name, "3c1f9a2e5b7d4e089a6c0d2e4f6a8b1c"), "SaveName not on the new guid: %s", name);
    jfree(s);
    snprintf(auto3, sizeof auto3, "Elven Expedition__AutoSave_%s_3", hex);          /* the game's own autosave in that world */
    CHECK(tfw_known(g, name) && tfw_known(NULL, auto3), "the new test world not hidden while listed");
    tfw_title_clear();
    CHECK(!tfw_listed(0), "the title left the list");
    CHECK(tfw_known(g, name), "back on the title: the test world shows in the Load list (guid + name)");
    CHECK(tfw_known(NULL, auto3), "back on the title: the game's autosave of the test world shows / goes to the cloud (name)");
    CHECK(tfw_known(g, "renamed copy"), "back on the title: a test world copy with another name shows (guid)");
    tfw_reg_dir(d2);                                     /* another computer: a cloud copy, no list, no record */
    CHECK(tfw_known(g, name) && tfw_known(NULL, auto3), "another computer: the test world from the cloud shows");

    /* a test world made before the mark: listed by the older version (test_worlds.json as it leaves it when the game was
       closed in a fight), then the title clears the list; known on this computer after a restart too */
    snprintf(p, sizeof p, "%s/test_worlds.json", d3);
    FILE *f = fopen(p, "wb"); if (f) { fputs("{\"format\":\"custom-modes-test-worlds\",\"version\":1,\"worlds\":[\"7cc6e44d0a1b4c2d8e3f405162738495\"]}\n", f); fclose(f); }
    const char *oldn = "Elven-Expedition_DPS Check_Very-Hard__AutoSave_7cc6e44d0a1b4c2d8e3f405162738495_0";
    tfw_reg_dir(d3);
    CHECK(tfw_known("7cc6e44d-0a1b-4c2d-8e3f-405162738495", oldn), "old test world not hidden while listed");
    tfw_title_clear();
    tfw_reg_dir(d1); tfw_reg_dir(d3);                    /* the game started again: read from disk */
    CHECK(!tfw_listed(0), "the title left the old list");
    CHECK(tfw_known("7cc6e44d-0a1b-4c2d-8e3f-405162738495", NULL) && tfw_known(NULL, oldn),
          "back on the title, restarted: the test world made before the mark shows (the record lost it)");

    /* never a player's world: a wiki download with a test world's kind of name, a player's world, a random guid that
       starts like the mark (version 4, as the game makes them) */
    static const char *const players[][2] = {
        { "9f2b6c1d-3e4a-4b5c-8d6e-7f8091a2b3c4", "Team-1_DPS Check_Very-Hard__AutoSave_9f2b6c1d3e4a4b5c8d6e7f8091a2b3c4_0" },
        { "f816ebc6-195c-432c-9fff-754b75ee3738", "ElvenExpedition__AutoSave_f816ebc6195c432c9fff754b75ee3738_0" },
        { "7e57f167-1a2b-4c3d-8e4f-5a6b7c8d9e0f", "MyClan__AutoSave_7e57f1671a2b4c3d8e4f5a6b7c8d9e0f_2" } };
    for (size_t i = 0; i < sizeof players / sizeof *players; i++)
        CHECK(!tfw_known(players[i][0], players[i][1]) && !tfw_known(NULL, players[i][1]), "a player's world taken for a test world: %s", players[i][1]);
    printf("wikisave_test: %s (%d failed)\n", fails ? "FAIL" : "PASS", fails);
    return fails != 0;
}
