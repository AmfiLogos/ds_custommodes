// mu_ui.h - shared pieces of the My Units and Bring Units windows: a game-style window on the kit's screen layer that fits
// the screen at every UI scale, texts that never spill (ellipsis), game icons, the Compact Plus card, a frame tick.
#pragma once
#include "dsl_sdk.h"
#include "mu_card.h"

/* colours (mpmenu_kit.h's palette, approved mockups) as dsl_ui rgba */
#define MU_C_MAIN   0xCCBEB8FFu
#define MU_C_SUB    0xA69085FFu
#define MU_C_GOLD   0xD9BE8FFFu
#define MU_C_COPPER 0xB6835AFFu
#define MU_C_ROW    0x36323AFFu
#define MU_C_ROW2   0x2B2830FFu
#define MU_C_BAND   0x2D2A31FFu
#define MU_C_BAD    0xE06A5AFFu
#define MU_C_BODY   0x222025FFu                 /* window body (the multiplayer lobby's #222025) */
#define MU_C_WELL   0x161419FFu                 /* the card list's background */

/* the Unit Builder's door (lane builder, PLAN_V13 section 4): NULL until that lane is in the build */
#ifdef _WIN32
#define MU_WEAK_IMPORT __attribute__((weak))
#else
#define MU_WEAK_IMPORT __attribute__((weak_import))      /* + -Wl,-U,_cm_builder_open (build_module.sh) */
#endif
extern void cm_builder_open(const char *unit_id_or_null, void (*saved)(const char *unit_id, void *ud), void *ud) MU_WEAK_IMPORT;
int mu_builder_present(void);

