#!/usr/bin/env python3
"""My Units > World units > "Open in Wiki Builder" (owner 2026-11-01, PLAN_v013 item 16) in one hidden loader test copy
(background-only, small window, muted, notch fix off; never the owner's game, saves, library or screen). The copy's game
saves folder holds a COPY of a real save; the wiki is a local folder (DSCM_WIKI_DIR: dbdata/gallery_codec.js, the
builder's tables as the wiki serves them). The loader's quiet mode refuses the browser open in a test copy and logs it.
Fails when (the breaks this catches):
  - the link the button opens does not load EXACTLY the ticked units of that save in the wiki builder: the builder itself
    (builder.js in node) decodes the link's team code and compares each unit with its OWN save import of that save
    (race, background, level, talents, traits, skills, trees, inscriptions, gear in its slots, rolls, majors, souls,
    elixirs, name; the two by-design differences of units/mu_save.c are left out: portrait from the save's UnitPortraitID
    and the acquired traits the builder's import drops) and with its own computeBuild stats;
  - the link is not the wiki contract (dungeonsettlers.wiki/builder?team=..&from=game&pub=1|0&tn=<world>&gx=<16 hex>), or
    the browser open is not tried with exactly that link;
  - the gallery switch: pub=1 by default; after the switch is flipped the link says pub=0 and the choice is kept on disk;
  - the wiki's tables differ from this version's (a renamed key): the link must NOT go out (no open tried, the window
    says so);
  - the save copy, the owner's library file or a save changed.
  export_test.py   env: HM_BUILD = loader build folder (dsload.dylib with the dev key, mods/multiplayer, mods/custom-modes)
                        LD_ROOT, WT_SAVE as worlds_test.py; DSB_DIR = the wiki builder folder (default
                        ~/dungeonsettlersdb/site/builder; its ../dbdata/gallery_codec.js is served)
Output: WT_OUT (default <HM_BUILD>/../test_env/export/<time>): results.json, dsload.log."""
import base64, json, os, re, shutil, subprocess, sys, threading, time, traceback, urllib.parse

HERE = os.path.dirname(os.path.abspath(__file__))
os.environ.setdefault('WT_OUT', os.path.join(os.path.dirname(os.environ.get('HM_BUILD', os.path.expanduser('~/ds_launcher/build'))),
                                              'test_env', 'export', time.strftime('%Y%m%d-%H%M%S')))
sys.path.insert(0, HERE)
import worlds_test as W  # noqa: E402  (the copy, kit, driver requests and the test library of the World units test)
from worlds_test import HP, lrig, log, check  # noqa: E402

X = 'ULX'; HP.H = HP.G = X
OUT = W.OUT
WIKI = os.path.join(OUT, 'wiki')
DSB = os.path.expanduser(os.environ.get('DSB_DIR', '~/dungeonsettlersdb/site/builder'))
CODEC = os.path.join(DSB, '..', 'dbdata', 'gallery_codec.js')
NODE_DIR = os.path.join(HERE, 'node')


def dsload():
    p = os.path.join(lrig.idir(X), 'dsload.log')
    return open(p, encoding='utf-8', errors='replace').read() if os.path.exists(p) else ''


def wait_log(pat, after, timeout=30):
    t0 = time.time()
    while time.time() - t0 < timeout:
        m = re.findall(pat, dsload()[after:])
        if m:
            return m
        time.sleep(0.5)
    return []


