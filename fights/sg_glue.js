// Custom Modes - Test Fights glue (owner 2026-10-15: "custom fights and boss fights should follow the way it's done
// in the wiki"). Runs in QuickJS (sg_js.c) AFTER the wiki's own data.js, builder.js and savegen.js, downloaded from
// dungeonsettlers.wiki and never shipped. Nothing here computes a unit or a save: every call hands the wiki's own
// functions the same arguments its "Test this team in-game" panel (builder.js renderSaveGen / sgBuild) hands them.
// Two exceptions: nameKeys() below (a text-key unit name, owner 2026-10-25) and carry() (Test Fights' World units:
// a unit taken straight from a save brings its bag and, on request, its affecters; owner 2026-10-25).
// The reference harness (test/ref_node.js) loads this same file in node, so the in-game result is compared with
// the wiki's code on V8 byte for byte.
//
// Headless, builder.js runs only its pure core (everything after "UI (browser only)" needs a document). The panel's
// own small helpers (sgBossList, sgDummy, sgEnemies, sgKitDefaults, sgMgmtFor, sgDiffTiers, b64dec, SG_MAX_SQUAD,
// SG_SHOW_BASE) live in that UI part, so they are LIFTED from the builder.js source text (__CM_BUILDER_SRC, set by
// the host) and evaluated here, never retyped: when the wiki changes one, the game follows.
var CMF = (function () {
  'use strict';
  var SG = module.exports;
  // the deps builder.js loadSaveGen() passes to SG.init, name for name
  SG.init({ D: DSB, computeBuild: computeBuild, skillCost: skillCost,
    activeTrees: activeTrees, learnedByTree: learnedByTree, treeOfSkillFor: treeOfSkillFor,
    treeInscKey: treeInscKey, mainSPBudget: mainSPBudget, subSPBudget: subSPBudget,
    unitSkillSets: unitSkillSets, quickFor: quickFor,
    stackPassives: stackPassives, stackCountFor: stackCountFor, stackLvFor: stackLvFor,
    slugTeam: slugifyTeamName, diffLabel: diffLabel });

  // ---- lift top-level declarations out of the builder.js source
  // the player sees msg; the missing declaration rides in the stack, which only the log shows
  function liftErr(msg, decl) { var e = new Error(msg); try { e.stack = '(' + decl.trim() + ') ' + (e.stack || ''); } catch (x) {} return e; }
  function lift(src, decl) {
    var at = src.indexOf('\n' + decl);
    if (at < 0) throw liftErr('The wiki changed. Test Fights needs an update to use it.', decl);
    at += 1;
    if (decl.indexOf('var ') === 0) return src.slice(at, src.indexOf(';', at) + 1);
    // a function: match its braces, skipping strings and comments
    var i = src.indexOf('{', at), depth = 0;
    for (; i < src.length; i++) {
      var ch = src.charAt(i);
      if (ch === '"' || ch === "'") { var q = ch; for (i++; i < src.length && src.charAt(i) !== q; i++) if (src.charAt(i) === '\\') i++; }
      else if (ch === '/' && src.charAt(i + 1) === '/') i = src.indexOf('\n', i);
      else if (ch === '/' && src.charAt(i + 1) === '*') i = src.indexOf('*/', i) + 1;
      else if (ch === '{') depth++;
      else if (ch === '}' && --depth === 0) return src.slice(at, i + 1);
    }
    throw liftErr("Could not read the wiki's builder files.", decl);
  }
  var UI = (function () {
    var src = typeof __CM_BUILDER_SRC === 'string' ? __CM_BUILDER_SRC : '';
    var names = ['sgDiffTiers', 'sgBossList', 'sgDummy', 'sgEnemies', 'sgKitDefaults', 'sgMgmtFor', 'b64dec'];
    var body = names.map(function (n) { return lift(src, 'function ' + n + '('); }).join('\n') + '\n' +
      lift(src, 'var SG_MAX_SQUAD ') + '\n' + lift(src, 'var SG_SHOW_BASE ') + '\n' +
      'return { sgDiffTiers: sgDiffTiers, sgBossList: sgBossList, sgDummy: sgDummy, sgEnemies: sgEnemies,' +
      ' sgKitDefaults: sgKitDefaults, sgMgmtFor: sgMgmtFor, b64dec: b64dec,' +
      ' SG_MAX_SQUAD: SG_MAX_SQUAD, SG_SHOW_BASE: SG_SHOW_BASE };';
    // the UI part's own `D` is the page's DSB payload
    return (new Function('D', 'diffOrder', body))(DSB, diffOrder);
  })();
  if (typeof __CM_BUILDER_SRC !== 'undefined') __CM_BUILDER_SRC = null;   // 700 KB not kept

  // Magic Saturation (site assets/app.js dsMsat): Abyss rooms only, default day 100, keystones 0/20/40/60/100
  var MSAT_DEFAULT = 100, MSAT_KEYS = [0, 20, 40, 60, 100];
  // builder.js sgRoomIsAbyss: the payload's own flag, else dsMsat.isAbyss (key starts with UNIT_Abyss)
  function isAbyssBoss(id) {
    var bl = UI.sgBossList().filter(function (x) { return x.id === id; })[0];
    if (!bl) return false;
    var u = (DSB.units || []).filter(function (x) { return x.k === bl.key; })[0];
    return !!((u && u.abyss) || String(bl.key).indexOf('UNIT_Abyss') === 0);
  }

  // Everything the Test Fights tab shows, with the wiki panel's defaults (renderSaveGen `st`; combat = the wiki's
  // globalDiff() with nothing stored = Normal).
  function panel() {
    var S = DSB.savegen || {}, K = S.kits || {}, dummy = UI.sgDummy();
    var bosses = UI.sgBossList().map(function (b) {
      return { id: b.id, key: b.key, n: b.n, ic: b.ic, abyss: isAbyssBoss(b.id) };
    });
    var modes = [];
    if (dummy) modes.push('dps');
    modes.push('boss', 'arena');
    if (UI.SG_SHOW_BASE) modes.push('base');   // withheld on the wiki while the base template is a day-0 start
    var mgmtFor = {}, labels = {};
    diffOrder().forEach(function (t) { mgmtFor[t] = UI.sgMgmtFor(t); labels[t] = diffLabel(t); });
    return {
      tag: S.tag, siteVer: S.siteVer, tpl: S.tpl || {}, tplNames: S.tplNames || [],
      maxSquad: UI.SG_MAX_SQUAD, modes: modes, baseWithheld: !UI.SG_SHOW_BASE,
      defaults: { mode: dummy ? 'dps' : 'boss', boss: (bosses[0] || {}).id || null, combat: 'Normal',
        mgmt: UI.sgMgmtFor('Normal'), kits: UI.sgKitDefaults(), kitInsc: true, supplies: {}, msat: MSAT_DEFAULT },
      bosses: bosses, dummy: dummy,
      combatTiers: diffOrder(), mgmtTiers: UI.sgDiffTiers(), mgmtFor: mgmtFor, diffLabels: labels,
      supplies: S.supplies || [], kits: K.kits || [], kitInsc: K.insc || null, kitMiss: K.miss || [],
      enemies: UI.sgEnemies().map(function (u) { return { k: u.k, n: u.n, ic: u.ic || null, abyss: !!u.abyss }; }),
      msat: { def: MSAT_DEFAULT, keys: MSAT_KEYS }
    };
  }

  // The one change made to the wiki's save: a clan unit whose name is a text key (a save unit never renamed, as My Units'
  // snapshot keeps it: TEXTKEY_PLAYERNAME_...) gets that key as DefaultUnitNameTextKey too, as the game keeps such a unit.
  // savegen.js writes the race's _0 default there, and the game then shows UnitNameTextKey as typed (it takes the unit for
  // a renamed one: EntityDisplayNameExtensions.GetPlayerUnitDisplayName), so the unit panel and the multi-unit cards
  // showed "TEXTKEY_PLAYERNAME_Elf_Male_13" (owner 2026-10-25). Nothing to do once the wiki writes the key itself.
  function nameKeys(save) {
    var ec = save.EntityComponentSaveData.EntityComponents, uc = save.EntityContainerSaveData.UnitContainerSaveData || {};
    Object.keys(uc).forEach(function (side) {
      ((uc[side] || {}).PlayerGuidList || []).forEach(function (g) {
        (ec[g] || []).forEach(function (c) {
          var p = c && c.Type === 10 ? c.Data : null, k = p && p.UnitNameTextKey;
          if (typeof k === 'string' && k.indexOf('TEXTKEY_') === 0 && p.DefaultUnitNameTextKey !== k) p.DefaultUnitNameTextKey = k;
        });
      });
    });
  }

  // World units (owner 2026-10-25: "when loading units from save directly bring over the items they carry", "Give the
  // option to import their affecters from save as is"). A build taken straight from a save holds `_cm`, made by the game
  // side from the save (units/mu_worlds.c mu_save_fight_build; keys the game does not know are already left out):
  //   bag: its backpack's filled slots, each with the item entity's components as the save holds them;
  //   aff: its AffecterHolders, only when the player ticked "Bring buffs and effects from the save".
  // The wiki's generator has no input for either, so they go into its finished save here, the way the game keeps them:
  //   - the unit's backpack becomes the save's, slot for slot, each item a registered entity (a copy of the save's own,
  //     under a new guid). The generator's own fill for this unit goes (the ammunition it packs for a bow user: the save's
  //     bag is the unit's real one); the battle kits the player ticked stay, in the free slots after the save's items.
  //   - the save's holders as they are (Key, Stack, Duration, InitialDuration); one the build wrote too is replaced by the
  //     save's. The difficulty holders stay the fight's (the player picks the difficulty in this window).
  // A unit is found by its place: the generator writes the roster in build order (the template's own units are scrubbed
  // first); its name must match the build's, else nothing is brought and a note says why.
  function comp(cs, t) { for (var i = 0; i < (cs || []).length; i++) if (cs[i] && cs[i].Type === t) return cs[i].Data; return null; }
  var ZERO_GUID = '00000000-0000-0000-0000-000000000000';
  function newGuid(ec) {
    var h = '0123456789abcdef', g;
    do {
      var t = ''; for (var i = 0; i < 32; i++) t += h.charAt(Math.floor(Math.random() * 16));
      g = t.slice(0, 8) + '-' + t.slice(8, 12) + '-4' + t.slice(13, 16) + '-' + '89ab'.charAt(Math.floor(Math.random() * 4)) + t.slice(17, 20) + '-' + t.slice(20);
    } while (ec[g]);
    return g;
  }
  function bringBag(save, side, bp, bag, kitKeys, who, notes, probs) {
    var ec = save.EntityComponentSaveData.EntityComponents, ic = save.EntityContainerSaveData.ItemContainerSaveData[side];
    if (!bp || !bp.ItemSlots || !ic) { probs.push(who + ': no backpack in the test world for the bag from the save.'); return; }
    var keep = [], lost = 0;
    bp.ItemSlots.forEach(function (sl) {
      if (!sl || !sl.Key) return;
      if (kitKeys[sl.Key]) { keep.push(sl); return; }
      delete ec[sl.Guid]; delete ic.ItemSaveData[sl.Guid]; if (ic.ItemAutoFixable) delete ic.ItemAutoFixable[sl.Guid];
    });
    var slots = bp.ItemSlots.map(function () { return { Guid: ZERO_GUID, Key: '', Amount: 0, AutoFixed: false }; });
    function free(at) {
      if (at >= 0 && at < slots.length && !slots[at].Key) return at;
      for (var i = 0; i < slots.length; i++) if (!slots[i].Key) return i;
      return -1;
    }
    bag.forEach(function (it) {
      var at = free(it.slot);
      if (at < 0 || !it.comps) { lost++; return; }
      var g = newGuid(ec);
      ec[g] = JSON.parse(JSON.stringify(it.comps));
      ic.ItemSaveData[g] = { Guid: g, Key: it.key };
      if (ic.ItemAutoFixable) ic.ItemAutoFixable[g] = !!it.autofix;
      slots[at] = { Guid: g, Key: it.key, Amount: it.amount, AutoFixed: !!it.fixed };
    });
    keep.forEach(function (sl) {
      var at = free(-1);
      if (at >= 0) { slots[at] = sl; return; }
      delete ec[sl.Guid]; delete ic.ItemSaveData[sl.Guid]; if (ic.ItemAutoFixable) delete ic.ItemAutoFixable[sl.Guid];
      notes.push(who + ': the backpack is full with the save\'s bag - a battle kit item (' + sl.Key + ') was left out.');
    });
    bp.ItemSlots = slots;
    if (lost) probs.push(who + ': ' + lost + ' item(s) of the bag from the save did not fit or could not be read.');
  }
  function bringAff(af, holders, diffKeys) {
    if (!af) return;
    var cur = af.AffecterHolders || [];
    holders.forEach(function (h) {
      if (!h || !h.Key || diffKeys[h.Key]) return;
      var c = JSON.parse(JSON.stringify(h)), at = -1;
      for (var i = 0; i < cur.length && at < 0; i++) if (cur[i] && cur[i].Key === h.Key) at = i;
      if (at >= 0) cur[at] = c; else cur.push(c);
    });
    af.AffecterHolders = cur;
  }
  // what blocks the test world goes in probs (the panel then says what and makes no file), the rest in notes
  function carry(save, builds, kitIds, notes, probs) {
    if (!builds.some(function (b) { return b && b._cm; })) return 0;
    var ec = save.EntityComponentSaveData.EntityComponents, uc = save.EntityContainerSaveData.UnitContainerSaveData || {}, units = [];
    Object.keys(uc).forEach(function (side) { ((uc[side] || {}).PlayerGuidList || []).forEach(function (g) { units.push([g, side]); }); });
    if (units.length !== builds.length) {
      probs.push('The test world holds ' + units.length + ' units for ' + builds.length + ' picked units.');
      return 0;
    }
    var kitKeys = {}, di = (DSB.savegen || {}).diff || {}, diffKeys = {}, n = 0;
    ((DSB.savegen.kits || {}).kits || []).forEach(function (k) {
      if (kitIds.indexOf(k.id) >= 0) (k.items || []).forEach(function (r) { kitKeys[r.k] = 1; });
    });
    ['combatAff', 'mgmtAff'].forEach(function (t) { Object.keys(di[t] || {}).forEach(function (k) { diffKeys[di[t][k]] = 1; }); });
    builds.forEach(function (b, i) {
      var cm = b && b._cm; if (!cm) return;
      var comps = ec[units[i][0]] || [], prof = comp(comps, 10), who = b.name || ('unit ' + (i + 1));
      if (b.name && (!prof || prof.UnitNameTextKey !== b.name)) {
        probs.push(who + ': not found in the test world by its name, so its bag and effects could not be brought.');
        return;
      }
      if (cm.bag) bringBag(save, units[i][1], comp(comps, 5), cm.bag, kitKeys, who, notes, probs);
      if (cm.aff) bringAff(comp(comps, 3), cm.aff, diffKeys);
      n++;
    });
    return n;
  }

  // opts = the tab's choices: { mode, boss, builds:[stored builds], combat, mgmt, monsters:[{k,n}],
  //   supplies:{key:amount} | [{k,amount}], kits:{id:true} | [id], kitInsc, msat, team }
  // Builds go in as stored (the builder's own build objects, as the wiki's roster keeps them).
  // -> the wiki's generate() result with the save as the text its download writes (JSON.stringify(save, null, 2))
  function generate(o) {
    var builds = (o.builds || []).slice(0, UI.SG_MAX_SQUAD);   // the panel never sends more than an expedition holds
    var sup = Array.isArray(o.supplies) ? o.supplies
      : Object.keys(o.supplies || {}).map(function (k) { return { k: k, amount: o.supplies[k] }; });
    // kit ids in the payload's own order, as sgBuild passes them
    var kon = Array.isArray(o.kits) ? o.kits.reduce(function (m, k) { m[k] = true; return m; }, {})
      : (o.kits || UI.sgKitDefaults());
    var kits = ((DSB.savegen.kits || {}).kits || []).filter(function (k) { return kon[k.id]; }).map(function (k) { return k.id; });
    var abyss = (o.mode === 'boss' || o.mode === 'arena') && isAbyssBoss(o.boss);
    var combat = o.combat || 'Normal';
    return SG.generate({ mode: o.mode, boss: o.boss, builds: builds, combat: combat,
      mgmt: o.mgmt || UI.sgMgmtFor(combat), monsters: o.monsters || [], supplies: sup, kits: kits,
      msat: abyss ? (o.msat == null ? MSAT_DEFAULT : o.msat) : null,
      kitInsc: o.kitInsc !== false, raw: null, team: o.team == null ? '' : o.team,
      applyDifficulty: false
    }).then(function (r) {
      var cp = [];
      if (carry(r.save, builds, kits, r.notes, cp)) {                  // the save checked again with what carry() added
        var p2 = SG.validate(r.save, { roster: r.added != null ? r.added : builds.length });
        if (p2.length > (r.problems || []).length) r.problems = p2;
      }
      if (cp.length) r.problems = (r.problems || []).concat(cp);
      nameKeys(r.save);
      return { file: JSON.stringify(r.save, null, 2), filename: r.filename, problems: r.problems,
        notes: r.notes, warnings: r.warnings, added: r.added, roster: r.roster, room: r.room || null,
        clan: r.clan };
    });
  }

  // a gallery item's stored build (`blob` of /api/gallery?id=<id>&full=1: a ?team= code, or 'b:' + the character
  // JSON) -> builds, by builder.js's own decoders (the steps of its DSB_EMBED_API.decode, without the UI-side
  // validatedSnapshot pass, which needs the page)
  function decode(blob) {
    if (typeof blob !== 'string') return Promise.resolve([]);
    if (blob.indexOf('b:') === 0) {
      var b = UI.b64dec(blob.slice(2));
      return Promise.resolve(b && typeof b === 'object' ? [b] : []);
    }
    return decodeTeam(blob).then(function (d) { return (d && d.units) ? d.units : []; });
  }

  // the template a fight needs (builder.js sgTemplateFor)
  function templateFor(mode, boss) {
    if (mode === 'base') return 'base';
    if (mode === 'dps') return 'boss_' + (((DSB.savegen || {}).dummy || {}).boss || 'golem');
    return 'boss_' + (boss || 'golem');
  }

  return { panel: panel, generate: generate, decode: decode, templateFor: templateFor,
    templateUrl: SG.templateUrl, tag: SG.TAG };
})();
