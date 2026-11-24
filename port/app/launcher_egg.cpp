// The easter egg over the launcher (launcher_egg.h). The detector, paths and corner aiming are the character select's
// own (runtime/gx/walter_egg.h); the per-frame motion, spin, squash, pop, outline colours and sizes follow
// runtime/gx/walter_egg.cpp. Drawn on a layered window over the launcher's client area with GDI+, so the faces float
// over every page; clicks on a face reach it, clicks elsewhere go through to the launcher.
// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "launcher_egg.h"
#include "../runtime/gx/walter_egg.h"
#include "../runtime/gx/walter_egg_picture.h"

#include <objidl.h>
#include <gdiplus.h>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <chrono>
#include <cmath>
#include <memory>
#include <vector>

#pragma comment(lib, "gdiplus.lib")

namespace launcher::egg {
namespace {
using namespace walter_egg;

constexpr double kStepsPerSecond = 60.0, kSpinFade = 30.0, kWallGrip = 0.45, kMaxSpin = 25.0, kCornerSpin = 9.0, kSlingSpin = 12.0;
constexpr size_t kMaxFaces = 200;
constexpr float kFaceSize[2] = {0.17f, 0.14f};   // of the launcher's height: Bailey, Walter
constexpr int kUpscale = 4, kGlowPad = 48;
const Gdiplus::Color kGlow[] = {{240, 255, 70, 110}, {240, 255, 150, 40}, {240, 255, 225, 60}, {240, 70, 235, 120},
                                {240, 50, 205, 255}, {240, 125, 120, 255}, {240, 230, 100, 255}};

struct Face {
  int which = 0;   // 0 Bailey, 1 Walter
  Launch path; double t = 0; float box_w = 0, box_h = 0; long long last_step = 0; int bounces = 0;
  double squash_x_at = -1, squash_y_at = -1, pop_at = -1, slung_at = -1; float slung_power = 0;
  double turn = 0, spin = 0, held = 0; bool grabbed = false; float grab_dx = 0, grab_dy = 0;
  Rng rng; float size = 0, x = 0, y = 0, scale_x = 1, scale_y = 1;
};

HWND g_main = nullptr, g_overlay = nullptr;
ULONG_PTR g_gdiplus = 0;
std::vector<Face> g_faces;
Detector g_keys, g_pad;
double g_flash_at = -10, g_last = 0;
uint64_t g_serial = 0;
int g_grabbed = -1; POINT g_grab_at{}, g_drag_at{};
// The two pictures (enlarged smoothly, as the character select does) and each one's outline shape.
std::unique_ptr<Gdiplus::Bitmap> g_face_big[2], g_glow_big[2];
// Sized for the current height: the face, and its outline in each of the seven colours.
std::unique_ptr<Gdiplus::Bitmap> g_face_sized[2], g_glow_sized[2][7];
int g_sized_for[2] = {0, 0};

double now_s() { return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count(); }
double step_length(float h) { return std::max(1.0, (double)h / 0.8 / 60.0 * 0.22); }
double velocity_x(const Launch& l, double t) { const long long k = wall_index(l.x0 + l.vx * t, l.w); const double v = (k % 2 == 0) ? l.vx : -l.vx; return l.mirror_x ? -v : v; }
double velocity_y(const Launch& l, double t) { const long long k = wall_index(l.y0 + l.vy * t, l.h); const double v = (k % 2 == 0) ? l.vy : -l.vy; return l.mirror_y ? -v : v; }

void new_launch(Face& f, float w, float h, double from_x, double from_y) {
  f.path = launch(w, h, step_length(h), f.rng.next() % 5 == 0, f.rng, from_x, from_y);   // about one in five aims at a corner
  f.box_w = w; f.box_h = h; f.t = 0; f.last_step = 0;
}

// ---------------------------------------------------------------- the pictures
void build_pictures(int which) {
  if (g_face_big[which]) return;
  const uint32_t* runs = which == 1 ? kPictureWalter : kPictureBailey;
  const uint32_t run_count = which == 1 ? kPictureWalterRuns : kPictureBaileyRuns;
  const int n = kPictureSize, m = kPictureSize * kUpscale, gm = m + 2 * kGlowPad;
  std::vector<float> src((size_t)n * n * 4, 0.0f);
  size_t at = 0;
  for (uint32_t i = 0; i + 1 < run_count * 2 && at < (size_t)n * n; i += 2)
    for (uint32_t k = 0; k < runs[i] && at < (size_t)n * n; ++k, ++at) {
      const uint32_t px = runs[i + 1]; const float a = (float)(px >> 24) / 255.0f;
      src[at * 4 + 0] = (float)(px & 0xFF) / 255.0f * a; src[at * 4 + 1] = (float)((px >> 8) & 0xFF) / 255.0f * a;
      src[at * 4 + 2] = (float)((px >> 16) & 0xFF) / 255.0f * a; src[at * 4 + 3] = a;
    }
  std::vector<int> tap((size_t)m * 4); std::vector<float> weight((size_t)m * 4);
  for (int o = 0; o < m; ++o) {
    const float pos = ((float)o + 0.5f) / (float)kUpscale - 0.5f; const int base = (int)std::floor(pos); const float t = pos - (float)base;
    const float w[4] = {((-t + 2.0f) * t - 1.0f) * t * 0.5f, ((3.0f * t - 5.0f) * t * t + 2.0f) * 0.5f, ((-3.0f * t + 4.0f) * t + 1.0f) * t * 0.5f, (t - 1.0f) * t * t * 0.5f};
    for (int k = 0; k < 4; ++k) { tap[(size_t)o * 4 + k] = std::clamp(base - 1 + k, 0, n - 1); weight[(size_t)o * 4 + k] = w[k]; }
  }
  std::vector<float> wide((size_t)m * n * 4);
  for (int y = 0; y < n; ++y) for (int x = 0; x < m; ++x) for (int c = 0; c < 4; ++c) {
    float v = 0; for (int k = 0; k < 4; ++k) v += weight[(size_t)x * 4 + k] * src[((size_t)y * n + tap[(size_t)x * 4 + k]) * 4 + c];
    wide[((size_t)y * m + x) * 4 + c] = v;
  }
  auto face = std::make_unique<Gdiplus::Bitmap>(m, m, PixelFormat32bppPARGB);
  std::vector<float> shape((size_t)gm * gm, 0.0f), pass((size_t)gm * gm, 0.0f);
  Gdiplus::BitmapData bd{}; Gdiplus::Rect all(0, 0, m, m);
  face->LockBits(&all, Gdiplus::ImageLockModeWrite, PixelFormat32bppPARGB, &bd);
  for (int y = 0; y < m; ++y) {
    uint32_t* row = (uint32_t*)((uint8_t*)bd.Scan0 + (size_t)y * bd.Stride);
    for (int x = 0; x < m; ++x) {
      float v[4] = {0, 0, 0, 0};
      for (int k = 0; k < 4; ++k) for (int c = 0; c < 4; ++c) v[c] += weight[(size_t)y * 4 + k] * wide[((size_t)tap[(size_t)y * 4 + k] * m + x) * 4 + c];
      const float a = std::clamp(v[3], 0.0f, 1.0f);
      const auto ch = [a](float pm) { return (uint32_t)(std::clamp(pm, 0.0f, a) * 255.0f + 0.5f); };   // premultiplied, kept within alpha
      row[x] = a > 0.004f ? ((uint32_t)(a * 255.0f + 0.5f) << 24 | ch(v[0]) << 16 | ch(v[1]) << 8 | ch(v[2])) : 0;
      shape[(size_t)(y + kGlowPad) * gm + (x + kGlowPad)] = a;
    }
  }
  face->UnlockBits(&bd);
  constexpr int kRadius = 9;   // the outline: the shape blurred (three box passes), then strong near the edge
  for (int round = 0; round < 3; ++round) {
    for (int y = 0; y < gm; ++y) {
      float sum = 0; for (int x = -kRadius; x <= kRadius; ++x) sum += shape[(size_t)y * gm + std::clamp(x, 0, gm - 1)];
      for (int x = 0; x < gm; ++x) { pass[(size_t)y * gm + x] = sum / (float)(2 * kRadius + 1);
        sum += shape[(size_t)y * gm + std::min(x + kRadius + 1, gm - 1)] - shape[(size_t)y * gm + std::max(x - kRadius, 0)]; }
    }
    for (int x = 0; x < gm; ++x) {
      float sum = 0; for (int y = -kRadius; y <= kRadius; ++y) sum += pass[(size_t)std::clamp(y, 0, gm - 1) * gm + x];
      for (int y = 0; y < gm; ++y) { shape[(size_t)y * gm + x] = sum / (float)(2 * kRadius + 1);
        sum += pass[(size_t)std::min(y + kRadius + 1, gm - 1) * gm + x] - pass[(size_t)std::max(y - kRadius, 0) * gm + x]; }
    }
  }
  auto glow = std::make_unique<Gdiplus::Bitmap>(gm, gm, PixelFormat32bppPARGB);
  Gdiplus::Rect gall(0, 0, gm, gm);
  glow->LockBits(&gall, Gdiplus::ImageLockModeWrite, PixelFormat32bppPARGB, &bd);
  for (int y = 0; y < gm; ++y) {
    uint32_t* row = (uint32_t*)((uint8_t*)bd.Scan0 + (size_t)y * bd.Stride);
    for (int x = 0; x < gm; ++x) { const uint32_t a = (uint32_t)(std::clamp(shape[(size_t)y * gm + x] * 2.6f, 0.0f, 1.0f) * 255.0f + 0.5f); row[x] = a << 24 | a << 16 | a << 8 | a; }
  }
  glow->UnlockBits(&bd);
  g_face_big[which] = std::move(face); g_glow_big[which] = std::move(glow);
}

// The face and its seven outlines at this size, made once per size (drawing them small each frame stays cheap).
void size_pictures(int which, int size) {
  if (g_sized_for[which] == size && g_face_sized[which]) return;
  build_pictures(which);
  g_sized_for[which] = size;
  g_face_sized[which] = std::make_unique<Gdiplus::Bitmap>(size, size, PixelFormat32bppPARGB);
  { Gdiplus::Graphics g(g_face_sized[which].get()); g.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
    g.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf); g.Clear(Gdiplus::Color(0, 0, 0, 0)); g.DrawImage(g_face_big[which].get(), 0, 0, size, size); }
  const int gsize = (int)std::lround(size * (1.0 + 2.0 * kGlowPad / (double)(kPictureSize * kUpscale)));
  for (int c = 0; c < 7; ++c) {
    g_glow_sized[which][c] = std::make_unique<Gdiplus::Bitmap>(gsize, gsize, PixelFormat32bppPARGB);
    Gdiplus::Graphics g(g_glow_sized[which][c].get()); g.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
    g.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf); g.Clear(Gdiplus::Color(0, 0, 0, 0));
    // The white shape tinted to the wall's colour (and its alpha).
    const Gdiplus::Color k = kGlow[c];
    Gdiplus::ColorMatrix cm = {{{k.GetR() / 255.0f, 0, 0, 0, 0}, {0, k.GetG() / 255.0f, 0, 0, 0}, {0, 0, k.GetB() / 255.0f, 0, 0},
                                {0, 0, 0, k.GetA() / 255.0f, 0}, {0, 0, 0, 0, 1}}};
    Gdiplus::ImageAttributes ia; ia.SetColorMatrix(&cm);
    g.DrawImage(g_glow_big[which].get(), Gdiplus::Rect(0, 0, gsize, gsize), 0, 0, g_glow_big[which]->GetWidth(), g_glow_big[which]->GetHeight(), Gdiplus::UnitPixel, &ia);
  }
}