const DSLUi *mu_kit(void);
const DSLHost *mu_host(void);
void mu_ui_init(const DSLHost *host, DSLPlugin *self);    /* cm_units_init */
void mu_log(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

/* the game's UI scale (Settings > UI scale), 1 when unknown. KIT-SCALE: our windows apply it themselves (mu_shell_open,
   mu_centre_scaled, gallery win_size: fit 0 / fit 1 and the 40-unit margin the kit's scale_follow has not). The kit's
   scale is opt-in per window (dsl_scale_follow, kit bdcb603): never call it on these windows (it would scale them twice) */
float mu_ui_scale(void);
/* the one window of Units Laboratory (owner 2026-10-25, UX review U1 / U5): My Units, the Unit Builder, the Wiki Build
   Gallery and Test Fights are the kit's window template (dsl_window_std DSL_WIN_MODAL, owner rule "existing places move onto
   the template"): a dim backdrop over the whole screen that takes every click, the window centred, the header (the title in
   the game's title font over the copper line), the game's red X and Escape. close(ud) = the window's Close (the red X;
   Escape when back(ud) did not go Back one level; it ends with mu_shell_close); back may be NULL. The shell owns the X and
   Escape: callers never push the window themselves (its boxes go on top of it). UI Scale stays ours (DSL_WIN_OWN_SCALE,
   KIT-SCALE): fit 0: the size shrinks to the screen layer (the caller lays out from *w / *h); fit 1: the size stays, the
   scale shrinks. An older kit (no template): our own frame with the same layout, the red X or a text X, our own Escape.
   Content goes below MU_SHELL_BAND. Returns the window panel (NULL = kit not ready). */
#define MU_SHELL_BAND 72.f                                 /* the template's header band */
typedef struct mu_shell { DSLObj back, win, x; int std, esc_ok; void (*close)(void *); int (*back_fn)(void *); void *ud; } mu_shell;
DSLObj mu_shell_open(mu_shell *s, const char *title, float want_w, float want_h, int fit, float *w, float *h,
                     void (*close)(void *ud), int (*back)(void *ud), void *ud);
void   mu_shell_close(mu_shell *s);
/* the height a fit-1 window may have at the game's UI scale before its scale goes down ((the screen layer's height - the
   shell's 40 margin) / UI scale): a window that grows with the screen (Test Fights' lists) asks this before mu_shell_open */
float  mu_shell_room_h(void);
int    mu_shell_count(void);                               /* shells open now (test) */
/* a backdrop over the whole kit screen layer: dims what is under it and takes its clicks (the game's notice look) */
DSLObj mu_backdrop(uint32_t rgba);
/* the game's red X (kit), a text "X" running cb(ud) on an older kit; win = the window pushed with esc_push */
DSLObj mu_close_x(DSLObj parent, float x, float y, float s, DSLObj win, dsl_ui_cb cb, void *ud);
/* place w at the game's UI scale, centred on the screen layer (a widget made on the layer, design size bw x bh) */
void   mu_centre_scaled(DSLObj w, float bw, float bh);
/* a plain colour rect, no sprite, clicks through (rgba 0: invisible, a holder for other widgets) */
DSLObj mu_plate(DSLObj parent, float x, float y, float w, float h, uint32_t rgba);
/* a single-line text that ends in "..." when it does not fit (never spills); flags DSL_TXT_*; WRAP = a wrapping box
   (no ellipsis, the caller sizes it) */
DSLObj mu_text(DSLObj parent, float x, float y, float w, float h, const char *s, float size, int flags, uint32_t rgba);
/* the size mu_text draws at: Japanese / Korean / Chinese never below 14 (smaller kanji became blobs at 960x540) */
float mu_text_size(float size);
/* a kit button whose label ends in "..." when it does not fit */
DSLObj mu_button(DSLObj parent, float x, float y, float w, float h, const char *label, dsl_ui_cb cb, void *ud);
/* a game icon (cm_il.h CM_ICON_*), square s; tip = hover title (NULL none). dim = drawn faded (not invested).
   No sprite for the key: a small dark square (the slot is kept) */
DSLObj mu_icon(DSLObj parent, float x, float y, float s, int kind, const char *key, int dim, const char *tip);
/* mu_tip.c: the game's own hover card (TooltipUI: Tooltip_Item / _Skill / _Affecter) on target for a game key. 1 = done,
   0 = no card for this kind (CM_ICON_TREE / _PORTRAIT) or the game's tooltip is not there (caller: the kit's name box) */
int  mu_tip_game(DSLObj target, int kind, const char *key);
void mu_tip_hide(void);
void mu_tip_forget(DSLObj target);                         /* before the icon is destroyed (a page built again) */
DSLObj mu_tip_test_target(int i);
size_t mu_tip_test_state(char *out, size_t cap);
/* the game's name of a key (TEXTKEY_<key>_NAME, then the key without its AFFECTER_ / prefix) */
void mu_name_of(const char *key, char *out, size_t cap);

/* Compact Plus card (owner 2026-10-15) drawn at x, y in parent, width w; returns its root panel, *h the height used.
   name / level: what to show (a save unit's localised name; NULL = the card's own). portrait_key overrides the card's
   avatar (a save's own face). bar: the skill bar to show (NULL = the card's own bar). teams: the names of the My Units
   teams the unit is in (owner 2026-10-25), shown after the name on its row as "Teams: A, B", shortened to "A +2" when
   they do not fit; NULL / 0 = nothing. note: a line on the right of the race line, inside the card (a snapshot's source:
   owner 2026-10-25 "only 3 units per line" when it was a line under the card): forms longest first, the first that fits
   is shown (else the last one cut with "..."); note_tip = the hover card's text (the full line); NULL / 0 = nothing. */
#define MU_CARD_H 128.f                        /* 4 cards fit the My Units list at UI scale 1 (owner 2026-10-17) */
#define MU_CARD_NARROW 360.f                   /* narrower: no in-game look on the card (Test Fights' World units) */
DSLObj mu_card_draw(DSLObj parent, float x, float y, float w, const mu_card *c, const char *name, int level,
                    const char *portrait_key, const char *const *bar, int nbar, const char *const *teams, int nteams,
                    const char *const *note, int nnote, const char *note_tip);
/* the same card with flags. MU_CARD_COMPACT (owner 2026-10-25, Test Fights: the World units list "only ever displays two
   units at once ... It should display more"): the name and the race line share the first row (the name keeps up to 55 %,
   the race line drops its weapon word, then ends in "..."), so the card is MU_CARD_H_COMPACT high; the portrait, the level,
   the three icon rows and every text size stay. No teams / note row in this form (pass NULL / 0). */
#define MU_CARD_COMPACT 1
#define MU_CARD_H_COMPACT 106.f
DSLObj mu_card_draw_ex(DSLObj parent, float x, float y, float w, const mu_card *c, const char *name, int level,
                       const char *portrait_key, const char *const *bar, int nbar, const char *const *teams, int nteams,
                       const char *const *note, int nnote, const char *note_tip, int flags);
float  mu_card_h(int flags);                   /* the card's height for these flags */
/* test support: the teams text / note the last mu_card_draw showed ("" none; the teams' "+N" part included when cut) */
const char *mu_card_teams_shown(void);
const char *mu_card_note_shown(void);
DSLObj mu_card_note_obj(void);                /* the last card's note holder (it takes the hover), NULL none */

/* frame tick: fn runs on the game's main thread between frames every ~ms while registered (mu_clip.c, OS timer) */
int  mu_tick_add(void (*fn)(void));
void mu_tick_remove(void (*fn)(void));
int  mu_os_timer(void (*fn)(void), int ms);               /* mu_clip.c: the OS main-thread timer behind mu_tick */

/* test support (overflow test): every text / icon drawn into cards since the last reset, with its card */
typedef struct mu_drawn { DSLObj o; int card, is_text; } mu_drawn;
int  mu_drawn_list(const mu_drawn **out);
DSLObj mu_drawn_card(int i);
void mu_drawn_reset(void);
