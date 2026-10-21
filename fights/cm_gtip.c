/* cm_gtip.c - see cm_gtip.h. Same game calls as the loader kit's tooltip (loader src/dsl_ui.c tip_show_at), with the
 * entry's own type and key instead of a stock ETC card whose texts are replaced. Names checked in the 25846389 dump:
 * Refactor.UI.TooltipUI.ShowTooltipAtScreenPosition(Guid, TooltipType, string, Vector2, TalentType, TooltipContext),
 * HideToolTip(Guid); TooltipContext.<HideHoldLayout>k__BackingField. */
#include "cm_gtip.h"
#include "cm_win.h"
#include "cm_il.h"
#include <stdio.h>
#include <string.h>

typedef struct { uint64_t lo, hi; } Guid;
typedef struct { float x, y; } V2;
typedef struct { float x, y, z; } V3;
static const Guid TIP_ID = { 0x7069746d63ull, 0x0000000000000005ull };   /* Custom Modes' own card session */
#define NT 16
static struct { uintptr_t h; int type; char key[96]; } T[NT]; static int nt;
static uintptr_t h_view, h_shown, h_test, h_arr;
static int shown, inited, ok;
static char shown_key[96];
static void *k_tui, *k_canvas, *k_v3;
static cm_m m_show, m_hide, m_mouse, m_corners, m_inparent, m_root, m_mode, m_cam, m_w2s, m_active, m_addcomp, m_getcomp_go, m_override, m_order;
static void *k_raycaster, *k_mask2d, *k_tbase;
static cm_m m_finds, m_tb_id, m_scr_w;
static int32_t ctx_size, f_hidehold = -1;
static Obj (*array_new)(void *klass, uintptr_t n);

static void tick(void *ud);
static void init(void) {
    if (inited) return;
    inited = 1;
    const DSLHost *H = cmw_host(); if (!H) return;
    k_tui = cm_il_class("Refactor.UI", "TooltipUI");
    m_show = cm_il_method("Refactor.UI", "TooltipUI", "ShowTooltipAtScreenPosition", 6, NULL);
    m_hide = cm_il_method("Refactor.UI", "TooltipUI", "HideToolTip", 1, NULL);
    void *k_ctx = cm_il_class("Refactor.UI", "TooltipContext");
    uint32_t (*value_size)(void *, uint32_t *) = (uint32_t (*)(void *, uint32_t *))H->il2cpp_fn("il2cpp_class_value_size");
    ctx_size = k_ctx && value_size ? (int32_t)value_size(k_ctx, NULL) : 0;
    int32_t o = cm_il_field(k_ctx, "<HideHoldLayout>k__BackingField"); f_hidehold = o >= 0x10 ? o - 0x10 : -1;   /* value type: minus the header */
    *(void **)&array_new = H->il2cpp_fn("il2cpp_array_new");
    k_v3 = cm_il_class("UnityEngine", "Vector3"); k_canvas = cm_il_class("UnityEngine", "Canvas");
    m_mouse = cm_il_method("UnityEngine", "Input", "get_mousePosition", 0, NULL);
    m_corners = cm_il_method("UnityEngine", "RectTransform", "GetWorldCorners", 1, NULL);
    m_inparent = cm_il_method("UnityEngine", "Component", "GetComponentInParent", 1, "System.Type");
    m_root = cm_il_method("UnityEngine", "Canvas", "get_rootCanvas", 0, NULL);
    m_mode = cm_il_method("UnityEngine", "Canvas", "get_renderMode", 0, NULL);
    m_cam = cm_il_method("UnityEngine", "Canvas", "get_worldCamera", 0, NULL);
    m_w2s = cm_il_method("UnityEngine", "RectTransformUtility", "WorldToScreenPoint", 2, "UnityEngine.Camera,UnityEngine.Vector3");
    m_active = cm_il_method("UnityEngine", "GameObject", "get_activeInHierarchy", 0, NULL);
    m_addcomp = cm_il_method("UnityEngine", "GameObject", "AddComponent", 1, "System.Type");
    m_getcomp_go = cm_il_method("UnityEngine", "GameObject", "GetComponent", 1, "System.Type");
    m_override = cm_il_method("UnityEngine", "Canvas", "set_overrideSorting", 1, NULL);
    m_order = cm_il_method("UnityEngine", "Canvas", "set_sortingOrder", 1, NULL);
    k_raycaster = cm_il_class("UnityEngine.UI", "GraphicRaycaster");
    k_mask2d = cm_il_class("UnityEngine.UI", "RectMask2D"); k_tbase = cm_il_class("Refactor.UI", "Tooltip_Base");
    m_finds = cm_il_method("UnityEngine", "Object", "FindObjectsOfType", 1, "System.Type");
    m_tb_id = cm_il_method("Refactor.UI", "Tooltip_Base", "get_TooltipID", 0, NULL);
    m_scr_w = cm_il_method("UnityEngine", "Screen", "get_width", 0, NULL);
    ok = k_tui && m_show.fn && m_hide.fn && ctx_size > 0 && ctx_size <= 0xC0 && f_hidehold >= 0 && f_hidehold < ctx_size &&
         array_new && k_v3 && m_mouse.fn && m_corners.fn;
    cmw_log("tf: game hover cards %s (TooltipContext %#x, HideHoldLayout %#x)", ok ? "on" : "OFF", ctx_size, f_hidehold);
    if (ok) cmw_tick_add(tick, NULL);
}

