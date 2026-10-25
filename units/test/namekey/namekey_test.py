#!/usr/bin/env python3
"""Unit names that are text keys in the saves Units Laboratory writes, as the game shows them (owner 2026-10-25: units from a
My Units "Save snapshot" in a test fight showed "TEXTKEY_PLAYERNAME_Elf_Male_13" on the Multi-unit view cards). A save
unit never renamed keeps the game's default name key (TEXTKEY_PLAYERNAME_...); the game localizes it only when the unit's
DefaultUnitNameTextKey is the same key, else it takes the key for a typed name (EntityDisplayNameExtensions.
GetPlayerUnitDisplayName, decomp 25846389), and the unit panel and the Multi-unit view cards print it raw.

Saves, made on this Mac from the wiki's public base save (4 units with default names), no game:
  tf  Test Fights: the 4 snapshot builds (nk_tool builds = mu_save_build + wk_json, what "Save snapshot" stores) through
      the wiki's save generator in this tree's engine and glue (fights/test/sg_cli), a training-dummy fight
  pb  Bring Units: the base save with each unit's profile as this tree's save writer makes it (nk_tool pb = pb_entry)
Then one hidden loader test copy NKN (background-only, small window, muted, notch fix off; never the owner's game, saves,
library or screen) with multiplayer (test control), multi-bar and the test module namekey-probe loads each save, in
English and in German.
Fails when (the break this catches): a unit's name on the stock unit panel (one unit selected) or on its Multi-unit view
card (all 4 selected) is not the game's text of its name key in that language (TextKeyExtensions.GetText), e.g. the
raw key; or the copy shows a window / crashes.

  namekey_test.py [out]    env: NK_KIT  a loader build folder: dsload.dylib + mods_all/mac/{multiplayer,multi-bar}
                                         (default <LD_ROOT>/loader/build)
                                LD_ROOT a ds_modloader checkout (its loader/test/lrig.py; default ~/ds_launcher)
                                NK_SITE the wiki site folder: builder/ + assets/saves/ (default ~/dungeonsettlersdb/site)
                                NK_OLD  another ds_custommodes tree (e.g. `git archive` of an older commit): its saves
                                        are loaded too and reported as "old" (expected to fail on a broken tree)
                                NK_LANGS the game's languages (default "0 3" = English, German)
Output: <out> (default units/test/namekey/tmp/<time>): results.json, the copy's logs per language."""
import json, os, re, shutil, subprocess, sys, time

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(os.path.dirname(os.path.dirname(HERE)))
H = os.path.expanduser('~')
LD = os.path.expanduser(os.environ.get('LD_ROOT', '~/ds_launcher'))
sys.path.insert(0, os.path.join(LD, 'loader', 'test'))
import lrig as r  # noqa: E402  (copies, start/stop, control socket, owner screen guard)

X = 'NKN'; I = r.idir(X)
KIT = os.path.expanduser(os.environ.get('NK_KIT') or os.path.join(LD, 'loader', 'build'))
SITE = os.path.expanduser(os.environ.get('NK_SITE', '~/dungeonsettlersdb/site'))
BASE = os.path.join(SITE, 'assets', 'saves', 'base_25846389.json')
LANGS = [int(x) for x in os.environ.get('NK_LANGS', '0 3').split()]
LANG_NAME = {0: 'en', 3: 'de', 4: 'ru', 7: 'ja'}
OUT = os.path.abspath(sys.argv[1]) if len(sys.argv) > 1 else os.path.join(HERE, 'tmp', time.strftime('%Y%m%d-%H%M%S'))
checks = []


def log(*a): print(time.strftime('%H:%M:%S'), *a, flush=True)
def check(name, ok, detail=''):
    checks.append({'name': name, 'ok': bool(ok), 'detail': str(detail)[:600]}); log('PASS' if ok else 'FAIL', name, '' if ok else detail)


