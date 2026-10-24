// cm_look.c - the game's text look for Units Laboratory's kit widgets (interface in cm_look.h).
// What the game draws (font probe in a hidden test copy, build 25846389, English; the same roles in every language):
//   title column labels     Lora (EasyRead) + "Lora-Regular SDF OutLine16" (SoftOutline16), 22
//   Options window          title "Option" Cinzel (VeryDecorative) + its plain material, 40; section heads Cinzel +
//                           OutLine16, 18; buttons, tabs, labels, check box labels, choice values Lora + OutLine16, 14-16
//   Load Campaign list      Lora + OutLine16, 12; the save-name field Lora + plain material (the kit's fields already are)
// The kit (loader dsl_ui.c) gives body text the font's plain material and the title font's material to titles, so here:
// body text, buttons, check boxes and value rows -> body font + SoftOutline16; titles under 26 (section heads, card
// names) -> title font + SoftOutline16; window titles (26 and up) keep the title font's plain material, as "Option".
// Fields keep the kit's look. Font and material come from LanguageSetting by name (Mac and Windows), for the current
// language (zh-Hans, zh-Hant, ja have their own faces and materials); NULL from the game = the kit's look stays.
#include "cm_look.h"
#include "cm_il.h"
#include <string.h>

static const DSLHost *H; static DSLPlugin *ME; static const DSLUi *K;   /* K = the loader's own table */
static DSLUi L;                                                           /* the parts' table */
static int il = -1;
static cm_m m_font, m_mat, m_setfont, m_setmat, m_kids, m_size, m_sizemax;
static void *k_tmp;

enum { EASY_READ = 0, VERY_DECORATIVE = 2 };                  /* FontStyleType */
enum { MAT_DEFAULT = 0, MAT_SOFT_OUTLINE16 = 1 };             /* FontMaterialType */

static int il_ready(void) {
    if (il >= 0) return il;
    if (!cm_il_init(H, ME)) return 0;                         /* not up yet: try again at the next widget */
    m_font = cm_il_method("Refactor.Setting", "LanguageSetting", "GetFont", 1, NULL);
    m_mat = cm_il_method("Refactor.Setting", "LanguageSetting", "GetMaterial", 2, NULL);
    m_setfont = cm_il_method("TMPro", "TMP_Text", "set_font", 1, NULL);
    m_setmat = cm_il_method("TMPro", "TMP_Text", "set_fontSharedMaterial", 1, NULL);
    m_size = cm_il_method("TMPro", "TMP_Text", "set_fontSize", 1, NULL);
    m_sizemax = cm_il_method("TMPro", "TMP_Text", "set_fontSizeMax", 1, NULL);
    m_kids = cm_il_method("UnityEngine", "GameObject", "GetComponentsInChildren", 2, "System.Type,System.Boolean");
    k_tmp = cm_il_class("TMPro", "TMP_Text");
    il = m_font.fn && m_mat.fn && m_setfont.fn && m_setmat.fn && m_kids.fn && k_tmp;
    H->log(ME, "look: the game's text look %s", il ? "on" : "OFF (a LanguageSetting / TMP method is missing: the kit's look stays)");
    return il;
}

