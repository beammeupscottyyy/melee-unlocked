// Arena room rules: who is in the room, who fights, who is next, and what a finished game or a
// button press changes. Plain data and plain functions: no sockets, no clock, no game state. One
// member (the owner, the lowest id present) applies every change and sends the room to the others,
// so every function here is deterministic and checks what it is given.
//
// The ring holds the fighters: two in singles (slots 0 and 1), four in doubles (team A in slots 0 and
// 1, team B in slots 2 and 3), three or four in a free-for-all (slots 0 to 3, each their own team).
// The line holds members waiting to play, front first. Everyone else is watching. The lineup
// changes only when a game ends.
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace slippi::arena {

constexpr int kMaxMembers = 16;
// Players one room takes (the host refuses the next one). The rules and the wire handle kMaxMembers;
// rooms ship with the size that real games proved.
constexpr int kRoomLimit = 8;
constexpr uint8_t kNone = 0xFF;
// A Slippi connect code: 10 at most, upper case. A YouGame room's members are YouGame player ids (no '#'), kept
// whole and as they are, up to kMaxId: the room's games find each fighter by that id (11-24 production).
constexpr size_t kMaxCode = 10, kMaxId = 40, kMaxName = 31;
constexpr int kRingMax = 4;
constexpr int kFfaMin = 3;   // a free-for-all needs this many fighters

enum class Mode : uint8_t {
  WinnerStays = 0,   // the losers go to the back of the line, the front of the line plays the winners
  TakeTurns = 1,     // whoever has been in longest steps out (in a fresh ring, the loser)
  Ask = 2,           // nothing changes on its own
};
enum class Answer : uint8_t { No = 0, Yes = 1, YesAndPlayWinner = 2 };
enum class Role : uint8_t { Absent, Fighter, InLine, Watching };
// What the ring plays: the room's one "Match" setting (Singles | Doubles | Free-for-all).
enum class Ring : uint8_t { Singles = 0, Doubles = 1, Ffa = 2 };
constexpr uint8_t kRingKinds = 3;
const char* ring_name(Ring kind);   // "Singles", "Doubles", "Free-for-all"

struct Member {
  uint8_t id = kNone;
  std::string code;    // Slippi connect code
  std::string name;
  uint16_t wins = 0, losses = 0;
  uint8_t character = kNone;   // picked on the character select (Slippi's external id), kNone not yet
  uint8_t costume = 0;
  // A Pro AI: the member who added it (its model plays on that member's machine, as that machine's
  // partner port), kNone for a person. It has no machine or links of its own, never votes or hosts,
  // and fights only beside its owner in a doubles or free-for-all ring (one machine can't play singles).
  uint8_t bot_of = kNone;
  uint8_t pro = 0;             // which pro (the model follows the character, ghost_ai)
};
inline bool is_bot(const Member& m) { return m.bot_of != kNone; }
constexpr int kMinPeople = 2;   // a match needs two machines: a ring with Pro AIs needs two people in it

struct Room {
  uint32_t rev = 0;                    // counts up with every change
  Mode mode = Mode::WinnerStays;
  Ring kind = Ring::Singles;           // singles, doubles (two teams) or a free-for-all (3 or 4 fighters)
  // Every member's build plays a free-for-all (the host sets it from the members' hellos; wire 3
  // carries it). Off: Free-for-all cannot be proposed, and a ring that would fall back to it goes
  // to singles instead.
  bool ffa_allowed = false;
  std::vector<Member> members;         // in the order they joined
  uint8_t ring[kRingMax] = {kNone, kNone, kNone, kNone};
  uint16_t tenure[kRingMax] = {0, 0, 0, 0};   // games in a row each has finished in the ring
  std::vector<uint8_t> line;           // waiting to play, front first
  uint32_t games = 0;                  // games finished with a winner
  uint8_t streak_id = kNone;           // the member (singles, free-for-all) or team leader (doubles) on a streak
  uint16_t streak = 0;
  bool in_game = false;
  // A member who is not fighting asks to play next, and which fighters have said no.
  uint8_t asker = kNone;
  uint8_t refused = 0;                 // bit per ring slot
  uint8_t declined = kNone;            // whose ask every fighter said no to (shown until the game ends)
  // Decided for the next game: a fighter gives up their spot. A voluntary change replaces the mode's
  // own rotation for that game.
  uint8_t leaving_slot = kNone;
  uint8_t incoming = kNone;            // who takes it; kNone: the front of the line then
  bool leaver_plays_winner = false;    // the one leaving goes to the front of the line
  bool play_winner_next = false;       // after the next finished game the losers go out, whatever the mode
  // Switching the ring's kind: everyone who would play must say yes.
  bool switch_pending = false;         // a switch to switch_to has been proposed
  Ring switch_to = Ring::Singles;      // the kind proposed
  uint16_t switch_votes = 0;           // bit per member id that said yes
};

// ---- reading ----
const Member* find(const Room& room, uint8_t id);
// A member's pick on the character select, for the others' panels. False when nothing changed.
bool set_character(Room& room, uint8_t id, uint8_t character, uint8_t costume);
Role role(const Room& room, uint8_t id);
inline bool doubles(const Room& r) { return r.kind == Ring::Doubles; }
inline bool ffa(const Room& r) { return r.kind == Ring::Ffa; }
int ring_size(const Room& room);                   // slots the ring has: 2 singles, 4 doubles and free-for-all
int fighters(const Room& room);                    // slots holding a member
// The ring can play: every slot filled, or in a free-for-all at least kFfaMin filled and nobody waiting
// for an open slot.
bool ring_ready(const Room& room);
int ring_slot(const Room& room, uint8_t id);       // the slot, or -1
int team_of_slot(const Room& room, int slot);      // singles and free-for-all: the slot; doubles: 0 for slots 0-1, 1 for 2-3
int line_place(const Room& room, uint8_t id);      // 0 is next, -1 not in line
uint8_t owner(const Room& room);                   // the lowest id present, kNone in an empty room
uint8_t next_up(const Room& room);                 // who comes in at the next change, kNone if nobody
// Whether `kind` can be proposed now: enough people in the room (doubles 4, free-for-all kFfaMin,
// and every build plays it), and not the kind the ring already is. `why` says why not, in the
// panel's words ("Doubles needs 4 people").
bool switch_offered(const Room& room, Ring kind, std::string* why = nullptr);
// The members a proposed switch (to `kind`) needs a yes from: everyone who would be in the new
// ring, fighters first (in slot order) then the line. Empty when the switch is not possible.
// Who would be in a new ring of `kind` (people and the Pro AIs that come with them), empty when it
// can't be played.
std::vector<uint8_t> lineup(const Room& room, Ring kind);
std::vector<uint8_t> switch_voters(const Room& room, Ring kind);
std::vector<uint8_t> switch_voters(const Room& room);   // for the pending switch
// True when the room obeys its own rules (ids unique and in range, ring and line hold members, nobody
// twice, pending choices name people who are there). decode() refuses a room that is not.
bool valid(const Room& room);

// ---- changes (each returns whether anything changed, and bumps rev when it did) ----
// The new member's id, or kNone when the room is full or the code is already in it. A free ring
// slot is filled; otherwise the new member joins the back of the line.
uint8_t join(Room& room, const std::string& code, const std::string& name);
bool leave(Room& room, uint8_t id);
// A person adds their Pro AI (one each, the room's new features on: ffa_allowed), or takes it away.
// The Pro AI's id, kNone when refused. `character` is Slippi's external id.
uint8_t add_bot(Room& room, uint8_t owner, uint8_t character, uint8_t pro);
bool remove_bot(Room& room, uint8_t owner);
uint8_t bot_of(const Room& room, uint8_t owner);   // the person's Pro AI, kNone
int people(const Room& room);                       // members who are people
bool set_mode(Room& room, Mode mode);
// Every member's build plays a free-for-all and Pro AIs (the host's word). Turning it off ends a
// pending free-for-all proposal, takes a free-for-all ring back to singles and removes the Pro AIs.
bool set_ffa_allowed(Room& room, bool allowed);
// A spectator steps into the line, or out of it to just watch. Fighters are not moved by this.
bool toggle_line(Room& room, uint8_t id);
// A member who is not fighting asks to play the next game.
bool ask(Room& room, uint8_t id);
// A fighter answers the ask. The first yes gives up that fighter's spot: at once between games,
// at the end of the game during one.
bool answer(Room& room, uint8_t fighter, Answer answer);
// A fighter offers their spot to the front of the line without being asked (at once between games,
// at the end of the game during one); again, during the game, takes it back.
bool give_up(Room& room, uint8_t fighter, bool play_winner = false);
// Propose another kind of ring. Anyone in the room may propose; the switch happens when every voter
// has said yes (immediately between games, otherwise when the game ends). A no ends the proposal.
bool propose_switch(Room& room, uint8_t id, Ring kind);
// The old toggle (builds before free-for-all): singles to doubles, or back.
bool propose_switch(Room& room, uint8_t id);
bool vote_switch(Room& room, uint8_t id, bool yes);
bool game_start(Room& room);
// winner_team: the winning team (doubles 0 or 1), the winning slot (singles 0 or 1, free-for-all 0 to
// 3), or kNone for a game with no winner (quit out, lost connection): nothing moves then.
bool game_end(Room& room, uint8_t winner_team);

// ---- wire form ----
// Wire 2 is what builds before free-for-all read: it carries no ring kind beyond doubles, so a room
// encoded for them must not be a free-for-all (the host keeps ffa_allowed off while such a build is
// in the room). Wire 3 adds the kind, the proposed kind, ffa_allowed and each member's Pro AI owner and
// pro. decode reads both.
constexpr uint8_t kWireLegacy = 2, kWireVersion = 3;
std::vector<uint8_t> encode(const Room& room, uint8_t version = kWireVersion);
bool decode(const uint8_t* data, size_t size, Room& out);

// ---- the live feed and the links ----
// Who sends the live feed to whom: for every member who is not fighting, the member they get it
// from (a fighter, or for bigger rooms another watcher who passes it on). Each sender serves at most
// `fanout` members. Deterministic; fighters are kNone.
std::vector<std::pair<uint8_t, uint8_t>> feed_tree(const Room& room, int fanout = 3);   // (member, from)
int feed_hops(const Room& room, uint8_t id, int fanout = 3);   // 0 for a fighter, -1 absent
// The connections the room needs for the next game: the fighters with each other, and every feed
// edge. Each pair is (lower id, higher id), sorted, no duplicates. Room messages travel the same
// links (each member passes a newer room on), which keeps everyone reachable.
std::vector<std::pair<uint8_t, uint8_t>> wanted_links(const Room& room, int fanout = 3);

// YouGame with no app (launcher only): a host's arena room is named by its connect code, so friends join by code.
// 12 of [a-z0-9] (the ticket route takes 6 to 12): "mu" and ten base-36 digits of the code's hash, case ignored.
std::string arena_by_code_room(const std::string& connect_code);

}  // namespace slippi::arena