# ------------------------------------------------------------------ the saves (no game)
def build_tools(tree, tag):
    """nk_tool + sg_cli of one ds_custommodes tree (sg_cli's build output in a scratch ds_multiplayer root, never the shared one)"""
    b = os.path.join(OUT, 'bin'); os.makedirs(b, exist_ok=True)
    nk = os.path.join(b, 'nk_tool_' + tag)
    srcs = [os.path.join(tree, 'units', f) for f in ('mu_save.c', 'mu_worlds.c', 'myunits.c', 'pvp_wiki.c', 'pvp_build.c', 'mu_card.c')]
    srcs += [os.path.join(tree, 'fe', f) for f in ('fe_json.c', 'fe_save.c')] + [os.path.join(tree, 'units', 'test', 'dsi_impl.c')]
    subprocess.run(['nice', '-n', '19', 'cc', '-O1', '-Wall', '-Wno-unused-parameter', '-I', os.path.join(tree, 'units'), '-I', tree,
                    '-I', os.path.join(tree, 'fe'), '-o', nk, os.path.join(HERE, 'nk_tool.c')] + srcs + ['-lz', '-lm'], check=True)
    mr = os.path.join(OUT, 'mproot_' + tag); os.makedirs(mr, exist_ok=True)
    for d in ('src', 'sdk'):
        if not os.path.lexists(os.path.join(mr, d)): os.symlink(os.path.join(H, 'ds_multiplayer', d), os.path.join(mr, d))
    subprocess.run(['zsh', os.path.join(tree, 'fights', 'build.sh')], env=dict(os.environ, MP_ROOT=mr), check=True, stdout=subprocess.DEVNULL)
    return nk, os.path.join(mr, 'build_c4', 'sg_cli')


def make_saves(tree, tag):
    nk, cli = build_tools(tree, tag)
    d = os.path.join(OUT, 'saves_' + tag); os.makedirs(d, exist_ok=True)
    builds = os.path.join(d, 'builds.json')
    subprocess.run([nk, 'builds', BASE, builds], check=True, stdout=subprocess.DEVNULL)
    pb = os.path.join(d, 'NameKeyPB%s__ManualSave_0.json' % tag.capitalize())
    subprocess.run([nk, 'pb', BASE, pb], check=True, stdout=subprocess.DEVNULL)
    cases = os.path.join(d, 'cases.json')
    json.dump([{'id': 'tf', 'builds': 'snap', 'opts': {'mode': 'dps', 'combat': 'Normal', 'kits': {'basic': True}, 'team': 'Names'}}], open(cases, 'w'))
    subprocess.run([cli, os.path.join(SITE, 'builder'), os.path.join(SITE, 'assets', 'saves'), d, cases, builds], check=True, stdout=subprocess.DEVNULL)
    tf = os.path.join(d, 'NameKeyTF%s__ManualSave_0.json' % tag.capitalize())
    os.replace(os.path.join(d, 'tf.save.json'), tf)
    return [('tf', tf), ('pb', pb)]


def unit_keys(path):
    """{guid: UnitNameTextKey} of the save's clan units"""
    s = json.load(open(path)); ec = s['EntityComponentSaveData']['EntityComponents']; out = {}
    for side in (s['EntityContainerSaveData']['UnitContainerSaveData'] or {}).values():
        for g in (side or {}).get('PlayerGuidList') or []:
            p = next((c['Data'] for c in ec[g] if c['Type'] == 10), {})
            out[g] = p.get('UnitNameTextKey', '')
    return out


# ------------------------------------------------------------------ the copy
def dlog():
    try: return open(os.path.join(I, 'dsload.log'), errors='replace').read()
    except OSError: return ''
