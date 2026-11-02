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

X = os.environ.get('DPS_COPY', 'DPS'); HP.H = HP.G = X      # the test copy's name (a lane may take its own)
NO_MP = '--no-mp' in sys.argv
HEAL = '--heal' in sys.argv                                   # a Custom fight: a healer + barrier squad that monsters attack
SOLO = '--solo' in sys.argv or bool(os.environ.get('DPS_PATH'))
DEMO = os.environ.get('DPS_DEMO')                              # lead task 19 B: visible owner demo, left open
PATH1 = os.environ.get('DPS_PATH')                            # lead task 8 (item 13 not reproduced): world | custom                                   # owner 2026-11-01 (item 13): a 1-unit DPS Check shows the meter, one row
LIVE_HP = '--no-live-hp' not in sys.argv
START = os.environ.get('DPS_START')                            # item 28: Advanced > "Start Next to the Scarecrow": near (on) | own (off)
MANY = os.environ.get('DPS_MANY')                              # item 25: a Custom fight, 4 units against 3 x 30 monsters (> 64 entities)
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
    if os.environ.get('DPS_NOTORCH'):                             # task 12 proof: the same units with no torch
        for u in units:
            u['build'].get('eq', {}).pop('Secondary', None)
    if os.environ.get('DPS_AFF'):                                 # task 16: one affecter on every unit (as a world unit's brought effects)
        for u in units:
            u['build']['_cm'] = {'aff': [{'Key': os.environ['DPS_AFF'], 'Stack': 1, 'Duration': 100000, 'InitialDuration': 100000}]}
    if os.environ.get('DPS_SECONDARY'):                           # task 16: this item in the Secondary slot
        for u in units:
            u['build'].setdefault('eq', {})['Secondary'] = os.environ['DPS_SECONDARY']
    if SOLO:                                                      # DPS_SOLO_UNIT: which one (1 = a staff user: a ranged unit alone)
        units = [units[int(os.environ.get('DPS_SOLO_UNIT', '0'))]]
    lib = {'format': 'ds-launcher-units', 'version': 1, 'game_build': '25846389', 'units': units,
           'teams': [{'id': 'dd' * 8, 'name': 'Solo' if SOLO else 'DPS squad', 'unit_ids': [u['id'] for u in units]}]}
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


def set_start_near(want):
    """item 28 (owner 2026-11-02, off by default): Test Fights > Advanced > "Start Next to the Scarecrow", by the player's click"""
    cmf(cmd='scroll', name='fights:sec.advanced', pos=0); time.sleep(0.8)
    if req('rect', target='fights:r.startnear').get('rc', -1) <= 0:
        click('fights:sec.advanced', 1.5)
    cmf(cmd='scroll', name='fights:r.startnear', pos=0); time.sleep(0.8)
    if bool(tfs().get('startnear')) != want:
        click('fights:r.startnear', 1.0)
    if bool(tfs().get('startnear')) != want:
        log('start option: screen click missed; the widget click', cmf(cmd='click', name='fights:r.startnear')); time.sleep(1.0)
    shot('advanced_startnear_' + ('on' if want else 'off'))
    check(f'(item 28) Advanced > "Start Next to the Scarecrow" {"on" if want else "off"}', bool(tfs().get('startnear')) == want, tfs().get('startnear'))


def start_fight(prep=None, world=None):
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
    n = 1 if SOLO else 4
    check(f'the {n}-unit team is the squad, mode DPS Check', tf.get('squad') == n and tf.get('mode') == 'dps', json.dumps(tf))
    sw_left = [n for n in ('speech', 'healnum', 'chatlog', 'effects') if req('rect', target=f'fights:r.dps_{n}').get('rc', -1) > 0]
    check('round 2: no DPS switches on the Test Fights start screen (they are on the in-game meter)', not sw_left, sw_left)
    check('(item 28) "Start Next to the Scarecrow" is off by default (owner 2026-11-02)', tfs().get('startnear') == 0, tfs().get('startnear'))
    if START:
        set_start_near(START == 'near')
    if os.environ.get('DPS_SMLEGACY'):                            # item 28 (a): the 0.1.48 start order (MoveOrder.Near), to show it
        log('sm_legacy', dps('sm_legacy 1'))
    if prep:
        prep()
    click('fights:start', 1)
    if world:
        world(); return
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


auto_on = [0]                                                     # squad units whose Auto Skill toggle was clicked on


def order_attack(at):
    """each squad unit: a left click on it (selected: the panel leaves the building), a click on its Auto Skill toggle in the
    bottom panel (test units start with Auto Skill OFF, owner 2026-11-01; the skill check below needs it on), then a right
    click on the scarecrow (the player's attack order); then the scarecrow is selected again and the mouse stays on it"""
    us = [b for b in dps('pos').get('b', []) if b.get('u') == 1 and b.get('on')]
    log('units', [(u['go'], u['s']) for u in us])
    n = 0
    for u in us:
        for dx, dy in ((0, 0), (0, 12), (0, 24), (0, 36)):
            req('click_xy', x=u['s'][0] + dx, y=u['s'][1] + dy); time.sleep(1.0)
            if not dps('state').get('bpanel_on'):
                tg = None if os.environ.get('DPS_NOAUTO') else next((o for o in json.loads(req('call', fn='tf_dps_test', s='options').get('out') or '[]') if o.get('opt') == 1 and o.get('on')), None)
                if tg:
                    r = tg['r']; req('click_xy', x=(r[0] + r[2]) / 2, y=(r[1] + r[3]) / 2); time.sleep(0.6); auto_on[0] += 1
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


