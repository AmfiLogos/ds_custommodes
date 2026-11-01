/* wk_export - the wiki builder team-code encoder + the export link + the system browser (see wk_export.h). */
#include "wk_export.h"
#include "pvp_wiki_tables.h"
#include "fe/fe_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <spawn.h>
#include <sys/wait.h>
extern char **environ;
#endif

typedef struct { const char **k; int n, cap; } klist;
static void kl_add(klist *l, const char *s, size_t n) {
    if (l->n == l->cap) { l->cap = l->cap ? l->cap * 2 : 64; l->k = realloc(l->k, sizeof *l->k * (size_t)l->cap); }
    char *c = malloc(n + 1); memcpy(c, s, n); c[n] = 0; l->k[l->n++] = c;
}
static void kl_free(klist *l) { for (int i = 0; i < l->n; i++) free((void *)l->k[i]); free(l->k); memset(l, 0, sizeof *l); }
static int kl_has(const klist *l, const char *s) { for (int i = 0; i < l->n; i++) if (!strcmp(l->k[i], s)) return 1; return 0; }
static int kcmp(const void *a, const void *b) { return strcmp(*(const char *const *)a, *(const char *const *)b); }
static void kl_sort(klist *l) { if (l->n > 1) qsort(l->k, (size_t)l->n, sizeof *l->k, kcmp); }
static void kl_keys(klist *l, const jv *o) { for (size_t i = 0; o && o->t == JN_OBJ && i < o->cnt; i++) kl_add(l, o->k[i], o->kn[i]); }
static void kl_field(klist *l, const jv *a, const char *f) {               /* [{k: ...}] */
    for (size_t i = 0; a && a->t == JN_ARR && i < a->cnt; i++) { const jv *k = jget(a->v[i], f); if (k && k->t == JN_STR) kl_add(l, k->s, k->n); }
}
static void kl_strs(klist *l, const jv *a) { for (size_t i = 0; a && a->t == JN_ARR && i < a->cnt; i++) if (a->v[i]->t == JN_STR) kl_add(l, a->v[i]->s, a->v[i]->n); }

/* the index tables a code is written against: ours (pvp_wiki_tables.h, made when Units Laboratory is built) or the live
   wiki's (wkx_live_load: the tables the wiki serves NOW, so a code made after a wiki data update still loads these units) */
enum { WT_RACE, WT_BG, WT_IND, WT_INSC, WT_TREE, WT_SKILL, WT_ITEM, WT_STAT, WT_SLOT, WT_SOUL, WT_ACQ, WT_MAJ, WT_TIER, WT_N };
typedef struct wk_tabs {
    int fp, n[WT_N];
    const jv *items, *ports;                 /* live only: D.items, D.portraits (NULL = ours) */
    const klist *item_keys, *race_keys;      /* live only: ITEM / RACE in the live index order */
} wk_tabs;
static const wk_tabs OURS = { WK_FP, { WK_RACE_N, WK_BG_N, WK_IND_N, WK_INSC_N, WK_TREE_N, WK_SKILL_N, WK_ITEM_N, WK_STAT_N, WK_SLOT_N,
                                       WK_SOUL_N, WK_ACQ_N, WK_MAJ_N, WK_TIER_N }, NULL, NULL, NULL, NULL };
static const jv *t_tags(const wk_tabs *T, int it) { return jget(jget(T->items, T->item_keys->k[it]), "tags"); }
static int t_ntag(const wk_tabs *T, int it) {
    if (!T->items) return WK_ITEM_NTAGS[it];
    const jv *tg = t_tags(T, it); return tg && tg->t == JN_ARR ? (int)tg->cnt : 0;
}
static void t_range(const wk_tabs *T, int it, int idx, double *lo, double *hi) {
    if (!T->items) { *lo = WK_TAG_RANGE[2 * (WK_ITEM_TAG_AT[it] + idx)]; *hi = WK_TAG_RANGE[2 * (WK_ITEM_TAG_AT[it] + idx) + 1]; return; }
    const jv *t = t_tags(T, it)->v[idx];
    *lo = t && t->t == JN_ARR && t->cnt >= 3 ? jnumv(t->v[1]) : 0; *hi = t && t->t == JN_ARR && t->cnt >= 3 ? jnumv(t->v[2]) : 0;
}
static int t_nport(const wk_tabs *T, int race) {
    if (!T->ports) return WK_PORTRAITS_N[race];
    const jv *p = jget(T->ports, T->race_keys->k[race]); return p && p->t == JN_ARR ? (int)p->cnt : 0;
}

