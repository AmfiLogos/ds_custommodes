#!/usr/bin/env python3
"""A Test Fight leaves the title exactly as it was (owner 2026-10-25: "when we do a test, we then have the homepage
homescreen changed to the base of the save we imported our units to for the test - shouldn't be the case, it should be
not updated when doing a test"), in one hidden loader test copy (background-only, small window, muted, notch fix off;
never the owner's game, saves, library or screen) with multiplayer + custom-modes + the test module font-probe (the units
test driver, with its "title" read-out). The copy's game saves folder is a test folder holding a COPY of a real save
(DSML_GAME_SAVES), the newest world = the last played one; its data folder starts from the 2026-10-11 save backup's
OutGame.json and UserCampaignLog.json; its My Units library is a test file (DSMP_UNITS_LIB); the wiki is the local site
folder (DSCM_WIKI_DIR).
Fails when (the breaks this catches), after each Test Fight (DPS Check, then a Boss fight on the hardest difficulty) and
the game's own Esc > Quit to title:
  - the title's background is not the real world it showed before the test (the bug: the test world's base);
  - Continue does not point at that real world, or pressing it loads another world;
  - OutGame.json (tutorials, items obtained, Downfall unlock) or SaveHeaderCache.json is not byte-equal to the copy taken
    before the test, or the items obtained the game holds in memory (written with its next record) differ from before;
    UserCampaignLog.json differs in more than PlayerApplicationMinute (the minutes the game was on, which every session
    adds);
  - a test world's save is still in the Saves folder or in the header cache (Load Campaign lists them) on the title;
  - the test world did not load with the test-world mark on its campaign guid (tf_worlds.h: known forever);
  - the real save copy, the source save or the owner's library change, or a window shows on a display.
  title_test.py   env: HM_BUILD = a loader build folder (dsload.dylib, mods/multiplayer, mods/custom-modes)
                       LD_ROOT = a ds_modloader checkout (its loader/test helpers; default ~/ds_launcher)
                       TT_SAVE = the real save to copy (default: the 2026-10-11 save backup's Elven Expedition)
                       TT_WIKI = the wiki folder (default ~/dungeonsettlersdb/site)
                       TT_MODES = the tests to run, comma separated (default dps,boss)
Output: TT_OUT (default <HM_BUILD>/../test_env/title/<time>): results.json, the three files before / after each test."""
import glob, hashlib, json, os, shutil, subprocess, sys, threading, time, traceback, zipfile

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(os.path.dirname(HERE))
LD = os.path.expanduser(os.environ.get('LD_ROOT', '~/ds_launcher'))
sys.path.insert(0, os.path.join(LD, 'loader', 'test'))
import hostmods_pair as HP  # noqa: E402  (copies, kits, start/stop, owner screen guard)
from hostmods_pair import lrig, log, check  # noqa: E402

X = 'TTL'; HP.H = HP.G = X
OUT = os.environ.get('TT_OUT') or os.path.join(os.path.dirname(HP.BUILD), 'test_env', 'title', time.strftime('%Y%m%d-%H%M%S'))
os.makedirs(OUT, exist_ok=True)
DRV = os.path.join(OUT, 'drv'); os.makedirs(os.path.join(DRV, 'cap'), exist_ok=True)
SEED = lrig.SEED_DATA
SAVE = os.environ.get('TT_SAVE') or os.path.join(SEED, 'Saves', 'ElvenExpedition__AutoSave_f816ebc6195c432c9fff754b75ee3738_0.json')
WIKI = os.path.expanduser(os.environ.get('TT_WIKI', '~/dungeonsettlersdb/site'))
MODES = [m for m in os.environ.get('TT_MODES', 'dps,boss').split(',') if m]
OWNER_LIB = os.path.expanduser('~/Library/Application Support/CanOpener/Dungeon Settlers/Mods/units/library.json')
LIB = os.path.join(OUT, 'lib', 'library.json')
GAME_SAVES = os.path.join(OUT, 'game_saves')
DATA = os.path.join(lrig.idir(X), 'data')
COL_LAB = 5                                                   # Units Laboratory's child index in the title column


def fstate(p):
    return (hashlib.sha256(open(p, 'rb').read()).hexdigest(), os.path.getmtime(p)) if os.path.exists(p) else (None, None)


def zip_dir(src, mid):
    zp = os.path.join(OUT, 'pkg', f'{mid}.zip'); os.makedirs(os.path.dirname(zp), exist_ok=True)
    with zipfile.ZipFile(zp, 'w', zipfile.ZIP_DEFLATED) as zf:
        for r, _, fs in os.walk(src):
            for f in sorted(fs):
                if not f.startswith('.'):
                    zf.write(os.path.join(r, f), os.path.join(mid, os.path.relpath(os.path.join(r, f), src)))
    return zp


