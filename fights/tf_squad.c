/* tf_squad.c - the squad cards of the Test Fights window (tf_squad.h). Built from the shared pieces: the unit's two looks
 * (mu_look.h: the portrait and the in-game sprite the game composes itself), mu_icon / mu_text (game sprites, game font,
 * text that ends in "..."), the kit's panel and hover card. No game hook, no game state: read only. */
#include "tf_squad.h"
#include "tf.h"
#include "cm_il.h"
#include "mu_ui.h"
#include "mu_look.h"
#include "mu_worlds.h"
#include "cm_win.h"
#include "i18n/ds_i18n.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NMAX 8
/* what a card shows, made once per pick (a World units pick reads its save: not on every paint) */
typedef struct { char key[600]; char name[96]; int lvl; char race[64], bg[64]; mu_look look; int ok; } Ent;
static Ent E[NMAX];
/* the live widgets of the row (weak GC handles: never a raw pointer across frames) */
static uintptr_t h_root, h_label, h_card[NMAX], h_face[NMAX], h_sprite[NMAX];
static int nshown, nslots, face_race[NMAX];
static char shown_name[NMAX][96], shown_label[96];

static void raw_copy(const jv *v, char *out, size_t n) {          /* a key (ASCII) from the build JSON */
    out[0] = 0;
    if (v && v->t == JN_STR) { char *r = jstrdup_raw(v); snprintf(out, n, "%s", r); free(r); }
}
static uint64_t fnv(const char *s) { uint64_t h = 1469598103934665603ull; for (; s && *s; s++) { h ^= (unsigned char)*s; h *= 1099511628211ull; } return h; }
static void ent_from_build(Ent *e, const jv *b) {
    mu_look_from_build(b, &e->look);
    raw_copy(jget(b, "race"), e->race, sizeof e->race); raw_copy(jget(b, "bg"), e->bg, sizeof e->bg);
    if (!e->lvl) e->lvl = (int)jnumv(jget(b, "lvl"));
    e->ok = 1;
}
/* "Name  Lv 12" (the Gallery's row text, TR("%s  Lv %d")) -> "Name": the format's own text around %s is cut */
static void gal_name(const char *full, int lvl, char *out, size_t n) {
    char f[160]; snprintf(f, sizeof f, TR("%s  Lv %d"), "\x01", lvl);
    char *m = strchr(f, '\x01'); size_t pre = m ? (size_t)(m - f) : 0, suf = m ? strlen(m + 1) : 0, L = strlen(full);
    if (m && L > pre + suf && !strncmp(full, f, pre) && !strcmp(full + L - suf, m + 1)) snprintf(out, n, "%.*s", (int)(L - pre - suf), full + pre);
    else snprintf(out, n, "%s", full);
}
static void ent_fill(Ent *e, const tfs_pick *p) {
    char key[600];
    if (p->src == TFS_LIB) snprintf(key, sizeof key, "L|%s|%llx", p->id ? p->id : "", (unsigned long long)(p->build ? jhash(p->build) : 0));   /* an edit in the Unit Builder shows */
    else if (p->src == TFS_GAL) snprintf(key, sizeof key, "G|%llx|%d", (unsigned long long)fnv(p->gal_json), p->gal_i);
    else snprintf(key, sizeof key, "S|%s|%s", p->path ? p->path : "", p->guid ? p->guid : "");
    if (e->ok && !strcmp(e->key, key)) { if (p->name && p->src != TFS_GAL) snprintf(e->name, sizeof e->name, "%s", p->name); return; }   /* a rename shows */
    memset(e, 0, sizeof *e); snprintf(e->key, sizeof e->key, "%s", key);
    e->lvl = p->level;
    snprintf(e->name, sizeof e->name, "%s", p->name ? p->name : "");
    if (p->src == TFS_LIB) { if (p->build) ent_from_build(e, p->build); }
    else if (p->src == TFS_GAL) {
        char er[200] = ""; jv *a = p->gal_json ? jparse(p->gal_json, strlen(p->gal_json), er, sizeof er) : NULL;
        if (a && a->t == JN_ARR && p->gal_i >= 0 && (size_t)p->gal_i < a->cnt) ent_from_build(e, a->v[p->gal_i]);
        else cmw_log("tf: squad card: gallery build %d not read (%s)", p->gal_i, er);
        jfree(a);
        gal_name(p->name ? p->name : "", e->lvl, e->name, sizeof e->name);   /* the build's level cuts "  Lv N" off */
    } else {
        char er[400] = ""; jv *b = p->path && p->guid ? mu_save_build_obj(p->path, p->guid, er, sizeof er) : NULL;   /* the save cache: parsed once */
        if (b) ent_from_build(e, b); else cmw_log("tf: squad card: %s not read from its save (%s)", p->guid ? p->guid : "?", er);
        jfree(b);
    }
    if (!e->ok) e->ok = -1;                                            /* not read: kept as it is (no new read on every paint) */
}

