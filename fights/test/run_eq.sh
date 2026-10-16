#!/bin/zsh
# Byte-for-byte proof: the wiki's save generator in QuickJS (sg_cli) == the same wiki files on node/V8 (ref_node.js)
# for every case in cases.json (every fight type, difficulty, supplies, kits, team names, gallery builds) and for
# the gallery blob decode. Needs: build.sh run first; the wiki source (default ~/dungeonsettlersdb/site).
# usage: custom/fights/test/run_eq.sh [wiki site dir] [blobs.json]
#   env BUILDER / SAVES override <site>/builder and <site>/assets/saves (e.g. an sg_cache root: <root>/builder, <root>/saves)
# blobs.json = {"<gallery id>": "<blob>"} from /api/gallery?id=<id>&full=1 (fetched by fetch_blobs.sh; not committed)
set -e
# R = the ds_multiplayer root (headers src/ sdk/, build output): MP_ROOT, else the parent folder when this repo sits in it as custom/, else ~/ds_multiplayer
H=${0:A:h}; R=${MP_ROOT:-${H:h:h:h}}; [[ -n $MP_ROOT || -f $R/src/mp_api.h ]] || R=$HOME/ds_multiplayer; B=$R/build_c4
SITE=${1:-$HOME/dungeonsettlersdb/site}
BLOBS=${2:-$B/blobs.json}
BUILDER=${BUILDER:-$SITE/builder}; SAVES=${SAVES:-$SITE/assets/saves}
OWNER=${OWNER:-$HOME/dungeonsettlersdb/pipeline/savegen/testdata/owner_team.json}
export TZ=UTC DET_SEED=${DET_SEED:-12345} DET_NOW=${DET_NOW:-1760000000000} DET_JS=$H/det_prelude.js
REF=$B/eq_ref; QJ=$B/eq_qjs
for d in $REF $QJ; do [[ -d $d ]] && mv $d $B/old_$(basename $d)_$(date +%s); mkdir -p $d; done
# 1. decode the gallery blobs with the reference, to build the gallery build sets
echo '[]' > $B/eq_nocases.json; echo '{}' > $B/eq_nosets.json
node $H/ref_node.js $BUILDER $SAVES $REF $B/eq_nocases.json $B/eq_nosets.json $BLOBS
python3 - $REF $OWNER $BLOBS $B/eq_builds.json <<'EOF'
import json, sys
ref, owner, blobs, out = sys.argv[1:5]
B = json.load(open(blobs))
team, solo = [], []
for k, v in B.items():
    d = json.load(open(f'{ref}/decode_{k}.json'))
    (solo if v.startswith('b:') else team).extend(d)
json.dump({'owner': json.load(open(owner)), 'gal': team, 'gal_solo': solo[:4]}, open(out, 'w'))
print('build sets: owner, gal', len(team), 'gal_solo', len(solo[:4]))
EOF
# 2. every case on both engines (sg_cli rebuilt first: never an old binary)
${H:h}/build.sh
node $H/ref_node.js $BUILDER $SAVES $REF $H/cases.json $B/eq_builds.json $BLOBS
$B/sg_cli $BUILDER $SAVES $QJ $H/cases.json $B/eq_builds.json $BLOBS
# 3. compare every file
same=0; diff=0
for f in $REF/*; do
  n=${f:t}
  if [[ ! -f $QJ/$n ]]; then echo "MISSING in quickjs: $n"; diff=$((diff+1)); continue; fi
  if cmp -s $f $QJ/$n; then same=$((same+1)); else echo "DIFFERENT: $n"; diff=$((diff+1)); fi
done
for f in $QJ/*; do n=${f:t}; [[ $n == timing.txt || -f $REF/$n ]] || { echo "EXTRA in quickjs: $n"; diff=$((diff+1)); }; done
echo "files identical: $same, different/missing: $diff"
# every case but the error_* ones must give a save (an error on both sides is "identical" too)
want=$(python3 -I -c 'import json,sys; print(sum(not c["id"].startswith("error_") for c in json.load(open(sys.argv[1]))))' $H/cases.json)
saves=($REF/*.save.json(N)); errs=($REF/*.error.txt(N))
echo "saves: $#saves of $want ok, errors (same text both sides): $#errs"
cat $QJ/timing.txt | head -3
[[ $diff == 0 && $#saves == $want ]]
