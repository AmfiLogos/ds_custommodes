#!/usr/bin/env python3
"""Units Laboratory draws its texts like the game's own menus (owner 2026-10-24: "Font inside of the units lab are not the
same as the rest of the menus"; home/cm_look.c). One hidden loader test copy (ULK: background-only, small window, muted,
notch fix off; never the owner's game, saves or screen) with multiplayer + custom-modes + the test module font-probe.
The game's own texts are the reference, read in the same run and language: the title column's labels, the Options
window's labels (body), section heads and title. Then the Units Laboratory page in the title column and its windows
My Units, Unit Builder, Wiki Build Gallery and Test Fights.
Fails when (the breaks this catches):
  - a Units Laboratory text has another font or material than the game uses for that role (body text, buttons, check
    boxes, value rows: the Options labels' font + material; titles under 26: the section heads'; window titles: the
    Options title's). Text fields keep the kit's look (the game's name fields are plain too) and are not checked.
  - a button of the Units Laboratory page has another font, material or size than the stock title column, or its label
    does not fit its button.
  - a page does not open.
On the 0.1.13 code it fails (plain material, labels 18).
  look_test.py                     env: HM_BUILD = a loader build folder (dsload.dylib with the dev key, mods/multiplayer,
                                        mods/custom-modes: loader/build.sh of ds_modloader with CM_PIN = the commit to test)
                                        LD_ROOT = a ds_modloader checkout (its loader/test helpers; default ~/ds_launcher)
                                        LANGTYPE = the game's language (3 de, 4 ru, 7 ja; default en)
Output: LOOK_OUT (default <HM_BUILD>/../test_env/look/<time>): results.json, fonts_*.jsonl, frames in drv/cap/."""
import json, os, shutil, subprocess, sys, threading, time, traceback, zipfile

HERE = os.path.dirname(os.path.abspath(__file__))
LD = os.path.expanduser(os.environ.get('LD_ROOT', '~/ds_launcher'))
sys.path.insert(0, os.path.join(LD, 'loader', 'test'))
import hostmods_pair as HP  # noqa: E402  (copies, kits, start/stop, owner screen guard)
from hostmods_pair import lrig, log, check  # noqa: E402

X = 'ULK'; HP.H = HP.G = X
OUT = os.environ.get('LOOK_OUT') or os.path.join(os.path.dirname(HP.BUILD), 'test_env', 'look', time.strftime('%Y%m%d-%H%M%S'))
os.makedirs(OUT, exist_ok=True)
DRV = os.path.join(OUT, 'drv'); os.makedirs(os.path.join(DRV, 'cap'), exist_ok=True)
PAGES = (('My Units', 'mu:close'), ('Unit Builder', 'bld:cancel'), ('Wiki Build Gallery', None), ('Test Fights', None))
COL = {'Settings': 38, 'Units Laboratory': 5}                # child index in the title column (every language)


def probe_pkg():
    """the test module as a mod zip (built here, never shipped)"""
    d = os.path.join(OUT, 'pkg', 'font-probe'); os.makedirs(d, exist_ok=True)
    subprocess.run(['nice', '-n', '19', 'clang', '-arch', 'arm64', '-O2', '-Wall', '-Wno-unused-function', '-I' + os.path.join(LD, 'loader', 'sdk'),
                    '-dynamiclib', '-framework', 'CoreFoundation', '-framework', 'CoreGraphics', '-lobjc', '-install_name', '@rpath/font-probe.dylib',
                    '-o', os.path.join(d, 'font-probe.dylib'), os.path.join(HERE, 'font_probe', 'font_probe.c')], check=True)
    subprocess.run(['codesign', '-f', '-s', '-', os.path.join(d, 'font-probe.dylib')], capture_output=True)
    shutil.copy2(os.path.join(HERE, 'font_probe', 'mod.json'), os.path.join(d, 'mod.json'))
    return zip_dir(d, 'font-probe')


def zip_dir(src, mid):
    zp = os.path.join(OUT, 'pkg', f'{mid}.zip'); os.makedirs(os.path.dirname(zp), exist_ok=True)
    with zipfile.ZipFile(zp, 'w', zipfile.ZIP_DEFLATED) as zf:
        for r, _, fs in os.walk(src):
            for f in sorted(fs):
                if not f.startswith('.'):
                    zf.write(os.path.join(r, f), os.path.join(mid, os.path.relpath(os.path.join(r, f), src)))
    return zp


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


def texts(name, wait=3.0):
    """every text active on screen now (font-probe "fonts"), and a frame of the game"""
    time.sleep(wait)
    r = req('fonts', name=name, all=0); req('capture', name=name)
    p = os.path.join(DRV, f'fonts_{name}.jsonl')
    t = [json.loads(l) for l in open(p)] if os.path.exists(p) else []
    log('texts', name, r.get('rc'), len(t))
    return t


def click(t, wait=2.0):
    r = req('click', target=t); time.sleep(wait)
    log('click', t, r.get('rc'), r.get('text'))
    return r.get('rc') == 0


def column():
    """the title column: [label, shown, child index]"""
    return [c.split('|') for c in (req('state').get('title_column') or '').split(';') if c]


