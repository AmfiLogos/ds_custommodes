"""Test Fights keeps every choice between tests and restarts (owner 2026-11-01; fights/tf_ui.c pref_kv). A game test that
opens Test Fights starts from the defaults: it calls fresh_settings(<copy data folder>, <out>) before the copy starts, so a
choice kept by an earlier run on the same copy (a mode, a squad, a time of day) never changes what the test sees. The kept
files are set aside into <out> (never deleted)."""
import os

FILES = ('settings.json', 'fights_gallery.txt')


def fresh_settings(data_dir, out):
    moved = []
    for f in FILES:
        p = os.path.join(data_dir, 'Mods', 'custom-modes', f)
        if os.path.exists(p):
            os.makedirs(out, exist_ok=True)
            os.replace(p, os.path.join(out, 'old_' + f)); moved.append(f)
    return moved
