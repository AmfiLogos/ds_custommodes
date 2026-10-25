#!/usr/bin/env python3
"""Units Laboratory UX fixes (UX review 2026-10-25, U1 / U3-U10; owner decisions of that day). Part 1 reads the language files
(no game). Part 2 runs ONE hidden loader test copy (default UXC: background-only, small window, muted, notch fix off; never
the owner's game, library, saves or screen) with custom-modes + the test module font-probe, in one language. It uses the
harness of esc_test.py (same copy rules, test library, wiki files with the repo's gallery fixture). Fails when (the breaks
this catches):
  - U3 (ru): the Discard box's buttons use the word of Cancel ("Отменить" = discard); U9 (de / ru): "Save to My Units" does
    not say where the build goes; U10 (de): the word "Level" for unit level next to the game's "Stufe";
  - U1: a click on the title page behind My Units or the Unit Builder reaches it (Back closed the page under the open
    window); My Units does not close when the title screen goes away (a world loads: it stayed over the game);
  - U4: the Delete Team box puts the destructive choice first, or dims only the window (the game's box: Cancel first, the
    whole screen dimmed);
  - U5: a window's close control is not the one square X in its top right corner (My Units had a "Close" button, the
    Builder a "Cancel" button);
  - U5 (kit 83636d0+): the loader logs CLOSE-TEXT-X: one of our windows closes with a text "X", not the game's red X;
  - the kit's UI-RULE guard (ds_modloader d529f52+) logs a line: a window of ours breaks an owner rule (red X, UI Scale,
    move, card click, font); split into the title part (our windows) and the test world (in-game panels);
  - U6: a talent tier name does not fit its chip (de "Außergewöhnli / ch"); U7: a boss stat name does not fit (de / ru);
    U8: a text is cut in the Gallery's Details while picking for the Builder ("... Life S...").
  ux_test.py [out] [--lang de|ru|en] [--no-build] [--no-world]     env: as esc_test.py (LD_ROOT, ESC_CM, DSL_TEST_COPY)
Exit 0 = PASS. <out>/summary.json, dsload.log, player.log."""
import json, os, signal, shutil, subprocess, sys, threading, time

HERE = os.path.dirname(os.path.abspath(__file__)); REPO = os.path.dirname(os.path.dirname(HERE))
os.environ.setdefault('DSL_TEST_COPY', 'UXC')
LANG = sys.argv[sys.argv.index('--lang') + 1] if '--lang' in sys.argv else 'de'
if '--lang' in sys.argv: del sys.argv[sys.argv.index('--lang'):sys.argv.index('--lang') + 2]
if not [a for a in sys.argv[1:] if not a.startswith('--')]: sys.argv.insert(1, os.path.join(REPO, 'build', 'ux_test_' + LANG))
sys.path.insert(0, HERE)
import esc_test as E  # noqa: E402  (its globals follow sys.argv / env: OUT, X, LIB, DRV)
from esc_test import check, st, cm, click, until, wait_for, home_on, mu_on, bld_on, wins, log  # noqa: E402
LANGTYPE = {'en': 0, 'de': 3, 'ru': 4}
# the title column shows the translated label ("Einheitenlabor"); the home page's buttons are found by their English key
TITLE = 'title:' + (json.load(open(os.path.join(REPO, 'lang', LANG + '.json'), encoding='utf-8')).get('Units Laboratory', 'Units Laboratory') if LANG != 'en' else 'Units Laboratory')
def open_home():
    E.to_title(); click(TITLE); return until(home_on)
E.open_home = open_home


