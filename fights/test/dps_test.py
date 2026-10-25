#!/usr/bin/env python3
"""Test Fights > DPS Check while it runs (owner 2026-10-25) in ONE hidden loader test copy (background-only, small window,
muted, notch fix off; never the owner's game, saves, library or screen) with multiplayer + custom-modes + the test module
font-probe (the units test driver: fake mouse, game frame capture, "call" into custom-modes' tf_dps_test).
A 4-unit team from the launcher fixture (a TEST library) -> Units Laboratory > Test Fights > DPS Check > Start (the wiki's
own save generator) -> the world loads; the squad hits the practice scarecrow (1,000,000 durability, self repair).
Fails when (the breaks this catches):
  (a) the inspect panel's durability text "<now> / <max>" wraps (more than one line) or runs past the panel's bar at
      1,000,000 / 1,000,000 or 999,999 / 1,000,000 (stock: "1000000" wrapped its last "0" onto a second line);
  (b) the scarecrow's panel (selected, the mouse kept on it) does not follow its durability while it takes damage (stock:
      it stays as it was when the panel was filled; the run shows that with our follow switched off);
  (c) the logged hits do not add up to the durability it lost over the burst (self repair counted apart: a rise between
      two reads);
  (d) a hit is missing from the chat over a 30 s burst (every logged line, in order, in the multiplayer chat's local lines).
  --no-mp: the multiplayer mod off: (d) reads our own log panel's lines instead (it shows; its line count = the hits).
  dps_test.py [--no-mp]   env: HM_BUILD = a loader build folder (dsload.dylib with the dev key, mods/multiplayer,
                                mods/custom-modes); LD_ROOT = a ds_modloader checkout (loader/test helpers; ~/ds_launcher);
                                LANGTYPE = the game's language (3 de, 4 ru, 7 ja; default en)
Output: DPS_OUT (default <HM_BUILD>/../test_env/dps/<time>): results.json, frames in drv/cap/."""
import json, os, shutil, subprocess, sys, threading, time, traceback, zipfile

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(os.path.dirname(HERE))
LD = os.path.expanduser(os.environ.get('LD_ROOT', '~/ds_launcher'))
sys.path.insert(0, os.path.join(LD, 'loader', 'test'))
import hostmods_pair as HP  # noqa: E402  (copies, kits, start/stop, owner screen guard)
from hostmods_pair import lrig, log, check  # noqa: E402

X = 'DPS'; HP.H = HP.G = X
NO_MP = '--no-mp' in sys.argv
OUT = os.environ.get('DPS_OUT') or os.path.join(os.path.dirname(HP.BUILD), 'test_env', 'dps', time.strftime('%Y%m%d-%H%M%S'))
os.makedirs(OUT, exist_ok=True)
DRV = os.path.join(OUT, 'drv'); os.makedirs(os.path.join(DRV, 'cap'), exist_ok=True)
LANG = os.environ.get('LANGTYPE')
LIB = os.path.join(OUT, 'lib', 'library.json')
COL_LAB = 5                                                   # Units Laboratory's child index in the title column
BURST = 30
REPAIR = 200.0


def zip_dir(src, mid):
    zp = os.path.join(OUT, 'pkg', f'{mid}.zip'); os.makedirs(os.path.dirname(zp), exist_ok=True)
    with zipfile.ZipFile(zp, 'w', zipfile.ZIP_DEFLATED) as zf:
        for r, _, fs in os.walk(src):
            for f in sorted(fs):
                if not f.startswith('.'):
                    zf.write(os.path.join(r, f), os.path.join(mid, os.path.relpath(os.path.join(r, f), src)))
    return zp