// ---------------------------------------------------------------- the faces
RECT client_screen() {
  RECT r{}; GetClientRect(g_main, &r); MapWindowPoints(g_main, nullptr, (POINT*)&r, 2); return r;
}
void send(int which) {
  if (g_faces.size() >= kMaxFaces) {
    auto oldest = std::find_if(g_faces.begin(), g_faces.end(), [](const Face& f) { return !f.grabbed; });
    g_faces.erase(oldest == g_faces.end() ? g_faces.begin() : oldest); g_grabbed = -1;
  }
  const RECT r = client_screen(); const float gw = (float)(r.right - r.left), gh = (float)(r.bottom - r.top);
  Face f; f.which = which;
  uint64_t seed = (uint64_t)std::chrono::steady_clock::now().time_since_epoch().count() + 0x9E3779B97F4A7C15ull * ++g_serial;
  seed = (seed ^ (seed >> 30)) * 0xBF58476D1CE4E5B9ull; seed = (seed ^ (seed >> 27)) * 0x94D049BB133111EBull;
  f.rng.s = (seed ^ (seed >> 31)) | 1;
  f.size = std::round(gh * kFaceSize[which]);
  f.spin = ((f.rng.next() & 1) ? 1.0 : -1.0) * (1.0 + 1.5 * f.rng.unit());
  new_launch(f, gw - f.size, gh - f.size, -1, -1);
  g_faces.push_back(f);
}

