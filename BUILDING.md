# Building ds_custommodes

This repo builds the Custom Modes mod for the mod loader: Mac (arm64) and Windows (x64).
All builds run on a Mac. The Windows files are cross-built on the Mac.

## 1. Requirements

You need a Mac with Apple silicon (arm64). The Mac module is arm64 only (`build_module.sh`).
The versions below are the ones used for the current builds.

| Tool | Version used | Used by |
|---|---|---|
| macOS | 27.2 | all |
| Xcode (clang, codesign, strip) | Xcode 27.1, Apple clang 21.0.0 | Mac build |
| llvm-mingw | clang 23.1.3, target x86_64-w64-windows-gnu | Windows build |
| Python 3 | 3.10.4 | Windows build (through ds_multiplayer `win/build_win.sh`) |
| git, rsync | git 2.54.0, openrsync | pinned multiplayer tree, Windows copy |
| GNU Make | 3.81 | tests |
| Node.js | v16.14.2 | equality tests only |

Python packages for the Windows build: `pefile` (2024.8.26), `lief` (1.0.0), `capstone` (5.0.7).

Download llvm-mingw from the macOS release of https://github.com/mstorsjo/llvm-mingw.
Fixed tool place (no env var): the scripts expect llvm-mingw at `tools/llvm-mingw` in your home folder
(`build.sh:41`, `fights/build.sh:14`, and ds_multiplayer `win/build_win.sh:12`). Put it there, use a link, or edit those lines.

## 2. External inputs (not in this repo)

Never commit these.

| Input | What it is | Env var |
|---|---|---|
| ds_multiplayer clone | https://github.com/AmfiLogos/ds_multiplayer. Gives the headers (`src/`), `sdk/dsl_sdk.h` and the Windows build. `build.sh` clones it into `build/ds_multiplayer` when missing. | `MP_REPO` |
| DS wiki site source (optional) | The DPS check picture of Test Fights (`assets/img/tour/sg-dps-map.png`, game art: never in git) is copied into the mod as `art/dps_check.png`. Missing = the card shows without it. | `DS_WIKI_SITE` (default `dungeonsettlersdb/site` in your home folder) |
| Multiplayer pin | The ds_multiplayer commit to build against: first line of `mp_pin.txt`. `build.sh` exports it read only into `build/mp/<sha>/`. | `MP_PIN` |
| `multiplayer.dylib` (Mac only, required) | The Mac multiplayer module to link `custom-modes-mp.dylib` against. Only its exported names are used. Take `mods/multiplayer/multiplayer.dylib` from a ds_multiplayer release or a mod loader build, or `build/dsrig.dylib` of a ds_multiplayer build. | `MP_DYLIB` |
| Loader SDK | Folder with `dsl_sdk.h`. Default: `sdk/` of the pinned tree. | `SDK` |
| Windows game folder (Windows only) | `win/25846389` of ds_multiplayer: copies of the Windows game files of Steam build 25846389 and the Il2CppDumper dump. ds_multiplayer `BUILDING.md` says how to make it. | `MP_GAME` (default `$MP_REPO/win/25846389`) |
| Mac dump (Windows only) | ds_multiplayer `win/win_map.py` also reads the Mac dump and the Mac GameAssembly arm64 slice. See ds_multiplayer `BUILDING.md`. | none (fixed paths in those scripts) |

The supported game is Dungeon Settlers, Steam build 25846389 (`mod.json`).

## 3. Build

Run each command from the repo root.

- Both platforms: `MP_DYLIB=<multiplayer.dylib> zsh build.sh`
- Mac only: `MP_DYLIB=<multiplayer.dylib> zsh build.sh mac`
- Windows only: `zsh build.sh win`

Outputs:
- `build/mac/custom-modes/`: `custom-modes.dylib` (solo part), `custom-modes-mp.dylib` (multiplayer part), `mod.json`.
- `build/win/custom-modes/`: `custom-modes.dll`, `custom-modes-mp.dll`, `mod.json`.

`build.sh` prints the sha256 of each built file. The Mac files are the same bytes as the mod loader build.
The Windows DLLs differ by the build time stamp and the build folder in QuickJS `__FILE__` strings.

`build_module.sh` is the Mac step alone. ds_multiplayer `src/build.sh` and the mod loader `loader/build.sh` call it with
`OUT`, `SDK`, `MP_SRC` (multiplayer headers) and `MP_DYLIB` set.

Each folder with a `sources.txt` (solo part) or `sources_mp.txt` (multiplayer part) adds its `.c` files to the build.

## 4. Tests

The test scripts look for a ds_multiplayer checkout. Set `MP_ROOT=<your ds_multiplayer checkout>`.
They write into that checkout (`build_bld/`, `build_c4/`, `build/units/`).

- PvP core: `make -C pvp/pvp_test test`
- My Units, links and card: `MP_ROOT=<checkout> make -C units/test test` (`NET=1` adds the live short links on dungeonsettlers.wiki).
- Unit Builder vs the wiki builder (Node.js): `MP_ROOT=<checkout> zsh builder/build.sh`, then
  `MP_ROOT=<checkout> zsh builder/test/run_eq.sh [count] [seed] <wiki site>/builder`.
- Test Fights save generator vs the wiki (Node.js): `MP_ROOT=<checkout> zsh fights/build.sh`, then
  `MP_ROOT=<checkout> zsh fights/test/run_eq.sh <wiki site>`.
  `fights/test/fetch_blobs.sh` fetches live gallery builds for the blob check.

The wiki tests need the source of the Dungeon Settlers wiki site (its `site/` folder). Without the argument they use
`dungeonsettlersdb/site` in your home folder.
