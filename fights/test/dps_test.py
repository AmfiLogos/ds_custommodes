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
PLACE = os.environ.get('DPS_PLACE')                            # placement-only run (UI scale / window size), its tag
UISCALE = os.environ.get('DPS_UISCALE')
WIN = os.environ.get('DPS_WIN')                                # e.g. 1920x1080
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


def meter(cmd='state'):
    r = req('call', fn='tf_meter_test', s=cmd)
    try:
        return json.loads(r.get('out') or '{}')
    except ValueError:
        log('tf_meter_test answer not JSON', cmd, (r.get('out') or '')[:300]); return {}


def hits_dump(frm=0):
    out = []
    for l in dlog(f'hits {frm}').split('\n'):
        if l.strip():
            i, d, res, u, rest = l.split(' ', 4); k, _, nm = rest.partition('|')
            out.append({'i': int(i), 'dmg': float(d), 'res': int(res), 'unit': int(u), 'key': k, 'skill': nm})
    return out


def overlap(a, b):
    return bool(a and b and len(a) == 4 and len(b) == 4 and a[0] < b[2] and b[0] < a[2] and a[1] < b[3] and b[1] < a[3])


def placement(tag):
    m = meter(); r, g = m.get('rect'), m.get('game_rects') or {}
    log('meter placement', tag, json.dumps({'rect': r, 'game': g, 'place': m.get('place')})[:600])
    check(f'(placement {tag}) the DPS meter shows on the left and covers neither the resource list nor the unit panel',
          m.get('shown') and r and r[2] > r[0] and not overlap(r, g.get('resources')) and not overlap(r, g.get('unitpanel')),
          json.dumps({'rect': r, 'game': g}))
    shot(f'meter_{tag}')


def unit_spots():
    p = dps('pos').get('b', [])
    return sorted((round(x['w'][0], 2), round(x['w'][1], 2)) for x in p if x.get('u') == 1 and x.get('on')), \
        (sum(1 for x in p if x.get('u') == 0), sum(1 for x in p if x.get('u') == 1))


def chat_lines():
    chat = dlog('chat').split('\n')
    return int(chat[0] or 0), int(chat[1] or 0), chat[2:2 + int(chat[1] or 0)]


BASIC = {None: 'Basic Attack', '3': 'Grundangriff', '4': 'Базовая атака', '7': '基本攻撃'}