def probe_pkg():
    """the test driver module (home/test/font_probe) as a mod zip (built here, never shipped)"""
    fp = os.path.join(REPO, 'home', 'test', 'font_probe')
    d = os.path.join(OUT, 'pkg', 'font-probe'); os.makedirs(d, exist_ok=True)
    subprocess.run(['nice', '-n', '19', 'clang', '-arch', 'arm64', '-O2', '-Wall', '-Wno-unused-function', '-I' + os.path.join(LD, 'loader', 'sdk'),
                    '-dynamiclib', '-framework', 'CoreFoundation', '-framework', 'CoreGraphics', '-lobjc', '-install_name', '@rpath/font-probe.dylib',
                    '-o', os.path.join(d, 'font-probe.dylib'), os.path.join(fp, 'font_probe.c')], check=True)
    subprocess.run(['codesign', '-f', '-s', '-', os.path.join(d, 'font-probe.dylib')], capture_output=True)
    shutil.copy2(os.path.join(fp, 'mod.json'), os.path.join(d, 'mod.json'))
    return zip_dir(d, 'font-probe')


seq = [0]
def req(cmd, timeout=30, **kw):
    seq[0] += 1; q = dict(kw, seq=seq[0], cmd=cmd)
    open(os.path.join(DRV, 'req.json.tmp'), 'w', encoding='utf-8').write(json.dumps(q, ensure_ascii=False)); os.replace(os.path.join(DRV, 'req.json.tmp'), os.path.join(DRV, 'req.json'))
    t0 = time.time()
    while time.time() - t0 < timeout:
        try:
            a = json.load(open(os.path.join(DRV, 'resp.json')))
            if a.get('seq') == seq[0]:
                return a
        except (OSError, ValueError):
            pass
        time.sleep(0.2)
    raise RuntimeError(f'no answer to {q}')


def title():
    return req('title').get('title') or {}


def tf():
    return ((req('state').get('cm') or {}).get('fights') or {}).get('tf') or {}


def cmf(**kw):
    """Test Fights' own test call (cm_win.c cmf_test): set {name,index} / type {name,text} / click {name} / state"""
    r = req('call', fn='cmf_test', s=json.dumps(kw, separators=(',', ':')))
    try:
        return json.loads(r.get('out') or '{}')
    except ValueError:
        return {'ok': False, 'raw': (r.get('out') or '')[:300]}


def click(t, wait=1.5):
    r = req('click', target=t); time.sleep(wait)
    log('click', t, r.get('rc'), r.get('text'))
    return r.get('rc') == 0


def column():
    return [c.split('|') for c in (req('state').get('title_column') or '').split(';') if c]


def by_index(rows, i):
    return next((r[0] for r in rows if len(r) > 2 and r[2] == str(i) and r[1] == '1'), None)


def wait(what, pred, timeout=120, every=1.0):
    try:
        return HP.wait(what, pred, timeout, every)
    except RuntimeError as e:
        log(str(e)); return None


def cache_path():
    for p in (os.path.join(DATA, 'SaveHeaderCache.json'), os.path.join(DATA, 'Saves', 'SaveHeaderCache.json')):
        if os.path.exists(p):
            return p
    return os.path.join(DATA, 'SaveHeaderCache.json')


def files():
    return {'OutGame.json': os.path.join(DATA, 'OutGame.json'), 'UserCampaignLog.json': os.path.join(DATA, 'UserCampaignLog.json'),
            'SaveHeaderCache.json': cache_path()}


def snap(tag):
    """byte copies of the three files the title and the next sessions read"""
    d = os.path.join(OUT, 'files', tag); os.makedirs(d, exist_ok=True); out = {}
    for n, p in files().items():
        b = open(p, 'rb').read() if os.path.exists(p) else None
        out[n] = b
        if b is not None:
            open(os.path.join(d, n), 'wb').write(b)
    return out


def json_diff(a, b, path=''):
    """the paths where two JSON values differ (short list)"""
    if type(a) is not type(b):
        return [path or '/']
    if isinstance(a, dict):
        r = []
        for k in sorted(set(a) | set(b)):
            r += json_diff(a.get(k), b.get(k), f'{path}/{k}') if k in a and k in b else [f'{path}/{k}']
        return r
    if isinstance(a, list):
        if len(a) != len(b):
            return [f'{path} (len {len(a)} -> {len(b)}: +{[x for x in b if x not in a][:6]})']
        r = []
        for i, (x, y) in enumerate(zip(a, b)):
            r += json_diff(x, y, f'{path}[{i}]')
        return r
    return [] if a == b else [f'{path} ({a!r} -> {b!r})']


