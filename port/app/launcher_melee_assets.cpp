// Melee's art from the player's disc (launcher_melee_assets.h). The texture offsets are the 1.02 NTSC MnMaAll.usd's
// image data (each checked for ink before use); the font is main.dol data at practice_menu.cpp's addresses.
// SPDX-License-Identifier: GPL-2.0-or-later
#include "launcher_melee_assets.h"
#include <windows.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <mutex>

namespace launcher::melee_assets {
namespace {

std::mutex g_mx;
std::string g_iso;
bool g_loaded = false;
Image g_hand, g_rim, g_fill, g_poses[3];
Font g_font;

uint32_t be32(const uint8_t* p) { return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3]; }

bool read_at(std::ifstream& f, uint64_t at, void* out, size_t n) {
  f.clear(); f.seekg((std::streamoff)at); f.read((char*)out, (std::streamsize)n); return (size_t)f.gcount() == n;
}
// A file on the disc by name (the disc's file table), or empty.
std::vector<uint8_t> disc_file(std::ifstream& f, const char* wanted) {
  uint8_t h[8];
  if (!read_at(f, 0x424, h, 8)) return {};
  const uint32_t fst_off = be32(h), fst_size = be32(h + 4);
  if (fst_size < 12 || fst_size > 32u * 1024 * 1024) return {};
  std::vector<uint8_t> fst(fst_size);
  if (!read_at(f, fst_off, fst.data(), fst_size)) return {};
  const uint32_t count = be32(fst.data() + 8);
  if (!count || (uint64_t)count * 12 > fst_size) return {};
  for (uint32_t i = 1; i < count; ++i) {
    const uint8_t* e = fst.data() + (size_t)i * 12;
    if (e[0]) continue;   // a folder
    const uint32_t name_at = count * 12 + (be32(e) & 0xFFFFFF);
    if (name_at >= fst_size) continue;
    const char* name = (const char*)fst.data() + name_at;
    if (_stricmp(name, wanted) != 0) continue;
    const uint32_t off = be32(e + 4), size = be32(e + 8);
    if (size > 64u * 1024 * 1024) return {};
    std::vector<uint8_t> out(size);
    if (!read_at(f, off, out.data(), size)) return {};
    return out;
  }
  return {};
}

// GX I4 and IA4 textures (8x8 tiles), to white with alpha.
Image decode(const uint8_t* d, size_t avail, int w, int h, bool ia4) {
  Image im; const size_t need = (size_t)((w + 7) / 8) * ((h + (ia4 ? 3 : 7)) / (ia4 ? 4 : 8)) * 32;
  if (avail < need) return im;
  im.w = w; im.h = h; im.bgra.assign((size_t)w * h, 0);
  size_t o = 0;
  const int bw = 8, bh = ia4 ? 4 : 8;
  for (int by = 0; by < (h + bh - 1) / bh; ++by)
    for (int bx = 0; bx < (w + bw - 1) / bw; ++bx) {
      for (int y = 0; y < bh; ++y)
        for (int x = 0; x < bw; ++x) {
          const int X = bx * bw + x, Y = by * bh + y;
          uint32_t a = 0, i = 255;
          if (ia4) { const uint8_t v = d[o + (size_t)y * bw + x]; a = (v >> 4) * 17; i = (v & 15) * 17; }
          else { const uint8_t b = d[o + ((size_t)y * bw + x) / 2]; a = ((x & 1) ? (b & 15) : (b >> 4)) * 17; }
          if (X < w && Y < h) im.bgra[(size_t)Y * w + X] = a << 24 | i << 16 | i << 8 | i;
        }
      o += (size_t)bw * bh * (ia4 ? 8 : 4) / 8;
    }
  return im;
}
// IA8 (GX format 3): 4x4 blocks, two bytes a pixel, alpha then intensity.
Image decode_ia8(const uint8_t* d, size_t avail, int w, int h) {
  Image im; if (avail < (size_t)((w + 3) / 4) * ((h + 3) / 4) * 32) return im;
  im.w = w; im.h = h; im.bgra.assign((size_t)w * h, 0); size_t o = 0;
  for (int by = 0; by < (h + 3) / 4; ++by)
    for (int bx = 0; bx < (w + 3) / 4; ++bx) {
      for (int y = 0; y < 4; ++y) for (int x = 0; x < 4; ++x) {
        const int X = bx * 4 + x, Y = by * 4 + y; const uint32_t a = d[o + (y * 4 + x) * 2], i = d[o + (y * 4 + x) * 2 + 1];
        if (X < w && Y < h) im.bgra[(size_t)Y * w + X] = a << 24 | i << 16 | i << 8 | i;
      }
      o += 32;
    }
  return im;
}
bool inked(const Image& im) {
  size_t n = 0; for (uint32_t p : im.bgra) n += (p >> 24) > 128;
  return im.w && n > im.bgra.size() / 20;
}
// The rim's outline filled: everything not reachable from the border through clear pixels.
Image filled(const Image& rim) {
  Image f = rim; if (!rim.w) return f;
  std::vector<uint8_t> out((size_t)rim.w * rim.h, 0); std::vector<std::pair<int, int>> stack;
  for (int x = 0; x < rim.w; ++x) { stack.push_back({x, 0}); stack.push_back({x, rim.h - 1}); }
  for (int y = 0; y < rim.h; ++y) { stack.push_back({0, y}); stack.push_back({rim.w - 1, y}); }
  while (!stack.empty()) {
    auto [x, y] = stack.back(); stack.pop_back();
    if (x < 0 || y < 0 || x >= rim.w || y >= rim.h) continue;
    uint8_t& seen = out[(size_t)y * rim.w + x];
    if (seen || (rim.bgra[(size_t)y * rim.w + x] >> 24) >= 128) continue;
    seen = 1; stack.push_back({x + 1, y}); stack.push_back({x - 1, y}); stack.push_back({x, y + 1}); stack.push_back({x, y - 1});
  }
  for (size_t i = 0; i < out.size(); ++i) if (!out[i]) f.bgra[i] = 0xFFFFFFFFu;
  return f;
}

void load() {
  if (g_loaded) return;
  g_loaded = true;
  std::error_code ec;
  if (g_iso.empty() || !std::filesystem::is_regular_file(std::filesystem::u8path(g_iso), ec)) return;
  std::ifstream f(std::filesystem::u8path(g_iso), std::ios::binary);
  char id[6] = {};
  if (!read_at(f, 0, id, 6) || std::memcmp(id, "GALE01", 6) != 0) return;   // Melee NTSC only
  const std::vector<uint8_t> mn = disc_file(f, "MnMaAll.usd");
  if (mn.size() > 0x20) {
    const uint8_t* data = mn.data() + 0x20; const size_t len = mn.size() - 0x20;
    auto at = [&](size_t off, int w, int h, bool ia4) { return off < len ? decode(data + off, len - off, w, h, ia4) : Image{}; };
    g_hand = at(0x06fca0, 56, 80, false); if (!inked(g_hand)) g_hand = {};
    g_rim = at(0x043c40, 48, 32, true); if (!inked(g_rim)) g_rim = {};
    g_fill = filled(g_rim);
  }
  // The character select's hand poses (1.02: MnSlChr.usd, data + 0x51a0 / 0x87a0 / 0xbda0).
  const std::vector<uint8_t> sl = disc_file(f, "MnSlChr.usd");
  if (sl.size() > 0x20) {
    const uint8_t* data = sl.data() + 0x20; const size_t len = sl.size() - 0x20;
    const size_t at_[3] = {0x51a0, 0x87a0, 0xbda0};
    for (int i = 0; i < 3; ++i) { g_poses[i] = at_[i] < len ? decode_ia8(data + at_[i], len - at_[i], 72, 96) : Image{}; if (!inked(g_poses[i])) g_poses[i] = {}; }
  }
  // main.dol is not in the file table: the disc header gives its offset (0x420); its own header gives its length.
  std::vector<uint8_t> dol;
  {
    uint8_t h[4]; uint8_t dh[0x100];
    if (read_at(f, 0x420, h, 4) && read_at(f, be32(h), dh, sizeof dh)) {
      uint64_t end = 0;
      for (int s = 0; s < 18; ++s) { const uint32_t off = be32(dh + s * 4), size = be32(dh + 0x90 + s * 4); if (size) end = std::max<uint64_t>(end, (uint64_t)off + size); }
      if (end > 0x100 && end < 16u * 1024 * 1024) { dol.resize((size_t)end); if (!read_at(f, be32(h), dol.data(), dol.size())) dol.clear(); }
    }
  }
  if (dol.size() > 0x100) {
    auto read = [&](uint32_t addr, size_t n) -> std::vector<uint8_t> {
      for (int s = 0; s < 18; ++s) {
        const uint32_t off = be32(dol.data() + s * 4), a = be32(dol.data() + 0x48 + s * 4), size = be32(dol.data() + 0x90 + s * 4);
        if (size && addr >= a && addr + n <= (uint64_t)a + size && off + (addr - a) + n <= dol.size())
          return std::vector<uint8_t>(dol.begin() + off + (addr - a), dol.begin() + off + (addr - a) + n);
      }
      return {};
    };
    const auto codes = read(0x8040C680u, 576), sjis = read(0x8040C8C0u, 576), metrics = read(0x8040CB00u, 576), pixels = read(0x8040CD40u, 287 * 512);
    if (codes.size() == 576 && sjis.size() == 576 && metrics.size() == 576 && pixels.size() == 287 * 512) {
      auto sjis_for = [](char c) -> uint16_t {
        if (c >= '0' && c <= '9') return (uint16_t)(0x824F + (c - '0'));
        if (c >= 'A' && c <= 'Z') return (uint16_t)(0x8260 + (c - 'A'));
        if (c >= 'a' && c <= 'z') return (uint16_t)(0x8281 + (c - 'a'));
        switch (c) {
          case ',': return 0x8143; case '.': return 0x8144; case ':': return 0x8146; case ';': return 0x8147; case '?': return 0x8148;
          case '!': return 0x8149; case '^': return 0x814F; case '_': return 0x8151; case '/': return 0x815E; case '~': return 0x8160;
          case '|': return 0x8162; case '\'': return 0x8166; case '"': return 0x8168; case '(': return 0x8169; case ')': return 0x816A;
          case '[': return 0x816D; case ']': return 0x816E; case '{': return 0x816F; case '}': return 0x8170; case '+': return 0x817B;
          case '-': return 0x817C; case '=': return 0x8181; case '<': return 0x8183; case '>': return 0x8184; case '$': return 0x8190;
          case '%': return 0x8193; case '#': return 0x8194; case '&': return 0x8195; case '*': return 0x8196; case '@': return 0x8197;
          default: return 0;
        }
      };
      for (int c = 32; c < 127; ++c) {
        const uint16_t want = sjis_for((char)c); if (!want) continue;
        for (int i = 0; i < 288; ++i) {
          if (((sjis[i * 2] << 8) | sjis[i * 2 + 1]) != want) continue;
          const int t = ((codes[i * 2] << 8) | codes[i * 2 + 1]) - 0x2000;
          if (t >= 0 && t < 287) g_font.glyph[c] = Glyph{t, metrics[t * 2], metrics[t * 2 + 1]};
          break;
        }
      }
      Image& sh = g_font.sheet; sh.w = 1024; sh.h = 288; sh.bgra.assign((size_t)sh.w * sh.h, 0);
      for (int g = 0; g < 287; ++g) {
        const uint8_t* d = pixels.data() + (size_t)g * 512; const int ox = (g % 32) * 32, oy = (g / 32) * 32;
        for (int tile = 0; tile < 16; ++tile) for (int r = 0; r < 8; ++r) for (int c = 0; c < 8; ++c) {
          const uint8_t b = d[tile * 32 + r * 4 + c / 2]; const uint32_t v = (uint32_t)(((c & 1) ? (b & 15) : (b >> 4)) * 17);
          sh.bgra[(size_t)(oy + (tile / 4) * 8 + r) * sh.w + ox + (tile % 4) * 8 + c] = v << 24 | 0xFFFFFFu;
        }
      }
      int inked_letters = 0;
      for (const char* p = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789"; *p; ++p) {
        const Glyph& gl = g_font.glyph[(unsigned char)*p]; if (gl.index < 0) continue;
        const int ox = (gl.index % 32) * 32, oy = (gl.index / 32) * 32; bool any = false;
        for (int y = 0; y < 32 && !any; ++y) for (int x = 0; x < 32; ++x) if ((sh.bgra[(size_t)(oy + y) * sh.w + ox + x] >> 24) > 128) { any = true; break; }
        inked_letters += any;
      }
      g_font.ok = inked_letters == 36;   // a mod disc can hold other data here: then the system font stays
    }
  }
}
}  // namespace

