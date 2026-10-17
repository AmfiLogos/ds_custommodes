/* Unit Builder filters (bld_filter.c) on a small background list: several choices at once.
 * cc -I.. -I../../fe filter_test.c ../bld_filter.c ../../fe/fe_json.c -o filter_test && ./filter_test */
#include "bld_filter.h"
#include <stdio.h>
#include <string.h>

static int fails;
static void check(const char *what, int ok) { printf("%s %s\n", ok ? "PASS" : "FAIL", what); fails += !ok; }
/* the backgrounds the filters keep, as "k,k," */
static void kept(const jv *bgs, const bld_pick *w, const bld_pick *a, const bld_pick *b, char *out, size_t n) {
    size_t o = 0; out[0] = 0;
    for (size_t i = 0; i < bgs->cnt; i++) if (bld_bg_pass(bgs->v[i], w, a, b)) {
        const jv *k = jget(bgs->v[i], "k"); o += (size_t)snprintf(out + o, n - o, "%.*s,", (int)k->n, k->s);
    }
}
int main(void) {
    const char *js = "[{\"k\":\"Guard\",\"tw\":[\"Sword\",\"Shield\",\"Spear\"],\"ta\":[\"Warrior\",\"Guardian\"],\"bon\":[\"GuardChance\"]},"
                     "{\"k\":\"Hunter\",\"tw\":[\"Bow\",\"Spear\"],\"ta\":[\"Warrior\",\"Survival\"],\"bon\":[\"Perception\"]},"
                     "{\"k\":\"Mage\",\"tw\":[\"FireStaff\"],\"ta\":[\"Arcane\"],\"bon\":[\"Intelligence\",\"Perception\"]}]";
    char e[200]; jv *bgs = jparse(js, strlen(js), e, sizeof e); if (!bgs) { printf("json: %s\n", e); return 1; }
    bld_pick w = { 0 }, a = { 0 }, b = { 0 }; char out[200];
    kept(bgs, &w, &a, &b, out, sizeof out); check("nothing chosen: every background", !strcmp(out, "Guard,Hunter,Mage,"));
    bld_pick_toggle(&w, "Spear"); bld_pick_toggle(&w, "Sword");
    kept(bgs, &w, &a, &b, out, sizeof out); check("2 weapon trees: only backgrounds granting both", !strcmp(out, "Guard,"));
    bld_pick_toggle(&w, "Sword");
    bld_pick_toggle(&a, "Warrior");
    kept(bgs, &w, &a, &b, out, sizeof out); check("weapon tree + auxiliary tree together", !strcmp(out, "Guard,Hunter,"));
    bld_pick_toggle(&a, "Survival");
    kept(bgs, &w, &a, &b, out, sizeof out); check("2 auxiliary trees + a weapon tree", !strcmp(out, "Hunter,"));
    bld_pick w2 = { 0 }, a2 = { 0 };
    bld_pick_toggle(&b, "Intelligence"); bld_pick_toggle(&b, "GuardChance");
    kept(bgs, &w2, &a2, &b, out, sizeof out); check("2 bonus stats: either one", !strcmp(out, "Guard,Mage,"));
    jfree(bgs);
    printf("%d FAIL\n", fails);
    return fails != 0;
}