def rows_check(tag):
    """owner 2026-10-26 (item 5): the DPS window "should show all of the units ... that are in the fight" (it showed 2 of 4):
    a row for every unit that hit, every row inside the shown list (no row only reachable by scrolling)"""
    try:
        m = until(lambda: (lambda x: x if len(x.get('units') or []) >= 4 else None)(meter()), 'every squad unit hit the scarecrow', 60, 1)
    except RuntimeError:
        m = meter()
    us = m.get('units') or []
    check(f'(item 5, {tag}) a row for each of the {len(us)} units that hit, every row shown without scrolling (rows {m.get("rows_h")} <= list {m.get("view_h")})',
          len(us) >= 4 and m.get('rows_n') == len(us) and m.get('rows_h', 1e9) <= m.get('view_h', -1) + 0.5,
          json.dumps({k: m.get(k) for k in ('rows_n', 'rows_h', 'view_h', 'rect')} | {'units': [u.get('name') for u in us], 'place': m.get('place')}, ensure_ascii=False)[:600])
    shot(f'meter_rows_{LANG or "en"}')


def unit_spots():
    p = dps('pos').get('b', [])
    return sorted((round(x['w'][0], 2), round(x['w'][1], 2)) for x in p if x.get('u') == 1 and x.get('on')), \
        (sum(1 for x in p if x.get('u') == 0), sum(1 for x in p if x.get('u') == 1))


def unit_spots_settled():
    """unit_spots once the squad stands still (task 20: each unit walks next to the scarecrow at the start)"""
    a = unit_spots()
    for _ in range(25):
        time.sleep(1); b = unit_spots()
        if b == a:
            return b
        a = b
    return a


def chat_lines(frm=0):
    """the chat's local lines ever added, those in its history now, and the history's local lines from frm on"""
    chat = dlog(f'chat {frm}').split('\n')
    return int(chat[0] or 0), int(chat[1] or 0), [x for x in chat[2:] if x != ''] if len(chat) > 2 else []


def size_click(btns, plus):
    """a left click on the kit's "-" (left half) or "+" (right half) of a panel_size holder (screen px, y up: x0, y0, x1, y1)"""
    w = (btns[2] - btns[0]) * 22 / 46.0
    req('click_xy', x=btns[2] - w / 2 if plus else btns[0] + w / 2, y=(btns[1] + btns[3]) / 2); time.sleep(1.5)


def size_checks():
    """SIZE RULE (owner 2026-10-25: "allow to both make the chat and the DPS check window smaller or bigger with a plus or
    minus"): the kit's "-" / "+" (dsl_panel_size) on the DPS meter and on the chat: 10 % a click, the panel and its text,
    kept for the next start (ui_places.tsv); each is left at 100 % for the checks after this"""
    wd = lambda r: (r or [0, 0, 0, 0])[2] - (r or [0, 0, 0, 0])[0]
    def hov(r):                                                   # kit 0.1.30: the "-" / "+" show only while the mouse is over the panel
        if r and r[2] > r[0]:
            req('hover_xy', x=(r[0] + r[2]) / 2, y=r[3] - 8); time.sleep(0.6)
    def mh():
        hov(meter().get('rect')); return meter()
    m0 = mh(); zb = m0.get('size_btns') or [0]
    check('size: the DPS meter has "-" / "+" (kit panel_size)', zb[0] == 1 and zb[3] - zb[1] > 4 and abs(m0.get('size', 0) - 1) < 1e-3, (m0.get('size'), zb))
    if zb[0] == 1:
        size_click(zb[1:], True); m1 = mh()
        size_click((m1.get('size_btns') or [0, 0, 0, 0, 0])[1:], False); size_click((mh().get('size_btns') or [0, 0, 0, 0, 0])[1:], False); m2 = mh()
        places = os.path.join(lrig.idir(X), 'data', 'Mods', 'ui_places.tsv')
        kept = os.path.exists(places) and 'custom-modes.dps_meter@size\t0.9' in open(places, encoding='utf-8', errors='replace').read()
        shot(f'meter_size90_{LANG or "en"}')
        check('size: the DPS meter: "+" = 110 %, "-" "-" = 90 % (its rect with it), the size kept in ui_places.tsv',
              abs(m1.get('size', 0) - 1.1) < 1e-3 and abs(wd(m1.get('rect')) / wd(m0.get('rect')) - 1.1) < 0.03
              and abs(m2.get('size', 0) - 0.9) < 1e-3 and abs(wd(m2.get('rect')) / wd(m0.get('rect')) - 0.9) < 0.03 and kept,
              json.dumps({'w': [wd(m0.get('rect')), wd(m1.get('rect')), wd(m2.get('rect'))], 'size': [m1.get('size'), m2.get('size')], 'kept': kept}))
        size_click((mh().get('size_btns') or [0, 0, 0, 0, 0])[1:], True)
    if NO_MP:
        return
    chat = lambda: (HP.c(X, cmd='mpm_status') or {}).get('chat') or {}
    req('key', code=36); time.sleep(1.5); hov(chat().get('rect')); c0 = chat(); zb = c0.get('zoom_btns')     # Enter: the box (read only: no session)
    check('size: the chat\'s open box has "-" / "+" (kit panel_size)', zb and zb[3] - zb[1] > 4 and abs(c0.get('zoom', 0) - 1) < 1e-3, (c0.get('zoom'), zb))
    if zb:
        size_click(zb, True); hov(chat().get('rect')); c1 = chat(); shot(f'chat_size110_{LANG or "en"}'); size_click(c1.get('zoom_btns') or zb, False); hov(chat().get('rect')); c2 = chat()
        check('size: the chat: "+" = 110 % (the open box with it), "-" = 100 % again',
              abs(c1.get('zoom', 0) - 1.1) < 1e-3 and abs(wd(c1.get('rect')) / wd(c0.get('rect')) - 1.1) < 0.03
              and abs(c2.get('zoom', 0) - 1) < 1e-3 and abs(wd(c2.get('rect')) - wd(c0.get('rect'))) <= 2,
              json.dumps({'w': [wd(c0.get('rect')), wd(c1.get('rect')), wd(c2.get('rect'))], 'zoom': [c1.get('zoom'), c2.get('zoom')]}))
    req('key', code=53); time.sleep(0.5)


BASIC = {None: 'Basic Attack', '3': 'Grundangriff', '4': 'Базовая атака', '7': '基本攻撃'}


