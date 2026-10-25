#!/usr/bin/env python3
"""Escape closes Units Laboratory's windows (owner 2026-10-25: "make sure Escape works to close our custom windows"). The
loader kit owns the key (kit 0.1.28, dsl_sdk.h esc_push): each window pushes itself, one press closes the top one as its own
Close / Cancel / X / Back. One hidden loader test copy (default M: background-only, small window, muted, notch fix off;
never the owner's game, library, saves or screen) with custom-modes + the test module font-probe (home/test/font_probe:
REAL Escape key events on the game's view, a fake mouse, Units Laboratory's test registry and the kit's Escape stack). Title
only. Fails when (the breaks this catches):
  - Escape leaves a window of ours open: the home page (Escape = Back), My Units, the Unit Builder, the Wiki Build Gallery,
    Test Fights; or closes more than the top one for one press (Test Fights over the Gallery, the Builder over My Units,
    a window over the home page: the window under it must stay);
  - the name box (Rename team) or the paste box with the keyboard in its field: the 1st press must cancel that box only
    (My Units stays), the 2nd closes My Units; the Delete team box: 1st press closes the box only (nothing deleted);
  - a plain field (Builder name, Gallery search): the 1st press only leaves the field, the window stays;
  - the Gallery's Details page: the 1st press = its Back (the list again, the Gallery stays), the 2nd closes the Gallery
    (owner 2026-10-25: "Go Back one level");
  - the Builder with an unsaved change: Escape asks first (the game's notice), Escape on that notice = "Keep editing" (the
    Builder stays); a gear slot's item list: Escape closes the list only;
  - the test library or the owner's real library file changed (Escape must never save or delete).
An older custom-modes on the same loader fails (its windows are not on the kit's Escape stack: My Units, the Builder and the
home page stay open).
  esc_test.py [out] [--no-build]
  env: LD_ROOT = a ds_modloader checkout with a built loader (loader/build/dsload.dylib, loader/test/lrig.py; default ~/ds_launcher)
       ESC_CM  = a built custom-modes folder (custom-modes.dylib, mod.json, lang/); default: this tree, built here
                 (build_module.sh with SDK = LD_ROOT/loader/sdk, MP_SRC = the loader's multiplayer pin src/)
       DSL_TEST_COPY = the copy (default M)
Exit 0 = PASS. <out>/summary.json, dsload.log, player.log."""
import glob, hashlib, json, os, shutil, signal, subprocess, sys, threading, time

HERE = os.path.dirname(os.path.abspath(__file__)); REPO = os.path.dirname(os.path.dirname(HERE))
LD = os.path.expanduser(os.environ.get('LD_ROOT', '~/ds_launcher')); LW = os.path.join(LD, 'loader')
sys.path.insert(0, os.path.join(LW, 'test'))
import lrig as r  # noqa: E402

ARGS = [a for a in sys.argv[1:] if not a.startswith('--')]
OUT = os.path.abspath(os.path.expanduser(ARGS[0] if ARGS else os.path.join(REPO, 'build', 'esc_test')))
X = os.environ.get('DSL_TEST_COPY', 'M'); I = r.idir(X); APP = os.path.join(I, 'DungeonSettlers.app')
DRV = os.path.join(OUT, 'drv'); MODS = os.path.join(OUT, 'mods'); LIB = os.path.join(OUT, 'lib', 'library.json')
GAME_SAVES = os.path.join(OUT, 'game_saves'); WIKI = os.path.expanduser(os.environ.get('DS_WIKI_SITE', '~/dungeonsettlersdb/site'))
OWNER_LIB = os.path.expanduser('~/Library/Application Support/CanOpener/Dungeon Settlers/Mods/units/library.json')
ESC = 53                                                              # macOS key code of Escape
log = lambda *a: print(time.strftime('%H:%M:%S'), *a, flush=True)
checks = []; violations = []; stop = threading.Event()


def check(name, ok, detail=''):
    checks.append({'name': name, 'ok': bool(ok), 'detail': str(detail)[:500]}); log('PASS' if ok else 'FAIL', name, '' if ok else detail)


def fstate(p):
    return (hashlib.sha256(open(p, 'rb').read()).hexdigest(), os.path.getmtime(p)) if os.path.exists(p) else (None, None)


