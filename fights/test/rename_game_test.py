#!/usr/bin/env python3
"""Test Fights with My Units units renamed in My Units (lead 2026-10-25: "a renamed unit fights under its new name"), in one
hidden loader test copy (background-only, small window, muted, notch fix off; never the owner's game, saves, library or
screen) with multiplayer + custom-modes + multi-bar + the test modules font-probe (the units test driver: clicks) and
namekey-probe (units/test/namekey: the names the player sees). The copy's My Units library is a test file (DSMP_UNITS_LIB)
made here from the wiki's public base save (4 units with default names, as My Units' "Save snapshot" stores them):
  unit 0  a default name key, renamed in My Units ("Brick Wall")                -> fights as "Brick Wall"
  unit 1  a default name key, not renamed (the entry shows the key's game text)  -> the key's game text in this language
  unit 2  a typed name ("Old Name") renamed in My Units ("New Name")             -> fights as "New Name"
  unit 3  a typed name ("Steady"), not renamed                                   -> "Steady"
Fails when (the break this catches): the 4 units, started from Units Laboratory > Test Fights > My Units (training-dummy
fight), do not show exactly those names on the stock unit panel (each unit alone) and on the Multi-unit view cards (all
4 selected), e.g. the old name of a renamed unit or a raw TEXTKEY_; or a window shows.
  rename_game_test.py      env: HM_BUILD = a loader build folder: dsload.dylib trusting the dev key, mods/multiplayer,
                                           mods/custom-modes (custom-modes: build_module.sh OUT=<HM_BUILD>/mods/custom-modes)
                                MB_DIR   = a built multi-bar module folder (default ~/ds_launcher/loader/build/mods_all/mac/multi-bar)
                                LD_ROOT  = a ds_modloader checkout (its loader/test helpers; default ~/ds_launcher)
                                LANGTYPE = the game's language (3 de, ...; default en)
Output: RN_OUT (default <HM_BUILD>/../test_env/rename/<time>): results.json, logs."""
import hashlib, json, os, shutil, subprocess, sys, threading, time, traceback, zipfile

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(os.path.dirname(HERE))
LD = os.path.expanduser(os.environ.get('LD_ROOT', '~/ds_launcher'))
sys.path.insert(0, os.path.join(LD, 'loader', 'test'))
import hostmods_pair as HP  # noqa: E402  (copies, kits, start/stop, owner screen guard)
from hostmods_pair import lrig, log, check  # noqa: E402

X = 'TFR'; HP.H = HP.G = X
OUT = os.environ.get('RN_OUT') or os.path.join(os.path.dirname(HP.BUILD), 'test_env', 'rename', time.strftime('%Y%m%d-%H%M%S'))
os.makedirs(OUT, exist_ok=True)
DRV = os.path.join(OUT, 'drv'); os.makedirs(os.path.join(DRV, 'cap'), exist_ok=True)
LANG = os.environ.get('LANGTYPE')
MB_DIR = os.path.expanduser(os.environ.get('MB_DIR', '~/ds_launcher/loader/build/mods_all/mac/multi-bar'))
BASE = os.path.expanduser('~/dungeonsettlersdb/site/assets/saves/base_25846389.json')
OWNER_LIB = os.path.expanduser('~/Library/Application Support/CanOpener/Dungeon Settlers/Mods/units/library.json')
LIB = os.path.join(OUT, 'lib', 'library.json')
NK = os.path.join(REPO, 'units', 'test', 'namekey')
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


def probe_pkg(mid, src, mod):
    """a test module (never shipped) built here as a mod zip"""
    d = os.path.join(OUT, 'pkg', mid); os.makedirs(d, exist_ok=True)
    subprocess.run(['nice', '-n', '19', 'clang', '-arch', 'arm64', '-O2', '-Wall', '-Wno-unused-function', '-I' + os.path.join(LD, 'loader', 'sdk'),
                    '-dynamiclib', '-framework', 'CoreFoundation', '-framework', 'CoreGraphics', '-lobjc', '-install_name', f'@rpath/{mid}.dylib',
                    '-o', os.path.join(d, f'{mid}.dylib'), src], check=True)
    subprocess.run(['codesign', '-f', '-s', '-', os.path.join(d, f'{mid}.dylib')], capture_output=True)
    if isinstance(mod, str): shutil.copy2(mod, os.path.join(d, 'mod.json'))
    else: json.dump(mod, open(os.path.join(d, 'mod.json'), 'w'), indent=1)
    return zip_dir(d, mid)


