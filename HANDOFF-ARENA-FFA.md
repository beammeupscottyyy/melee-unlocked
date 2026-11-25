# Arena free-for-all and Pro AI seats (feat/arena-ffa)

Branch `feat/arena-ffa` in worktree `C:\Users\Chandler\NEW project\melee-ffa`, merged with next994 at 1.0.14.
Plans: `C:\Users\Chandler\.claude\plans\streamed-wondering-token.md` (FFA), `C:\Users\Chandler\.claude\plans\pro-ai-seats.md`
(Pro AI seats). Not merged into next994, not pushed, not published.

## What players get

### Free-for-all
One room setting, **Match: Singles | Doubles | Free-for-all**, in the Arena panel:
- Rules page (Y): the Match row, left/right proposes the next kind; a kind that can't be played says why
  under the row ("Doubles needs 4 players", "Free-for-all needs 3 players",
  "Everyone needs the latest Melee Unlocked").
- R in the right panel proposes the next kind the room can play.
- Same vote as Doubles: everyone who would be in the new ring says yes; the switch happens between games.
- Offered by head count: Free-for-all at 3+, Doubles at 4+. One hint line when the room reaches 4.
- Leaving: Doubles with 3 left becomes Free-for-all (when every build plays it), else Singles;
  Free-for-all under 3 becomes Singles. The line and records are kept.
- Next game in Free-for-all, Winner stays: the winner keeps the spot, the losers go to the back of the
  line, the people who were waiting come in (longest waiting first). Take turns: the one in longest
  steps out. Ask: as before.
- Stages: the Doubles list (no Fountain of Dreams), drawn from the seed every player agrees on.
- Panel: gold "FREE-FOR-ALL" corners (no red/blue teams), each fighter's own record beside the name;
  Ring look shows all fighters; caption "FFA  WINNER STAYS".
- The character select in a free-for-all hides the team toggle; Start locks in (random stage).

### Pro AI seats (owner 11-23)
- **Rules page, "Your Pro AI":** Off, Fox (Cody), Falco (Mang0), Marth (Zain), Yoshi (aMSa). The Pro AI joins
  the room as your member; it plays beside you in Doubles and Free-for-all.
- **ADD PRO AI card** (connected to someone in Direct, not in a room): one press opens a room with your
  opponent and your Pro AI, and proposes a 3-player free-for-all once they're in (they get the vote).
- Two people, two Pro AIs: Doubles (each person with their AI by default) or a 4-fighter free-for-all.
- A Pro AI sits in and out with its owner, never plays Singles (one PC), never votes or hosts, leaves with
  its owner. One per person. Older builds in the room: Pro AIs (and free-for-all) are off.

### YouGame Friends: Pro AI partner (Doubles home)
- The partner door's tab: Find, Invite, **Pro AI**, P2, CPU. On Pro AI each A goes to the next pro (Fox (Cody),
  Falco (Mang0), Marth (Zain), Yoshi (aMSa)), then on to P2. The door's tag reads PRO, the panel
  "Partner: Fox (Cody)", and the coin goes down on the pro's fighter by itself.
- It is Player 2's local seat (the same LOCAL_SEAT claim P2 sends): Rooms sees a couch partner, nothing new on
  Crunch's side. In the match the shim asks `CMD_PROAI_SEAT` with the pro (1 byte, 0xFF none); the host answers
  with the setup's second local port when its id is `local:<connection>:1`; the model plays it as the partner pad.
- Code: `sourceport/game/include/mu_friends4_css.inc` (`mu_f4_want_next`, `mu_f4_proai_door`, `mu_f4_proai_lift`),
  `shim/mu_slippi_menu.c` (`mu_lobby_pros`, pad router), `shim/mu_online.c` (`proai_seat_ask`),
  `hle/slippi_online.cpp` (CMD_PROAI_SEAT), `hle/native_slippi_bridge.h` (0 or 1 byte).
- Not proven against Rooms (no YouGame accounts on this PC). Tests: `doubles_lobby_select_test.py`,
  `friends4_css_wiring_test.py`.

## How it works

