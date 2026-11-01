// "Open in Wiki Builder" guard (units/wk_export.c): the wiki builder ITSELF (builder.js in node) decodes each C team code
// to exactly what it decodes from its own encodeTeam of the same build(s). A drift of the C encoder from builder.js
// (a field, a flag bit, a list order, a roll step) fails here.
//   node export_check.js EXPORT.jsonl     lines {"what", "code", "build": a build or [builds]} from save_test export / enc
const path = require('path'), fs = require('fs'), zlib = require('zlib');
const { load } = require(path.join(__dirname, '..', 'node', 'builder_ref.js'));
const c = load();
function dec(code) {
  const bytes = c.dsbB64ToBytes(code); if (!bytes) return null;
  let d = null; try { d = c._decodeBlob(bytes); } catch (e) { d = null; }
  if (d) return d;
  try { return c._decodeBlob(Array.from(zlib.inflateRawSync(Buffer.from(bytes)))); } catch (e) { return null; }
}
(async () => {
  let n = 0, fail = 0, units = 0, v2 = 0;
  for (const l of fs.readFileSync(process.argv[2], 'utf8').split('\n')) {
    if (!l.trim()) continue;
    const x = JSON.parse(l), builds = Array.isArray(x.build) ? x.build : [x.build];
    const mine = dec(x.code), ref = dec(await c.encodeTeam(builds));
    n++; if (c.dsbB64ToBytes(x.code)[0] !== 1) v2++;
    // a C build's name has no '"', '\\' or control bytes (pvp_wiki.c: they become spaces, then trimmed): the same here
    const nm = d => d && d.units.forEach(u => { if (u.name != null) u.name = String(u.name).replace(/[\x00-\x1f"\\]/g, ' ').trim(); });
    nm(mine); nm(ref);
    const a = mine && JSON.stringify(mine.units), b = ref && JSON.stringify(ref.units);
    if (!mine || mine.warn || a !== b) {
      fail++;
      if (fail <= 5) console.log(`FAIL ${x.what}: the builder decodes the C code differently\n  C:       ${a}\n  builder: ${b}`);
    } else units += mine.units.length;
  }
  console.log(`== export codes vs builder.js encodeTeam: ${n} codes (${v2} deflate), ${units} units identical, ${fail} failed`);
  process.stdout.write('', () => process.exit(fail || !n ? 1 : 0));
})();
