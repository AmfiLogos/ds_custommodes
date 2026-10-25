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

## Test Fights: folded teams, bag and affecters from the save (2026-10-25, 0.1.23, lane dsfights)
- Owner: "team units should be collapsed by default rather than extended". A team row in the squad picker is one line
  (arrow, icon, name, n/4, box); its arrow (the kit's own chevron, turned down when open) lists its units. The box adds or
  removes all its units while folded too. World units' world rows show the same arrow (a click on the row opens it).
  Kept while the window is open (a language change keeps it); every open starts folded (tf_open, tf_open_with_builds).
- Owner: "when loading units from save directly bring over the items they carry". mu_save.c mu_save_unit_carry reads the
  unit's backpack (component Type 5): per filled slot the key, amount, AutoFixed, ItemAutoFixable and the item entity's
  components. mu_worlds.c mu_save_fight_build puts it beside the build as "_cm". sg_glue.js carry() writes it into the
  generated test world after the wiki's generator: the backpack slot for slot, each item a copy of the save's entity
  (durability, rolls, its own holders) under a new guid, registered in ItemContainerSaveData. The generator's own fill
  for that unit (the arrows it packs for a bow user) goes; ticked battle kits stay, in the free slots after.
  Why in the save and not the game's add-item call after the load: the game builds the items from the save with its own
  loader, nothing has to find the units after the load, and the copies keep durability and rolls (an add-item call makes
  new items). The game test proves the game holds them (a save the game writes right after the load).
- Owner: "Give the option to import their affecters from save as is (food buffs & others)". Switch "Bring buffs and
  effects from the save", default off, World units only. On: the unit's AffecterHolders as the save holds them (Key,
  Stack, Duration, InitialDuration); a holder the build also writes is replaced by the save's. Pick: the difficulty
  holders (AFFECTER_PlayerDifficulty*, AFFECTER_PlayerManagementDifficulty*) stay the fight's, because the player picks
  the difficulty in this window; a save's Very Hard holder next to a Normal fight would count twice.
- Keys the game's tables do not know (DataSheetManager GetItemData / GetAffecterData; both end in GetValueOrDefault,
  no exception) are left out and counted on one line under the switch before Start (tf_card.c tfc_known).
- carry() finds a unit by its place (the generator writes the roster in build order) and checks its name; a mismatch or
  a bag that does not fit is a problem: the test world is not made and the panel says why (English, as the generator's
  own problems).
- Tests: fights/test/run_saveunits.sh now also generates with the World units builds: bag slot by slot (key, amount,
  AutoFixed, item components), switch off = the plain build's holders only, switch on = every save holder as is and the
  fight's difficulty, kits on = bag + kit, an unknown key left out and counted (each fails by mutation: bag not brought,
  effects not brought, save difficulty kept, effects sent with the switch off). saveunits_game_test.py: teams folded on
  first open and after a reopen, the box adds 3 units while folded, the arrow opens / closes; fight 1 (switch off) and
  fight 2 (switch on, two units with a bag): the game's own save of the running world (test call tf_test_save) holds each
  unit's bag as in the save, no save affecter with the switch off, every save affecter within 1 s with it on.

## Test Fights > DPS Check while it runs (2026-10-25, 0.1.24; fights/tf_dps.c)
- Owner: the practice scarecrow's HP "997296 / 100000" wrapped its last "0" at 1,000,000; its HP stood still while the
  mouse stayed on it; "a DPS display log logging the damages in the chat of the MP function", every hit.
- Only in a world loaded from a DPS Check save (tf_wikisave.c tfw_dps_made, set in tf_worlds.c's load hook); every hook
  passes straight through anywhere else. Nothing changes game state.
- Panel (EntitySummaryUIView.UpdateBuildingEntityUI hook): "<now> / <max>" with the language's thousands
  separator (as the Test Fights card), one line. A number that fits its 50-unit box stays as the game shows it; a longer one
  grows away from the "/" (now right-aligned, max left-aligned) into the bar's free room; only if that is too narrow both
  shrink alike (never under 60 %). en/de/ru/ja: 1,000,000 fits at the game's size. The panel follows the shown building's
  durability through the loader's Fix live-hp ("Live building HP", every building; it calls UpdateBuildingEntityUI on each
  change, so this hook's number form runs each time). Without live-hp (not installed, switched off, failed) the frame tick
  here keeps the scarecrow's panel current itself (live_tick; `live_hp` asked every 30 frames, not at init: live-hp's own
  init runs after ours).