- Rules: `port/runtime/hle/arena_rules.{h,cpp}`: `Ring` kind; `Member::bot_of/pro`, `add_bot`,
  `remove_bot`, `lineup` (people, then the Pro AIs whose owners are in), `seat_bots` keeps every ring legal
  between games. Room wire 3 carries kinds and Pro AIs (wire 2 for older members, who turn both off:
  `ffa_allowed`). Tests `port/tests/arena_rules_test.cpp` (78, incl. 40 seeds x 600 random steps with Pro AIs).
- FFA match: a Teams search on the host's code with `MatchSearchSettings::arena_ffa`; teams off, doubles
  stage list, setup hash salted; `build_verdict` refuses a peer without `MU_BUILD_CAPS3_ARENA_FFA` (0x80).
  Slippi 3 fighters: pair by Direct searches on one port each (`Mesh` in `slippi_arena.cpp`), ready/go via
  the host, then the match from the paired addresses (`Matchmaking::local_peer`).
- Pro AI match: a ring with Pro AIs plays a setup frozen from the room (`make_plan` in `slippi_arena.cpp`,
  the Friends lobby's own path `yougame_lobby_setup.h`): each person, then their Pro AI on the next port;
  teams from the ring; stage and seed from the room's game count (new every game). People keep their own
  picks (`Plan::code_match`). `yougame::set_code_match_plan` hands it to the online code; room links and
  pair searches never take it (`Matchmaking::SetRoomLink`). Real Slippi: Direct pairs, then the setup's
  ports (`LocalPeer::ports`).
- The Pro AI on the wire: the model plays the seat on its owner's PC only (`mu_proai_seat_pad` in
  `shim/mu_practice_cpu.c`, nearest opponent it can hit) and goes out as that PC's partner pad
  (`shim/mu_online.c`; host query `CMD_PROAI_SEAT` 0xC6). Every PC plays it from the same confirmed inputs.
- Results: the end report carries a result byte; with Pro AI seats the reporter names the winner by port
  (result 3), a Pro AI too.
- Select: `shim/mu_slippi_css.c` reads bit 4 of the arena reply (cmd 0xDD): no team toggle, Start locks in.

## Proven (hidden, muted, this PC)

- Real Slippi service, HERO#162 + HERO#984: room Singles (6 games), the 3-player pairing path (6), FFA build
  vs shipped build (5, FFA correctly off), FFA of three with a Pro AI (8), Doubles with two Pro AIs (2).
  0 desyncs, 0 mismatched checksums.
- Loopback rooms: FFA 5 members, FFA 3 + watcher, Singles, Doubles 5, mixed with a shipped build;
  Pro AI: FFA of three (14 games), Doubles 2+2 (3), 3 people + 2 AIs with rotation and a watcher (5,
  recordings exact), FFA of four with 2 AIs (3).
- Final regression on the 1.0.14 merge: `C:\Users\Chandler\mu-ffa\final2.out`.

## Not proven / not built

- YouGame: Arena FFA and Pro AI under YouGame need signed-in production accounts (none on this PC).
- YouGame **Friends**: the Doubles partner Pro AI is built (above) but untested against Rooms. Pro AIs in the
  4-player Friends lobby (FFA, or on the other team) need Crunch's room server to accept local seats outside
  Doubles.
- Two Pro AIs on one PC (needs a second partner pad on the wire).
- Real players over the internet (only the two accounts on this PC, same LAN).

## Build and test

- `C:\Users\Chandler\mu-ffa\build_ffa.py [cfg-game game cfg-host host tests]`
- `C:\Users\Chandler\mu-ffa\run_room.py <name> --players N [--ffa|--doubles] [--watch k] [--bots k-char,...]`
  (Pro AI rooms: `MELEE_ONLINE_TEST_TIMER=75`, and the `ProAI` model folder beside the test build)
- `C:\Users\Chandler\mu-ffa\real_room.py <name> [frames]` (RR_MESH, RR_PLAN, RR_BIN_A/B)
- Junctions in this worktree: `sourceport\extern\melee` and `port\third_party\{ngx\include,ngx\lib,
  ngx\dlssnr,onnxruntime\bin}`. Delete them before removing the worktree.