def cmd(o, timeout=60): return r.send(X, o, timeout=timeout, connect_wait=5)
def probe(line, want, limit=20):
    """one request to namekey-probe; its new lines once one contains `want`"""
    mark = len(dlog())
    p = os.path.join(I, 'namekey_probe_req.txt'); open(p + '.tmp', 'w').write(line + '\n'); os.replace(p + '.tmp', p)
    t0 = time.time()
    while time.time() - t0 < limit:
        L = [l.split('namekey-probe: ', 1)[1] for l in dlog()[mark:].splitlines() if 'namekey-probe: ' in l]
        if any(want in l for l in L): return L
        if not r.alive(r.read_pid(X)): raise RuntimeError('the test copy is gone (crash?)')
        time.sleep(0.25)
    return []


def names():
    L = probe('names', 'names done')
    return [dict(bar=m[1], active=int(m[2]), unit=m[3], text_active=int(m[4]), text=m[5])
            for m in (re.match(r'bar \[(\S+) active=(\d) unit=(\S+) text_active=(\d)\] text=(.*)$', l) for l in L) if m]


def aside(p):
    """never delete: a file / folder of the copy goes to <out>/aside/"""
    if os.path.lexists(p):
        d = os.path.join(OUT, 'aside'); os.makedirs(d, exist_ok=True)
        shutil.move(p, os.path.join(d, os.path.basename(p) + '.%d' % (time.time() * 1000 % 100000)))


def make_copy():
    app = os.path.join(I, 'DungeonSettlers.app')
    if not os.path.exists(app):                              # an APFS clone of the ready hidden copy Z (make_instance.sh)
        src = os.path.join(r.INST, 'Z', 'DungeonSettlers.app')
        os.makedirs(I, exist_ok=True)
        subprocess.run(['cp', '-c', '-R', src, app], check=True)
        pl = os.path.join(app, 'Contents', 'Info.plist')
        for k, t, v in (('CFBundleIdentifier', '-string', 'com.CanOpener.Dungeon-Settlers.mp' + X), ('CFBundleName', '-string', 'DS rig ' + X),
                        ('LSBackgroundOnly', '-bool', 'YES'), ('LSUIElement', '-bool', 'YES')):
            subprocess.run(['plutil', '-replace', k, t, v, pl], check=True)
        subprocess.run(['codesign', '-f', '-s', '-', os.path.join(app, 'Contents', 'MacOS', 'Dungeon Settlers')], capture_output=True)
    r.require_background_only(X)


def mods():
    md = os.path.join(OUT, 'mods')
    if os.path.exists(md): aside(md)
    os.makedirs(md)
    for m in ('multiplayer', 'multi-bar'): shutil.copytree(os.path.join(KIT, 'mods_all', 'mac', m), os.path.join(md, m))
    pd = os.path.join(md, 'namekey-probe'); os.makedirs(pd)
    subprocess.run(['clang', '-arch', 'arm64', '-O2', '-Wall', '-I', os.path.join(LD, 'loader', 'sdk'), '-dynamiclib', '-install_name',
                    '@rpath/namekey-probe.dylib', '-o', os.path.join(pd, 'namekey-probe.dylib'), os.path.join(HERE, 'namekey_probe.c')], check=True)
    subprocess.run(['codesign', '-f', '-s', '-', os.path.join(pd, 'namekey-probe.dylib')], capture_output=True)
    json.dump({'id': 'namekey-probe', 'name': 'namekey probe (test)', 'version': '0.0.1', 'kind': 'code', 'api': 2, 'scope': 'client',
               'game_builds': ['25846389'], 'platforms': {'mac-arm64': {'entry': 'namekey-probe.dylib'}}}, open(os.path.join(pd, 'mod.json'), 'w'), indent=1)
    en = ['multiplayer', 'multi-bar', 'namekey-probe']
    prof = os.path.join(OUT, 'profile.json'); json.dump({'enabled': en, 'order': en}, open(prof, 'w'))
    return md, prof