void tfs_clear(void) {
    cm_il_hset(&h_root, NULL); cm_il_hset(&h_label, NULL);
    for (int i = 0; i < NMAX; i++) { cm_il_hset(&h_card[i], NULL); cm_il_hset(&h_face[i], NULL); cm_il_hset(&h_sprite[i], NULL); }
    nshown = nslots = 0;
}

float tfs_draw(DSLObj parent, float x, float y, float w, float h, const char *label, uint32_t label_rgba, float label_size,
               const tfs_pick *p, int n, int max) {
    const DSLUi *U = mu_kit();
    Obj old = cm_il_hget(h_root);
    if (cm_alive(old) && U) U->destroy(old);
    tfs_clear();
    cmw_forget("fights:squad.");
    if (!U || !parent) return 0;
    if (n > NMAX) n = NMAX;
    if (max < n) max = n;
    if (max > NMAX) max = NMAX;
    DSLObj root = mu_plate(parent, x, y, w, h, 0);                    /* an invisible holder: one destroy takes the row */
    if (!root) return 0;
    cm_il_hset(&h_root, root);
    /* the label, vertically centred, as wide as its text */
    DSLObj lt = mu_text(root, 0, (h - 26) / 2, w * 0.4f, 26, label, label_size, DSL_TXT_LEFT, label_rgba);
    float pw = lt ? cm_text_preferred_w(lt) : -1, lw = pw > 0 && pw < w * 0.4f ? pw : w * 0.4f;
    if (lt) U->place(lt, 0, (h - 26) / 2, lw + 2, 26);
    cm_il_hset(&h_label, lt); snprintf(shown_label, sizeof shown_label, "%s", label ? label : "");
    /* the slots: equal widths in what is left (at most 190 each) */
    const float gap = 6, x0 = lw + 14;
    float sw = (w - x0 - gap * (max - 1)) / (max ? max : 1);
    if (sw > 190) sw = 190;
    if (sw < 40) return h;                                             /* no room: the label alone */
    float img = h - 26, fit = (sw - 14) / 2;                           /* portrait + sprite squares, the name row below */
    if (img > fit) img = fit;
    if (img < 24) img = 24;
    for (int i = 0; i < max; i++) {
        float cx = x0 + i * (sw + gap);
        if (i >= n) { mu_plate(root, cx, 0, sw, h, 0x2B28304Cu); continue; }   /* a free slot: a faint frame */
        Ent *e = &E[i]; ent_fill(e, &p[i]);
        /* the card in its own slot holder: the kit's hover card then goes right beside the card, over the next one (the
           kit's rule: beside the element, off a group around it), not at the screen's edge (the window has no free side
           near the bottom row) */
        DSLObj slot = mu_plate(root, cx, 0, sw, h, 0);
        DSLObj card = slot ? U->panel(slot, 0, 0, sw, h, 0) : NULL;   /* the game's own panel, as the unit cards */
        if (!card) continue;
        cm_il_hset(&h_card[i], card);
        char cn[40]; snprintf(cn, sizeof cn, "fights:squad.card%d", i);
        cmw_named(card, CMW_TEXT, cn, NULL, NULL, e->name);          /* test: its rect (a hover on it = the player's hover) */
        /* the portrait (the game's portrait sprite of the face), else the race's icon (the unit cards' rule) */
        int two = sw >= img * 2 + 14;                                   /* room for the sprite next to the portrait */
        float ix = two ? (sw - img * 2 - 4) / 2 : (sw - img) / 2, iy = 3;   /* the pair centred on the card */
        DSLObj face = NULL; face_race[i] = 0;
        if (e->look.portrait[0] && cm_icon(CM_ICON_PORTRAIT, e->look.portrait)) face = mu_icon(card, ix, iy, img, CM_ICON_PORTRAIT, e->look.portrait, 0, NULL);
        else if (e->race[0] && cm_icon(CM_ICON_AFFECTER, e->race)) { face = mu_icon(card, ix, iy, img, CM_ICON_AFFECTER, e->race, 0, NULL); face_race[i] = 1; }
        else mu_plate(card, ix, iy, img, img, 0x1E1C21FFu);
        cm_il_hset(&h_face[i], face);
        /* the in-game sprite: the game composes it (UnitVisual.PaintAll) from the face's profile and the worn gear */
        DSLObj spr = NULL;
        if (two) {
            spr = mu_look_sprite(card, ix + img + 4, iy, img, img, &e->look);
            if (!spr) mu_plate(card, ix + img + 4, iy, img, img, 0x1E1C21FFu);
        }
        cm_il_hset(&h_sprite[i], spr);
        /* the name row: the name (ends in "...") and the level right-aligned after it */
        char lv[48]; snprintf(lv, sizeof lv, TR("Lv %d"), e->lvl > 0 ? e->lvl : 1);
        DSLObj lt2 = mu_text(card, sw - 6 - 56, h - 24, 56, 22, lv, 14, DSL_TXT_RIGHT, MU_C_GOLD);
        float lvw = lt2 ? cm_text_preferred_w(lt2) : -1; if (lvw <= 0 || lvw > 56) lvw = 56;
        mu_text(card, 6, h - 24, sw - 12 - lvw - 6, 22, e->name[0] ? e->name : TR("Unit"), 14, DSL_TXT_LEFT, MU_C_MAIN);
        /* hover: the kit's card with the full name, level, race and background */
        char rn[96] = "", bn[96] = "", body[300];
        if (e->race[0]) mu_name_of(e->race, rn, sizeof rn);
        if (e->bg[0]) mu_name_of(e->bg, bn, sizeof bn);
        snprintf(body, sizeof body, "%s%s%s%s%s", lv, rn[0] ? "  \xC2\xB7  " : "", rn, bn[0] ? "  \xC2\xB7  " : "", bn);
        cm_raycast(card, true);
        U->tooltip(card, e->name[0] ? e->name : TR("Unit"), body);
        snprintf(shown_name[i], sizeof shown_name[i], "%s", e->name);
        nshown = i + 1;
    }
    nslots = max;
    return h;
}

