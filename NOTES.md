# Notes

## Where this repo comes from (2026-10-16)
- History = `git subtree split -P custom` of ds_multiplayer main 1c8f1af (40 commits, last 8a7c362). Tag `v13` = 8a7c362:
  its files are the same as ds_multiplayer v13 (29f0fa6) custom/ (tree 1f55b21).
- Scan of every commit (contents, messages, authors) for the owner's user name and home-folder paths: 0 hits. No binaries, no keys.
  Nothing was rewritten.
- 9019e18 = ds_multiplayer mpfix 30074cd (PvP: hook FactionRelationInfo.IsRelation too), applied without the custom/ prefix.

## How it is built
- The mod loader (github.com/AmfiLogos/ds_modloader) builds Custom Modes from the commit in its loader/cm_pin.txt:
  - Mac: loader/build.sh exports this repo at the pin into build/cmpin/<sha>/ and runs build_module.sh with
    MP_SRC = the multiplayer pin's src/ (loader/mp_pin.txt).
  - Windows: loader/modules/build_mods.sh puts this repo at the pin in place of the multiplayer pin tree's custom/, then
    runs that tree's win/build_win.sh (DSMP_TARGET=module).
- build.sh here = the same, standalone (MP_REPO, MP_PIN = mp_pin.txt, MP_DYLIB for the Mac link).

## Byte-identical proof (v13 pins: cm 8a7c362-equal tree, mp 29f0fa6)
- Mac: custom-modes.dylib ff1b4883..., custom-modes-mp.dylib 0e6c7b84...: the same bytes as the v13 release, from the loader
  build and from build.sh.
- Windows: no rebuild matches a release byte for byte: lld writes the build time into each DLL (PE TimeDateStamp), and
  custom-modes.dll / multiplayer.dll hold the build folder in quickjs __FILE__ strings. Built in the same folder, loader main
  and the cm-pin branch give the same 21 files once the time stamp is zeroed.

## Wiki Build Gallery: Compact Plus, List, Details (2026-10-17)
- Owner feedback: the cards looked bad, no Compact Plus view, no way to see a build before saving it.
- Views: Compact Plus (default, the wiki's Compact+: portrait + level, race / background / health / attack, worn items |
  trees | inscriptions | soul stones | acquired traits, then active | passive skills) and List (one line a unit). The
  choice is kept in settings.json (`gallery.view`). Click a card (or Details) = every unit in full with the game's hover
  cards (mu_icon -> mu_tip), Save to My Units and Test Fight from there.
- The list shows a page of cards (4 teams or 6 units in Compact Plus, 8 / 10 in List) with < page >; ">" on the last
  loaded page loads the next wiki page. Reason: the loader kit holds 1024 live widgets for every window and frees the
  children of a destroyed widget only at its next sweep (~1 s). A Compact Plus page is ~490 widgets, so gal_ui keeps each
  widget it makes and destroys them one by one before it draws again (and drops their mu_tip entries: mu_tip_forget).
- A one-line TMP text is cut ("Lizardman ...") when its line is taller than its box, even with room to the side: gal_ui's
  text boxes are at least 1.6 x the font size high.
- Names in the game's language: race, background, stats, talents, trees (TEXTKEY_*); build titles and pseudonyms from
  the wiki's words in that language (/<lang>/builds/words.json: fr de ja ko ru zh zht; es and pt-BR fall back to English),
  as the wiki's own gallery in that language.
- Data: list items now carry so (soul stones), aq (acquired traits) and x (extra stats); W.skills[k][2] splits active /
  passive. No live build has tags yet: the tag chips were checked with a local wiki folder (DSCM_WIKI_DIR).

## The game's text look (2026-10-24, 0.1.14)
- Owner: "Font inside of the units lab are not the same as the rest of the menus". Measured in a test copy (EN, DE, ES, JA):
  the font assets were already the game's (Lora / Cinzel, or the language's own faces), the MATERIAL and one size were not.
  The game's menus draw body text, buttons, check box labels and choice values with the body font's "OutLine16" material
  (FontMaterialType SoftOutline16: a soft dark outline, slightly bolder glyphs), section heads in Cinzel + OutLine16, and
  the title column's labels at 22. The loader kit gives its texts the font's plain material (the multiplayer windows' rule
  of 2026-10-13) and the Units Laboratory page buttons were kit buttons at 18.