def builder_check(url, save, guids):
    """the builder (node) decodes the link's code and compares it with its own import of the save: [problems]"""
    js = r'''
const path = require('path'), fs = require('fs'), zlib = require('zlib');
const { load } = require(path.join(process.argv[1], 'builder_ref.js'));
const c = load(), code = process.argv[2], save = JSON.parse(fs.readFileSync(process.argv[3], 'utf8')), guids = JSON.parse(process.argv[4]);
function dec(code) { const b = c.dsbB64ToBytes(code); let d = null; try { d = c._decodeBlob(b); } catch (e) {}
  if (d) return d; try { return c._decodeBlob(Array.from(zlib.inflateRawSync(Buffer.from(b)))); } catch (e) { return null; } }
(async () => {
  const bad = [], mine = dec(code);
  if (!mine) { console.log(JSON.stringify(['the builder cannot read the link']));  process.exit(0); }
  if (mine.warn) bad.push('the builder says the link was made with other wiki data');
  const imp = {}; for (const u of c.dsbExtractSave(save, null).units) imp[u.guid] = c.dsbSaveUnitToBuild(u);
  const want = guids.map(g => imp[g]);
  if (want.some(x => !x)) bad.push('a ticked unit is not in the builder import');
  const ref = dec(await c.encodeTeam(want.filter(Boolean)));
  if (mine.units.length !== guids.length) bad.push(`${mine.units.length} units in the link, ${guids.length} ticked`);
  // by design (units/mu_save.c, test/save/NOTES.md): the portrait is the save's own UnitPortraitID, and the acquired traits
  // the builder's import drops are kept; names: the game's shown name vs the builder's English table
  const strip = u => { const o = JSON.parse(JSON.stringify(u)); delete o.portrait; delete o.acqInds; delete o.name; return o; };
  for (let i = 0; i < Math.min(mine.units.length, ref.units.length); i++) {
    const a = JSON.stringify(strip(mine.units[i])), b = JSON.stringify(strip(ref.units[i]));
    if (a !== b) bad.push(`unit ${i} (${guids[i]}): link ${a.slice(0, 400)} | builder import ${b.slice(0, 400)}`);
    const sa = JSON.stringify(c.computeBuild(mine.units[i]).st), sb = JSON.stringify(c.computeBuild(ref.units[i]).st);
    if (sa !== sb) bad.push(`unit ${i} stats: link ${sa} | builder import ${sb}`);
  }
  console.log(JSON.stringify(bad)); process.stdout.write('', () => process.exit(0));
})();
'''
    q = urllib.parse.urlparse(url); code = urllib.parse.parse_qs(q.query)['team'][0]
    r = subprocess.run(['node', '-e', js, NODE_DIR, code, save, json.dumps(guids)], capture_output=True, text=True, timeout=120,
                       env=dict(os.environ, DSB_DIR=DSB))
    try:
        return json.loads(r.stdout.strip().splitlines()[-1])
    except (ValueError, IndexError):
        return ['node failed: ' + (r.stderr or r.stdout)[-400:]]


def export(label, after):
    """press the button; the link the window built and whether the browser open was tried with it"""
    W.click('mu:export', 1)
    end = wait_log(r'my units: export (browser opened|REFUSED[^\n]*|failed[^\n]*)', after, 30)
    urls = re.findall(r'my units: export url (\S+)', dsload()[after:])
    opens = [o.rstrip(',') for o in re.findall(r'quiet: outgoing open blocked \(posix_spawn\): /usr/bin/open (\S+)', dsload()[after:])]
    log(label, end, urls[-1][:160] if urls else None, len(opens))
    return (urls[-1] if urls else ''), opens, (end[-1] if end else '')


def run(save_copy):
    s = W.mu(); worlds = s.get('worlds') or []
    sv = os.path.splitext(os.path.basename(save_copy))[0]
    gi = next(i for i, w in enumerate(worlds) if w.split('|')[2] == sv)
    clan = worlds[gi].split('|')[0]
    W.click(f'mu:world{gi}', 4); s = W.mu(); wunits = s.get('wunits') or []
    check('the save copy\'s world shows its units', len(wunits) >= 3, wunits)
    check('the gallery switch starts on Public', s.get('x_pub') == 1, s.get('x_pub'))
    W.click('mu:tick0'); W.click('mu:tick2'); s = W.mu()
    check('2 units ticked', s.get('wticked') == 2, s.get('wticked'))
    ticked = [wunits[0], wunits[2]]

    # 1. public: the link loads exactly the ticked units, the open is tried with it
    a = len(dsload()); url, opens, end = export('export public', a)
    q = urllib.parse.urlparse(url); p = urllib.parse.parse_qs(q.query)
    check('the link is the wiki contract (builder, from=game, pub=1, tn = the world, gx = 16 hex)',
          q.scheme == 'https' and q.netloc == 'dungeonsettlers.wiki' and q.path in ('/builder', '/builder/') and p.get('from') == ['game']
          and p.get('pub') == ['1'] and p.get('tn') == [clan] and re.fullmatch(r'[0-9a-f]{16}', (p.get('gx') or [''])[0] or '') is not None,
          url[:300])
    check('the browser open was tried with exactly that link (refused + logged by the test copy\'s quiet mode)', bool(opens) and opens[-1] == url,
          (opens[-1][:120] if opens else None, end))
    bad = builder_check(url, save_copy, ticked) if url else ['no link']
    check('the wiki builder loads EXACTLY the 2 ticked units of the save (its own save import + computeBuild)', not bad, bad[:3])

    # 2. the switch: private, kept on disk, the link says pub=0; same units = same gx
    W.click('mu:export_pub', 1); s = W.mu()
    pref = os.path.join(os.path.dirname(W.LIB), 'wiki_export.txt')
    check('the switch flips to Private and the choice is kept', s.get('x_pub') == 0 and os.path.exists(pref) and 'public=0' in open(pref).read(),
          (s.get('x_pub'), os.path.exists(pref)))
    a = len(dsload()); url2, opens2, _ = export('export private', a)
    p2 = urllib.parse.parse_qs(urllib.parse.urlparse(url2).query)
    check('private: the link says pub=0, same units = same team code and gx', p2.get('pub') == ['0'] and p2.get('team') == p.get('team') and p2.get('gx') == p.get('gx'),
          url2[:200])

    # 3. none ticked and the save holds <= 8 units: all of them
    for k in (0, 2):
        W.click(f'mu:tick{k}')
    if len(wunits) <= 8:
        a = len(dsload()); url3, _, _ = export('export all', a)
        bad = builder_check(url3, save_copy, wunits) if url3 else ['no link']
        check(f'none ticked: all {len(wunits)} units of the save, exactly', not bad, bad[:3])

    # 4. the wiki's tables changed (one renamed item key): no link goes out
    good = open(os.path.join(WIKI, 'dbdata', 'gallery_codec.js')).read()
    open(os.path.join(WIKI, 'dbdata', 'gallery_codec.js'), 'w').write(good.replace('"ITEM_WoodClub"', '"ITEM_WoodClubX"', 1))
    W.click('mu:tick0')
    a = len(dsload()); url4, opens4, end4 = export('export outdated', a)
    s = W.mu()
    check('the wiki\'s tables differ: REFUSED, no browser open tried, the window says to update', 'REFUSED' in end4 and not opens4 and 'Update' in (s.get('msg') or ''),
          (end4, len(opens4), s.get('msg')))
    open(os.path.join(WIKI, 'dbdata', 'gallery_codec.js'), 'w').write(good)


