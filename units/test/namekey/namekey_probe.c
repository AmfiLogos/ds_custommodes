// namekey-probe: TEST ONLY module for namekey_test.py (never shipped). Reads the unit names the player sees, from a
// request file polled at StandaloneInputModule.UpdateModule (main thread): <DSRIG_DIR>/namekey_probe_req.txt, one line:
//   sel <i>      SelectPlayerUnitRequested(i): select the i-th party unit (a portrait click)
//   add <i>      ToggleSelectUnitRequested(i): Shift + portrait click (add the i-th party unit)
//   names        every QuickSlotUI (the stock bar + the Multi-unit view cards "QuickSlotUI_Multi<k>"): its unit and the
//                name text shown with it (a card: its name text; the stock bar: the unit panel's EntitySummaryUIView._txtName)
//   text <key>   TextKeyExtensions.GetText(key): the game's text of a key in the current language
// Each answer is dsload.log lines "namekey-probe: ...". Every game call by il2cpp name (multibar_probe.c in ds_modloader
// loader/test is the model).
#include "dsl_sdk.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

DSL_PLUGIN();
static const DSLHost *H;
static DSLPlugin *ME;
typedef void *Obj;
#define AT(o, off, T) (*(T *)((char *)(o) + (off)))
#define MC(mi, R, ...) ((R (*)(__VA_ARGS__))(*(void **)(mi)))

static char req_path[1024];
static void (*o_tick)(Obj, const void *);
static void (*add_event)(Obj, Obj, const void *);
static Obj (*obj_new)(void *), (*str_new)(const char *), (*tobj)(void *);
static void *(*ctype)(void *), *(*methods)(void *, void **);
static const char *(*mname)(void *);
static uint32_t (*pcount)(void *);
static uint8_t (*is_gen)(void *);
static void *mainflow_k, *qsui_k, *esv_k, *tmp_k;
static void *m_find_any, *m_find_all, *m_name, *m_go, *m_active_h, *m_tr, *m_tr_find, *m_get_comp, *m_get_text, *m_gettext;
static int32_t f_selected, f_txtname;

static void *msig(void *k, const char *name, int argc) {
    void *(*parent)(void *) = H->il2cpp_fn("il2cpp_class_get_parent");
    for (void *c = k; c; c = parent(c)) {
        void *it = NULL, *m;
        while ((m = methods(c, &it))) if (!strcmp(mname(m), name) && (int)pcount(m) == argc && !(is_gen && is_gen(m)) && *(void **)m) return m;
    }
    return NULL;
}
static void guid_str(const uint8_t *g, char *out) {           /* .NET Guid layout -> the usual text */
    snprintf(out, 40, "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x", g[3], g[2], g[1], g[0], g[5], g[4], g[7], g[6],
             g[8], g[9], g[10], g[11], g[12], g[13], g[14], g[15]);
}
static void u8(Obj s, char *out, size_t cap) {                /* System.String (UTF-16) -> UTF-8; newlines as spaces */
    size_t k = 0; int32_t n = s ? AT(s, 0x10, int32_t) : 0; const uint16_t *w = (const uint16_t *)((char *)s + 0x14);
    for (int32_t i = 0; i < n && k + 5 < cap; i++) {
        uint32_t c = w[i];
        if (c >= 0xD800 && c < 0xDC00 && i + 1 < n) { c = 0x10000 + ((c - 0xD800) << 10) + (w[i + 1] - 0xDC00); i++; }
        if (c == '\n' || c == '\r') c = ' ';
        if (c < 0x80) out[k++] = (char)c;
        else if (c < 0x800) { out[k++] = (char)(0xC0 | c >> 6); out[k++] = (char)(0x80 | (c & 63)); }
        else if (c < 0x10000) { out[k++] = (char)(0xE0 | c >> 12); out[k++] = (char)(0x80 | (c >> 6 & 63)); out[k++] = (char)(0x80 | (c & 63)); }
        else { out[k++] = (char)(0xF0 | c >> 18); out[k++] = (char)(0x80 | (c >> 12 & 63)); out[k++] = (char)(0x80 | (c >> 6 & 63)); out[k++] = (char)(0x80 | (c & 63)); }
    }
    out[k] = 0;
}
static Obj go_of(Obj c) { return c ? MC(m_go, Obj, Obj, void *)(c, m_go) : NULL; }
static int active_h(Obj c) { Obj g = go_of(c); return g ? MC(m_active_h, uint8_t, Obj, void *)(g, m_active_h) & 1 : 0; }
static void obj_name(Obj o, char *out, size_t cap) { out[0] = 0; if (o) u8(MC(m_name, Obj, Obj, void *)(o, m_name), out, cap); }
static void tmp_text(Obj t, char *out, size_t cap) { out[0] = 0; if (t) u8(MC(m_get_text, Obj, Obj, void *)(t, m_get_text), out, cap); }