def game_pids():
    out = subprocess.run(['pgrep', '-f', '^' + os.path.join(I, r.EXE_REL)], capture_output=True, text=True).stdout
    return [int(p) for p in out.split()]


def guard():                                                          # the copy never shows a window or comes to the front
    while not stop.is_set():
        pids = game_pids()
        if pids:
            bad = [w for w in r.windows(pids) if w['alpha'] > 0 and w['onscreen'] and w['x'] > -5000 and w['y'] > -5000]
            fp = r.front_pid()
            if bad or fp in pids:
                violations.append({'t': time.time(), 'windows': bad, 'front': fp})
                for p in pids: os.kill(p, signal.SIGKILL)
                log('GUARD KILLED', bad, fp)
        stop.wait(0.5)


def put(src, dst):
    os.makedirs(os.path.dirname(dst), exist_ok=True); shutil.copy2(src, dst + '.tmp'); os.replace(dst + '.tmp', dst)


def build():
    fp = os.path.join(REPO, 'home', 'test', 'font_probe'); d = os.path.join(MODS, 'font-probe'); os.makedirs(d, exist_ok=True)
    subprocess.run(['nice', '-n', '19', 'clang', '-arch', 'arm64', '-O2', '-Wall', '-Wno-unused-function', '-I' + os.path.join(LW, 'sdk'),
                    '-dynamiclib', '-framework', 'CoreFoundation', '-framework', 'CoreGraphics', '-lobjc', '-install_name', '@rpath/font-probe.dylib',
                    '-o', os.path.join(d, 'font-probe.dylib'), os.path.join(fp, 'font_probe.c')], check=True)
    subprocess.run(['codesign', '-f', '-s', '-', os.path.join(d, 'font-probe.dylib')], capture_output=True)
    shutil.copy2(os.path.join(fp, 'mod.json'), os.path.join(d, 'mod.json'))
    cm = os.path.join(MODS, 'custom-modes')
    if os.environ.get('ESC_CM'):
        if os.path.isdir(cm): shutil.move(cm, cm + '.old.%d' % time.time())
        shutil.copytree(os.path.expanduser(os.environ['ESC_CM']), cm)
    else:
        pin = open(os.path.join(LW, 'mp_pin.txt')).readline().strip()
        src = os.path.join(LW, 'build', 'pin', pin, 'src')
        if not os.path.isfile(os.path.join(src, 'mp_api.h')): raise SystemExit(f'no multiplayer headers in {src}: build the loader first')
        os.makedirs(cm, exist_ok=True)
        subprocess.run(['zsh', os.path.join(REPO, 'build_module.sh')], check=True, stdout=subprocess.DEVNULL,
                       env=dict(os.environ, OUT=cm, SDK=os.path.join(LW, 'sdk'), MP_SRC=src))


def install():
    if game_pids(): raise SystemExit(f'copy {X} is running: not installing over it')
    put(os.environ.get('ESC_DYLIB') or os.path.join(LW, 'build', 'dsload.dylib'), os.path.join(APP, 'Contents', 'Frameworks', 'dsload.dylib'))
    for mid in ('custom-modes', 'font-probe'):
        src = os.path.join(MODS, mid); dst = os.path.join(APP, 'Contents', 'Resources', 'mods', mid)
        if os.path.isdir(dst): shutil.move(dst, os.path.join(I, 'old_mods', '%s.%d' % (mid, time.time())))   # no stale file of another build
        for root, _, files in os.walk(src):
            for f in files: put(os.path.join(root, f), os.path.join(dst, os.path.relpath(os.path.join(root, f), src)))
    s = os.path.join(LW, 'lang')
    if os.path.isdir(s):
        for f in os.listdir(s): put(os.path.join(s, f), os.path.join(APP, 'Contents', 'Frameworks', 'lang', f))
    subprocess.run(['codesign', '-f', '-s', '-', os.path.join(APP, 'Contents', 'MacOS', 'Dungeon Settlers')], capture_output=True)


