/* Online CSS rank panel, laid out exactly as Slippi Dolphin's: project-slippi/slippi-ssbm-c
 * Scenes/CSS/RankInfo/RankInfo.c and RankInfo.h (GPL-3.0). The same "Rank" label, rank name, rating
 * and rating change at the same positions, sizes and colors, Slippi's own rank icon art from slpCSS
 * (its rank_icons, frame = rank), and the same count-up of the rating with its sounds after a set.
 * YouGame supplies the standing; its four divisions show as Slippi's three (yougame_ranked.h
 * rank_enum). No Slippi server IO.
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include <mu_native.h>
#include <mu_test_env.h>
#include <mu_ranked_setup_online.h>
#include "mu_rank_text.h"
#include <stdio.h>
#include <string.h>
#include <melee/gm/gm_1601.h>
#include <melee/sc/types.h>
#include <melee/lb/lbaudio_ax.h>
#include <sysdolphin/baselib/aobj.h>
#include <sysdolphin/baselib/gobj.h>
#include <sysdolphin/baselib/gobjgxlink.h>
#include <sysdolphin/baselib/gobjobject.h>
#include <sysdolphin/baselib/gobjuserdata.h>
#include <sysdolphin/baselib/jobj.h>
#include <sysdolphin/baselib/sislib.h>

/* RankInfo.h */
static const char* const RANK_STRINGS[] = {
    "PENDING", "BRONZE 1", "BRONZE 2", "BRONZE 3", "SILVER 1", "SILVER 2", "SILVER 3", "GOLD 1", "GOLD 2",
    "GOLD 3", "PLATINUM 1", "PLATINUM 2", "PLATINUM 3", "DIAMOND 1", "DIAMOND 2", "DIAMOND 3", "MASTER 1",
    "MASTER 2", "MASTER 3", "GRANDMASTER",
};
enum { PLACEMENT_THRESHOLD = 5, RATING_CHANGE_LEN = 60, RANK_CHANGE_LEN = 10, RATING_NOTIFICATION_LEN = 200 };
#define LOW_RATING_THRESHOLD 10.f
#define MED_RATING_THRESHOLD 20.f
enum { RANK_UP_SMALL = 0xAA, RANK_DOWN_SMALL = 0xBE, RATING_INCREASE = 0x69, RATING_DECREASE = 0x19E,
       TICK_UP = 0xBB, TICK_DOWN = 0xEA, TICK_UP_VOL = 150, TICK_DOWN_VOL = 100 };
static const GXColor white = {255, 255, 255, 255};
static const GXColor gray = {142, 145, 150, 255};
static const GXColor red = {255, 0, 0, 255};
static const GXColor yellow = {255, 200, 0, 255};
static const GXColor green = {3, 252, 28, 255};
static const GXColor light_gray = {170, 173, 178, 255};

/* slpCSS's rank_icons (mu_slippi_chat.c SlpCssChat, slot 5 after the chat window, the chat message,
 * the mode and the connect help). */
typedef struct SlpCssRank {
    DISC_PTR(void) chat_window;
    DISC_PTR(void) chat_message;
    DISC_PTR(void) mode;
    DISC_PTR(void) connect_help;
    DISC_PTR(DynamicModelDesc) rank_icons;
} DISC_STRUCT SlpCssRank;

enum { RANK_LABEL, RANK_NAME, RATING, CHANGE, SET_SCORE, LINE_COUNT };
static struct {
    HSD_Text* text;
    HSD_JObj* icon;
    char lines[LINE_COUNT][40];
    unsigned frames;
    int matched;
    int anim;                 /* a rating change is playing */
    unsigned timer, change_len;
    float from, to;           /* rating, tenths/10 */
    int rank_from, rank_to;
    int placed;
    MuYgU8 left_status, left;
} panel;
/* The last result shown: a result is played once, however often the CSS comes back. */
static MuYgI32 shown_before = -1, shown_after = -1;
static MuYgU16 shown_matches = 0xFFFF;

static int active(void)
{
    /* MELEE_TEST_RANK_PANEL (tests only): the panel on any online character select, so a hidden run (which
     * cannot queue Ranked) can capture it. */
    static int test = -1;
    if (test < 0) test = mu_test_getenv("MELEE_TEST_RANK_PANEL") != NULL;
    return mu_slippi_css_online() && (test || mu_slippi_state()->mode == MU_SLP_MODE_RANKED);
}