- home/cm_look.c: the parts get a copy of the kit's table (cm_look_ui) whose text / button / toggle / value_row set the
  new widget's texts to LanguageSetting.GetFont + GetMaterial(style, SoftOutline16); window titles (26 and up) keep the
  title font's plain material (as the game's Options title), fields keep the kit's look (the game's name fields are plain).
  The page buttons get the stock column's 22 (cm_look_title_button); a label too long for 262 still shrinks (kit text fit).
- Test: home/test/look_test.py (one hidden copy; the game's own Options window and title column are the reference).


## My Units: team delete box, World units, snapshot (2026-10-25, 0.1.15)
- Owner: Delete team "Ask each time"; "My save / world units" so he can test things with his units; "save a snapshot of
  chosen units into regular my units, which can show time & save of snapshot". Design picks: PLAN_v013.md (ds_multiplayer,
  2026-10-25 lane dsunits).
- Delete team: a box inside the My Units window (the game's notice box has two buttons): Team and units / Team only /
  Cancel, with the number of units that go (myunits.c mu_team_split, mu_team_delete_units: a unit also in another team
  stays).
- units/mu_worlds.c: the worlds of the folders Load Campaign lists (DSML_GAME_SAVES = game saves, the modded game's Saves
  folder) in ONE list, no group headings (owner 2026-10-25, 0.1.16): one row per campaign guid, its newest file in either
  folder, newest world first; the CampaignSaveHeader is cut out of the file text and parsed alone.
  Our hub / PvP worlds, multiplayer temp saves and Test Fights worlds are never listed. A world's units are the
  Bring Units save reader's (mu_save.c), read only. Saves are only read.
- Snapshot: mu_snapshot -> a normal library unit + units[i].source (myunits.h mu_source); the card shows
  "<world> · save <time> · snapshot <time>" on a line under it.
- Tests: units/test mu_test (t_team_delete), units/test/save/run.sh (save_test worlds: newest save listed, filters,
  snapshot = the save's unit, save unchanged), units/test/worlds_test.py (one hidden copy, test library DSMP_UNITS_LIB,
  game saves = a folder with a copy of a real save; the owner's library checked unchanged).
- Teams on the cards (owner 2026-10-25, 0.1.17: "display the name of the team(s) the unit is currently in"): after the
  unit's name on its row, "Team: A" / "Teams: A, B"; the name keeps up to 55 % of the row; names that do not fit become
  "A, B +N" (no whole name fits: the first name cut with "..." and "+N" apart). Read from the library at every redraw, so
  Save as team, Delete team, Delete and Rename show it at once. A save's cards show none. 4 cards per page kept (in-card,
  no extra line). Test: worlds_test.py teams_check after each action (all pages, against the file).
- 0.1.18 (owner 2026-10-25): "there's only 3 units per line": the snapshot's source is now INSIDE the card, on the right of
  the race line (the race line keeps up to 60 %), longest form that fits: full line, "<world> · snapshot <time>",
  "<world> · <time>", "<world> · <MM-DD HH:MM>"; the full line is on its hover card ("Snapshot"). Slots are MU_CARD_H + 8
  again: 4 per page at UI scale 1 (a 4-unit team of snapshots on one page; worlds_test.py fails otherwise).
- "Rename team" (owner: "no way to rename teams"): a button next to "Delete team" (both only in a team's view), the same
  name box as the unit rename (field hint "Team name"), the team picked when the box opens (myunits.c mu_team_rename:
  the name only; units, author and gallery id stay). Bottom bar: the shown buttons at their label width from the left
  (layout_bottom), so the team buttons do not leave a gap in All units; pager 160 wide.

## Default unit names in the saves we write (2026-10-25, 0.1.19)
- Owner: "when importing units from my save, from the snapshot, they don't have their names saved": in a test fight the
  Multi-unit view cards (mod multi-bar) showed "TEXTKEY_PLAYERNAME_Elf_Male_13"; the top portrait bar showed "Silvarion".
- Cause: a save unit never renamed keeps the game's default name KEY (mu_save.c: build name = UnitNameTextKey). The game
  shows a unit's name through EntityDisplayNameExtensions.GetDisplayName -> GetPlayerUnitDisplayName (decomp 25846389):
  UnitNameTextKey is localized only when it equals DefaultUnitNameTextKey (or that one is empty); otherwise it is a typed
  name and shown as it is. A stock unit holds the same key in both fields (wiki base save). The wiki's savegen.js (Test
  Fights) and our pb_entry (Bring Units) wrote the race's "_0" key as DefaultUnitNameTextKey, so the key was shown raw
  wherever the game uses GetDisplayName, e.g. the stock unit panel (EntitySummaryUIView) and the multi-bar cards.
  The portrait bar reads the key itself (SubUI_UnitShortCutParts), hence "Silvarion" there.
- Fix: a name that starts with TEXTKEY_ goes in both fields: pvp_build.c pb_entry, and fights/sg_glue.js nameKeys() on
  the wiki's generated save (clan units only; a no-op once the wiki writes it). multi-bar needed no change (it already
  uses GetDisplayName). The library and saves are not changed: snapshot units already in My Units get their names.
