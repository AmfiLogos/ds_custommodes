#!/usr/bin/env python3
"""Test Fights > World units (owner 2026-10-25: "Test fights should allow to select directly people from one of your current
saves") in one hidden loader test copy (background-only, small window, muted, notch fix off; never the owner's game, saves,
library or screen) with multiplayer + custom-modes + the test module font-probe (the units test driver). The copy's game
saves folder is a test folder holding a COPY of a real save (DSML_GAME_SAVES); its modded saves are its own; its My Units
library is a test file (DSMP_UNITS_LIB) with 4 fixture units and one team of 3.
Fails when (the breaks this catches):
  - My Units: a team row shows its units on the first open, or again after the window opened again; its box does not add
    all of its units while it is folded; its arrow does not open / close it;
  - the World units tab does not list the save copy's world, a world is open when the window opens, or opening it does not
    show that save's units as cards; ticking two units does not put exactly those two in the squad;
  - fight 1 (effects switch off, its default; Custom fight, one Weak Grey Rat, no battle kits): the test world does not
    load, the squad is not the two picked units (names) at their save levels, a unit's bag in the running game is not its
    bag in the save (slot, key, amount), an affecter of the save that the build does not write shows up, the fight does not
    start (HP changes) or end (the rat killed) within 4 minutes, or the test world's saves stay after the title;
  - fight 2 (window opened again, effects switch on, two units that carry items): a bag differs, or an affecter of the save
    is missing in the running game or has another stack / initial time / remaining time (more than 1 s off), or a
    difficulty holder is not the fight's own (Normal);
  - the save copy, the source save or the owner's library change, or a window shows on a display. (The copy never sees the
    owner's save folders: its HOME, data folder and game saves folder are its own.)
  - a click on a team row's picture or on a World units card does not tick / untick it like its box (CARD RULE);
  - "Pick 1-4" on a folded world does not pick the save's first 4 units in the order of the game's own portrait bar
    (checked by loading that save copy in the game and reading the bar), Squad does not show 4 / 4, or the world opens.
  The running game's units are read from a save the game itself writes of the test world right after the load, paused
  (tf_test_save: the game's own CampaignSaveLoader.Save).
  saveunits_game_test.py   env: HM_BUILD = a loader build folder (dsload.dylib trusting the dev key, mods/multiplayer,
                                           mods/custom-modes)
                                LD_ROOT = a ds_modloader checkout (its loader/test helpers; default ~/ds_launcher)
                                LANGTYPE = the game's language (3 de, 4 ru, 7 ja; default en): shots + text fit only, no fight
                                ST_SAVE = the real save to copy (default: the 2026-10-11 save backup's Elven Expedition)
Output: ST_OUT (default <HM_BUILD>/../test_env/saveunits/<time>): results.json, frames in drv/cap/."""
import glob, hashlib, json, os, shutil, subprocess, sys, threading, time, traceback, zipfile

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(os.path.dirname(HERE))
LD = os.path.expanduser(os.environ.get('LD_ROOT', '~/ds_launcher'))
sys.path.insert(0, os.path.join(LD, 'loader', 'test'))
import hostmods_pair as HP  # noqa: E402  (copies, kits, start/stop, owner screen guard)
from hostmods_pair import lrig, log, check  # noqa: E402

X = 'TFS'; HP.H = HP.G = X
OUT = os.environ.get('ST_OUT') or os.path.join(os.path.dirname(HP.BUILD), 'test_env', 'saveunits', time.strftime('%Y%m%d-%H%M%S'))
os.makedirs(OUT, exist_ok=True)
DRV = os.path.join(OUT, 'drv'); os.makedirs(os.path.join(DRV, 'cap'), exist_ok=True)
LANG = os.environ.get('LANGTYPE')
SAVE = os.environ.get('ST_SAVE') or os.path.expanduser('~/DungeonSettlers_SaveBackups/2026-10-11-1827/Saves/ElvenExpedition__AutoSave_f816ebc6195c432c9fff754b75ee3738_0.json')
OWNER_LIB = os.path.expanduser('~/Library/Application Support/CanOpener/Dungeon Settlers/Mods/units/library.json')
LIB = os.path.join(OUT, 'lib', 'library.json')
GAME_SAVES = os.path.join(OUT, 'game_saves')
COL_LAB = 5                                                   # Units Laboratory's child index in the title column
MONSTER = 'Weak Grey Rat'


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
            # errors='replace': the test registry keeps a control's text cut at a byte count (cm_win.c Reg.text), and a long
            # Russian text ends inside a character there
            a = json.load(open(os.path.join(DRV, 'resp.json'), encoding='utf-8', errors='replace'))
            if a.get('seq') == seq[0]:
                return a
        except (OSError, ValueError):
            pass
        time.sleep(0.2)
    raise RuntimeError(f'no answer to {q}')


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