def run():
    start_fight()
    shot(f'world_{LANG or "en"}')
    spots0, counts0 = unit_spots_settled()
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
    check('Auto Skill turned on for each ordered unit (its toggle in the bottom panel, as a player)', auto_on[0] == n, (auto_on[0], n))
    until(lambda: dps('state').get('hits', 0) > 5, 'the squad hits the scarecrow', 120, 1)
    t_first = time.time(); n30 = [None]                          # events logged in the first 30 s of hits (Auto Skill check below)
    s = dps('state'); check('the scarecrow selected again, the mouse kept on it', s.get('same') and s.get('bpanel_on'), json.dumps(s)[:300])
    check('the follow is ' + ('live-hp' if LIVE_HP else "Units Laboratory's own (no live-hp)"), s.get('live_hp') == (1 if LIVE_HP else 0), s.get('live_hp'))
    if PLACE:
        time.sleep(4); placement(PLACE); return
    placement(f'{LANG or "en"}_1.0_960')
    rows_check('4-unit fight')
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
    # "DPS Log in Chat" (owner 2026-10-25: "the damage toggle wasn't working as expected": off left every line in the chat, on
    # lost the lines of the off time): off takes our lines out of the chat at once and none come while it is off (the meter
    # counts on); on gives the whole log back at once, the off time too, and the open box shows its newest lines
    def shown():                                                  # the chat's rows on screen now (idle lines / open box): their text
        return [r.get('text') or '' for r in ((HP.c(X, cmd='mpm_status') or {}).get('chat') or {}).get('rows') or []]
    if not NO_MP:                                                 # --no-mp: no chat (our own panel follows the switch: fb_tick)
        n0 = chat_lines()[1]
        click('dps:chatlog', 0.0); time.sleep(1.0); in_off, vis_off = chat_lines()[1], shown(); ma = meter().get('total', 0)
        time.sleep(10); in_off2 = chat_lines()[1]; mb = meter().get('total', 0)
        check('"DPS Log in Chat" off: our lines leave the chat within 1 s (none on screen), none come in 10 s of hits, the meter still counts',
              n0 > 0 and in_off == 0 and not vis_off and in_off2 == 0 and mb > ma and not dps('state')['sw'][2], (n0, in_off, vis_off[:2], in_off2, ma, mb))
        l_on = dps('state').get('lines', 0); click('dps:chatlog', 0.0); time.sleep(1.0)
        req('key', code=36); time.sleep(1.0); _, in_on, cl = chat_lines(); vis_on = shown()   # Enter: the box with the history
        check('"DPS Log in Chat" on: the whole log is back in the chat within 1 s (the off time too), the open box shows its newest lines',
              dps('state')['sw'][2] == 1 and l_on > n0 and in_on >= min(l_on, 2500) and len(vis_on) > 3 and vis_on[0] in cl[-3:],
              json.dumps({'before_off': n0, 'log_at_on': l_on, 'chat_after_on': in_on, 'shown': vis_on[:2], 'chat_newest': cl[-2:]}, ensure_ascii=False)[:600])
        shot(f'chatlog_on_{LANG or "en"}'); req('key', code=53); time.sleep(0.5)
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
    size_checks()
    # Restart test, twice: a fresh world (scarecrow full, the squad at its start spots, the same objects), the meter at 0
    for r_i in (1, 2):
        click('dps:restart', 0.5)
        until(lambda: (lambda s: s.get('on') == 1 and s.get('title') == 0 and s.get('hits', 9) < 3)(dps('state')), f'the restarted world ({r_i})', 300, 2)
        time.sleep(8)
        m = meter(); total, inhist, cl = chat_lines()            # before the squad's walk to the scarecrow ends (task 20)
        spots, counts = unit_spots_settled()
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


heal_auto = [0]


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
    # task 23: summons need Auto Skill, OFF at the start since 0.1.48 (owner). After the basic heals, each squad unit gets
    # Auto Skill on with its toggle in the unit panel (as a player); then the healer's Strange Seed and its summon's heals
    if not os.environ.get('DPS_NOAUTO'):
        for u in [b for b in dps('pos').get('b', []) if b.get('u') == 1 and b.get('on') and 'PlayerUnit' in b.get('go', '')]:
            req('click_xy', x=u['s'][0], y=u['s'][1]); time.sleep(0.8)
            tg = next((o for o in json.loads(req('call', fn='tf_dps_test', s='options').get('out') or '[]') if o.get('opt') == 1 and o.get('on')), None)
            if tg:
                r = tg['r']; req('click_xy', x=(r[0] + r[2]) / 2, y=(r[1] + r[3]) / 2); time.sleep(0.6); heal_auto[0] += 1
        log('heal: Auto Skill turned on for', heal_auto[0])
        t1 = time.time()
        while time.time() - t1 < 90 and not any(h.get('kind') == 1 and h.get('skill', '').endswith('(summon)') for h in hits_dump(0)):
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
    # a Custom fight (task 24): damage dealt = the squad's damage to non-squad units (a DPS Check: to the scarecrow)
    dsum = sum(h['dmg'] for h in full if h['kind'] == 0 and inspan(h['i']) and un.get(h['unit'], {}).get('player') and not un.get(h['target'], {}).get('player'))
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


