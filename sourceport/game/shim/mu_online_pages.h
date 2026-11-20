/* The online menu's two pages (owner's pick 11-18): YouGame's five rows ("YouGame Ranked" ..
 * "YouGame Squad Strike") and Slippi's four ("Slippi Direct", Unranked, "Slippi Teams", "Slippi Party"; this build has no
 * Slippi Ranked). Z flips the page;
 * the menu always opens on YouGame's page (owner 11-19); Unranked's slider value is remembered by the
 * host (online-menu.json).
 * The same rows under Melee Unlocked's launcher (split layout) and in the YouGame app (app layout).
 * Shared between mu_slippi_menu.c (rows, input) and mu_menu_label.c (row labels). */
#ifndef MU_ONLINE_PAGES_H
#define MU_ONLINE_PAGES_H
#include <sysdolphin/baselib/gobj.h>
#include <sysdolphin/baselib/sislib.h>

enum MuPagesLabel {
    MP_L_NONE, MP_L_YG_RANKED, MP_L_YG_UNRANKED, MP_L_YG_FRIENDS, MP_L_YG_DOUBLES, MP_L_YG_SQUAD,
    MP_L_SLP_UNRANKED, MP_L_SLP_DIRECT, MP_L_SLP_TEAMS, MP_L_SLP_PARTY, MP_L_BOTH_UNRANKED,
    MP_L_TOURNAMENT, MP_L_END
};

int mu_slippi_online_pages(void);            /* the online menu is the two pages (launcher or app) */
int mu_online_pages_page(void);            /* the page shown: 0 YouGame, 1 Slippi */
int mu_online_pages_label(int row);          /* MuPagesLabel of the row on the page shown, 0 none */
void mu_menu_description_append(HSD_Text* text, const char* message);
struct HSD_JObj;
void mu_online_pages_relabel(struct HSD_JObj* label, int row);   /* mu_menu_label.c: the row's label set again where it is */
struct HSD_GObj;
void mu_mnmain_redescribe(struct HSD_GObj* gp);   /* mn/mnmain.c: the hovered row's description made again */
#endif