def probe_pkg():
    fp = os.path.join(REPO, 'home', 'test', 'font_probe')
    d = os.path.join(OUT, 'pkg', 'font-probe'); os.makedirs(d, exist_ok=True)
    subprocess.run(['nice', '-n', '19', 'clang', '-arch', 'arm64', '-O2', '-Wall', '-Wno-unused-function', '-I' + os.path.join(LD, 'loader', 'sdk'),
                    '-dynamiclib', '-framework', 'CoreFoundation', '-framework', 'CoreGraphics', '-lobjc', '-install_name', '@rpath/font-probe.dylib',
                    '-o', os.path.join(d, 'font-probe.dylib'), os.path.join(fp, 'font_probe.c')], check=True)
    subprocess.run(['codesign', '-f', '-s', '-', os.path.join(d, 'font-probe.dylib')], capture_output=True)
    shutil.copy2(os.path.join(fp, 'mod.json'), os.path.join(d, 'mod.json'))
    return zip_dir(d, 'font-probe')


def make_library():
    fx = json.load(open(os.path.join(REPO, 'units', 'test', 'data', 'launcher_library.json')))
    units = fx['units'][:4]
    for i, u in enumerate(units):                                 # the fixture units carry no weapon: a squad must hit
        u['id'] = f'{0xb0 + i:02x}' * 8
        u['build'].setdefault('eq', {})['Primary'] = ('ITEM_CopperSword', 'ITEM_CopperBow', 'ITEM_FireCopperStaff', 'ITEM_CopperSpear')[i]
    lib = {'format': 'ds-launcher-units', 'version': 1, 'game_build': '25846389', 'units': units,
           'teams': [{'id': 'dd' * 8, 'name': 'DPS squad', 'unit_ids': [u['id'] for u in units]}]}
    os.makedirs(os.path.dirname(LIB), exist_ok=True)
    json.dump(lib, open(LIB, 'w'), indent=2)


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
        time.sleep(0.1)
    raise RuntimeError(f'no answer to {q}')


def dps(s):
    r = req('call', fn='tf_dps_test', s=s)
    try:
        return json.loads(r.get('out') or '{}')
    except ValueError:
        log('tf_dps_test answer not JSON', s, (r.get('out') or '')[:300]); return {}


def dlog(s):
    return req('call', fn='tf_dps_test_log', s=s, timeout=60).get('out') or ''


def click(t, wait=1.5):
    r = req('click', target=t); time.sleep(wait)
    log('click', t, r.get('rc'), r.get('text'))
    return r.get('rc') == 0


def shot(name):
    time.sleep(1.0); req('capture', name=name); log('shot', name)


def column():
    return [c.split('|') for c in (req('state').get('title_column') or '').split(';') if c]


def by_index(rows, i):
    return next((r[0] for r in rows if len(r) > 2 and r[2] == str(i) and r[1] == '1'), None)


def until(pred, what, timeout, every=1.0):
    t0 = time.time()
    while time.time() - t0 < timeout:
        v = pred()
        if v:
            return v
        time.sleep(every)
    raise RuntimeError(f'timed out: {what}')


def fmt(v):
    sep = {'3': '.', '4': ' '}.get(LANG, ',')
    s = f'{int(v):,}'
    return s.replace(',', sep)


def fits(r, label):
    """both texts on one line; the drawn "<now>" between the bar box's left edge and the "/", the drawn "<max>" between
    the "/" and the box's right edge (world x of TMP's textBounds against the boxes of the bar's Amount part)"""
    c, m, bx = r.get('cur') or {}, r.get('max') or {}, {b['name']: b['x'] for b in r.get('boxes') or []}
    par, div = (r.get('boxes') or [{}])[0].get('x'), bx.get('Text_Divider')
    eps = 0.002 * abs((par or [0, 1])[1] - (par or [0, 1])[0])
    ok = c.get('lines') == 1 and m.get('lines') == 1 and par and div and c.get('ink') and m.get('ink')
    if ok:
        (c0, c1), (m0, m1) = c['ink'], m['ink']
        ok = par[0] - eps <= c0 < c1 <= div[0] + eps and div[1] - eps <= m0 < m1 <= par[1] + eps
    return check(label, ok, json.dumps({'scale': r.get('scale'), 'cur': c, 'max': m, 'boxes': r.get('boxes')}, ensure_ascii=False))


def tfs():
    return ((req('state').get('cm') or {}).get('fights') or {}).get('tf') or {}