- Test: units/test/namekey/namekey_test.py (one hidden copy, en + de; the Test Fights save from sg_cli and a pb_entry
  save; stock panel + multi-bar cards = TextKeyExtensions.GetText(key)). NK_OLD = an older tree: its saves fail.

## Test Fights: World units (2026-10-25, 0.1.20, lane dsfights)
- Owner: "Test fights should allow to select directly people from one of your current saves." A third source in the
  Test Fights squad picker, labelled "World units" (the same words as My Units' section; on a second row under My Units /
  Wiki Build Gallery: three labels never fit one row, ES "Galería de builds de la wiki").
- One World units list for both windows: units/mu_window.c `mu_player_worlds` (the folders Load Campaign lists, Test
  Fights' test worlds left out) and `mu_world_units` (a world's units, default names in the game's language), declared in
  mu_worlds.h. My Units and Test Fights call them; nothing is copied.
- The list: one row per world (clan name, save time, how many of its units are picked); a click opens it (a second click
  closes it): its newest save's units as Compact Plus cards (read only), each with a tick. A card narrower than
  MU_CARD_NARROW (360) leaves the in-game look out (the name and icon rows need the room; the portrait shows the face).
  A save that cannot be read shows the reader's own line (WARN colour); a unit that cannot be built shows a red line on
  its card; the other units still show.
- Picks: world guid + unit guid (+ the shown name), max = the fight's squad size. Picks of a world gone from the list are
  dropped when the window opens; picks of a unit its newest save no longer holds are dropped when that world is opened,
  and at Start such a pick stops Start with a line that names the unit.
- Mixing sources: the picker's existing rule is kept: the squad is the picks of the tab that is shown (My Units, Gallery
  or World units); the other tabs keep their picks but they are not sent.
- Start: each pick is built straight from its world's newest save by `mu_save_build_obj` (mu_worlds.c), the same call a
  snapshot makes (mu_snapshot now uses it): the save import's build + the save's own skill bar as the builder's `quick`
  pick, so the save generator writes the save's bar (builder.js quickFor keeps the skills the build has). The test world's
  clan is the world's clan name when all picks come from one world.
- Default names: a unit never renamed fights under its name key (TEXTKEY_PLAYERNAME_...); the generator wrote the
  template's DefaultUnitNameTextKey, so the game showed the key itself (found by the game test here and by lane dsnames).
  Fixed once, by lane dsnames (fights/sg_glue.js nameKeys: the key in both name fields); the game test checks the names.
- Limit found: the wiki generator's templates have 4 skill bar slots; a save unit with 5 bar skills fights with the first
  4 (the 5th stays learned). The generator is the wiki's (savegen.js quickSlotCount); not changed here.