def diff_of(a, b):
    if a == b:
        return []
    if a is None or b is None:
        return [f'file {"appeared" if a is None else "gone"}']
    try:
        return json_diff(json.loads(a.decode('utf-8-sig')), json.loads(b.decode('utf-8-sig'))) or ['bytes differ (same JSON)']
    except ValueError:
        return ['bytes differ (not JSON)']


def test_files():
    """the test worlds' files on disk: their GUIDs come from custom-modes' own list"""
    try:
        tw = json.load(open(os.path.join(DATA, 'Mods', 'custom-modes', 'test_worlds.json'))).get('worlds') or []
    except (OSError, ValueError):
        tw = []
    return tw, [p for p in glob.glob(os.path.join(DATA, 'Saves', '**', '*.json'), recursive=True) if any(g in os.path.basename(p) for g in tw)]


def pick_mode(mid):
    for i in range(6):
        if tf().get('mode') == mid:
            return True
        cmf(cmd='set', name='fights:r.mode', index=i); time.sleep(0.8)
    return tf().get('mode') == mid


def hardest():
    """the last Combat tier (the management tier follows it): Downfall's unlock rule needs both above Hard"""
    seen = []
    for i in range(8):
        cmf(cmd='set', name='fights:r.combat', index=i); time.sleep(0.5)
        c = tf().get('combat')
        if c in seen:
            break
        seen.append(c)
    cmf(cmd='set', name='fights:r.combat', index=len(seen) - 1); time.sleep(0.8)
    return seen


def one_test(mid, before):
    """Units Laboratory > Test Fights > <mode> > Start; the test world loaded; Esc > Quit to title; the title as before"""
    ok = click('title:' + (by_index(column(), COL_LAB) or 'Units Laboratory'), 3) and click('home:Test Fights', 4)
    check(f'[{mid}] Units Laboratory > Test Fights opened', ok)
    s = wait('Test Fights engine', lambda: (lambda t: t if t.get('engine') in (2, 3) else None)(tf()), 300, 2) or {}
    check(f'[{mid}] the wiki\'s fight maker is ready', s.get('engine') == 2, s)
    if s.get('engine') != 2:
        return False
    if (tf().get('squad') or 0) < 2:
        for k in (0, 1):
            cmf(cmd='click', name=f'fights:l.unit{k}'); time.sleep(0.8)
    check(f'[{mid}] a squad of two My Units units', tf().get('squad') == 2, tf())
    check(f'[{mid}] test chosen', pick_mode(mid), tf().get('mode'))
    if mid == 'boss':
        tiers = hardest(); s = tf()
        check(f'[{mid}] hardest difficulty chosen', s.get('combat') == tiers[-1], (tiers, s.get('combat'), s.get('mgmt')))
    t0 = time.time()
    cmf(cmd='click', name='fights:start')
    ok = wait('test world loaded', lambda: HP.st(X).get('scene') == 2, 240, 2)
    check(f'[{mid}] Start loads the test world', ok, f'{time.time() - t0:.0f} s; tf {tf() if not ok else ""}')
    if not ok:
        return False
    tw, tfs = test_files()
    t = wait('the test world in the game', lambda: (lambda t: t if t.get('lguid') in tw else None)(title()), 120, 1) or title()
    check(f'[{mid}] the world loaded is the test world', bool(tw) and t.get('lguid') in tw, (t.get('lsave'), t.get('lguid'), tw))
    lg = t.get('lguid') or ''
    check(f'[{mid}] the test world loads with the test-world mark on its guid (7e57f167, version digit 0: known forever)',
          len(lg) == 32 and lg.startswith('7e57f167') and lg[12] == '0', (t.get('lsave'), lg))
    time.sleep(20)                                            # the world runs a while (autosaves, records)
    HP.c(X, cmd='mpm_quit_title')
    back = wait('title', lambda: HP.st(X).get('scene') == 1, 180, 2)
    check(f'[{mid}] Esc > Quit to title goes back to the title', back)
    if not back:
        return False
    after = wait('title preview drawn', lambda: (lambda t: t if t.get('menu') and t.get('prev') and t.get('psave') else None)(title()), 60, 1) or title()
    check(f'[{mid}] the title\'s background is the real world it showed before the test (not the test world)',
          after.get('psave') == before['title']['psave'] and after.get('pguid') == before['title']['pguid'] and after.get('pguid') not in tw,
          (after.get('psave'), after.get('pguid'), 'before', before['title']['psave']))
    check(f'[{mid}] Continue points at that real world', after.get('cont') == before['title']['cont'], (after.get('cont'), 'before', before['title']['cont']))
    gone = wait('test world files removed', lambda: not test_files()[1], 30, 1)
    time.sleep(3)
    _, left = test_files()
    check(f'[{mid}] back on the title no test world save is left in the Saves folder (Load Campaign)', gone and not left, [os.path.basename(p) for p in left][:4])
    items = title().get('items')
    check(f'[{mid}] the items obtained the game holds in memory are as before the test (the next write would save them)',
          items == before['title'].get('items') and items not in (None, -1), (items, 'before', before['title'].get('items')))
    now = snap(f'after_{mid}')
    cache = (now.get('SaveHeaderCache.json') or b'').decode('utf-8', 'replace')
    check(f'[{mid}] the header cache lists no test world', not [g for g in tw if g in cache.replace('-', '').lower()], tw)
    for n in ('OutGame.json', 'SaveHeaderCache.json'):
        d = diff_of(before['files'][n], now[n])
        check(f'[{mid}] {n} byte-equal to the copy from before the test', not d, d[:8])
    d = [x for x in diff_of(before['files']['UserCampaignLog.json'], now['UserCampaignLog.json']) if not x.startswith('/PlayerApplicationMinute')]
    check(f'[{mid}] UserCampaignLog.json: only PlayerApplicationMinute (minutes the game was on) may change', not d, d[:8])
    return True


