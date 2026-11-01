#!/usr/bin/env python3
"""Units Laboratory > Test Fights setup (owner notes 2026-10-26 / answers 2026-11-01, PLAN_v013 items 1, 14, 15, 17) in ONE
hidden loader test copy (background-only, small window, muted, notch fix off; never the owner's game, saves, library or
screen) with multiplayer + custom-modes + the test module font-probe (the units test driver). Fails when (the breaks this
catches):
  (1) a Test Fights choice is lost: every choice set to a non-default value (test, room, Magic Saturation, monsters, how
      many, the monster search, combat + own management difficulty, kits, inscription, time of day, My Units team, World
      units picks, their effects switch, the source tab) must read back the same after Start -> the test world ->
      Esc > Quit to title -> Test Fights again, and after the game copy is stopped and started again (owner: "when
      starting a new test the settings of the previous tests are not saved"; "keep ALL test settings, ALWAYS, also after
      the game restarts");
  (2) a kit's hover card opens away from the kit row (owner: "the hover window for the kits ... are all the way to the
      left of the main test frame"): the card must be inside the screen, not over the row, and next to it (gap at most
      12 % of the screen width, the loader kit's rule), level with it;
  (3) a test world starts with Auto Skill ON on a squad unit (owner 2026-11-01: it must start OFF): the game's own save of
      the running world (test call tf_test_save) has BehaviourOption AutoAttack (1) off on every My Units squad unit;
  (4) a test world does not start at 08:00 (day) by default, or at night (21:00) with Time of Day = Night: the game's own
      save of the running world, TimeSaveData hour of day (the game takes day / night from it: 06:00-20:00 = day).
  (5) a Test Fights section does not fold, or does not stay folded (owner 2026-11-01, PLAN_v013 item 6): every heading is
      the kit's section header; a click folds "Difficulty"; still folded after a test, when the window opens again and
      after a game restart; a second click opens it.
  (6) Weather (owner 2026-11-01: "set the map to clear weather by default, and you can add advanced options for weather
      (time of test probably should be an advanced option too)"): the "Advanced" section (Weather + Time of Day) is folded
      the first time and a click opens it; a test world with the default weather shows Clear (the game's live map weather,
      test call tf_test_weather = MapWeatherHandler._nowWeather, and every region of the game's own save) also from an
      Abyss room, whose template holds Rain; with Heavy Rain picked it shows Heavy Rain; the pick is kept after a test and
      a restart, and Advanced stays open.
  setup_test.py   env: HM_BUILD = a loader build folder (dsload.dylib trusting the dev key, mods/multiplayer, mods/custom-modes)
                       LD_ROOT = a ds_modloader checkout (loader/test helpers; default ~/ds_launcher)
                       TS_WIN = a window size for the hover check (e.g. 1920x1080; default 960x540); TS_UISCALE = UI Scale
                       TS_HOVER_ONLY=1 = only (2) (one open of Test Fights, no fight)
Output: TS_OUT (default <HM_BUILD>/../test_env/setup/<time>): results.json, dsload.log, player.log."""
import glob, json, os, shutil, subprocess, sys, threading, time, traceback, zipfile

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(os.path.dirname(HERE))
LD = os.path.expanduser(os.environ.get('LD_ROOT', '~/ds_launcher'))
sys.path.insert(0, os.path.join(LD, 'loader', 'test'))
sys.path.insert(0, HERE)
import hostmods_pair as HP  # noqa: E402  (copies, kits, start/stop, owner screen guard)
from hostmods_pair import lrig, log, check  # noqa: E402
import tf_fresh  # noqa: E402

