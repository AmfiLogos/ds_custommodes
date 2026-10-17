# i18n of Custom Modes (branch i18n)

Two lanes, merged here: the solo part (custom-modes.dylib, branch i18n-solo) and the multiplayer part
(custom-modes-mp.dylib, branch i18n-mp). Both parts keep their own string tables (one `DSI_IMPLEMENTATION` each:
home/cm_i18n.c and cm_mp.c) and read the same files.

## Integration
- Shared core updated (i18n/ds_i18n.h, tools/i18n_tool.py, copied unchanged): TRN_NOOP, `\u` escapes in the
  extractor, `format_arg` attributes (clang checks printf arguments against the English key: the Mac build with -Wall
  has no format warning; the Windows build passes -Wno-format, so only the Mac build checks), `_wfopen` on Windows,
  `-DDSI_PREFIX=xx_` renaming for the one-DLL Windows rig.
- One language folder for both parts: `mods/custom-modes/lang/`.
  - Mac: solo = loader `module_dir` + `/lang` (`<Mods>/custom-modes`); -mp = dladdr folder of custom-modes-mp.dylib +
    `/lang`. cm_module.c loads custom-modes-mp.dylib from the folder of custom-modes.dylib (cm_my_dir), so both are
    the same folder.
  - Windows: both parts take their own DLL folder with GetModuleFileNameW and make it UTF-8 (the core opens files
    with `_wfopen` on a UTF-8 name; the loader's `module_dir` and GetModuleFileNameA are ANSI, wrong for a non-ASCII
    install path). custom-modes-mp.dll sits next to custom-modes.dll, so it is the same folder.
- Title button: the solo part registers it again after a language change only when `host->api_version >= 4`
  (home/cm_i18n.c `cm_i18n_loader_relabels`). The loader i18n branch still has `DSL_API_VERSION 3` (checked
  2026-10-17): the check is kept as it is. With loader API 4 (same owner + click + ud = relabel) it works with no change here.
- Compared by value: no code path compares a translated CM label. cm_home_test_button compares the English key or the
  id; fe_game.c / pvp_units.c error codes ("host only", "nobody brought units") stay English codes; tf_ui.c compares mode ids.
- units/pvp_units.c (in the -mp dylib, left by both lanes): the error texts the player sees are TR (Bring window
  errors through pvp_bring_picks, the "pvp_units_bring" / "pvp_units_wiki" answers the PvP Bring window shows):
  "Not in a multiplayer session.", "No units to send.", "The units could not be sent to the host.",
  "This link was made with other wiki data (another game version): make it again on the wiki." (same keys as fe_game.c
  and wk_link.c), "unit %d: %s". Not wrapped: say() logs, rig-only answers ("need file and guids", "need link"),
  the cmd_start codes.
- Extract: `python3 tools/i18n_tool.py extract lang .` from the repo root (the tool skips quickjs/, build/, old/;
  file names in _where.tsv are repo-relative). **548 keys, 27 plural** in lang/en.json. Table keys (TR_NOOP) are in
  it: home entries, Unit Builder sections, PvP rows, save folder labels, wiki kind words, cm_mp.c message patterns.
- Tests (English, no game run), with MP_ROOT = the pinned tree build/mp/<pin>/ inside the worktree:
  build.sh all green (Mac + Windows), pvp_test 607/607, mu_test 148 PASS 0 FAIL, save_test 5693 passed 0 failed,
  fights run_eq (64 files identical, 25/25 saves), builder run_eq (300/300 equal), gal_names (792 names identical).