def req(cmd, timeout=20, **kw):
    """esc_test's req, the request written as UTF-8 (the driver reads no \\u escapes: a Russian label was not found)"""
    E.SEQ[0] += 1; q = dict(kw, seq=E.SEQ[0], cmd=cmd)
    tmp = os.path.join(E.DRV, 'req.json.tmp'); open(tmp, 'w', encoding='utf-8').write(json.dumps(q, ensure_ascii=False)); os.replace(tmp, os.path.join(E.DRV, 'req.json'))
    t0 = time.time()
    while time.time() - t0 < timeout:
        try:
            j = json.load(open(os.path.join(E.DRV, 'resp.json'), encoding='utf-8'))
            if j.get('seq') == E.SEQ[0]: return j
        except (OSError, ValueError): pass
        time.sleep(0.05)
        if not E.game_pids(): raise RuntimeError('the game copy is gone (crash?)')
    return {'timeout': q}
E.req = req


# ------------------------------------------------------------------ part 1: the language files
def lang_checks():
    L = lambda c: json.load(open(os.path.join(REPO, 'lang', c + '.json'), encoding='utf-8'))
    ru, de = L('ru'), L('de')
    cancel = ru.get('Cancel')
    check('U3 ru: Discard is not the word of Cancel', ru.get('Discard') and ru.get('Discard') != cancel and 'Отмен' not in ru.get('Discard', ''), (ru.get('Discard'), cancel))
    keep = ru.get('Keep Editing') or ru.get('Keep editing')
    check('U3 ru: Keep Editing is not the word of Cancel', keep and keep != cancel and keep != ru.get('Discard'), keep)
    check('U9 de: Save to My Units says where (Meine Einheiten)', 'Einheiten' in de.get('Save to My Units', ''), de.get('Save to My Units'))
    check('U9 ru: Save to My Units says where (Мои юниты)', 'юнит' in ru.get('Save to My Units', ''), ru.get('Save to My Units'))
    import re
    lv = [(k, v) for k, v in de.items() for x in (v if isinstance(v, list) else [v]) if re.search(r'\bLevel\b(?!aufstieg)', x)]
    check('U10 de: unit level is "Stufe" (the game\'s word), never "Level"', not lv, lv[:4])


# ------------------------------------------------------------------ part 2: the game
def make_copy():
    app = os.path.join(E.I, 'DungeonSettlers.app')
    if os.path.exists(app): return
    src = os.path.join(E.r.INST, 'Z', 'DungeonSettlers.app'); os.makedirs(E.I, exist_ok=True)
    subprocess.run(['cp', '-c', '-R', src, app], check=True)
    pl = os.path.join(app, 'Contents', 'Info.plist')
    for k, t, v in (('CFBundleIdentifier', '-string', f'com.CanOpener.Dungeon-Settlers.mp{E.X}'), ('CFBundleName', '-string', f'DS rig {E.X}'),
                    ('LSBackgroundOnly', '-bool', 'YES'), ('LSUIElement', '-bool', 'YES')):
        subprocess.run(['plutil', '-replace', k, t, v, pl], check=True)
    subprocess.run(['codesign', '-f', '-s', '-', os.path.join(app, 'Contents', 'MacOS', 'Dungeon Settlers')], capture_output=True)
    log('copy made from', src)


_prep = E.r.prepare_data
def prepare_data(x):                                                  # the copy's language (its own UserSetting.json)
    _prep(x)
    p = os.path.join(E.r.idir(x), 'data', 'UserSetting.json'); s = json.load(open(p))
    s['GeneralSettingData']['SaveCurrentLanguageType'] = LANGTYPE[LANG]; json.dump(s, open(p, 'w'), indent=1)
E.r.prepare_data = prepare_data


def rect(t):
    j = req('rect', target=t); a = j.get('at') or [0, 0, 0, 0]
    return a if j.get('rc', -1) > 0 else None                         # x0 y0 x1 y1, screen pixels (y up)


