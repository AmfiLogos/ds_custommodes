#!/usr/bin/env python3
"""Units Laboratory > Test Fights squad row (owner 2026-11-02, PLAN_v013 item 30: "where you have written squad 4/4 ... display
the avatar and the NPC icon of each unit that we are bringing to the fight. With maybe their name as well"; answer: "npc icon
is the ingame sprite"), in ONE hidden loader test copy (background-only, small window, muted, notch fix off; never the owner's
game, saves, library or screen). Fails when (the breaks this catches):
  (1) a squad unit has no card next to "Squad: n / 4", for each source: My Units (a team of 4), World units (Pick 1-4 of a
      save) and Wiki Build Gallery builds (the Gallery's Test Fight hand-over);
  (2) a card's portrait is blank (its Image holds no sprite) or its in-game sprite is missing (the game's UnitVisual did not
      paint it);
  (3) a card shows another unit's name, or the cards do not follow the squad (a unit unticked = its card goes, the label
      says 3 / 4).
  (4) a My Units name written with JSON escapes (\\u00ef, \\", an emoji as a surrogate pair) shows its escapes instead of
      the name (lane nvmyunits; the host guard is units/test/mu_test.c t_names).
Uses setup_test.py's driver helpers (the same hidden copy rules). Writes one screenshot of the window (squad_<source>.png).
  squad_test.py   env: HM_BUILD (a loader build: dev-key dsload.dylib, mods/multiplayer, mods/custom-modes), LD_ROOT,
                       SQ_WIN = window size (default 1728x1117), SQ_UISCALE (default 1.1), SQ_OUT = output folder
Output: SQ_OUT: results.json, dsload.log, player.log, squad_*.png."""
import json, os, shutil, sys, threading, time, traceback

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(os.path.dirname(HERE))
OUT = os.environ.get('SQ_OUT') or os.path.join(REPO, 'build', 'test_env', 'squad', time.strftime('%Y%m%d-%H%M%S'))
os.makedirs(OUT, exist_ok=True)
os.environ.setdefault('TS_COPY', 'SQU'); os.environ['TS_OUT'] = OUT     # setup_test's helpers work on this copy and folder
sys.path.insert(0, HERE)
import setup_test as ST  # noqa: E402  (req / call / cmf / click / open_tf / start_copy / zip_dir / probe_pkg)
from setup_test import HP, lrig, log, check  # noqa: E402

X = ST.X
WIN = os.environ.get('SQ_WIN', '1728x1117')
UISCALE = os.environ.get('SQ_UISCALE', '1.1')
FX = json.load(open(os.path.join(REPO, 'units', 'test', 'data', 'launcher_library.json')))['units']


def safe(s):
    """the test export's name text (quotes, backslashes and control characters are spaces)"""
    return ''.join(' ' if c in '"\\' or ord(c) < 0x20 else c for c in s)


# names written with JSON escapes (lane nvmyunits 2026-11-02: they showed their escapes as spaces): main() writes the library
# with ensure_ascii, so the file holds \u00ef, \" and the emoji as a surrogate pair; the window must show the names as typed
ESC_NAMES = ['Ana\u00efs', 'Bj\u00f6rn "BJ"', '\U0001F600 Smile']


def lib_units():
    """the 4 library units of the check: fixture builds, the first three named ESC_NAMES"""
    units = [dict(u) for u in FX[:4]]
    for i, u in enumerate(units):
        u['id'] = f'{0xa0 + i:02x}' * 8
        if i < len(ESC_NAMES):
            u['name'] = ESC_NAMES[i]
    return units


def hover(tag, i, name):
    """the kit's hover card of a squad card: shows, titled with the unit's name, next to the card, inside the screen"""
    t = {}
    ST.req('hover', target=f'fights:squad.card{i}'); time.sleep(2.0)
    for _ in range(6):                                                # the card is moved a few frames after it shows
        t = json.loads(ST.call('tf_test_tip') or '{}'); time.sleep(0.4)
    log('tip', json.dumps(t, ensure_ascii=False))
    check(f'[{tag}] the hover card is titled with the unit\'s name', t.get('title') == safe(name), (t.get('title'), safe(name)))
    ST.judge(tag, f'squad card {i}', t.get('shown') == 1, t.get('card') or [0] * 4, t.get('el') or [0] * 4, t.get('sw') or 0, t.get('sh') or 0, t.get('panel'))
    ST.req('mouse_off'); time.sleep(0.8)