def wiki_dir():
    """the wiki files the mod reads (DSCM_WIKI_DIR): the wiki site's own files + the gallery's list from this repo's fixture
    (gallery/test/data/api/gallery: 2 builds), so the Gallery shows cards with no network"""
    d = os.path.join(OUT, 'wiki'); os.makedirs(os.path.join(d, 'api'), exist_ok=True)
    for n in os.listdir(WIKI):
        if n != 'api' and not os.path.lexists(os.path.join(d, n)): os.symlink(os.path.join(WIKI, n), os.path.join(d, n))
    for n in os.listdir(os.path.join(WIKI, 'api')) if os.path.isdir(os.path.join(WIKI, 'api')) else []:
        if not os.path.lexists(os.path.join(d, 'api', n)): os.symlink(os.path.join(WIKI, 'api', n), os.path.join(d, 'api', n))
    shutil.copy2(os.path.join(REPO, 'gallery', 'test', 'data', 'api', 'gallery'), os.path.join(d, 'api', 'gallery'))
    return d


def make_library():
    """3 fixture units; Team A = u0 u1 (a team for Rename team / Delete team)"""
    fx = json.load(open(os.path.join(REPO, 'units', 'test', 'data', 'launcher_library.json')))
    units = fx['units'][:3]
    for i, u in enumerate(units): u['id'] = f'{0xa0 + i:02x}' * 8; u['name'] = f'Test unit {i}'
    lib = {'format': 'ds-launcher-units', 'version': 1, 'game_build': '25846389', 'units': units,
           'teams': [{'id': 'aa' * 8, 'name': 'Team A', 'unit_ids': [units[0]['id'], units[1]['id']]}]}
    os.makedirs(os.path.dirname(LIB), exist_ok=True); json.dump(lib, open(LIB, 'w'), indent=2)


def start():
    r.require_background_only(X); r.prepare_data(X); r.require_windowed(X)
    pd = os.path.join(I, 'home', 'Library', 'Application Support', f'com.CanOpener.Dungeon-Settlers.mp{X}'); os.makedirs(pd, exist_ok=True)
    for f in ('UserSetting.json', 'OutGame.json'):
        if os.path.exists(os.path.join(I, 'data', f)): shutil.copy2(os.path.join(I, 'data', f), os.path.join(pd, f))
    mods = os.path.join(I, 'data', 'Mods'); os.makedirs(mods, exist_ok=True); os.makedirs(GAME_SAVES, exist_ok=True)
    for f in glob.glob(os.path.join(mods, 'worlds.json')):
        os.makedirs(os.path.join(mods, 'old'), exist_ok=True); os.replace(f, os.path.join(mods, 'old', 'worlds.json.%d' % time.time()))
    prof = os.path.join(OUT, 'profile.json'); json.dump({'enabled': ['custom-modes', 'font-probe'], 'order': ['custom-modes', 'font-probe']}, open(prof, 'w'))
    os.makedirs(os.path.join(DRV, 'cap'), exist_ok=True)
    for f in ('req.json', 'resp.json'):
        if os.path.exists(os.path.join(DRV, f)): os.replace(os.path.join(DRV, f), os.path.join(DRV, f + '.old'))
    crd = os.path.join(OUT, 'crashrep'); os.makedirs(crd, exist_ok=True); open(os.path.join(crd, 'answer'), 'w').write('no')
    env = {'HOME': os.path.join(I, 'home'), 'CFFIXED_USER_HOME': os.path.join(I, 'home'), 'USER': os.environ.get('USER', ''),
           'LOGNAME': os.environ.get('LOGNAME', ''), 'PATH': '/usr/bin:/bin:/usr/sbin:/sbin', 'TMPDIR': os.environ.get('TMPDIR', '/tmp'),
           'DSRIG_DIR': I, 'DSRIG_FPS': '60', **r.ctl_env(), 'DYLD_INSERT_LIBRARIES': os.path.join(APP, 'Contents', 'Frameworks', 'dsload.dylib'),
           'DSL_PROFILE': prof, 'DSML_DATA': os.path.join(I, 'data'), 'DSML_SESSION': os.path.join(mods, 'worlds.json'),
           'DSL_SOFT_STAGE': '-20000,-20000', 'DSL_SOFT_NOACTIVATE': '1', 'DSL_NO_ALERT': '1', 'DSL_NOTCH': '0', 'DSL_UPDATE': '0',
           'DSL_CLOUD': '0', 'DSL_PREWARM': '0', 'DSL_CRASHREP_TEST': crd,
           'DSL_UNITS_TEST': DRV, 'DSMP_UNITS_LIB': LIB, 'DSML_GAME_SAVES': GAME_SAVES, 'DSMP_CLIP_PB': 'dsesc-cm-test-%d' % os.getpid(),
           'DSCM_WIKI_DIR': wiki_dir()}
    env.update(r.soft_test_env())
    a = [os.path.join(I, r.EXE_REL), '-logFile', os.path.join(I, 'player.log'), '-screen-fullscreen', '0', '-screen-width', '960', '-screen-height', '540']
    p = subprocess.Popen(a, env=env, cwd=I, stdin=subprocess.DEVNULL, stdout=open(os.path.join(I, 'stdout.log'), 'ab'),
                         stderr=subprocess.STDOUT, preexec_fn=lambda: os.nice(5), start_new_session=True)
    open(r.pidfile(X), 'w').write(f'{p.pid}\n'); log('started', p.pid)


