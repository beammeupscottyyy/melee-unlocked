// The character select's easter egg in the launcher (runtime/gx/walter_egg.h): Up Up Down Down Left Right Left Right,
// then B A sends Bailey bouncing around the launcher, A B sends Walter; each code sends one more (200 at most, then
// the oldest leaves). Same detector, same paths, corners, spin, squash, pop and coloured outline per wall as on the
// character select. Grab one with the mouse to pull it and let go to sling it; Esc sends them all away.
// Keys: the arrow keys and the B and A keys; a controller's D-pad, B and A through feed_pad().
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <windows.h>
#include <cstdint>

namespace launcher::egg {
void init(HWND main_window);
// A controller's buttons this poll (walter_egg's GameCube bits: D-pad, A, B), from the launcher's controller support.
void feed_pad(uint16_t buttons);
void main_moved();   // the launcher moved or changed size: the faces' window follows
}