X = os.environ.get('TS_COPY', 'TSU'); HP.H = HP.G = X   # TS_COPY: another copy name (lanes side by side)
OUT = os.environ.get('TS_OUT') or os.path.join(os.path.dirname(HP.BUILD), 'test_env', 'setup', time.strftime('%Y%m%d-%H%M%S'))
os.makedirs(OUT, exist_ok=True)
DRV = os.path.join(OUT, 'drv'); os.makedirs(os.path.join(DRV, 'cap'), exist_ok=True)
SEED = lrig.SEED_DATA
SAVE = os.path.join(SEED, 'Saves', 'ElvenExpedition__AutoSave_f816ebc6195c432c9fff754b75ee3738_0.json')
WIKI = os.path.expanduser(os.environ.get('TS_WIKI', '~/dungeonsettlersdb/site'))
WIN = os.environ.get('TS_WIN')
UISCALE = os.environ.get('TS_UISCALE')                        # the game's UI Scale (e.g. 1.4)
HOVER_ONLY = os.environ.get('TS_HOVER_ONLY') == '1'
OWNER_LIB = os.path.expanduser('~/Library/Application Support/CanOpener/Dungeon Settlers/Mods/units/library.json')
LIB = os.path.join(OUT, 'lib', 'library.json')
GAME_SAVES = os.path.join(OUT, 'game_saves')
DATA = os.path.join(lrig.idir(X), 'data')
COL_LAB = 5
seq = [0]


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


def call(fn, s=''):
    return req('call', fn=fn, s=s).get('out') or ''


def tf():
    return ((req('state').get('cm') or {}).get('fights') or {}).get('tf') or {}


def cmf(**kw):
    r = req('call', fn='cmf_test', s=json.dumps(kw, separators=(',', ':')))
    try:
        return json.loads(r.get('out') or '{}')
    except ValueError:
        return {'ok': False, 'raw': (r.get('out') or '')[:300]}


def controls():
    return (cmf(cmd='state').get('controls') or {})


def click(t, wait=1.5):
    r = req('click', target=t); time.sleep(wait)
    return r.get('rc') == 0


def column():
    return [c.split('|') for c in (req('state').get('title_column') or '').split(';') if c]


def wait(what, pred, timeout=120, every=1.0):
    try:
        return HP.wait(what, pred, timeout, every)
    except RuntimeError as e:
        log(str(e)); return None


def kept():
    """the kept choices, "key=value;" (Units Laboratory's test call tf_test_kept), as a dict"""
    return dict(p.split('=', 1) for p in call('tf_test_kept').split(';') if '=' in p)


def open_tf(tag):
    lab = next((r[0] for r in column() if r and r[0] == 'Units Laboratory' and len(r) > 1 and r[1] == '1'), None) or 'Units Laboratory'
    ok = click('title:' + lab, 3) and click('home:Test Fights', 3)
    check(f'[{tag}] Units Laboratory > Test Fights opened', ok)
    s = wait('Test Fights engine', lambda: (lambda t: t if t.get('engine') in (2, 3) else None)(tf()), 300, 2) or {}
    check(f'[{tag}] the wiki\'s fight maker is ready', s.get('engine') == 2, s)
    return s.get('engine') == 2


def judge(tag, name, shown, c, e, sw, sh, more=None):
    """a hover card next to its element: inside the screen, off the element, gap <= 12 % of the screen width, level with it"""
    near = max(64.0, 0.12 * sw)
    dx = c[0] - e[2] if c[0] > e[2] else e[0] - c[2] if e[0] > c[2] else 0
    over = min(c[2], e[2]) - max(c[0], e[0]) > 1 and min(c[3], e[3]) - max(c[1], e[1]) > 1
    check(f'[{tag}] {name}: its hover card shows', shown, more)
    check(f'[{tag}] {name}: the card is inside the screen ({sw}x{sh})', c[0] >= -1 and c[1] >= -1 and c[2] <= sw + 1 and c[3] <= sh + 1, (c, sw, sh))
    check(f'[{tag}] {name}: the card does not cover the element', not over, (c, e))
    check(f'[{tag}] {name}: the card is next to the element (gap {dx:.0f} px <= {near:.0f}) and level with it',
          dx <= near and c[1] <= e[3] + 1 and c[3] >= e[1] - 1, {'card': c, 'el': e, 'more': more})