/* test only (driver "call"; fights/test/squad_test.py): the squad row now. {"label":"..","slots":S,"n":N,"cards":[{"name":"..",
   "card":1,"face":1,"race_icon":0,"sprite":1}]}: card / face / sprite = the widget is alive and its Image holds a sprite
   (a blank square = 0). 0 ok. */
static int has_sprite(uintptr_t h) {
    static cm_m get; static void *kim;
    if (!get.fn) { get = cm_il_method("UnityEngine.UI", "Image", "get_sprite", 0, NULL); kim = cm_il_class("UnityEngine.UI", "Image"); }
    Obj o = cm_il_hget(h);
    if (!cm_alive(o) || !get.fn || !kim) return 0;
    Obj im = cm_comp(o, kim);
    return im && cm_alive(CM_CALL(get, Obj, Obj, const void *)(im, get.mi)) ? 1 : 0;
}
static void jsafe(const char *s, char *o, size_t n) {
    size_t k = 0;
    for (; s && *s && k + 2 < n; s++) o[k++] = (*s == '"' || *s == '\\' || (unsigned char)*s < 0x20) ? ' ' : *s;
    o[k] = 0;
}
__attribute__((visibility("default"))) int tf_test_squad(const char *s, char *out, size_t cap) {
    (void)s;
    char lb[200]; jsafe(shown_label, lb, sizeof lb);
    size_t k = (size_t)snprintf(out, cap, "{\"label\":\"%s\",\"slots\":%d,\"n\":%d,\"root\":%d,\"cards\":[", lb, nslots, nshown, cm_alive(cm_il_hget(h_root)) ? 1 : 0);
    for (int i = 0; i < nshown && k + 300 < cap; i++) {
        char nm[200]; jsafe(shown_name[i], nm, sizeof nm);
        k += (size_t)snprintf(out + k, cap - k, "%s{\"name\":\"%s\",\"card\":%d,\"face\":%d,\"race_icon\":%d,\"sprite\":%d}", i ? "," : "", nm,
                              cm_alive(cm_il_hget(h_card[i])) ? 1 : 0, has_sprite(h_face[i]), face_race[i], has_sprite(h_sprite[i]));
    }
    if (k + 3 < cap) snprintf(out + k, cap - k, "]}");
    return 0;
}
/* test only (driver "call"): open Test Fights with Gallery builds, as the Gallery's Test Fight button does. s = {"title":"..",
   "names":["Name  Lv 1",..],"builds":[{build},..]}. 0 ok. */
__attribute__((visibility("default"))) int tf_test_squad_gal(const char *s, char *out, size_t cap) {
    char er[200] = ""; jv *r = s ? jparse(s, strlen(s), er, sizeof er) : NULL;
    jv *bs = r ? jget(r, "builds") : NULL, *nm = r ? jget(r, "names") : NULL;
    if (!bs || bs->t != JN_ARR) { snprintf(out, cap, "bad input: %s", er); jfree(r); return -1; }
    char *bj = jdump(bs, 0, NULL), *title = jstrdup_raw(jget(r, "title")); const char *names[8]; char *nb[8] = { 0 }; int n = 0;
    for (size_t i = 0; nm && nm->t == JN_ARR && i < nm->cnt && n < 8; i++) { nb[n] = jstrdup_raw(nm->v[i]); names[n] = nb[n]; n++; }
    tf_open_with_builds(bj, title, names, n);
    snprintf(out, cap, "opened %d", n);
    for (int i = 0; i < n; i++) free(nb[i]);
    free(bj); free(title); jfree(r);
    return 0;
}