void move(Face& f, float gw, float gh, double now, double dt) {
  f.size = std::round(gh * kFaceSize[f.which]);
  const float size = f.size, w = gw - size, h = gh - size;
  if (f.grabbed) { f.held += dt; return; }
  const double clock = now - f.held;
  if (std::abs(w - f.box_w) > 0.5f || std::abs(h - f.box_h) > 0.5f) {   // the launcher changed size: carry on from the same spot
    const double px = f.path.x(f.t) / std::max(1.0, f.path.w) * w, py = f.path.y(f.t) / std::max(1.0, f.path.h) * h;
    new_launch(f, w, h, std::clamp(px, 0.0, (double)w), std::clamp(py, 0.0, (double)h));
  }
  double pace = 1.0;
  if (f.slung_at >= 0) pace += 9.0 * f.slung_power * std::exp(-(clock - f.slung_at) / 1.1);
  f.t += dt * kStepsPerSecond * pace;
  const double radius = std::max(1.0, 0.5 * (double)size), per_second = kStepsPerSecond * pace;
  const auto grip = [&](double rolling) { f.spin += kWallGrip * (rolling - f.spin); };
  const long long step = (long long)std::floor(f.t);
  for (long long s = f.last_step + 1; s <= step; ++s) {
    f.last_step = s;
    const int hit = f.path.hits(s);
    if (!hit) continue;
    ++f.bounces;
    if (hit & 1) f.squash_x_at = clock;
    if (hit & 2) f.squash_y_at = clock;
    const double vx = velocity_x(f.path, (double)s - 1.0) * per_second, vy = velocity_y(f.path, (double)s - 1.0) * per_second;
    if (hit & 1) grip((f.path.x((double)s) > f.path.w * 0.5 ? -vy : vy) / radius);
    if (hit & 2) grip((f.path.y((double)s) > f.path.h * 0.5 ? vx : -vx) / radius);
    if (hit == 3) {   // a corner: a burst of spin, a pop and a flash, and straight on out of it
      f.spin += (f.spin >= 0 ? 1.0 : -1.0) * kCornerSpin; f.pop_at = clock;
      if (now - g_flash_at >= 1.5) g_flash_at = now;
      new_launch(f, w, h, f.path.x((double)s), f.path.y((double)s));
      break;
    }
  }
  if (f.path.corner_step < 0 && f.t > 12000.0) new_launch(f, w, h, f.path.x(f.t), f.path.y(f.t));
  f.x = (float)f.path.x(f.t); f.y = (float)f.path.y(f.t);
  auto squash = [&](double at) { const double a = clock - at; return a >= 0 && a < 0.18 ? (float)(1.0 - a / 0.18) : 0.0f; };
  const float sx = squash(f.squash_x_at), sy = squash(f.squash_y_at);
  float pop = 1.0f;
  if (const double a = clock - f.pop_at; f.pop_at >= 0 && a >= 0 && a < 0.35) pop += 0.22f * (float)std::sin(3.14159265358979 * a / 0.35);
  f.scale_x = (1.0f - 0.22f * sx + 0.12f * sy) * pop; f.scale_y = (1.0f - 0.22f * sy + 0.12f * sx) * pop;
  f.spin = std::clamp(f.spin, -kMaxSpin, kMaxSpin) * std::exp(-dt / kSpinFade);
  f.turn = std::remainder(f.turn + f.spin * dt, 6.283185307179586);
}

