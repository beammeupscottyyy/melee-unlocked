// Online late controller read: how long each frame waits between sending its pad and running.
//
// A rollback match lines the two games up on the moments they send the pad for the same frame number,
// and a pad's frame number has its sender's input delay in it. So a player with one frame less delay
// runs one frame ahead of the other: their own presses show a frame sooner, and the other player's
// inputs have a frame less time to arrive, which costs nobody but themselves (their own rollbacks).
//
// With the late read the game is given one frame less than the chosen delay, and each frame then
// waits between sending its pad and running. The wait gives the other player's inputs their time
// back; whatever of the frame is not waited is input lag saved. The wait is the shortest one that
// would have kept every remote pad of the last ten seconds 6 ms early, so the saving is what the
// connection has to spare: a whole frame on a short connection, and a connection with no room never
// starts a match this way (has_room, judged from the ping before the delay is given to the game).
// Nothing changes on the wire: the other game sees a player whose delay is one lower.
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <algorithm>
#include <array>
#include <atomic>
#include <cstdint>
#include <cstdlib>

namespace host::late_read {

// "Variable online delay" (settings, Input delay): off unless the player turns it on (owner 11-24: optional,
// never the default). MELEE_ONLINE_LATE_READ=0/1 overrides the setting (tests).
inline std::atomic<bool> g_setting{false};
inline void set_enabled(bool on) { g_setting.store(on, std::memory_order_relaxed); }
inline bool enabled() {
  static const int env = [] { const char* v = std::getenv("MELEE_ONLINE_LATE_READ"); return v && *v ? (*v == '0' ? 0 : 1) : -1; }();
  return env >= 0 ? env == 1 : g_setting.load(std::memory_order_relaxed);
}

constexpr double kFrameMs = 16.683;     // the game's 59.94 Hz frame
constexpr double kGuardMs = 6.0;        // every remote pad of the window stays this early
constexpr double kMaxHoldMs = 9.0;      // the longest wait: the frame's own work must still fit
constexpr double kWorkSpareMs = 2.0;    // left after the wait and the work
constexpr double kRoomReserveMs = 3.0;  // a match starts this way only if the wait it needs is this far under the longest
constexpr int32_t kFirstSampleFrame = 60;   // the two games are put in step on frame 30: arrivals count after it

// Whether a match with this chosen delay and ping may run one frame less: the wait it needs to keep
// the other player's pads the guard early must stay well under the longest wait, because a wait at
// its limit leaves those pads less time than the chosen delay did (measured: 36 ms ping at delay 2
// sat at the limit and rolled back 30 times a game, 30 ms ping needed 5 to 7 ms and never did).
// `work_ms` is this computer's frame work just before the match (what 19 of 20 recent frames needed):
// the wait cannot be longer than the frame leaves after the work, so a busy computer has no room
// either. Measured 11-23 on a PC at full load from other programs: the wait never fit (0.0 ms on
// average), 51 and 85 rollbacks a game against 6 to 25 with the chosen delay.
inline bool has_room(int chosen_delay, double ping_ms, double work_ms = 0.0) {
  const double needed = kGuardMs - ((chosen_delay - 1) * kFrameMs - ping_ms / 2.0);
  const double longest = std::min(kMaxHoldMs, kFrameMs - kWorkSpareMs - work_ms);
  return chosen_delay >= 2 && needed <= longest - kRoomReserveMs;
}

class Control {
 public:
  void reset() { *this = Control(); }