def status_checks(tag):
    """owner 2026-10-26 (item 7): "Waiting for the first hit" "is kind of displaced and looks kind of odd" since the "-" / "+".
    With an empty list (before the first hit) the status shows in the list: its box centred in the list (within 3 px),
    its text on one line and not cut, never over the "-" / "+"; at 60 %, 100 % and 150 % size, with the mouse over the
    panel (the "-" / "+" shown) and away from it (hover-only "-" / "+", kit 0.1.30)"""
    def one(lbl):
        time.sleep(1.0); m = meter(); st, lr, zb = m.get('status') or {}, m.get('list_rect') or [0, 0, 0, 0], m.get('size_btns') or [0, 0, 0, 0, 0]
        sr = st.get('rect') or [0, 0, 0, 0]
        cx, cy = (sr[0] + sr[2]) / 2 - (lr[0] + lr[2]) / 2, (sr[1] + sr[3]) / 2 - (lr[1] + lr[3]) / 2
        ok = (m.get('rows_n') == 0 and st.get('where') == 1 and st.get('text') and abs(cx) <= 3 and abs(cy) <= 3
              and sr[0] >= lr[0] - 1 and sr[2] <= lr[2] + 1 and not st.get('cut') and 0 < st.get('pw', 0) <= st.get('bw', 0) + 0.5
              and not (zb[0] and overlap(sr, zb[1:])))
        seen_btns.add(bool(zb[0]))
        return check(f'(item 7, {tag}) {lbl}: the empty list shows "{st.get("text")}" centred in it, one line, not cut, never over the "-" / "+"', ok,
                     json.dumps({'status': st, 'list': lr, 'btns': zb, 'size': m.get('size'), 'rows_n': m.get('rows_n'), 'd': [round(cx, 1), round(cy, 1)]}, ensure_ascii=False))
    seen_btns = set()
    m = meter(); r = m.get('rect') or [0, 0, 0, 0]
    if m.get('rows_n') != 0:
        check(f'(item 7, {tag}) the meter is empty before the first hit', False, m.get('rows_n')); return
    over = lambda: req('hover_xy', x=(r[0] + r[2]) / 2, y=r[3] - 8)          # the title bar (screen y up)
    away = lambda: req('hover_xy', x=r[2] + 300, y=(r[1] + r[3]) / 2)
    over(); one('100 %, mouse over the panel'); away(); one('100 %, mouse away'); shot(f'empty_{tag}_100')
    for want, plus, n in ((0.6, False, 4), (1.5, True, 9), (1.0, False, 5)):
        for _ in range(n):
            over(); time.sleep(0.6); zb = meter().get('size_btns') or [0, 0, 0, 0, 0]
            size_click(zb[1:], plus)
        m = meter(); r = m.get('rect') or r
        if abs(m.get('size', 0) - want) > 1e-3:
            check(f'(item 7, {tag}) size {want:.0%} reached', False, m.get('size')); return
        over(); one(f'{want:.0%}, mouse over the panel'); away(); one(f'{want:.0%}, mouse away'); shot(f'empty_{tag}_{int(want * 100)}')
    log('item 7: "-" / "+" states seen (True = shown)', sorted(seen_btns))


def section_checks(tag):
    """PLAN_v013 item 6 (owner: sections fold, "otherwise they just take a lot of space for no reason"), lead task 10: the
    meter's "Stats" and "Settings" blocks fold with the kit's dsl_section (a click on the header); the state is kept when
    the panel folds / opens and over Restart Test Floor (a new world); the unit rows still show"""
    def hdr(name):
        r = req('rect', target=name); a = r.get('at') or [0, 0, 0, 0]
        if r.get('rc', -1) <= 0:
            return False
        req('click_xy', x=(a[0] + a[2]) / 2, y=(a[1] + a[3]) / 2); time.sleep(1.5); return True
    m0 = meter()
    if not (m0.get('sec') or [0])[0]:
        check(f'(item 6, {tag}) the meter has the kit\'s sections (kit 0.1.30)', False, m0.get('sec')); return
    ok1 = hdr('dps:sec_settings') and hdr('dps:sec_stats'); m1 = meter()
    sw_vis = req('rect', target='dps:speech').get('rc', -1) > 0; tv = req('rect', target='dps:time').get('rc', -1) > 0
    rs = req('rect', target='dps:restart').get('rc', -1) > 0
    shot(f'sections_folded_{tag}')
    check(f'(item 6, {tag}) a click on "Stats" / "Settings" folds them: their lines hidden, Restart Test Floor and the rows still show, the panel shorter',
          ok1 and m1.get('sec') == [1, 0, 0] and not sw_vis and not tv and rs and m1.get('rows_n') == m0.get('rows_n')
          and (m1.get('rect') or [0, 0, 0, 0])[3] - (m1.get('rect') or [0, 0, 0, 0])[1] < (m0.get('rect') or [0, 0, 0, 0])[3] - (m0.get('rect') or [0, 0, 0, 0])[1],
          json.dumps({'sec': [m0.get('sec'), m1.get('sec')], 'switch_shown': sw_vis, 'time_shown': tv, 'restart': rs, 'rows': [m0.get('rows_n'), m1.get('rows_n')], 'rect': [m0.get('rect'), m1.get('rect')]}))
    def sw(m):                                                    # the panel's scale and width
        r = m.get('rect') or [0, 0, 0, 0]; return ((m.get('place') or {}).get('s') or 0, r[2] - r[0])
    def same(a, b):
        return a[0] > 0 and b[0] > 0 and abs(a[0] - b[0]) <= 0.01 * a[0] and abs(a[1] - b[1]) <= max(1, 0.01 * a[1])
    click('dps:fold', 1.5); mf = meter(); click('dps:fold', 1.5); m2 = meter()
    check(f'(task 21, {tag}) folding Stats / Settings or the whole panel never changes the meter\'s scale or width (owner: "It shouldn\'t")',
          same(sw(m0), sw(m1)) and same(sw(m0), sw(mf)), json.dumps({'open': sw(m0), 'sections_folded': sw(m1), 'panel_folded': sw(mf)}))
    check(f'(item 6, {tag}) still folded after the panel is folded and opened again', m2.get('sec') == [1, 0, 0], m2.get('sec'))
    click('dps:restart', 2)
    until(lambda: (lambda x: x.get('on') == 1 and x.get('title') == 0 and x.get('hits', 1) == 0)(dps('state')), 'the restarted world', 600, 3); time.sleep(8)
    m3 = meter()
    check(f'(item 6, {tag}) still folded in the new test (Restart Test Floor)', m3.get('shown') == 1 and m3.get('sec') == [1, 0, 0], json.dumps({k: m3.get(k) for k in ('shown', 'sec')}))
    hdr('dps:sec_settings'); hdr('dps:sec_stats'); m4 = meter()
    check(f'(item 6, {tag}) a second click opens them again (switches shown)', m4.get('sec') == [1, 1, 1] and req('rect', target='dps:speech').get('rc', -1) > 0, m4.get('sec'))


