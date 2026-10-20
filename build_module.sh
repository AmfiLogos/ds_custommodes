#!/bin/zsh
# Build Units Laboratory (mod id custom-modes; PLAN_V13 section 4 "fixed interfaces"). Output in $OUT (default ../build):
#   custom-modes.dylib      the module: cm_module.c + every custom/<folder>/sources.txt; no link to multiplayer
#                           (title home, My Units, Gallery, Test Fights, Unit Builder). Foreign Expedition and PvP are in
#                           the multiplayer mod since the split (owner 2026-10-20): there is no custom-modes-mp.dylib.
#   mod.json                (with OUT given by the loader build: its mods/custom-modes folder)
# Folder lists: one .c path per line, relative to custom/ ("#" lines and blank lines skipped). Each folder that has a list
# is also an include folder. Lane units owns this script; other lanes edit only their own lists (XM reads the same lists).
# Env: SDK = folder with dsl_sdk.h (default ../sdk), MP_SRC = multiplayer headers (default ../src; mp_api.h for the test
#      commands). MP_DYLIB is not used any more (nothing links multiplayer).
# Called by src/build.sh (rig: next to dsrig.dylib) and by ~/ds_launcher/loader/build.sh (OUT=build/mods/custom-modes).
set -e; H="${0:A:h}"; cd "$H"
OUT=${OUT:-../build}; OUT=${OUT:A}; mkdir -p $OUT
SDK=${SDK:-../sdk}; SDK=${SDK:A}; MP_SRC=${MP_SRC:-../src}; MP_SRC=${MP_SRC:A}
SOLO=(); INC=()
for d in */(N/); do
  d=${d%/}
  [[ -f $d/sources.txt ]] && INC+=(-I$d)
  [[ -f $d/sources.txt ]] && SOLO+=(${(f)"$(grep -v '^[[:space:]]*\(#\|$\)' $d/sources.txt || true)"})
  [[ -f $d/sources_mp.txt ]] && { echo "custom-modes: $d/sources_mp.txt: the multiplayer part moved to the multiplayer mod" >&2; exit 1; }
done
SOLO=(${(u)SOLO})                                       # a file listed by two folders is built once
for f in $SOLO; do [[ -f $f ]] || { echo "custom-modes: listed source $f not found" >&2; exit 1; }; done
CF=(-arch arm64 -mmacosx-version-min=12.0 -O2 -fwrapv -Wall -Wno-unused-function -D_DARWIN_C_SOURCE -I$SDK -I. $INC -I$MP_SRC)   # its own headers before multiplayer's (same names: fe_json.h ...)
# -fwrapv: QuickJS (custom/fights) needs wrapping signed overflow; harmless for the rest
# solo: AppKit / CoreFoundation for the clipboard (units/mu_clip.c), CFNetwork + zlib for wiki short links
nice -n 19 clang $CF -dynamiclib -install_name @rpath/custom-modes.dylib -o $OUT/custom-modes.dylib.tmp cm_module.c $SOLO \
  -framework CoreFoundation -framework CFNetwork -lobjc -lz -Wl,-U,_cm_builder_open -Wl,-U,_cm_gallery_pick
codesign -f -s - $OUT/custom-modes.dylib.tmp 2>/dev/null; mv -f $OUT/custom-modes.dylib.tmp $OUT/custom-modes.dylib
if [[ -e $OUT/custom-modes-mp.dylib ]]; then T=$HOME/ToDelete_$(date +%F); mkdir -p $T; mv -f $OUT/custom-modes-mp.dylib $T/custom-modes-mp.dylib.$(date +%H%M%S); fi   # gone since the split (a stale copy would be shipped)
if [[ ${OUT:t} == custom-modes ]]; then
  cp mod.json $OUT/mod.json
  # the language files (lang/<code>.json, read by home/cm_i18n.c): a subfolder, never in mod.json "files" (not hashed)
  if [[ -d lang ]]; then mkdir -p $OUT/lang; for f in lang/*.json(N); do [[ ${f:t} == en.json ]] || cp $f $OUT/lang/; done; fi
  # pictures the windows show (art/, read by path): a subfolder too, never in mod.json "files". Game art stays out of
  # git (.gitignore): it is copied at build time from the wiki site source (DS_WIKI_SITE, default dungeonsettlersdb/site
  # in the home folder). Missing = the card shows without its picture.
  W=${DS_WIKI_SITE:-$HOME/dungeonsettlersdb/site}
  if [[ -f $W/assets/img/tour/sg-dps-map.png ]]; then mkdir -p $OUT/art; cp $W/assets/img/tour/sg-dps-map.png $OUT/art/dps_check.png
  else echo "custom-modes: no DPS check picture (DS_WIKI_SITE/assets/img/tour/sg-dps-map.png): the card shows without it" >&2; fi
fi
echo "built $OUT/custom-modes.dylib (${#SOLO} sources)"