def hover_check(tag):
    """(2) the hover card of each kit row (the loader kit's card) and of the inscription icon (Units Laboratory's cm_gtip,
    the same placement rule)"""
    rows = sorted(k for k in controls() if k.startswith('fights:r.kitrow'))
    check(f'[{tag}] kit rows shown (the hover target of each kit)', rows, list(controls())[:40])
    sw = sh = 0
    for name in rows[:2]:
        cmf(cmd='scroll', name=name, pos=0); time.sleep(0.8)            # the kits are at the bottom of the right list
        req('hover', target=name); time.sleep(2.0)
        t = {}
        for _ in range(6):                                            # the card is moved a few frames after it shows
            t = json.loads(call('tf_test_tip') or '{}'); time.sleep(0.4)
        log('tip', name, json.dumps(t))
        sw, sh = t.get('sw') or 0, t.get('sh') or 0
        judge(tag, name, t.get('shown') == 1, t.get('card') or [0] * 4, t.get('el') or [0] * 4, sw, sh, {'title': t.get('title'), 'panel': t.get('panel')})
        req('mouse_off'); time.sleep(0.8)
    if 'fights:r.inscicon' in controls():
        cmf(cmd='hover', name='fights:r.inscicon'); time.sleep(2.5)
        g = cmf(cmd='hover', name='fights:r.inscicon'); log('gtip', json.dumps(g))
        judge(tag, 'fights:r.inscicon', g.get('card') == 1, g.get('rect') or [0] * 4, g.get('el') or [0] * 4, sw, sh, g.get('key'))
        cmf(cmd='hover'); time.sleep(0.5)

def set_choices():
    """(1) every choice to a value that is not its default, through the window's own controls (cm_win cmf_test)"""
    for i in range(6):                                                # Custom Fight: its room, Magic Saturation, monsters
        if tf().get('mode') == 'arena':
            break
        cmf(cmd='set', name='fights:r.mode', index=i); time.sleep(0.8)
    for i in range(8):                                                # an Abyss room (Magic Saturation shows)
        cmf(cmd='set', name='fights:r.boss', index=i); time.sleep(0.6)
        if 'fights:r.msat' in controls():
            break
    if 'fights:r.msat' in controls():
        cmf(cmd='set', name='fights:r.msat', index=2); time.sleep(0.5)
    cmf(cmd='type', name='fights:r.monq', text='Rat'); time.sleep(1.0)
    cmf(cmd='set', name='fights:r.moncount', index=4); time.sleep(0.5)   # 5
    cmf(cmd='click', name='fights:r.hit0'); time.sleep(1.0)
    cmf(cmd='set', name='fights:r.combat', index=2); time.sleep(0.8)
    cmf(cmd='click', name='fights:r.mgmtown'); time.sleep(0.8)
    cmf(cmd='set', name='fights:r.mgmt', index=0); time.sleep(0.5)
    for k in ('fights:r.kit0', 'fights:r.kit1'):
        cmf(cmd='click', name=k); time.sleep(0.8)
    if 'fights:r.insc' in controls():
        cmf(cmd='click', name='fights:r.insc'); time.sleep(0.8)
    adv_open()
    cmf(cmd='set', name='fights:r.tod', index=1); time.sleep(0.5)     # Night (the weather stays Clear: the default's check)
    click('fights:l.src0', 1.5); click('fights:l.team0', 1.5)         # My Units: the 4-unit team
    click('fights:l.src2', 2.0); cmf(cmd='click', name='fights:l.waff'); time.sleep(0.8)
    click('fights:l.world0', 2.5); click('fights:l.world0.pick', 2.0)   # World units: Pick 1-4 of the seed save


def adv_open():
    """(6) open the folded "Advanced" section with a click on its header (scrolled into view: it is the last section)"""
    if 'fights:r.tod' in controls():
        return True
    cmf(cmd='scroll', name='fights:sec.advanced', pos=0); time.sleep(0.8)
    click('fights:sec.advanced', 1.5)
    c = controls()
    check('[advanced] a click on "Advanced" opens it: Weather and Time of Day show', 'fights:r.weather' in c and 'fights:r.tod' in c,
          sorted(k for k in c if 'sec.' in k or k in ('fights:r.weather', 'fights:r.tod')))
    return 'fights:r.tod' in c


