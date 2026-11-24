// Edge detection and input suppression for the in-game settings shortcut.
#pragma once
#include <atomic>
#include <chrono>
#include <cstdint>
#include <string>

namespace host::settings_shortcut {

// The chord is Start + D-pad Down + Z: three buttons nobody presses together in play, so it never
// fires by accident (Z + Start alone did, and it also sat on the input overlay's Z + Start test).
inline bool poll_chord(bool connected, uint16_t& buttons, bool& was_down, uint16_t chord) {
  const bool down = connected && (buttons & chord) == chord;
  const bool pressed = down && !was_down;
  // Latched until every chord button is up: releasing Down and Z before Start used to hand the
  // UI a lone Start press, and Start closes the panel, so the menu shut the moment it opened.
  if (pressed) was_down = true;
  else if (was_down && (!connected || (buttons & chord) == 0)) was_down = false;
  if (connected && was_down) buttons &= static_cast<uint16_t>(~chord);
  return pressed;
}
inline bool poll_z_start(bool connected, uint16_t& buttons, bool& was_down) {
  return poll_chord(connected, buttons, was_down, 0x1000u | 0x0004u | 0x0010u);   // Start + D-pad Down + Z
}
// The Practice panel's chord: Start + D-pad Up + Z unless the player set another (Settings > Interface,
// "Practice menu buttons"). A box controller whose firmware makes the D-pad from other buttons could
// not press the default together (buro, 11-23).
constexpr uint16_t kSettingsChord = 0x1000u | 0x0004u | 0x0010u;          // Start + D-pad Down + Z
constexpr uint16_t kDefaultPracticeChord = 0x1000u | 0x0008u | 0x0010u;   // Start + D-pad Up + Z
constexpr uint16_t kChordButtons = 0x1F7Fu;   // the buttons a chord can use (no analog shoulder bits)
inline std::atomic<uint16_t>& practice_chord() { static std::atomic<uint16_t> chord{kDefaultPracticeChord}; return chord; }
// Two or more buttons, and neither part of the settings chord nor holding all of it (either would open
// both menus at once).
inline bool valid_practice_chord(uint16_t chord) {
  int count = 0;
  for (uint16_t b = chord; b; b &= static_cast<uint16_t>(b - 1)) ++count;
  return count >= 2 && (chord & kChordButtons) == chord && (chord & kSettingsChord) != chord &&
         (chord & kSettingsChord) != kSettingsChord;
}
inline std::string chord_name(uint16_t chord) {
  static const struct { uint16_t bit; const char* name; } kNames[] = {
      {0x1000, "Start"}, {0x0008, "D-pad Up"}, {0x0004, "D-pad Down"}, {0x0001, "D-pad Left"}, {0x0002, "D-pad Right"},
      {0x0100, "A"}, {0x0200, "B"}, {0x0400, "X"}, {0x0800, "Y"}, {0x0010, "Z"}, {0x0040, "L"}, {0x0020, "R"}};
  std::string out;
  for (const auto& n : kNames)
    if (chord & n.bit) out += (out.empty() ? "" : " + ") + std::string(n.name);
  return out.empty() ? "none" : out;
}
inline std::string practice_chord_name() { return chord_name(practice_chord().load(std::memory_order_relaxed)); }
inline bool poll_practice(bool connected, uint16_t& buttons, bool& was_down) {
  return poll_chord(connected, buttons, was_down, practice_chord().load(std::memory_order_relaxed));
}
// Setting the chord: while recording, the buttons held on any controller are published here and
// nothing reaches the game or the menus (window.cpp), so the chord being pressed does not act.
inline std::atomic<bool>& recording() { static std::atomic<bool> on{false}; return on; }
inline std::atomic<uint16_t>& recorded_held() { static std::atomic<uint16_t> held{0}; return held; }
// Recording ends by itself after 10.5 s even if Settings closes first, so the controllers come back.
inline std::atomic<int64_t>& recording_until() { static std::atomic<int64_t> until{0}; return until; }
inline int64_t chord_clock_ms() {
  return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
}
inline void start_recording() {
  recorded_held().store(0xFFFF, std::memory_order_relaxed);   // nothing read yet
  recording_until().store(chord_clock_ms() + 10500, std::memory_order_relaxed);
  recording().store(true, std::memory_order_relaxed);
}
inline bool recording_active() {
  if (!recording().load(std::memory_order_relaxed)) return false;
  if (chord_clock_ms() > recording_until().load(std::memory_order_relaxed)) { recording().store(false); return false; }
  return true;
}

// Training reads a lone Start as its own pause menu. Hands rarely press three buttons on the same
// frame, so Start often landed a frame before D-pad Up + Z: the pause menu opened under the Practice
// panel with its cursor on Speed, and the first stick push after the panel closed set Speed to 2/3.
// In Training a lone Start now waits up to kStartWait polls for the rest of a chord. A Start that is
// still alone after that reaches the game, and a tap shorter than the wait is handed over on release.
// `taken`: a chord (or the overlay) has this Start, so it never goes to the game.
constexpr int kStartWait = 3;
inline void hold_lone_start(bool active, bool taken, uint16_t raw, uint16_t& buttons, int& waited) {
  const bool start = (raw & 0x1000u) != 0;
  if (!active || taken) {
    waited = start ? kStartWait + 1 : 0;
    return;
  }
  if (start) {
    if (waited < kStartWait) {
      buttons &= static_cast<uint16_t>(~0x1000u);
      ++waited;
    } else {
      waited = kStartWait + 1;   // waited long enough: it is the game's
    }
  } else {
    if (waited > 0 && waited <= kStartWait) buttons |= 0x1000u;   // a short tap still counts
    waited = 0;
  }
}

}  // namespace host::settings_shortcut
