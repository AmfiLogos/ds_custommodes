#!/bin/zsh
# Test Fights' World units: units picked from a save enter the fight with the save's level, gear, skills and skill bar
# (saveunits_test.c: the game's Start path through the wiki's save generator, the test world read back and compared).
# The saves are COPIED first; the test only reads the copies. Needs the wiki site source (builder scripts + templates).
# usage: fights/test/run_saveunits.sh [wiki site dir] [SAVE...]   (env MP_ROOT as fights/build.sh)
#   default saves: the 2026-10-11 save backup's two worlds (a real clan) + the wiki's base save of build 25846389
set -e
H=${0:A:h}; C=${H:h:h}; R=${MP_ROOT:-${C:h}}; [[ -n $MP_ROOT || -f $R/src/mp_api.h ]] || R=$HOME/ds_multiplayer; B=$R/build_c4
SITE=${1:-$HOME/dungeonsettlersdb/site}; shift $(( $# > 0 ? 1 : 0 ))
SAVES=("$@")
if (( ! $#SAVES )); then
  SAVES=($HOME/DungeonSettlers_SaveBackups/2026-10-11-1827/Saves/*__AutoSave_*.json(N) $SITE/assets/saves/base_25846389.json(N))
fi
nice -n 19 zsh ${H:h}/build.sh > /dev/null
OUT=$B/saveunits/$(date +%Y%m%d-%H%M%S); mkdir -p $OUT/saves
for f in $SAVES; do cp $f $OUT/saves/; done
$B/saveunits_test $SITE/builder $SITE/assets/saves $OUT $OUT/saves/*.json | tee $OUT/result.txt