def squad():
    try:
        return json.loads(ST.call('tf_test_squad') or '{}')
    except ValueError as e:
        return {'error': repr(e)}


def judge(tag, want_names, want_label):
    """(1)-(3): one card per unit, in pick order, each with a portrait sprite, an in-game sprite and its own name"""
    q = {}
    for _ in range(10):                                               # the row is drawn on the click's paint
        q = squad()
        if q.get('n') == len(want_names):
            break
        time.sleep(0.5)
    log(tag, json.dumps(q, ensure_ascii=False))
    cards = q.get('cards') or []
    check(f'[{tag}] the row says "{want_label}"', q.get('label') == want_label and q.get('root') == 1, (q.get('label'), q.get('root')))
    check(f'[{tag}] one card per squad unit ({len(want_names)})', q.get('n') == len(want_names) and all(c.get('card') for c in cards), q)
    check(f'[{tag}] every card has its portrait (a sprite, not a blank square)', all(c.get('face') for c in cards), cards)
    check(f'[{tag}] every card has the unit\'s in-game sprite (the game\'s UnitVisual)', all(c.get('sprite') for c in cards), cards)
    check(f'[{tag}] every card names its own unit, in pick order', [c.get('name') for c in cards] == [safe(n) for n in want_names],
          ([c.get('name') for c in cards], [safe(n) for n in want_names]))
    return q


def shot(name):
    p = os.path.join(OUT, name)
    try:
        r = HP.c(X, cmd='mpm_shot', path=p, how='backbuffer')
    except Exception as e:
        r = {'error': repr(e)}
    ok = os.path.exists(p) and os.path.getsize(p) > 1000
    log('screenshot', p, 'ok' if ok else f'MISSING {r}')
    return p if ok else None


def run():
    if not ST.open_tf('first open'):
        return
    judge('nothing picked', [], 'Squad: 0 / 4')
    # My Units: the team of 4 (its tick picks all of them)
    ST.click('fights:l.src0', 1.5); ST.click('fights:l.team0', 2.0)
    team = [u['name'] for u in lib_units()]
    judge('My Units team', team, 'Squad: 4 / 4')
    hover('My Units team', 0, team[0])
    shot('squad_my_units.png')
    # a change: one unit unticked -> its card goes
    ST.click('fights:l.team0.open', 1.5); ST.click('fights:l.team0.u1', 2.0)
    judge('My Units, one unticked', [team[0]] + team[2:], 'Squad: 3 / 4')
    # World units: Pick 1-4 of the seed save
    ST.click('fights:l.src2', 2.0); ST.click('fights:l.world0', 2.5); ST.click('fights:l.world0.pick', 2.5)
    names = [n for n in (ST.tf().get('snames') or '').split('|') if n]
    check('[World units] Pick 1-4 picked units of the save (setup of the check)', len(names) >= 1, ST.tf())
    judge('World units', names, f'Squad: {len(names)} / 4')
    shot('squad_world_units.png')
    # Wiki Build Gallery: the Gallery's Test Fight hand-over (tf_open_with_builds) with 4 builds
    gb = FX[4:8]
    gal = {'title': 'Gallery squad', 'names': [f"{u['name']}  Lv {int(u['build'].get('lvl') or 1)}" for u in gb], 'builds': [u['build'] for u in gb]}
    log('gallery hand-over', ST.call('tf_test_squad_gal', json.dumps(gal, ensure_ascii=False)))
    time.sleep(2.5)
    check('[Gallery] Test Fights shows the Gallery source', ST.tf().get('src') == 1, ST.tf())
    judge('Gallery builds', [u['name'] for u in gb], 'Squad: 4 / 4')
    shot('squad_gallery.png')


