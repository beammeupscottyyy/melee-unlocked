// Production CSS rank panel (Slippi RankInfo layout) and splash score with a fake renderer and cached
// host IO. SPDX-License-Identifier: GPL-3.0-or-later
#include <mu_ranked.h>
#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>
#include <cstring>
static int checks;
#define CHECK(x) do { ++checks; if (!(x)) { std::cerr << __LINE__ << ": " << #x << '\n'; std::exit(1); } } while (0)
typedef float f32;
struct GXColor { unsigned char r,g,b,a; };
struct HSD_GObj { void (*user_data_remove_func)(void*) = nullptr; };
struct SisBuffer { unsigned char *data, *end; };
static unsigned char font_slot;
static SisBuffer storage{&font_slot, &font_slot};
struct sislib_UnkAlloc3 { sislib_UnkAlloc3* x0=nullptr; int xA=0; };
static sislib_UnkAlloc3 css_camera;
static sislib_UnkAlloc3* HSD_SisLib_804D797C = &css_camera;
static unsigned char* HSD_SisLib_804D1124[5] = {&font_slot};
struct HSD_Text {
  SisBuffer* alloc_data=&storage;
  unsigned char* sis_buffer=&font_slot;
  HSD_GObj* entity=nullptr;
  int default_kerning=0, default_alignment=-1;
  float pos_x=1,pos_y=1;
  struct { float x,y; } font_size{};
  std::vector<std::string> rows;
  struct Row {float x,y,sx=1,sy=1; GXColor color{};};
  std::vector<Row> layout;
};
// Disc symbols and the icon's joint, as far as the panel touches them.
#define DISC_PTR(T) T*
#define DISC_STRUCT
#define DP(p) (p)
struct HSD_Joint { int unused; };
struct DynamicModelDesc { HSD_Joint* joint; void* anims; };
struct Vec3 { float x=0,y=0,z=0; };
struct HSD_JObj { Vec3 translate, scale{1,1,1}; float tex_frame=-1, bob_frame=-1; bool linked=false; };
enum { JOBJ_TYPE = 1, TOBJ_MASK = 2, JOBJ_MASK = 4, AOBJ_ARG_AF = 8, AOBJ_ARG_AOV = 16 };
static void HSD_AObjReqAnim() {}
static void HSD_AObjStopAnim() {}
static void HSD_ForeachAnim(HSD_JObj* j, int, int mask, void (*fn)(), int arg, ...) {
  if (fn != HSD_AObjReqAnim || arg != AOBJ_ARG_AF) return;
  va_list ap; va_start(ap, arg); const float frame = (float) va_arg(ap, double); va_end(ap);
  (mask == TOBJ_MASK ? j->tex_frame : j->bob_frame) = frame;
}
static int joints;
static HSD_JObj* HSD_JObjLoadJoint(HSD_Joint*) { ++joints; return new HSD_JObj{}; }
static void HSD_JObjSetMtxDirtySub(HSD_JObj*) {}
static void HSD_JObjReqAnimAll(HSD_JObj*, float) {}
static void HSD_JObjAnimAll(HSD_JObj*) {}
static void gm_8016895C(HSD_JObj*, DynamicModelDesc*, int) {}
static int HSD_GObj_JObjKind;
static void HSD_GObj_JObjCallback(HSD_GObj*, int) {}
static void HSD_GObjObject_80390A70(HSD_GObj*, int, HSD_JObj* j) { j->linked = true; }
static void GObj_SetupGXLink(HSD_GObj*, void (*)(HSD_GObj*, int), int, int) {}
static std::vector<int> sounds;
static void lbAudioAx_800237A8(int id, int, int) { sounds.push_back(id); }
static int shown_calls;
void mu_ranked_update_result_shown() { ++shown_calls; }

enum { MU_SLP_MODE_RANKED, MU_SLP_MODE_UNRANKED };
enum { MU_SLP_MM_IDLE, MU_SLP_MM_INITIALIZING, MU_SLP_MM_MATCHMAKING,
       MU_SLP_MM_OPPONENT_CONNECTING, MU_SLP_MM_CONNECTION_SUCCESS, MU_SLP_MM_ERROR };