/* screen rect x0,y0,x1,y1 in pixels (overlay canvas: world = pixels; camera canvas: through its camera) */
static int rect_screen(Obj rt, float r[4]) {
    Obj arr = cm_il_hget(h_arr);
    if (!arr) { arr = array_new(k_v3, 4); if (!arr) return 0; h_arr = cm_il_hnew(arr); }
    CM_CALL(m_corners, void, Obj, Obj, const void *)(rt, arr, m_corners.mi);
    V3 *c = (V3 *)((char *)arr + 0x20);
    Obj cv = k_canvas && m_inparent.fn ? CM_CALL(m_inparent, Obj, Obj, Obj, const void *)(rt, cm_il_type_obj(k_canvas), m_inparent.mi) : NULL;
    Obj root = cm_alive(cv) && m_root.fn ? CM_CALL(m_root, Obj, Obj, const void *)(cv, m_root.mi) : NULL;
    Obj cam = cm_alive(root) && m_mode.fn && CM_CALL(m_mode, int, Obj, const void *)(root, m_mode.mi) != 0 && m_cam.fn ? CM_CALL(m_cam, Obj, Obj, const void *)(root, m_cam.mi) : NULL;
    for (int i = 0; i < 4; i++) if (cm_alive(cam) && m_w2s.fn) { V2 s = CM_CALL(m_w2s, V2, Obj, V3, const void *)(cam, c[i], m_w2s.mi); c[i].x = s.x; c[i].y = s.y; }
    r[0] = r[2] = c[0].x; r[1] = r[3] = c[0].y;
    for (int i = 1; i < 4; i++) { if (c[i].x < r[0]) r[0] = c[i].x; if (c[i].x > r[2]) r[2] = c[i].x; if (c[i].y < r[1]) r[1] = c[i].y; if (c[i].y > r[3]) r[3] = c[i].y; }
    return 1;
}
static Obj view(void) {
    Obj v = cm_il_hget(h_view);
    if (!cm_alive(v)) { v = cm_find(k_tui, true); cm_il_hset(&h_view, v); }
    return cm_alive(v) ? v : NULL;
}
/* the card above the Custom Modes windows (as the loader kit's own tooltip: loader dsl_ui.c raise_view, order 32050) */
static void raise_view(Obj v) {
    if (!k_canvas || !m_inparent.fn || !m_override.fn || !m_order.fn) return;
    Obj cv = CM_CALL(m_inparent, Obj, Obj, Obj, const void *)(cm_tr(v), cm_il_type_obj(k_canvas), m_inparent.mi);
    if (!cm_alive(cv)) return;
    Obj root = m_root.fn ? CM_CALL(m_root, Obj, Obj, const void *)(cv, m_root.mi) : NULL;
    if (root == cv) {                                                  /* the view sits in the root canvas: its own */
        Obj g = cm_go(v); if (!g || !m_getcomp_go.fn || !m_addcomp.fn) return;
        cv = CM_CALL(m_getcomp_go, Obj, Obj, Obj, const void *)(g, cm_il_type_obj(k_canvas), m_getcomp_go.mi);
        if (!cm_alive(cv)) cv = CM_CALL(m_addcomp, Obj, Obj, Obj, const void *)(g, cm_il_type_obj(k_canvas), m_addcomp.mi);
        if (k_raycaster && !cm_alive(CM_CALL(m_getcomp_go, Obj, Obj, Obj, const void *)(g, cm_il_type_obj(k_raycaster), m_getcomp_go.mi)))
            CM_CALL(m_addcomp, Obj, Obj, Obj, const void *)(g, cm_il_type_obj(k_raycaster), m_addcomp.mi);
        if (!cm_alive(cv)) return;
    }
    CM_CALL(m_override, void, Obj, bool, const void *)(cv, true, m_override.mi);
    CM_CALL(m_order, void, Obj, int, const void *)(cv, 32050, m_order.mi);
}
/* the mouse really sees the target there: inside every RectMask2D above it (a scrolled list's viewport: a row scrolled
   out of sight showed its card) and under no later sibling of its parents (the Gallery window opened over Test Fights).
   Stops at the canvas: above it are no UI rects. */
