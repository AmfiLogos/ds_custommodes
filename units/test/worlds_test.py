#!/usr/bin/env python3
"""My Units 0.1.15 (owner 2026-10-25) in one hidden loader test copy (ULW: background-only, small window, muted, notch fix
off; never the owner's game, saves, library or screen) with multiplayer + custom-modes + the test module font-probe (the
units test driver). The copy reads and writes a TEST library (DSMP_UNITS_LIB, made here from the launcher fixture) and its
game saves folder is a test folder holding a COPY of a real save (DSML_GAME_SAVES); its modded saves are its own.
Fails when (the breaks this catches):
  - "Delete team" > "Team and units" deletes another unit than the team's units that are in no other team, or loses a unit
    of another team, or the number the box shows is not the number deleted; "Team only" deletes a unit; Cancel changes
    anything; the file on disk does not say the same as the window.
  - the plain unit Delete (tick, Delete, confirm) does not delete exactly the ticked units from the file.
  - World units: a world of the game saves or the modded saves is missing, or the shown units are not the units of that
    save; the world's units can be renamed / edited / deleted (they are read only).
  - "Save snapshot": the new library units are not the ticked save units (source unit guid, world, save, save time), or the
    card shows no source line.
  - a library card shows a wrong or stale team list (the teams its unit is in, read from the file) after Save as team,
    Rename, Delete team (both choices) or Delete, on any page; a save's card shows teams.
  - the owner's real library file changed (sha256 + time), or a save file changed.
  worlds_test.py           env: HM_BUILD = a loader build folder (dsload.dylib, mods/multiplayer, mods/custom-modes)
                                LD_ROOT = a ds_modloader checkout (its loader/test helpers; default ~/ds_launcher)
                                LANGTYPE = the game's language (3 de, 4 ru, 7 ja; default en): shots only for text fit
                                WT_SAVE = the real save to copy (default: the 2026-10-11 save backup's Elven Expedition)
Output: WT_OUT (default <HM_BUILD>/../test_env/worlds/<time>): results.json, frames in drv/cap/."""
import hashlib, json, os, re, shutil, subprocess, sys, threading, time, traceback, zipfile

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(os.path.dirname(HERE))
LD = os.path.expanduser(os.environ.get('LD_ROOT', '~/ds_launcher'))
sys.path.insert(0, os.path.join(LD, 'loader', 'test'))
import hostmods_pair as HP  # noqa: E402  (copies, kits, start/stop, owner screen guard)
from hostmods_pair import lrig, log, check  # noqa: E402

X = 'ULW'; HP.H = HP.G = X
OUT = os.environ.get('WT_OUT') or os.path.join(os.path.dirname(HP.BUILD), 'test_env', 'worlds', time.strftime('%Y%m%d-%H%M%S'))
os.makedirs(OUT, exist_ok=True)
DRV = os.path.join(OUT, 'drv'); os.makedirs(os.path.join(DRV, 'cap'), exist_ok=True)
LANG = os.environ.get('LANGTYPE')
SAVE = os.environ.get('WT_SAVE') or os.path.expanduser('~/DungeonSettlers_SaveBackups/2026-10-11-1827/Saves/ElvenExpedition__AutoSave_f816ebc6195c432c9fff754b75ee3738_0.json')
OWNER_LIB = os.path.expanduser('~/Library/Application Support/CanOpener/Dungeon Settlers/Mods/units/library.json')
LIB = os.path.join(OUT, 'lib', 'library.json')
GAME_SAVES = os.path.join(OUT, 'game_saves')
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


def make_library():
    """6 fixture units; team A = u0 u1 u2, team B = u2 u3, a long-named team C = u2 (u2 in three: its teams get cut)"""
    fx = json.load(open(os.path.join(REPO, 'units', 'test', 'data', 'launcher_library.json')))
    units = fx['units'][:6]
    for i, u in enumerate(units):
        u['id'] = f'{0xa0 + i:02x}' * 8; u['name'] = f'Test unit {i}'
    lib = {'format': 'ds-launcher-units', 'version': 1, 'game_build': '25846389', 'units': units,
           'teams': [{'id': 'aa' * 8, 'name': 'Team A', 'unit_ids': [units[0]['id'], units[1]['id'], units[2]['id']]},
                     {'id': 'bb' * 8, 'name': 'Team B', 'unit_ids': [units[2]['id'], units[3]['id']]},
                     {'id': 'cc' * 8, 'name': 'Shield & Greatsword Fellowship of the Northern Hills', 'unit_ids': [units[2]['id']]}]}
    os.makedirs(os.path.dirname(LIB), exist_ok=True)
    json.dump(lib, open(LIB, 'w'), indent=2)
    return [u['id'] for u in units]


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
        time.sleep(0.2)
    raise RuntimeError(f'no answer to {q}')