def start_fight():
    ok = False
    for _ in range(3):                                            # the title column differs without multiplayer: by label first
        rows = column()
        lab = next((r[0] for r in rows if r and r[0] == 'Units Laboratory' and len(r) > 1 and r[1] == '1'), None) or by_index(rows, COL_LAB) or 'Units Laboratory'
        click('title:' + lab, 3)
        if req('rect', target='home:Test Fights').get('rc', -1) > 0 and click('home:Test Fights', 3):
            ok = True; break
    check('Units Laboratory > Test Fights opened', ok)
    until(lambda: tfs().get('engine') == 2, "the wiki's Test Fights files", 300, 2)
    click('fights:l.team0', 2)
    tf = tfs()
    check('the 4-unit team is the squad, mode DPS Check', tf.get('squad') == 4 and tf.get('mode') == 'dps', json.dumps(tf))
    click('fights:start', 1)
    until(lambda: (lambda s: s.get('on') == 1 and s.get('title') == 0)(dps('state')), 'the DPS Check world', 600, 3)
    time.sleep(8)


def select_dummy():
    """click the scarecrow (its BuildingObject's screen point, a few points around it) until the panel shows it"""
    p = dps('pos'); log('pos', json.dumps(p)[:600])
    b = next((x for x in p.get('b', []) if 'scarecrow' in (x.get('pk', '') + x.get('go', '')).lower()), None)
    if not b:
        return None
    for dx, dy in ((0, 0), (0, 12), (0, 24), (0, 36), (8, 18), (-8, 18), (0, 48)):
        x, y = b['s'][0] + dx, b['s'][1] + dy
        req('click_xy', x=x, y=y); time.sleep(1.2); req('hover_xy', x=x, y=y); time.sleep(0.8)
        s = dps('state')
        if s.get('sr') and s.get('view_on') and s.get('bpanel_on') and s.get('same') and s.get('dmax', 0) >= 999999:
            log('selected at', x, y)
            return (x, y)
    return None


def order_attack(at):
    """each squad unit: a left click on it (selected: the panel leaves the building), then a right click on the scarecrow
    (the player's attack order); then the scarecrow is selected again and the mouse stays on it"""
    us = [b for b in dps('pos').get('b', []) if b.get('u') == 1 and b.get('on')]
    log('units', [(u['go'], u['s']) for u in us])
    n = 0
    for u in us:
        for dx, dy in ((0, 0), (0, 12), (0, 24), (0, 36)):
            req('click_xy', x=u['s'][0] + dx, y=u['s'][1] + dy); time.sleep(1.0)
            if not dps('state').get('bpanel_on'):
                req('click_xy', x=at[0], y=at[1], button=1); time.sleep(1.0); n += 1
                break
    req('click_xy', x=at[0], y=at[1]); time.sleep(1.0); req('hover_xy', x=at[0], y=at[1]); time.sleep(0.5)
    return n