def run():
    t = wait('title preview drawn', lambda: (lambda t: t if t.get('menu') and t.get('prev') and t.get('psave') else None)(title()), 120, 1) or title()
    sv = os.path.splitext(os.path.basename(SAVE))[0]
    check('before the test: the title\'s background is the real world (the newest save, the last played)', t.get('psave') == sv and t.get('pvalid') == 1, t)
    check('before the test: Continue points at that world', t.get('cont') == sv, t.get('cont'))
    time.sleep(3)
    before = {'title': t, 'files': snap('before')}
    pg = t.get('pguid')
    log('before', {n: (len(b) if b else None) for n, b in before['files'].items()})
    for mid in MODES:
        if not one_test(mid, before):
            return
        time.sleep(2)
    # Continue, pressed: the real world loads
    rows = column()
    cont = next((r[0] for r in rows if r and r[0].lower() == 'continue'), 'Continue')
    click('title:' + cont, 2)
    t = wait('the real world in the game', lambda: (lambda t: t if t.get('lguid') == pg else None)(title()), 240, 2) or title()
    check('pressing Continue loads the real world (the last played), not a test world', t.get('lguid') == pg, (t.get('lsave'), t.get('lguid'), 'want', pg))
    time.sleep(15)
    log('OutGame.json after Continue (the real world may add its own records):', diff_of(before['files']['OutGame.json'], snap('after_continue')['OutGame.json'])[:8])


def main():
    o0 = fstate(OWNER_LIB); s0 = fstate(SAVE)
    g = threading.Thread(target=HP.guard, daemon=True); g.start()
    res = {}
    try:
        os.makedirs(os.path.dirname(LIB), exist_ok=True)
        fx = json.load(open(os.path.join(REPO, 'units', 'test', 'data', 'launcher_library.json')))
        json.dump({'format': 'ds-launcher-units', 'version': 1, 'game_build': '25846389', 'units': fx['units'][:2], 'teams': []}, open(LIB, 'w'), indent=1)
        os.makedirs(GAME_SAVES, exist_ok=True)
        cp = os.path.join(GAME_SAVES, os.path.basename(SAVE)); shutil.copy2(SAVE, cp)
        if os.path.exists(DATA):                              # a fresh data folder each run (never deleted: set aside)
            aside = os.path.expanduser(f'~/ToDelete_2026-10-25/dstitle/data_{time.strftime("%H%M%S")}')
            os.makedirs(os.path.dirname(aside), exist_ok=True); shutil.move(DATA, aside)
        HP.make_copy(X)
        pk = {'custom-modes': zip_dir(os.path.join(HP.BUILD, 'mods', 'custom-modes'), 'custom-modes'), 'font-probe': probe_pkg()}
        HP.kit(X, pk, ['custom-modes', 'font-probe'], ['multiplayer', 'custom-modes', 'font-probe'])
        for f in ('OutGame.json', 'UserCampaignLog.json'):
            shutil.copy2(os.path.join(SEED, f), os.path.join(DATA, f))
        crash = os.path.join(OUT, 'crash'); os.makedirs(crash, exist_ok=True); open(os.path.join(crash, 'answer'), 'w').write('no')
        HP.room_for(1)
        HP.start(X, 'http://127.0.0.1:9', {'DSL_UNITS_TEST': DRV, 'DSL_CRASHREP_TEST': crash, 'DSMP_UNITS_LIB': LIB, 'DSML_GAME_SAVES': GAME_SAVES,
                                           'DSCM_WIKI_DIR': WIKI})
        lrig.wait_for(X, lambda s: s.get('scene') == 1 and not s.get('busy'), 'title', timeout=1500); time.sleep(6)
        run()
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
        check("the owner's library file is unchanged (sha256 + time)", fstate(OWNER_LIB) == o0)
        check('the source save is unchanged', fstate(SAVE) == s0)
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