def up(lang, md, prof):
    if r.alive(r.read_pid(X)): raise SystemExit('copy %s already running' % X)
    for p in ('mp.json', 'dsload.log', 'player.log', 'namekey_probe_req.txt', os.path.join('data', 'Saves'), os.path.join('data', 'SaveHeaderCache.json'),
              os.path.join('data', 'Mods', 'worlds.json'), os.path.join('data', 'Mods', 'save_backups')):
        aside(os.path.join(I, p))
    os.makedirs(os.path.join(I, 'data', 'Saves'), exist_ok=True)
    prep = r.prepare_data
    def prep_lang(x):                                        # the game's language (UserSetting.json, regenerated every start)
        prep(x); us = os.path.join(r.idir(x), 'data', 'UserSetting.json'); s = json.load(open(us))
        s['GeneralSettingData']['SaveCurrentLanguageType'] = lang; json.dump(s, open(us, 'w'), indent=1)
    r.prepare_data = prep_lang
    os.environ.update({'LRIG_DYLIB': os.path.join(KIT, 'dsload.dylib'), 'DSL_MODS_DIR': md, 'DSL_PROFILE': prof,
                       'DSML_DATA': os.path.join(I, 'data'), 'DSL_CLOUD': '0', 'DSL_NO_ALERT': '1', 'DSRIG_BG': '1'})
    os.environ.pop('DSML_SESSION', None)
    try:
        r.start(X, window=True)
    finally:
        r.prepare_data = prep
    r.wait_for(X, lambda s: s.get('scene') == 1 and not s.get('busy'), 'title', timeout=900)
    L = dlog()
    check('%s: multi-bar ready, probe ready' % LANG_NAME.get(lang, lang), 'multi-bar: ready' in L and 'namekey-probe: ready' in L,
          [l for l in L.splitlines() if 'multi-bar' in l or 'namekey-probe' in l][-4:])


def load(path):
    """load a save from the title or from a world: first the load itself (out of the world, busy or loading), then the world"""
    log('load', os.path.basename(path), cmd({'cmd': 'load_save', 'file': path}))
    t0 = time.time()
    while True:
        s = cmd({'cmd': 'status'}, 10)
        if s.get('scene') != 2 or s.get('busy') or (s.get('map_loading') or {}).get('is_loading'): break
        if time.time() - t0 > 90: raise RuntimeError('the load of %s never started: %s' % (os.path.basename(path), str(s)[:300]))
        time.sleep(0.3)
    r.wait_for(X, lambda s: s.get('scene') == 2 and not s.get('busy') and s.get('tick', -1) > 0, 'world', timeout=400, every=2)
    time.sleep(3)


def check_save(lang, tag, kind, path):
    """stock unit panel: each unit alone; Multi-unit view: all 4. Every name = GetText(its key) in this language."""
    ln = LANG_NAME.get(lang, str(lang)); keys = unit_keys(path); what = '%s %s %s' % (ln, tag, kind)
    want = {}
    for g, k in keys.items():
        t = next((l.split(' = ', 1)[1] for l in probe('text ' + k, 'text %s = ' % k) if l.startswith('text %s = ' % k)), None)
        want[g] = t
    check('%s: the units have default name keys, the game has a text for each' % what,
          len(keys) == 4 and all(k.startswith('TEXTKEY_PLAYERNAME_') for k in keys.values()) and all(t and not t.startswith('TEXTKEY_') for t in want.values()),
          {keys[g]: want[g] for g in keys})
    panel = {}
    for i in range(4):
        probe('sel %d' % i, 'sel %d:' % i); time.sleep(1.0)        # the panel draws the new unit on a later frame
        t0 = time.time()
        while time.time() - t0 < 8:
            bars = names(); stock = next((b for b in bars if not b['bar'].startswith('QuickSlotUI_Multi') and b['active']), None)
            if stock and stock['unit'] in keys and stock['unit'] not in panel and stock['text']: panel[stock['unit']] = stock['text']; break
            time.sleep(0.7)
    check('%s: stock unit panel shows each unit\'s name' % what, len(panel) == 4 and all(panel[g] == want[g] for g in panel),
          {keys.get(g, g): panel[g] for g in panel})
    probe('sel 0', 'sel 0:'); time.sleep(1)
    for i in (1, 2, 3): probe('add %d' % i, 'add %d:' % i); time.sleep(0.8)
    cards = []
    t0 = time.time()
    while time.time() - t0 < 10:
        cards = [b for b in names() if b['bar'].startswith('QuickSlotUI_Multi') and b['active']]
        if len(cards) == 4: break
        time.sleep(0.7)
    check('%s: Multi-unit view cards show each unit\'s name' % what,
          len(cards) == 4 and sorted(c['unit'] for c in cards) == sorted(keys) and all(c['text'] == want.get(c['unit']) for c in cards),
          [(keys.get(c['unit'], c['unit']), c['text']) for c in cards])
    return {'want': {keys[g]: want[g] for g in keys}, 'panel': {keys.get(g, g): panel[g] for g in panel},
            'cards': {keys.get(c['unit'], c['unit']): c['text'] for c in cards}}