def main():
    o0 = (os.path.getmtime(ST.OWNER_LIB) if os.path.exists(ST.OWNER_LIB) else None)
    g = threading.Thread(target=HP.guard, daemon=True); g.start()
    res = {}
    try:
        units = lib_units()
        os.makedirs(os.path.dirname(ST.LIB), exist_ok=True)
        json.dump({'format': 'ds-launcher-units', 'version': 1, 'game_build': '25846389', 'units': units,       # \u escapes (ESC_NAMES)
                   'teams': [{'id': 'cc' * 8, 'name': 'Squad check', 'unit_ids': [u['id'] for u in units]}]}, open(ST.LIB, 'w', encoding='utf-8'), indent=1, ensure_ascii=True)
        os.makedirs(ST.GAME_SAVES, exist_ok=True); shutil.copy2(ST.SAVE, os.path.join(ST.GAME_SAVES, os.path.basename(ST.SAVE)))
        if os.path.exists(ST.DATA):                                   # a fresh data folder each run (never deleted: set aside)
            aside = os.path.expanduser(f'~/ToDelete_{time.strftime("%Y-%m-%d")}/squad_test/data_{time.strftime("%H%M%S")}')
            os.makedirs(os.path.dirname(aside), exist_ok=True); shutil.move(ST.DATA, aside)
        HP.make_copy(X)
        ST.tf_fresh.fresh_settings(ST.DATA, OUT)
        pk = {'custom-modes': ST.zip_dir(os.path.join(HP.BUILD, 'mods', 'custom-modes'), 'custom-modes'), 'font-probe': ST.probe_pkg()}
        HP.kit(X, pk, ['custom-modes', 'font-probe'], ['multiplayer', 'custom-modes', 'font-probe'])
        for f in ('OutGame.json', 'UserCampaignLog.json'):
            shutil.copy2(os.path.join(ST.SEED, f), os.path.join(ST.DATA, f))
        ww, wh = (int(v) for v in WIN.split('x'))                     # the window size: the copy's settings and its start arguments
        lrig.win_size = lambda: (ww, wh)
        popen = HP.subprocess.Popen
        def popen_size(args, *a, **kw):
            if '-screen-width' in args:
                args = list(args); args[args.index('-screen-width') + 1] = str(ww); args[args.index('-screen-height') + 1] = str(wh)
            return popen(args, *a, **kw)
        HP.subprocess.Popen = popen_size
        prep = lrig.prepare_data
        def prep_scale(x):
            prep(x); us = os.path.join(lrig.idir(x), 'data', 'UserSetting.json'); u = json.load(open(us))
            u['GraphicSettingData']['UIScale'] = float(UISCALE); json.dump(u, open(us, 'w'), indent=1)
        lrig.prepare_data = prep_scale
        ST.start_copy()
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
        check("the owner's library file is unchanged", (os.path.getmtime(ST.OWNER_LIB) if os.path.exists(ST.OWNER_LIB) else None) == o0)
        for f in ('dsload.log', 'player.log'):
            p = os.path.join(lrig.idir(X), f)
            if os.path.exists(p):
                shutil.copy2(p, os.path.join(OUT, f))
        dl = os.path.join(OUT, 'dsload.log')
        rules = [l.strip()[:200] for l in open(dl, encoding='utf-8', errors='replace') if 'UI-RULE' in l] if os.path.exists(dl) else ['no dsload.log']
        check('no UI-RULE line from the kit\'s guard', not rules, rules[:4])
        res.update({'checks': HP.checks, 'all_pass': all(k['ok'] for k in HP.checks) and bool(HP.checks)})
        json.dump(res, open(os.path.join(OUT, 'results.json'), 'w'), indent=1, default=str)
        log('RESULT', 'PASS' if res['all_pass'] else 'FAIL', f"{sum(k['ok'] for k in HP.checks)}/{len(HP.checks)}", os.path.join(OUT, 'results.json'))
    sys.exit(0 if res.get('all_pass') else 1)


if __name__ == '__main__':
    main()