def run():
    start_fight()
    shot(f'world_{LANG or "en"}')
    spots0, counts0 = unit_spots()
    at = select_dummy()
    check('the scarecrow selected by a click, the mouse kept on it (inspect panel shows it)', at is not None)
    if not at:
        return
    s = dps('state'); log('state', json.dumps(s, ensure_ascii=False)[:800])
    lay = req('call', fn='tf_dps_test', s='layout').get('out') or ''
    open(os.path.join(OUT, 'layout.json'), 'w').write(lay)
    # (a) text fit at the dummy's max, both ways round: the FULL numbers
    fits(dps('fit 1000000 1000000'), f'(a) "{fmt(1000000)} / {fmt(1000000)}" one line each ({LANG or "en"})')
    shot(f'panel_full_{LANG or "en"}')
    r = dps('fit 999999 1000000'); fits(r, f'(a) "{fmt(999999)} / {fmt(1000000)}" one line each ({LANG or "en"})')
    check('(a) the texts are the full numbers with the language\'s separator', (r.get('cur') or {}).get('text') == fmt(999999) and (r.get('max') or {}).get('text') == fmt(1000000),
          ((r.get('cur') or {}).get('text'), (r.get('max') or {}).get('text')))
    shot(f'panel_999999_{LANG or "en"}')
    n = order_attack(at)
    check('the squad ordered to attack the scarecrow (right click, as a player)', n >= 1, n)
    until(lambda: dps('state').get('hits', 0) > 5, 'the squad hits the scarecrow', 120, 1)
    s = dps('state'); check('the scarecrow selected again, the mouse kept on it', s.get('same') and s.get('bpanel_on'), json.dumps(s)[:300])
    if PLACE:
        time.sleep(4); placement(PLACE); return
    placement(f'{LANG or "en"}_1.0_960')
    # (b) stock: the panel stands still while the mouse stays on it; ours follows
    dps('live 0'); s0 = dps('state'); time.sleep(4); s1 = dps('state')
    info['stock_panel_still'] = (s0.get('cur') or {}).get('text') == (s1.get('cur') or {}).get('text') and s1.get('dur') < s0.get('dur')
    log('stock (follow off): panel', (s0.get('cur') or {}).get('text'), '->', (s1.get('cur') or {}).get('text'), 'while durability', s0.get('dur'), '->', s1.get('dur'))
    dps('live 1'); time.sleep(0.5)
    ok, seen = True, []
    for _ in range(8):                                            # the read is between frames: the text may be one frame behind
        time.sleep(0.5); s = dps('state'); t = (s.get('cur') or {}).get('text') or ''; seen.append((t, s.get('dur')))
        v = int(''.join(ch for ch in t if ch.isdigit()) or -1)
        if abs(v - int(s.get('dur', -1))) > 5000:                   # a frame of hits / a repair step
            ok = False
    check('(b) the hovered panel follows the durability (8 reads over 4 s, each the building\'s durability now, at most a frame behind)', ok and len({t for t, _ in seen}) >= 3, seen)
    shot(f'panel_live_{LANG or "en"}')
    # (b2) the Inspect window (the button above the unit panel) follows too
    btn = next((x for x in json.loads(req('call', fn='tf_dps_test', s='summary').get('out') or '[]') if x.get('text') in ('Inspect', 'Untersuchen', 'Осмотреть', '調査')), None)
    log('inspect button', btn)
    if btn:
        r = btn['r']; req('click_xy', x=(r[0] + r[2]) / 2, y=(r[1] + r[3]) / 2); time.sleep(2.0)
        ok, seen = True, []
        for _ in range(8):
            time.sleep(0.5); s = dps('state'); t = s.get('ins_txt') or ''; seen.append((t, s.get('dur')))
            v = int(''.join(ch for ch in t if ch.isdigit()) or -1)
            if not s.get('ins') or abs(v - int(s.get('dur', -1))) > 5000:
                ok = False
        check('(b2) the Inspect window\'s durability follows the scarecrow (8 reads over 4 s, full number form)', ok and len({t for t, _ in seen}) >= 3 and any(',' in t or '.' in t or ' ' in t for t, _ in seen), seen)
        mi = meter(); time.sleep(1.2); mi = meter()                 # the meter moves right of the window within 30 frames
        check('(b2) the DPS meter does not cover the open Inspect window', mi.get('rect') and (mi.get('game_rects') or {}).get('inspect') and not overlap(mi['rect'], mi['game_rects']['inspect']),
              json.dumps({'meter': mi.get('rect'), 'inspect': (mi.get('game_rects') or {}).get('inspect')}))
        shot(f'inspect_live_{LANG or "en"}')
        req('click_xy', x=(r[0] + r[2]) / 2, y=(r[1] + r[3]) / 2); time.sleep(1.0)
    else:
        check('(b2) the Inspect button found above the unit panel', False)
    req('click_xy', x=at[0], y=at[1]); time.sleep(1.0); req('hover_xy', x=at[0], y=at[1]); time.sleep(0.5)
    # (c) + (d) + speech / heal numbers off: a 30 s burst (defaults: speech off, heal numbers off, log in chat on)
    s0 = dps('state')
    check('switch defaults: speech off, heal numbers off, log in chat on, start on first hit on', s0.get('sw') == [0, 0, 1, 1], s0.get('sw'))
    rows = [(time.time(), s0['hits'], s0['sum'], s0['dur'], s0['repaired'])]; n0 = s0['lines']; h0 = s0['heads']
    t0 = time.time(); fx_bad = []; k = 0
    while time.time() - t0 < BURST:
        time.sleep(0.25); s = dps('state'); rows.append((time.time(), s['hits'], s['sum'], s['dur'], s['repaired'])); k += 1
        if k % 6 == 0:
            v = dps('vfx')
            if v.get('speech') or any(x.strip().lstrip('+') == '200' for x in v.get('texts', [])):
                fx_bad.append(v)
    n1, h1 = s['lines'], s['heads']                               # the same read as the last row
    json.dump(rows, open(os.path.join(OUT, 'burst.json'), 'w'))
    lost = rows[0][3] - rows[-1][3]; dealt = rows[-1][2] - rows[0][2]; hits = rows[-1][1] - rows[0][1]; rep = rows[-1][4] - rows[0][4]
    # per read: logged damage - durability lost = what the self repair added in that read (counted on its own path)
    bad = [(round((b[2] - a[2]) - (a[3] - b[3]), 3), round(b[4] - a[4], 3)) for a, b in zip(rows, rows[1:])
           if abs((b[2] - a[2]) - (a[3] - b[3]) - (b[4] - a[4])) > 0.07 * max(1, b[1] - a[1]) + 0.07]
    info.update({'burst_hits': hits, 'burst_dealt': dealt, 'burst_lost': lost, 'burst_repaired': rep})
    check(f'(c) {hits} hits in {BURST} s; their sum = the durability lost + what the self repair added (every 0.25 s read)', hits > 20 and rep > 0 and not bad,
          json.dumps({'dealt': dealt, 'lost': lost, 'repaired': rep, 'bad_reads': bad[:5]}))
    check('speech + heal numbers off: no speech bubble and no "200" over the scarecrow in 30 s of hits (the repair went on)', not fx_bad and rep > 0,
          json.dumps({'bad': fx_bad[:3], 'counters': {k2: s.get(k2) for k2 in ('speech_shown', 'speech_hidden', 'heal_shown', 'heal_hidden')}}))
    mine = dlog(str(n0)).split('\n')[:n1 - n0]
    hl = [m for m in mine if m.startswith('<indent=')]
    check('(d) one indented line per hit over the burst, a header line per change of unit', len(hl) == hits and len(mine) - len(hl) == h1 - h0, (len(mine), len(hl), hits, h1 - h0))
    hd = hits_dump(rows[0][1])[:hits]
    rawkey = [h for h in hd if not h['skill'] or h['skill'].startswith(('SKILL_', 'AFFECTER_', 'TEXTKEY_'))]
    basic_ok = all(h['skill'] == BASIC.get(LANG, h['skill']) for h in hd if h['key'].startswith(('SKILL_Basic', 'SKILL_NPCBasic')))
    check('each hit names its skill, auto attacks as the game\'s "Basic attack", never a raw key', not rawkey and basic_ok and any(h['key'].startswith('SKILL_Basic') for h in hd),
          json.dumps({'raw': rawkey[:3], 'skills': sorted({(h['key'], h['skill']) for h in hd})[:8]}, ensure_ascii=False))
    total, inhist, cl = chat_lines()
    tail = cl[n0:n1]                                              # the chat's local lines = this fight's log from its start
    same = [a for a, b in zip(mine, tail) if a != b]
    check('(d) every burst line is in the multiplayer chat, in order (its local lines)', total >= n1 and inhist == total and len(tail) == len(mine) and not same,
          json.dumps({'chat_total': total, 'in_history': inhist, 'log': n1, 'first_diff': same[:2]}, ensure_ascii=False))
    req('key', code=36); time.sleep(1.5); shot(f'chat_open_{LANG or "en"}')      # Enter: the box with the history
    req('key', code=53); time.sleep(0.5)
    # (e) (f) (g) the DPS meter: Reset (armed: the next hit starts it), 8 s, Stop
    click('dps:firsthit', 0.5); click('dps:reset', 1.0); m = meter(); time.sleep(2); m_b = meter()   # "Start on first hit" off: Reset stays at 0
    check('(f) Reset clears the meter (and it stays at 0 with "Start on first hit" off)', m.get('total') == 0 and m.get('elapsed') == 0 and not m.get('running') and m_b.get('total') == 0 and not m_b.get('running'),
          json.dumps({'after': [m.get('total'), m.get('elapsed'), m.get('running')], '2s': [m_b.get('total'), m_b.get('running')], 'sw': dps('state').get('sw')}))
    click('dps:firsthit', 0.5); click('dps:reset', 0.5)            # back on: the next hit starts it
    time.sleep(8); click('dps:unit0', 1.0); shot(f'meter_skills_{LANG or "en"}')
    click('dps:stop', 0.5); m1 = meter(); h_a = dps('state')['hits']; time.sleep(4); m2 = meter(); h_b = dps('state')['hits']
    check('(f) Stop freezes the numbers while the squad goes on hitting', m1.get('total', 0) > 0 and m1.get('total') == m2.get('total') and m1.get('elapsed') == m2.get('elapsed') and h_b > h_a,
          json.dumps({'m1': [m1.get('total'), m1.get('elapsed')], 'm2': [m2.get('total'), m2.get('elapsed')], 'hits': [h_a, h_b]}))
    cnt, frm = [], 0
    while True:                                                   # the counted hits, a page at a time
        pg = json.loads(req('call', fn='tf_meter_test', s=f'counted {frm}').get('out') or '{}')
        cnt += pg.get('hits') or []; frm = len(cnt)
        if not pg.get('more') or not pg.get('hits'):
            break
    byi = {h['i']: h for h in hits_dump(0)}
    idx = [c[0] for c in cnt]
    sums_ok = abs(sum(c[1] for c in cnt) - m2.get('total', -1)) < 1e-3 and all(abs(byi[i]['dmg'] - d) < 1e-3 for i, d in cnt if i in byi) and all(i in byi for i in idx)
    contiguous = idx == list(range(idx[0], idx[0] + len(idx))) if idx else False
    check('(e) the meter\'s total = the sum of the logged hits it counted (the same hits, one run of the log)', sums_ok and contiguous and len(cnt) > 3,
          json.dumps({'n': len(cnt), 'total': m2.get('total'), 'sum': sum(c[1] for c in cnt), 'first': idx[:3], 'last': idx[-3:]}))
    gbad = []
    for u in m2.get('units') or []:
        sk = u.get('skills') or []
        if abs(sum(x['dmg'] for x in sk) - u['dmg']) > 1e-3 or sum(x['hits'] for x in sk) != u['hits']:
            gbad.append(u)
    check('(g) each unit\'s per-skill rows add up to its row', m2.get('units') and not gbad, json.dumps(gbad[:2], ensure_ascii=False)[:400])
    check('the meter shows the self repair on its own line', m2.get('repaired', -1) >= 0, m2.get('repaired'))
    click('dps:start', 0.5)
    # "DPS log in chat" off: no chat line, the meter counts on
    click('dps:chatlog', 0.5); c0 = chat_lines()[0]; ma = meter().get('total', 0); time.sleep(15); c1 = chat_lines()[0]; mb = meter().get('total', 0)
    check('"DPS log in chat" off: 0 new chat lines in 15 s of hits, the meter still counts', c1 == c0 and mb > ma and not dps('state')['sw'][2], (c0, c1, ma, mb))
    click('dps:chatlog', 0.5)
    # speech + heal numbers on: they show again
    click('dps:speech', 0.3); click('dps:healnum', 0.3); seen_sp = seen_heal = False
    for _ in range(24):
        time.sleep(0.5); v = dps('vfx')
        seen_sp = seen_sp or v.get('speech', 0) > 0
        seen_heal = seen_heal or any(x.strip().lstrip('+') == '200' for x in v.get('texts', []))
        if seen_sp and seen_heal:
            break
    shot(f'speech_on_{LANG or "en"}')
    check('speech + heal numbers on: a bubble and a "200" show over the scarecrow (12 s)', seen_sp and seen_heal, (seen_sp, seen_heal, dps('state').get('sw')))
    click('dps:speech', 0.3); click('dps:healnum', 0.3)
    # our own panel (no chat / "Chat: Off"), forced on here
    dps('fb 1'); time.sleep(1.5); f = dps('fb_state'); t = (f.get('text') or {}).get('text', '')
    check('our own log panel (forced on): shown, 10 lines', f.get('fb_on') == 1 and len(t.split('|')) == 10, json.dumps(f, ensure_ascii=False)[:400])
    shot(f'fallback_panel_{LANG or "en"}'); dps('fb 0')
    # Restart test, twice: a fresh world (scarecrow full, the squad at its start spots, the same objects), the meter at 0
    for r_i in (1, 2):
        click('dps:restart', 0.5)
        until(lambda: (lambda s: s.get('on') == 1 and s.get('title') == 0 and s.get('hits', 9) < 3)(dps('state')), f'the restarted world ({r_i})', 300, 2)
        time.sleep(8)
        spots, counts = unit_spots(); m = meter(); total, inhist, cl = chat_lines()
        at2 = select_dummy(); s = dps('state')
        check(f'Restart test #{r_i}: the scarecrow full, the squad at its start spots, the same objects, the meter at 0, "--- restart ---" in the chat',
              at2 is not None and s.get('dur') == s.get('dmax') == 1000000.0 and spots == spots0 and counts == counts0 and m.get('total') == 0 and cl[:1] and 'restart' in cl[0],
              json.dumps({'dur': s.get('dur'), 'spots_same': spots == spots0, 'counts': [counts, counts0], 'meter': m.get('total'), 'chat0': cl[:1]}, ensure_ascii=False))
        if r_i == 1:
            shot(f'restarted_{LANG or "en"}')
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
        if WIN:                                                   # another window size: the copy's settings and its start arguments
            ww, wh = (int(v) for v in WIN.split('x'))
            lrig.win_size = lambda: (ww, wh)
            popen = HP.subprocess.Popen
            def popen_size(args, *a, **kw):
                if '-screen-width' in args:
                    args = list(args); args[args.index('-screen-width') + 1] = str(ww); args[args.index('-screen-height') + 1] = str(wh)
                return popen(args, *a, **kw)
            HP.subprocess.Popen = popen_size
        if LANG or UISCALE:
            prep = lrig.prepare_data
            def prep_lang(x):
                prep(x); us = os.path.join(lrig.idir(x), 'data', 'UserSetting.json'); s = json.load(open(us))
                if LANG:
                    s['GeneralSettingData']['SaveCurrentLanguageType'] = int(LANG)
                if UISCALE:
                    s['GraphicSettingData']['UIScale'] = float(UISCALE)
                json.dump(s, open(us, 'w'), indent=1)
            lrig.prepare_data = prep_lang
        crash = os.path.join(OUT, 'crash'); os.makedirs(crash, exist_ok=True); open(os.path.join(crash, 'answer'), 'w').write('no')
        cmset = os.path.join(lrig.idir(X), 'data', 'Mods', 'custom-modes', 'settings.json')   # the lab's switches: fresh defaults
        if os.path.exists(cmset):
            os.replace(cmset, os.path.join(OUT, 'old_settings.json'))
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