def snapshot_builds():
    """the base save's 4 units as My Units' snapshot stores them (units/test/namekey/nk_tool builds)"""
    b = os.path.join(OUT, 'bin'); os.makedirs(b, exist_ok=True); nk = os.path.join(b, 'nk_tool')
    srcs = [os.path.join(REPO, 'units', f) for f in ('mu_save.c', 'mu_worlds.c', 'myunits.c', 'pvp_wiki.c', 'pvp_build.c', 'mu_card.c')]
    srcs += [os.path.join(REPO, 'fe', f) for f in ('fe_json.c', 'fe_save.c')] + [os.path.join(REPO, 'units', 'test', 'dsi_impl.c')]
    subprocess.run(['nice', '-n', '19', 'cc', '-O1', '-w', '-I', os.path.join(REPO, 'units'), '-I', REPO, '-I', os.path.join(REPO, 'fe'),
                    '-o', nk, os.path.join(NK, 'nk_tool.c')] + srcs + ['-lz', '-lm'], check=True)
    p = os.path.join(OUT, 'builds.json')
    subprocess.run([nk, 'builds', BASE, p], check=True, stdout=subprocess.DEVNULL)
    return json.load(open(p))['snap']


# ------------------------------------------------------------------ drivers
seq = [0]
def req(cmd, timeout=30, **kw):
    seq[0] += 1; q = dict(kw, seq=seq[0], cmd=cmd)
    open(os.path.join(DRV, 'req.json.tmp'), 'w', encoding='utf-8').write(json.dumps(q, ensure_ascii=False)); os.replace(os.path.join(DRV, 'req.json.tmp'), os.path.join(DRV, 'req.json'))
    t0 = time.time()
    while time.time() - t0 < timeout:
        try:
            a = json.load(open(os.path.join(DRV, 'resp.json'), encoding='utf-8', errors='replace'))
            if a.get('seq') == seq[0]:
                return a
        except (OSError, ValueError):
            pass
        time.sleep(0.2)
    raise RuntimeError(f'no answer to {q}')


def tf():
    return ((req('state').get('cm') or {}).get('fights') or {}).get('tf') or {}


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


def dlog():
    try: return open(os.path.join(lrig.idir(X), 'dsload.log'), errors='replace').read()
    except OSError: return ''


def probe(line, want, limit=20):
    """one request to namekey-probe; its new answer lines once one contains `want`"""
    mark = len(dlog()); p = os.path.join(lrig.idir(X), 'namekey_probe_req.txt')
    open(p + '.tmp', 'w').write(line + '\n'); os.replace(p + '.tmp', p)
    t0 = time.time()
    while time.time() - t0 < limit:
        L = [l.split('namekey-probe: ', 1)[1] for l in dlog()[mark:].splitlines() if 'namekey-probe: ' in l]
        if any(want in l for l in L): return L
        time.sleep(0.25)
    return []


def names():
    import re
    L = probe('names', 'names done')
    return [dict(bar=m[1], active=int(m[2]), unit=m[3], text=m[5])
            for m in (re.match(r'bar \[(\S+) active=(\d) unit=(\S+) text_active=(\d)\] text=(.*)$', l) for l in L) if m]


def key_text(k):
    return next((l.split(' = ', 1)[1] for l in probe('text ' + k, 'text %s = ' % k) if l.startswith('text %s = ' % k)), None)


# ------------------------------------------------------------------ the run
def library(snap, t1):
    """the 4 units (see the docstring); names as JSON text, the library as My Units writes it"""
    b = [dict(x) for x in snap[:4]]
    b[2]['name'] = 'Old Name'; b[3]['name'] = 'Steady'
    names_ = ['Brick Wall', t1, 'New Name', 'Steady']
    t = int(time.time() * 1000)
    units = [{'id': '%016x' % (0xa11ce0 + i), 'name': names_[i], 'created': t, 'updated': t, 'build': b[i]} for i in range(4)]
    os.makedirs(os.path.dirname(LIB), exist_ok=True)
    json.dump({'format': 'ds-launcher-units', 'version': 1, 'game_build': '25846389', 'units': units, 'teams': []}, open(LIB, 'w'), indent=1, ensure_ascii=False)
    return names_


