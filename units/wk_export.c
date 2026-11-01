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
static int enc_build(wr *w, const wk_build *b, const char *name) {
    if (b->race < 0 || b->race >= WK_RACE_N) return -1;
    wu(w, (uint32_t)b->race);
    wu(w, b->bg >= 0 && b->bg < WK_BG_N ? (uint32_t)b->bg + 1 : 0);
    wu(w, (uint32_t)(b->lvl < 1 ? 1 : b->lvl > 255 ? 255 : b->lvl));
    uint32_t tp = 0; for (int i = 0; i < 6; i++) tp = tp * 5 + (uint32_t)(b->tal[i] >= 0 && b->tal[i] < WK_TIER_N ? b->tal[i] : 1);
    wu(w, tp);
    const char *nm; size_t nl = name_of(name ? name : b->name, &nm);
    int elix = 0, cond = 0;
    for (int k = 0; k < 6; k++) if (b->has_elix && b->elix[k] > 0) elix = 1;
    for (int k = 0; k < b->ncond && k < 8; k++) if (b->cond[k] > 0) cond = 1;
    int port = b->portrait >= 0 && b->portrait < WK_PORTRAITS_N[b->race];
    uint32_t fl = (uint32_t)(b->offer & 3) | (uint32_t)(b->roll & 3) << 2 | (uint32_t)(b->advanced ? 1 : 0) << 4 | (uint32_t)(b->add_shield ? 1 : 0) << 5
        | (uint32_t)(nl > 0) << 6 | (uint32_t)port << 7 | (uint32_t)(b->nman > 0) << 8 | (uint32_t)(b->has_mb ? 1 : 0) << 9
        | (uint32_t)(b->npstk > 0) << 10 | (uint32_t)(b->nsouls > 0) << 11 | (uint32_t)(b->nsaved_inds > 0) << 12 | (uint32_t)(b->nsaved_insc > 0) << 13
        | (uint32_t)(b->nacq > 0) << 14 | (uint32_t)(b->hide_head ? 1 : 0) << 15 | (uint32_t)elix << 16 | (uint32_t)cond << 17;
    wu(w, fl);
    wlist(w, b->inds, b->ninds, WK_IND_N);
    wlist(w, b->trees, b->ntrees, WK_TREE_N);
    wlist(w, b->learned, b->nlearned, WK_SKILL_N);
    wlist(w, b->insc, b->ninsc, WK_INSC_N);
    uint32_t eqmask = 0;
    for (int s = 0; s < WK_SLOT_N && s < WK_SLOTS; s++) if (b->eq[s] >= 0 && b->eq[s] < WK_ITEM_N) eqmask |= 1u << s;
    wu(w, eqmask);
    for (int s = 0; s < WK_SLOT_N && s < WK_SLOTS; s++) if (eqmask >> s & 1) wu(w, (uint32_t)b->eq[s]);
    /* a tag the item lacks is dropped, as the encoder does */
    uint32_t tagmask = 0; int ntag[WK_SLOTS] = { 0 };
    for (int s = 0; s < WK_SLOT_N && s < WK_SLOTS; s++) if (eqmask >> s & 1) {
        for (int k = 0; k < b->ntag[s] && k < 8; k++) if (b->tag[s][k].idx >= 0 && b->tag[s][k].idx < WK_ITEM_NTAGS[b->eq[s]]) ntag[s]++;
        if (ntag[s]) tagmask |= 1u << s;
    }
    wu(w, tagmask);
    for (int s = 0; s < WK_SLOT_N && s < WK_SLOTS; s++) if (tagmask >> s & 1) {
        wu(w, (uint32_t)ntag[s]);
        int it = b->eq[s];
        for (int k = 0; k < b->ntag[s] && k < 8; k++) {
            int idx = b->tag[s][k].idx; if (idx < 0 || idx >= WK_ITEM_NTAGS[it]) continue;
            double lo = WK_TAG_RANGE[2 * (WK_ITEM_TAG_AT[it] + idx)], hi = WK_TAG_RANGE[2 * (WK_ITEM_TAG_AT[it] + idx) + 1];
            long fr = hi > lo ? jround((b->tag[s][k].v - lo) / (hi - lo) * 255) : 0;
            wu(w, (uint32_t)idx); wbyte(w, (int)(fr < 0 ? 0 : fr > 255 ? 255 : fr));
        }
    }
    if (nl) { wu(w, (uint32_t)nl); for (size_t k = 0; k < nl; k++) wbyte(w, (unsigned char)nm[k]); }
    if (port) wu(w, (uint32_t)b->portrait);
    if (b->nman > 0) {
        int k = 0; for (int i = 0; i < b->nman; i++) if (b->man[i].stat >= 0 && b->man[i].stat < WK_STAT_N) k++;
        wu(w, (uint32_t)k);
        for (int i = 0; i < b->nman; i++) if (b->man[i].stat >= 0 && b->man[i].stat < WK_STAT_N) { wu(w, (uint32_t)b->man[i].stat); wzz(w, (int)jround(b->man[i].v * 100.0)); }
    }
    if (b->has_mb) for (int k = 0; k < 6; k++) wzz(w, (int)jround(b->mb[k] * 100.0));
    if (b->npstk > 0) {
        int k = 0; for (int i = 0; i < b->npstk; i++) if (b->pstk[i].skill >= 0 && b->pstk[i].skill < WK_SKILL_N && b->pstk[i].st >= 0) k++;
        wu(w, (uint32_t)k);
        for (int i = 0; i < b->npstk; i++) if (b->pstk[i].skill >= 0 && b->pstk[i].skill < WK_SKILL_N && b->pstk[i].st >= 0) { wu(w, (uint32_t)b->pstk[i].skill); wu(w, (uint32_t)b->pstk[i].st); }
    }
    if (b->nsouls > 0) wlist(w, b->souls, b->nsouls, WK_SOUL_N);
    if (b->nsaved_inds > 0) wlist(w, b->saved_inds, b->nsaved_inds, WK_IND_N);
    if (b->nsaved_insc > 0) wlist(w, b->saved_insc, b->nsaved_insc, WK_INSC_N);
    if (b->nacq > 0) wlist(w, b->acq, b->nacq, WK_ACQ_N);
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
int wk_encode_named(const wk_build *b, int n, const char *const *names, char *out, size_t outn) {
    if (!b || n < 1 || n > WK_MAX_UNITS || !out || !outn) return -1;
    wr w = { 0 };
    wbyte(&w, 1); wbyte(&w, (WK_FP >> 8) & 0xff); wbyte(&w, WK_FP & 0xff); wu(&w, (uint32_t)n);
    for (int i = 0; i < n; i++) if (enc_build(&w, &b[i], names ? names[i] : NULL)) { free(w.b); return -1; }
    if (w.bad) { free(w.b); return -1; }
    /* v1 (the raw blob, version 1): every builder reads it (builder.js decodeTeam tries it first). Not v2: the Windows
       module has zlib's inflate only (ds_multiplayer win/build_win.sh zl_*), and v1 keeps both platforms' links the same */
    int rc = b64url(w.b, w.n, out, outn);
    free(w.b);
    return rc;
}

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
int wk_export_url(const wk_build *b, int n, const char *const *names, const char *team_name, int pub, const char *lang, char *out, size_t outn) {
    char code[8192], tn[400] = "";
    if (wk_encode_named(b, n, names, code, sizeof code)) return -1;
    if (team_name && urlenc(team_name, tn, sizeof tn) == (size_t)-1) tn[0] = 0;
    const char *l = lang && *lang && strcmp(lang, "en") ? lang : "";
    int k = snprintf(out, outn, "%s%s%s/builder?team=%s&from=game&pub=%d%s%s&gx=%016llx", WKX_HOST, *l ? "/" : "", l, code,
                     pub ? 1 : 0, tn[0] ? "&tn=" : "", tn, (unsigned long long)wk_hash(b, n));
    return k > 0 && (size_t)k < outn ? 0 : -1;
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

/* ---- the live wiki's tables (WKX_CODEC_PATH, the builder's own index source, as builder.js dsbIdx builds them) vs ours.
   Our tables (pvp_wiki_tables.h) are made from the wiki data when Units Laboratory is built; the wiki changes its data on
   a game update. The link carries INDEXES, and the wiki's fingerprint only counts the table sizes, so a renamed key would
   load another item / skill without a warning: the export goes out only when every list is the same, key for key. */
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
static int kl_same(const klist *l, const char *const *t, int tn, const char *what, char *err, size_t errn) {
    for (int i = 0; i < l->n || i < tn; i++) {
        const char *a = i < l->n ? l->k[i] : "(none)", *b = i < tn ? t[i] : "(none)";
        if (strcmp(a, b)) { snprintf(err, errn, "%s %d: the wiki has %s, this version %s", what, i, a, b); return 0; }
    }
    return 1;
}
int wkx_codec_check(const char *text, size_t n, char *err, size_t errn) {
    if (err && errn) err[0] = 0;
    const char *p = text ? strstr(text, "var D = ") : NULL;
    if (!p) { snprintf(err, errn, "the wiki's builder tables were not found"); return -1; }
    p += 8;
    /* the object ends at the first "};\n" or ";\nvar " after it: parse the longest prefix that is one JSON value */
    const char *e = strstr(p, ";\nvar ");
    if (!e) { snprintf(err, errn, "the wiki's builder tables were not found"); return -1; }
    char pe[200]; jv *D = jparse(p, (size_t)(e - p), pe, sizeof pe);
    if (!D || D->t != JN_OBJ) { snprintf(err, errn, "the wiki's builder tables do not read: %s", pe); if (D) jfree(D); return -1; }
    const jv *RET = jget(D, "retired");
    klist L = { 0 }; int ok = 1; char w[64];
#define LIST(build, tab, tn, what) do { if (ok) { build; ok = kl_same(&L, tab, tn, what, err, errn); } kl_free(&L); } while (0)
    LIST((kl_field(&L, jget(D, "races"), "k"), ({ const jv *r = jget(RET, "races"); for (size_t i = 0; r && r->t == JN_OBJ && i < r->cnt; i++) { char t[96]; snprintf(t, sizeof t, "%.*s", (int)r->kn[i], r->k[i]); if (!kl_has(&L, t)) kl_add(&L, r->k[i], r->kn[i]); } }), kl_sort(&L)), WK_RACE, WK_RACE_N, "race");
    LIST((kl_keys(&L, jget(D, "bg")), kl_sort(&L)), WK_BG, WK_BG_N, "background");
    LIST((kl_keys(&L, jget(D, "ind")), ({ const jv *r = jget(RET, "ind"); for (size_t i = 0; r && r->t == JN_OBJ && i < r->cnt; i++) { char t[96]; snprintf(t, sizeof t, "%.*s", (int)r->kn[i], r->k[i]); if (!kl_has(&L, t)) kl_add(&L, r->k[i], r->kn[i]); } }), kl_sort(&L)), WK_IND, WK_IND_N, "trait");
    LIST((kl_field(&L, jget(D, "insc"), "k"), kl_sort(&L)), WK_INSC, WK_INSC_N, "inscription");
    LIST((kl_keys(&L, jget(D, "trees")), kl_sort(&L)), WK_TREE, WK_TREE_N, "skill tree");
    LIST((kl_keys(&L, jget(D, "skills")), kl_sort(&L)), WK_SKILL, WK_SKILL_N, "skill");
    LIST((kl_keys(&L, jget(D, "items")), kl_sort(&L)), WK_ITEM, WK_ITEM_N, "item");
    LIST((kl_field(&L, jget(D, "sv"), "k"), kl_sort(&L)), WK_STAT, WK_STAT_N, "stat");
    LIST(kl_strs(&L, jget(D, "slots")), WK_SLOT, WK_SLOT_N, "equipment slot");
    LIST((kl_keys(&L, jget(D, "souls")), kl_sort(&L)), WK_SOUL, WK_SOUL_N, "soul stone");
    LIST(kl_strs(&L, jget(D, "majors")), WK_MAJ, WK_MAJ_N, "major stat");
    LIST(kl_strs(&L, jget(jget(D, "tiers"), "order")), WK_TIER, WK_TIER_N, "talent tier");
    /* ACQ: D.acqOrder (the ones D.acq has) first, then the other D.acq keys sorted (dsbIdx ACQKEYS) */
    if (ok) {
        klist all = { 0 }; kl_field(&all, jget(D, "acq"), "k");
        const jv *o = jget(D, "acqOrder");
        for (size_t i = 0; o && o->t == JN_ARR && i < o->cnt; i++) if (o->v[i]->t == JN_STR) { char t[96]; snprintf(t, sizeof t, "%.*s", (int)o->v[i]->n, o->v[i]->s); if (kl_has(&all, t) && !kl_has(&L, t)) kl_add(&L, t, strlen(t)); }
        kl_sort(&all);
        for (int i = 0; i < all.n; i++) if (!kl_has(&L, all.k[i])) kl_add(&L, all.k[i], strlen(all.k[i]));
        ok = kl_same(&L, WK_ACQ, WK_ACQ_N, "acquired trait", err, errn);
        kl_free(&all); kl_free(&L);
    }
    /* portraits per race (the link carries the index in the race's pool) */
    for (int r = 0; ok && r < WK_RACE_N; r++) {
        snprintf(w, sizeof w, "portrait of %s", WK_RACE[r]);
        LIST(kl_strs(&L, jget(jget(D, "portraits"), WK_RACE[r])), WK_PORTRAITS[r], WK_PORTRAITS_N[r], w);
    }
    /* each item's rolled bonuses: index and range (the link carries the index and a 1/255 step of the range) */
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
#undef LIST
    jfree(D);
    return ok ? 0 : -1;
}
