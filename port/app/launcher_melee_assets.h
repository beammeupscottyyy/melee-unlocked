// Melee's own art for the launcher, read from the player's disc (a 1.02 NTSC .iso or .gcm) when it is needed: the
// character select's hand, the rules box frame the menus use for buttons, and the menu font (main.dol's 287 glyphs, the
// same addresses as runtime/gx/practice_menu.cpp). Nothing of the game is stored by the launcher or shipped with it.
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace launcher::melee_assets {

struct Image { int w = 0, h = 0; std::vector<uint32_t> bgra; };   // straight (not premultiplied) BGRA, top row first

void set_disc(const std::string& iso_path);
// Each empty when the disc is missing, not a 1.02 disc image, or the data is not where 1.02 keeps it.
const Image& hand();          // the character select hand, white with soft edges (56x80)
// The character select hand's poses (72x96, white with alpha): 0 pointing, 1 the grab, 2 open. Empty when missing.
const Image& hand_pose(int pose);
const Image& button_rim();    // the rules box rim (48x32), white
const Image& button_fill();   // the same shape filled (the rim's own outline), white

struct Glyph { int index = -1; int left = 0, right = 0; };
struct Font { bool ok = false; Image sheet; Glyph glyph[128]; };   // 32 glyphs a row of 32x32 cells, white with alpha
const Font& font();
// Test runs: writes the hand, the rim, the fill and the font sheet as .bmp files into `dir`.
void dump(const std::string& dir);

}  // namespace launcher::melee_assets
