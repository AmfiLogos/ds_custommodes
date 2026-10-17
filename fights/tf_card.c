/* tf_card.c - see tf_card.h. Names checked in the 25846389 dump: Refactor.Util.DataSheetManager.GetUnitData(string)
 * -> Util.Sheet.UnitTableData (float fields by name); Refactor.ResourceLoader.LoadUnitObject / LoadAbyssObject(string)
 * -> UnitObject (the prefab, not an instance; EntityObjectFactory.SpawnUnitObject uses the Abyss one for Abyss units). */
#include "tf_card.h"
#include "cm_win.h"
#include <stdio.h>
#include <string.h>

const char *const tfc_field[TFC_N] = { "MaxHealthTotal", "PhysicalAttackPower", "MagicAttackPower", "AttackSpeed",
                                       "CriticalChance", "PhysicalResistance", "MagicResistance", "MovementSpeed" };
static void *k_dsm, *k_rl, *k_sr, *k_utd;
static cm_m m_ready, m_unit, m_rl_ready, m_load_unit, m_load_abyss, m_kids, m_sprite, m_name, m_go;
static int32_t f_stat[TFC_N];
static int inited;

static void init(void) {
    if (inited) return;
    inited = 1;
    k_dsm = cm_il_class("Refactor.Util", "DataSheetManager");
    m_ready = cm_il_method("Refactor.Util", "DataSheetManager", "get_IsReady", 0, NULL);
    m_unit = cm_il_method("Refactor.Util", "DataSheetManager", "GetUnitData", 1, "System.String");
    k_utd = cm_il_class("Util.Sheet", "UnitTableData");
    for (int i = 0; i < TFC_N; i++) f_stat[i] = cm_il_field(k_utd, tfc_field[i]);
    k_rl = cm_il_class("Refactor", "ResourceLoader");
    m_rl_ready = cm_il_method("Refactor", "ResourceLoader", "get_IsGameResourceLoaderInitialized", 0, NULL);
    m_load_unit = cm_il_method("Refactor", "ResourceLoader", "LoadUnitObject", 1, "System.String");
    m_load_abyss = cm_il_method("Refactor", "ResourceLoader", "LoadAbyssObject", 1, "System.String");
    k_sr = cm_il_class("UnityEngine", "SpriteRenderer");
    m_kids = cm_il_method("UnityEngine", "GameObject", "GetComponentsInChildren", 2, "System.Type,System.Boolean");
    m_sprite = cm_il_method("UnityEngine", "SpriteRenderer", "get_sprite", 0, NULL);
    m_name = cm_il_method("UnityEngine", "Object", "get_name", 0, NULL);
    m_go = cm_il_method("UnityEngine", "Component", "get_gameObject", 0, NULL);
    cmw_log("tf: boss card: table %s, %d stat fields, art %s", k_dsm && m_unit.fn ? "yes" : "NO",
            (int)(f_stat[0] >= 0) + (f_stat[1] >= 0) + (f_stat[2] >= 0) + (f_stat[3] >= 0) + (f_stat[4] >= 0) + (f_stat[5] >= 0) + (f_stat[6] >= 0) + (f_stat[7] >= 0),
            m_load_unit.fn && m_kids.fn && m_sprite.fn ? "yes" : "NO");
}

int tfc_unit_stats(const char *key, float out[TFC_N]) {
    init();
    if (!key || !*key || !k_dsm || !m_unit.fn) return -1;
    Obj dsm = cm_find(k_dsm, true);
    if (!cm_alive(dsm)) return -1;
    if (m_ready.fn && !CM_CALL(m_ready, bool, Obj, const void *)(dsm, m_ready.mi)) return -1;
    Obj row = CM_CALL(m_unit, Obj, Obj, Obj, const void *)(dsm, cm_il_str(key), m_unit.mi);
    if (!row) return -1;
    for (int i = 0; i < TFC_N; i++) out[i] = f_stat[i] >= 0 ? CM_AT(row, f_stat[i], float) : 0;
    return 0;
}

static Obj body_sprite(Obj unit_obj) {
    if (!cm_alive(unit_obj) || !k_sr || !m_kids.fn || !m_sprite.fn) return NULL;
    Obj go = cm_go(unit_obj); if (!go) return NULL;
    Obj arr = CM_CALL(m_kids, Obj, Obj, Obj, bool, const void *)(go, cm_il_type_obj(k_sr), true, m_kids.mi);
    size_t n = arr ? CM_AT(arr, 0x18, uintptr_t) : 0; Obj first = NULL;
    for (size_t i = 0; i < n; i++) {
        Obj r = CM_AT((char *)arr + 0x20, i * sizeof(Obj), Obj);
        Obj sp = cm_alive(r) ? CM_CALL(m_sprite, Obj, Obj, const void *)(r, m_sprite.mi) : NULL;
        if (!cm_alive(sp)) continue;
        char nm[64] = "";
        if (m_go.fn && m_name.fn) cm_il_utf8(CM_CALL(m_name, Obj, Obj, const void *)(CM_CALL(m_go, Obj, Obj, const void *)(r, m_go.mi), m_name.mi), nm, sizeof nm);
        if (!strcmp(nm, "Body")) return sp;                            /* the wiki's unit art: the Body sprite */
        if (!first) first = sp;
    }
    return first;
}
#define NART 8
static struct { char key[80]; uintptr_t h; int tried; } art[NART]; static int nart;
Obj tfc_unit_sprite(const char *key) {
    init();
    if (!key || !*key) return NULL;
    int at = -1;
    for (int i = 0; i < nart; i++) if (!strcmp(art[i].key, key)) at = i;
    if (at >= 0) { Obj o = cm_il_hget(art[at].h); if (cm_alive(o) || art[at].tried) return cm_alive(o) ? o : NULL; }
    Obj rl = k_rl ? cm_find(k_rl, true) : NULL;
    if (!cm_alive(rl) || (m_rl_ready.fn && !CM_CALL(m_rl_ready, bool, Obj, const void *)(rl, m_rl_ready.mi))) return NULL;   /* not yet: asked again */
    cm_m m = strstr(key, "_Abyss") && m_load_abyss.fn ? m_load_abyss : m_load_unit;
    Obj u = m.fn ? CM_CALL(m, Obj, Obj, Obj, const void *)(rl, cm_il_str(key), m.mi) : NULL;
    Obj sp = body_sprite(u);
    if (at < 0) { at = nart < NART ? nart++ : NART - 1; snprintf(art[at].key, sizeof art[at].key, "%s", key); art[at].h = 0; }
    cm_il_hset(&art[at].h, sp); art[at].tried = 1;
    cmw_log("tf: boss card art %s: %s", key, sp ? "Body sprite" : "none");
    return sp;
}
