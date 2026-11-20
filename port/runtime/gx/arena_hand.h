// The online character select's hand and the Arena panels (arena_panel.cpp): when the hand goes into
// a side's panel. Plain functions of the hand's place in the select's own units (x -35 at the left
// limit, 26 at the right, 29 with m-ex; y up, 0 the picture's middle) and a card's top and bottom in
// the same units, so they are tested without a window (tests/arena_rules_test.cpp).
//
// Two ways in:
//  - a push: the hand at the edge with the stick still pushing out (in a room the whole edge is the
//    panel; out of a room only level with a card);
//  - out of a room, a rest: the hand simply there beside the ARENA or WARM UP card, held still for a
//    moment. Before 0.9.990 only a push let the hand in, so a hand that arrived and was let go, that
//    stopped a little short of the edge, that slid onto the card along the edge, or that was pushed
//    gently (a worn stick, a diagonal: 45 to 60 to the side) never opened the card ("sometimes when I
//    push the hand over here for Arena it doesn't detect it").
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

namespace arena_hand {

constexpr float kLeftEdge = -34.8f, kRightEdge = 25.8f;   // the hand's limits, a little inside them
// A push: the stick past this toward the edge. The hand moves from about 14 (mncharsel.c getStickDelta).
constexpr int kPushStick = 30;
// A rest: the hand's point past the select's last icons (Young Link ends at 24.4, Roy at 23.6;
// Dr. Mario, Falco and Pichu start at -30) and below Young Link's row, which ends at y 6: higher up the
// right edge the hand is on Ganondorf or Young Link, whose players stop there and press A to pick.
constexpr float kLeftReach = -31.0f, kRightReach = 24.6f;
constexpr float kIconsBottom = 6.0f;

// How level the glove is with a card (top above bottom): 2 level with it, 1 only the glove's lower part
// over it (the glove hangs about seven units below the point the game reports), 0 neither.
constexpr int level(float hy, float top, float bottom) {
  if (hy - 2.0f <= top && hy + 5.0f >= bottom) return 2;
  if (hy - 7.0f <= top && hy >= bottom) return 1;
  return 0;
}

// The hand at side's edge (1 left, 2 right) with the stick pushing out.
constexpr bool pushing(int side, float hx, int stick_x) {
  return side == 1 ? hx <= kLeftEdge && stick_x < -kPushStick : hx >= kRightEdge && stick_x > kPushStick;
}

// Out of a room: the hand beside a card on that side without a push. The glove spans from a unit
// above its point to seven below it; some of it is over the card.
constexpr bool beside(int side, float hx, float hy, float top, float bottom) {
  if (side == 2 ? hx < kRightReach : hx > kLeftReach) return false;
  if (hy >= kIconsBottom) return false;
  return hy - 7.0f <= top && hy + 1.0f >= bottom;
}

// In a panel: the hand still at its side (a little slack); out of a room the cards reach in further.
constexpr bool at_side(int side, float hx, bool in_room) {
  const float slack = 0.5f;
  return side == 1 ? hx <= (in_room ? kLeftEdge : kLeftReach) + slack : hx >= (in_room ? kRightEdge : kRightReach) - slack;
}

// The frames a way in takes: a push level with a card, a push with only the glove's lower part over
// one (half a second, so stopping on Young Link to pick him never opens the panel), a rest.
constexpr int kPushFrames = 10, kPushFramesOverCard = 30, kRestFrames = 8;
constexpr float kRestStill = 1.0f;   // a resting hand moved less than this (units) while it counted

}  // namespace arena_hand
