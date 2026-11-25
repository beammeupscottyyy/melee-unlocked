// Late controller read: which matches may start with it and how long each frame waits (host/late_read.h).
// SPDX-License-Identifier: GPL-2.0-or-later
#include <cmath>
#include <cstdio>
#include "late_read.h"

using namespace host::late_read;

static int g_failures = 0;
#define EXPECT(condition) \
  do { if (!(condition)) { std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition); ++g_failures; } } while (0)

static bool near(double a, double b, double tolerance = 0.06) { return std::fabs(a - b) <= tolerance; }

// `frames` game frames with the same arrival, work and lateness; each frame waits what it was told.
static void run(Control& c, int& frame, int frames, double early_ms, double work_ms = 3.0, bool late = false, bool waits = true) {
  for (int i = 0; i < frames; ++i, ++frame) {
    const double wait = c.step(frame, true, early_ms, work_ms, late);
    c.note_waited(waits ? wait : 0.0);
  }
}

static void test_room() {
  EXPECT(!has_room(1, 0.0));       // delay 1 has no frame to give
  EXPECT(has_room(2, 1.0));
  EXPECT(has_room(2, 30.0));
  EXPECT(has_room(2, 33.0));
  EXPECT(!has_room(2, 34.0));      // the wait it needs comes within 3 ms of the limit
  EXPECT(!has_room(2, 36.0));      // measured: 30 rollbacks a game with the wait at its limit
  EXPECT(!has_room(2, 50.0));
  EXPECT(has_room(3, 65.0));       // auto delay's 3 frames at 65 ms: one frame less and 5 to 6 ms of wait
  EXPECT(!has_room(3, 80.0));
  // This computer's frame work: a normal PC (1 to 3 ms) gets the same answers, a busy one none.
  EXPECT(has_room(2, 20.0, 3.0) && has_room(2, 33.0, 3.0) && has_room(3, 65.0, 3.0));
  EXPECT(has_room(2, 20.0, 12.0));
  EXPECT(!has_room(2, 20.0, 13.0));   // measured 11-23: frames of 13 ms and up, the wait never fit
  EXPECT(!has_room(2, 33.0, 7.0));    // a longer ping needs a longer wait: less work fits beside it
}

static void test_wait_follows_the_arrivals() {
  // Before any arrival counts (the games are put in step on frame 30) the wait climbs to its limit.
  Control c;
  int frame = 1;
  run(c, frame, kFirstSampleFrame - 1, 20.0);
  EXPECT(near(c.hold_ms(), kMaxHoldMs));
  // Pads that come 10 ms early need no wait: it goes down slowly, to nothing.
  run(c, frame, 400, 10.0);
  EXPECT(near(c.hold_ms(), 0.0));
  EXPECT(near(c.least_early_ms(), 10.0));
  // Pads that would come 1 ms late without a wait: the wait keeps them the guard early.
  Control d;
  frame = 1;
  run(d, frame, 400, -1.0);
  EXPECT(near(d.hold_ms(), kGuardMs + 1.0));
  // One pad 5 ms later than the rest is remembered for ten seconds, then forgotten.
  run(d, frame, 1, -6.0);
  run(d, frame, 20, -1.0);
  EXPECT(near(d.hold_ms(), kMaxHoldMs));   // 12 ms wanted, the limit given
  run(d, frame, 700, -1.0);
  EXPECT(near(d.hold_ms(), kGuardMs + 1.0));
}

static void test_wait_never_moves_fast() {
  Control c;
  int frame = kFirstSampleFrame;
  double before = c.hold_ms();
  for (int i = 0; i < 30; ++i, ++frame) {
    const double wait = c.step(frame, true, -20.0, 3.0, false);
    EXPECT(wait - before <= 1.0 + 1e-9);          // up: a ms a frame at most
    before = wait;
  }
  for (int i = 0; i < 700; ++i, ++frame) {
    const double wait = c.step(frame, true, 30.0, 3.0, false);
    EXPECT(before - wait <= 0.05 + 1e-9);         // down: a twentieth of a ms a frame
    before = wait;
  }
}

static void test_work_and_lateness() {
  // One slow frame (the load at the start of a game) does not take the wait away.
  Control c;
  int frame = 1;
  run(c, frame, 30, 20.0);
  run(c, frame, 1, 20.0, 50.0);
  run(c, frame, 20, 20.0);
  EXPECT(near(c.work_ms(), 3.0));
  EXPECT(near(c.hold_ms(), kMaxHoldMs));
  // A computer whose frames take 9 ms keeps only the wait that fits beside them.
  Control d;
  frame = 1;
  run(d, frame, 200, -20.0, 9.0);
  EXPECT(near(d.hold_ms(), kFrameMs - kWorkSpareMs - 9.0));
  // A frame the wait itself made late (wait and work filled the frame) shortens the wait ...
  Control e;
  frame = 1;
  run(e, frame, 100, -20.0, 3.0);
  EXPECT(near(e.hold_ms(), kMaxHoldMs));
  run(e, frame, 1, -20.0, 8.0, true);            // 9 ms waited + 8 ms of work: late because of the wait
  EXPECT(e.late_cut_ms() > 0.9);
  // ... and a frame that was late with nothing waited (a stall, or the schedule catching up) does not.
  Control f;
  frame = 1;
  run(f, frame, 100, -20.0, 3.0);
  run(f, frame, 10, -20.0, 3.0, true, false);
  EXPECT(near(f.late_cut_ms(), 0.0));
}

static void test_a_game_that_went_badly() {
  // A steady connection on a computer with room: nothing to hold against the next game.
  Control good;
  int frame = 1;
  run(good, frame, 1800, 2.0);
  EXPECT(!good.went_badly() && good.late_arrivals() == 0 && good.short_frames() == 0);
  // One late pad every second: the wait climbs to its limit and the pads still come after the frame started.
  Control jitter;
  frame = 1;
  for (int i = 0; i < 1800; ++i, ++frame) jitter.note_waited(jitter.step(frame, true, i % 60 == 0 ? -30.0 : 2.0, 3.0, false));
  EXPECT(jitter.late_arrivals() >= 25 && jitter.went_badly());
  // A computer whose frames take 12 ms has no room for the wait a 30 ms ping needs.
  Control slow;
  frame = 1;
  run(slow, frame, 1800, -1.0, 12.0);
  EXPECT(slow.short_frames() > 1000 && slow.went_badly());
  // Too short a game says nothing.
  Control brief;
  frame = 1;
  run(brief, frame, 300, -1.0, 12.0);
  EXPECT(!brief.went_badly());
}

int main() {
  test_room();
  test_a_game_that_went_badly();
  test_wait_follows_the_arrivals();
  test_wait_never_moves_fast();
  test_work_and_lateness();
  if (g_failures) { std::printf("late_read_test: %d check(s) failed\n", g_failures); return 1; }
  std::printf("late_read_test: all checks passed\n");
  return 0;
}