def mu():
    return (req('state').get('cm') or {}).get('mu') or {}


def click(t, wait=1.5):
    r = req('click', target=t); time.sleep(wait)
    log('click', t, r.get('rc'), r.get('text'))
    return r.get('rc') == 0


def shown(t):
    return req('rect', target=t).get('rc', -1) > 0


def shot(name):
    time.sleep(1.0); req('capture', name=name); log('shot', name)


def disk():
    d = json.load(open(LIB))
    return d, {u['id'] for u in d['units']}, {t['name']: t['unit_ids'] for t in d.get('teams', [])}


def column():
    return [c.split('|') for c in (req('state').get('title_column') or '').split(';') if c]


def by_index(rows, i):
    return next((r[0] for r in rows if len(r) > 2 and r[2] == str(i) and r[1] == '1'), None)


def team_text_ok(txt, want):
    """the card's teams text for a unit in teams `want` (file order): "Team(s): a, b", or the first k names + " +N" """
    if not want:
        return txt == ''
    if LANG:
        return txt != ''
    pre = 'Team: ' if len(want) == 1 else 'Teams: '
    if not txt.startswith(pre):
        return False
    body = txt[len(pre):]
    if body == ', '.join(want):
        return True
    m = re.match(r'^(.*) \+(\d+)$', body)
    k = len(want) - int(m.group(2)) if m else 0
    return bool(m) and 1 <= k < len(want) and m.group(1) == ', '.join(want[:k])


