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
static void *k_raycaster;
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
static void hide(void) {
    if (!shown) return;
    shown = 0; shown_key[0] = 0; cm_il_hset(&h_shown, NULL);
    Obj v = view(); if (v) CM_CALL(m_hide, void, Obj, Guid, const void *)(v, TIP_ID, m_hide.mi);
}
static void show(int i, Obj target) {
    Obj v = view(); float r[4];
    if (!v || !rect_screen(target, r)) return;
    if (shown) hide();
    raise_view(v);
    struct { uint8_t b[0xC0]; } ctx; memset(&ctx, 0, sizeof ctx); ctx.b[f_hidehold] = 1;   /* no "hold for more" part */
    V2 pos = { r[2] + 8, r[3] };                                       /* the card's top-left: right of the icon, level with its top */
    CM_CALL(m_show, void, Obj, Guid, int, Obj, V2, int, __typeof__(ctx), const void *)(v, TIP_ID, T[i].type, cm_il_str(T[i].key), pos, 0, ctx, m_show.mi);
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
        if (rect_screen(t, r) && m.x >= r[0] && m.x <= r[2] && m.y >= r[1] && m.y <= r[3]) { hit = i; hit_t = t; }
    }
    Obj cur = cm_il_hget(h_shown);
    if (hit >= 0 && (!shown || cur != hit_t || strcmp(shown_key, T[hit].key))) show(hit, hit_t);
    else if (hit < 0 && shown) hide();
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