/* yougame_ranked.h rank_enum: challenger is Grandmaster; YouGame's division 1 is a tier's top. */
static int slippi_rank(const MuYgRank* r)
{
    if (!r->known || !r->placed) return 0;
    if (r->tier == 6) return 19;
    if (r->tier < 1 || r->tier > 5 || r->division < 1 || r->division > 4) return 0;
    return 1 + 3 * (r->tier - 1) + (r->division == 1 ? 2 : r->division == 2 ? 1 : 0);
}

static void line(int id, const char* value, const GXColor* color)
{
    if (color) HSD_SisLib_803A74F0(panel.text, id, (GXColor*) color);
    if (strcmp(panel.lines[id], value) == 0) return;
    strncpy(panel.lines[id], value, sizeof panel.lines[id] - 1);
    HSD_SisLib_803A70A0(panel.text, id, "%s", value);
}

/* SetRankIcon: the icon's texture animation at frame = rank. */
static void set_icon(int rank)
{
    if (!panel.icon) return;
    HSD_ForeachAnim(panel.icon, JOBJ_TYPE, TOBJ_MASK, HSD_AObjReqAnim, AOBJ_ARG_AF, (f32) rank);
    HSD_JObjAnimAll(panel.icon);
    HSD_ForeachAnim(panel.icon, JOBJ_TYPE, TOBJ_MASK, HSD_AObjStopAnim, AOBJ_ARG_AOV, NULL, NULL);
}

/* SetRankText */
static void set_text(int rank, float rating, unsigned matches, int fetching)
{
    char rating_text[24];
    line(RANK_NAME, RANK_STRINGS[rank], &white);
    if (fetching && rank == 0) {
        line(RATING, "\x81\x48\x81\x48\x81\x48\x81\x48", &gray);   /* "????" */
        return;
    }
    if (matches >= PLACEMENT_THRESHOLD) sprintf(rating_text, "%0.1f", rating);
    else sprintf(rating_text, "%u SETS REQUIRED", PLACEMENT_THRESHOLD - matches);
    line(RATING, rating_text, &light_gray);
}

/* This port's set line under Slippi's panel; a long note shrinks to stay inside the panel. */
static void score_line(const char* value)
{
    float width = (float) strlen(value) * 20.5f, scale = 0.3f;
    if (strcmp(panel.lines[SET_SCORE], value) == 0) return;
    if (width * scale > 180.0f) scale = 180.0f / width;
    HSD_SisLib_803A7548(panel.text, SET_SCORE, scale, scale);
    line(SET_SCORE, value, &light_gray);
}

static void free_panel(void* unused)
{
    (void) unused;
    if (panel.text) HSD_SisLib_803A5CC4(panel.text);
    memset(&panel, 0, sizeof panel);
}

/* InitRankIcon */
static void init_icon(int rank)
{
    SlpCssRank* css = (SlpCssRank*) mu_slippi_css_slpcss();
    DynamicModelDesc* desc = css ? DP(css->rank_icons) : NULL;
    HSD_GObj* gobj;
    if (!desc || !DP(desc->joint)) return;
    gobj = GObj_Create(4, 5, 0x80);
    if (!gobj) return;
    panel.icon = HSD_JObjLoadJoint(DP(desc->joint));
    if (!panel.icon) return;
    panel.icon->translate.x = -9.f;
    /* Slippi's -12.5 put the icon's top over the "Rank" label here (owner 11-24 and 11-25, captures with
     * MELEE_TEST_RANK_PANEL): centred on the rank and rating lines beside it instead. */
    panel.icon->translate.y = -14.3f;
    panel.icon->scale.x = 2.f;
    panel.icon->scale.y = 2.f;
    HSD_JObjSetMtxDirtySub(panel.icon);
    gm_8016895C(panel.icon, desc, 0);
    HSD_JObjReqAnimAll(panel.icon, 0.0f);
    set_icon(rank);
    /* The bob animation at its first frame, as Slippi leaves it. */
    HSD_ForeachAnim(panel.icon, JOBJ_TYPE, JOBJ_MASK, HSD_AObjReqAnim, AOBJ_ARG_AF, 0.f);
    HSD_JObjAnimAll(panel.icon);
    HSD_ForeachAnim(panel.icon, JOBJ_TYPE, JOBJ_MASK, HSD_AObjStopAnim, AOBJ_ARG_AOV, NULL, NULL);
    /* The model's joint animation above writes the root's translation: place the icon after it, or the
     * position set before is lost (the icon stayed over the "Rank" label whatever it was set to). */
    panel.icon->translate.x = -9.f;
    panel.icon->translate.y = -14.3f;
    HSD_JObjSetMtxDirtySub(panel.icon);
    HSD_GObjObject_80390A70(gobj, HSD_GObj_JObjKind, panel.icon);
    GObj_SetupGXLink(gobj, HSD_GObj_JObjCallback, 1, 129);
}