def run(snap):
    lang = {'3': 'de', '4': 'ru', '7': 'ja'}.get(LANG, LANG) if LANG else 'en'
    t1 = key_text(snap[1]['name'])
    check(f'{lang}: the game has a text for the not renamed unit\'s name key {snap[1]["name"]}', t1 and not t1.startswith('TEXTKEY_'), t1)
    want = sorted(library(snap, t1 or snap[1]['name']))
    ok = click('title:' + (by_index(column(), COL_LAB) or 'Units Laboratory'), 3) and click('home:Test Fights', 4)
    check(f'{lang}: Units Laboratory > Test Fights opened', ok)
    s = wait('Test Fights engine', lambda: (lambda t: t if t.get('engine') in (2, 3) else None)(tf()), 300, 2) or {}
    check(f'{lang}: the wiki\'s fight maker is ready', s.get('engine') == 2, s)
    click('fights:l.src0', 2)
    for i in range(4): click(f'fights:l.unit{i}', 1.5)
    s = tf()
    check(f'{lang}: 4 My Units units in the squad, training-dummy fight', s.get('squad') == 4 and s.get('mode') == 'dps', (s.get('squad'), s.get('mode'), s.get('status')))
    t0 = time.time()
    click('fights:start', 0.5)
    ok = wait('test world loaded', lambda: HP.st(X).get('scene') == 2 and not HP.st(X).get('busy'), 240, 2)
    check(f'{lang}: Start loads the test world', ok, f'{time.time() - t0:.0f} s; tf {tf() if not ok else ""}')
    if not ok:
        return
    time.sleep(6)
    panel = {}
    for i in range(4):
        probe('sel %d' % i, 'sel %d:' % i); time.sleep(1.0)
        t0 = time.time()
        while time.time() - t0 < 8:
            st = next((b for b in names() if not b['bar'].startswith('QuickSlotUI_Multi') and b['active']), None)
            if st and st['unit'] not in panel and st['text']: panel[st['unit']] = st['text']; break
            time.sleep(0.7)
    check(f'{lang}: stock unit panel: the renamed units under their new names, the others under theirs', sorted(panel.values()) == want,
          (sorted(panel.values()), want))
    probe('sel 0', 'sel 0:'); time.sleep(1)
    for i in (1, 2, 3): probe('add %d' % i, 'add %d:' % i); time.sleep(0.8)
    cards = []
    t0 = time.time()
    while time.time() - t0 < 10:
        cards = [b for b in names() if b['bar'].startswith('QuickSlotUI_Multi') and b['active']]
        if len(cards) == 4: break
        time.sleep(0.7)
    check(f'{lang}: Multi-unit view cards: the same names', sorted(c['text'] for c in cards) == want, (sorted(c['text'] for c in cards), want))
    HP.c(X, cmd='mpm_quit_title')
    wait('title', lambda: HP.st(X).get('scene') == 1, 120, 2)


def main():
    o0 = fstate(OWNER_LIB)
    g = threading.Thread(target=HP.guard, daemon=True); g.start()
    res = {}
    try:
        snap = snapshot_builds()
        check('the base save gives 4 snapshot units with default name keys', len(snap) >= 4 and all(str(b.get('name', '')).startswith('TEXTKEY_PLAYERNAME_') for b in snap[:4]),
              [b.get('name') for b in snap])
        library(snap, '')                                     # a library file before the start (names set again once the game runs)
        HP.make_copy(X)
        pk = {'custom-modes': zip_dir(os.path.join(HP.BUILD, 'mods', 'custom-modes'), 'custom-modes'), 'multi-bar': zip_dir(MB_DIR, 'multi-bar'),
              'font-probe': probe_pkg('font-probe', os.path.join(REPO, 'home', 'test', 'font_probe', 'font_probe.c'),
                                      os.path.join(REPO, 'home', 'test', 'font_probe', 'mod.json')),
              'namekey-probe': probe_pkg('namekey-probe', os.path.join(NK, 'namekey_probe.c'),
                                         {'id': 'namekey-probe', 'name': 'namekey probe (test)', 'version': '0.0.1', 'kind': 'code', 'api': 2,
                                          'scope': 'client', 'game_builds': ['25846389'], 'platforms': {'mac-arm64': {'entry': 'namekey-probe.dylib'}}})}
        mods = ['custom-modes', 'multi-bar', 'font-probe', 'namekey-probe']
        HP.kit(X, pk, mods, ['multiplayer'] + mods)
        if LANG:
            prep = lrig.prepare_data
            def prep_lang(x):
                prep(x); us = os.path.join(lrig.idir(x), 'data', 'UserSetting.json'); s = json.load(open(us))
                s['GeneralSettingData']['SaveCurrentLanguageType'] = int(LANG); json.dump(s, open(us, 'w'), indent=1)
            lrig.prepare_data = prep_lang
        crash = os.path.join(OUT, 'crash'); os.makedirs(crash, exist_ok=True); open(os.path.join(crash, 'answer'), 'w').write('no')
        HP.room_for(1)
        HP.start(X, 'http://127.0.0.1:9', {'DSL_UNITS_TEST': DRV, 'DSL_CRASHREP_TEST': crash, 'DSMP_UNITS_LIB': LIB})
        lrig.wait_for(X, lambda s: s.get('scene') == 1 and not s.get('busy'), 'title', timeout=1500); time.sleep(6)
        L = dlog()
        check('multi-bar, font-probe and namekey-probe ready', 'multi-bar: ready' in L and 'namekey-probe: ready' in L, [l for l in L.splitlines() if 'probe' in l][-3:])
        run(snap)
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
        for f in ('dsload.log', 'player.log'):
            p = os.path.join(lrig.idir(X), f)
            if os.path.exists(p):
                shutil.copy2(p, os.path.join(OUT, f))
        res.update({'checks': HP.checks, 'all_pass': all(k['ok'] for k in HP.checks) and bool(HP.checks)})
        json.dump(res, open(os.path.join(OUT, 'results.json'), 'w'), indent=1, default=str, ensure_ascii=False)
        log('RESULT', 'PASS' if res['all_pass'] else 'FAIL', f"{sum(k['ok'] for k in HP.checks)}/{len(HP.checks)}", os.path.join(OUT, 'results.json'))
    sys.exit(0 if res.get('all_pass') else 1)


if __name__ == '__main__':
    main()