def run_solo():
    """owner 2026-11-01 (item 13): "the DPS meter does not show when the fight has ONE unit". A 1-unit DPS Check: the meter
    shows, with exactly one row (the unit), visible in the list; then the empty-list status checks (item 7)"""
    start_fight()
    m0 = meter(); s0 = dps('state')
    log('solo: before hits', json.dumps({'meter': {k: m0.get(k) for k in ('shown', 'small', 'folded', 'rect', 'rows_n', 'view_h')}, 'dps': {k: s0.get(k) for k in ('on', 'title', 'hits')}}))
    shot('solo_world')
    status_checks('solo')                                         # the list is empty until the unit is ordered to attack
    at = select_dummy()
    check('(solo) the scarecrow selected by a click', at is not None)
    if not at:
        return
    n = order_attack(at)
    check('(solo) the unit ordered to attack the scarecrow', n == 1, n)
    until(lambda: dps('state').get('hits', 0) > 5, 'the unit hits the scarecrow', 120, 1)
    time.sleep(3); m = meter(); shot('solo_meter')
    us = m.get('units') or []
    log('solo: meter', json.dumps({k: m.get(k) for k in ('shown', 'small', 'rect', 'rows_n', 'rows_h', 'view_h', 'total')}), [u.get('name') for u in us])
    check('(item 13) a 1-unit DPS Check shows the meter (not the small Restart panel), with one row for the unit, inside the visible list',
          m.get('shown') == 1 and len(us) == 1 and m.get('rows_n') == 1 and m.get('rows_h', 1e9) <= m.get('view_h', -1) + 0.5,
          json.dumps({k: m.get(k) for k in ('shown', 'small', 'rows_n', 'rows_h', 'view_h')} | {'units': [u.get('name') for u in us]}, ensure_ascii=False))
    section_checks('solo')


def solo_meter(tag, want_full=True):
    """the meter after the one unit hit (or the small panel): logged and checked"""
    m = meter(); us = m.get('units') or []
    log(f'path {tag}: meter', json.dumps({k: m.get(k) for k in ('shown', 'small', 'folded', 'rect', 'rows_n', 'rows_h', 'view_h', 'total')}), [u.get('name') for u in us])
    shot(f'path_{tag}')
    if want_full:
        check(f'(task 8, {tag}) the meter shows with one row for the one unit', m.get('shown') == 1 and len(us) == 1 and m.get('rows_n') == 1
              and m.get('rows_h', 1e9) <= m.get('view_h', -1) + 0.5, json.dumps({k: m.get(k) for k in ('shown', 'small', 'folded', 'rows_n', 'rows_h', 'view_h')}))
    return m


def premove(at):
    """task 20 (DPS_PREMOVE=dx,dy screen points from the scarecrow): the player's move order next to it first"""
    if not os.environ.get('DPS_PREMOVE'):
        return at
    dx, dy = (float(v) for v in os.environ['DPS_PREMOVE'].split(','))
    log('premove options before', req('call', fn='tf_dps_test', s='options').get('out'))
    for u in [b for b in dps('pos').get('b', []) if b.get('u') == 1 and b.get('on')]:
        req('click_xy', x=u['s'][0], y=u['s'][1]); time.sleep(1.0)
        req('click_xy', x=at[0] + dx, y=at[1] + dy, button=1); time.sleep(8)
    log('premove options after', req('call', fn='tf_dps_test', s='options').get('out'))
    log('premove', [(b.get('go'), b.get('w'), b.get('s')) for b in dps('pos').get('b', []) if b.get('u') == 1])
    return select_dummy() or at


def attack_and_wait(tag):
    at = select_dummy()
    if not at:
        check(f'(task 8, {tag}) the scarecrow selected', False); return False
    at = premove(at)
    order_attack(at)
    if DEMO:                                                      # the owner watches 60 s; the hit count goes to the log
        shot('demo_order'); time.sleep(60); log('DEMO hits after 60 s', dps('state').get('hits', 0)); shot('demo_60s'); return True
    try:
        until(lambda: dps('state').get('hits', 0) > 5, 'the unit hits', 120, 1)
    except RuntimeError:
        h = dps('state').get('hits', 0); log(f'path {tag}: only {h} hits in 120 s')
        if not h:
            check(f'(task 8, {tag}) the unit hits the scarecrow', False, dps('state')); return False
    time.sleep(3); return True