def by_index(rows, i):
    return next((r[0] for r in rows if len(r) > 2 and r[2] == str(i) and r[1] == '1'), None)


def role_ref(opt):
    """the game's font + material per role from its Options window: body (EasyRead labels), head (VeryDecorative section
       heads), wtitle (the window title)"""
    ref = {}
    for t in opt:
        if 'GameOptionUI' not in t['path'] or t['das_style'] < 0:
            continue
        k = 'body' if t['das_style'] == 0 and t['das_mat'] == 1 else 'head' if t['das_style'] == 2 and t['das_mat'] == 1 \
            else 'wtitle' if t['das_style'] == 2 and t['das_mat'] == 0 else None
        if k and k not in ref:
            ref[k] = (t['font'], t['mat'])
    return ref


def role_of(t, ref):
    if t['font'] == ref['body'][0]:
        return 'body'
    if t['font'] == ref['head'][0]:
        return 'head' if t['size'] < 26 else 'wtitle'
    return None


def main():
    g = threading.Thread(target=HP.guard, daemon=True); g.start()
    res = {}
    try:
        HP.make_copy(X)
        pk = {'custom-modes': zip_dir(os.path.join(HP.BUILD, 'mods', 'custom-modes'), 'custom-modes'), 'font-probe': probe_pkg()}
        HP.kit(X, pk, ['custom-modes', 'font-probe'], ['multiplayer', 'custom-modes', 'font-probe'])
        lang = os.environ.get('LANGTYPE')
        if lang:
            prep = lrig.prepare_data
            def prep_lang(x):
                prep(x); us = os.path.join(lrig.idir(x), 'data', 'UserSetting.json'); s = json.load(open(us))
                s['GeneralSettingData']['SaveCurrentLanguageType'] = int(lang); json.dump(s, open(us, 'w'), indent=1)
            lrig.prepare_data = prep_lang
        crash = os.path.join(OUT, 'crash'); os.makedirs(crash, exist_ok=True); open(os.path.join(crash, 'answer'), 'w').write('no')
        HP.room_for(1)
        HP.start(X, 'http://127.0.0.1:9', {'DSL_UNITS_TEST': DRV, 'DSL_CRASHREP_TEST': crash})
        lrig.wait_for(X, lambda s: s.get('scene') == 1 and not s.get('busy'), 'title', timeout=1500); time.sleep(6)
        rows = column()
        title = texts('title')
        stock = next((t for t in title if 'SubUI_TitleMenuButton_LoadCampaign/Txt_Name' in t['path']), None)
        check('the stock title column label is there (reference)', stock is not None, stock and (stock['font'], stock['mat'], stock['size']))
        ok = click('title:' + (by_index(rows, COL['Settings']) or 'Settings'), 3)
        ref = role_ref(texts('options'))
        check("the game's Options window: body, section head and title look found (reference)", ok and len(ref) == 3, ref)
        req('key', code=53); time.sleep(1.5)
        if stock is None or len(ref) < 3:
            raise RuntimeError('no reference')
        seen = {(t['path'], t['text']) for t in title}               # the loader's own corner texts stay out of the check
        ok = click('title:' + (by_index(column(), COL['Units Laboratory']) or 'Units Laboratory'), 3)
        home = [t for t in texts('home') if 'Credit(Clone)(Clone)/Txt_Name' in t['path'] and (t['path'], t['text']) not in seen]
        def home_bad(t):
            if (t['font'], t['mat']) != (stock['font'], stock['mat']) or t['size'] > stock['size'] + 0.5:
                return 'look'
            if t['ovf'] == 1:
                return 'does not fit'
            if t['size'] < stock['size'] - 0.5 and t['pw'] * stock['size'] / t['size'] <= t['rw'] + 1:   # smaller, yet the stock size fits
                return 'smaller than the stock column'
            return None
        bad = [(t['text'], home_bad(t), t['font'], t['mat'], t['size'], t['pw'], t['rw']) for t in home if home_bad(t)]
        check('Units Laboratory page: 5 buttons with the stock title column look (font, material, size; label fits)',
              ok and len(home) == 5 and not bad, bad[:5] or len(home))
        info = {'stock': stock, 'ref': ref, 'home_sizes': [(t['text'], t['size']) for t in home]}
        for page, close in PAGES:
            ok = click('home:' + page, 4)
            name = page.lower().replace(' ', '_')
            lab = [t for t in texts(name, 6 if 'Gallery' in page else 4) if 'dsl_ui_layer' in t['path'] and (t['path'], t['text']) not in seen
                   and 'Text Area' not in t['path']]
            bad = []
            for t in lab:
                r = role_of(t, ref)
                if r is None or t['mat'] != ref[r][1]:
                    bad.append((t['text'][:30], t['font'], t['mat'], t['size'], r))
            check(f"{page}: opened, every text in the game's look for its role", ok and len(lab) >= 10 and not bad, bad[:6] or len(lab))
            if close:
                click(close, 2)
            else:
                req('call', fn='cmf_test', s='{"cmd":"close"}'); time.sleep(2)
        res['info'] = info
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