static void do_names(void) {
    Obj esv = MC(m_find_any, Obj, Obj, int32_t, void *)(tobj(ctype(esv_k)), 1, m_find_any);
    Obj stock = esv ? AT(esv, f_txtname, Obj) : NULL;          /* the unit panel's name text */
    char sname[96] = "", clone[112] = "";
    obj_name(go_of(stock), sname, sizeof sname); snprintf(clone, sizeof clone, "%s(Clone)", sname);   /* multi-bar clones it per card */
    Obj arr = MC(m_find_all, Obj, Obj, uint8_t, void *)(tobj(ctype(qsui_k)), 1, m_find_all);
    int n = arr ? (int)AT(arr, 0x18, uintptr_t) : 0;
    for (int i = 0; i < n; i++) {
        Obj v = AT(arr, 0x20 + 8 * i, Obj); if (!v) continue;
        char nm[64], g[40], txt[400] = ""; obj_name(go_of(v), nm, sizeof nm); guid_str((uint8_t *)v + f_selected, g);
        int card = !strncmp(nm, "QuickSlotUI_Multi", 17);
        if (card) {
            Obj panel = MC(m_tr_find, Obj, Obj, Obj, void *)(MC(m_tr, Obj, Obj, void *)(v, m_tr), str_new("Panel_QuickSlot"), m_tr_find);
            Obj ttr = panel ? MC(m_tr_find, Obj, Obj, Obj, void *)(panel, str_new(clone), m_tr_find) : NULL;
            Obj t = ttr ? MC(m_get_comp, Obj, Obj, Obj, void *)(ttr, tobj(ctype(tmp_k)), m_get_comp) : NULL;
            if (t) tmp_text(t, txt, sizeof txt); else snprintf(txt, sizeof txt, "<no %s>", clone);
        } else tmp_text(stock, txt, sizeof txt);
        H->log(ME, "namekey-probe: bar [%s active=%d unit=%s text_active=%d] text=%s", nm, active_h(v), g, card ? 1 : active_h(stock), txt);
    }
    H->log(ME, "namekey-probe: names done (%d bars, stock text object '%s')", n, sname);
}

static Obj new_event(const char *name, int idx) {
    void *k = H->find_class("Refactor.Main.Event", name);
    void *it = NULL, *m, *mi = NULL;
    void *(*param)(void *, uint32_t) = H->il2cpp_fn("il2cpp_method_get_param");
    char *(*tname)(void *) = H->il2cpp_fn("il2cpp_type_get_name");
    while (k && (m = methods(k, &it))) if (!strcmp(mname(m), ".ctor") && pcount(m) == 1 && !strcmp(tname(param(m, 0)), "System.Int32")) mi = m;
    if (!mi) { H->log(ME, "namekey-probe: %s(int) not found", name); return NULL; }
    Obj ev = obj_new(k);
    MC(mi, void, Obj, int32_t, void *)(ev, idx, mi);
    return ev;
}

static void run_req(Obj eq, const char *line) {
    char cmd[32] = "", arg[200] = ""; int a = 0;
    sscanf(line, "%31s %199s", cmd, arg); a = atoi(arg);
    if (!strcmp(cmd, "names")) { do_names(); return; }
    if (!strcmp(cmd, "text")) {
        char t[400]; u8(MC(m_gettext, Obj, Obj, void *)(str_new(arg), m_gettext), t, sizeof t);
        H->log(ME, "namekey-probe: text %s = %s", arg, t); return;
    }
    Obj ev = NULL;
    if (!strcmp(cmd, "sel")) ev = new_event("SelectPlayerUnitRequested", a);
    else if (!strcmp(cmd, "add")) ev = new_event("ToggleSelectUnitRequested", a);
    else { H->log(ME, "namekey-probe: unknown request '%s'", line); return; }
    if (ev) add_event(eq, ev, NULL);
    H->log(ME, "namekey-probe: %s: %s", line, ev ? "sent" : "FAILED");
}

