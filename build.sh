#!/bin/zsh
# Standalone build of Custom Modes for the mod loader: mac-arm64 and win-x64. The Mac files are byte-identical to the
# loader build's; the Windows DLLs differ by their build time stamp and the build folder in quickjs' __FILE__ strings.
# It needs the multiplayer mod's headers (src/: mp_api.h for the test commands), dsl_sdk.h (sdk/) and, for Windows, its
# build tools (win/build_win.sh translates the game addresses and builds custom-modes.dll; it links nothing of multiplayer). From ONE pinned
# ds_multiplayer commit (mp_pin.txt), exported read-only with `git archive` into build/mp/<sha>/.
#   build.sh [mac|win|all]       output: build/mac/custom-modes/ and build/win/custom-modes/ (default all)
# Env: MP_REPO  = a ds_multiplayer clone (default: build/ds_multiplayer, cloned from GitHub when missing)
#      MP_PIN   = ds_multiplayer commit (default: first line of mp_pin.txt)
#      SDK      = folder with dsl_sdk.h (default: the pinned tree's sdk/)
#      MP_GAME  = Windows only: the game files + dump folder win/25846389 the Windows build reads (never in git;
#                 default $MP_REPO/win/25846389). Needs ~/tools/llvm-mingw.
set -e; H="${0:A:h}"; cd "$H"; WHAT=${1:-all}
MP_REPO=${MP_REPO:-$H/build/ds_multiplayer}
[[ -d $MP_REPO/.git ]] || git clone -q https://github.com/AmfiLogos/ds_multiplayer $MP_REPO
PIN=${MP_PIN:-$(head -1 mp_pin.txt)}
SHA=$(git -C $MP_REPO rev-parse --verify -q "$PIN^{commit}" || { git -C $MP_REPO fetch -q origin && git -C $MP_REPO rev-parse --verify "$PIN^{commit}"; }) \
  || { echo "mp pin $PIN not found in $MP_REPO" >&2; exit 1; }
T=$H/build/mp/$SHA
if [[ ! -f $T/win/build_win.sh ]]; then mkdir -p $T.tmp; git -C $MP_REPO archive $SHA src sdk win | tar -x -C $T.tmp; mv $T.tmp $T; fi
SDK=${SDK:-$T/sdk}; SDK=${SDK:A}
# the kit's templates (window_std, card_toggle, rules_own: ds_modloader d529f52+; panel_size: kit 0.1.29) are used: an older
# dsl_sdk.h copy stops here
grep -q dsl_panel_size $SDK/dsl_sdk.h || { echo "dsl_sdk.h in $SDK has no kit templates (dsl_panel_size): build with SDK=<ds_modloader>/loader/sdk" >&2; exit 1; }

if [[ $WHAT == (mac|all) ]]; then
  O=$H/build/mac/custom-modes; mkdir -p $O
  OUT=$O SDK=$SDK MP_SRC=$T/src zsh $H/build_module.sh
  for f in $O/*.dylib; do strip -S $f 2>/dev/null; codesign -f -s - $f 2>/dev/null; done   # as the loader's build_mods.sh ships them
fi
if [[ $WHAT == (win|all) ]]; then
  G=${MP_GAME:-$MP_REPO/win/25846389}
  [[ -e $T/win/25846389 ]] || ln -s ${G:A} $T/win/25846389
  # this folder (minus build/, which holds the pinned tree) replaces the pinned tree's custom/ (the Windows tools scan it all)
  mkdir -p $H/build/old; [[ -d $T/custom ]] && mv $T/custom $H/build/old/custom-$(date +%Y%m%d-%H%M%S)
  rsync -a --exclude /build/ --exclude /.git/ $H/ $T/custom/
  # the multiplayer module folder this also writes is not used here (DSL_MODJSON only has to be a mod.json)
  DSMP_TARGET=module DSL_SDK=$SDK DSL_MODJSON=$H/mod.json nice -n 19 zsh $T/win/build_win.sh | tail -3
  O=$H/build/win/custom-modes; mkdir -p $O
  for f in custom-modes.dll; do cp $T/win/build/mods/custom-modes/$f $O/$f.tmp; mv -f $O/$f.tmp $O/$f
    ~/tools/llvm-mingw/bin/llvm-strip --strip-debug $O/$f; done
  cp mod.json $O/mod.json
  if [[ -d lang ]]; then mkdir -p $O/lang; for f in lang/*.json(N); do [[ ${f:t} == en.json ]] || cp $f $O/lang/; done; fi
  W=${DS_WIKI_SITE:-$HOME/dungeonsettlersdb/site}             # the DPS check picture (game art, never in git): as build_module.sh
  if [[ -f $W/assets/img/tour/sg-dps-map.png ]]; then mkdir -p $O/art; cp $W/assets/img/tour/sg-dps-map.png $O/art/dps_check.png; fi
fi
for f in $H/build/(mac|win)/custom-modes/*.(dylib|dll)(N); do shasum -a 256 $f | sed "s|  $H/| |"; done