static void start_change(const MuYgResult* r)
{
    float change = (float) (r->after - r->before) / 10.f, size = change < 0 ? -change : change;
    panel.anim = 1;
    panel.timer = 0;
    panel.change_len = size < LOW_RATING_THRESHOLD ? RATING_CHANGE_LEN / 4 :
                       size < MED_RATING_THRESHOLD ? RATING_CHANGE_LEN / 2 : RATING_CHANGE_LEN;
    panel.from = (float) r->before / 10.f;
    panel.to = (float) r->after / 10.f;
    panel.rank_from = slippi_rank(&r->rank_before);
    panel.rank_to = slippi_rank(&r->rank_after);
    panel.placed = r->rank_after.matches >= PLACEMENT_THRESHOLD || r->rank_after.placed;
    set_icon(panel.rank_from);
    set_text(panel.rank_from, panel.from, r->rank_after.matches, 0);
    shown_before = r->before; shown_after = r->after; shown_matches = r->rank_after.matches;
}

/* UpdateRatingChange / UpdateRankChangeAnim */
static void step_change(const MuYgResult* r)
{
    float change = panel.to - panel.from;
    char text[24];
    if (!panel.placed) { panel.anim = 0; mu_ranked_update_result_shown(); return; }
    if (panel.timer % 2 == 1 && panel.timer < panel.change_len) {
        sprintf(text, "%0.1f", panel.from + change * ((float) panel.timer / (float) panel.change_len));
        line(RATING, text, NULL);
        lbAudioAx_800237A8(change > 0 ? TICK_UP : change < 0 ? TICK_DOWN : 0xD0,
                           change > 0 ? TICK_UP_VOL : change < 0 ? TICK_DOWN_VOL : 0, 64);
    } else if (panel.timer == panel.change_len) {
        sprintf(text, "%0.1f", panel.to);
        line(RATING, text, NULL);
    }
    if (panel.timer == panel.change_len + RANK_CHANGE_LEN && panel.rank_to != panel.rank_from)
        lbAudioAx_800237A8(panel.rank_to > panel.rank_from ? RANK_UP_SMALL : RANK_DOWN_SMALL, 255, 64);
    if (panel.timer == panel.change_len + RANK_CHANGE_LEN / 2 + 1) {
        if (r->after != r->before) {
            sprintf(text, "%s%0.1f", change > 0 ? "\x81\x7B" : "\x81\x7C", change < 0 ? -change : change);
            line(CHANGE, text, change > 0 ? &green : &red);
            lbAudioAx_800237A8(change > 0 ? RATING_INCREASE : RATING_DECREASE, 255, 64);
        } else {
            line(CHANGE, "result pending", &yellow);
        }
        if (panel.rank_to != panel.rank_from) {
            set_icon(panel.rank_to);
            set_text(panel.rank_to, panel.to, r->rank_after.matches, 0);
        }
    }
    if (panel.timer == panel.change_len + RANK_CHANGE_LEN + RATING_NOTIFICATION_LEN - 1) {
        line(CHANGE, "", NULL);
        panel.anim = 0;
        mu_ranked_update_result_shown();
    }
    panel.timer++;
}

static void refresh(void)
{
    MuYgRanks ranks;
    MuYgU8 e3[16];
    unsigned got = 0;
    int ok = mu_ranked_read_ranks(&ranks), fetching = 1;
    const MuMatchState* ms = mu_slippi_css_msrb();
    panel.matched = ms->connection_state == MU_SLP_MM_OPPONENT_CONNECTING ||
                    ms->connection_state == MU_SLP_MM_CONNECTION_SUCCESS;
    panel.left = ok && (ranks.result.reason == MU_YG_REASON_FORFEIT_WON ||
                        ranks.result.reason == MU_YG_REASON_FORFEIT_LOST) ? ranks.result.reason : 0;
    panel.left_status = panel.left ? ranks.result.status : 0;
    if (ok && mu_slippi_cmd(0xE3, NULL, 0, e3, sizeof e3, &got) == 0 && got == sizeof e3) fetching = e3[1] == 0;
    if (!ok) {
        line(RANK_NAME, "Error", &white);
        line(RATING, "Failed to get rank", &red);
        return;
    }
    if (panel.anim) return;
    if (!mu_ranked_practice() && ranks.result.status == MU_YG_RESULT_RECORDED &&
        (ranks.result.before != shown_before || ranks.result.after != shown_after ||
         ranks.result.rank_after.matches != shown_matches)) {
        start_change(&ranks.result);
        return;
    }
    set_icon(slippi_rank(&ranks.self));
    set_text(slippi_rank(&ranks.self), (float) ranks.self.rating / 10.f, ranks.self.matches,
             fetching && !ranks.self.known);
}