SEQ = [0]
def req(cmd, timeout=20, **kw):
    SEQ[0] += 1; q = dict(kw, seq=SEQ[0], cmd=cmd)
    tmp = os.path.join(DRV, 'req.json.tmp'); open(tmp, 'w').write(json.dumps(q)); os.replace(tmp, os.path.join(DRV, 'req.json'))
    t0 = time.time()
    while time.time() - t0 < timeout:
        try:
            j = json.load(open(os.path.join(DRV, 'resp.json')))
            if j.get('seq') == SEQ[0]: return j
        except (OSError, ValueError): pass
        time.sleep(0.05)
        if not game_pids(): raise RuntimeError('the game copy is gone (crash?)')
    return {'timeout': q}


st = lambda: req('state')
cm = lambda s, k: (s.get('cm') or {}).get(k) or {}
home_on = lambda s: cm(s, 'home').get('open') == 1
mu_on = lambda s: cm(s, 'mu').get('open') == 1
bld_on = lambda s: cm(s, 'bld').get('open') == 1
wins = lambda s: cm(s, 'fights').get('windows', '').split()
notice_on = lambda: req('rect', target='notice_ok').get('rc', -1) > 0


def wait_for(pred, limit=8, step=0.25):
    t0 = time.time()
    while time.time() - t0 < limit:
        v = pred()
        if v: return v
        time.sleep(step)
    return None


def until(pred, limit=8):
    return wait_for(lambda: (lambda s: s if pred(s) else None)(st()), limit) or st()


def click(t, wait=1.0):
    j = req('click', target=t); time.sleep(wait)
    if j.get('rc') != 0: log('click', t, 'rc', j.get('rc'))
    return j.get('rc') == 0


def press():
    """one real Escape (keyDown + keyUp on the game's view), then a few frames for the kit and the windows to act"""
    req('key', code=ESC); time.sleep(1.2); return st()


def to_title():                                                       # between cases: none of ours open (a failed case may leave one)
    s = st()
    for _ in range(8):
        if not (home_on(s) or mu_on(s) or bld_on(s) or wins(s) or notice_on()): return
        if wins(s): req('call', fn='cmf_test', s='{"cmd":"close"}')
        elif bld_on(s):
            if not notice_on(): click('bld:cancel')
            if notice_on(): click('notice_ok')                       # Discard
        elif notice_on(): click('notice_cancel')
        elif mu_on(s): click('mu:close')
        elif home_on(s): click('home:Back')
        time.sleep(0.5); s = st()
    log('to_title: still open', s.get('esc'))


def open_home():
    to_title(); click('title:Units Laboratory'); return until(home_on)


def open_mu():
    open_home(); click('home:My Units', 1.5); return until(mu_on)


