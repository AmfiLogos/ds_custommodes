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
      it stays as it was when the panel was filled). With the loader's Fix live-hp (HM_BUILD/mods/live-hp, in the kit)
      that fix does the follow and Units Laboratory puts its number form on it; --no-live-hp: live-hp not in the kit,
      Units Laboratory's own follow must do it (a test tool always shows live HP; lead 2026-10-25);
  (c) the logged hits do not add up to the durability it lost over the burst (self repair counted apart: a rise between
      two reads);
  (d) a hit is missing from the chat over a 30 s burst (every logged line, in order, in the multiplayer chat's local lines).
  --no-mp: the multiplayer mod off: (d) reads our own log panel's lines instead (it shows; its line count = the hits).
  dps_test.py [--no-mp] [--no-live-hp]   env: HM_BUILD = a loader build folder (dsload.dylib with the dev key, mods/multiplayer,
                                mods/custom-modes, mods/live-hp); LD_ROOT = a ds_modloader checkout (loader/test helpers; ~/ds_launcher);
                                LANGTYPE = the game's language (3 de, 4 ru, 7 ja; default en)
Output: DPS_OUT (default <HM_BUILD>/../test_env/dps/<time>): results.json, frames in drv/cap/."""
import collections, json, os, re, shutil, subprocess, sys, threading, time, traceback, zipfile

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(os.path.dirname(HERE))
LD = os.path.expanduser(os.environ.get('LD_ROOT', '~/ds_launcher'))
sys.path.insert(0, os.path.join(LD, 'loader', 'test'))
import hostmods_pair as HP  # noqa: E402  (copies, kits, start/stop, owner screen guard)
from hostmods_pair import lrig, log, check  # noqa: E402

X = 'DPS'; HP.H = HP.G = X
NO_MP = '--no-mp' in sys.argv
HEAL = '--heal' in sys.argv                                   # a Custom fight: a healer + barrier squad that monsters attack
LIVE_HP = '--no-live-hp' not in sys.argv
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
NAMEKEY, NAMED = 'TEXTKEY_PLAYERNAME_Elf_Male_13', {None: 'Silvarion'}   # TextKeyTable_en
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
    if HEAL:                                                      # healer (Healing Touch), barriers: Iron Clad (7), Fire Cloak (4, 1)
        units = [fx['units'][i] for i in (24, 7, 4, 1)]
        units[1]['build'].setdefault('eq', {})['Primary'] = 'ITEM_CopperSword'
        for i, u in enumerate(units):
            u['id'] = f'{0xc0 + i:02x}' * 8
        lib = {'format': 'ds-launcher-units', 'version': 1, 'game_build': '25846389', 'units': units,
               'teams': [{'id': 'ee' * 8, 'name': 'Heal squad', 'unit_ids': [u['id'] for u in units]}]}
        os.makedirs(os.path.dirname(LIB), exist_ok=True)
        json.dump(lib, open(LIB, 'w'), indent=2)
        return
    units = fx['units'][:4]
    for i, u in enumerate(units):                                 # the fixture units carry no weapon: a squad must hit
        u['id'] = f'{0xb0 + i:02x}' * 8
        u['build'].setdefault('eq', {})['Primary'] = ('ITEM_CopperSword', 'ITEM_NatureCopperStaff', 'ITEM_FireCopperStaff', 'ITEM_CopperSpear')[i]
    units[0]['name'] = units[0]['build']['name'] = NAMEKEY                # a unit with the game's default name key (never renamed)
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
    sw_left = [n for n in ('speech', 'healnum', 'chatlog', 'effects') if req('rect', target=f'fights:r.dps_{n}').get('rc', -1) > 0]
    check('round 2: no DPS switches on the Test Fights start screen (they are on the in-game meter)', not sw_left, sw_left)
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
    """every event: amount, result, source unit, kind (0 damage, 1 heal, 2 absorb, 3 effect), game tick, target, on the scarecrow, basic"""
    out = []
    for l in dlog(f'hits {frm}').split('\n'):
        if l.strip():
            i, d, res, u, kind, tick, tg, od, basic, rest = l.split(' ', 9); k, _, nm = rest.partition('|')
            out.append({'i': int(i), 'dmg': float(d), 'res': int(res), 'unit': int(u), 'kind': int(kind), 'tick': int(tick), 'target': int(tg),
                        'on_dummy': int(od), 'basic': int(basic), 'key': k, 'skill': nm})
    return out


def units_dump():
    out = {}
    for l in dlog('units').split('\n'):
        if l.strip():
            i, pl, dm, nm = l.split(' ', 3); out[int(i)] = {'player': int(pl), 'dummy': int(dm), 'name': nm}
    return out


PART = re.compile(r'^(?P<name>.+?)(?: \u00d7(?P<n>\d+))? \u00bb (?P<rest>.+)$')


def parse_line(ln):
    """an indented chat line -> its parts: (name, n, targets text, signed amounts, raw)"""
    t = re.sub(r'<[^>]*>', '', ln).replace('\u200b', '')
    parts = []
    for raw in t.split(' \u00b7 '):
        m = PART.match(raw.strip())
        if not m:
            parts.append({'raw': raw, 'name': raw.split(' (')[0], 'n': 1, 'amts': [], 'absorb': ' absorbed ' in raw or ':' in raw}); continue
        amts = [int(a) for a in re.findall(r'([-+]\d+)', m.group('rest'))]
        parts.append({'raw': raw, 'name': m.group('name'), 'n': int(m.group('n') or 1), 'amts': amts, 'rest': m.group('rest')})
    return parts


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


def chat_lines(frm=0):
    """the chat's local lines ever added, those in its history now, and the history's local lines from frm on"""
    chat = dlog(f'chat {frm}').split('\n')
    return int(chat[0] or 0), int(chat[1] or 0), [x for x in chat[2:] if x != ''] if len(chat) > 2 else []


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
    t_first = time.time(); n30 = [None]                          # events logged in the first 30 s of hits (Auto Skill check below)
    s = dps('state'); check('the scarecrow selected again, the mouse kept on it', s.get('same') and s.get('bpanel_on'), json.dumps(s)[:300])
    check('the follow is ' + ('live-hp' if LIVE_HP else "Units Laboratory's own (no live-hp)"), s.get('live_hp') == (1 if LIVE_HP else 0), s.get('live_hp'))
    if PLACE:
        time.sleep(4); placement(PLACE); return
    placement(f'{LANG or "en"}_1.0_960')
    # (b) the panel follows while the mouse stays on it (live-hp; the stock game keeps the value it had when filled)
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
    check('switch defaults: speech off, heal numbers off, log in chat on, start on first hit on, effects in log on', s0.get('sw') == [0, 0, 1, 1, 1], s0.get('sw'))
    rows = [(time.time(), s0['hits'], s0['sum'], s0['dur'], s0['repaired'])]; n0 = s0['lines']; h0 = s0['heads']
    t0 = time.time(); fx_bad = []; k = 0
    while time.time() - t0 < BURST:
        time.sleep(0.25); s = dps('state'); rows.append((time.time(), s['hits'], s['sum'], s['dur'], s['repaired'])); k += 1
        if n30[0] is None and time.time() - t_first >= 30:
            n30[0] = s['events']
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
    ev30 = [h for h in hits_dump(0) if n30[0] is None or h['i'] < n30[0]]; un = units_dump()
    act = sorted({h['skill'] for h in ev30 if h['kind'] == 0 and not h['basic'] and h['key'].startswith('SKILL_') and un.get(h['unit'], {}).get('player')})
    info['active_skills_30s'] = act
    check(f'the squad casts its active skills (Auto Skill on): hits of {len(act)} active skills in the first 30 s of the fight', act, act)
    check('speech + heal numbers off: no speech bubble and no "200" over the scarecrow in 30 s of hits (the repair went on)', not fx_bad and rep > 0,
          json.dumps({'bad': fx_bad[:3], 'counters': {k2: s.get(k2) for k2 in ('speech_shown', 'speech_hidden', 'heal_shown', 'heal_hidden')}}))
    mine = dlog(str(n0)).split('\n')[:n1 - n0]
    # (d) ask 10: one indented line per source unit and game tick; parts = the events of that tick, same source = "×n total"
    st = dps('state'); L, E = st['lines'], st['ev_done']
    alll = dlog('0').split('\n')[:L]; ev = hits_dump(0)[:E]; un = units_dump()
    json.dump({'lines': alll, 'events': ev, 'units': un}, open(os.path.join(OUT, 'log_events.json'), 'w'), ensure_ascii=False, indent=0)
    groups, order = {}, []
    for h in ev:
        k = (h['unit'], h['tick'])
        if k not in groups:
            groups[k] = []; order.append(k)
        groups[k].append(h)
    ind = [x for x in alll if x.startswith('<indent=')]
    gbad = []
    for gi, k in enumerate(order[:len(ind)]):
        want = {}
        for h in groups[k]:
            w = want.setdefault((h['kind'], h['skill'][:28]), {'n': 0, 'tot': 0.0})
            w['n'] += 1; w['tot'] += h['dmg']
        got = parse_line(ind[gi])
        for (kind, nm), w in want.items():
            g = next((p for p in got if p['name'].startswith(nm.split('...')[0][:20])), None)
            if not g or (kind in (0, 1) and (g['n'] != w['n'] or abs(sum(abs(a) for a in g['amts']) - round(w['tot'])) > max(1, w['n']))):
                gbad.append({'line': re.sub(r'<[^>]*>', '', ind[gi])[:160], 'want': [kind, nm, w], 'got': got[:3]}); break
    check('(d) one indented line per unit and game tick (no long line split here), each part "×n" with the events\' count and total',
          len(ind) == len(order) and not gbad, json.dumps({'lines': len(ind), 'groups': len(order), 'bad': gbad[:2]}, ensure_ascii=False)[:700])
    signs, unnamed = [], []
    for ln in ind:
        for p in parse_line(ln):
            nm = p['name']
            if not nm.strip() or nm.startswith(('SKILL_', 'AFFECTER_', 'TEXTKEY_', 'STATUS_')):
                unnamed.append(p['raw'])
            if 'rest' in p and ':' in p['rest'] and not re.search(r'[-+]\d|Miss|Verfehlt', p['rest']) and 'absorb' not in p['raw']:
                signs.append(p['raw'])
            if 'rest' in p and not p['rest'].split(':')[0].strip():
                signs.append(p['raw'])
    check('every part names its source (never empty, never a raw key), its target(s), and a signed amount (damage -X, heal +X)', not unnamed and not signs,
          json.dumps({'unnamed': unnamed[:3], 'unsigned_or_no_target': signs[:3]}, ensure_ascii=False))
    dmg_parts = [p for ln in ind for p in parse_line(ln) if 'rest' in p and '-' in p['rest']]
    heal_ev = [h for h in ev if h['kind'] == 1]
    check('damage parts show "-", heal parts "+" (heals in the log: %d)' % len(heal_ev), all(a < 0 for p in dmg_parts for a in p['amts'] if '-' in p['rest'] and '+' not in p['rest'])
          and all(any(f'+{round(h["dmg"])}' in re.sub(r'<[^>]*>', '', x) or '\u00d7' in x for x in ind) for h in heal_ev[:5]), len(heal_ev))
    basic_ev = [h for h in ev if h['basic']]
    BS = {None: ' (basic)', '3': ' (Grundangriff)', '4': ' (базовая)', '7': '（基本）'}.get(LANG, ' (basic)')
    check('basic attacks show their real name + "(basic)"', basic_ev and all(h['skill'].endswith(BS) for h in basic_ev),
          json.dumps(sorted({(h['key'], h['skill']) for h in ev})[:10], ensure_ascii=False))
    heads = sorted({re.sub(r'<[^>]*>', '', m).replace('\u200b', '') for m in alll if m and not m.startswith('<indent=') and 'restart' not in m})
    check('the log names each unit with the game\'s display name (a default name key shows as its name, never the key)',
          heads and not any(h.startswith('TEXTKEY_') for h in heads) and (LANG or NAMED[None] in heads), heads)
    eff = [h for h in ev if h['kind'] == 3]
    eff_missing = [h for h in eff if not any(h['skill'][:20] in re.sub(r'<[^>]*>', '', x) for x in ind)]
    info['effects'] = len(eff); info['absorbs'] = sum(1 for h in ev if h['kind'] == 2); info['heals'] = len(heal_ev)
    check('effects put on show in the log (effects in log on): %d effects' % len(eff), not eff_missing, json.dumps(eff_missing[:3], ensure_ascii=False))
    total, inhist, cl = chat_lines(n0)
    json.dump({'log': mine, 'chat': cl[:n1 - n0], 'n0': n0, 'n1': n1}, open(os.path.join(OUT, 'chat_vs_log.json'), 'w'), ensure_ascii=False, indent=0)
    tail = cl[:n1 - n0]                                              # the chat's local lines = this fight's log from its start
    same = [a for a, b in zip(mine, tail) if a != b]
    check('(d) every burst line is in the multiplayer chat, in order (its local lines)', total >= n1 and inhist == total and len(tail) == len(mine) and not same,
          json.dumps({'chat_total': total, 'in_history': inhist, 'log': n1, 'first_diff': same[:2]}, ensure_ascii=False))
    # round 2 item 4: an effect re-applied at its cap writes no line (Reckless Assault: 1 stack, no duration, re-applied every tick)
    ra = collections.Counter(h['target'] for h in ev if h['kind'] == 3 and h['key'] == 'AFFECTER_RecklessAssault')
    capped = (dps('state').get('ev') or {}).get('effect_capped', 0)
    info.update({'reckless_lines': dict(ra), 'effect_capped': capped})
    check(f'round 2: no line for an effect re-applied at its cap (Reckless Assault lines per unit {dict(ra)}, {capped} re-applies at the cap left out)',
          (not ra or max(ra.values()) <= 2) and capped > 20, (dict(ra), capped))
    # round 2 item 6: unit names in the chat's own font (no <b>: it drew another face); only the colour differs
    heads = [x for x in chat_lines()[2] if x and not x.startswith('<indent=')]
    check('round 2: the unit name lines use the chat\'s font (no <b> / <font> tag, colour only)', heads and not any('<b>' in x or '<font' in x for x in heads), heads[:3])
    # round 2 item 5: "Effects in Log" off hides the effect parts already in the chat at once; on shows them again
    effre = re.compile('<color=#A69085>[^<]*\u00bb')
    def eff_n():
        return sum(1 for x in chat_lines()[2] if effre.search(x))
    e_on = eff_n(); click('dps:effects', 1.0); e_off, c_off = eff_n(), len(chat_lines()[2]); click('dps:effects', 1.0); e_back = eff_n()
    check('round 2: "Effects in Log" off hides the effect lines already in the chat at once, on shows them again',
          e_on > 0 and e_off == 0 and c_off > 0 and e_back >= e_on and dps('state')['sw'][4] == 1, (e_on, e_off, c_off, e_back))
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
    contiguous = all(byi[i]['kind'] == 0 and byi[i]['on_dummy'] for i in idx if i in byi) and len(idx) > 0   # only damage on the scarecrow (no heal, no absorb)
    check('(e) the meter\'s total = the sum of the logged damage on the scarecrow it counted (no heal, no absorb counted as damage)', sums_ok and contiguous and len(cnt) > 3,
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
    # round 2 item 2: the meter moves with a right-click drag (the kit's MOVE RULE), stays there and keeps its place
    r0 = meter().get('rect') or [0, 0, 0, 0]
    req('drag_xy', x=(r0[0] + r0[2]) / 2, y=max(r0[1], r0[3]) - 10, dx=160, dy=-90, button=1); time.sleep(2.5)
    r1 = meter().get('rect') or [0, 0, 0, 0]; time.sleep(3); m3 = meter(); r2 = m3.get('rect') or [0, 0, 0, 0]
    places = os.path.join(lrig.idir(X), 'data', 'Mods', 'ui_places.tsv')
    kept = os.path.exists(places) and 'custom-modes.dps_meter' in open(places, encoding='utf-8', errors='replace').read()
    check('round 2: the DPS meter moves with a right-click drag (its top-left by the drag), stays there 3 s later and its place is kept',
          abs((r1[0] - r0[0]) - 160) < 20 and abs((r1[3] - r0[3]) + 90) < 20 and r2 == r1 and (m3.get('moved') or [0])[0] == 1 and kept,
          json.dumps({'before': r0, 'after': r1, '3s': r2, 'moved': m3.get('moved'), 'kept': kept}))
    check('round 2: "Restart Test Floor" fits its button (no ellipsis)', (m3.get('restart_cut') or [1])[0] == 0, m3.get('restart_cut'))
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


def cmf(**kw):
    r = req('call', fn='cmf_test', s=json.dumps(kw))
    try:
        return json.loads(r.get('out') or '{}')
    except ValueError:
        return {}


def group_check(L, E, kinds=(0, 1, 2)):
    """the lines [0, L) against the events [0, E): one indented line per (source, tick); each part of a kind in `kinds` with
    the events' count and total (absorb: "absorbed N")"""
    alll = dlog('0').split('\n')[:L]; ev = hits_dump(0)[:E]
    groups, order = {}, []
    for h in ev:
        k = (h['unit'], h['tick'])
        if k not in groups:
            groups[k] = []; order.append(k)
        groups[k].append(h)
    ind = [x for x in alll if x.startswith('<indent=')]
    bad = []
    for gi, k in enumerate(order[:len(ind)]):
        want = {}
        for h in groups[k]:
            w = want.setdefault((h['kind'], h['skill'][:28]), {'n': 0, 'tot': 0.0})
            w['n'] += 1; w['tot'] += h['dmg']
        raw = ind[gi]; got = parse_line(raw); plain = re.sub(r'<[^>]*>', '', raw)
        for (kind, nm), w in want.items():
            if kind not in kinds:
                continue
            if kind == 2:
                m = re.search(r'%s \(.*?\): absorbed (\d+)' % re.escape(nm.split('...')[0][:20]), plain)
                if not m or abs(int(m.group(1)) - round(w['tot'])) > 1:
                    bad.append({'line': plain[:160], 'want': [kind, nm, w]})
                continue
            g = next((p for p in got if p['name'].startswith(nm.split('...')[0][:20])), None)
            shown = sum(abs(a) for a in g['amts']) if g else 0
            if g and len(g['amts']) == 1:                         # every target got the same amount: shown once, after the last target
                shown *= len(g['rest'].split(': ')[0].split(', '))
            if not g or g['n'] != w['n'] or abs(shown - round(w['tot'])) > max(1, w['n']):
                bad.append({'line': plain[:160], 'want': [kind, nm, w], 'got': got[:3]})
            elif kind == 1 and ('<color=#7EC86B>' not in raw or '+' not in g.get('rest', '')):
                bad.append({'line': raw[:200], 'heal_not_green_or_unsigned': nm})
    return alll, ev, ind, order, bad


def run_heal():
    """a Custom fight (monsters attack the squad); the log and meter forced on for it (test switch: the same code as in a
    DPS Check, which has no attacker)"""
    rows = column()
    lab = next((r[0] for r in rows if r and r[0] == 'Units Laboratory' and len(r) > 1 and r[1] == '1'), None) or by_index(rows, COL_LAB) or 'Units Laboratory'
    click('title:' + lab, 3); click('home:Test Fights', 3)
    until(lambda: tfs().get('engine') == 2, "the wiki's Test Fights files", 300, 2)
    click('fights:l.team0', 2)
    cmf(cmd='set', name='fights:r.mode', index=2); time.sleep(1.5)
    check('Custom fight chosen for the healer + barrier squad', tfs().get('mode') == 'arena' and tfs().get('squad') == 4, json.dumps(tfs()))
    cmf(cmd='set', name='fights:r.moncount', index=2); time.sleep(0.5)        # 3 of each
    added = 0
    for q in ('Rat', 'Goblin', 'Slime', 'Spider', 'Bat'):           # plain monsters (not the Abyss ones: they would wipe the squad)
        cmf(cmd='type', name='fights:r.monq', text=q); time.sleep(1.2)
        for h in range(12):
            r = req('rect', target=f'fights:r.hit{h}')
            if r.get('rc', -1) <= 0:
                break
            if 'abyss' not in (r.get('text') or '').lower():
                log('monster', r.get('text')); cmf(cmd='click', name=f'fights:r.hit{h}'); time.sleep(1.0); added += 1
                break
        if added == 2:
            break
    check('monsters added to the room', added > 0, added)
    dps('force 1')
    click('fights:start', 1)
    until(lambda: (lambda s: s.get('on') == 1 and s.get('title') == 0)(dps('state')), 'the Custom fight world', 600, 3)
    time.sleep(6); click('dps:start', 0.5)
    p = dps('pos').get('b', []); json.dump(p, open(os.path.join(OUT, 'heal_pos.json'), 'w'))
    squad = [b for b in p if b.get('u') == 1 and b.get('on') and 'PlayerUnit' in b.get('go', '')]
    foes = [b for b in p if b.get('u') == 1 and b.get('on') and 'PlayerUnit' not in b.get('go', '')]
    log('squad', len(squad), 'foes', [(f['go'], f['w']) for f in foes][:6])
    if foes:                                                      # each unit: select it, right-click the nearest monster (a player's order)
        for u in squad:
            f = min(foes, key=lambda f: (f['w'][0] - u['w'][0]) ** 2 + (f['w'][1] - u['w'][1]) ** 2)
            for dy in (0, 12, 24):
                req('click_xy', x=u['s'][0], y=u['s'][1] + dy); time.sleep(0.8)
                if not dps('state').get('bpanel_on'):
                    break
            req('click_xy', x=f['s'][0], y=f['s'][1] + 12, button=1); time.sleep(0.8)
    t0 = time.time()
    while time.time() - t0 < 40 and (dps('state').get('ev') or {}).get('dmg', 0) < 5:   # the fight is on
        time.sleep(1)
    b = dps('barrier 150 600 4')                                  # test only: the next 4 effects on squad units become the game's Barrier (small: hits still get through, the healer heals)
    check('test barrier armed (the game\'s Barrier through its own AffecterSimulator)', b.get('ok') == 1, b)
    t0 = time.time()
    while time.time() - t0 < 120:
        ev = dps('state').get('ev') or {}
        if ev.get('heal', 0) >= 4 and ev.get('absorb', 0) >= 2:
            break
        time.sleep(3)
    time.sleep(3); click('dps:stop', 0.5); time.sleep(1.0)
    shot('heal_fight_meter')
    st = dps('state'); m = meter()
    alll, ev, ind, order, bad = group_check(st['lines'], st['ev_done'])
    json.dump({'lines': alll, 'events': ev, 'units': units_dump(), 'meter': m}, open(os.path.join(OUT, 'heal_events.json'), 'w'), ensure_ascii=False, indent=0)
    un = units_dump()
    heals = [h for h in ev if h['kind'] == 1]; absorbs = [h for h in ev if h['kind'] == 2]
    info['barrier'] = dps('barrier_state')
    byi = {h['i']: h for h in ev}                                 # an absorb follows its hit's damage event (same source, target, tick)
    pair = [(byi.get(a['i'] - 1), a) for a in absorbs]
    pair = [(d, a) for d, a in pair if d and d['kind'] == 0 and d['unit'] == a['unit'] and d['target'] == a['target'] and d['tick'] == a['tick']]
    blocked = [d for d, a in pair if abs(d['dmg']) < 0.5]
    neg = [d for d, a in pair if d['dmg'] < -0.5]
    info.update({'absorb_pairs': len(pair), 'fully_blocked': len(blocked), 'absorb_sum': sum(a['dmg'] for a in absorbs)})
    check(f'an absorbed hit is never damage: every absorb has its hit, and a fully blocked hit logs 0 damage ({len(blocked)} of {len(pair)})',
          absorbs and len(pair) == len(absorbs) and blocked and not neg, json.dumps([[d['dmg'], a['dmg']] for d, a in pair[:8]]))
    info.update({'heal_events': len(heals), 'absorb_events': len(absorbs), 'dmg_events': sum(1 for h in ev if h['kind'] == 0)})
    check(f'heals and absorbs happened in the fight ({len(heals)} heals, {len(absorbs)} absorbs)', heals and absorbs, st.get('ev'))
    check('every heal and absorb is in the log: one line per unit and tick, heal "+X" in green with its source and target, absorb on its own "absorbed N"',
          len(ind) == len(order) and not bad, json.dumps({'lines': len(ind), 'groups': len(order), 'bad': bad[:3]}, ensure_ascii=False)[:800])
    heads_ok = True
    cur = None
    for x in alll:                                                # a heal part sits under its healer's header
        if x and not x.startswith('<indent='):
            cur = re.sub(r'<[^>]*>', '', x).replace('​', '')
        elif x.startswith('<indent=') and '#7EC86B' in x and cur is None:
            heads_ok = False
    healers = {un[h['unit']]['name'] for h in heals}
    check('heal lines sit under the healer\'s header', heads_ok and all(any(re.sub(r'<[^>]*>', '', x).replace('​', '') == n for x in alll) for n in healers), sorted(healers))
    spans = m.get('spans') or []
    def inspan(i):
        return any(a <= i < (b if b is not None and b >= 0 else 10 ** 9) for a, b in spans)
    full = hits_dump(0)                                           # every event (the meter's spans may reach past the lines' last tick)
    hs = sum(h['dmg'] for h in full if h['kind'] == 1 and inspan(h['i']) and un.get(h['unit'], {}).get('player'))
    asum = sum(h['dmg'] for h in full if h['kind'] == 2 and inspan(h['i']))
    dsum = sum(h['dmg'] for h in full if h['kind'] == 0 and h['on_dummy'] and inspan(h['i']))
    check('the meter\'s Healing = the sum of the squad\'s heal lines it measured; Absorbed = the sum of the absorbs; damage dealt holds neither',
          abs(m.get('heal', -1) - hs) < 0.01 and abs(m.get('absorbed', -1) - asum) < 0.01 and abs(m.get('total', -1) - dsum) < 0.01 and hs > 0,
          json.dumps({'meter': [m.get('heal'), m.get('absorbed'), m.get('total')], 'logged': [hs, asum, dsum], 'spans': spans[:3]}))
    # summons count for their squad unit (lead 2026-10-25): "Strange Seed (summon)" under the owner's header and in its meter row
    sm = [h for h in heals if h['skill'].endswith('(summon)')]
    owners = {un[h['unit']]['name'] for h in sm}
    srow = [(u['name'], x['skill'], x.get('heal', 0)) for u in m.get('units') or [] for x in u.get('skills') or [] if x['skill'].endswith('(summon)')]
    under, cur = True, None
    for x in alll:
        if x and not x.startswith('<indent='):
            cur = re.sub(r'<[^>]*>', '', x).replace('\u200b', '')
        elif '(summon)' in x and cur not in owners:
            under = False
    check(f'summon heals count for their squad unit: {len(sm)} "(summon)" heals under {sorted(owners)}, meter rows {srow[:3]}',
          sm and all(un[h['unit']]['player'] for h in sm) and under and any(r[0] in owners and r[2] > 0 for r in srow), json.dumps({'n': len(sm), 'rows': srow[:4]}, ensure_ascii=False))
    uh = {u['name']: u for u in m.get('units') or []}
    sk_bad = [u for u in uh.values() if abs(sum(x.get('heal', 0) for x in u.get('skills') or []) - u.get('heal', 0)) > 1e-3]
    check('each unit\'s Healing = its skills\' Healing', not sk_bad and any(u.get('heal', 0) > 0 for u in uh.values()), json.dumps(sk_bad[:2], ensure_ascii=False)[:300])
    req('key', code=36); time.sleep(1.5); shot('heal_fight_chat'); req('key', code=53)


info = {}
def main():
    g = threading.Thread(target=HP.guard, daemon=True); g.start()
    res = {}
    try:
        make_library()
        HP.make_copy(X)
        pk = {'custom-modes': zip_dir(os.path.join(HP.BUILD, 'mods', 'custom-modes'), 'custom-modes'), 'font-probe': probe_pkg()}
        lh = ['live-hp'] if LIVE_HP else []
        if LIVE_HP: pk['live-hp'] = zip_dir(os.path.join(HP.BUILD, 'mods', 'live-hp'), 'live-hp')
        HP.kit(X, pk, ['custom-modes'] + lh + ['font-probe'], (['custom-modes'] + lh + ['font-probe'] if NO_MP else ['multiplayer', 'custom-modes'] + lh + ['font-probe']))
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
        places = os.path.join(lrig.idir(X), 'data', 'Mods', 'ui_places.tsv')        # the kit's kept panel places: a fresh start
        if os.path.exists(places):
            os.replace(places, os.path.join(OUT, 'old_ui_places.tsv'))
        HP.room_for(1)
        HP.start(X, 'http://127.0.0.1:9', {'DSL_UNITS_TEST': DRV, 'DSL_CRASHREP_TEST': crash, 'DSMP_UNITS_LIB': LIB})
        until(lambda: req('state', timeout=10).get('title_column'), 'the title', 1500, 3); time.sleep(6)
        run_heal() if HEAL else run()
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
        dl = os.path.join(OUT, 'dsload.log')                     # the kit's UI-RULE guard (KIT NOTE 2: templates + guards)
        rules = [l.strip()[:200] for l in open(dl, encoding='utf-8', errors='replace') if 'UI-RULE' in l] if os.path.exists(dl) else ['no dsload.log']
        check('no UI-RULE line from the kit\'s guard (UI Scale, move, red X, card, font rules)', not rules, rules[:4])
        res.update({'checks': HP.checks, 'info': info, 'all_pass': all(k['ok'] for k in HP.checks) and bool(HP.checks)})
        json.dump(res, open(os.path.join(OUT, 'results.json'), 'w'), indent=1, default=str)
        log('RESULT', 'PASS' if res['all_pass'] else 'FAIL', f"{sum(k['ok'] for k in HP.checks)}/{len(HP.checks)}", os.path.join(OUT, 'results.json'))
    sys.exit(0 if res.get('all_pass') else 1)


if __name__ == '__main__':
    main()