def run_paths():
    """lead task 8: other ways a 1-unit test may start (owner: "Test fight : doesn't display dps meter if one character")"""
    rows = column()
    lab = next((r[0] for r in rows if r and r[0] == 'Units Laboratory' and len(r) > 1 and r[1] == '1'), None) or by_index(rows, COL_LAB) or 'Units Laboratory'
    click('title:' + lab, 3); click('home:Test Fights', 3)
    until(lambda: tfs().get('engine') == 2, "the wiki's Test Fights files", 300, 2)
    if PATH1 == 'world':                                            # a world unit (from a save) as the only unit, DPS Check
        click('fights:l.src2', 2); t = tfs(); log('worlds', t.get('worlds'))
        click('fights:l.world0', 3); t = tfs(); us = [u for u in (t.get('wunits') or '').split(',') if u]
        if not us:
            check('(task 8, world) the save copy\'s world lists units', False, json.dumps(t)[:400]); return
        click(f"fights:l.wtick{os.environ.get('DPS_WUNIT', '0')}", 2); t = tfs()
        check('(task 8, world) one world unit is the squad, mode DPS Check', t.get('squad') == 1 and t.get('mode') == 'dps', json.dumps(t)[:400])
        click('fights:start', 1)
        until(lambda: (lambda x: x.get('on') == 1 and x.get('title') == 0)(dps('state')), 'the DPS Check world', 600, 3); time.sleep(8)
        log('path world: stem', dps('state').get('stem'), meter().get('shown'))
        if not attack_and_wait('world'):
            return
        if os.environ.get('DPS_HOLD_WORLD'):
            hold_check('world unit ' + os.environ.get('DPS_WUNIT', '0')); return
        if os.environ.get('DPS_DIAG'):                              # task 11: what the world unit does after its first hit
            log('diag hits', json.dumps(hits_dump(0), ensure_ascii=False)[:1500])
            for k in range(6):
                p = [x for x in dps('pos').get('b', []) if x.get('u') == 1]
                log('diag pos', k, json.dumps(p)[:400], 'hits', dps('state').get('hits')); shot(f'diag_{k}'); time.sleep(5)
            log('diag units', json.dumps(units_dump(), ensure_ascii=False)[:600])
            at2 = select_dummy(); h0 = dps('state').get('hits'); order_attack(at2) if at2 else None; time.sleep(15)
            log('diag second order: hits', h0, '->', dps('state').get('hits'), 'pos', json.dumps([x.get('w') for x in dps('pos').get('b', []) if x.get('u') == 1]))
            log('diag tick hits', json.dumps([(h['tick'], h['key']) for h in hits_dump(0)]))
        solo_meter('world')
        # the meter folded while the fight runs, opened again: the row still there
        click('dps:fold', 1.5); click('dps:fold', 1.5); solo_meter('world_refold')
        # a second test right after the first (Restart Test Floor), the meter at 0, the row back after the hits
        click('dps:restart', 2)
        until(lambda: (lambda x: x.get('on') == 1 and x.get('title') == 0 and x.get('hits', 1) == 0)(dps('state')), 'the restarted world', 600, 3); time.sleep(8)
        m = meter(); check('(task 8, restart) the meter shows after Restart Test Floor (empty, waiting)', m.get('shown') == 1 and m.get('rows_n') == 0, json.dumps({k: m.get(k) for k in ('shown', 'small', 'rows_n')}))
        if attack_and_wait('restart'):
            solo_meter('restart')
    else:                                                           # a Custom fight with one unit of his side (monsters)
        click('fights:l.team0', 2)
        cmf(cmd='set', name='fights:r.mode', index=2); time.sleep(1.5)
        cmf(cmd='set', name='fights:r.moncount', index=0); time.sleep(0.5)
        cmf(cmd='type', name='fights:r.monq', text='Rat'); time.sleep(1.2); cmf(cmd='click', name='fights:r.hit0'); time.sleep(1.0)
        t = tfs(); check('(task 8, custom) one unit, Custom fight', t.get('squad') == 1 and t.get('mode') == 'arena', json.dumps(t)[:300])
        want = os.environ.get('DPS_ARENA_METER') == '1'               # task 24: Advanced > "DPS Meter" (off by default)
        cmf(cmd='scroll', name='fights:sec.advanced', pos=0); time.sleep(0.8)
        if req('rect', target='fights:r.meter').get('rc', -1) <= 0:
            click('fights:sec.advanced', 1.5)
        if bool(tfs().get('meter')) != want:
            cmf(cmd='scroll', name='fights:r.meter', pos=0); time.sleep(0.8); click('fights:r.meter', 1.0)
        if bool(tfs().get('meter')) != want:
            log('task 24: screen click missed; the widget click', cmf(cmd='click', name='fights:r.meter')); time.sleep(1.0)
        check(f'(task 24) Advanced > DPS Meter set {"on" if want else "off"}', bool(tfs().get('meter')) == want, tfs().get('meter'))
        click('fights:start', 1)
        until(lambda: (lambda x: x.get('shown', 0) + x.get('small', 0) > 0)(meter()), 'the Custom fight world', 600, 3)
        time.sleep(15)
        m = solo_meter('custom', want_full=False)
        info['custom_fight_meter'] = {'shown': m.get('shown'), 'small': m.get('small')}
        if want:                                                  # the unit's attack order on the nearest monster (a player's right click)
            p = dps('pos').get('b', []); sq = [b for b in p if b.get('u') == 1 and b.get('on') and 'PlayerUnit' in b.get('go', '')]
            foes = [b for b in p if b.get('u') == 1 and b.get('on') and 'PlayerUnit' not in b.get('go', '')]
            for u in sq[:1]:
                if foes:
                    f = min(foes, key=lambda f: (f['w'][0] - u['w'][0]) ** 2 + (f['w'][1] - u['w'][1]) ** 2)
                    req('click_xy', x=u['s'][0], y=u['s'][1]); time.sleep(0.8); req('click_xy', x=f['s'][0], y=f['s'][1] + 12, button=1); time.sleep(0.8)
            log('task 24: squad', len(sq), 'foes', len(foes), 'ev', dps('state').get('ev'))
            t0 = time.time()
            while time.time() - t0 < 60 and not (meter().get('units') or []):
                time.sleep(3)
            m = meter(); us = m.get('units') or []; log('task 24: ev', dps('state').get('ev'))
            check('(task 24) DPS Meter on: a Custom fight shows the meter, with a row for the unit\'s damage to the monsters',
                  m.get('shown') == 1 and len(us) == 1 and (m.get('total') or 0) > 0, json.dumps({k: m.get(k) for k in ('shown', 'small', 'rows_n', 'total')}))
        else:
            check('(task 24) DPS Meter off (the default): a Custom fight shows the small "Restart Test Floor" panel, not the DPS meter',
                  m.get('small') == 1 and m.get('shown') == 0, json.dumps({k: m.get(k) for k in ('shown', 'small')}))


def hold_check(tag, secs=60):
    """lead task 12 (owner: tests start with Auto Skill OFF, and the meter must show DPS): after the attack order every
    squad unit keeps hitting the scarecrow for 60 s; with Auto Skill off no active skill is cast"""
    n0 = len(hits_dump(0)); time.sleep(secs); ev = [h for h in hits_dump(0)[n0:] if h['kind'] == 0 and h['on_dummy']]
    un = units_dump(); sq = [i for i, u in un.items() if u['player']]
    per = {un[i]['name']: sum(1 for h in ev if h['unit'] == i) for i in sq}
    skills = sorted({h['key'] for h in ev if not h['basic'] and h['key'].startswith('SKILL_')})
    keys = {}
    for h in ev:
        keys[h['key']] = keys.get(h['key'], 0) + 1
    m = meter(); info.setdefault('hold', {})[tag] = {'per_unit': per, 'skills': skills, 'keys': keys, 'dps5': m.get('dps5'), 'rep': dps('state').get('rep')}
    log(f'hold {tag}', json.dumps(info['hold'][tag], ensure_ascii=False))
    log('meter raw', json.dumps(m, ensure_ascii=False)[:2500]); shot('hold_' + re.sub(r'\W+', '_', tag))
    noauto = bool(os.environ.get('DPS_NOAUTO'))
    check(f'(task 12, {tag}) every squad unit keeps hitting the scarecrow for {secs} s (at least 5 hits each), DPS (last 5 s) > 0'
          + (', no active skill cast (Auto Skill off)' if noauto else ''),
          per and min(per.values()) >= 5 and (m.get('dps5') or 0) > 0 and (not noauto or not skills), json.dumps(info['hold'][tag], ensure_ascii=False))