// Let go of a pull: it flies off the other way, the faster the further it was pulled, spinning by where it was held.
void sling(Face& f, float gw, float gh, double now, float dir_x, float dir_y, float power, double spin_add) {
  const float w = gw - f.size, h = gh - f.size;
  Launch l = launch(w, h, step_length(gh - f.size), false, f.rng, std::clamp((double)f.x, 0.0, (double)w), std::clamp((double)f.y, 0.0, (double)h));
  const double len = std::max(1e-6, std::hypot((double)dir_x, (double)dir_y)), speed = step_length(gh - f.size);
  l.mirror_x = dir_x < 0; l.mirror_y = dir_y < 0;
  l.vx = std::max(0.05, speed * std::abs(dir_x) / len); l.vy = std::max(0.05, speed * std::abs(dir_y) / len);
  l.x0 = l.mirror_x ? w - std::clamp((double)f.x, 0.0, (double)w) : std::clamp((double)f.x, 0.0, (double)w);
  l.y0 = l.mirror_y ? h - std::clamp((double)f.y, 0.0, (double)h) : std::clamp((double)f.y, 0.0, (double)h);
  l.corner_step = -1;
  f.path = l; f.box_w = w; f.box_h = h; f.t = 0; f.last_step = 0;
  f.slung_at = now - f.held; f.slung_power = power;
  f.spin = std::clamp(f.spin + spin_add, -kMaxSpin, kMaxSpin);
}