- Tests: fights/test/run_saveunits.sh (saveunits_test.c: copies of real saves + the wiki base save, picks through
  mu_save_build_obj and the wiki generator in QuickJS, the test world read back: level, race, background, talents, worn
  items + rolls, learned skills, trees, inscriptions, soul stones, name and the save's bar; fails on a lost level or a
  computed bar, checked by mutation). fights/test/saveunits_game_test.py (one hidden copy: World units tab, open the save
  copy's world, tick 2, Custom fight with one Weak Grey Rat, Start, the squad = the picked units by name with their save
  levels, combat, the rat killed, back to the title, test world removed; LANGTYPE = shots + card text fit only).
- Renamed units in Test Fights (lead 2026-10-25, 0.1.21): Test Fights handed the wiki's generator the stored build, so a
  unit renamed in My Units (mu_rename / the launcher change the entry's name, never the build's) fought under its old
  name. Now fights/tf_ui.c takes myunits.c mu_fight_build_json: the build with the My Units name when the unit was
  renamed. Not a rename: the entry shows the build's own name; a default name key whose entry shows the key's game text
  (cm_gettext; the snapshot's case: the key stays, the game names it per language); an unnamed build whose entry shows
  the "<Race> <level>" label. A snapshot taken in another language whose name text differs counts as renamed (it shows the
  snapshot's text; the "language at snapshot time" point is left as it is). Bring Units unchanged (mu_build_of: the
  library name always wins). Test: fights/test/rename_game_test.py (real Test Fights tab, My Units source, 4 units:
  renamed key / not renamed key / renamed typed / typed; stock panel + multi-bar cards): main e5e1099 FAIL ("Old Name",
  "R'dor"), this 11/11 en + de.

## A Test Fight leaves the title and the player's records as they were (2026-10-25, 0.1.22)
- Owner (v0.1.27, custom-modes 0.1.16): "when we do a test, we then have the homepage homescreen changed to the base of the
  save we imported our units to for the test - shouldn't be the case". Found on today's build (fights/test/title_test.py):
  - title background = the test world (TitleBasePreviewSaveSelector picks the newest save; the test world's saves are
    still in Saves when the title is drawn: MoveSceneToTitle draws it before our 30-frame tick removes them);
  - OutGame.json gained the squad's gear in ItemObtainedSaveData.ObtainedItems (+6), and the game's memory kept them for
    its next write. Continue, SaveHeaderCache.json and UserCampaignLog.json were right (the tick's refresh; only
    PlayerApplicationMinute, the minutes the game was on, changes as in any session).
- Pick: keep the game from using / writing them during a test (no byte copy + marker needed: nothing on disk changes):
  - fights/tf_worlds.c title_init: hooks TitleBasePreviewSaveSelector.TryLoadCampaignSaveData (a test world answers
    "cannot load": Select takes the next world) and LatestCampaignSaveSelector.TrySelect (a test world pick is replaced by
    the next save of its sorted list that is no test world and exists). Read only; covers saves left by a crash too.
  - og_init: CampaignSaveLoader.TryLoad (every world load) marks a test world loaded: the records in memory are copied
    (OutGameRecordContainer.CreateSaveData) and SaveLoadHelper.SaveOutGameRecord is held; leaving it (title tick, or a
    TryLoad of another world) runs each record container's .ctor again on the same objects with the copy.
  - The tick's "our load is starting" wait now ends at the TryLoad of the test world (then the title gone), not at the
    first frame without the title: a load from inside a world (Restart test) passes through the title scene first
    (LoadCampaign: MoveSceneToTitle, then TryLoad), and the old rule could remove the world being loaded. tfw_write keeps
    older test world GUIDs listed until the title removes them.
  - New hook sites (loader docs/hooks.json needs `hooklist.py gen` at the next cm_pin): TitleBasePreviewSaveSelector.
    TryLoadCampaignSaveData/4, LatestCampaignSaveSelector.TrySelect/3, CampaignSaveLoader.TryLoad/3,
    SaveLoadHelper.SaveOutGameRecord/1. All have direct callers on Windows (win_callers.py), so none is inlined away.
- Test: fights/test/title_test.py (one hidden copy; a copy of a real save as the newest world; DPS Check, then a Boss
  fight on Downfall; Esc > Quit to title; then Continue pressed). The test module font-probe has a "title" read-out.
