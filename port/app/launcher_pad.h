// The launcher with a controller: a GameCube controller on the official adapter (or an XInput pad) moves Melee's
// character select hand around the launcher. Control stick or D-pad: move the hand. A: click. B: back (Esc).
// Start: Enter. C-stick: scroll. The D-pad, A and B also reach the easter egg (launcher_egg.h).
// The adapter is held only while the launcher is in front and no game runs, and let go at once otherwise, so the game
// always gets it.
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <windows.h>

namespace launcher::pad {
void init(HWND main_window);
void game_running(bool running);   // the launcher started or saw the end of the game
bool hand_shown();                 // the controller moved the hand lately: the launcher's windows hide the mouse arrow
}
