#!/bin/zsh
# Guard (lane nvmyunits 2026-11-02, NOTES.md "RULE: JSON text on screen"): ONE JSON string decoder, fe/fe_json.c jstr_utf8.
# Six hand-made copies had drifted (no surrogate pairs, BMP only, escapes shown as spaces in My Units). This fails when a
# C file decodes \u escapes by hand again. Not checked: fe/fe_json.c (the decoder), QuickJS (its own JS parser),
# i18n/ds_i18n.h (the language-file parser shared with the loader kit).
# usage: units/test/one_decoder.sh   (run by `make -C units/test test`)
cd "${0:A:h}/../.." || exit 2
hits=$(git grep -n -E "== 'u'|case 'u'" -- '*.c' '*.h' ':!fe/fe_json.c' ':!fights/quickjs/' ':!i18n/ds_i18n.h')
if [[ -n $hits ]]; then
  print -r -- "FAIL one decoder: decode JSON text with fe_json.h jstr_utf8, not by hand:"
  print -r -- "$hits"
  exit 1
fi
print "one decoder: PASS"