def cases():
    lib0 = fstate(LIB)
    # 1. the home page: Escape = Back
    s = open_home()
    check('1 home page open, on the Escape stack', home_on(s) and s.get('esc_n', 0) >= 1, (cm(s, 'home'), s.get('esc')))
    s = press()
    check('1 Escape closes the home page (Back): the title column again', not home_on(s) and req('rect', target='title:Units Laboratory').get('rc', -1) > 0, (cm(s, 'home'), s.get('esc')))
    # 2. My Units over the home page
    s = open_mu()
    if not (mu_on(s) and cm(s, 'mu').get('lib') == LIB):
        raise RuntimeError(f'My Units not open on the TEST library: {cm(s, "mu").get("lib")}')   # never touch the owner's file
    check('2 My Units open over the home page', mu_on(s) and home_on(s), s.get('esc'))
    s = press()
    check('2 Escape closes My Units, the home page stays', not mu_on(s) and home_on(s), (mu_on(s), home_on(s), s.get('esc')))
    s = press()
    check('2 next Escape closes the home page', not home_on(s), s.get('esc'))
    # 3. Rename team with the keyboard in the name box (DSL_ESC_EDIT)
    open_mu(); click('mu:team0'); click('mu:rename_team'); s = until(lambda s: cm(s, 'mu').get('rename') == 1)
    check('3 the name box shows (Rename team)', cm(s, 'mu').get('rename') == 1, cm(s, 'mu').get('rename'))
    click('mu:rename_field'); s = until(lambda s: s.get('typing') == 1)
    check('3 its field has the keyboard', s.get('typing') == 1, s.get('typing'))
    s = press()
    check('3 1st Escape = the name box\'s Cancel, My Units stays', cm(s, 'mu').get('rename') == 0 and mu_on(s), (cm(s, 'mu').get('rename'), mu_on(s), s.get('esc')))
    s = press()
    check('3 2nd Escape closes My Units', not mu_on(s) and home_on(s), (mu_on(s), s.get('esc')))
    # 4. the Delete team box
    open_mu(); click('mu:team0'); click('mu:delete_team'); s = until(lambda s: cm(s, 'mu').get('dlg') == 1)
    check('4 the Delete team box shows', cm(s, 'mu').get('dlg') == 1, s.get('esc'))
    s = press()
    check('4 1st Escape closes the box only (My Units stays, nothing deleted)', cm(s, 'mu').get('dlg') == 0 and mu_on(s) and fstate(LIB) == lib0
          and cm(s, 'mu').get('teams') == 1 and cm(s, 'mu').get('units') == 3, (cm(s, 'mu').get('dlg'), mu_on(s), cm(s, 'mu').get('teams'), s.get('esc')))
    s = press()
    check('4 2nd Escape closes My Units', not mu_on(s), s.get('esc'))
    # 5. the paste box (no link on the test pasteboard) with the keyboard in its field (DSL_ESC_EDIT)
    open_mu(); req('call', fn='mu_test_clip', s=''); click('mu:add'); s = until(lambda s: cm(s, 'mu').get('box') == 1)
    check('5 the paste box shows', cm(s, 'mu').get('box') == 1, cm(s, 'mu').get('msg'))
    click('mu:box_field'); s = until(lambda s: s.get('typing') == 1)
    s = press()
    check('5 1st Escape (field with the keyboard) = the paste box\'s Cancel, My Units stays', cm(s, 'mu').get('box') == 0 and mu_on(s), (cm(s, 'mu').get('box'), mu_on(s), s.get('esc')))
    s = press()
    check('5 2nd Escape closes My Units', not mu_on(s), s.get('esc'))
    # 6. the Unit Builder over My Units
    open_mu(); click('mu:new', 1.5); s = until(bld_on)
    check('6 the Unit Builder opens over My Units', bld_on(s) and mu_on(s), s.get('esc'))
    s = press()
    check('6 Escape closes the Builder only (My Units stays)', not bld_on(s) and mu_on(s), (bld_on(s), mu_on(s), s.get('esc')))
    # 7. the Builder's name field (no flag) after an unsaved change (a race picked), the order a player meets: the 1st press
    #    only leaves the field; Escape = Cancel -> the game's notice; Escape there = "Keep editing" (the field that lost the
    #    keyboard must not take it back when the notice shows: the kit clears its selection)
    click('mu:new', 1.5)
    wait_for(lambda: (lambda s: s if cm(s, 'bld').get('engine') == 2 and not cm(s, 'bld').get('busy') else None)(st()), 40)
    click('bld:race:AFFECTER_Elf', 1.5); s = until(lambda s: cm(s, 'bld').get('dirty') == 1)
    check('7 a race picked: an unsaved change', cm(s, 'bld').get('dirty') == 1, cm(s, 'bld'))
    click('bld:name'); s = until(lambda s: s.get('typing') == 1)
    check('7 the Builder\'s name field has the keyboard', s.get('typing') == 1 and bld_on(s), s.get('typing'))
    s = press()
    check('7 1st Escape only leaves the Builder\'s name field (the Builder stays, no notice)', bld_on(s) and s.get('typing') == 0 and not notice_on(), (bld_on(s), s.get('typing'), s.get('esc')))
    s = press(); nt = notice_on()
    check('7 Escape = Cancel: the game\'s notice asks (unsaved change), the Builder stays', nt and bld_on(s), (nt, bld_on(s), s.get('esc')))
    check('7 the name field did not take the keyboard back', s.get('typing') == 0, s.get('typing'))
    s = press(); nt = notice_on()
    check('7 Escape on the notice = Keep editing (the Builder stays)', not nt and bld_on(s), (nt, bld_on(s), s.get('esc')))
    press(); wait_for(notice_on, 4); click('notice_ok', 1.5); s = st()   # Discard: closes the Builder the button's way
    check('7 Discard closes the Builder, My Units stays', not bld_on(s) and mu_on(s), (bld_on(s), mu_on(s)))
    # 8. a gear slot's item list in the Builder (needs the wiki builder from DS_WIKI_SITE)
    open_mu(); click('mu:new', 1.5)
    s = wait_for(lambda: (lambda s: s if cm(s, 'bld').get('engine') == 2 and not cm(s, 'bld').get('busy') else None)(st()), 40) or st()
    check('8 the Builder engine is ready (wiki builder files)', cm(s, 'bld').get('engine') == 2, cm(s, 'bld'))
    click('bld:nav:Gear', 1.5); click('bld:slot:Primary', 1.5); s = st()
    lst = req('rect', target='bld:item:').get('rc', -1) > 0
    check('8 the Primary slot\'s item list shows', lst and cm(s, 'bld').get('slot', -1) >= 0, (cm(s, 'bld').get('slot'), s.get('esc')))
    s = press(); lst = req('rect', target='bld:item:').get('rc', -1) > 0
    check('8 1st Escape closes the item list only (the Builder stays)', not lst and cm(s, 'bld').get('slot') == -1 and bld_on(s), (lst, bld_on(s), s.get('esc')))
    s = press()
    check('8 2nd Escape closes the Builder (no change made: no notice)', not bld_on(s) and mu_on(s) and not notice_on(), (bld_on(s), mu_on(s), cm(s, 'bld').get('dirty'), s.get('esc')))
    s = press()
    check('8 next Escape closes My Units', not mu_on(s) and home_on(s), s.get('esc'))
    # 9. the Wiki Build Gallery over the home page; its search field (no flag)
    open_home(); click('home:Wiki Build Gallery', 1.5); s = until(lambda s: 'gallery' in wins(s))
    check('9 the Gallery opens over the home page', 'gallery' in wins(s) and home_on(s), wins(s))
    click('gallery:s.field'); s = until(lambda s: s.get('typing') == 1)
    s = press()
    check('9 1st Escape only leaves the search field (the Gallery stays)', 'gallery' in wins(s) and s.get('typing') == 0, (wins(s), s.get('typing')))
    s = press()
    check('9 2nd Escape closes the Gallery, the home page stays', 'gallery' not in wins(s) and home_on(s), (wins(s), home_on(s), s.get('esc')))
    # 10. Test Fights over the Gallery: one press closes one window
    click('home:Wiki Build Gallery', 1.5); until(lambda s: 'gallery' in wins(s))
    req('call', fn='cmf_test', s='{"cmd":"open","win":"fights"}'); s = until(lambda s: 'fights' in wins(s))
    check('10 Test Fights opens over the Gallery', wins(s) == ['gallery', 'fights'] or sorted(wins(s)) == ['fights', 'gallery'], wins(s))
    s = press()
    check('10 Escape closes Test Fights only (the Gallery stays)', wins(s) == ['gallery'] and home_on(s), (wins(s), s.get('esc')))
    s = press()
    check('10 next Escape closes the Gallery, the home page stays', wins(s) == [] and home_on(s), (wins(s), s.get('esc')))
    # 11. Test Fights from the home page
    click('home:Test Fights', 1.5); s = until(lambda s: 'fights' in wins(s))
    s = press()
    check('11 Escape closes Test Fights, the home page stays', wins(s) == [] and home_on(s), (wins(s), s.get('esc')))
    s = press()
    check('11 next Escape closes the home page', not home_on(s), s.get('esc'))
    # 12. the Gallery's Details page (owner 2026-10-25 "Go Back one level"): 1st Escape = its Back, 2nd closes the Gallery
    gal = lambda s: cm(s, 'fights').get('gal') or {}
    open_home(); click('home:Wiki Build Gallery', 1.5)
    s = wait_for(lambda: (lambda s: s if gal(s).get('items', 0) > 0 else None)(st()), 40) or st()
    check('12 the Gallery lists builds (the wiki answered)', gal(s).get('items', 0) > 0, gal(s).get('status'))
    click('gallery:c0.card', 1.5); s = until(lambda s: gal(s).get('preview', -1) >= 0)
    check('12 a build\'s Details page shows', gal(s).get('preview', -1) >= 0, gal(s).get('preview'))
    s = press()
    check('12 1st Escape = Back (the list again, the Gallery stays)', 'gallery' in wins(s) and gal(s).get('preview', -1) == -1, (wins(s), gal(s).get('preview')))
    s = press()
    check('12 2nd Escape closes the Gallery, the home page stays', 'gallery' not in wins(s) and home_on(s), (wins(s), home_on(s)))
    press()
    check('the test library is unchanged (Escape never saves or deletes)', fstate(LIB) == lib0)