struct MuSlippiMenuState { int mode; } state;
struct MuMatchState { int connection_state; int local_index; } match;
static MuRankedSet set;
static MuYgRanks ranks;
static bool online = true, rank_ok = true, e3_ok = true, has_icons = true;
#if defined(YOUGAME_RANKED_BRANCH) && YOUGAME_RANKED_BRANCH
static bool practice;
int mu_ranked_practice() { return practice; }
#endif
static int request, fetches, reads, e3_status = 1, text_count;
static void (*cleanup)(void*);
static HSD_GObj owner;
static bool mu_slippi_css_online() { return online; }
static MuSlippiMenuState* mu_slippi_state() { return &state; }
static const MuMatchState* mu_slippi_css_msrb() { return &match; }
static int mu_slippi_css_scene_request() { return request; }
const MuRankedSet* mu_ranked_set() { return &set; }
int mu_ranked_read_ranks(MuYgRanks* out) {
  ++reads; *out = rank_ok ? ranks : MuYgRanks{}; return rank_ok;
}
static int mu_slippi_cmd(unsigned cmd, const void*, unsigned, void* out, unsigned size, unsigned* got) {
  if (cmd == 0xE4) { ++fetches; return 0; }
  CHECK(cmd == 0xE3 && size == 16);
  if (!e3_ok) return -1;
  std::memset(out, 0, size); static_cast<unsigned char*>(out)[1] = e3_status;
  *got = size; return 0;
}
static HSD_Joint icon_joint;
static DynamicModelDesc icon_desc{&icon_joint, nullptr};
static void* slpcss_symbol[5] = {nullptr, nullptr, nullptr, nullptr, &icon_desc};
static void* mu_slippi_css_slpcss() { return has_icons ? slpcss_symbol : nullptr; }
static void HSD_SisLib_803A5CC4(HSD_Text* t);
static void text_destroy(void*) {}
static HSD_Text* HSD_SisLib_803A6754(int, int) {
  ++text_count; auto* t = new HSD_Text{};
  static HSD_GObj entity{text_destroy}; t->entity=&entity; return t;
}
static int HSD_SisLib_803A6B98(HSD_Text* t, float x, float y, const char*, const char* s) {
  t->rows.emplace_back(s); t->layout.push_back({x,y}); return int(t->rows.size()-1);
}
static void HSD_SisLib_803A7548(HSD_Text* t, int id, float sx, float sy) {t->layout.at(id).sx=sx;t->layout.at(id).sy=sy;}
static void HSD_SisLib_803A74F0(HSD_Text* t, int id, GXColor* color) { t->layout.at(id).color=*color; }
static void HSD_SisLib_803A70A0(HSD_Text* t, int id, const char*, const char* s) { t->rows.at(id) = s; }
static void HSD_SisLib_803A5CC4(HSD_Text* t) { --text_count; delete t; }
static HSD_GObj* GObj_Create(int, int, int) { return &owner; }
static void GObj_InitUserData(HSD_GObj*, int, void (*fn)(void*), void*) { cleanup = fn; }
#include "../../sourceport/game/shim/mu_ranked_rank.c"

