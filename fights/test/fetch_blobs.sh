#!/bin/zsh
# Live gallery blobs for run_eq.sh: the first 3 items of each list (teams/characters x popular/recent), with their
# stored builds (/api/gallery?id=<id>&full=1). Output: build_c4/blobs.json (git-ignored: other people's builds).
set -e
# R = the ds_multiplayer root (headers src/ sdk/, build output): MP_ROOT, else the parent folder when this repo sits in it as custom/, else ~/ds_multiplayer
H=${0:A:h}; R=${MP_ROOT:-${H:h:h:h}}; [[ -n $MP_ROOT || -f $R/src/mp_api.h ]] || R=$HOME/ds_multiplayer; B=$R/build_c4; mkdir -p $B
python3 - $B/blobs.json <<'PY'
import json, subprocess, sys
def get(u): return json.loads(subprocess.run(['curl', '-sf', '-m', '20', u], capture_output=True, check=True).stdout)
W = 'https://dungeonsettlers.wiki/api/gallery'
ids = []
for kind in ('team', 'char'):
    for sort in ('popular', 'recent'):
        ids += [it['id'] for it in get(f'{W}?kind={kind}&sort={sort}')['items'][:3]]
blobs = {}
for i in dict.fromkeys(ids):
    j = get(f'{W}?id={i}&full=1')
    if 'blob' in j: blobs[i] = j['blob']
json.dump(blobs, open(sys.argv[1], 'w'))
print(len(blobs), 'blobs ->', sys.argv[1])
PY