## Open
- home/cm_i18n.c `read_usersetting` opens `<data_dir>/../UserSetting.json` with fopen: on Windows a non-ASCII user
  folder fails there (then the first language is English until the game's own value is read, at most ~30 s). It
  depends on the encoding of the loader's `data_dir` (not checked); not changed.
- The language poll, GetText keys and the live re-labels are not tested in the game yet (later lane).

# Solo part (lane i18n_cmsolo, branch i18n-solo)

Scope: `custom-modes.dylib` (units/, home/, fights/ without quickjs, gallery/, builder/, cm_module.c, its build parts).
Not touched: pvp/, fe/, cm_mp.c, `units/pvp_units.c` (built into custom-modes-mp.dylib: lane i18n_cmmp).

## How it works
- `i18n/ds_i18n.h` and `tools/i18n_tool.py`: the shared core, copied unchanged.
- `home/cm_i18n.c`: the one `DSI_IMPLEMENTATION` of custom-modes.dylib. `cm_i18n_init` runs first in `dsl_plugin_init`:
  `dsi_set_dir(<module_dir>/lang)` (Windows: the DLL's own folder, UTF-8), first language = `UserSetting.json` (`<data_dir>/../UserSetting.json`,
  `dsi_lang_from_usersetting`), so the title button and home entries are registered in the saved language.
  Then `LanguageSetting.GetCurrentLanguage` (resolved by name, `il2cpp_runtime_class_init` once) is read every frame in
  the cm_win.c frame tick (InputDispatcher.Update). While the game boots the static is still 0: the poll follows the game
  only once it agrees with UserSetting.json (or after 1800 frames). A change calls every `cm_i18n_on_change` callback.
- On a change: home page built again (reopened if open); My Units, Bring Units and Unit Builder windows built again with
  the same state (the screen-resize path); Test Fights and Gallery windows opened again.
- `cm_i18n_on_change` / `cm_i18n_loader_relabels` are exported: the -mp part (it links custom-modes.dylib) can register
  its own redraw there. Its TR tables are its own (DSI symbols are hidden per image).
- `fe/fe_json.c` and `fe/fe_save.c` are in the SOLO dylib (fe/sources.txt): a TR there uses these tables.
- Language files ship as `lang/*.json` (not en.json) next to the dylib: `build_module.sh` (OUT = .../custom-modes) and
  `build.sh win` copy them. The loader's build_mods.sh skips files in subfolders for `files`, so lang/ is not hashed.
- Tests: `units/test/dsi_impl.c` gives the test binaries the tables (English); added to units/test/Makefile,
  units/test/save/run.sh, units/test/http/*.sh, fights/build.sh, builder/build.sh.
- `tools/i18n_wrap.py`: the wrapping as exact edits + two regex rules (err-buffer snprintf, wk_link set_err). It skips
  edits already there, so it can run again on a newer main; it reports edits whose old text moved.
- Solo part alone: 294 keys, 12 plural. Extract for the repo: see Integration.

## Needs the loader (not done here)
- Title button label: the loader copies it once (`dsl_ui.c` TBE). A second `title_add_button` today ADDS a button.
  CM registers again after a change only when `host->api_version >= 4` (`cm_i18n_loader_relabels`). Wanted in the
  loader: a second `title_add_button` from the same module with the same click + ud updates label/tip (and re-sets the
  clone's text); `menu_add_tab` with the same id + owner updates the title. Until then the title button changes
  language at the next start (it is registered in the UserSetting.json language).

## By-value fixes
- `home/cm_home.c` `cm_home_test_button`: by the entry's English label (the TR key) or its id, or "Back"; the screen
  text is never compared. `cm_home_test_state` reports English labels.
- `units/wk_http.c`: "network down, do not try the other backend" compared the error TEXT ("timed out", "no internet",
  curl "Timeout"/"resolve"); now a flag set by the backends (CF timeout / -1009 / 8 / 12, curl 28 / 6 / 5).

## UTF-8
- `home/cm_il.c cm_il_str`: `dsi_utf8_fix_end` on a copy before `il2cpp_string_new`.
- `fights/tf_ui.c unesc` and `gallery/gallery.c unesc`: `dsi_utf8_fix_end` after the byte cap.
- fix_end after byte-capped snprintf of translated text: Bring Units title/subtitle, "Team %zu", card line, kit-miss
  lines, Magic Saturation days, gallery card text, builder sub lines.
- Not changed: cm_win.c `x_str_new(name)` for save names / screenshot paths (file names we make, ASCII).

## Game words (TEXTKEY at runtime, our English as fallback; `cm_game_text` / `cm_gw` strip rich tags)
| English | TEXTKEY | where |
|---|---|---|
| Back | TEXTKEY_Common_Back | home page |
| Magic Saturation | TEXTKEY_AFFECTER_MagicSaturation_NAME | Test Fights row |
| Combat / Management | TEXTKEY_SaveLoad_CombatDifficulty / _ManagementDifficulty | Test Fights rows |
| Easy, Normal, Hard, Very Hard, Devastation | TEXTKEY_CreateCampaign_<tier>Difficulty (tier id from the panel) | Test Fights values |
| boss / monster names | TEXTKEY_<unit key>_NAME (panel `bosses[].key`, `enemies[].k`) | Test Fights |
| practice dummy | TEXTKEY_BUILDING_DungeonScareCrow_NAME (panel `dummy.k`) | Test Fights DPS text |
| carry inscription | TEXTKEY_AFFECTER_HighGradeMaxWeightInscription_NAME (panel `kitInsc.k`) | Test Fights kits |
| Skills | TEXTKEY_GAMEGUIDE_SkillsAbility_NAME | Unit Builder nav + heading |
| Sword, Greatsword, Spear, Blunt, Bow, Shield, Dagger | TEXTKEY_Equipment_<X>_NAME | card weapon word |
| Fire / Water / Nature Staff | TEXTKEY_Equipment_FireStaff_NAME etc. (English: "Fire Magic Staff") | card |
| Main / Sub Accessory | TEXTKEY_EquipmentType_Accessory1 / 2 | card |
| Body Armor | TEXTKEY_EquipmentType_Body ("Torso Armor") | card |
| Torch / Arrow | TEXTKEY_ITEM_Torch_NAME / TEXTKEY_Tooltip_RequiredAmmoAmmo_Arrow | card |
Unit names already came from GetText (kept). No key with exactly that word (TR, translators decide):
Difficulty, Gear, Skill trees, Weapon trees, Race, Races, Background, Traits, Inscriptions, Helmet, Hood, Axe.
Game words inside our sentences, keys the translators can copy: Cancel TEXTKEY_CommonPopUp_Cancel, Close
TEXTKEY_Common_Close, Save TEXTKEY_Common_Save, Level TEXTKEY_Shared_Level_NAME, Background TEXTKEY_BackgroundBiography_NAME
(a biography title, not used), Units TEXTKEY_GameOption_UnitSelection.

## Not wrapped, and why
- Logs, rig/test commands, JSON keys, ids, test registry names (`nav:Race`, `fights:start` ...).
- Internal engine errors that go to the log (`eng_why`): bld_js.c / sg_js.c ("QuickJS runtime failed", "BLD.%s missing",
  "panel: not JSON" ...); pvp_wiki.c binary-decode details (logged; the player sees "This link is damaged...");
  pvp_build.c build notes (not shown); "internal: ... JSON" texts.
- Text from the wiki (English): Test Fights kit names, kit-miss reasons, gallery words (races, trees, nouns, name
  templates from builds/words.json), everything the Unit Builder lists (names, descriptions, lock reasons, slot labels,
  stat names). The OS texts in errors (strerror, curl_easy_strerror).
- `units/pvp_wiki.c` "%s %.*s is not in this game version": the kind word is TR'd separately (word order fixed).

## Builder JS (list only, not changed)
`builder/bld_glue.js`: 'this tree is not active', 'requires level {#}', 'requires {#}', 'not enough main SP',
'not enough sub SP', stat short labels 'HP' 'P.Atk' 'M.Atk' 'Atk Spd' 'Crit %' 'Crit Dmg' 'Phys Res' 'Mag Res' 'Move',
"New Character", "The wiki's Unit Builder changed. This mod needs an update to use it.", "Could not read the wiki's
builder files.". The rest comes from the downloaded wiki `builder.js` + `data.js` (English).
To show the builder (and Test Fights panel, gallery words) in the game language: the wiki has localized copies at
`/<lang>/builder/{data.js,builder.js,savegen.js}` and `/<lang>/builds/words.json` for de, fr, ja, ko, ru, zh, zht (no es,
pt-BR: those stay English). It would take: sg_cache root + URL per language (`Mods/custom-modes/wiki/<lang>/`, game code
-> wiki code: zh-Hans zh, zh-Hant zht), a reload of the engine on a language change, a check that the localized data.js
carries the same tag and keys (the save generator must make the same file), and the glue strings above passed in
from C (TR) or read from the localized builder.js.

## Left
- mod.json name/description stay English (loader lane adds "i18n").
- Bring Units title/subtitle are the caller's (fe/pvp: -mp lane).
- No game run in this lane: the language poll, GetText keys and the relang rebuilds are untested in the game.

# Multiplayer part (lane i18n_cmmp, branch i18n-mp)

Scope: `pvp/`, `fe/`, `cm_mp.c` (custom-modes-mp.dylib / .dll). The solo part (units/, fights/, builder/, gallery/,
home/, cm_module.c, mod.json) is lane i18n_cmsolo.

## What is in the branch
- `i18n/ds_i18n.h`, `tools/i18n_tool.py`: the shared core, copied unchanged.
- `cm_mp.c`: `DSI_IMPLEMENTATION` (this image's tables; symbols hidden on Mac), `lang_dir()` = `<folder of this dylib>/lang`
  (dladdr on Mac, GetModuleFileNameW as UTF-8 on Windows), `lang_poll()` registered as `MP_FRAME_ALWAYS`:
  `Refactor.Setting.LanguageSetting.GetCurrentLanguage` resolved by name (class_by_name + il2cpp_class_get_method_from_name),
  English until the title scene is up. On a change: the two host choices are registered again (same id = replaced in
  place; label cut to 47 bytes, tip to 299, at a character boundary).
- `fe/cm_mp_text.h` (implemented in cm_mp.c): `cmt_set` (tmp_set with a whole UTF-8 character at the end), `cmt_game`
  (TextKeyExtensions.GetText, cached per language), `cmt_level`, `cmt_day`, `cmt_msg` (see below), `cmt_gen`.
- `tools/i18n_wrap_mp.py`: the mechanical pass (tmp_set -> cmt_set; TR() around text literals given to cmt_set,
  tip_try, TIPW, few_set, few_label, mk_button labels, center, ui_notice and snprintf formats). Re-runnable and
  idempotent: on a newer main run `python3 tools/i18n_wrap_mp.py`, then check the diff. The hand edits are commit 2.

## Hard cases done by hand
- PvP refusal on the wire (`PVP_M_MODS_REFUSED`): `pvp_modset_check_why` gives a code + args (`pvp_why`: code, n, who,
  a, b). The mail is `{"code","n","who","a","b","why"}`; each guest renders the code in its own language
  (`pvp_why_parse`). `why` = the host's own text, so a guest from before the codes still shows something; a new guest
  that gets an old host's mail (no `code`) shows its `why` as it is. An unnamed player goes as `who: ""` and each
  side shows TR("A player"). Test: pvp_test (607 checks, 10 new: round trip, old-host mail, a French guest table,
  a dropped bad translation).
- Plurals -> TRN: unit counts (pvp_ui team columns, lobby "N units brought", Bring status, Bring hint, brought-units
  note), captures to win, FE "N units each", save rows "· N units", results list, dungeon loot items, "N dungeon items:
  storage first...", "Up to N units each in this hub", Vanilla check "mod/mods" + "it/them".
- Glued pieces -> one format: "%s (you)", "%s's hub rules:" / "The host's hub rules:", "%s's hub" / "The host's hub",
  "%s ended the expedition..." / "The host ended...", CTF and round-end centre messages are whole sentences per team
  ("%s took the Red flag", "Blue wins the match  %d - %d", ...), "Could not bring these units: the save could not be read."
- Colour / size / pos tags moved out of the text where it was easy (results table, status words, Bring rows, HUD line,
  lobby name suffix, Brought home / Already Brought Home boxes). Left inside: the FE Bring note (`<color=#CCBEB8>At
  home:</color> ...`, several coloured words inside one paragraph); the core keeps tags identical.
- UTF-8: unit names from GetText / GetDisplayName now go through `utf16_to_utf8` (was the ASCII-only `str_utf8`: "???"
  for non-Latin names) in pvp_ui.c and pvp_game.c `pvp_ui_unit_name`. Save name cut (`%.24s` + "…") cuts at a
  character boundary. Every composed label goes through `cmt_set` (fix_end); small buffers grown for longer languages.
- Compared by value (kept as codes, translated only for display):
  - fe_game.c `"nobody brought"` / `"host only"` (cmd_start JSON errors) stay English codes; the shown texts are TR.
  - fe_game.c `strstr(m, "choose per unit")` on the fe_save.c error: fe_save.c stays English (pure C, compiled alone by
    ds_multiplayer ext/foreign_expedition/tests/test_c.py, and it is the solo dylib's file); the Results window shows
    W.err through `cmt_msg`, which matches the known English patterns (cm_mp.c `msg_pat`, TR_NOOP keys, up to two %s)
    and prints the translation.
  - fe_game.c save folder labels ("Multiplayer folder", "Game folder", "where they came from") are stored English
    (TR_NOOP) and translated where shown; fe_result_info.host "the host" is a code for "no name".
- Live language change (only on the title screen): FE windows set every text each frame (the 4 static button labels
  moved into few_sync); the PvP v1 host page panel re-labels on `cmt_gen()` change; host choices re-registered.
  PvP windows, HUD and result window are built in a campaign (language fixed there).

## Game words
Taken from the game at runtime (cmt_game, English fallback when the key is missing):
| Our text | TEXTKEY | where |
|---|---|---|
| Level (Level 5) | TEXTKEY_STATUS_Level_NAME ("Level"; fr Niveau, de Stufe) + " %d" | PvP Bring cards, FE unit cards, FE results table head |
| Day 12 | TEXTKEY_SaveLoad_Date ("Day{0}"; ko "{0} 일차", ja "{0}日目") | FE save rows, Bring home to, My Units Bring "when" |
| Back | TEXTKEY_Shared_Back | PvP Bring Back button + tip title, FE list Back |
| Cancel | TEXTKEY_CommonPopUp_Cancel | FE Cancel button + tip title |
| Close | TEXTKEY_Common_Close | PvP window Close button + tip title |
Unit and item names were already from GetText (kept).

Game words INSIDE our sentences (stay in TR text; translators: use the game's term):
| English | TEXTKEY |
|---|---|
| storage ("storage first, then backpacks") | TEXTKEY_BUILDING_Storage_NAME "Storage" |
| backpacks | none found (no exact key) |
| Load campaign ("Load it from Load campaign") | TEXTKEY_TitleMenu_LoadCampaign "Load Campaign" |
| settlement ("This settlement", "return to the settlement") | TEXTKEY_MAPTYPE_Base "Settlement" |
| Experience ("Experience does not count here") | TEXTKEY_Shared_EXP_NAME "Experience" |
| Skill tree ("No skill tree yet") | TEXTKEY_RecruitUI_SelectedCandidateSkillTree "Skill tree" |
| Dead / Died | TEXTKEY_Common_Dead "Dead" |
| Dungeon loot, Expedition (Expedition version / Results), Recruiting, XP | no exact key: ours |
Our own names (one glossary with the other lanes): Foreign Expedition, PvP, PvP Match, Vanilla check, Bring Units,
My Units, Capture the Flag, Planning pause, Blue / Red (teams), hub.

## Not wrapped, and why
- say() in fe_game.c, rig_log, rig/test command JSON (`fe_exec`, `pvp_exec`, `pvp_mods_exec`, cmd_* "error" codes),
  pvp_core.c `pvp_err_text` (logs only), file names, keys.
- fe_save.c / fe_json.c messages: English at the source (see above); fe_json parser details ("value expected" ...)
  and fe_check_refs details stay English (technical, only inside "details in the log").
- fe_hub.c world names "Foreign Expedition Hub" / "PvP Base": written into the save file (save data, not UI).
- FE results date `strftime("%d %b %Y")` (C locale month names) and fe_save's applied date: left as they are.
- pvp_ui.c br_list_saves uses str_utf8 for persistentDataPath (a non-ASCII user folder would not list saves; old).

## Build / shipping notes
- `i18n/ds_i18n.h` is found through the repo root include (-I.), on Windows through `-I$C` (custom/). lang/ ships as
  one `mods/custom-modes/lang/` for both dylibs (build_module.sh and build.sh win copy it, see the solo part); it must
  stay out of mod.json `files` (mod-set hash).
- Windows rig target (win/build_win.sh without DSMP_TARGET=module) builds multiplayer + both CM parts into ONE DLL:
  the ds_multiplayer build compiles CM solo files with -DDSI_PREFIX=cms_ and mp files with -DDSI_PREFIX=cmm_ (core
  renaming), so the three string tables do not clash. No CM file declares dsi_* by hand. build.sh uses the module
  target (separate DLLs).
- Tests: `MP_DYLIB=... zsh build.sh all` green (Mac + Windows), `make -C pvp/pvp_test test` 607/607. fe_save.c /
  fe_json.c are unchanged, so ext/foreign_expedition/tests/test_c.py is not affected (it also picks ds_multiplayer's
  own custom/fe copy first). No game run.
- This lane alone: 257 keys (15 plural). Extract for the repo: see Integration.
