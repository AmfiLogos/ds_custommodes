#!/usr/bin/env python3
"""hook_shared_guard.py - fail when a Units Laboratory hook sits on code that other methods share.

Lead task 26 (2026-11-02): tf_dps.c hooked DefaultMode.ActivateMode, an empty method. On Windows its code is the shared
'ret 0' stub of GameAssembly.dll (8695 other methods start at the same address), so the hook would run for every one
of them with an unrelated self. A hook target must have its own code on BOTH platforms.

The list comes from the loader repo's tools/hooks/hooklist.py (sources + the Mac / Windows dumps of the supported game
build). This script runs it over THIS working tree (tracked and new files, no build output) and fails when a
custom-modes hook has shared_with > 0 on any platform, unless the method is in SHARED_SAFE below (only for a hook whose
body never reads self or its arguments, with the reason).

Usage: python3 tools/hook_shared_guard.py [--loader ~/ds_launcher] [--all]   (--all also lists other mods' hits)
"""
import argparse, json, os, shutil, subprocess, sys, tempfile

SHARED_SAFE = {}   # "Namespace.Class$$Method": "why the body is safe on shared code"

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--loader", default=os.path.expanduser("~/ds_launcher"))
    ap.add_argument("--all", action="store_true")
    a = ap.parse_args()
    repo = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    tool = os.path.join(a.loader, "tools", "hooks", "hooklist.py")
    if not os.path.exists(tool):
        sys.exit(f"hook_shared_guard: no hooklist at {tool}")
    files = subprocess.run(["git", "-C", repo, "ls-files", "-co", "--exclude-standard"], capture_output=True, text=True, check=True).stdout.split("\n")
    tmp = tempfile.mkdtemp(prefix="cm_hooks_")
    tree, out = os.path.join(tmp, "tree"), os.path.join(tmp, "out")
    for f in files:
        if not f or f.startswith("build/") or not os.path.isfile(os.path.join(repo, f)):
            continue
        os.makedirs(os.path.dirname(os.path.join(tree, f)), exist_ok=True)
        shutil.copy2(os.path.join(repo, f), os.path.join(tree, f))
    r = subprocess.run([sys.executable, tool, "gen", "--cm", tree, "--out", out], cwd=a.loader, capture_output=True, text=True)
    if r.returncode:
        sys.exit("hook_shared_guard: hooklist gen failed:\n" + (r.stdout + r.stderr)[-2000:])
    d = json.load(open(os.path.join(out, "hooks.json")))
    bad, other, n = [], [], 0
    for h in d["hooks"]:
        sw = {p: len(v.get("shared_with") or []) for p, v in (h.get("platforms") or {}).items()}
        if h.get("owner") == "custom-modes":
            n += 1
        if not any(sw.values()):
            continue
        row = f"{h.get('owner')}: {h['method']} shared_with {sw}"
        if h.get("owner") == "custom-modes" and h["method"] not in SHARED_SAFE:
            bad.append(row)
        else:
            other.append(row)
    if a.all:
        for x in other:
            print("other mod:", x)
    for x in bad:
        print("FAIL", x)
    print(f"hook_shared_guard: {n} custom-modes hooks, {len(bad)} on shared code")
    sys.exit(1 if bad else 0)

if __name__ == "__main__":
    main()