def scarecrow_xy():
    b = next((x for x in dps('pos').get('b', []) if 'scarecrow' in (x.get('pk', '') + x.get('go', '')).lower()), None)
    return b.get('s') if b else None


def cam_on_scarecrow():
    """the camera starts on the squad's spawn, the scarecrow ~36 tiles away (off screen in a 960 x 540 copy): pan it there for
    the shot (a drag with the middle button, then the top / bottom edge), each try measured by the scarecrow's screen point"""
    W, H = 960, 540
    tries = []
    for how in ('drag2', 'drag0', 'edge'):
        for _ in range(8):
            p = scarecrow_xy()
            if not p:
                return tries
            if 60 < p[0] < W - 60 and 60 < p[1] < H - 60:
                tries.append((how, 'on screen', p)); log('camera', tries); return tries
            dx, dy = W / 2 - p[0], H / 2 - p[1]
            k = min(1.0, 200 / max(1.0, (dx * dx + dy * dy) ** 0.5))
            if how.startswith('drag'):
                req('drag_xy', x=W / 2, y=H / 2, dx=dx * k, dy=dy * k, button=int(how[-1])); time.sleep(1.5)
            else:
                req('hover_xy', x=W / 2, y=H - 2 if dy < 0 else 2); time.sleep(2.0); req('hover_xy', x=W / 2, y=H / 2); time.sleep(0.5)
            q = scarecrow_xy(); tries.append((how, p, q))
            if q == p:
                break
    log('camera', tries)
    return tries


def start_check(tag, near, worlds0=0):
    """item 28: the DPS Check's start move, from the game's map tiles (InMapDataInterface.GetLogicalPosition): near = each
    squad unit on its own free tile next to the scarecrow; own = each unit on the tile the test put it on"""
    until(lambda: (lambda x: x.get('state') == 1 and x.get('done') == 1 and x.get('worlds', 0) > worlds0)(dps('sm')), f'the start move ({tag})', 180, 1)
    unit_spots_settled(); time.sleep(1)
    s = dps('sm'); info.setdefault('start', {})[tag] = s; log(f'start {tag}', json.dumps(s))
    p = dps('pos').get('b', [])                                   # screen points of the scarecrow and the squad (a crop of the shot)
    info['start'][tag]['screen'] = [(b.get('go', '')[:24], b.get('s')) for b in p if b.get('u') == 1 and b.get('on') or 'scarecrow' in (b.get('pk', '') + b.get('go', '')).lower()]
    info['start'][tag]['camera'] = cam_on_scarecrow()
    click('dps:fold', 1.0); shot('start_' + tag); click('dps:fold', 1.0)   # the meter folded: it covers the scarecrow's room
    us = s.get('u') or []
    if s.get('legacy'):                                           # the 0.1.48 order: shown and logged, not checked
        log(f'start {tag} (0.1.48 order): tiles from the scarecrow', [u.get('ring_now') for u in us]); return s
    if near:
        ok = bool(us) and all(u['ring_now'] == 1 and u['now'] == u['to'] for u in us) and len({tuple(u['now']) for u in us}) == len(us)
        what = 'every squad unit ends on its own free tile next to the scarecrow'
    else:
        ok = bool(us) and all(u['now'] == u['from'] for u in us)
        what = 'every squad unit stays on the tile the test put it on'
    check(f'(item 28, {tag}) {what} (game map tiles), sent as the game\'s move order to a tile', ok and s.get('how') == 1 and s.get('near') == int(near), json.dumps(s))
    return s


def run_start():
    """item 28 + review 24b: the start move at the start and again after Restart Test Floor (the new world's own objects)"""
    near = START == 'near'
    start_fight()
    s0 = start_check('start', near)
    click('dps:restart', 2)
    until(lambda: (lambda x: x.get('on') == 1 and x.get('title') == 0 and x.get('hits', 9) < 3)(dps('state')), 'the restarted world', 600, 3)
    s1 = start_check('restart', near, s0.get('worlds', 0))
    check('(24b) Restart Test Floor: one more start move, in the new world', s1.get('worlds', 0) == s0.get('worlds', 0) + 1 and s1.get('state') == 1,
          json.dumps({'worlds': [s0.get('worlds'), s1.get('worlds')], 'sent': [s0.get('sent'), s1.get('sent')]}))


def prep_many():
    cmf(cmd='set', name='fights:r.mode', index=2); time.sleep(1.5)
    cmf(cmd='set', name='fights:r.moncount', index=29); time.sleep(0.5)
    cmf(cmd='type', name='fights:r.monq', text='Weak'); time.sleep(1.2)   # weak monsters: the squad kills many, they all take part
    ctl = cmf(cmd='state').get('controls') or {}
    hits = sorted(n for n in ctl if n.startswith('fights:r.hit'))[:3]
    log('many: monsters', [(n, (ctl.get(n) or {}).get('t')) for n in hits])
    for n in hits:
        cmf(cmd='click', name=n); time.sleep(1.0)
    if len(hits) < 3:                                             # fewer than 3 kinds: the same kind again (each click adds 30, at most 30 a kind)
        cmf(cmd='type', name='fights:r.monq', text='Rat'); time.sleep(1.2)
        for i in range(3 - len(hits)):
            cmf(cmd='click', name=f'fights:r.hit{i}'); time.sleep(1.0)
    cmf(cmd='scroll', name='fights:sec.advanced', pos=0); time.sleep(0.8)
    if req('rect', target='fights:r.meter').get('rc', -1) <= 0:
        click('fights:sec.advanced', 1.5)
    if not tfs().get('meter'):
        cmf(cmd='click', name='fights:r.meter'); time.sleep(1.0)
    t = tfs(); log('many', json.dumps(t)[:600])
    check('(item 25) a Custom fight of the 4 units, DPS Meter on', t.get('mode') == 'arena' and t.get('squad') == 4 and t.get('meter') == 1, json.dumps(t)[:300])