void mu_ranked_rank_enter(void)
{
    HSD_GObj* owner;
    MuYgRanks ranks;
    int rank = 0;
    memset(&panel, 0, sizeof panel);
    if (!active()) return;
    if (mu_ranked_read_ranks(&ranks)) rank = slippi_rank(&ranks.self);
    init_icon(rank);
    panel.text = mu_rank_text_create();
    if (!panel.text) return;
    /* InitRankInfoText: kerning 1, left aligned, scale 0.01 (here: font 0.1 with positions in tenths
     * of a world unit, so Slippi's x/10 and scale/10). */
    panel.text->default_kerning = 1;
    /* Slippi's text->use_aspect = 1 and text->aspect.X *= 2.5 (m-ex Text: use_aspect is SIS fitting at 0x48,
     * aspect the text box at 0x0C). Left unset, every line was laid out against the default box. */
    panel.text->default_fitting = 1;
    panel.text->box_size_x *= 2.5f;
    panel.text->default_alignment = 0;
    panel.text->pos_x = 0;
    panel.text->pos_y = 0;
    panel.text->font_size.x = panel.text->font_size.y = 0.1f;
    {
        static const float pos[LINE_COUNT][3] = {
            {-111.0f, 85.0f, .4f},     /* "Rank" (-1110, 850) x4 */
            {-64.0f, 110.0f, .4f},     /* rank   (-640, 1100) x4 */
            {-64.0f, 125.0f, .35f},    /* rating (-640, 1250) x3.5 */
            {-22.5f, 126.0f, .3f},     /* change (-225, 1260) x3 */
            {-111.0f, 142.0f, .3f},    /* this port's set line, under Slippi's panel */
        };
        int i;
        for (i = 0; i < LINE_COUNT; ++i) {
            int id = HSD_SisLib_803A6B98(panel.text, pos[i][0], pos[i][1], "%s", "");
            HSD_SisLib_803A7548(panel.text, id, pos[i][2], pos[i][2]);
        }
    }
    owner = GObj_Create(4, 5, 0x80);
    if (!owner) { free_panel(NULL); return; }
    GObj_InitUserData(owner, 4, free_panel, NULL);
    line(RANK_LABEL, "Rank", &gray);
    set_text(rank, 0, 0, 1);
    if (mu_ranked_set()->cur_game > 1 || mu_ranked_set()->score[0] || mu_ranked_set()->score[1])
        mu_slippi_cmd(0xE4, NULL, 0, NULL, 0, NULL);
    refresh();
}

void mu_ranked_rank_frame(void)
{
    char score[64] = "";
    const MuRankedSet* set;
    const MuMatchState* ms;
    MuYgRanks ranks;
    if (!active() || mu_slippi_css_scene_request()) return;
    if (!panel.text) mu_ranked_rank_enter();
    if (!panel.text) return;
    if (panel.anim) {
        if (mu_ranked_read_ranks(&ranks)) step_change(&ranks.result);
        else panel.anim = 0;
    }
    if (++panel.frames >= 15) {
        panel.frames = 0;
        refresh();
    }
    ms = mu_slippi_css_msrb();
    /* Every frame, not only at the refresh: a new search clears the set line at once. */
    panel.matched = ms->connection_state == MU_SLP_MM_OPPONENT_CONNECTING ||
                    ms->connection_state == MU_SLP_MM_CONNECTION_SUCCESS;
    set = mu_ranked_set();
    if (panel.left && ms->connection_state == MU_SLP_MM_IDLE && !mu_ranked_practice()) {
        if (panel.left == MU_YG_REASON_FORFEIT_WON) strcpy(score, "Opponent left: you win");
        else strcpy(score, panel.left_status == MU_YG_RESULT_RECORDED ?
                    "You left: counted as a loss" : "You left the set");
    } else if (set->cur_game > 1 && set->max_games &&
               (ms->connection_state == MU_SLP_MM_IDLE || panel.matched)) {
        const unsigned me = (ms->local_index & 3) == 1, need = (set->max_games + 1u) / 2u;
        const unsigned mine = set->score[me], theirs = set->score[!me];
        if (mine >= need || theirs >= need)
            sprintf(score, "Set %s %u:%u", mine >= need ? "won" : "lost", mine, theirs);
        else if (ms->connection_state == MU_SLP_MM_IDLE)
            sprintf(score, "Set ended early %u:%u", mine, theirs);
        else
            sprintf(score, "Game %u - %u:%u", (unsigned) set->cur_game, mine, theirs);
    }
    score_line(score);
}