def teams_check(label):
    """every card of the list shown (all pages): the teams its unit is in now, read from the library file"""
    d = json.load(open(LIB))
    inteams = {u['id']: [t['name'] for t in d.get('teams', []) if u['id'] in t['unit_ids']] for u in d['units']}
    s = mu(); pp = max(1, s.get('per_page', 1)); pages = max(1, -(-s.get('shown', 0) // pp)); bad = []; seen = 0
    for p in range(pages):
        if p:
            click('mu:next', 1); s = mu()
        for c in s.get('card_list') or []:
            if not c.get('id'):
                continue
            seen += 1
            if c['id'] not in inteams or not team_text_ok(c.get('teams', ''), inteams[c['id']]):
                bad.append((c['id'], c.get('teams'), inteams.get(c['id'])))
    for p in range(pages - 1):                                    # back to the first page (the next step ticks there)
        click('mu:prev', 1)
    check(f'{label}: every card shows the teams its unit is in now (all {pages} pages)', not bad and seen == s.get('shown') and mu().get('page') == 0, bad[:3] or seen)


def run(ids):
    s = mu()
    check('My Units opened on the TEST library (never the owner\'s file)', s.get('open') == 1 and s.get('lib') == LIB and s.get('units') == 6 and s.get('teams') == 3,
          (s.get('lib'), s.get('units'), s.get('teams')))
    worlds = s.get('worlds') or []
    sv = os.path.splitext(os.path.basename(SAVE))[0]
    check('World units: the game saves folder\'s world (the real save copy) and the modded saves\' world, in one list, newest first',
          any(w.split('|')[2] == sv and w.split('|')[1] == '0' for w in worlds) and any('MoonlightBrigade' in w.split('|')[2] and w.split('|')[1] == '1' for w in worlds)
          and [w.split('|')[3] for w in worlds] == sorted((w.split('|')[3] for w in worlds), reverse=True), worlds)
    if LANG:                                                      # text fit shots only
        shot(f'teams_{LANG}')
        ov = req('call', fn='mu_test_overflow', s='')
        check(f'cards with their teams inside their boxes ({LANG})', ov.get('rc') == 0, (ov.get('out') or '')[-300:])
        click('mu:team0'); click('mu:delete_team'); shot(f'dlg_{LANG}'); click('mu:dlg_cancel')
        click('mu:world0', 3); shot(f'world_{LANG}')
        click('mu:tick0'); click('mu:snapshot', 2); click('mu:all', 2)
        s = mu(); pp = max(1, s.get('per_page', 1))
        for _ in range((s.get('shown', 0) - 1) // pp):                # the snapshot unit is the last one: its page
            click('mu:next', 1)
        s = mu(); shot(f'src_{LANG}')
        check(f'the snapshot unit\'s source line is on screen ({LANG})', s.get('src_lines') == 1, s.get('src_lines'))
        ov = req('call', fn='mu_test_overflow', s='')
        check(f'cards and source lines inside their boxes ({LANG})', ov.get('rc') == 0, (ov.get('out') or '')[-300:])
        return
    shot('teams_en')
    teams_check('start')
    ov = req('call', fn='mu_test_overflow', s='')
    check('cards with their teams inside their boxes (en)', ov.get('rc') == 0, (ov.get('out') or '')[-300:])
    # ---- Save as team: u0 + u2 -> a new team (u2 then in four teams)
    click('mu:all'); click('mu:tick0'); click('mu:tick2'); click('mu:save_team', 2)
    _, _, dt = disk()
    check('Save as team: a new team of the 2 ticked units in the file', len(dt) == 4 and any(v == [ids[0], ids[2]] for v in dt.values()), dt)
    teams_check('Save as team (the new team\'s view)')
    click('mu:all', 2); teams_check('Save as team (All units)')
    # ---- Rename u2: its teams stay on its card
    click('mu:tick2'); click('mu:rename'); req('field_set', target='mu:rename_field', text='Renamed two'); click('mu:rename_ok', 2)
    d, _, _ = disk()
    check('Rename: the unit\'s new name is in the file', next(u['name'] for u in d['units'] if u['id'] == ids[2]) == 'Renamed two')
    teams_check('Rename')
    click('mu:tick2'); check('no unit ticked any more', mu().get('ticked') == 0)
    # ---- Delete team: the box, Cancel
    click('mu:team0'); click('mu:delete_team')
    s = mu(); shot('dlg_en')
    check('Delete team shows the box with Team and units / Team only / Cancel', s.get('dlg') == 1 and shown('mu:dlg_units') and shown('mu:dlg_only') and shown('mu:dlg_cancel'), s.get('dlg'))
    d0, du0, dt0 = disk()
    click('mu:dlg_cancel'); s = mu(); _, du, dt = disk()
    check('Cancel: nothing deleted', s.get('dlg') == 0 and du == du0 and dt == dt0, (s.get('units'), s.get('teams')))
    # ---- Team and units on team A: its units in no other team go, the others stay, other teams keep their units
    gone = {u for u in dt0['Team A'] if not any(u in v for k, v in dt0.items() if k != 'Team A')}
    click('mu:team0'); click('mu:delete_team'); click('mu:dlg_units', 2)
    s = mu(); _, du, dt = disk()
    check(f'Team and units: the {len(gone)} units in no other team are deleted, the rest stay, other teams unchanged (window + file)',
          gone and du == du0 - gone and dt == {k: v for k, v in dt0.items() if k != 'Team A'} and s.get('units') == len(du), (s.get('units'), sorted(du), dt, s.get('msg')))
    click('mu:all', 2); teams_check('Delete team, Team and units')
    # ---- Team only on the first team left: every unit stays
    du0, dt0 = du, dt; first = json.load(open(LIB))['teams'][0]['name']
    click('mu:team0'); click('mu:delete_team'); click('mu:dlg_only', 2)
    s = mu(); _, du, dt = disk()
    check('Team only: the team goes, every unit stays (window + file)', du == du0 and dt == {k: v for k, v in dt0.items() if k != first} and s.get('units') == len(du), (s.get('units'), sorted(du), dt))
    click('mu:all', 2); teams_check('Delete team, Team only')
    # ---- plain unit Delete: tick the first 2 cards, Delete, confirm
    cl = [c['id'] for c in (mu().get('card_list') or [])][:2]
    click('mu:tick0'); click('mu:tick1'); click('mu:delete', 2); click('notice_ok', 2)
    s = mu(); _, du2, _ = disk()
    check('unit Delete: the 2 ticked units are deleted, the count drops, the file is saved', len(cl) == 2 and du2 == du - set(cl) and s.get('units') == len(du2), (s.get('units'), sorted(du2), s.get('msg')))
    teams_check('Delete')
    n_before = len(du2)
    # ---- World units: the real save copy (game saves), read only
    gi = next(i for i, w in enumerate(worlds) if w.split('|')[2] == sv)
    click(f'mu:world{gi}', 4)
    s = mu(); shot('world_en')
    wunits = s.get('wunits') or []
    check('a world shows its save\'s units as cards, with no teams', s.get('sel_world') == gi and len(wunits) >= 2 and s.get('cards', 0) >= 2
          and not any(c.get('teams') for c in s.get('card_list') or []), (s.get('sel_world'), len(wunits), s.get('cards')))
    check('a world\'s units are read only (no Save as team / Rename / Delete / Delete team; Save snapshot shown)',
          not shown('mu:save_team') and not shown('mu:rename') and not shown('mu:delete') and not shown('mu:delete_team') and shown('mu:snapshot'))
    # ---- Save snapshot of the first 2 units
    click('mu:tick0'); click('mu:tick1'); s = mu()
    check('2 world units ticked', s.get('wticked') == 2, s.get('wticked'))
    click('mu:snapshot', 3)
    s = mu(); d, du, _ = disk()
    new = [u for u in d['units'] if u['id'] not in ids]
    src = [u.get('source') or {} for u in new]
    hdr = next(w for w in worlds if w.split('|')[2] == sv).split('|')
    check('Save snapshot: 2 new My Units units, each with the ticked save unit, world, save, save time and snapshot time',
          s.get('units') == n_before + 2 and len(new) == 2 and sorted(x.get('unit') for x in src) == sorted(wunits[:2])
          and all(x.get('kind') == 'snapshot' and x.get('world') == hdr[0] and x.get('save') == sv and x.get('save_time') == hdr[3] and x.get('taken', 0) > 0 for x in src),
          (s.get('units'), src, s.get('msg')))
    # ---- the card shows the source line
    click('mu:all', 2); s = mu(); pp = max(1, s.get('per_page', 1)); last = (s.get('shown', 1) - 1) // pp
    for _ in range(last):                                         # the snapshot units are the last ones: the last page
        click('mu:next', 1)
    s = mu(); shot('src_en')
    t = req('rect', target='mu:src0').get('text') or ''
    want = sum(1 for u in d['units'][last * pp:(last + 1) * pp] if u.get('source'))   # the snapshot units on that page
    check('a snapshot unit\'s card shows its source on one line (world + save time)', want >= 1 and s.get('src_lines') == want and hdr[0] in t and hdr[3] in t,
          (s.get('src_lines'), want, t))
    ov = req('call', fn='mu_test_overflow', s='')
    check('cards and source lines inside their boxes (en)', ov.get('rc') == 0, (ov.get('out') or '')[-300:])
    # ---- the modded saves' world shows too
    mi = next(i for i, w in enumerate(worlds) if 'MoonlightBrigade' in w.split('|')[2])
    click(f'mu:world{mi}', 4); s = mu(); shot('world_mod_en')
    check('the modded saves\' world shows its units', s.get('sel_world') == mi and len(s.get('wunits') or []) >= 1, (s.get('sel_world'), len(s.get('wunits') or [])))


def main():
    o0 = fstate(OWNER_LIB); s0 = fstate(SAVE)
    g = threading.Thread(target=HP.guard, daemon=True); g.start()
    res = {}
    try:
        ids = make_library()
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
        HP.start(X, 'http://127.0.0.1:9', {'DSL_UNITS_TEST': DRV, 'DSL_CRASHREP_TEST': crash, 'DSMP_UNITS_LIB': LIB, 'DSML_GAME_SAVES': GAME_SAVES})
        lrig.wait_for(X, lambda s: s.get('scene') == 1 and not s.get('busy'), 'title', timeout=1500); time.sleep(6)
        ok = click('title:' + (by_index(column(), COL_LAB) or 'Units Laboratory'), 3) and click('home:My Units', 4)
        check('Units Laboratory > My Units opened', ok and mu().get('open') == 1)
        run(ids)
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
