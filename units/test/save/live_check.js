// "Open in Wiki Builder" after a wiki data update (lead task 9, units/wk_export.c wkx_live_load / wkx_encode_live).
// The wiki's builder tables change with game updates and data fixes; the export re-maps every index BY KEY to the tables
// the wiki serves at the click. This guard makes changed copies of the real codec (gallery_codec.js) and checks that the
// wiki, reading the export code with the CHANGED tables, gets the same units as with today's tables.
//   node live_check.js make CODEC OUTDIR       writes OUTDIR/codec_shift.js and OUTDIR/codec_drop.js
//   node live_check.js check CODEC CHANGED LIVE.jsonl shift|drop|old
// shift: new keys sort in front of the old ones in every list (every index moves), the slots, the major stats, the
//        talent tiers, every item's bonuses and every portrait pool are re-ordered. The units must come back identical,
//        with the wiki's fingerprint (no "made with other data" warning) and nothing left out.
// drop:  the most used item of the export and one trait are gone from the wiki. The units must come back identical minus
//        those, and the lines that had them say so (dropped > 0).
// old:   negative control: the code written against OUR tables (code0) read with the shifted tables must NOT pass.
const fs = require('fs'), path = require('path'), vm = require('vm');
function loadCodec(file) {
  const m = { exports: {} };
  vm.runInNewContext(fs.readFileSync(file, 'utf8'), { module: m, exports: m.exports, Promise, console, Uint8Array, TextDecoder, TextEncoder, atob: s => Buffer.from(s, 'base64').toString('binary'), btoa: s => Buffer.from(s, 'binary').toString('base64') }, { filename: file });
  return m.exports;
}
function codecD(text) { const m = text.match(/var D = ([\s\S]*?);\nvar /); return { D: JSON.parse(m[1]), at: m.index + 8, len: m[1].length }; }
function withD(text, D) { const c = codecD(text); return text.slice(0, c.at) + JSON.stringify(D) + text.slice(c.at + c.len); }
// key order does not matter (the shifted tables write the majors in another order): compare with sorted keys
const canon = v => Array.isArray(v) ? '[' + v.map(canon).join(',') + ']' : v && typeof v === 'object' ? '{' + Object.keys(v).sort().map(k => JSON.stringify(k) + ':' + canon(v[k])).join(',') + '}' : JSON.stringify(v);
if (process.argv[2] === 'make') {
  const [codec, out] = process.argv.slice(3), text = fs.readFileSync(codec, 'utf8');
  // shift
  const D = codecD(text).D;
  D.items = Object.assign({ ITEM_AAAShift: JSON.parse(JSON.stringify(Object.values(D.items)[0])) }, D.items);
  D.skills = Object.assign({ AAAShift: Object.values(D.skills)[0] }, D.skills);
  D.trees = Object.assign({ AAAShift: Object.values(D.trees)[0] }, D.trees);
  D.ind = Object.assign({ AFFECTER_AAAShift: Object.values(D.ind)[0] }, D.ind);
  D.bg = Object.assign({ AFFECTER_AAAShift: 1 }, D.bg);
  if (D.souls) D.souls = Object.assign({ AAAShift: Object.values(D.souls)[0] }, D.souls);
  D.insc = [Object.assign({}, D.insc[0], { k: 'AAAShift' })].concat(D.insc);
  D.sv = [Object.assign({}, D.sv[0], { k: 'AAAShift' })].concat(D.sv);
  D.races = [{ k: 'AFFECTER_AAAShift' }].concat(D.races);
  D.slots = D.slots.slice().reverse();
  D.majors = D.majors.slice().reverse();
  D.tiers.order = D.tiers.order.slice().reverse();
  for (const k in D.items) if (D.items[k].tags) D.items[k].tags = D.items[k].tags.slice().reverse();
  for (const r in D.portraits || {}) D.portraits[r] = D.portraits[r].slice().reverse();
  D.acq = [{ k: 'AAAShift' }].concat(D.acq || []);   // appended after acqOrder: older ones keep their index
  fs.writeFileSync(path.join(out, 'codec_shift.js'), withD(text, D));
  // drop: the item given by DROP_ITEM and the trait DROP_IND (run.sh picks the most used ones)
  const E = codecD(text).D;
  delete E.items[process.env.DROP_ITEM]; delete E.ind[process.env.DROP_IND];
  fs.writeFileSync(path.join(out, 'codec_drop.js'), withD(text, E));
  process.exit(0);
}
const [codec, changed, jsonl, mode] = process.argv.slice(3);
const A = loadCodec(codec), B = loadCodec(changed), DA = codecD(fs.readFileSync(codec, 'utf8')).D, DB = codecD(fs.readFileSync(changed, 'utf8')).D;
// a unit by keys only: a bonus by its stat key (the index moves with the bonus order), names cleaned as export_check.js
function keyed(u, D) {
  u = JSON.parse(JSON.stringify(u));
  for (const s in u.tags || {}) u.tags[s] = u.tags[s].map(p => ({ k: D.items[u.eq[s]].tags[p.i][0], v: Math.round(p.v * 1e6) / 1e6 })).sort((a, b) => a.k < b.k ? -1 : 1);
  if (u.name != null) u.name = String(u.name).replace(/[\x00-\x1f"\\]/g, ' ').trim();
  return u;
}
function minus(u) {                       // what the drop change removes from a unit read with today's tables
  for (const s in u.eq) if (u.eq[s] === process.env.DROP_ITEM) { delete u.eq[s]; delete u.tags[s]; }
  for (const f of ['inds', 'savedInds']) if (u[f]) u[f] = u[f].filter(k => k !== process.env.DROP_IND);
  // the wire has no "empty saved list" (builder.js writes savedInds only when it has entries): with no traits left at
  // all, the builder's own fallback (savedInds || inds) gives the same unit, so the list is absent in the export
  if (u.savedInds && !u.savedInds.length && !(u.inds || []).length) delete u.savedInds;
  return u;
}
(async () => {
  let n = 0, fail = 0, units = 0, withDrop = 0;
  for (const l of fs.readFileSync(jsonl, 'utf8').split('\n')) {
    if (!l.trim()) continue;
    const x = JSON.parse(l), ref = await A.decodeTeam(x.code0), got = await B.decodeTeam(mode === 'old' ? x.code0 : x.code);
    n++;
    let ok = ref && got && !got.warn && ref.units.length === got.units.length;
    let a = '', b = '';
    if (ok) {
      const ru = ref.units.map(u => keyed(u, DA)).map(u => mode === 'drop' ? minus(u) : u), gu = got.units.map(u => keyed(u, DB));
      a = canon(gu); b = canon(ru); ok = a === b;
      const had = JSON.stringify(ref.units).includes('"' + process.env.DROP_ITEM + '"') || JSON.stringify(ref.units).includes('"' + process.env.DROP_IND + '"');
      if (mode === 'shift' && x.dropped !== 0) ok = false;
      if (mode === 'drop' && had) { withDrop++; if (!(x.dropped > 0)) ok = false; }
    }
    if (!ok) { fail++; if (fail <= 3 && mode !== 'old') { let i = 0; while (i < a.length && a[i] === b[i]) i++; console.log(`FAIL ${x.what} (${mode}, warn ${got && got.warn}, dropped ${x.dropped}) at ${i}\n  changed: ...${a.slice(Math.max(0, i - 200), i + 200)}\n  today:   ...${b.slice(Math.max(0, i - 200), i + 200)}`); } }
    else units += got.units.length;
  }
  if (mode === 'old') {
    console.log(`== export after a wiki update, negative control (our-table codes on shifted tables): ${fail} of ${n} codes wrong, as they must be`);
    process.stdout.write('', () => process.exit(fail === n && n ? 0 : 1));
  } else {
    console.log(`== export after a wiki update (${mode}): ${n} codes, ${units} units identical${mode === 'drop' ? `, ${withDrop} codes had the removed item / trait` : ''}, ${fail} failed`);
    process.stdout.write('', () => process.exit(fail || !n || (mode === 'drop' && !withDrop) ? 1 : 0));
  }
})();