def x_ok(name, xt, wt):
    """the window's close control: one square X in its top right corner (inside the header band)"""
    x, w = rect(xt), rect(wt)
    if not x or not w: return check(f'U5 {name}: the X and the window are on screen', False, (x, w))
    xw, xh, ww, wh = x[2] - x[0], x[3] - x[1], w[2] - w[0], w[3] - w[1]
    check(f'U5 {name}: the close control is a square X at the top right of the window',
          abs(xw - xh) <= 2 and w[2] - x[2] < ww * 0.06 and w[3] - x[3] < wh * 0.08, (x, w))


WORLD_AT = [None]                                                     # dsload.log size when the test world part starts


def corner(name):
    """the title's corner buttons shown now (Roadmap, Discord: TitleMenuUI/EtcButtonHolderTransform), from the active texts"""
    req('fonts', name=name, all=0); f = os.path.join(E.DRV, f'fonts_{name}.jsonl')
    return [json.loads(l).get('text') for l in open(f, encoding='utf-8', errors='replace') if 'EtcButtonHolderTransform' in l] if os.path.exists(f) else None


def cases():
    lib0 = E.fstate(E.LIB)
    E.to_title(); c0 = corner('u12_title')
    check('U12 (setup) the title shows its corner buttons', bool(c0), c0)
    gal = lambda s: cm(s, 'fights').get('gal') or {}
    tf = lambda s: cm(s, 'fights').get('tf') or {}
    # U1: My Units over the home page takes the clicks of the page (Back under the backdrop)
    s = E.open_mu()
    if not (mu_on(s) and cm(s, 'mu').get('lib') == E.LIB): raise RuntimeError(f'My Units not open on the TEST library: {cm(s, "mu").get("lib")}')
    click('home:Back', 1.2); s = st()
    check('U1 a click on the page\'s Back beside My Units does nothing (both stay)', home_on(s) and mu_on(s), (home_on(s), mu_on(s)))
    x_ok('My Units', 'mu:close', 'mu:win')
    c1 = corner('u12_mu_open')
    check('U12 the title\'s corner buttons do not show over an open window (they drew over its corner, clickable)', c1 == [], c1)
    # U4: the Delete Team box
    click('mu:team0'); click('mu:delete_team'); s = until(lambda s: cm(s, 'mu').get('dlg') == 1)
    d, w = rect('mu:dlg'), rect('mu:win')
    check('U4 the Delete Team box dims the whole screen (not only the window)', bool(d and w) and d[0] < w[0] - 5 and d[2] > w[2] + 5, (d, w))
    c, o, u = rect('mu:dlg_cancel'), rect('mu:dlg_only'), rect('mu:dlg_units')
    check('U4 the game\'s order: Cancel, Team Only, Team and Units', bool(c and o and u) and c[0] < o[0] < u[0], (c, o, u))
    click('mu:dlg_x', 1.0); s = st()
    check('U4 the box\'s red X = Cancel: the box closes, nothing deleted', cm(s, 'mu').get('dlg') == 0 and cm(s, 'mu').get('teams') == 1 and E.fstate(E.LIB) == lib0, (cm(s, 'mu').get('dlg'), cm(s, 'mu').get('teams')))
    # CARD RULE (kit template dsl_card_toggle): a click on a card's text ticks it, a second click unticks it
    click('mu:all', 1.0); t0 = cm(st(), 'mu').get('ticked'); click('mu:card0', 1.0); t1 = cm(st(), 'mu').get('ticked'); click('mu:card0', 1.0); t2 = cm(st(), 'mu').get('ticked')
    check('a click on a My Units card ticks it, a second click unticks it', (t0, t1, t2) == (0, 1, 0), (t0, t1, t2))
    # U5 + U6: the Unit Builder (X = Cancel; the Level-Ups page's talent chips)
    click('mu:new', 1.5); s = until(bld_on)
    x_ok('Unit Builder', 'bld:cancel', 'bld:win')
    s = wait_for(lambda: (lambda s: s if cm(s, 'bld').get('engine') == 2 and not cm(s, 'bld').get('busy') else None)(st()), 60) or st()
    check('the Builder engine is ready (wiki builder files)', cm(s, 'bld').get('engine') == 2, cm(s, 'bld'))
    click('bld:nav:Level-Ups', 2.0); s = st()
    check('U6 every talent tier name fits its chip', cm(s, 'bld').get('tal_cut') == 0, cm(s, 'bld').get('tal_cut'))
    click('bld:cancel', 1.5); s = st()
    check('the X closes the Builder with no change (no notice), My Units stays', not bld_on(s) and mu_on(s), (bld_on(s), mu_on(s)))
    click('mu:close', 1.2); s = st()
    check('the X closes My Units', not mu_on(s) and home_on(s), (mu_on(s), home_on(s)))
    c2 = corner('u12_mu_closed')
    check('U12 the corner buttons are back when our windows are closed', sorted(c2 or []) == sorted(c0 or []), (c2, c0))
    # U1: the Unit Builder from the home page also takes the page's clicks
    E.open_home(); click('home:Unit Builder', 1.5); s = until(bld_on)
    click('home:Back', 1.2); s = st()
    check('U1 a click on the page\'s Back beside the Unit Builder does nothing', home_on(s) and bld_on(s), (home_on(s), bld_on(s)))
    click('bld:cancel', 1.5)
    # U8: the Gallery picking for the Builder: Details with "Use This Unit", nothing cut
    req('call', fn='cmf_test', s='{"cmd":"open","win":"gallery_pick"}')
    s = wait_for(lambda: (lambda s: s if gal(s).get('items', 0) > 0 else None)(st()), 40) or st()
    x_ok('Wiki Build Gallery', 'gallery:close', 'gallery:title')
    click('gallery:c0.view', 1.5); s = until(lambda s: gal(s).get('preview', -1) >= 0)
    check('U8 picking: Details shows "Use This Unit"', rect('gallery:p.pick0') is not None, gal(s).get('preview'))
    cr, pk = rect('gallery:p.crumb0'), rect('gallery:p.pick0')
    check('U8 picking: the stat line is under "Use This Unit", across the whole width (not cut beside it)',
          bool(cr and pk) and cr[3] <= pk[1] + 1 and cr[2] >= pk[2] - 2, (cr, pk))
    check('U8 picking: no text cut in Details', gal(s).get('cut') == 0, gal(s).get('cut'))
    req('call', fn='cmf_test', s='{"cmd":"close","win":"gallery"}'); time.sleep(1.0)
    # U7: Test Fights' boss card
    E.open_home(); click('home:Test Fights', 1.5)
    s = wait_for(lambda: (lambda s: s if tf(s).get('engine') in (2, 3) else None)(st()), 90) or st()
    x_ok('Test Fights', 'fights:close', 'fights:title')
    r = req('call', fn='cmf_test', s='{"cmd":"set","name":"fights:r.mode","index":1}'); time.sleep(1.5); s = st()
    check('U7 every boss stat name fits the card', tf(s).get('mode') == 'boss' and tf(s).get('boss_cut') == 0, (tf(s).get('mode'), tf(s).get('boss_cut')))
    req('call', fn='cmf_test', s='{"cmd":"set","name":"fights:r.mode","index":0}'); time.sleep(1.0)
    check('the test library is unchanged', E.fstate(E.LIB) == lib0)
    if '--no-world' in sys.argv: return
    WORLD_AT[0] = os.path.getsize(os.path.join(E.I, 'dsload.log')) if os.path.exists(os.path.join(E.I, 'dsload.log')) else 0
    # U1: the title goes away (a world loads: Test Fights' Start under My Units here) = My Units closes with it
    req('call', fn='cmf_test', s='{"cmd":"close","win":"fights"}'); time.sleep(1.0)
    E.open_mu()
    req('call', fn='cmf_test', s='{"cmd":"open","win":"fights"}'); s = until(lambda s: 'fights' in wins(s))
    wait_for(lambda: (lambda s: s if tf(s).get('engine') in (2, 3) else None)(st()), 60)
    click('fights:l.team0', 1.5); click('fights:start', 1.0)
    gone = wait_for(lambda: (req('title').get('title') or {}).get('menu') == 0, 240, 2)
    check('a world loads (the title screen went away)', bool(gone), req('title').get('title'))
    s = wait_for(lambda: (lambda s: s if not mu_on(s) else None)(st()), 8) or st()
    check('U1 My Units closed with the title screen (never left over the game)', not mu_on(s) and not bld_on(s), (mu_on(s), bld_on(s)))