/* ---- LEB128 writer (builder.js DSBW) */
typedef struct { uint8_t *b; size_t n, cap; int bad; } wr;
static void wbyte(wr *w, int v) {
    if (w->bad) return;
    if (w->n == w->cap) { size_t c = w->cap ? w->cap * 2 : 256; uint8_t *p = realloc(w->b, c); if (!p) { w->bad = 1; return; } w->b = p; w->cap = c; }
    w->b[w->n++] = (uint8_t)(v & 0xff);
}
static void wu(wr *w, uint32_t v) { do { uint8_t x = v & 0x7f; v >>= 7; wbyte(w, v ? x | 0x80 : x); } while (v); }
static void wzz(wr *w, int v) { wu(w, v < 0 ? ((uint32_t)(-(int64_t)v) << 1) - 1 : (uint32_t)v << 1); }
static long jround(double x) { return (long)floor(x + 0.5); }         /* JavaScript Math.round */
static void wlist(wr *w, const int *v, int n, int table_n) {
    int k = 0; for (int i = 0; i < n; i++) if (v[i] >= 0 && v[i] < table_n) k++;
    wu(w, (uint32_t)k);
    for (int i = 0; i < n; i++) if (v[i] >= 0 && v[i] < table_n) wu(w, (uint32_t)v[i]);
}
/* the name as builder.js writes it: trimmed; the decoder takes <= 48 bytes, so a longer one is cut at a UTF-8 boundary */
static size_t name_of(const char *p, const char **s) {
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
    size_t n = strlen(p); while (n && (p[n - 1] == ' ' || p[n - 1] == '\t' || p[n - 1] == '\n' || p[n - 1] == '\r')) n--;
    if (n > 48) { n = 48; while (n && ((unsigned char)p[n] & 0xC0) == 0x80) n--; }
    *s = p; return n;
}
static int enc_build(wr *w, const wk_build *b, const char *name, const wk_tabs *T) {
    if (b->race < 0 || b->race >= T->n[WT_RACE]) return -1;
    wu(w, (uint32_t)b->race);
    wu(w, b->bg >= 0 && b->bg < T->n[WT_BG] ? (uint32_t)b->bg + 1 : 0);
    wu(w, (uint32_t)(b->lvl < 1 ? 1 : b->lvl > 255 ? 255 : b->lvl));
    uint32_t tp = 0; for (int i = 0; i < 6; i++) tp = tp * 5 + (uint32_t)(b->tal[i] >= 0 && b->tal[i] < T->n[WT_TIER] ? b->tal[i] : 1);
    wu(w, tp);
    const char *nm; size_t nl = name_of(name ? name : b->name, &nm);
    int elix = 0, cond = 0;
    for (int k = 0; k < 6; k++) if (b->has_elix && b->elix[k] > 0) elix = 1;
    for (int k = 0; k < b->ncond && k < 8; k++) if (b->cond[k] > 0) cond = 1;
    int port = b->portrait >= 0 && b->portrait < t_nport(T, b->race);
    uint32_t fl = (uint32_t)(b->offer & 3) | (uint32_t)(b->roll & 3) << 2 | (uint32_t)(b->advanced ? 1 : 0) << 4 | (uint32_t)(b->add_shield ? 1 : 0) << 5
        | (uint32_t)(nl > 0) << 6 | (uint32_t)port << 7 | (uint32_t)(b->nman > 0) << 8 | (uint32_t)(b->has_mb ? 1 : 0) << 9
        | (uint32_t)(b->npstk > 0) << 10 | (uint32_t)(b->nsouls > 0) << 11 | (uint32_t)(b->nsaved_inds > 0) << 12 | (uint32_t)(b->nsaved_insc > 0) << 13
        | (uint32_t)(b->nacq > 0) << 14 | (uint32_t)(b->hide_head ? 1 : 0) << 15 | (uint32_t)elix << 16 | (uint32_t)cond << 17;
    wu(w, fl);
    wlist(w, b->inds, b->ninds, T->n[WT_IND]);
    wlist(w, b->trees, b->ntrees, T->n[WT_TREE]);
    wlist(w, b->learned, b->nlearned, T->n[WT_SKILL]);
    wlist(w, b->insc, b->ninsc, T->n[WT_INSC]);
    uint32_t eqmask = 0;
    for (int s = 0; s < T->n[WT_SLOT] && s < WK_SLOTS; s++) if (b->eq[s] >= 0 && b->eq[s] < T->n[WT_ITEM]) eqmask |= 1u << s;
    wu(w, eqmask);
    for (int s = 0; s < T->n[WT_SLOT] && s < WK_SLOTS; s++) if (eqmask >> s & 1) wu(w, (uint32_t)b->eq[s]);
    /* a tag the item lacks is dropped, as the encoder does */
    uint32_t tagmask = 0; int ntag[WK_SLOTS] = { 0 };
    for (int s = 0; s < T->n[WT_SLOT] && s < WK_SLOTS; s++) if (eqmask >> s & 1) {
        for (int k = 0; k < b->ntag[s] && k < 8; k++) if (b->tag[s][k].idx >= 0 && b->tag[s][k].idx < t_ntag(T, b->eq[s])) ntag[s]++;
        if (ntag[s]) tagmask |= 1u << s;
    }
    wu(w, tagmask);
    for (int s = 0; s < T->n[WT_SLOT] && s < WK_SLOTS; s++) if (tagmask >> s & 1) {
        wu(w, (uint32_t)ntag[s]);
        int it = b->eq[s];
        for (int k = 0; k < b->ntag[s] && k < 8; k++) {
            int idx = b->tag[s][k].idx; if (idx < 0 || idx >= t_ntag(T, it)) continue;
            double lo, hi; t_range(T, it, idx, &lo, &hi);
            long fr = hi > lo ? jround((b->tag[s][k].v - lo) / (hi - lo) * 255) : 0;
            wu(w, (uint32_t)idx); wbyte(w, (int)(fr < 0 ? 0 : fr > 255 ? 255 : fr));
        }
    }
    if (nl) { wu(w, (uint32_t)nl); for (size_t k = 0; k < nl; k++) wbyte(w, (unsigned char)nm[k]); }
    if (port) wu(w, (uint32_t)b->portrait);
    if (b->nman > 0) {
        int k = 0; for (int i = 0; i < b->nman; i++) if (b->man[i].stat >= 0 && b->man[i].stat < T->n[WT_STAT]) k++;
        wu(w, (uint32_t)k);
        for (int i = 0; i < b->nman; i++) if (b->man[i].stat >= 0 && b->man[i].stat < T->n[WT_STAT]) { wu(w, (uint32_t)b->man[i].stat); wzz(w, (int)jround(b->man[i].v * 100.0)); }
    }
    if (b->has_mb) for (int k = 0; k < 6; k++) wzz(w, (int)jround(b->mb[k] * 100.0));
    if (b->npstk > 0) {
        int k = 0; for (int i = 0; i < b->npstk; i++) if (b->pstk[i].skill >= 0 && b->pstk[i].skill < T->n[WT_SKILL] && b->pstk[i].st >= 0) k++;
        wu(w, (uint32_t)k);
        for (int i = 0; i < b->npstk; i++) if (b->pstk[i].skill >= 0 && b->pstk[i].skill < T->n[WT_SKILL] && b->pstk[i].st >= 0) { wu(w, (uint32_t)b->pstk[i].skill); wu(w, (uint32_t)b->pstk[i].st); }
    }
    if (b->nsouls > 0) wlist(w, b->souls, b->nsouls, T->n[WT_SOUL]);
    if (b->nsaved_inds > 0) wlist(w, b->saved_inds, b->nsaved_inds, T->n[WT_IND]);
    if (b->nsaved_insc > 0) wlist(w, b->saved_insc, b->nsaved_insc, T->n[WT_INSC]);
    if (b->nacq > 0) wlist(w, b->acq, b->nacq, T->n[WT_ACQ]);
    if (elix) { wu(w, 6); for (int k = 0; k < 6; k++) wu(w, (uint32_t)(b->elix[k] > 0 ? b->elix[k] : 0)); }
    if (cond) { wu(w, WK_COND_N); for (int k = 0; k < WK_COND_N; k++) wu(w, (uint32_t)(k < 8 && k < b->ncond && b->cond[k] > 0 ? b->cond[k] : 0)); }
    return w->bad ? -1 : 0;
}
static int b64url(const uint8_t *in, size_t n, char *out, size_t outn) {
    static const char T[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
    size_t o = 0;
    for (size_t i = 0; i < n; i += 3) {
        uint32_t v = (uint32_t)in[i] << 16 | (i + 1 < n ? (uint32_t)in[i + 1] << 8 : 0) | (i + 2 < n ? in[i + 2] : 0);
        int k = i + 2 < n ? 4 : i + 1 < n ? 3 : 2;
        for (int j = 0; j < k; j++) { if (o + 1 >= outn) return -1; out[o++] = T[(v >> (18 - 6 * j)) & 63]; }
    }
    if (o >= outn) return -1;
    out[o] = 0; return 0;
}
static int enc_team(const wk_tabs *T, const wk_build *b, int n, const char *const *names, char *out, size_t outn) {
    if (!b || n < 1 || n > WK_MAX_UNITS || !out || !outn) return -1;
    wr w = { 0 };
    wbyte(&w, 1); wbyte(&w, (T->fp >> 8) & 0xff); wbyte(&w, T->fp & 0xff); wu(&w, (uint32_t)n);
    for (int i = 0; i < n; i++) if (enc_build(&w, &b[i], names ? names[i] : NULL, T)) { free(w.b); return -1; }
    if (w.bad) { free(w.b); return -1; }
    /* v1 (the raw blob, version 1): every builder reads it (builder.js decodeTeam tries it first). Not v2: the Windows
       module has zlib's inflate only (ds_multiplayer win/build_win.sh zl_*), and v1 keeps both platforms' links the same */
    int rc = b64url(w.b, w.n, out, outn);
    free(w.b);
    return rc;
}
int wk_encode_named(const wk_build *b, int n, const char *const *names, char *out, size_t outn) { return enc_team(&OURS, b, n, names, out, outn); }

int wkx_encode_live(const wkx_live *x, const wk_build *b, int n, const char *const *names, char *out, size_t outn, int *dropped);
static size_t urlenc(const char *s, char *o, size_t n) {
    static const char H[] = "0123456789ABCDEF"; size_t k = 0;
    for (; s && *s; s++) {
        unsigned char c = (unsigned char)*s;
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~') { if (k + 1 >= n) return (size_t)-1; o[k++] = (char)c; }
        else { if (k + 3 >= n) return (size_t)-1; o[k++] = '%'; o[k++] = H[c >> 4]; o[k++] = H[c & 15]; }
    }
    if (k >= n) return (size_t)-1;
    o[k] = 0; return k;
}
int wkx_export_url(const wkx_live *x, const wk_build *b, int n, const char *const *names, const char *team_name, int pub, const char *lang, char *out, size_t outn, int *dropped) {
    char code[8192], tn[400] = "";
    int rc = wkx_encode_live(x, b, n, names, code, sizeof code, dropped);
    if (rc) return rc;
    if (team_name && urlenc(team_name, tn, sizeof tn) == (size_t)-1) tn[0] = 0;
    const char *l = lang && *lang && strcmp(lang, "en") ? lang : "";
    /* gx hashes the units in OUR indexes: the same units give the same gx before and after a wiki data update */
    int k = snprintf(out, outn, "%s%s%s/builder?team=%s&from=game&pub=%d%s%s&gx=%016llx", WKX_HOST, *l ? "/" : "", l, code,
                     pub ? 1 : 0, tn[0] ? "&tn=" : "", tn, (unsigned long long)wk_hash(b, n));
    return k > 0 && (size_t)k < outn ? 0 : -1;
}
int wk_export_url(const wk_build *b, int n, const char *const *names, const char *team_name, int pub, const char *lang, char *out, size_t outn) {
    return wkx_export_url(NULL, b, n, names, team_name, pub, lang, out, outn, NULL);
}
int wk_open_url(const char *url) {
    if (!url || strncmp(url, WKX_HOST "/", strlen(WKX_HOST) + 1)) return -1;      /* only our wiki, only https */
    for (const char *p = url; *p; p++) if ((unsigned char)*p <= ' ' || *p == '"' || *p == '\\') return -1;
#ifdef _WIN32
    int wn = MultiByteToWideChar(CP_UTF8, 0, url, -1, NULL, 0); if (wn <= 0) return -1;
    wchar_t *wu8 = malloc((size_t)wn * sizeof *wu8); if (!wu8) return -1;
    MultiByteToWideChar(CP_UTF8, 0, url, -1, wu8, wn);
    /* shell32 by name: the module's link line (ds_multiplayer win/build_win.sh) has no -lshell32 */
    typedef HINSTANCE (WINAPI *shex_fn)(HWND, LPCWSTR, LPCWSTR, LPCWSTR, LPCWSTR, INT);
    HMODULE sh = LoadLibraryW(L"shell32.dll");
    shex_fn shex = sh ? (shex_fn)(void *)GetProcAddress(sh, "ShellExecuteW") : NULL;
    HINSTANCE h = shex ? shex(NULL, L"open", wu8, NULL, NULL, SW_SHOWNORMAL) : NULL;
    free(wu8);
    return (INT_PTR)h > 32 ? 0 : -1;
#else
    /* as the loader's bug report button (ds_modloader loader/src/dsl_bugrep.c): /usr/bin/open, waited for (it returns at
       once). In test copies the loader's quiet mode refuses it and logs the URL (dsl_quiet_mac.c note_open) */
    pid_t pid; int st = 0; char *av[] = { "/usr/bin/open", (char *)url, NULL };
    if (posix_spawn(&pid, av[0], NULL, NULL, av, environ)) return -1;
    if (waitpid(pid, &st, 0) < 0) return -1;
    return WIFEXITED(st) && WEXITSTATUS(st) == 0 ? 0 : -1;
#endif
}
int wk_encode(const wk_build *b, int n, char *out, size_t outn) { return wk_encode_named(b, n, NULL, out, outn); }

/* ---- the live wiki's tables (WKX_CODEC_PATH, the builder's own index source, as builder.js dsbIdx builds them).
   Our tables (pvp_wiki_tables.h) are made from the wiki data when Units Laboratory is built; the wiki changes its data on
   a game update or a data fix. The link carries INDEXES, and the wiki's fingerprint only counts the table sizes, so a
   code written against our tables after such a change would load another item / skill. So the export re-maps every
   index BY KEY to the lists the wiki serves at the moment of the click (wkx_live_load), writes the wiki's fingerprint and
   bonus ranges, and leaves out what the wiki no longer has: the button keeps working across wiki updates (lead task 9).
   wkx_codec_check (the lists are the same as ours, key for key) is the release guard: tables regenerated before a release. */
static const char *const WT_WHAT[WT_N] = { "race", "background", "trait", "inscription", "skill tree", "skill", "item", "stat",
                                           "equipment slot", "soul stone", "acquired trait", "major stat", "talent tier" };
static const char *const *const WT_OURS[WT_N] = { WK_RACE, WK_BG, WK_IND, WK_INSC, WK_TREE, WK_SKILL, WK_ITEM, WK_STAT, WK_SLOT,
                                                  WK_SOUL, WK_ACQ, WK_MAJ, WK_TIER };
static void kl_ret(klist *l, const jv *r) {           /* + the retired keys (dsbIdx withRetired) */
    for (size_t i = 0; r && r->t == JN_OBJ && i < r->cnt; i++) { char t[96]; snprintf(t, sizeof t, "%.*s", (int)r->kn[i], r->k[i]); if (!kl_has(l, t)) kl_add(l, r->k[i], r->kn[i]); }
}
/* "var D = {...}" of the codec file, parsed; NULL + err */
static jv *codec_d(const char *text, char *err, size_t errn) {
    const char *p = text ? strstr(text, "var D = ") : NULL;
    if (!p) { snprintf(err, errn, "the wiki's builder tables were not found"); return NULL; }
    p += 8;
    const char *e = strstr(p, ";\nvar ");
    if (!e) { snprintf(err, errn, "the wiki's builder tables were not found"); return NULL; }
    char pe[200]; jv *D = jparse(p, (size_t)(e - p), pe, sizeof pe);
    if (!D || D->t != JN_OBJ) { snprintf(err, errn, "the wiki's builder tables do not read: %s", pe); if (D) jfree(D); return NULL; }
    return D;
}
/* every index list in dsbIdx order */
static void live_lists(const jv *D, klist L[WT_N]) {
    const jv *RET = jget(D, "retired");
    memset(L, 0, sizeof(klist) * WT_N);
    kl_field(&L[WT_RACE], jget(D, "races"), "k"); kl_ret(&L[WT_RACE], jget(RET, "races")); kl_sort(&L[WT_RACE]);
    kl_keys(&L[WT_BG], jget(D, "bg")); kl_sort(&L[WT_BG]);
    kl_keys(&L[WT_IND], jget(D, "ind")); kl_ret(&L[WT_IND], jget(RET, "ind")); kl_sort(&L[WT_IND]);
    kl_field(&L[WT_INSC], jget(D, "insc"), "k"); kl_sort(&L[WT_INSC]);
    kl_keys(&L[WT_TREE], jget(D, "trees")); kl_sort(&L[WT_TREE]);
    kl_keys(&L[WT_SKILL], jget(D, "skills")); kl_sort(&L[WT_SKILL]);
    kl_keys(&L[WT_ITEM], jget(D, "items")); kl_sort(&L[WT_ITEM]);
    kl_field(&L[WT_STAT], jget(D, "sv"), "k"); kl_sort(&L[WT_STAT]);
    kl_strs(&L[WT_SLOT], jget(D, "slots"));
    kl_keys(&L[WT_SOUL], jget(D, "souls")); kl_sort(&L[WT_SOUL]);
    /* ACQ: D.acqOrder (the ones D.acq has) first, then the other D.acq keys sorted (dsbIdx ACQKEYS) */
    klist all = { 0 }; kl_field(&all, jget(D, "acq"), "k");
    const jv *o = jget(D, "acqOrder");
    for (size_t i = 0; o && o->t == JN_ARR && i < o->cnt; i++) if (o->v[i]->t == JN_STR) { char t[96]; snprintf(t, sizeof t, "%.*s", (int)o->v[i]->n, o->v[i]->s); if (kl_has(&all, t) && !kl_has(&L[WT_ACQ], t)) kl_add(&L[WT_ACQ], t, strlen(t)); }
    kl_sort(&all);
    for (int i = 0; i < all.n; i++) if (!kl_has(&L[WT_ACQ], all.k[i])) kl_add(&L[WT_ACQ], all.k[i], strlen(all.k[i]));
    kl_free(&all);
    kl_strs(&L[WT_MAJ], jget(D, "majors"));
    kl_strs(&L[WT_TIER], jget(jget(D, "tiers"), "order"));
}
static int live_fp(const klist L[WT_N]) {             /* dsbIdx fp: RACE, BG, IND, INSC, TREE, SKILL, ITEM, SLOT sizes */
    static const int k[8] = { WT_RACE, WT_BG, WT_IND, WT_INSC, WT_TREE, WT_SKILL, WT_ITEM, WT_SLOT };
    int fp = 0; for (int i = 0; i < 8; i++) fp = (fp * 131 + L[k[i]].n) & 0xffff;
    return fp;
}
static int kl_find(const klist *l, const char *s) { for (int i = 0; i < l->n; i++) if (!strcmp(l->k[i], s)) return i; return -1; }

int wkx_codec_check(const char *text, size_t n, char *err, size_t errn) {
    (void)n;
    if (err && errn) err[0] = 0;
    jv *D = codec_d(text, err, errn); if (!D) return -1;
    klist L[WT_N]; live_lists(D, L);
    int ok = 1; char w[64];
    for (int t = 0; ok && t < WT_N; t++) {
        int tn = OURS.n[t];
        for (int i = 0; i < L[t].n || i < tn; i++) {
            const char *a = i < L[t].n ? L[t].k[i] : "(none)", *b = i < tn ? WT_OURS[t][i] : "(none)";
            if (strcmp(a, b)) { snprintf(err, errn, "%s %d: the wiki has %s, this version %s", WT_WHAT[t], i, a, b); ok = 0; break; }
        }
    }
    /* portraits per race (the link carries the index in the race's pool) */
    for (int r = 0; ok && r < WK_RACE_N; r++) {
        klist P = { 0 }; kl_strs(&P, jget(jget(D, "portraits"), WK_RACE[r]));
        snprintf(w, sizeof w, "portrait of %s", WK_RACE[r]);
        for (int i = 0; ok && (i < P.n || i < WK_PORTRAITS_N[r]); i++) {
            const char *a = i < P.n ? P.k[i] : "(none)", *b = i < WK_PORTRAITS_N[r] ? WK_PORTRAITS[r][i] : "(none)";
            if (strcmp(a, b)) { snprintf(err, errn, "%s %d: the wiki has %s, this version %s", w, i, a, b); ok = 0; }
        }
        kl_free(&P);
    }
    /* each item's rolled bonuses: key and range (the link carries the index and a 1/255 step of the range) */
    for (int i = 0; ok && i < WK_ITEM_N; i++) {
        const jv *tg = jget(jget(jget(D, "items"), WK_ITEM[i]), "tags");
        int tn = tg && tg->t == JN_ARR ? (int)tg->cnt : 0;
        if (tn != WK_ITEM_NTAGS[i]) { snprintf(err, errn, "item %s: the wiki has %d bonuses, this version %d", WK_ITEM[i], tn, WK_ITEM_NTAGS[i]); ok = 0; break; }
        for (int k = 0; k < tn; k++) {
            const jv *t = tg->v[k]; double lo = t && t->t == JN_ARR && t->cnt >= 3 ? jnumv(t->v[1]) : -1, hi = t && t->t == JN_ARR && t->cnt >= 3 ? jnumv(t->v[2]) : -1;
            double mlo = WK_TAG_RANGE[2 * (WK_ITEM_TAG_AT[i] + k)], mhi = WK_TAG_RANGE[2 * (WK_ITEM_TAG_AT[i] + k) + 1];
            if (fabs(lo - mlo) > 1e-9 || fabs(hi - mhi) > 1e-9) { snprintf(err, errn, "item %s bonus %d: the wiki has %g..%g, this version %g..%g", WK_ITEM[i], k, lo, hi, mlo, mhi); ok = 0; break; }
        }
    }
    for (int t = 0; t < WT_N; t++) kl_free(&L[t]);
    jfree(D);
    return ok ? 0 : -1;
}

struct wkx_live { jv *D; klist L[WT_N]; short *map[WT_N]; wk_tabs T; };
wkx_live *wkx_live_load(const char *text, size_t n, char *err, size_t errn) {
    (void)n;
    if (err && errn) err[0] = 0;
    jv *D = codec_d(text, err, errn); if (!D) return NULL;
    wkx_live *x = calloc(1, sizeof *x); if (!x) { jfree(D); return NULL; }
    x->D = D; live_lists(D, x->L);
    const int *ours_n = OURS.n;
    for (int t = 0; t < WT_N; t++) {
        x->map[t] = malloc(sizeof(short) * (size_t)(ours_n[t] ? ours_n[t] : 1));
        for (int i = 0; i < ours_n[t]; i++) x->map[t][i] = (short)kl_find(&x->L[t], WT_OURS[t][i]);
        x->T.n[t] = x->L[t].n;
    }
    x->T.fp = live_fp(x->L); x->T.items = jget(D, "items"); x->T.ports = jget(D, "portraits");
    x->T.item_keys = &x->L[WT_ITEM]; x->T.race_keys = &x->L[WT_RACE];
    /* what the link format itself fixes: 6 major stats (the 6 talents, growths, elixirs) and at most 5 talent tiers
       (packed base 5). Another shape = the wiki changed the format: only a new version of Units Laboratory can write it */
    int bad = x->L[WT_MAJ].n != 6 || x->L[WT_TIER].n < 1 || x->L[WT_TIER].n > 5 || x->L[WT_RACE].n < 1 || x->L[WT_SLOT].n > WK_SLOTS;
    for (int i = 0; !bad && i < 6 && i < WK_MAJ_N; i++) if (x->map[WT_MAJ][i] < 0) bad = 1;
    if (bad) { snprintf(err, errn, "the wiki's link format changed (major stats / talent tiers / slots)"); wkx_live_free(x); return NULL; }
    return x;
}
void wkx_live_free(wkx_live *x) {
    if (!x) return;
    for (int t = 0; t < WT_N; t++) { kl_free(&x->L[t]); free(x->map[t]); }
    jfree(x->D); free(x);
}
int wkx_live_fp(const wkx_live *x) { return x ? x->T.fp : -1; }
/* one build in our indexes -> the same build in the live wiki's indexes; what the wiki no longer has is left out
   (counted in *dropped). -1 = the unit's race is gone (no unit to write) */
static int map_list(const wkx_live *x, int t, const int *in, int n, int *out, int *dropped) {
    int k = 0;
    for (int i = 0; i < n; i++) { int v = in[i] >= 0 && in[i] < OURS.n[t] ? in[i] : -1; int m = v >= 0 ? x->map[t][v] : -1; if (m >= 0) out[k++] = m; else (*dropped)++; }
    return k;
}
static int to_live(const wkx_live *x, const wk_build *b, wk_build *o, int *dropped) {
    *o = *b;
    if (b->race < 0 || b->race >= WK_RACE_N || x->map[WT_RACE][b->race] < 0) return -1;
    o->race = x->map[WT_RACE][b->race];
    o->bg = b->bg >= 0 && b->bg < WK_BG_N ? x->map[WT_BG][b->bg] : -1; if (b->bg >= 0 && o->bg < 0) (*dropped)++;
    for (int i = 0; i < 6 && i < WK_MAJ_N; i++) {                                 /* majors may be re-ordered; tiers by key */
        int j = x->map[WT_MAJ][i], tr = b->tal[i] >= 0 && b->tal[i] < WK_TIER_N ? x->map[WT_TIER][b->tal[i]] : -1;
        o->tal[j] = tr >= 0 ? tr : 1;
        o->mb[j] = b->mb[i]; o->elix[j] = b->elix[i];
    }
    o->ninds = map_list(x, WT_IND, b->inds, b->ninds, o->inds, dropped);
    o->ntrees = map_list(x, WT_TREE, b->trees, b->ntrees, o->trees, dropped);
    o->nlearned = map_list(x, WT_SKILL, b->learned, b->nlearned, o->learned, dropped);
    o->ninsc = map_list(x, WT_INSC, b->insc, b->ninsc, o->insc, dropped);
    o->nsouls = map_list(x, WT_SOUL, b->souls, b->nsouls, o->souls, dropped);
    o->nsaved_inds = map_list(x, WT_IND, b->saved_inds, b->nsaved_inds, o->saved_inds, dropped);
    o->nsaved_insc = map_list(x, WT_INSC, b->saved_insc, b->nsaved_insc, o->saved_insc, dropped);
    o->nacq = map_list(x, WT_ACQ, b->acq, b->nacq, o->acq, dropped);
    for (int s = 0; s < WK_SLOTS; s++) { o->eq[s] = -1; o->ntag[s] = 0; }
    for (int s = 0; s < WK_SLOT_N && s < WK_SLOTS; s++) {
        int it = b->eq[s]; if (it < 0 || it >= WK_ITEM_N) continue;
        int ls = x->map[WT_SLOT][s], li = x->map[WT_ITEM][it];
        if (ls < 0 || ls >= WK_SLOTS || li < 0) { (*dropped)++; continue; }
        o->eq[ls] = li;
        const jv *tg = t_tags(&x->T, li);
        for (int k = 0; k < b->ntag[s] && k < 8; k++) {                         /* a bonus by its stat key in the live pool */
            int idx = b->tag[s][k].idx, m = -1;
            if (idx >= 0 && idx < WK_ITEM_NTAGS[it]) {
                const char *key = WK_TAG_KEY[WK_ITEM_TAG_AT[it] + idx];
                for (size_t j = 0; tg && tg->t == JN_ARR && j < tg->cnt; j++) {
                    const jv *t = tg->v[j];
                    if (t && t->t == JN_ARR && t->cnt >= 1 && t->v[0]->t == JN_STR && t->v[0]->n == strlen(key) && !memcmp(t->v[0]->s, key, t->v[0]->n)) { m = (int)j; break; }
                }
            }
            if (m < 0) { (*dropped)++; continue; }
            o->tag[ls][o->ntag[ls]].idx = m; o->tag[ls][o->ntag[ls]++].v = b->tag[s][k].v;
        }
    }
    o->portrait = -1;                                                             /* by its key in the race's live pool */
    if (b->portrait >= 0 && b->portrait < WK_PORTRAITS_N[b->race]) {
        const jv *pool = jget(x->T.ports, x->L[WT_RACE].k[o->race]);
        for (size_t j = 0; pool && pool->t == JN_ARR && j < pool->cnt; j++) {
            const jv *p = pool->v[j]; const char *key = WK_PORTRAITS[b->race][b->portrait];
            if (p->t == JN_STR && p->n == strlen(key) && !memcmp(p->s, key, p->n)) { o->portrait = (int)j; break; }
        }
        if (o->portrait < 0) (*dropped)++;
    }
    int k = 0;
    for (int i = 0; i < b->nman; i++) { int st = b->man[i].stat >= 0 && b->man[i].stat < WK_STAT_N ? x->map[WT_STAT][b->man[i].stat] : -1; if (st >= 0) { o->man[k].stat = st; o->man[k++].v = b->man[i].v; } else (*dropped)++; }
    o->nman = k; k = 0;
    for (int i = 0; i < b->npstk; i++) { int sk = b->pstk[i].skill >= 0 && b->pstk[i].skill < WK_SKILL_N ? x->map[WT_SKILL][b->pstk[i].skill] : -1; if (sk >= 0) { o->pstk[k].skill = sk; o->pstk[k++].st = b->pstk[i].st; } else (*dropped)++; }
    o->npstk = k;
    return 0;
}
int wkx_encode_live(const wkx_live *x, const wk_build *b, int n, const char *const *names, char *out, size_t outn, int *dropped) {
    int d = 0; if (dropped) *dropped = 0;
    if (!x) return enc_team(&OURS, b, n, names, out, outn);
    if (!b || n < 1 || n > WK_MAX_UNITS) return -1;
    wk_build *lb = malloc(sizeof *lb * (size_t)n); if (!lb) return -1;
    for (int i = 0; i < n; i++) if (to_live(x, &b[i], &lb[i], &d)) { free(lb); return -2; }
    int rc = enc_team(&x->T, lb, n, names, out, outn);
    free(lb);
    if (dropped) *dropped = d;
    return rc;
}