def shot(name):
    time.sleep(1.0); req('capture', name=name); log('shot', name)


def column():
    return [c.split('|') for c in (req('state').get('title_column') or '').split(';') if c]


def by_index(rows, i):
    return next((r[0] for r in rows if len(r) > 2 and r[2] == str(i) and r[1] == '1'), None)


def wait(what, pred, timeout=120, every=1.0):
    try:
        return HP.wait(what, pred, timeout, every)
    except RuntimeError as e:
        log(str(e)); return None


def save_levels(path):
    """unit guid -> ProgressionStats.Level in the save (what the fight must show)"""
    s = json.load(open(path, encoding='utf-8'))
    out = {}
    for g, comps in (s.get('EntityComponentSaveData', {}).get('EntityComponents') or {}).items():
        for c in comps or []:
            if c.get('Type') == 0:
                lv = ((c.get('Data') or {}).get('ProgressionStats') or {}).get('Level')
                if lv is not None:
                    out[g.lower().replace('-', '')] = int(round(lv))
    return out


def units_carry(path):
    """clan units of a save: guid -> {name key, bag [(slot, key, amount)], aff {key: (Stack, Duration, InitialDuration)}}"""
    s = json.load(open(path, encoding='utf-8'))
    ec = s['EntityComponentSaveData']['EntityComponents']; out = {}
    for v in (s['EntityContainerSaveData'].get('UnitContainerSaveData') or {}).values():
        for g in (v or {}).get('PlayerGuidList') or []:
            cs = {c['Type']: c['Data'] for c in ec.get(g) or [] if c}
            out[g.lower()] = {'name': (cs.get(10) or {}).get('UnitNameTextKey'),
                              'bag': [(i, x['Key'], x['Amount']) for i, x in enumerate((cs.get(5) or {}).get('ItemSlots') or []) if x and x.get('Key')],
                              'aff': {h['Key']: (h.get('Stack'), h.get('Duration'), h.get('InitialDuration')) for h in (cs.get(3) or {}).get('AffecterHolders') or []}}
    return out


def by_name(units, name):
    return next((u for u in units.values() if u['name'] == name), None)


def bar_order(path):
    """the clan units in the order of the game's portrait bar: the map the save is on (CampaignSaveHeader.CurrentMap,
    MapType Base 0 / Dungeon 1) in PlayerGuidList order, then the other maps' units"""
    s = json.load(open(path, encoding='utf-8')); uc = s['EntityContainerSaveData'].get('UnitContainerSaveData') or {}
    cur = {0: 'Base', 1: 'Dungeon'}.get((s.get('CampaignSaveHeader') or {}).get('CurrentMap'))
    first = list((uc.get(cur) or {}).get('PlayerGuidList') or [])
    return first + [g for v in uc.values() for g in (v or {}).get('PlayerGuidList') or [] if g not in first]


def is_diff(k):
    return k.startswith('AFFECTER_PlayerDifficulty') or k.startswith('AFFECTER_PlayerManagementDifficulty')


def live_save(tag):
    """the game writes the running test world right after the load, paused (Units Laboratory's test call tf_test_save =
    the game's CampaignSaveLoader.Save): what the game holds"""
    HP.c(X, cmd='set_speed', speed=0)
    nm = f'mp_tfscheck_{tag}'; r = req('call', fn='tf_test_save', s=nm); log('test save', r.get('rc'), r.get('out'))
    p = wait('the check save', lambda: next(iter(glob.glob(os.path.join(lrig.idir(X), 'data', 'Saves', nm + '.json'))), None), 30, 1)
    log('live save', p)
    return units_carry(p) if p else {}


def test_world_file():
    try:
        tw = json.load(open(os.path.join(lrig.idir(X), 'data', 'Mods', 'custom-modes', 'test_worlds.json'))).get('worlds') or []
    except (OSError, ValueError):
        return None, []
    f = next((p for p in glob.glob(os.path.join(lrig.idir(X), 'data', 'Saves', '*.json')) if any(g in p for g in tw)), None)
    return f, tw