def game_save(tag):
    """the game writes the running test world (Units Laboratory's test call tf_test_save = CampaignSaveLoader.Save)"""
    HP.c(X, cmd='set_speed', speed=0)
    nm = f'mp_tsucheck_{tag}'; log('test save', call('tf_test_save', nm))
    p = wait('the check save', lambda: next(iter(glob.glob(os.path.join(DATA, 'Saves', nm + '.json'))), None), 30, 1)
    if not p:
        return None
    s = json.load(open(p, encoding='utf-8-sig'))
    shutil.copy2(p, os.path.join(OUT, os.path.basename(p)))
    return s


def clan_options(s):
    """BehaviourOption of each unit of the player's clan (EntityInfoComponent, Type 1: Option)"""
    ec = s['EntityComponentSaveData']['EntityComponents']; uc = s['EntityContainerSaveData'].get('UnitContainerSaveData') or {}
    out = {}
    for side in uc.values():
        for g in (side or {}).get('PlayerGuidList') or []:
            info = next((c['Data'] for c in ec.get(g) or [] if c and c.get('Type') == 1), None)
            if info is not None:
                out[g] = info.get('Option', 0)
    return out


def fight(tag, want_hour, autoskill_off, want_weather):
    t0 = time.time()
    cmf(cmd='click', name='fights:start')
    ok = wait('test world loaded', lambda: HP.st(X).get('scene') == 2, 300, 2)
    check(f'[{tag}] Start loads the test world', ok, f'{time.time() - t0:.0f} s; tf {tf() if not ok else ""}')
    if not ok:
        return False
    time.sleep(8)
    live = call('tf_test_weather')
    check(f'[{tag}] the map shows the weather {want_weather} (the game\'s MapWeatherHandler)', live == want_weather, live)
    s = game_save(tag)
    check(f'[{tag}] the game wrote the running test world', s is not None)
    if s:
        t = s.get('TimeSaveData') or {}
        check(f'[{tag}] the test world\'s clock is {want_hour:02d}:00 ({"day" if 6 <= want_hour < 20 else "night"}) as the game holds it',
              t.get('Hour', -1) % 24 == want_hour and t.get('Tick', 0) // 45 % 60 <= 30, t)               # the world runs a few game minutes before the save
        e = s.get('EnvironmentSaveData') or {}; wd = e.get('WeatherDatas') or {}
        check(f'[{tag}] every region of the game\'s save holds {want_weather}, no re-roll during the test',
              wd and set(wd.values()) == {want_weather} and min(e.get('BaseWeatherChangeMin', 0), e.get('DungeonWeatherChangeMin', 0)) > t.get('Min', 0) + 1440,
              {'weather': wd, 'base': e.get('BaseWeatherChangeMin'), 'dungeon': e.get('DungeonWeatherChangeMin'), 'min': t.get('Min')})
        o = clan_options(s)
        if autoskill_off:
            check(f'[{tag}] every squad unit starts with Auto Skill OFF (BehaviourOption AutoAttack 1 not set)', o and not [g for g, v in o.items() if v & 1], o)
        else:
            log(f'[{tag}] World units keep the save\'s Auto Skill bit:', o)
    HP.c(X, cmd='set_speed', speed=1)
    HP.c(X, cmd='mpm_quit_title')
    back = wait('title', lambda: HP.st(X).get('scene') == 1, 180, 2)
    check(f'[{tag}] Esc > Quit to title goes back to the title', back)
    time.sleep(5)
    return bool(back)


def start_copy():
    HP.room_for(1)
    crash = os.path.join(OUT, 'crash'); os.makedirs(crash, exist_ok=True); open(os.path.join(crash, 'answer'), 'w').write('no')
    HP.start(X, 'http://127.0.0.1:9', {'DSL_UNITS_TEST': DRV, 'DSL_CRASHREP_TEST': crash, 'DSMP_UNITS_LIB': LIB, 'DSML_GAME_SAVES': GAME_SAVES,
                                       'DSCM_WIKI_DIR': WIKI})
    lrig.wait_for(X, lambda s: s.get('scene') == 1 and not s.get('busy'), 'title', timeout=1500); time.sleep(6)


def run():
    if not open_tf('first open'):
        return
    k0 = kept(); log('defaults', k0)
    check('[first open] Time of Day defaults to Day', k0.get('tod') == 'day', k0)
    check('[first open] Weather defaults to Clear (Sunny)', k0.get('weather') == 'Sunny', k0)
    c0 = controls()
    check('[first open] "Advanced" is folded: its header shows, Weather and Time of Day do not', 'fights:sec.advanced' in c0 and 'fights:r.tod' not in c0 and 'fights:r.weather' not in c0,
          sorted(k for k in c0 if 'sec.' in k or k in ('fights:r.weather', 'fights:r.tod')))
    hover_check('first open')
    if HOVER_ONLY:
        return
    set_choices()
    k1 = kept(); log('set', k1)
    want = ('mode', 'boss', 'combat', 'mgmt_own', 'kits', 'mons', 'moncount', 'monq', 'src', 'ids', 'sv', 'svn', 'waff', 'tod')
    same = [k for k in want if k1.get(k) == k0.get(k)]
    check('every choice was changed from its default (setup of the check)', not same and k1.get('src') == '2' and k1.get('tod') == 'night',
          {k: (k0.get(k), k1.get(k)) for k in same})
    # a new test: Start (World units, night) -> the world -> title -> Test Fights again
    if not fight('night, World units, default weather', 21, False, 'Sunny'):
        return
    if not open_tf('after a test'):
        return
    k2 = kept()
    check('[after a test] every choice is as it was before Start', k2 == k1, {k: (k1.get(k), k2.get(k)) for k in set(k1) | set(k2) if k1.get(k) != k2.get(k)})
    # (5) a section folds (the kit's dsl_section): Difficulty folded by a click on its header
    check('[sections] every heading is a fold header (the kit\'s section)', all(f'fights:sec.{k}' in controls() for k in ('squad', 'test', 'difficulty', 'kits')),
          [k for k in controls() if k.startswith('fights:sec.')])
    click('fights:sec.difficulty', 1.5)
    c = controls()
    check('[sections] a click on "Difficulty" folds it: its header stays, its rows go', 'fights:sec.difficulty' in c and 'fights:r.combat' not in c,
          [k for k in c if k.startswith('fights:sec.') or k == 'fights:r.combat'])
    # day + My Units: the default time, Auto Skill off
    click('fights:l.src0', 1.5); cmf(cmd='set', name='fights:r.tod', index=0); time.sleep(0.8)
    check('[advanced] still open after a test', 'fights:r.weather' in controls())
    cmf(cmd='set', name='fights:r.weather', index=2); time.sleep(0.8)  # Heavy Rain
    k3 = kept(); t3 = tf()
    check('[day] My Units team still picked (4 units, the team named), Time of Day = Day, Weather = Heavy Rain',
          t3.get('squad') == 4 and t3.get('team') == 0 and k3.get('tod') == 'day' and k3.get('weather') == 'HeavyRain',
          (t3.get('squad'), t3.get('team'), k3.get('tod'), k3.get('weather')))
    if not fight('day, My Units, Heavy Rain', 8, True, 'HeavyRain'):
        return
    if not open_tf('after a test, Difficulty folded'):
        return
    c = controls()
    check('[sections] Difficulty still folded when Test Fights opens again', 'fights:sec.difficulty' in c and 'fights:r.combat' not in c, sorted(k for k in c if 'sec.' in k or k == 'fights:r.combat'))
    # a restart of the game copy
    HP.stop(X); time.sleep(3)
    start_copy()
    if not open_tf('after a restart'):
        return
    c = controls()
    check('[sections] Difficulty still folded after the game restarts', 'fights:sec.difficulty' in c and 'fights:r.combat' not in c, sorted(k for k in c if 'sec.' in k or k == 'fights:r.combat'))
    click('fights:sec.difficulty', 1.5)
    check('[sections] a second click opens it again', 'fights:r.combat' in controls())
    check('[advanced] still open after the game restarts', 'fights:r.weather' in controls())
    k4 = kept(); t4 = tf()
    check('[after a restart] the squad is the same team (the save keeps its name)', (t4.get('squad'), t4.get('team')) == (t3.get('squad'), t3.get('team')),
          ((t4.get('squad'), t4.get('team')), 'before', (t3.get('squad'), t3.get('team'))))
    check('[after a restart] every choice is as it was before the restart', k4 == k3, {k: (k3.get(k), k4.get(k)) for k in set(k3) | set(k4) if k3.get(k) != k4.get(k)})


def main():
    o0 = (os.path.getmtime(OWNER_LIB) if os.path.exists(OWNER_LIB) else None)
    g = threading.Thread(target=HP.guard, daemon=True); g.start()
    res = {}
    try:
        fx = json.load(open(os.path.join(REPO, 'units', 'test', 'data', 'launcher_library.json')))
        units = fx['units'][:4]
        for i, u in enumerate(units):
            u['id'] = f'{0xa0 + i:02x}' * 8
        os.makedirs(os.path.dirname(LIB), exist_ok=True)
        json.dump({'format': 'ds-launcher-units', 'version': 1, 'game_build': '25846389', 'units': units,
                   'teams': [{'id': 'cc' * 8, 'name': 'Setup squad', 'unit_ids': [u['id'] for u in units]}]}, open(LIB, 'w'), indent=1)
        os.makedirs(GAME_SAVES, exist_ok=True); shutil.copy2(SAVE, os.path.join(GAME_SAVES, os.path.basename(SAVE)))
        if os.path.exists(DATA):                                      # a fresh data folder each run (never deleted: set aside)
            aside = os.path.expanduser(f'~/ToDelete_{time.strftime("%Y-%m-%d")}/setup_test/data_{time.strftime("%H%M%S")}')
            os.makedirs(os.path.dirname(aside), exist_ok=True); shutil.move(DATA, aside)
        HP.make_copy(X)
        tf_fresh.fresh_settings(DATA, OUT)
        pk = {'custom-modes': zip_dir(os.path.join(HP.BUILD, 'mods', 'custom-modes'), 'custom-modes'), 'font-probe': probe_pkg()}
        HP.kit(X, pk, ['custom-modes', 'font-probe'], ['multiplayer', 'custom-modes', 'font-probe'])
        for f in ('OutGame.json', 'UserCampaignLog.json'):
            shutil.copy2(os.path.join(SEED, f), os.path.join(DATA, f))
        if WIN:                                                       # another window size: the copy's settings and its start arguments
            ww, wh = (int(v) for v in WIN.split('x'))
            lrig.win_size = lambda: (ww, wh)
            popen = HP.subprocess.Popen
            def popen_size(args, *a, **kw):
                if '-screen-width' in args:
                    args = list(args); args[args.index('-screen-width') + 1] = str(ww); args[args.index('-screen-height') + 1] = str(wh)
                return popen(args, *a, **kw)
            HP.subprocess.Popen = popen_size
        if UISCALE:
            prep = lrig.prepare_data
            def prep_scale(x):
                prep(x); us = os.path.join(lrig.idir(x), 'data', 'UserSetting.json'); u = json.load(open(us))
                u['GraphicSettingData']['UIScale'] = float(UISCALE); json.dump(u, open(us, 'w'), indent=1)
            lrig.prepare_data = prep_scale
        start_copy()
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
        check("the owner's library file is unchanged", (os.path.getmtime(OWNER_LIB) if os.path.exists(OWNER_LIB) else None) == o0)
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