static std::vector<std::string> splash;
static GXColor splash_colors[5];
static int create_subtext(HSD_Text*, const GXColor*, float, float, float, const char*, const char* value) {
  splash.emplace_back(value); return 0;
}
static void splash_score(int local = 0) {
  HSD_Text* t = nullptr;
#include "ranked_splash_score.inc"
}
static MuYgRank rank(const char* label, int tier, int division) {
  MuYgRank out{}; out.known = out.placed = 1; out.tier = tier; out.division = division;
  out.rating = 15347; out.matches = 5; out.placement_total = 5;
  std::strcpy(out.label, label); return out;
}
static const std::string& row(int id) { return panel.text->rows.at(id); }
static bool color_is(int id, GXColor c) { return !std::memcmp(&panel.text->layout.at(id).color, &c, sizeof c); }
// Slippi RankInfo.c InitRankInfoText: scale 0.01 with x/y in world units; here font 0.1 and tenths.
static void layout() {
  CHECK(panel.text->rows.size()==LINE_COUNT && panel.text->pos_x==0 && panel.text->pos_y==0);
  CHECK(panel.text->font_size.x==.1f && panel.text->font_size.y==.1f);
  CHECK(panel.text->default_kerning==1 && panel.text->default_alignment==0);
  const float want[LINE_COUNT][3]={{-111,85,.4f},{-64,110,.4f},{-64,125,.35f},{-22.5f,126,.3f},{-111,142,0}};
  for (int i=0; i<LINE_COUNT; ++i) {
    const auto& r=panel.text->layout[i];
    CHECK(r.x==want[i][0] && r.y==want[i][1]);
    if (i!=SET_SCORE) CHECK(r.sx==want[i][2] && r.sy==want[i][2]);
    // Inside the screen (world +-32 x +-24) and the set line inside the panel's 18 units.
    const float width=row(i).size()*20.5f*r.sx;
    CHECK(r.x*.1f>=-32 && (r.x+width)*.1f<=32 && r.y*.1f<=24);
    if (i==SET_SCORE) CHECK(width<=180.001f && r.sx<=.3f);
  }
  CHECK(row(RANK_LABEL)=="Rank" && color_is(RANK_LABEL,{142,145,150,255}));
}
static void tick(int n=15) { for (int i=0; i<n; ++i) mu_ranked_rank_frame(); layout(); }
static void leave() {
  cleanup(nullptr); CHECK(!panel.text && !panel.icon && text_count == 0);
}
static void enter() { mu_ranked_rank_enter(); CHECK(panel.text); layout(); }
static void shows(const char* name, const char* rating) {
  if (row(RANK_NAME)!=name || row(RATING)!=rating) std::cerr << "shows: " << row(RANK_NAME) << " / " << row(RATING) << " want " << name << " / " << rating << '\n';
  CHECK(row(RANK_NAME)==name && color_is(RANK_NAME,{255,255,255,255}));
  CHECK(row(RATING)==rating);
}
int main() {
  set.max_games = 3; set.cur_game = 1;
  const MuRankedSet initial = set;
  online = false; mu_ranked_rank_enter(); CHECK(!panel.text && reads == 0);
  online = true; state.mode = MU_SLP_MODE_UNRANKED;
  mu_ranked_rank_enter(); CHECK(!panel.text && reads == 0);
  state.mode = MU_SLP_MODE_RANKED;

  // Still fetching, nothing known: Slippi's PENDING and "????".
  e3_status = 0; enter(); CHECK(fetches == 0 && text_count == 1 && joints == 1);
  shows("PENDING", "\x81\x48\x81\x48\x81\x48\x81\x48");
  // Slippi's icon: slpCSS rank_icons at (-9, -12.5) x2, texture frame = rank.
  CHECK(panel.icon && panel.icon->linked && panel.icon->translate.x == -9.f && panel.icon->translate.y == -12.5f);
  CHECK(panel.icon->scale.x == 2.f && panel.icon->scale.y == 2.f && panel.icon->tex_frame == 0 && panel.icon->bob_frame == 0);
  e3_status = 1;
  // YouGame's four divisions show as Slippi's three: division 1 is the tier's top.
  struct { int tier, division; const char* name; int frame; } names[] = {
    {1,4,"BRONZE 1",1},{1,3,"BRONZE 1",1},{1,2,"BRONZE 2",2},{1,1,"BRONZE 3",3},{2,2,"SILVER 2",5},
    {3,4,"GOLD 1",7},{3,1,"GOLD 3",9},{4,2,"PLATINUM 2",11},{5,1,"DIAMOND 3",15},{6,0,"GRANDMASTER",19},
    {0,0,"PENDING",0},{7,1,"PENDING",0},{3,5,"PENDING",0}};
  for (const auto& n : names) {
    ranks.self = rank("host label", n.tier, n.division); tick();
    shows(n.name, n.frame ? "1534.7" : "1534.7");
    CHECK(panel.icon->tex_frame == n.frame);
    CHECK(color_is(RATING,{170,173,178,255}));
  }
  ranks.self = rank("Gold IV", 3, 4); ranks.self.rating = 10080; tick(); shows("GOLD 1", "1008.0");
  // Placements: Slippi's "N SETS REQUIRED" with the PENDING icon.
  ranks.self.placed = 0; ranks.self.matches = 3; tick(); shows("PENDING", "2 SETS REQUIRED");
  CHECK(panel.icon->tex_frame == 0);
  ranks.self.matches = 0; tick(); shows("PENDING", "5 SETS REQUIRED");
  rank_ok = false; tick();
  CHECK(row(RANK_NAME)=="Error" && row(RATING)=="Failed to get rank" && color_is(RATING,{255,0,0,255}));
  rank_ok = true; e3_ok = false; ranks.self = rank("Gold IV", 3, 4); tick(); shows("GOLD 1", "1534.7");
  e3_ok = true; leave();

  // No slpCSS (no Slippi CSS file): the text panel still works without the icon.
  has_icons = false; enter(); CHECK(!panel.icon && joints == 1); shows("GOLD 1", "1534.7"); leave();
  has_icons = true; enter();

  ranks.opponent = rank("Diamond I", 5, 1);
  match.connection_state = MU_SLP_MM_CONNECTION_SUCCESS; tick();
  CHECK(row(SET_SCORE).empty());
  set.cur_game = 2; set.score[0] = 1; tick();
  CHECK(row(SET_SCORE) == "Game 2 - 1:0");
  splash_score(); CHECK(splash == std::vector<std::string>{"Game 2 - 1:0"});
  // Port 1 local (drawn on the left, build 25): its own count comes first on the splash and CSS.
  splash.clear(); splash_score(1); CHECK(splash == std::vector<std::string>{"Game 2 - 0:1"});
  match.local_index = 1; tick(1); CHECK(row(SET_SCORE) == "Game 2 - 0:1");
  match.local_index = 0; tick(1); splash.clear();
  set.cur_game = 3; set.score[1] = 1; tick(1); CHECK(row(SET_SCORE) == "Game 3 - 1:1");
  // A finished set says so (build 30): never "Game 2 - 2:0", as if game 2 were next.
  set.score[0] = 2; tick(1); CHECK(row(SET_SCORE) == "Set won 2:1");
  match.local_index = 1; tick(1); CHECK(row(SET_SCORE) == "Set lost 1:2");
  set.cur_game = 2; set.score[1] = 0; tick(1); CHECK(row(SET_SCORE) == "Set lost 0:2");
  match.local_index = 0; tick(1); CHECK(row(SET_SCORE) == "Set won 2:0");
  set.max_games = 5; tick(1); CHECK(row(SET_SCORE) == "Game 2 - 2:0");
  set.max_games = 3; set.cur_game = 3; set.score[0] = set.score[1] = 1; tick(1);
  CHECK(row(SET_SCORE) == "Game 3 - 1:1");
  request = 1; const auto count = reads; tick(); CHECK(reads == count); request = 0;
  match.connection_state = MU_SLP_MM_MATCHMAKING; tick(1); CHECK(row(SET_SCORE).empty());
  // Cut short (a leave or a dropped link, CTO 11-01): the CSS it returns to is idle.
  match.connection_state = MU_SLP_MM_IDLE; tick(1); CHECK(row(SET_SCORE) == "Set ended early 1:1");
  set.cur_game = 2; set.score[1] = 0; tick(1); CHECK(row(SET_SCORE) == "Set ended early 1:0");
  match.local_index = 1; tick(1); CHECK(row(SET_SCORE) == "Set ended early 0:1");
  match.local_index = 0; set.score[0] = 2; tick(1);
  CHECK(row(SET_SCORE) == "Set won 2:0"); // a finished set wins over "ended early"
  // 122.24: a set the room gave by a leave says so, game 1 included, rated or not, until a search.
  set.cur_game = 1; set.score[0] = set.score[1] = 0;
  ranks.result.status = MU_YG_RESULT_UNRATED; ranks.result.reason = MU_YG_REASON_FORFEIT_WON; tick();
  CHECK(row(SET_SCORE) == "Opponent left: you win");
  ranks.result.reason = MU_YG_REASON_FORFEIT_LOST; tick(); CHECK(row(SET_SCORE) == "You left the set");
  match.connection_state = MU_SLP_MM_MATCHMAKING; tick(1); CHECK(row(SET_SCORE).empty());
  match.connection_state = MU_SLP_MM_IDLE; ranks.result = {}; tick(); CHECK(row(SET_SCORE).empty());
  set.cur_game = 3; set.score[0] = set.score[1] = 1;
  match.connection_state = MU_SLP_MM_ERROR; tick(1); CHECK(row(SET_SCORE).empty());
  match.connection_state = MU_SLP_MM_IDLE; tick(1);
  const auto after = set; leave(); enter();
  CHECK(fetches == 1 && !std::memcmp(&after, &set, sizeof set)); leave();
  set = initial; enter(); CHECK(fetches == 1); leave();

  // After a rated set: Slippi's count-up from the old rating, its sounds, the green change and
  // the new rank, played once however often the CSS comes back.
  ranks.self = rank("Gold II", 3, 2); ranks.self.rating = 15250; // the standing after the set
  ranks.result.status = MU_YG_RESULT_RECORDED; ranks.result.reason = 0;
  ranks.result.before = 15000; ranks.result.after = 15250;
  ranks.result.rank_before = rank("Gold III", 3, 3); ranks.result.rank_after = rank("Gold II", 3, 2);
  sounds.clear(); shown_calls = 0; enter();
  shows("GOLD 1", "1500.0"); CHECK(panel.icon->tex_frame == 7 && panel.anim);
  bool saw_change = false, saw_mid = false;
  for (int i = 0; i < 300 && panel.anim; ++i) {
    mu_ranked_rank_frame();
    if (row(RATING) != "1500.0" && row(RATING) != "1525.0") saw_mid = true;
    if (row(CHANGE) == "\x81\x7B" "25.0") { saw_change = true; CHECK(color_is(CHANGE,{3,252,28,255})); }
  }
  layout();
  CHECK(!panel.anim && saw_change && saw_mid && shown_calls == 1);
  shows("GOLD 2", "1525.0"); CHECK(panel.icon->tex_frame == 8 && row(CHANGE).empty());
  CHECK(std::count(sounds.begin(), sounds.end(), (int) TICK_UP) > 10);
  CHECK(std::count(sounds.begin(), sounds.end(), (int) RATING_INCREASE) == 1);
  CHECK(std::count(sounds.begin(), sounds.end(), (int) RANK_UP_SMALL) == 1);
  leave(); sounds.clear(); enter(); tick(60); CHECK(!panel.anim && sounds.empty() && shown_calls == 1);
  leave();
  // A loss counts down in red with Slippi's down sounds.
  ranks.result.before = 15250; ranks.result.after = 15200; ranks.result.rank_before = ranks.result.rank_after;
  ranks.self.rating = 15200;
  enter(); bool saw_loss = false;
  for (int i = 0; i < 300 && panel.anim; ++i) {
    mu_ranked_rank_frame();
    if (row(CHANGE) == "\x81\x7C" "5.0") { saw_loss = true; CHECK(color_is(CHANGE,{255,0,0,255})); }
  }
  CHECK(saw_loss && shown_calls == 2 && row(RATING) == "1520.0");
  CHECK(std::count(sounds.begin(), sounds.end(), (int) TICK_DOWN) > 0);
  CHECK(std::count(sounds.begin(), sounds.end(), (int) RATING_DECREASE) == 1);
  CHECK(std::count(sounds.begin(), sounds.end(), (int) RANK_DOWN_SMALL) == 0);
  leave();
  // Still placing: no count-up, the result is marked shown at once.
  ranks.result.after = 15300; ranks.result.rank_after.placed = 0; ranks.result.rank_after.matches = 2;
  enter(); tick(2); CHECK(!panel.anim && shown_calls == 3); leave();
  ranks.result = {};

  state.mode = MU_SLP_MODE_UNRANKED; splash.clear(); splash_score(); CHECK(splash.empty());
#if defined(YOUGAME_RANKED_BRANCH) && YOUGAME_RANKED_BRANCH
  // Practice never plays a rated result and keeps the real standing.
  state.mode = MU_SLP_MODE_RANKED; practice = true;
  ranks.self = rank("Gold II", 3, 2);
  ranks.result.status = MU_YG_RESULT_RECORDED; ranks.result.before = 15000; ranks.result.after = 16000;
  ranks.result.rank_after = rank("Gold II", 3, 2);
  enter(); tick(); CHECK(!panel.anim); shows("GOLD 2", "1534.7"); leave();
  practice = false; ranks.result = {};
#endif
  std::cout << "rank panel: " << checks << " checks passed\n";
}
