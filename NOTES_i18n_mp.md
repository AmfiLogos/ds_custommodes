# i18n, multiplayer part of Custom Modes (lane i18n_cmmp, branch i18n-mp)

Scope: `pvp/`, `fe/`, `cm_mp.c` (custom-modes-mp.dylib / .dll). The solo part (units/, fights/, builder/, gallery/,
home/, cm_module.c, mod.json) is lane i18n_cmsolo.

## What is in the branch
- `i18n/ds_i18n.h`, `tools/i18n_tool.py`: the shared core, copied unchanged.
- `cm_mp.c`: `DSI_IMPLEMENTATION` (this image's tables; symbols hidden on Mac), `lang_dir()` = `<folder of this dylib>/lang`
  (dladdr on Mac, GetModuleHandleExA on Windows), `lang_poll()` registered as `MP_FRAME_ALWAYS`:
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
- No change to build.sh / build_module.sh: `i18n/ds_i18n.h` is found through the repo root include (-I.), on Windows
  through `-I$C` (custom/). lang/ is shipped by the loader package (one `mods/custom-modes/lang/` for both dylibs); it
  must stay out of mod.json `files` (mod-set hash). build.sh does not copy lang/ into build/<os>/custom-modes/.
- .gitignore: added !/i18n/ !/tools/ !/lang/ !/NOTES_i18n.md !/NOTES_i18n_mp.md (the solo lane likely adds the first ones too: an easy merge).
- Windows rig target (win/build_win.sh without DSMP_TARGET=module) builds multiplayer + both CM parts into ONE
  baselib.dll: if multiplayer or the solo part also defines DSI_IMPLEMENTATION there, that link has duplicate dsi_*
  symbols. build.sh uses the module target (separate DLLs), which links fine. Not changed here.
- Tests: `MP_DYLIB=... zsh build.sh all` green (Mac + Windows), `make -C pvp/pvp_test test` 607/607. fe_save.c /
  fe_json.c are unchanged, so ext/foreign_expedition/tests/test_c.py is not affected (it also picks ds_multiplayer's
  own custom/fe copy first). No game run.
- Extract: `python3 tools/i18n_tool.py extract lang pvp fe <dir with cm_mp.c>`: 257 keys from this lane (15 plural).