def main():
    o0 = W.fstate(W.OWNER_LIB); s0 = W.fstate(W.SAVE)
    g = threading.Thread(target=HP.guard, daemon=True); g.start()
    res = {}
    try:
        W.make_library()
        os.makedirs(W.GAME_SAVES, exist_ok=True)
        cp = os.path.join(W.GAME_SAVES, os.path.basename(W.SAVE)); shutil.copy2(W.SAVE, cp); c0 = W.fstate(cp)
        os.makedirs(os.path.join(WIKI, 'dbdata'), exist_ok=True)
        shutil.copy2(CODEC, os.path.join(WIKI, 'dbdata', 'gallery_codec.js'))
        HP.make_copy(X)
        pk = {'custom-modes': W.zip_dir(os.path.join(HP.BUILD, 'mods', 'custom-modes'), 'custom-modes'), 'font-probe': W.probe_pkg()}
        HP.kit(X, pk, ['custom-modes', 'font-probe'], ['multiplayer', 'custom-modes', 'font-probe'])
        crash = os.path.join(OUT, 'crash'); os.makedirs(crash, exist_ok=True); open(os.path.join(crash, 'answer'), 'w').write('no')
        HP.room_for(1)
        HP.start(X, 'http://127.0.0.1:9', {'DSL_UNITS_TEST': W.DRV, 'DSL_CRASHREP_TEST': crash, 'DSMP_UNITS_LIB': W.LIB,
                                           'DSML_GAME_SAVES': W.GAME_SAVES, 'DSCM_WIKI_DIR': WIKI})
        lrig.wait_for(X, lambda s: s.get('scene') == 1 and not s.get('busy'), 'title', timeout=1500); time.sleep(6)
        ok = W.click('title:' + (W.by_index(W.column(), W.COL_LAB) or 'Units Laboratory'), 3) and W.click('home:My Units', 4)
        check('Units Laboratory > My Units opened on the TEST library', ok and W.mu().get('open') == 1 and W.mu().get('lib') == W.LIB)
        run(cp)
        check('the real save copy is unchanged', W.fstate(cp) == c0)
    except BaseException as e:
        log(traceback.format_exc())
        check('run finished', False, repr(e))
    finally:
        check('never a visible window on a display, never frontmost (guard)', not HP.violations, HP.violations[:3])
        HP.stop_guard.set()
        try:
            HP.stop(X)
        except Exception as e:
            log('stop', repr(e))
        check("the owner's library file is unchanged (sha256 + time)", W.fstate(W.OWNER_LIB) == o0)
        check('the source save is unchanged', W.fstate(W.SAVE) == s0)
        for f in ('dsload.log', 'player.log'):
            p = os.path.join(lrig.idir(X), f)
            if os.path.exists(p):
                shutil.copy2(p, os.path.join(OUT, f))
        res.update({'checks': HP.checks, 'all_pass': all(k['ok'] for k in HP.checks) and bool(HP.checks)})
        json.dump(res, open(os.path.join(OUT, 'results.json'), 'w'), indent=1, default=str)
        log('RESULT', 'PASS' if res['all_pass'] else 'FAIL', f"{sum(k['ok'] for k in HP.checks)}/{len(HP.checks)}", os.path.join(OUT, 'results.json'))
    sys.exit(0 if res.get('all_pass') else 1)


if __name__ == '__main__':
    main()