/* every TMP text of the widget (its own and its children's, hidden ones too) in the font of style with the material mat */
static void restyle(DSLObj w, int style, int mat) {
    if (!w || il_ready() != 1) return;
    Obj f = CM_CALL(m_font, Obj, int32_t, const void *)(style, m_font.mi);
    Obj m = CM_CALL(m_mat, Obj, int32_t, int32_t, const void *)(style, mat, m_mat.mi);
    if (!cm_alive(f) || !cm_alive(m)) return;
    static uintptr_t said[2][2];          /* log each font + material once (a language change: again); an address, never used */
    if (said[style == VERY_DECORATIVE][mat] != (uintptr_t)m) {
        said[style == VERY_DECORATIVE][mat] = (uintptr_t)m;
        static cm_m m_name; if (!m_name.fn) m_name = cm_il_method("UnityEngine", "Object", "get_name", 0, NULL);
        char fn[96] = "", mn[96] = "";
        if (m_name.fn) { cm_il_utf8(CM_CALL(m_name, Obj, Obj, const void *)(f, m_name.mi), fn, sizeof fn);
                         cm_il_utf8(CM_CALL(m_name, Obj, Obj, const void *)(m, m_name.mi), mn, sizeof mn); }
        H->log(ME, "look: %s texts: font \"%s\", material \"%s\"", style == VERY_DECORATIVE ? (mat ? "head" : "window title") : "body", fn, mn);
    }
    Obj g = cm_go((Obj)w); if (!g) return;
    Obj arr = CM_CALL(m_kids, Obj, Obj, Obj, bool, const void *)(g, cm_il_type_obj(k_tmp), true, m_kids.mi);
    if (!arr) return;
    uintptr_t n = CM_AT(arr, 0x18, uintptr_t);               /* Il2CppArray: max_length +0x18, items +0x20 */
    for (uintptr_t i = 0; i < n; i++) {
        Obj t = CM_AT(arr, 0x20 + i * sizeof(Obj), Obj);
        if (!cm_alive(t)) continue;
        CM_CALL(m_setfont, void, Obj, Obj, const void *)(t, f, m_setfont.mi);
        CM_CALL(m_setmat, void, Obj, Obj, const void *)(t, m, m_setmat.mi);
    }
}
static void body(DSLObj w) { restyle(w, EASY_READ, MAT_SOFT_OUTLINE16); }

static DSLObj l_text(DSLObj parent, float x, float y, float w, float h, const char *s, float size, int flags, uint32_t rgba) {
    DSLObj t = K->text(parent, x, y, w, h, s, size, flags, rgba);
    if (!(flags & DSL_TXT_TITLE)) body(t);
    else restyle(t, VERY_DECORATIVE, size >= 26 ? MAT_DEFAULT : MAT_SOFT_OUTLINE16);
    return t;
}
static DSLObj l_button(DSLObj parent, float x, float y, float w, float h, const char *label, dsl_ui_cb click, void *ud) {
    DSLObj b = K->button(parent, x, y, w, h, label, click, ud); body(b); return b;
}
static DSLObj l_toggle(DSLObj parent, float x, float y, float w, const char *label, int on, dsl_ui_cb change, void *ud) {
    DSLObj t = K->toggle(parent, x, y, w, label, on, change, ud); body(t); return t;
}
static DSLObj l_value_row(DSLObj parent, float x, float y, float w, const char *label, const char *const *values, int n,
                          int index, dsl_ui_cb change, void *ud) {
    DSLObj v = K->value_row(parent, x, y, w, label, values, n, index, change, ud); body(v); return v;
}

const DSLUi *cm_look_ui(const DSLHost *host, DSLPlugin *self) {
    if (K) return &L;
    const DSLUi *k = dsl_ui(host); if (!k) return NULL;
    H = host; ME = self;
    memset(&L, 0, sizeof L);
    memcpy(&L, k, k->size < sizeof L ? k->size : sizeof L);   /* an older loader's table: the calls it lacks stay NULL */
    L.size = k->size < sizeof L ? k->size : (uint32_t)sizeof L;
    if (L.text) L.text = l_text;
    if (L.button) L.button = l_button;
    if (L.toggle) L.toggle = l_toggle;
    if (L.value_row) L.value_row = l_value_row;
    K = k;
    return &L;
}

void cm_look_title_button(DSLObj b) {
    if (!b || il_ready() != 1 || !m_size.fn || !m_sizemax.fn) return;
    Obj t = cm_text_of((Obj)b); if (!t) return;
    CM_CALL(m_sizemax, void, Obj, float, const void *)(t, 22.f, m_sizemax.mi);   /* the kit's text fit may still shrink a long one */
    CM_CALL(m_size, void, Obj, float, const void *)(t, 22.f, m_size.mi);
}