def run():
    start_fight()
    shot(f'world_{LANG or "en"}')
    at = select_dummy()
    check('the scarecrow selected by a click, the mouse kept on it (inspect panel shows it)', at is not None)
    if not at:
        return
    s = dps('state'); log('state', json.dumps(s, ensure_ascii=False)[:800])
    lay = req('call', fn='tf_dps_test', s='layout').get('out') or ''
    open(os.path.join(OUT, 'layout.json'), 'w').write(lay)
    # (a) text fit at the dummy's max, both ways round
    fits(dps('fit 1000000 1000000'), f'(a) "{fmt(1000000)} / {fmt(1000000)}" one line each ({LANG or "en"})')
    shot(f'panel_full_{LANG or "en"}')
    r = dps('fit 999999 1000000'); fits(r, f'(a) "{fmt(999999)} / {fmt(1000000)}" one line each ({LANG or "en"})')
    check('(a) the texts are the numbers with the language\'s separator', (r.get('cur') or {}).get('text') == fmt(999999) and (r.get('max') or {}).get('text') == fmt(1000000),
          ((r.get('cur') or {}).get('text'), (r.get('max') or {}).get('text')))
    shot(f'panel_999999_{LANG or "en"}')
    # (b) stock: the panel stands still while the mouse stays on it; ours follows
    n = order_attack(at)
    check('the squad ordered to attack the scarecrow (right click, as a player)', n >= 1, n)
    until(lambda: dps('state').get('hits', 0) > 5, 'the squad hits the scarecrow', 120, 1)
    s = dps('state'); check('the scarecrow selected again, the mouse kept on it', s.get('same') and s.get('bpanel_on'), json.dumps(s)[:300])
    dps('live 0'); s0 = dps('state'); time.sleep(4); s1 = dps('state')
    stock_still = (s0.get('cur') or {}).get('text') == (s1.get('cur') or {}).get('text') and s1.get('dur') < s0.get('dur')
    log('stock (follow off): panel', (s0.get('cur') or {}).get('text'), '->', (s1.get('cur') or {}).get('text'), 'while durability', s0.get('dur'), '->', s1.get('dur'))
    info['stock_panel_still'] = stock_still
    dps('live 1'); time.sleep(0.5)
    ok, seen = True, []
    for _ in range(8):                                            # the read is between frames: the text may be one frame behind
        time.sleep(0.5); s = dps('state'); t = (s.get('cur') or {}).get('text') or ''; seen.append((t, s.get('dur')))
        v = int(''.join(ch for ch in t if ch.isdigit()) or -1)
        if abs(v - int(s.get('dur', -1))) > 5000:                   # a frame of hits / a repair step
            ok = False
    check('(b) the hovered panel follows the durability (8 reads over 4 s, each the building\'s durability now, at most a frame behind)', ok and len({t for t, _ in seen}) >= 3, seen)
    shot(f'panel_live_{LANG or "en"}')
    # (c) + (d): a 30 s burst
    s0 = dps('state'); rows = [(time.time(), s0['hits'], s0['sum'], s0['dur'])]; n0 = s0['lines']
    t0 = time.time()
    while time.time() - t0 < BURST:
        time.sleep(0.25); s = dps('state'); rows.append((time.time(), s['hits'], s['sum'], s['dur']))
    n1 = s['lines']                                               # the same read as the last row
    json.dump(rows, open(os.path.join(OUT, 'burst.json'), 'w'))
    lost = rows[0][3] - rows[-1][3]; dealt = rows[-1][2] - rows[0][2]; hits = rows[-1][1] - rows[0][1]
    rises = [(round(b[0] - rows[0][0], 2), round(b[3] - a[3], 3)) for a, b in zip(rows, rows[1:]) if b[3] > a[3] + 1e-3]
    # per read: logged damage - durability lost = what the self repair gave back, a whole number of its steps (game table
    # AFFECTER_SelfRepair FloatValues [200]: 200 a step); a hit missing from the log or a wrong logged value breaks that
    gaps = [((b[2] - a[2]) - (a[3] - b[3]), b[1] - a[1]) for a, b in zip(rows, rows[1:])]
    bad = [(round(g, 3), h) for g, h in gaps if abs(g - REPAIR * round(g / REPAIR)) > 0.07 * max(1, h) + 0.07 or g < -0.07 * max(1, h) - 0.07]
    repair = sum(REPAIR * round(g / REPAIR) for g, _ in gaps)
    info.update({'burst_hits': hits, 'burst_dealt': dealt, 'burst_lost': lost, 'repair_rises': rises, 'repair_total': repair})
    check(f'(c) {hits} hits in {BURST} s; their sum = the durability lost + the self repair\'s whole steps (every 0.25 s read)', hits > 20 and not bad,
          json.dumps({'dealt': dealt, 'lost': lost, 'repair': repair, 'rises': rises[:5], 'bad_reads': bad[:5]}))
    mine = dlog(str(n0)).split('\n')[:n1 - n0]
    check('(d) one log line per hit over the burst', n1 - n0 == hits and len(mine) == hits, (n1 - n0, hits, len(mine)))
    if NO_MP:
        s = dps('state')
        check('(d) no multiplayer mod: our own log panel shows them', s.get('fb') == 1 and s.get('fb_on') == 1, json.dumps(s)[:300])
        shot(f'fallback_panel_{LANG or "en"}')
    else:
        chat = dlog('chat').split('\n')
        total, inhist, cl = int(chat[0] or 0), int(chat[1] or 0), chat[2:2 + int(chat[1] or 0)]
        mine_esc = [m.replace('<', '<​') for m in mine]            # the chat shows tags as typed (escaped)
        tail = cl[n0:n1]                                              # the chat's local lines = this fight's log from its start
        same = [a for a, b in zip(mine_esc, tail) if a != b]
        check('(d) every burst line is in the multiplayer chat, in order (its local lines)', total >= n1 and inhist == total and len(tail) == len(mine) and not same,
              json.dumps({'chat_total': total, 'in_history': inhist, 'log': n1, 'first_diff': same[:2], 'esc_hint': mine_esc[:1]}, ensure_ascii=False))
        req('key', code=36); time.sleep(1.5); shot(f'chat_open_{LANG or "en"}')      # Enter: the box with the history
        req('key', code=53); time.sleep(0.5)
    if not NO_MP:                                                 # our own panel (no chat / "Chat: Off"), forced on here
        dps('fb 1'); time.sleep(1.5); f = dps('fb_state'); t = (f.get('text') or {}).get('text', '')
        last = dlog(str(max(0, f.get('lines', 1) - 1))).strip().replace('"', "'").replace('\\', "'")
        check('our own log panel (forced on): shown, 10 lines, the newest = the log\'s newest', f.get('fb_on') == 1 and len(t.split('|')) == 10 and t.split('|')[-1] == last,
              json.dumps({'fb': f, 'last': last}, ensure_ascii=False)[:600])
        shot(f'fallback_panel_{LANG or "en"}')
        dps('fb 0')
    log('last lines', json.dumps(dps('state').get('last'), ensure_ascii=False))