static unsigned frames;
static void my_tick(Obj self, const void *mi) {
    o_tick(self, mi);
    if (++frames % 10) return;
    FILE *f = fopen(req_path, "r");
    if (!f) return;
    char line[256] = ""; if (!fgets(line, sizeof line, f)) line[0] = 0; fclose(f);
    line[strcspn(line, "\r\n")] = 0;
    if (!line[0]) return;
    if ((f = fopen(req_path, "w"))) fclose(f);                 /* emptied = done */
    Obj mf = MC(m_find_any, Obj, Obj, int32_t, void *)(tobj(ctype(mainflow_k)), 0, m_find_any);
    int32_t o_eq = H->field_offset(mainflow_k, "_eventQueue");
    Obj eq = mf && o_eq >= 0 ? AT(mf, o_eq, Obj) : NULL;
    if (!eq) { H->log(ME, "namekey-probe: no MainFlow / EventQueue (%d)", o_eq); return; }
    run_req(eq, line);
}

DSL_EXPORT int dsl_plugin_init(const DSLHost *host, DSLPlugin *self) {
    H = host; ME = self;
    const char *d = getenv("DSRIG_DIR");
    if (!d) { H->log(ME, "namekey-probe: no DSRIG_DIR (test copies only)"); return 1; }
    snprintf(req_path, sizeof req_path, "%s/namekey_probe_req.txt", d);
    *(void **)&obj_new = H->il2cpp_fn("il2cpp_object_new"); *(void **)&str_new = H->il2cpp_fn("il2cpp_string_new");
    *(void **)&tobj = H->il2cpp_fn("il2cpp_type_get_object"); *(void **)&ctype = H->il2cpp_fn("il2cpp_class_get_type");
    *(void **)&methods = H->il2cpp_fn("il2cpp_class_get_methods"); *(void **)&mname = H->il2cpp_fn("il2cpp_method_get_name");
    *(void **)&pcount = H->il2cpp_fn("il2cpp_method_get_param_count"); *(void **)&is_gen = H->il2cpp_fn("il2cpp_method_is_generic");
    *(void **)&add_event = H->find_method_address("Refactor.Main", "EventQueue", "AddEvent", 1);
    mainflow_k = H->find_class("Refactor.Main", "MainFlow"); qsui_k = H->find_class("Refactor.UI", "QuickSlotUI");
    esv_k = H->find_class("Refactor.UI", "EntitySummaryUIView"); tmp_k = H->find_class("TMPro", "TMP_Text");
    void *ko = H->find_class("UnityEngine", "Object"), *kc = H->find_class("UnityEngine", "Component"), *kg = H->find_class("UnityEngine", "GameObject");
    void *kt = H->find_class("UnityEngine", "Transform");
    m_find_any = msig(ko, "FindAnyObjectByType", 2); m_find_all = msig(ko, "FindObjectsOfType", 2); m_name = msig(ko, "get_name", 0);
    m_active_h = msig(kg, "get_activeInHierarchy", 0); m_get_comp = msig(kc, "GetComponent", 1); m_go = msig(kc, "get_gameObject", 0);
    m_tr = msig(kc, "get_transform", 0); m_tr_find = msig(kt, "Find", 1); m_get_text = msig(tmp_k, "get_text", 0);
    m_gettext = msig(H->find_class("Refactor", "TextKeyExtensions"), "GetText", 1);
    f_selected = H->field_offset(qsui_k, "_selected"); f_txtname = H->field_offset(esv_k, "_txtName");
    if (!obj_new || !add_event || !mainflow_k || !qsui_k || !esv_k || !tmp_k || !m_find_any || !m_find_all || !m_get_comp || !m_tr_find
        || !m_get_text || !m_gettext || f_selected < 0 || f_txtname < 0) {
        H->log(ME, "namekey-probe: names missing"); return 4;
    }
    if (H->hook_method(ME, "UnityEngine.EventSystems", "StandaloneInputModule", "UpdateModule", 0, (void *)my_tick, (void **)&o_tick)) return 3;
    H->log(ME, "namekey-probe: ready (%s)", req_path);
    return 0;
}