def run_many():
    """item 25 (owner 2026-11-02: "no limit or very high limit"): more than 64 entities in one logged fight. The table held 64
    entries; every later entity shared entry 63 (its damage went to the wrong row)"""
    start_fight(prep_many, lambda: until(lambda: (lambda x: x.get('shown', 0) + x.get('small', 0) > 0)(meter()), 'the Custom fight world', 600, 3))
    time.sleep(8)
    p = dps('pos').get('b', []); sq = [b for b in p if b.get('u') == 1 and b.get('on') and 'PlayerUnit' in b.get('go', '')]
    foes = [b for b in p if b.get('u') == 1 and b.get('on') and 'PlayerUnit' not in b.get('go', '')]
    log('many: squad', len(sq), 'monsters', len(foes))
    for u in sq:                                                  # each unit: the player's attack order on the nearest monster
        if foes:
            f = min(foes, key=lambda f: (f['w'][0] - u['w'][0]) ** 2 + (f['w'][1] - u['w'][1]) ** 2)
            req('click_xy', x=u['s'][0], y=u['s'][1]); time.sleep(0.8); req('click_xy', x=f['s'][0], y=f['s'][1] + 12, button=1); time.sleep(0.8)
    gs = []
    t0 = time.time()
    while time.time() - t0 < 240:
        gs = [l.split(' ')[1] for l in dlog('guids').split('\n') if l.strip()]
        if len(gs) > 70:
            break
        time.sleep(5)
    un = units_dump(); hs = hits_dump(0); m = meter()
    names = [u['name'] for u in un.values()]; sq = [i for i, u in un.items() if u['player']]
    bad = [h for h in hs if not (0 <= h['unit'] < len(gs) and 0 <= h['target'] < len(gs))]
    rows = [u.get('name') for u in (m.get('units') or [])]
    info['many'] = {'entries': len(gs), 'distinct': len(set(gs)), 'squad': [un[i]['name'] for i in sq], 'events': len(hs), 'rows': rows, 'secs': round(time.time() - t0)}
    log('many', json.dumps(info['many'], ensure_ascii=False)); shot('many_meter')
    check('(item 25) more than 64 entities in the log, each with its own entry (no shared entry), each name its own',
          len(gs) > 64 and len(set(gs)) == len(gs) and len(set(names)) == len(names) == len(gs), json.dumps(info['many'], ensure_ascii=False)[:600])
    check('(item 25) every event points at entries in the table; the meter rows are squad units only (at most 4)',
          not bad and len(sq) <= 4 and set(rows) <= {un[i]['name'] for i in sq}, json.dumps({'bad': bad[:3], 'rows': rows, 'squad': [un[i]['name'] for i in sq]}, ensure_ascii=False))


def run_hold():
    start_fight()
    if START:                                                     # item 28: the start move first (the one-hit fix holds with the option off and on)
        start_check('start', START == 'near')
    at = select_dummy()
    check('the scarecrow selected by a click', at is not None)
    if not at:
        return
    at = premove(at)
    n = order_attack(at)
    check('the squad ordered to attack the scarecrow', n >= 1, n)
    time.sleep(10)
    hold_check(('solo' if SOLO else '4-unit') + (' auto off' if os.environ.get('DPS_NOAUTO') else ' auto on') + (f' start {START}' if START else ''))
    if START:
        log('after the hold: the units\' tiles', json.dumps(dps('sm')))


info = {}
def main():
    if not DEMO:
        g = threading.Thread(target=HP.guard, daemon=True); g.start()
    else:                                                          # owner demo (lead task 19 B): a VISIBLE player-like window
        lrig.require_background_only = lambda x: None              # the lead set LSBackgroundOnly / LSUIElement false on this copy
        popen0 = HP.subprocess.Popen
        def popen_demo(args, *a, **kw):
            if '-screen-width' not in args:                         # only the game's own start
                return popen0(args, *a, **kw)
            env = dict(kw.get('env') or {}); env.pop('DSL_SOFT_NOACTIVATE', None); env.pop('DSRIG_HIDEWIN', None)
            env['DSRIG_LIVE'] = '1'; kw['env'] = env                # the loader's "not a hidden test copy" mode
            return popen0(args, *a, **kw)
        HP.subprocess.Popen = popen_demo
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
        if os.environ.get('DPS_CMSET'):                           # task 19: a player's Units Laboratory switches (a copy of his file)
            os.makedirs(os.path.dirname(cmset), exist_ok=True); shutil.copy(os.environ['DPS_CMSET'], cmset)
        if os.environ.get('DPS_PLACES'):                          # task 19: a player's kept panel places (a copy of his file)
            shutil.copy(os.environ['DPS_PLACES'], places)
        HP.room_for(1)
        HP.start(X, 'http://127.0.0.1:9', {'DSL_UNITS_TEST': DRV, 'DSL_CRASHREP_TEST': crash, 'DSMP_UNITS_LIB': LIB})
        until(lambda: req('state', timeout=10).get('title_column'), 'the title', 1500, 3); time.sleep(6)
        run_heal() if HEAL else run_hold() if os.environ.get('DPS_HOLD') else run_many() if MANY else run_start() if START else run_paths() if PATH1 else run_solo() if SOLO else run()
    except BaseException as e:
        log(traceback.format_exc())
        check('run finished', False, repr(e))
    finally:
        if DEMO:                                                  # left open: the lead closes it (its pid is in game.pid)
            log('DEMO OPEN: pid', open(lrig.pidfile(X)).read().strip(), '(left running)')
            os._exit(0)
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