def main():
    lang_checks()
    if '--lang-only' in sys.argv: return E_exit()
    if os.getloadavg()[0] > 20: raise SystemExit('load over 20: no test copy now')
    make_copy()
    if E.game_pids(): raise SystemExit(f'copy {E.X} already running: {E.game_pids()}')
    os.makedirs(E.OUT, exist_ok=True)
    if '--no-build' not in sys.argv: E.build()
    E.install(); E.make_library(); owner0 = E.fstate(E.OWNER_LIB)
    g = threading.Thread(target=E.guard, daemon=True); g.start()
    try:
        E.start()
        s = wait_for(lambda: (lambda s: s if s.get('kit') == 1 and req('rect', target=TITLE, timeout=5).get('rc', -1) > 0 else None)(st()), 240, 1)
        check('title reached, Units Laboratory button and the test driver up', bool(s), s and s.get('title_column'))
        time.sleep(3)
        cases()
    except Exception as e:
        check('no exception', False, repr(e))
    finally:
        E.stop.set(); time.sleep(0.6)
        for p in E.game_pids(): os.kill(p, signal.SIGTERM)
        time.sleep(3)
        for p in E.game_pids(): os.kill(p, signal.SIGKILL)
        for f in ('dsload.log', 'player.log'):
            if os.path.exists(os.path.join(E.I, f)): shutil.copy2(os.path.join(E.I, f), os.path.join(E.OUT, f))
    log_txt = open(os.path.join(E.OUT, 'dsload.log'), encoding='utf-8', errors='replace').read() if os.path.exists(os.path.join(E.OUT, 'dsload.log')) else ''
    tx = [l for l in log_txt.splitlines() if 'CLOSE-TEXT-X' in l]
    check('U5 every window closes with the game\'s red X (no CLOSE-TEXT-X in dsload.log)', not tx, tx[:3])
    cut = WORLD_AT[0] if WORLD_AT[0] is not None else len(log_txt.encode('utf-8'))
    raw = log_txt.encode('utf-8'); title_part, world_part = raw[:cut].decode('utf-8', 'replace'), raw[cut:].decode('utf-8', 'replace')
    r1 = [l for l in title_part.splitlines() if 'UI-RULE' in l]; r2 = [l for l in world_part.splitlines() if 'UI-RULE' in l]
    check('no UI-RULE line for our windows on the title (kit guard: red X, UI Scale, card click, font)', not r1, r1[:4])
    if WORLD_AT[0] is not None: check('no UI-RULE line in the test world (in-game panels)', not r2, r2[:4])
    check('the owner\'s real library file is unchanged', E.fstate(E.OWNER_LIB) == owner0, E.OWNER_LIB)
    check('copy never on screen / frontmost', not E.violations, E.violations)
    E_exit()


def E_exit():
    fails = [c for c in E.checks if not c['ok']]
    os.makedirs(E.OUT, exist_ok=True)
    json.dump({'lang': LANG, 'checks': E.checks, 'violations': E.violations}, open(os.path.join(E.OUT, 'summary.json'), 'w'), indent=1, ensure_ascii=False)
    log(f'RESULT cm ux_test ({LANG}): {len(E.checks) - len(fails)} PASS, {len(fails)} FAIL')
    sys.exit(1 if fails else 0)


if __name__ == '__main__':
    main()