  // One game frame, after its pad went out. `early_ms`: how long ago the remote pad for this frame
  // arrived (negative: still on its way), when `have_early`. `work_ms`: the game thread's work in the
  // frame before, and `late` whether that frame finished after its place on the 60 Hz schedule.
  // `fixed_ms` >= 0 fixes the wait (tests). Returns the wait for this frame in ms.
  double step(int32_t frame, bool have_early, double early_ms, double work_ms, bool late, double fixed_ms = -1.0) {
    if (have_early && frame >= kFirstSampleFrame) early_[early_count_++ % early_.size()] = (float)early_ms;
    work_[work_count_++ % work_.size()] = (float)work_ms;
    // What nearly every frame of the last two seconds needed (19 in 20): one slow frame, such as the
    // load at the start of a game, must not take the wait away for two seconds.
    std::array<float, 120> sorted = work_;
    const unsigned have = std::min<unsigned>(work_count_, (unsigned)work_.size());
    std::sort(sorted.begin(), sorted.begin() + have);
    work_now_ = sorted[have * 19 / 20 < have ? have * 19 / 20 : have - 1];
    // A frame the wait itself made late (its wait and its work filled the frame) sent the next pad
    // late: the wait gives a ms back for each such frame and takes it again over the next seconds. A
    // computer that cannot fit the wait ends up without it. A frame that was late with nothing waited
    // (the computer stalled, or the schedule is catching up after a stall) is not the wait's doing:
    // measured, one 40 ms stall used to cut the wait for half a minute, just when it was needed.
    const bool wait_made_it_late = late && last_waited_ + work_ms > kFrameMs - 0.5;
    late_cut_ = wait_made_it_late ? std::min(late_cut_ + 1.0, kMaxHoldMs) : std::max(late_cut_ - 0.02, 0.0);
    const double longest = std::clamp(kFrameMs - kWorkSpareMs - work_now_ - late_cut_, 0.0, kMaxHoldMs);
    double target = longest;   // before any arrival is known: the longest wait
    if (early_count_) {
      least_early_ = early_[0];
      for (unsigned i = 1; i < std::min<unsigned>(early_count_, (unsigned)early_.size()); ++i) least_early_ = std::min(least_early_, (double)early_[i]);
      target = std::clamp(kGuardMs - least_early_, 0.0, longest);
    }
    if (fixed_ms >= 0.0) target = std::min(fixed_ms, kFrameMs - kWorkSpareMs);
    // Up at once when the other player's pads come later (a ms a frame), down slowly when they come sooner.
    hold_ = target > hold_ ? std::min(target, hold_ + 1.0) : std::max(target, hold_ - 0.05);
    sum_ += hold_; ++frames_;
    if (frame >= kFirstSampleFrame) {
      // The two ways a game played this way goes badly: the other player's pad comes after the frame
      // started even with the wait (a guess, and a rollback if it was wrong), or this computer's own
      // frames leave no room for the wait the connection needs.
      if (have_early && early_ms + hold_ < 0.0) ++late_arrivals_;
      if (early_count_ && longest < kMaxHoldMs && kGuardMs - least_early_ > longest + 0.5) ++short_frames_;
    }
    return hold_;
  }

  // Whether this game showed that the connection or this computer does not suit the late read: more
  // than six late pads in ten seconds on average, or the wait cut short by slow frames a tenth of the
  // time. The delay cannot change during a game, so the answer is for the next one: it is not offered
  // again to this opponent. Measured 11-22 on an overloaded PC (frames of 13 to 21 ms): 11 to 20
  // rollbacks a game where plain delay had none.
  bool went_badly() const {
    return frames_ >= 600 && (late_arrivals_ * 600 > 6 * frames_ || short_frames_ * 10 > frames_);
  }
  uint64_t late_arrivals() const { return late_arrivals_; }
  uint64_t short_frames() const { return short_frames_; }

  // How long the frame really waited (less than asked when the frame started late).
  void note_waited(double ms) { last_waited_ = ms; }

  double hold_ms() const { return hold_; }
  double average_hold_ms() const { return frames_ ? sum_ / (double)frames_ : 0.0; }
  double saved_ms() const { return kFrameMs - average_hold_ms(); }   // against the chosen delay
  double least_early_ms() const { return least_early_; }
  double work_ms() const { return work_now_; }
  double late_cut_ms() const { return late_cut_; }
  uint64_t frames() const { return frames_; }

 private:
  std::array<float, 600> early_{};   // how early each remote pad was with no wait at all (ten seconds)
  std::array<float, 120> work_{};    // the game thread's work per frame (two seconds)
  unsigned early_count_ = 0, work_count_ = 0;
  double hold_ = 0.0, sum_ = 0.0, least_early_ = 0.0, work_now_ = 0.0, late_cut_ = 0.0, last_waited_ = 0.0;
  uint64_t frames_ = 0, late_arrivals_ = 0, short_frames_ = 0;
};

}  // namespace host::late_read