// ---------------------------------------------------------------- drawing
void draw() {
  const RECT r = client_screen(); const int W = r.right - r.left, H = r.bottom - r.top;
  if (W <= 0 || H <= 0) return;
  SetWindowPos(g_overlay, nullptr, r.left, r.top, W, H, SWP_NOACTIVATE | SWP_NOZORDER);
  BITMAPINFO bi{}; bi.bmiHeader.biSize = sizeof bi.bmiHeader; bi.bmiHeader.biWidth = W; bi.bmiHeader.biHeight = -H; bi.bmiHeader.biPlanes = 1; bi.bmiHeader.biBitCount = 32;
  void* bits = nullptr; HDC screen = GetDC(nullptr), dc = CreateCompatibleDC(screen);
  HBITMAP bm = CreateDIBSection(screen, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
  if (!bm) { DeleteDC(dc); ReleaseDC(nullptr, screen); return; }
  HGDIOBJ old = SelectObject(dc, bm);
  {
    Gdiplus::Bitmap canvas(W, H, W * 4, PixelFormat32bppPARGB, (BYTE*)bits);
    Gdiplus::Graphics g(&canvas);
    g.Clear(Gdiplus::Color(0, 0, 0, 0));
    g.SetInterpolationMode(Gdiplus::InterpolationModeBilinear); g.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);
    for (const Face& f : g_faces) {
      const int size = (int)f.size; if (size < 4) continue;
      size_pictures(f.which, size);
      const float cx = f.x + size * 0.5f, cy = f.y + size * 0.5f;
      g.ResetTransform(); g.TranslateTransform(cx, cy); g.RotateTransform((float)(f.turn * 180.0 / 3.14159265358979)); g.ScaleTransform(f.scale_x, f.scale_y);
      const auto& glow = g_glow_sized[f.which][f.bounces % 7]; const float gs = (float)glow->GetWidth();
      g.DrawImage(glow.get(), -gs * 0.5f, -gs * 0.5f, gs, gs);
      g.DrawImage(g_face_sized[f.which].get(), -size * 0.5f, -size * 0.5f, (float)size, (float)size);
    }
    g.ResetTransform();
    if (const double since = now_s() - g_flash_at; since >= 0 && since < 0.75) {   // a corner's flash
      Gdiplus::SolidBrush b(Gdiplus::Color((BYTE)(110 * (1.0 - since / 0.75)), 255, 255, 255)); g.FillRectangle(&b, 0, 0, W, H);
    }
  }
  // Test runs (MELEE_LAUNCHER_TEST_EGG): one frame, 3 s in, written next to the launcher for the hidden checks.
  static double test_start = 0; static bool test_saved = false;
  if (!test_saved && std::getenv("MELEE_LAUNCHER_TEST") && std::getenv("MELEE_LAUNCHER_TEST_EGG")) {
    if (!test_start) test_start = now_s();
    if (now_s() - test_start > 3.0) {
      test_saved = true;
      BITMAPFILEHEADER fh{}; fh.bfType = 0x4D42; fh.bfOffBits = sizeof fh + sizeof(BITMAPINFOHEADER); fh.bfSize = fh.bfOffBits + W * H * 4;
      if (FILE* f = std::fopen("egg-capture.bmp", "wb")) { std::fwrite(&fh, sizeof fh, 1, f); std::fwrite(&bi.bmiHeader, sizeof bi.bmiHeader, 1, f); std::fwrite(bits, 4, (size_t)W * H, f); std::fclose(f); }
    }
  }
  POINT dst{r.left, r.top}, src{0, 0}; SIZE sz{W, H}; BLENDFUNCTION bf{AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};
  UpdateLayeredWindow(g_overlay, screen, &dst, &sz, dc, &src, 0, &bf, ULW_ALPHA);
  SelectObject(dc, old); DeleteObject(bm); DeleteDC(dc); ReleaseDC(nullptr, screen);
}