def open_world(sv):
    """World units tab, the save copy's world opened: (index, unit guids) or (None, [])"""
    click('fights:l.src2', 2)
    s = tf(); rows = [w.split('|') for w in (s.get('worlds') or '').split(';') if w]
    gi = next((i for i, w in enumerate(rows) if w[0] == sv), None)
    check('the World units tab lists the save copy\'s world (game saves) and the modded saves\' world', s.get('src') == 2 and gi is not None and len(rows) >= 2, rows)
    check('World units: every world starts folded when the window opens', s.get('wsel') == -1, s.get('wsel'))
    if gi is None:
        return None, []
    click(f'fights:l.world{gi}', 3)
    s = tf(); us = [u for u in (s.get('wunits') or '').split(',') if u]
    return gi, us


def folded():
    click('fights:l.src0', 1.5)
    return req('rect', target='fights:l.team0').get('rc', -1) > 0 and req('rect', target='fights:l.team0.u0').get('rc', -1) <= 0


def teams_folded_part():
    """task 2 (owner: "team units should be collapsed by default rather than extended")"""
    check('My Units: a team row starts folded (its units not listed) on the first open', folded(), '')
    click('fights:l.team0', 2)
    s = tf()
    check('the team box adds all its units while the team is folded', s.get('src') == 0 and s.get('squad') == 3 and req('rect', target='fights:l.team0.u0').get('rc', -1) <= 0, (s.get('src'), s.get('squad')))
    click('fights:l.team0.open', 1.5)
    shown = req('rect', target='fights:l.team0.u0').get('rc', -1) > 0
    shot(f'team_open_{LANG or "en"}')
    click('fights:l.team0.open', 1.5)
    check('the arrow opens the team (its units listed) and closes it again', shown and req('rect', target='fights:l.team0.u0').get('rc', -1) <= 0, shown)
    click('fights:l.team0', 1.5)                                  # its units out of the squad again
    # CARD RULE (owner: "Clicking on the cards doesn't allow to turn on or off select"): a click on the team's picture
    click('fights:l.team0.icon', 1.5); n1 = tf().get('squad')
    click('fights:l.team0.icon', 1.5); n0 = tf().get('squad')
    check('a click on the team row (its picture, not the box) adds all its units, a second one takes them out', n1 == 3 and n0 == 0, (n1, n0))


def pick_rat():
    for i in range(4):
        if tf().get('mode') == 'arena':
            break
        cmf(cmd='set', name='fights:r.mode', index=i); time.sleep(0.8)
    cmf(cmd='type', name='fights:r.monq', text=MONSTER); time.sleep(0.8)
    cmf(cmd='set', name='fights:r.moncount', index=0); time.sleep(0.5)
    ctl = cmf(cmd='state').get('controls') or {}
    hit = next((n for n, c in ctl.items() if n.startswith('fights:r.hit') and c.get('t') == MONSTER), None)
    if hit:
        cmf(cmd='click', name=hit); time.sleep(0.8)
    for i in range(8):                                            # the battle kits off: the bag must be the save's alone
        k = ctl.get(f'fights:r.kit{i}')
        if k and k.get('v') == 1:
            cmf(cmd='click', name=f'fights:r.kit{i}'); time.sleep(0.5)
    return hit


def start_and_load():
    t0 = time.time()
    click('fights:start', 0.3)
    ok = wait('test world loaded', lambda: HP.st(X).get('scene') == 2, 240, 0.5)
    check('Start loads the test world', ok, f'{time.time() - t0:.0f} s; tf {tf() if not ok else ""}')
    return ok


def bag_check(tag, src_units, live, picked):
    bad = []
    for g in picked:
        su = src_units.get(g.lower()); lu = by_name(live, su['name']) if su else None
        if not su or not lu or lu['bag'] != su['bag']:
            bad.append((su and su['name'], su and su['bag'], lu and lu['bag']))
    check(f'[{tag}] each unit\'s bag in the fight = its bag in the save (slot, key, amount)', not bad and picked, bad[:2])


