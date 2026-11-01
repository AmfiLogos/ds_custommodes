#!/bin/zsh
# mu_save tests: real saves (read only copies) + the round trip through the save writer. ASan + UBSan.
#   custom/units/test/save/run.sh            (from anywhere; needs node + ~/dungeonsettlersdb for the round-trip builds)
set -e
setopt pipefail
H=${0:A:h}; C=${H:h:h:h}; T=$H/tmp   # C = this repo (Custom Modes)
mkdir -p $T $H/old
[ ! -d $T/run ] || mv $T/run $H/old/run_$(date +%s)
mkdir -p $T/run
nice -n 19 cc -O1 -g -Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers -fsanitize=address,undefined \
  -I $C/units -I $C -I $C/fe -o $T/save_test $H/save_test.c \
  $C/units/{mu_save.c,mu_worlds.c,myunits.c,pvp_wiki.c,pvp_build.c,mu_card.c,wk_export.c} $C/fe/{fe_json.c,fe_save.c} $C/units/test/dsi_impl.c -lz -lm
export ASAN_OPTIONS=halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1
B=~/DungeonSettlers_SaveBackups/2026-10-11-1827/Saves
I=~/ds_multiplayer/instances/A/data/Saves
st=0
# the rig copies' test saves come and go: the ones present (the save backup's are always there)
SAVES=($B/*.json(N) $I/WikiGolem__*.json(N) $I/MoonlightBrigade__AutoSave_fe5ea00005a54b06a374b2810b9a732d_0.json(N) \
  $I/MoonlightNormal__*.json(N) $I/MPFE_0c0c23a7.json(N))
echo "${#SAVES} real saves"
$T/save_test list $SAVES > $T/run/list.txt || st=1
# World units + snapshot (a copy of a real save; the library is a test file under tmp/run)
$T/save_test worlds $T/run/worlds $B/ElvenExpedition__AutoSave_f816ebc6195c432c9fff754b75ee3738_0.json > $T/run/worlds.txt || st=1
# the builder's own import of each real save vs the reader
for f in $SAVES; do
  k=${${f:t}%.json}
  node $H/builder_import.js $f > $T/run/ref_$k.jsonl
  $T/save_test ref $f $T/run/ref_$k.jsonl > $T/run/refout_$k.txt || st=1
done
# round trip: random builds the builder itself made legal (validate), + the launcher's library fixture
N=${N:-120}
for seed in 5 11 424242; do
  (cd $C/units/test/node && node builder_ref.js gen $N $seed | node builder_ref.js validate) > $T/run/builds_$seed.jsonl
  $T/save_test rt $T/run/builds_$seed.jsonl $T/run > $T/run/rt_$seed.txt || st=1
done
python3 -I -c 'import json,sys; [print(json.dumps(u["build"])) for u in json.load(open(sys.argv[1]))["units"]]' \
  $C/units/test/data/launcher_library.json > $T/run/builds_library.jsonl
$T/save_test rt $T/run/builds_library.jsonl $T/run > $T/run/rt_library.txt || st=1
# "Open in Wiki Builder" (wk_export.c): every real save's units + the random builds -> C team codes, which the builder
# itself must decode exactly as its own encodeTeam of the same builds (export_check.js)
EXPORT_OUT=$T/run/export.jsonl $T/save_test export $SAVES > $T/run/export.txt || st=1
EXPORT_OUT=$T/run/export.jsonl $T/save_test enc $T/run/builds_*.jsonl > $T/run/enc.txt || st=1
node $H/export_check.js $T/run/export.jsonl > $T/run/export_check.txt || st=1
# the export's live-data check (wkx_codec_check): the wiki's builder tables == ours (else no link goes out); a renamed item
# key and a changed bonus range must be refused (the size-only fingerprint misses both)
CODEC=${DSB_DIR:-$HOME/dungeonsettlersdb/site/builder}/../dbdata/gallery_codec.js
$T/save_test codec $CODEC 1 > $T/run/codec.txt || st=1
python3 -I -c 'import sys; s=open(sys.argv[1]).read(); open(sys.argv[2],"w").write(s.replace("\"ITEM_WoodClub\"","\"ITEM_WoodClubX\"",1))' $CODEC $T/run/codec_key.js
python3 -I -c 'import sys,re; s=open(sys.argv[1]).read(); i=s.index("ITEM_SilverRing"); j=s.index("5.0,10.0",i); open(sys.argv[2],"w").write(s[:j]+"5.0,11.0"+s[j+8:])' $CODEC $T/run/codec_range.js
$T/save_test codec $T/run/codec_key.js 0 > $T/run/codec_key.txt || st=1
$T/save_test codec $T/run/codec_range.js 0 > $T/run/codec_range.txt || st=1
grep -h "^==\|passed\|FAIL\|ERROR\|Sanitizer\|runtime error" $T/run/*.txt | head -60
exit $st