- Stock (decomp 25846389): a building's durability in that panel is set only when the panel is filled
  (EntitySummaryUIPresenter.RefreshSummary); status changes refresh UNITS only. So every building's panel stands still in
  the stock game; the Fix live-hp makes them all follow. Measured: with no follow the text stayed while durability fell.
- Hit log (DamageSimulator.Simulate hook, one call per hit): target = BUILDING_DungeonScareCrow (the wiki's DPS target) ->
  "<unit display name>: <skill / affecter name in the game's language> <TotalDamage as F0>[ Crit|Guard]", "Miss" / "Immune"
  with no number (the game's VFXExecution words). Into the multiplayer chat (mp_api v3 chat_local: never sent, no rate
  limit, shows in single player); without multiplayer (or "Chat: Off", or an older multiplayer) into our own panel
  (bottom-left, 10 lines, wheel scrolls; without multiplayer the game's camera zoom is held while the pointer is on it:
  not proven in game). All lines kept for the fight; a new DPS Check clears.
- The scarecrow's self repair (AFFECTER_SelfRepair, 200) did NOT pause while it was hit: +200 about every second through a
  30 s burst. The durability drop understates the squad's damage by that; the log does not.
- New hook sites (docs/hooks.json at the next cm_pin): EntitySummaryUIView.UpdateBuildingEntityUI/1,
  DamageSimulator.Simulate/4, UnityEngine.Input.get_mouseScrollDelta/0 (ONLY when multiplayer is not loaded; multiplayer
  owns that site otherwise: sites.tsv note wanted). All have Windows callers or are virtual (Simulate): none inlined away.
- Test: fights/test/dps_test.py (one hidden copy; 4 fixture units with weapons; Test Fights > DPS Check > Start; the
  scarecrow clicked, the mouse kept on it; each unit right-clicks it). Checks (a) one line and inside the bar at 1,000,000
  and 999,999 (TMP textBounds against the "/" and the bar box), (b) the panel follows, (c) per 0.25 s read the logged
  damage = durability lost + whole repair steps, (d) one line per hit, all in the chat in order; our own panel forced on
  shows the newest 10. --no-mp (multiplayer off) did not get past the title in a hidden copy (the title button click did
  not open the lab: 3 tries), so the no-multiplayer path itself is only proven through the forced panel.

## Escape closes our windows (owner 2026-10-25, 0.1.25: "make sure Escape works to close our custom windows")
- The loader kit owns the key (kit 0.1.28, dsl_sdk.h `esc_push` / `esc_drop`, wrappers `dsl_esc_push` / `dsl_esc_drop`):
  each window pushes itself when it shows, one press calls the close of the top-most one that still shows, and the game
  never sees that press. Pushed here, close = the window's own button: the home page (Back), My Units (Close), its paste
  box and name box (their Cancel, `DSL_ESC_EDIT`: also while their field has the keyboard), its Delete team box (Cancel),
  the Unit Builder (Cancel: unsaved changes -> the game's notice, which the kit pushes itself: Escape there = "Keep
  editing"), a gear slot's item list in the Builder (closes the list), the Gallery and Test Fights (cm_win X).
  Plain fields (Builder name / search, Gallery and Test Fights search): the 1st press only leaves the field.
  Gallery Details: Escape = its Back first, the next press closes the Gallery (owner 2026-10-25 "Go Back one level";
  cm_win.c cmw_set_back: a window's sub-page Back runs before the window closes, so the window keeps its place on the stack).
- A window rebuilt (screen size, UI scale, language) is pushed again with its open boxes after it. Hidden with
  set_active = esc_drop; destroyed = the kit drops it.
- cm_win's own Escape polling (win_tick) runs only for a window the kit did not take (an older loader): never two closes
  for one press. With an older loader My Units, the Builder and the home page have no Escape (as before 0.1.19).
- Builds need the loader's sdk/ (kit 0.1.28): an older dsl_sdk.h stops the build (#error in home/cm_look.c), so a build
  can never ship without Escape. build.sh: `SDK=<ds_modloader>/loader/sdk` until the multiplayer pin's sdk/ has it.
- A field the Escape left also lets go of the EventSystem selection (kit fix, dsesc 2026-10-25): before, when the window
  survived the next Escape (Builder Cancel showing "Discard your changes?"), the field took the keyboard back and the 3rd
  Escape only left it again. esc_test case 7 meets it in the player's order (unsaved change first, then the name field).
- Test: units/test/esc_test.py (one hidden copy, title, real Escape key events through the test module font-probe, test
  library DSMP_UNITS_LIB; the owner's library checked unchanged). It fails with 0.1.18 on the same loader.
- 0.1.26 (owner asks 1-9, 2026-10-25, task file LANE_TASKS_dsdps): the full numbers stay (no "997.3K"). The Inspect window
  (SubUI_EntityInspectionStatus.SetBuildingStatus hook) follows too: the Fix live-hp calls SetBuildingStatus again on
  each change (with the entity's own StatusReader); this hook writes the rows in the thousands form, with the bottom
  panel's StatusReader, through DASText.SetText (a TMP set_text on a DASText is replaced by its own copy). Without
  live-hp, ins_tick does the window's follow here.
- DPS meter (fights/tf_meter.c): left side, folds, Start / Stop / Reset, "Start on first hit"; time, damage dealt, DPS,
  DPS last 5 s, what the scarecrow repaired (StatusSimulator.Simulate hook: Stat Durability on it, the log's Amount);
  one row per unit (portrait, name, damage, DPS, share, bar), a click shows its skills. The ONE hit list (tf_dps_hits)
  feeds the meter and the chat. Placed from the game's own panels at run time (resource list, unit panel, an open
  Inspect window: then right of it), checked at UI 1.0 / 1.4 and 960x540 / 1920x1080. Game clock = TimeHandler
  _currentTick * TICK_INTERVAL (speed / pause do not use Time.timeScale).
- Switches (cm_prefs, also in the Test Fights setup under the DPS card): Scarecrow speech OFF, Scarecrow heal numbers OFF,
  DPS log in chat ON, Start on first hit ON. Speech and the "+200" are their view iterators ended at once
  (AffecterFX_SelfRepair.OnFXCall, StatusResultFX.HandleFX: "<>1__state" = -1); the sim is untouched, the repair goes on.
- Chat log format (mp_api v3 MP_CHATL_RICH): a header line with the unit's name (gold, bold) when the hitting unit
  changes, then one indented line per hit: skill (auto attacks SKILL_Basic* / SKILL_NPCBasic* = the game's
  TEXTKEY_SKILL_BasicAttack_NAME "Basic Attack"), damage in a monospaced column, "Crit" in orange. Names escaped.
  Open choice (owner): the table flag IsBasicAttackSkill would also call Twin Thrust, Rapid Fire, Fire Missile and Healing
  Touch "Basic Attack"; the key prefix keeps their names.
- Restart test (tf_worlds.c tf_restart, in every test world; a small panel with only that button outside DPS Check): the
  start save kept by tfw_write is written and loaded again by the Start path. A restart passes through the title scene
  (the title background world loads first): the "--- restart ---" chat line waits until the world shows 30 frames.
- 0.1.27 (owner ask 10 + answers, 2026-10-25): the log shows every damage, heal, shield absorb and effect put on where a
  squad unit or the scarecrow takes part (hooks DamageSimulator / HealSimulator / AffecterSimulator / StatusSimulator.Simulate,
  read only). One indented line per source and game tick (TimeHandler._currentTick), parts joined by " · ": "<source>[ ×n]
  » <targets>: -X[ Crit]" (damage), "+X" (heal, green), "<shield> (<owner>): absorbed X" (StatType Barrier before - after;
  the absorbed part is no damage), "<effect> » <targets>" (quiet; only effects the game names; switch "Effects in log", on).
  The arrow is "»": the chat's fonts have no "→" (it showed "?"). Basic attacks (SkillTableData.IsBasicAttackSkill, class
  Util.Sheet) read "<name> (basic)". A source with no skill behind it (an item such as a torch) is named by its key.
  Heals that add nothing (full health) are not logged; the overheal part is not known from the game's heal log (it holds
  max health and max health + the added part only), so "(n over)" never shows yet.
- Meter: "Healing" column (squad units, per skill too), "Absorbed" line; only damage on the scarecrow counts as damage.
- Who follows (lead 2026-10-25: "the DPS Check is a test tool and must always show live HP"): with the Fix live-hp running,
  live-hp only (every building; our hooks add the number form); without it, our own live_tick / ins_tick (scarecrow only).
  One owner per hook site holds: ours is a frame poll, live-hp's is EntitySummaryUIPresenter.RefreshOnFrame. Units
  Laboratory 0.1.28 / 0.1.29 (7a77697, a97607e) had no own follow: dps_test --no-live-hp fails there ((b), (b2)), passes on 0.1.30.
