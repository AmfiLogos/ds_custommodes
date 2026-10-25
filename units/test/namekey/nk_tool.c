/* nk_tool - the host half of namekey_test.py (no game). SAVE = a save whose units have the game's default names
 * (UnitNameTextKey = DefaultUnitNameTextKey = TEXTKEY_PLAYERNAME_...), e.g. the wiki's public base save.
 *   nk_tool builds SAVE OUT.json   {"snap": [build, ...]}: each unit as My Units' "Save snapshot" stores it (mu_save_build
 *                                  -> wk_json, as mu_snapshot), the builds Test Fights hands the wiki's save generator
 *                                  (input of fights/test/sg_cli)
 *   nk_tool pb SAVE OUT.json       SAVE with each unit's profile (Type 10) replaced by the one the Bring Units save writer
 *                                  (pb_entry) makes from that snapshot build; everything else unchanged
 * Exit 1 on an error. */
#include "mu_save.h"
#include "pvp_build.h"
#include "fe/fe_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int put(const char *path, const char *s, size_t n) {
    FILE *f = fopen(path, "wb"); if (!f) { fprintf(stderr, "cannot write %s\n", path); return 1; }
    fwrite(s, 1, n, f); fclose(f); return 0;
}
static jv *comp_of(jv *comps, int type) {
    for (size_t i = 0; comps && i < comps->cnt; i++) if (jnumv(jget(comps->v[i], "Type")) == type) return comps->v[i];
    return NULL;
}

int main(int argc, char **argv) {
    if (argc != 4 || (strcmp(argv[1], "builds") && strcmp(argv[1], "pb"))) { fprintf(stderr, "usage: nk_tool builds|pb SAVE OUT.json\n"); return 2; }
    static mu_save_unit U[64]; char e[400];
    int n = mu_save_units(argv[2], U, 64, e, sizeof e);
    if (n <= 0) { fprintf(stderr, "%s: no units (%s)\n", argv[2], e); return 1; }
    jv *save = !strcmp(argv[1], "pb") ? jload(argv[2], e, sizeof e) : NULL, *snap = jarr();
    if (!strcmp(argv[1], "pb") && !save) { fprintf(stderr, "%s: %s\n", argv[2], e); return 1; }
    for (int i = 0; i < n; i++) {
        wk_build b;
        if (mu_save_build(argv[2], U[i].guid, &b, e, sizeof e)) { fprintf(stderr, "unit %s: %s\n", U[i].guid, e); return 1; }
        printf("unit %s name %s\n", U[i].guid, b.name);
        if (save) {
            char notes[2000]; jv *en = pb_entry(&b, "test", notes, sizeof notes, e, sizeof e);
            if (!en) { fprintf(stderr, "pb_entry %s: %s\n", U[i].guid, e); return 1; }
            jv *ec = jpath(save, "EntityComponentSaveData.EntityComponents"), *mine = ec ? comp_of(jget(ec, U[i].guid), 10) : NULL;
            jv *pb = comp_of(jpath(en, "closure.unit.components"), 10);
            if (!mine || !pb) { fprintf(stderr, "unit %s: no profile component\n", U[i].guid); return 1; }
            jset(mine, "Data", jcopy(jget(pb, "Data")));
            jfree(en);
        } else {
            char *js = wk_json(&b, 1); jv *a = js ? jparse(js, strlen(js), e, sizeof e) : NULL; free(js);
            if (!a || a->t != JN_ARR || a->cnt != 1) { fprintf(stderr, "unit %s: build JSON: %s\n", U[i].guid, e); return 1; }
            jpush(snap, jcopy(a->v[0])); jfree(a);
        }
    }
    size_t L; char *txt;
    if (save) txt = jdump(save, 0, &L);
    else { jv *o = jobj(); jset(o, "snap", snap); snap = NULL; txt = jdump(o, 0, &L); jfree(o); }
    int rc = put(argv[3], txt, L);
    free(txt); jfree(save); jfree(snap);
    return rc;
}