int face_at(POINT p) {   // overlay coordinates
  for (int i = (int)g_faces.size() - 1; i >= 0; --i) {
    const Face& f = g_faces[i]; const float c = f.size * 0.5f;
    if (std::hypot(p.x - (f.x + c), p.y - (f.y + c)) <= c) return i;
  }
  return -1;
}

void tick() {
  const double now = now_s(); const double dt = g_last > 0 ? std::clamp(now - g_last, 0.0, 0.1) : 1.0 / 60.0; g_last = now;
  // The code, from the keyboard while the launcher is in front (arrows, then B A or A B).
  if (GetForegroundWindow() == g_main || GetForegroundWindow() == g_overlay) {
    auto down = [](int vk) { return (GetAsyncKeyState(vk) & 0x8000) != 0; };
    uint16_t b = 0;
    if (down(VK_UP)) b |= kDU; if (down(VK_DOWN)) b |= kDD; if (down(VK_LEFT)) b |= kDL; if (down(VK_RIGHT)) b |= kDR;
    if (down('A')) b |= kA; if (down('B')) b |= kB;
    if (const int code = g_keys.feed(b)) send(code == kWalter ? 1 : 0);
    if (down(VK_ESCAPE) && !g_faces.empty()) { g_faces.clear(); g_grabbed = -1; }
  }
  if (g_faces.empty()) { if (IsWindowVisible(g_overlay)) ShowWindow(g_overlay, SW_HIDE); return; }
  if (IsIconic(g_main)) return;
  const RECT r = client_screen(); const float gw = (float)(r.right - r.left), gh = (float)(r.bottom - r.top);
  for (Face& f : g_faces) move(f, gw, gh, now, dt);
  if (g_grabbed >= 0 && g_grabbed < (int)g_faces.size()) {   // a held face follows the mouse
    POINT p; GetCursorPos(&p); ScreenToClient(g_overlay, &p);
    Face& f = g_faces[g_grabbed]; f.x = std::clamp((float)p.x - f.grab_dx, 0.0f, gw - f.size); f.y = std::clamp((float)p.y - f.grab_dy, 0.0f, gh - f.size); g_drag_at = p;
  }
  if (!IsWindowVisible(g_overlay)) ShowWindow(g_overlay, SW_SHOWNOACTIVATE);
  draw();
}