def lease():
    lane = 'namekey-%d' % os.getpid(); t0 = time.time()
    while time.time() - t0 < 1800:
        p = subprocess.run([sys.executable, f'{H}/ds_multiplayer/rig/rig.py', 'lease', lane, '--pid', str(os.getpid())], capture_output=True, text=True)
        if p.returncode == 0 and p.stdout.strip(): return lane, p.stdout.strip()
        time.sleep(20)
    raise SystemExit('no rig lease in 30 min')


def main():
    os.makedirs(OUT, exist_ok=True)
    sets = [('new', REPO)] + ([('old', os.path.expanduser(os.environ['NK_OLD']))] if os.environ.get('NK_OLD') else [])
    saves = [(tag, kind, p) for tag, tree in sets for kind, p in make_saves(tree, tag)]
    log('saves:', [os.path.basename(p) for _, _, p in saves])
    make_copy(); md, prof = mods()
    r.cap_wait('namekey')
    lane, pair = lease(); log('lease', pair)
    S = {}
    gp = os.path.join(I, 'guard.log'); g0 = os.path.getsize(gp) if os.path.exists(gp) else 0
    try:
        # one start per save: a load from a world (the 3rd one) once stayed queued for good; from the title it always runs
        for lang in LANGS:
            for tag, kind, p in saves:
                what = '%s %s %s' % (LANG_NAME.get(lang, lang), tag, kind)
                try:
                    up(lang, md, prof); load(p); S[what] = check_save(lang, tag, kind, p)
                except Exception as e:
                    check('%s: run finished' % what, False, repr(e))
                finally:
                    try:
                        if r.alive(r.read_pid(X)): r.stop(X)
                    except Exception as e: log('stop:', e)
                    for f in ('dsload.log', 'player.log'):
                        if os.path.exists(os.path.join(I, f)): shutil.copy2(os.path.join(I, f), os.path.join(OUT, '%s_%s' % (what.replace(' ', '_'), f)))
    finally:
        g = open(gp, errors='replace').read()[g0:] if os.path.exists(gp) else ''
        check('guard: never a visible window, never frontmost', 'GUARD KILLED' not in g, g[-300:])
        subprocess.run([sys.executable, f'{H}/ds_multiplayer/rig/rig.py', 'release', pair, lane], capture_output=True)
        S['checks'] = checks; json.dump(S, open(os.path.join(OUT, 'results.json'), 'w'), indent=1, ensure_ascii=False)
    new = [c for c in checks if ' old ' not in c['name']]
    fails = [c for c in new if not c['ok']]
    old = [c for c in checks if ' old ' in c['name']]
    print('\n%d PASS %d FAIL%s; out %s' % (len(new) - len(fails), len(fails),
          ' (old tree: %d PASS %d FAIL)' % (sum(c['ok'] for c in old), sum(not c['ok'] for c in old)) if old else '', OUT))
    sys.exit(1 if fails else 0)


if __name__ == '__main__':
    main()
