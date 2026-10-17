# i18n, solo part (lane i18n_cmsolo, branch i18n-solo)

Scope: `custom-modes.dylib` (units/, home/, fights/ without quickjs, gallery/, builder/, cm_module.c, its build parts).
Not touched: pvp/, fe/, cm_mp.c, `units/pvp_units.c` (built into custom-modes-mp.dylib: lane i18n_cmmp).

## How it works
- `i18n/ds_i18n.h` and `tools/i18n_tool.py`: the shared core, copied unchanged.
- `home/cm_i18n.c`: the one `DSI_IMPLEMENTATION` of custom-modes.dylib. `cm_i18n_init` runs first in `dsl_plugin_init`:
  `dsi_set_dir(<module_dir>/lang)`, first language = `UserSetting.json` (`<data_dir>/../UserSetting.json`,
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
- Extract: `python3 tools/i18n_tool.py extract lang units home fights gallery builder` (+ the -mp lane's fe pvp).
  Solo part alone: 294 keys, 12 plural.

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
