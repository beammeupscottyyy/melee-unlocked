#include "settings_chord.h"
#include <cstdlib>

static void expect(bool value) { if (!value) std::abort(); }

// The settings chord is Start + D-pad Down + Z (0x1000 | 0x0004 | 0x0010).
int main() {
  bool held = false;
  uint16_t buttons = 0;
  expect(!host::settings_shortcut::poll_z_start(true, buttons, held));
  buttons = 0x0010;
  expect(!host::settings_shortcut::poll_z_start(true, buttons, held));
  buttons = 0x1000;
  expect(!host::settings_shortcut::poll_z_start(true, buttons, held));
  buttons = 0x1010;   // the old Z + Start pair alone must not open the panel any more
  expect(!host::settings_shortcut::poll_z_start(true, buttons, held));
  expect(buttons == 0x1010);
  buttons = 0x1014 | 0x0100;
  expect(host::settings_shortcut::poll_z_start(true, buttons, held));
  expect(buttons == 0x0100);  // The shortcut never sends Start, Down or Z to the game/UI.
  buttons = 0x1014;
  expect(!host::settings_shortcut::poll_z_start(true, buttons, held));
  expect(buttons == 0);
  buttons = 0;
  expect(!host::settings_shortcut::poll_z_start(true, buttons, held));
  buttons = 0x1014;
  expect(host::settings_shortcut::poll_z_start(true, buttons, held));
  expect(buttons == 0);
  buttons = 0x1014;
  expect(!host::settings_shortcut::poll_z_start(false, buttons, held));
  expect(buttons == 0x1014);
  // Releasing Down and Z first must not leak a lone Start (which closes the panel).
  held = false;
  buttons = 0x1014;
  expect(host::settings_shortcut::poll_z_start(true, buttons, held));
  buttons = 0x1000;
  expect(!host::settings_shortcut::poll_z_start(true, buttons, held));
  expect(buttons == 0);
  buttons = 0;
  expect(!host::settings_shortcut::poll_z_start(true, buttons, held));
  buttons = 0x1000;   // a fresh Start after full release reaches the UI
  expect(!host::settings_shortcut::poll_z_start(true, buttons, held));
  expect(buttons == 0x1000);

  // Training: a lone Start waits for D-pad Up + Z instead of opening Training's pause menu.
  using host::settings_shortcut::hold_lone_start;
  using host::settings_shortcut::kStartWait;
  int waited = 0;
  bool practice = false;
  auto poll = [&](uint16_t raw) {
    uint16_t b = raw;
    host::settings_shortcut::poll_practice(true, b, practice);
    hold_lone_start(true, practice, raw, b, waited);
    return b;
  };
  expect(poll(0x1000) == 0);             // Start a frame early: held back
  expect(poll(0x1018) == 0);             // D-pad Up + Z join: the chord, nothing reaches the game
  expect(practice);
  expect(poll(0x1000) == 0);             // releasing Up and Z first still sends nothing
  expect(poll(0) == 0);                  // and no Start is owed afterwards
  expect(!practice && waited == 0);
  // A Start that stays alone reaches the game after the wait and stays down while held.
  for (int i = 0; i < kStartWait; ++i) expect(poll(0x1000) == 0);
  expect(poll(0x1000) == 0x1000);
  expect(poll(0x1000) == 0x1000);
  expect(poll(0) == 0);
  // A quick tap is handed over on release, once.
  expect(poll(0x1000) == 0);
  expect(poll(0) == 0x1000);
  expect(poll(0) == 0);
  // Other buttons pass straight through while Start waits.
  expect(poll(0x1100) == 0x0100);
  expect(poll(0x0100) == 0x1100);
  expect(poll(0) == 0);
  // Outside Training nothing is held.
  uint16_t b = 0x1000;
  hold_lone_start(false, false, 0x1000, b, waited);
  expect(b == 0x1000);
  b = 0;
  hold_lone_start(false, false, 0, b, waited);
  expect(b == 0 && waited == 0);
  {
    // The Practice chord can be set (buro, 11-23: a box controller's firmware could not press the default).
    namespace sc = host::settings_shortcut;
    expect(sc::practice_chord().load() == sc::kDefaultPracticeChord);
    expect(sc::chord_name(sc::kDefaultPracticeChord) == "Start + D-pad Up + Z");
    expect(sc::chord_name(0x0400 | 0x0040 | 0x1000) == "Start + X + L");
    expect(sc::valid_practice_chord(0x0400 | 0x0040 | 0x1000));   // Start + X + L
    expect(sc::valid_practice_chord(sc::kDefaultPracticeChord));
    expect(!sc::valid_practice_chord(0x0100));                    // one button
    expect(!sc::valid_practice_chord(0x1010));                    // Start + Z: part of the settings chord
    expect(!sc::valid_practice_chord(sc::kSettingsChord));         // the settings chord itself
    expect(!sc::valid_practice_chord(sc::kSettingsChord | 0x0100));   // holds all of it
    expect(!sc::valid_practice_chord(0x8000 | 0x1000));           // not a chord button
    // A set chord opens the panel, and the default one no longer does.
    sc::practice_chord().store(0x0400 | 0x0040 | 0x1000);
    bool held2 = false;
    uint16_t pad = sc::kDefaultPracticeChord;
    expect(!sc::poll_practice(true, pad, held2));
    pad = 0;
    expect(!sc::poll_practice(true, pad, held2));
    pad = 0x0400 | 0x0040 | 0x1000 | 0x0100;
    expect(sc::poll_practice(true, pad, held2));
    expect(pad == 0x0100);   // its buttons never reach the game
    expect(sc::practice_chord_name() == "Start + X + L");
    sc::practice_chord().store(sc::kDefaultPracticeChord);
    // Recording ends by itself.
    sc::start_recording();
    expect(sc::recording_active() && sc::recorded_held().load() == 0xFFFF);
    sc::recording_until().store(sc::chord_clock_ms() - 1);
    expect(!sc::recording_active() && !sc::recording().load());
  }
  return 0;
}
