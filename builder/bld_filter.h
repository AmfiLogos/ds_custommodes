/* bld_filter - the Unit Builder's list filters, pure C (no game code, tested in builder/test/filter_test.c).
 * A filter holds any number of chosen values ("" none = everything passes). As in the wiki builder:
 *   - background tree chips (bgMatchesTrees): EVERY chosen tree must be granted (AND inside the row);
 *   - every other filter (bonus stat, item type, rarity, skill kind, inscription kind ...): ANY chosen value (OR);
 *   - different filters AND with each other. */
#pragma once
#include "fe_json.h"

#define BLD_PICK_MAX 48
typedef struct bld_pick { char sel[BLD_PICK_MAX][64]; int n; } bld_pick;

int  bld_pick_has(const bld_pick *p, const char *k);
void bld_pick_toggle(bld_pick *p, const char *k);          /* chosen <-> not chosen */
int  bld_pick_any(const bld_pick *p, const char *k);       /* nothing chosen, or k is chosen */
int  bld_pick_any_in(const bld_pick *p, const jv *arr);    /* nothing chosen, or one of arr's strings is chosen */
int  bld_pick_all_in(const bld_pick *p, const jv *arr);    /* every chosen value is one of arr's strings */
/* one background of the state's "bgs" list: weapon trees tw (all chosen), auxiliary trees ta (all chosen), bonus bon (any) */
int  bld_bg_pass(const jv *bg, const bld_pick *weapon, const bld_pick *aux, const bld_pick *bonus);