info = {}
def main():
    g = threading.Thread(target=HP.guard, daemon=True); g.start()
    res = {}
    try:
        make_library()
        HP.make_copy(X)
        pk = {'custom-modes': zip_dir(os.path.join(HP.BUILD, 'mods', 'custom-modes'), 'custom-modes'), 'font-probe': probe_pkg()}
        HP.kit(X, pk, ['custom-modes', 'font-probe'], (['custom-modes', 'font-probe'] if NO_MP else ['multiplayer', 'custom-modes', 'font-probe']))
        if LANG:
            prep = lrig.prepare_data
            def prep_lang(x):
                prep(x); us = os.path.join(lrig.idir(x), 'data', 'UserSetting.json'); s = json.load(open(us))
                s['GeneralSettingData']['SaveCurrentLanguageType'] = int(LANG); json.dump(s, open(us, 'w'), indent=1)
            lrig.prepare_data = prep_lang
        crash = os.path.join(OUT, 'crash'); os.makedirs(crash, exist_ok=True); open(os.path.join(crash, 'answer'), 'w').write('no')
        HP.room_for(1)
        HP.start(X, 'http://127.0.0.1:9', {'DSL_UNITS_TEST': DRV, 'DSL_CRASHREP_TEST': crash, 'DSMP_UNITS_LIB': LIB})
        until(lambda: req('state', timeout=10).get('title_column'), 'the title', 1500, 3); time.sleep(6)
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
        for f in ('dsload.log', 'player.log', 'rig.log'):
            p = os.path.join(lrig.idir(X), f)
            if os.path.exists(p):
                shutil.copy2(p, os.path.join(OUT, f))
        res.update({'checks': HP.checks, 'info': info, 'all_pass': all(k['ok'] for k in HP.checks) and bool(HP.checks)})
        json.dump(res, open(os.path.join(OUT, 'results.json'), 'w'), indent=1, default=str)
        log('RESULT', 'PASS' if res['all_pass'] else 'FAIL', f"{sum(k['ok'] for k in HP.checks)}/{len(HP.checks)}", os.path.join(OUT, 'results.json'))
    sys.exit(0 if res.get('all_pass') else 1)


if __name__ == '__main__':
    main()