void set_disc(const std::string& iso) { std::lock_guard<std::mutex> lock(g_mx); if (iso != g_iso) { g_iso = iso; g_loaded = false; g_hand = g_rim = g_fill = {}; for (auto& p : g_poses) p = {}; g_font = {}; } }
const Image& hand() { std::lock_guard<std::mutex> lock(g_mx); load(); return g_hand; }
const Image& hand_pose(int pose) { static const Image none; std::lock_guard<std::mutex> lock(g_mx); load(); return pose >= 0 && pose < 3 ? g_poses[pose] : none; }
const Image& button_rim() { std::lock_guard<std::mutex> lock(g_mx); load(); return g_rim; }
const Image& button_fill() { std::lock_guard<std::mutex> lock(g_mx); load(); return g_fill; }
const Font& font() { std::lock_guard<std::mutex> lock(g_mx); load(); return g_font; }

void dump(const std::string& dir) {
  auto save = [&](const Image& im, const char* name) {
    if (!im.w) return;
    BITMAPFILEHEADER fh{}; BITMAPINFOHEADER ih{}; ih.biSize = sizeof ih; ih.biWidth = im.w; ih.biHeight = -im.h; ih.biPlanes = 1; ih.biBitCount = 32;
    fh.bfType = 0x4D42; fh.bfOffBits = sizeof fh + sizeof ih; fh.bfSize = fh.bfOffBits + (DWORD)im.bgra.size() * 4;
    if (FILE* f = std::fopen((dir + "/" + name).c_str(), "wb")) { std::fwrite(&fh, sizeof fh, 1, f); std::fwrite(&ih, sizeof ih, 1, f); std::fwrite(im.bgra.data(), 4, im.bgra.size(), f); std::fclose(f); }
  };
  save(hand(), "asset-hand.bmp"); save(hand_pose(0), "asset-hand-point.bmp"); save(hand_pose(1), "asset-hand-grab.bmp"); save(hand_pose(2), "asset-hand-open.bmp"); save(button_rim(), "asset-rim.bmp"); save(button_fill(), "asset-fill.bmp"); save(font().sheet, "asset-font.bmp");
}

}  // namespace launcher::melee_assets