def back_to_title():
    f, tw = test_world_file()
    HP.c(X, cmd='mpm_quit_title')
    back = wait('title', lambda: HP.st(X).get('scene') == 1, 120, 2)
    left = wait('test world removed', lambda: not [p for p in glob.glob(os.path.join(lrig.idir(X), 'data', 'Saves', '*.json')) if any(g in p for g in tw)], 30, 1) if tw else None
    check('back on the title, the test world\'s saves are removed', back and tw and left, (back, tw))
    time.sleep(4)


def open_fights():
    ok = click('title:' + (by_index(column(), COL_LAB) or 'Units Laboratory'), 3) and click('home:Test Fights', 4)
    check('Units Laboratory > Test Fights opened', ok)
    return ok


def run():
    s = wait('Test Fights engine', lambda: (lambda t: t if t.get('engine') in (2, 3) else None)(tf()), 300, 2) or {}
    check('Test Fights opened and the wiki\'s fight maker is ready', s.get('engine') == 2, s)
    teams_folded_part()
    sv = os.path.splitext(os.path.basename(SAVE))[0]
    gi, us = open_world(sv)
    check('the world shows its save\'s units as cards (read only), each with a tick', gi is not None and len(us) >= 2 and req('rect', target='fights:l.wcard0').get('rc', -1) > 0
          and req('rect', target='fights:l.wtick1').get('rc', -1) > 0, (gi, len(us)))
    if len(us) < 2:
        return
    check('"Bring buffs and effects from the save" starts off', tf().get('waff') == 0 and req('rect', target='fights:l.waff').get('rc', -1) > 0, tf().get('waff'))
    # CARD RULE: a click anywhere on a unit's card (its middle, not the box) ticks it, a second click unticks it
    click('fights:l.wcard1', 1.5); on = [p.split('/')[1] for p in (tf().get('spicks') or '').split(',') if '/' in p]
    click('fights:l.wcard1', 1.5); off = [p.split('/')[1] for p in (tf().get('spicks') or '').split(',') if '/' in p]
    check('a click on a World units card ticks its unit, a second click unticks it', on == [us[1]] and off == [], (on, off))
    pick = [0, 1]                                             # both cards in view (the list is 460 high)
    for k in pick:
        click(f'fights:l.wtick{k}', 2)
    s = tf(); picks = [p.split('/')[1] for p in (s.get('spicks') or '').split(',') if '/' in p]
    check('ticking two units puts exactly those two in the squad', picks == [us[k] for k in pick] and s.get('squad') == 2, (picks, s.get('squad')))
    ctl = cmf(cmd='state').get('controls') or {}
    names = [(ctl.get(f'fights:l.wtick{k}') or {}).get('t', '') for k in pick]
    ov = req('call', fn='mu_test_overflow', s='')
    check(f'cards inside their boxes, texts not wider than their boxes ({LANG or "en"})', ov.get('rc') == 0, (ov.get('out') or '')[-300:])
    shot(f'worlds_{LANG or "en"}')
    if LANG:                                                  # text fit shots only
        return
    src = units_carry(os.path.join(GAME_SAVES, os.path.basename(SAVE)))
    # ---- fight 1: effects off; Custom fight, one Weak Grey Rat, in the boss room, no battle kits
    hit = pick_rat(); s = tf()
    check('Custom fight with one Weak Grey Rat chosen', s.get('mode') == 'arena' and hit is not None, (s.get('mode'), hit, s.get('status')))
    shot('before_start_en')
    levels = save_levels(os.path.join(GAME_SAVES, os.path.basename(SAVE)))
    want = {names[i]: levels.get(us[k].lower().replace('-', '')) for i, k in enumerate(pick)}
    if not start_and_load():
        return
    live = live_save('off')
    bag_check('effects off', src, live, [us[k] for k in pick])
    f, _ = test_world_file(); file_units = units_carry(f) if f else {}
    leak = []
    for k in pick:
        su = src[us[k].lower()]; fu = by_name(file_units, su['name']); lu = by_name(live, su['name'])
        only = set(su['aff']) - set((fu or {}).get('aff') or {})            # the save's own holders the build does not write
        leak += [(su['name'], a) for a in only if not lu or a in lu['aff']]
    check('[effects off] no affecter from the save shows up in the fight', file_units and live and not leak, leak[:4])
    ul = HP.c(X, cmd='list_units').get('units') or []
    pl = [u for u in ul if u.get('faction') == 1 and not u.get('dead')]
    foes = [u for u in ul if u.get('faction') != 1 and not u.get('dead') and u.get('x') is not None]
    got = {}
    for u in pl:
        lv = (HP.c(X, cmd='unit_stats', guid=u['guid']).get('stats') or {}).get('Level')
        got[u['name']] = int(round(lv)) if isinstance(lv, (int, float)) else lv
    check('the squad in the world is the two picked save units, named as the game names them (never a raw text key)', sorted(got) == sorted(want), (got, want))
    check('each unit has its level from the save', got == want, (got, want))
    check('the rat is there', len(foes) == 1 and 'WeakGreyRat' in foes[0].get('key', ''), [(f.get('key'), f.get('hp')) for f in foes])
    if not pl or not foes:
        return
    hp0 = {u['guid']: u['hp'] for u in ul}
    for p in pl:
        HP.c(X, cmd='order_move', guid=p['guid'], x=foes[0]['cell'][0], y=foes[0]['cell'][1])
    HP.c(X, cmd='set_speed', speed=2)
    ch = wait('combat', lambda: [u for u in HP.c(X, cmd='list_units').get('units') or [] if u['guid'] in hp0 and abs(u['hp'] - hp0[u['guid']]) > 0.5], 90, 2)
    check('the fight starts (HP changes)', ch, [(u['name'], round(hp0[u['guid']]), round(u['hp'])) for u in (ch or [])][:4])
    end = wait('the rat killed', lambda: (lambda us_: us_ if not [u for u in us_ if u.get('faction') != 1 and not u.get('dead') and u.get('x') is not None] else None)(
        HP.c(X, cmd='list_units').get('units') or []), 240, 3)
    alive = [u['name'] for u in (end or []) if u.get('faction') == 1 and not u.get('dead')]
    check('the fight ends: the rat is killed, the squad stands', end is not None and len(alive) == len(pl), alive)
    HP.c(X, cmd='set_speed', speed=0)
    shot('fight_end_en')
    back_to_title()
    # ---- fight 2: the window opened again (folded again), effects on, two units that carry things
    if not open_fights():
        return
    wait('Test Fights engine', lambda: tf().get('engine') == 2, 60, 1)
    check('My Units: the team is folded again after the window opened again', folded(), '')
    gi, us = open_world(sv)
    for k in pick:
        cmf(cmd='click', name=f'fights:l.wtick{k}'); time.sleep(0.5)
    full = [k for k, g in enumerate(us) if src.get(g.lower(), {}).get('bag')][:2]
    for k in full:
        cmf(cmd='click', name=f'fights:l.wtick{k}'); time.sleep(0.5)
    click('fights:l.waff', 1.5)
    s = tf(); picks = [p.split('/')[1] for p in (s.get('spicks') or '').split(',') if '/' in p]
    check('two units with a bag picked, effects switch on', len(full) == 2 and picks == [us[k] for k in full] and s.get('waff') == 1, (full, picks, s.get('waff')))
    shot('effects_on_en')
    if not start_and_load():
        return
    live = live_save('on')
    bag_check('effects on', src, live, picks)
    bad = []
    for g in picks:
        su = src[g.lower()]; lu = by_name(live, su['name'])
        for a, (st_, du, ini) in su['aff'].items():
            if is_diff(a):
                continue
            h = (lu or {}).get('aff', {}).get(a)
            if not h or h[0] != st_ or h[2] != ini or abs((h[1] or 0) - (du or 0)) > 1.0:
                bad.append((su['name'], a, (st_, du, ini), h))
        if lu and [a for a in lu['aff'] if is_diff(a) and a not in ('AFFECTER_PlayerDifficultyNormal', 'AFFECTER_PlayerManagementDifficultyNormal')]:
            bad.append((su['name'], 'difficulty', [a for a in lu['aff'] if is_diff(a)]))
    check('[effects on] each unit has every affecter of the save (key, stack, remaining time within 1 s); difficulty = the fight\'s', picks and not bad, bad[:4])
    back_to_title()
    # ---- "Pick 1-4" (owner: "a button to select the leading 4 units of the save"): the game's portrait bar order
    if not open_fights():
        return
    wait('Test Fights engine', lambda: tf().get('engine') == 2, 60, 1)
    click('fights:l.src2', 2)
    rows = [w.split('|') for w in (tf().get('worlds') or '').split(';') if w]
    gi = next((i for i, w in enumerate(rows) if w[0] == sv), None)
    click(f'fights:l.world{gi}.pick', 2)
    s = tf(); got = [p.split('/')[1] for p in (s.get('spicks') or '').split(',') if '/' in p]
    want4 = bar_order(os.path.join(GAME_SAVES, os.path.basename(SAVE)))[:4]
    stl = ((cmf(cmd='state').get('controls') or {}).get('fights:status') or {}).get('t', '')
    check('Pick 1-4 on a folded world: the save\'s first 4 units in the portrait bar order (the map it is on first), Squad 4 / 4, the world stays folded',
          got == want4 and s.get('squad') == 4 and '4 / 4' in stl and s.get('wsel') == -1, (got, want4, s.get('squad'), stl[:40], s.get('wsel')))
    shot('pick4_en')
    names4 = (s.get('snames') or '').split('|')
    HP.c(X, cmd='load_save', file=os.path.join(GAME_SAVES, os.path.basename(SAVE)))      # the same save in the game itself
    ok = wait('the save loaded', lambda: HP.st(X).get('scene') == 2, 180, 1)
    bar = wait('the portrait bar', lambda: (lambda r: r.get('out') if r.get('rc') == 0 and r.get('out') else None)(req('call', fn='tf_test_bar', s='')), 60, 2) or ''
    shown = [b.split('#')[0] for b in bar.split('|') if b]
    check('the 4 picked units are the first 4 on the game\'s own portrait bar for that save, in its order', ok and shown[:4] == names4 and len(names4) == 4, (shown, names4))
    HP.c(X, cmd='mpm_quit_title'); wait('title', lambda: HP.st(X).get('scene') == 1, 120, 2)


