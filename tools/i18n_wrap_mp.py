#!/usr/bin/env python3
"""i18n_wrap_mp.py - the mechanical part of the i18n pass on the multiplayer part of Custom Modes (fe/, pvp/).

  python3 tools/i18n_wrap_mp.py [--check]      (from the repo root; --check: only print what would change)

Re-runnable on a newer main: it only adds what is missing, never wraps twice.
 1. tmp_set( -> cmt_set( in the window files (fe/cm_mp_text.h: no cut UTF-8 character at the end of a label).
 2. Wraps player text literals in TR(...) where they are passed to a text sink: cmt_set / tip_try / TIPW / few_set /
    few_label (text args), mk_button (label), center / ui_notice, and the format of snprintf(buf, cap, "...").
    A literal counts as player text when it holds a word (3+ letters) once printf conversions and <tags> are taken out,
    and does not look like a key, path or JSON. Literals already in TR / TRN / TR_NOOP stay as they are.
The hard cases (plurals, pieces glued together, text compared by value, game words) are done by hand in the same
branch; see NOTES_i18n.md.
"""
import re
import sys

FILES = ["pvp/pvp_ui.c", "fe/fe_menu.c", "pvp/pvp_game.c", "fe/fe_game.c", "fe/fe_hub.c"]
RENAME_TMP_SET = {"pvp/pvp_ui.c", "fe/fe_menu.c"}
# sink name -> argument indexes whose literals are player text ("from" = this index and every later one)
SINKS = {
    "cmt_set": [1], "tip_try": [1, 2], "TIPW": [1, 2], "few_set": [1], "few_label": [1],
    "mk_button": [10], "center": [0], "ui_notice": [0], "snprintf": [2],
}
SKIP_LINE = re.compile(r"\b(rig_log|say|bput|bstr|jstr|jnum|fe_log|FE_LOG|strcmp|strncmp|strstr|fopen|getenv)\s*\(")
LIT = re.compile(r'"(?:[^"\\\n]|\\.)*"')


def texty(lit):
    s = lit[1:-1]
    if any(x in s for x in ("TEXTKEY", '\\"', ".json", "/%s", "%s/", "MPFE_", "MPPvP_", "DS_", "->")):
        return False
    if re.fullmatch(r"%s\.[a-z]+", s):                 # "%s.tmp": a file name
        return False
    t = re.sub(r"%[-+ #0-9.*]*[hlzjt]*[a-zA-Z%]", " ", s)
    t = re.sub(r"<[^<>]{0,64}>", " ", t)
    t = re.sub(r"\\x[0-9A-Fa-f]{2}", " ", t)
    return re.search(r"[A-Za-z]{3,}", t) is not None or t.strip() in ("OK", "XP")


def split_args(src, i):
    """src[i] == '(' -> (end index of ')', [(start, end) of each arg])"""
    depth, j, args, a0, n = 0, i, [], i + 1, len(src)
    while j < n:
        c = src[j]
        if c == '"' or c == "'":
            q = c
            j += 1
            while j < n and src[j] != q:
                j += 2 if src[j] == "\\" else 1
        elif c in "([{":
            depth += 1
        elif c in ")]}":
            depth -= 1
            if depth == 0:
                args.append((a0, j))
                return j, args
        elif c == "," and depth == 1:
            args.append((a0, j))
            a0 = j + 1
        j += 1
    return None, []


def wrap_arg(src, a, b, edits):
    seg = src[a:b]
    k = 0
    while True:
        m = LIT.search(seg, k)
        if not m:
            return
        s, e = m.start(), m.end()
        while True:                                   # adjacent literals "a" "b" are one
            m2 = re.match(r'\s*("(?:[^"\\\n]|\\.)*")', seg[e:])
            if not m2:
                break
            e += m2.end()
        k = e
        before = seg[:s].rstrip()
        if re.search(r"\b(TR|TRN|TR_NOOP)\s*\($", before) or re.search(r"\b(TR|TRN)\s*\([^()]*,\s*$", before):
            continue
        lits = LIT.findall(seg[s:e])
        if not texty('"' + "".join(x[1:-1] for x in lits) + '"'):
            continue
        edits.append((a + s, a + e))


def process(path, check):
    src = open(path, encoding="utf-8").read()
    orig = src
    if path in RENAME_TMP_SET:
        src = re.sub(r"(?<![\w.>])tmp_set\(", "cmt_set(", src)
    edits = []
    for name, idx in SINKS.items():
        for m in re.finditer(r"(?<![\w.>])" + name + r"\s*\(", src):
            ls = src.rfind("\n", 0, m.start()) + 1
            le = src.find("\n", m.start())
            line = src[ls:le]
            if line.lstrip().startswith(("/*", "*", "//", "#define")) or "static void cmt_set" in line:
                continue
            pre = src[ls:m.start()]
            if SKIP_LINE.search(pre + name + "(") and name != "snprintf":
                continue
            end, args = split_args(src, m.end() - 1)
            if end is None:
                continue
            if name == "snprintf" and len(args) >= 3:
                # snprintf into a JSON / path / key buffer: never; player text has a texty format
                fmt = src[args[2][0]:args[2][1]].strip()
                if not fmt.startswith('"'):
                    continue
            if name == "snprintf" and len(args) == 3 and "%" not in src[args[2][0]:args[2][1]]:
                n0 = len(edits)                               # no conversion: snprintf(b, n, "%s", TR("...")), so a
                wrap_arg(src, args[2][0], args[2][1], edits)  # translation is never read as a format
                if len(edits) > n0:
                    edits[-1] = edits[-1] + ("%s",)
                continue
            for i in idx:
                if i < len(args):
                    wrap_arg(src, args[i][0], args[i][1], edits)
    edits = sorted(set(edits))
    out, last = [], 0
    for ed in edits:
        s, e = ed[0], ed[1]
        if s < last:
            continue
        out.append(src[last:s])
        out.append(('"%s", ' if len(ed) > 2 else "") + "TR(" + src[s:e] + ")")
        last = e
    out.append(src[last:])
    new = "".join(out)
    if new != orig and '#include "fe/cm_mp_text.h"' not in new:   # after the first local include
        m = re.search(r'^#include "[^"]+"\n', new, re.M)
        if m:
            new = new[:m.end()] + '#include "fe/cm_mp_text.h"\n' + new[m.end():]
    if new != orig:
        print(f"{path}: {len(edits)} literal(s) wrapped" + (" (tmp_set -> cmt_set)" if path in RENAME_TMP_SET else ""))
        if not check:
            open(path, "w", encoding="utf-8").write(new)


def main():
    check = "--check" in sys.argv
    for f in FILES:
        process(f, check)


if __name__ == "__main__":
    main()
