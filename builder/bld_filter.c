/* bld_filter - the Unit Builder's list filters (bld_filter.h). */
#include "bld_filter.h"
#include <stdio.h>
#include <string.h>

int bld_pick_has(const bld_pick *p, const char *k) {
    for (int i = 0; k && i < p->n; i++) if (!strcmp(p->sel[i], k)) return 1;
    return 0;
}
void bld_pick_toggle(bld_pick *p, const char *k) {
    if (!k || !*k) return;
    for (int i = 0; i < p->n; i++) if (!strcmp(p->sel[i], k)) { memmove(p->sel[i], p->sel[i + 1], (size_t)(p->n - i - 1) * sizeof p->sel[0]); p->n--; return; }
    if (p->n < BLD_PICK_MAX) snprintf(p->sel[p->n++], sizeof p->sel[0], "%s", k);
}
int bld_pick_any(const bld_pick *p, const char *k) { return !p->n || bld_pick_has(p, k); }
static int arr_has(const jv *arr, const char *k) {
    for (size_t i = 0; arr && arr->t == JN_ARR && i < arr->cnt; i++) if (jstreq(arr->v[i], k)) return 1;
    return 0;
}
int bld_pick_any_in(const bld_pick *p, const jv *arr) {
    if (!p->n) return 1;
    for (int i = 0; i < p->n; i++) if (arr_has(arr, p->sel[i])) return 1;
    return 0;
}
int bld_pick_all_in(const bld_pick *p, const jv *arr) {
    for (int i = 0; i < p->n; i++) if (!arr_has(arr, p->sel[i])) return 0;
    return 1;
}
int bld_bg_pass(const jv *bg, const bld_pick *weapon, const bld_pick *aux, const bld_pick *bonus) {
    return bld_pick_all_in(weapon, jget(bg, "tw")) && bld_pick_all_in(aux, jget(bg, "ta")) && bld_pick_any_in(bonus, jget(bg, "bon"));
}