static int on_tree(Obj rt);
static int in_r(const float r[4], float x, float y) { return x >= r[0] && x <= r[2] && y >= r[1] && y <= r[3]; }
static int visible_at(Obj t, float mx, float my) {
    for (Obj a = t; cm_alive(a) && !(k_canvas && cm_comp(a, k_canvas)); ) {
        Obj p = cm_parent(a); float r[4];
        if (!p) break;
        if (k_mask2d && cm_comp(p, k_mask2d) && rect_screen(p, r) && !in_r(r, mx, my)) return 0;
        for (int i = cm_sibling(a) + 1, n = cm_child_count(p); i < n; i++) {
            Obj c = cm_child(p, i);
            if (on_tree(c) && rect_screen(c, r) && in_r(r, mx, my)) return 0;
        }
        a = p;
    }
    return 1;
}
/* placement as the loader kit's tooltips (owner 2026-10-16 #3): outside the panel the target sits in (the widest
   parent narrower than 3/4 of the screen: the window), at its right edge, top level with the target's top; no room on
   the right -> outside its left edge. Never over the window. The card's size is known a few frames after Show. */
static float tp_panel[4], tp_top; static int tp_check, tp_side, tp_i;
static float screen_w(void) { return m_scr_w.fn ? (float)CM_CALL(m_scr_w, int, const void *)(m_scr_w.mi) : 1920.f; }
static void panel_of(Obj target, const float el[4], float out[4]) {
    memcpy(out, el, 4 * sizeof *out);
    float sw = screen_w(); Obj t = target;
    for (int d = 0; d < 16 && t; d++) {
        t = cm_parent(t); float r[4];
        if (!t || !rect_screen(t, r) || r[2] - r[0] >= sw * 0.75f) break;
        if (r[0] <= el[0] + 1 && r[2] >= el[2] - 1 && r[2] - r[0] >= out[2] - out[0]) memcpy(out, r, sizeof r);
    }
}
static int card_rect(float r[4]) {                                    /* the shown card: the Tooltip_Base with our id */
    if (!k_tbase || !m_finds.fn || !m_tb_id.fn) return 0;
    Obj arr = CM_CALL(m_finds, Obj, Obj, const void *)(cm_il_type_obj(k_tbase), m_finds.mi);
    int n = arr ? (int)*(uintptr_t *)((char *)arr + 0x18) : 0;
    for (int i = 0; i < n; i++) {
        Obj c = *(Obj *)((char *)arr + 0x20 + (size_t)i * 8);
        Guid g = CM_CALL(m_tb_id, Guid, Obj, const void *)(c, m_tb_id.mi);
        if (g.lo == TIP_ID.lo && g.hi == TIP_ID.hi) return rect_screen(cm_tr(c), r);
    }
    return 0;
}
static void show_at(int i, V2 pos) {
    Obj v = view(); if (!v) return;
    struct { uint8_t b[0xC0]; } ctx; memset(&ctx, 0, sizeof ctx); ctx.b[f_hidehold] = 1;   /* no "hold for more" part */
    CM_CALL(m_show, void, Obj, Guid, int, Obj, V2, int, __typeof__(ctx), const void *)(v, TIP_ID, T[i].type, cm_il_str(T[i].key), pos, 0, ctx, m_show.mi);
}
static void place_frame(void) {
    if (!shown || tp_check <= 0) return;
    tp_check--;
    float c[4]; if (!card_rect(c) || c[2] - c[0] <= 1 || tp_side) return;
    if (c[0] < tp_panel[2] - 1 || c[2] > screen_w() + 1) {           /* off the screen, or the stock clamp put it over the window */
        float x = tp_panel[0] - 8 - (c[2] - c[0]); if (x < 0) x = 0;
        tp_side = 1; tp_check = 0; show_at(tp_i, (V2){ x, tp_top });
    }
}
static void hide(void) {
    if (!shown) return;
    shown = 0; shown_key[0] = 0; cm_il_hset(&h_shown, NULL); tp_check = 0;
    Obj v = view(); if (v) CM_CALL(m_hide, void, Obj, Guid, const void *)(v, TIP_ID, m_hide.mi);
}
static void show(int i, Obj target) {
    Obj v = view(); float r[4];
    if (!v || !rect_screen(target, r)) return;
    if (shown) hide();
    raise_view(v);
    panel_of(target, r, tp_panel); tp_top = r[3]; tp_side = 0; tp_i = i;
    show_at(i, (V2){ tp_panel[2] + 8, r[3] });                        /* the card's top-left: right of the window, level with the icon */
    tp_check = 4;
    shown = 1; cm_il_hset(&h_shown, target); snprintf(shown_key, sizeof shown_key, "%s", T[i].key);
}
static int on_tree(Obj rt) {
    if (!cm_alive(rt)) return 0;
    Obj go = cm_go(rt);
    return go && (!m_active.fn || CM_CALL(m_active, bool, Obj, const void *)(go, m_active.mi));
}
static void tick(void *ud) {
    (void)ud;
    if (!nt && !shown) return;
    for (int i = nt - 1; i >= 0; i--) if (!cm_alive(cm_il_hget(T[i].h))) { cm_il_hfree(T[i].h); T[i] = T[--nt]; }
    V3 m = CM_CALL(m_mouse, V3, const void *)(m_mouse.mi);
    Obj test = cm_il_hget(h_test); int hit = -1; Obj hit_t = NULL;
    for (int i = 0; i < nt && hit < 0; i++) {
        Obj t = cm_il_hget(T[i].h); if (!on_tree(t)) continue;
        if (test) { if (t == test) { hit = i; hit_t = t; } continue; }
        float r[4];
        if (rect_screen(t, r) && in_r(r, m.x, m.y) && visible_at(t, m.x, m.y)) { hit = i; hit_t = t; }
    }
    Obj cur = cm_il_hget(h_shown);
    if (hit >= 0 && (!shown || cur != hit_t || strcmp(shown_key, T[hit].key))) show(hit, hit_t);
    else if (hit < 0 && shown) hide();
    else place_frame();
}

void cm_gtip_add(DSLObj target, int type, const char *key) {
    init();
    if (!target || !ok) return;
    for (int i = 0; i < nt; i++) if (cm_il_hget(T[i].h) == (Obj)target) {
        if (!key) { if (cm_il_hget(h_shown) == (Obj)target) hide(); cm_il_hfree(T[i].h); T[i] = T[--nt]; return; }
        T[i].type = type; snprintf(T[i].key, sizeof T[i].key, "%s", key); return;
    }
    if (!key || nt >= NT) return;
    T[nt].h = cm_il_hnew((Obj)target); T[nt].type = type; snprintf(T[nt].key, sizeof T[nt].key, "%s", key); nt++;
}
void cm_gtip_test_hover(DSLObj target) { init(); cm_il_hset(&h_test, (Obj)target); if (!target) hide(); }
int cm_gtip_shown(char *key_out, size_t n) { if (key_out && n) snprintf(key_out, n, "%s", shown ? shown_key : ""); return shown; }