LRESULT CALLBACK proc(HWND w, UINT m, WPARAM wp, LPARAM lp) {
  switch (m) {
    case WM_MOUSEACTIVATE: return MA_NOACTIVATE;
    case WM_TIMER: tick(); return 0;
    case WM_LBUTTONDOWN: {
      POINT p{(short)LOWORD(lp), (short)HIWORD(lp)}; const int i = face_at(p);
      if (i >= 0) { Face& f = g_faces[i]; f.grabbed = true; f.spin = 0; f.grab_dx = p.x - f.x; f.grab_dy = p.y - f.y; g_grabbed = i; g_grab_at = p; g_drag_at = p; SetCapture(w); }
      return 0;
    }
    case WM_LBUTTONUP: {
      if (g_grabbed >= 0 && g_grabbed < (int)g_faces.size()) {
        Face& f = g_faces[g_grabbed]; f.grabbed = false;
        const RECT r = client_screen(); const float gw = (float)(r.right - r.left), gh = (float)(r.bottom - r.top);
        const float dx = (float)(g_grab_at.x - g_drag_at.x), dy = (float)(g_grab_at.y - g_drag_at.y);   // pulled one way, it flies the other
        const float power = std::clamp((float)std::hypot(dx, dy) / std::max(1.0f, f.size * 2.0f), 0.0f, 1.0f);
        const double off = ((double)f.grab_dx - f.size * 0.5) / std::max(1.0, f.size * 0.5);   // held off-centre: it spins
        if (power > 0.02f) sling(f, gw, gh, now_s(), dx, dy, power, -off * kSlingSpin * power);
        else new_launch(f, gw - f.size, gh - f.size, std::clamp((double)f.x, 0.0, (double)(gw - f.size)), std::clamp((double)f.y, 0.0, (double)(gh - f.size)));   // let go in place: it carries on from here
      }
      g_grabbed = -1; ReleaseCapture(); return 0;
    }
  }
  return DefWindowProcW(w, m, wp, lp);
}
}  // namespace

void init(HWND main_window) {
  g_main = main_window;
  Gdiplus::GdiplusStartupInput in; Gdiplus::GdiplusStartup(&g_gdiplus, &in, nullptr);
  WNDCLASSW wc{}; wc.lpfnWndProc = proc; wc.hInstance = GetModuleHandleW(nullptr); wc.lpszClassName = L"MeleeUnlockedEgg"; wc.hCursor = LoadCursorW(nullptr, IDC_HAND);
  RegisterClassW(&wc);
  g_overlay = CreateWindowExW(WS_EX_LAYERED | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, L"MeleeUnlockedEgg", L"", WS_POPUP, 0, 0, 1, 1, g_main, nullptr, GetModuleHandleW(nullptr), nullptr);
  SetTimer(g_overlay, 1, 16, nullptr);
  // Test runs: "walter,bailey,..." sends those faces at start (a hidden desktop has no keyboard to type the code).
  if (const char* t = std::getenv("MELEE_LAUNCHER_TEST") ? std::getenv("MELEE_LAUNCHER_TEST_EGG") : nullptr)
    for (const char* q = t; *q; ) { send(std::strncmp(q, "walter", 6) == 0 ? 1 : 0); const char* c = std::strchr(q, ','); if (!c) break; q = c + 1; }
}
void feed_pad(uint16_t buttons) { if (const int code = g_pad.feed(buttons)) send(code == kWalter ? 1 : 0); }
void main_moved() { if (g_overlay && !g_faces.empty()) draw(); }

}  // namespace launcher::egg