def main():
    if game_pids(): raise SystemExit(f'copy {X} already running: {game_pids()}')
    if os.getloadavg()[0] > 20: raise SystemExit('load over 20: no test copy now')
    os.makedirs(OUT, exist_ok=True)
    if '--no-build' not in sys.argv: build()
    install(); make_library(); owner0 = fstate(OWNER_LIB)
    g = threading.Thread(target=guard, daemon=True); g.start()
    try:
        start()
        s = wait_for(lambda: (lambda s: s if s.get('kit') == 1 and req('rect', target='title:Units Laboratory', timeout=5).get('rc', -1) > 0 else None)(st()), 240, 1)
        check('title reached, Units Laboratory button and the test driver up', bool(s), s and s.get('title_column'))
        check('the loader has the kit Escape stack (kit 0.1.28)', bool(s) and s.get('esc_n', -1) >= 0, s and s.get('esc_n'))
        time.sleep(3)
        cases()
    except Exception as e:
        check('no exception', False, repr(e))
    finally:
        stop.set(); time.sleep(0.6)
        for p in game_pids(): os.kill(p, signal.SIGTERM)
        time.sleep(3)
        for p in game_pids(): os.kill(p, signal.SIGKILL)
        for f in ('dsload.log', 'player.log'):
            if os.path.exists(os.path.join(I, f)): shutil.copy2(os.path.join(I, f), os.path.join(OUT, f))
    check('the owner\'s real library file is unchanged', fstate(OWNER_LIB) == owner0, OWNER_LIB)
    check('copy never on screen / frontmost', not violations, violations)
    lg = os.path.join(OUT, 'dsload.log')
    miss = [l for l in open(lg, errors='replace').read().splitlines() if 'ESC-MISSING' in l] if os.path.exists(lg) else ['no dsload.log']
    check('sweep: no ESC-MISSING line (every window of ours has its Escape registration; loader NOTES "Escape")', not miss, miss[:3])
    fails = [c for c in checks if not c['ok']]
    json.dump({'checks': checks, 'violations': violations}, open(os.path.join(OUT, 'summary.json'), 'w'), indent=1)
    log(f'RESULT cm esc_test: {len(checks) - len(fails)} PASS, {len(fails)} FAIL')
    sys.exit(1 if fails else 0)


if __name__ == '__main__':
    main()