def main():
    o0 = fstate(OWNER_LIB); s0 = fstate(SAVE)
    g = threading.Thread(target=HP.guard, daemon=True); g.start()
    res = {}
    try:
        os.makedirs(os.path.dirname(LIB), exist_ok=True)              # 4 fixture units, team A = the first 3 (task 2)
        units = json.load(open(os.path.join(REPO, 'units', 'test', 'data', 'launcher_library.json')))['units'][:4]
        for i, u in enumerate(units):
            u['id'] = f'{0xb0 + i:02x}' * 8; u['name'] = f'Team unit {i}'
        json.dump({'format': 'ds-launcher-units', 'version': 1, 'game_build': '25846389', 'units': units,
                   'teams': [{'id': 'cc' * 8, 'name': 'Team A', 'unit_ids': [u['id'] for u in units[:3]]}]}, open(LIB, 'w'))
        os.makedirs(GAME_SAVES, exist_ok=True)
        cp = os.path.join(GAME_SAVES, os.path.basename(SAVE)); shutil.copy2(SAVE, cp); c0 = fstate(cp)
        HP.make_copy(X)
        pk = {'custom-modes': zip_dir(os.path.join(HP.BUILD, 'mods', 'custom-modes'), 'custom-modes'), 'font-probe': probe_pkg()}
        HP.kit(X, pk, ['custom-modes', 'font-probe'], ['multiplayer', 'custom-modes', 'font-probe'])
        if LANG:
            prep = lrig.prepare_data
            def prep_lang(x):
                prep(x); us = os.path.join(lrig.idir(x), 'data', 'UserSetting.json'); s = json.load(open(us))
                s['GeneralSettingData']['SaveCurrentLanguageType'] = int(LANG); json.dump(s, open(us, 'w'), indent=1)
            lrig.prepare_data = prep_lang
        crash = os.path.join(OUT, 'crash'); os.makedirs(crash, exist_ok=True); open(os.path.join(crash, 'answer'), 'w').write('no')
        HP.room_for(1)
        rd = lrig.ctl_env().get('DSRIG_READ_DIRS', '') + ':' + OUT          # the rig may load the save copy (Pick 1-4 check)
        HP.start(X, 'http://127.0.0.1:9', {'DSL_UNITS_TEST': DRV, 'DSL_CRASHREP_TEST': crash, 'DSMP_UNITS_LIB': LIB, 'DSML_GAME_SAVES': GAME_SAVES, 'DSRIG_READ_DIRS': rd})
        lrig.wait_for(X, lambda s: s.get('scene') == 1 and not s.get('busy'), 'title', timeout=1500); time.sleep(6)
        open_fights()
        run()
        check('the real save copy is unchanged', fstate(cp) == c0)
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
