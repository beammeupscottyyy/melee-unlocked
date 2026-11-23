#pragma once
// Does a stage's online form draw its floors where its collision is?
//
// An online stage form keeps the original's collision. A skin can still draw its platforms elsewhere:
// 20XX's tilting Battlefield (GrNBa.lat) draws its side platforms tilted and tilts their collision
// with its own game code, which this game does not run, so online the art sloped across flat
// platforms nobody could see (CHARA, 11-23). Checked at the rest pose: for each floor line, the top
// drawn surface near the line, sampled along it. Only lines whose art the original itself draws on
// the line at rest are judged; most stages place theirs by animation, and those are left alone.
#include "stage_dat_safety.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <map>
#include <mutex>
#include <set>
#include <string>
#include <vector>

namespace host::cosmetics::stage_safety {
namespace floor_fit {

using Mat = std::array<float, 12>;   // 3x4, row major
struct Vec { float x, y, z; };
struct Tri { Vec v[3]; };

inline float f32(const Archive& z, uint32_t at) {
  const uint32_t w = z.get(at);
  float f; std::memcpy(&f, &w, 4); return f;
}
inline uint16_t u16(const Archive& z, uint32_t at) {
  const uint8_t* p = z.bytes.data() + 32ull + at; return uint16_t((p[0] << 8) | p[1]);
}
inline uint8_t u8(const Archive& z, uint32_t at) { return z.bytes[32ull + at]; }

inline Mat mul(const Mat& a, const Mat& b) {
  Mat m{};
  for (int r = 0; r < 3; ++r) {
    for (int c = 0; c < 4; ++c) {
      float s = c == 3 ? a[r * 4 + 3] : 0.0f;
      for (int k = 0; k < 3; ++k) s += a[r * 4 + k] * b[k * 4 + c];
      m[r * 4 + c] = s;
    }
  }
  return m;
}
inline Mat identity() { return {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0}; }
inline Vec apply(const Mat& m, const Vec& v) {
  return {m[0] * v.x + m[1] * v.y + m[2] * v.z + m[3], m[4] * v.x + m[5] * v.y + m[6] * v.z + m[7],
          m[8] * v.x + m[9] * v.y + m[10] * v.z + m[11]};
}
// A joint's own transform: rotation X, Y, Z (HSD's order), scale, translation.
inline Mat local(const Archive& z, uint32_t j) {
  float t[9];
  for (int k = 0; k < 9; ++k) t[k] = f32(z, j + 0x14 + 4 * k);
  const float cx = std::cos(t[0]), sx = std::sin(t[0]), cy = std::cos(t[1]), sy = std::sin(t[1]);
  const float cz = std::cos(t[2]), sz = std::sin(t[2]);
  const float r[9] = {cy * cz, cz * sx * sy - cx * sz, cx * cz * sy + sx * sz,
                      cy * sz, cx * cz + sx * sy * sz, cx * sy * sz - cz * sx,
                      -sy, cy * sx, cx * cy};
  return {r[0] * t[3], r[1] * t[4], r[2] * t[5], t[6], r[3] * t[3], r[4] * t[4], r[5] * t[5], t[7],
          r[6] * t[3], r[7] * t[4], r[8] * t[5], t[8]};
}

struct Mesh {
  const Archive& z;
  std::vector<Tri> tris;
  std::map<uint32_t, Mat> world;
  unsigned budget = 4000000;   // vertices read; a broken file stops here instead of running on
  explicit Mesh(const Archive& a) : z(a) {}

  bool ptr(uint32_t at) const { return z.pointers.count(at) != 0; }

  bool bind(uint32_t j, Mat* out) const {
    const auto w = world.find(j);
    if (w == world.end()) return false;
    if (ptr(j + 0x38)) {
      const uint32_t ib = z.get(j + 0x38);
      if (!z.bounds(ib, 48)) return false;
      Mat inv{};
      for (int k = 0; k < 12; ++k) inv[k] = f32(z, ib + 4 * k);
      *out = mul(w->second, inv);
    } else {
      *out = w->second;
    }
    return true;
  }

  void pobj(uint32_t p, const Mat& owner) {
    if (!z.bounds(p, 0x18) || !ptr(p + 8) || !ptr(p + 0x10)) return;
    struct Desc { uint32_t attr, type, cnt, comp; uint8_t frac; uint16_t stride; uint32_t data; bool has_data; };
    std::vector<Desc> descs;
    for (uint32_t vl = z.get(p + 8); descs.size() <= 32; vl += 24) {
      if (!z.bounds(vl, 24)) return;
      const uint32_t attr = z.get(vl);
      if (attr == 0xFF) break;
      descs.push_back({attr, z.get(vl + 4), z.get(vl + 8), z.get(vl + 12), u8(z, vl + 16), u16(z, vl + 18),
                       z.get(vl + 20), ptr(vl + 20)});
    }
    if (descs.size() > 32) return;
    const uint16_t flags = u16(z, p + 12);
    const uint32_t kind = flags & 0x3000;
    Mat base = owner;
    if (kind == 0 && ptr(p + 0x14)) { Mat m; if (bind(z.get(p + 0x14), &m)) base = m; }
    std::vector<Mat> envelopes;
    if (kind == 0x2000 && ptr(p + 0x14)) {
      const uint32_t list = z.get(p + 0x14);
      for (uint32_t k = 0; k < 64 && ptr(list + 4 * k); ++k) {
        const uint32_t ed = z.get(list + 4 * k);
        Mat sum{};
        for (uint32_t q = 0; q < 16 && ptr(ed + 8 * q); ++q) {
          Mat m;
          if (!bind(z.get(ed + 8 * q), &m)) continue;
          const float weight = f32(z, ed + 8 * q + 4);
          for (int e = 0; e < 12; ++e) sum[e] += weight * m[e];
        }
        envelopes.push_back(sum);
      }
    }
    auto size_of = [](uint32_t comp) -> uint32_t { return comp <= 1 ? 1 : comp <= 3 ? 2 : 4; };
    auto attr_bytes = [&](const Desc& d) -> uint32_t {
      if (d.type == 2) return 1;
      if (d.type == 3) return 2;
      if (d.type != 1) return 0;
      if (d.attr <= 8) return 1;
      if (d.attr == 9) return (d.cnt == 1 ? 3 : 2) * size_of(d.comp);
      if (d.attr == 10) return (d.cnt ? 9 : 3) * size_of(d.comp);
      if (d.attr == 11 || d.attr == 12) { static const uint32_t c[6] = {2, 3, 4, 2, 3, 4}; return d.comp < 6 ? c[d.comp] : 4; }
      if (d.attr >= 13 && d.attr <= 20) return (d.cnt ? 2 : 1) * size_of(d.comp);
      return 0;
    };
    int position = -1, matrix_index = -1;
    uint32_t vertex = 0, pos_at = 0, pn_at = 0;
    for (size_t k = 0; k < descs.size(); ++k) {
      if (descs[k].attr == 9) { position = (int)k; pos_at = vertex; }
      if (descs[k].attr == 0) { matrix_index = (int)k; pn_at = vertex; }
      vertex += attr_bytes(descs[k]);
    }
    if (position < 0 || !vertex) return;
    const Desc& pos = descs[position];
    // Positions by index into an array (8 or 16 bit), or written in the display list itself (direct:
    // Arcade's Dream Land floor).
    if (pos.type == 1 ? false : (pos.type != 2 && pos.type != 3) || !pos.has_data) return;
    const uint32_t ncomp = pos.cnt == 1 ? 3 : 2, csize = size_of(pos.comp);
    const uint32_t stride = pos.stride ? pos.stride : ncomp * csize;
    const float scale = pos.comp == 4 ? 1.0f : 1.0f / float(1u << (pos.frac & 31));
    auto read = [&](uint32_t at, const Mat& m, Vec* out) {
      if (!z.bounds(at, ncomp * csize)) return false;
      float c[3] = {0, 0, 0};
      for (uint32_t k = 0; k < ncomp; ++k) {
        const uint32_t a = at + k * csize;
        switch (pos.comp) {
          case 4: c[k] = f32(z, a); break;
          case 3: c[k] = float(int16_t(u16(z, a))) * scale; break;
          case 2: c[k] = float(u16(z, a)) * scale; break;
          case 1: c[k] = float(int8_t(u8(z, a))) * scale; break;
          default: c[k] = float(u8(z, a)) * scale; break;
        }
      }
      *out = apply(m, {c[0], c[1], c[2]});
      return std::isfinite(out->x) && std::isfinite(out->y) && std::isfinite(out->z);
    };
    uint32_t o = z.get(p + 0x10);
    const uint32_t end = o + uint32_t(u16(z, p + 14)) * 32;
    if (!z.bounds(o, end - o)) return;
    while (o + 3 <= end) {
      const uint8_t op = u8(z, o);
      if (op == 0) { ++o; continue; }
      const uint8_t prim = op & 0xF8;
      if (prim != 0x80 && prim != 0x90 && prim != 0x98 && prim != 0xA0) break;
      const uint32_t n = u16(z, o + 1);
      o += 3;
      if (uint64_t(o) + uint64_t(n) * vertex > end || n > budget) return;
      budget -= n;
      std::vector<Vec> vs(n);
      for (uint32_t i = 0; i < n; ++i) {
        const uint32_t at = o + i * vertex;
        const uint32_t source = pos.type == 1 ? at + pos_at
                                : pos.data + (pos.type == 2 ? u8(z, at + pos_at) : u16(z, at + pos_at)) * stride;
        const Mat* m = &base;
        if (matrix_index >= 0 && !envelopes.empty()) {
          const uint32_t e = u8(z, at + pn_at) / 3;
          if (e < envelopes.size()) m = &envelopes[e];
        }
        if (!read(source, *m, &vs[i])) return;
      }
      o += n * vertex;
      if (prim == 0x90) for (uint32_t i = 0; i + 2 < n; i += 3) tris.push_back({{vs[i], vs[i + 1], vs[i + 2]}});
      else if (prim == 0x80) for (uint32_t i = 0; i + 3 < n; i += 4) {
        tris.push_back({{vs[i], vs[i + 1], vs[i + 2]}}); tris.push_back({{vs[i], vs[i + 2], vs[i + 3]}});
      }
      else if (prim == 0x98) for (uint32_t i = 0; i + 2 < n; ++i) tris.push_back({{vs[i], vs[i + 1], vs[i + 2]}});
      else for (uint32_t i = 1; i + 1 < n; ++i) tris.push_back({{vs[0], vs[i], vs[i + 1]}});
      if (tris.size() > 2000000) return;
    }
  }

  // Every model's joints (map_head +8, +12), their world matrices first, then the meshes the game
  // draws, as HSD_JObjDispAll walks them: a joint's own meshes when the file gives it a pass bit
  // (opaque, translucent or texture edge, 0x1C0000; the loader copies joint flags as they are). Its
  // children are always visited: the subtree bits (0x70000000) are rebuilt from the children when
  // the tree is put together (UpdateParentTrspBits), whatever the file says (Arcade's Dream Land
  // floor sits under joints saved without them). Hidden (0x10), particle and spline (0x4020) and
  // instance (0x1000) joints draw none. A skin hides art by clearing the pass bits (20XX's tilting
  // Battlefield does on its flat platforms).
  bool build() {
    if (!z.roots.count("map_head")) return false;
    const uint32_t h = z.roots.at("map_head");
    if (!ptr(h + 8)) return false;
    const uint32_t models = z.get(h + 8), count = z.get(h + 12);
    if (count > 256) return false;
    std::function<void(uint32_t, const Mat&, unsigned)> place = [&](uint32_t j, const Mat& parent, unsigned depth) {
      for (unsigned guard = 0; depth < 200 && guard < 4096 && z.bounds(j, 0x40) && !world.count(j); ++guard) {
        const Mat w = mul(parent, local(z, j));
        world[j] = w;
        if (ptr(j + 8)) place(z.get(j + 8), w, depth + 1);
        if (!ptr(j + 12)) break;
        j = z.get(j + 12);
      }
    };
    for (uint32_t i = 0; i < count; ++i) if (ptr(models + i * 52)) place(z.get(models + i * 52), identity(), 0);
    std::vector<uint32_t> drawn;
    std::set<uint32_t> seen;
    std::function<void(uint32_t, unsigned)> visit = [&](uint32_t j, unsigned depth) {
      for (unsigned guard = 0; depth < 200 && guard < 4096 && world.count(j) && seen.insert(j).second; ++guard) {
        const uint32_t flags = z.get(j + 4);
        if (!(flags & 0x1000)) {
          if ((flags & 0x1C0000) && !(flags & 0x10) && !(flags & 0x4020)) drawn.push_back(j);
          if (ptr(j + 8)) visit(z.get(j + 8), depth + 1);
        }
        if (!ptr(j + 12)) break;
        j = z.get(j + 12);
      }
    };
    for (uint32_t i = 0; i < count; ++i) if (ptr(models + i * 52)) visit(z.get(models + i * 52), 0);
    for (uint32_t j : drawn) {
      const Mat& w = world.at(j);
      if (!ptr(j + 0x10)) continue;
      uint32_t d = z.get(j + 0x10);
      for (unsigned k = 0; k < 256 && z.bounds(d, 16); ++k) {
        if (ptr(d + 12)) {
          uint32_t p = z.get(d + 12);
          for (unsigned kk = 0; kk < 256 && z.bounds(p, 0x18); ++kk) {
            pobj(p, w);
            if (!ptr(p + 4)) break;
            p = z.get(p + 4);
          }
        }
        if (!ptr(d + 4)) break;
        d = z.get(d + 4);
      }
    }
    return true;
  }
};

struct Line { float x0, y0, x1, y1; };
// The floor lines (coll_data: points +0, lines +8, the floor range +0x10).
inline bool floors(const Archive& z, std::vector<Line>* out) {
  if (!z.roots.count("coll_data")) return false;
  const uint32_t c = z.roots.at("coll_data");
  if (!z.bounds(c, 0x14) || !z.pointers.count(c) || !z.pointers.count(c + 8)) return false;
  const uint32_t points = z.get(c), lines = z.get(c + 8);
  const int start = int16_t(u16(z, c + 0x10)), n = int16_t(u16(z, c + 0x12));
  if (start < 0 || n < 0 || n > 4096) return false;
  for (int i = start; i < start + n; ++i) {
    const uint32_t l = lines + uint32_t(i) * 16;
    if (!z.bounds(l, 4)) return false;
    const uint32_t v0 = points + u16(z, l) * 8u, v1 = points + u16(z, l + 2) * 8u;
    if (!z.bounds(v0, 8) || !z.bounds(v1, 8)) return false;
    out->push_back({f32(z, v0), f32(z, v0 + 4), f32(z, v1), f32(z, v1 + 4)});
  }
  return true;
}

// The share of points along the line where the art stands on it, front to back within 40 units of
// the play plane. A point counts when either
//  - the nearest surface that faces up or down (a floor, not a wall or the front of a slab) is within
//    `tolerance` of the line and runs along it (within about 5 degrees), so decorations above a floor
//    do not count against it, or
//  - the highest drawn edge within `window` of the line is within `tolerance` of it: a floor drawn as
//    the top edge of a face (Arcade's Dream Land has no level top).
// A platform drawn tilted across a level line fails both away from its middle: its surfaces lean
// against the line and its top drifts off it.
inline float score(const std::vector<Tri>& tris, const Line& l, float tolerance, float window = 10.0f) {
  const int n = std::clamp(int(std::fabs(l.x1 - l.x0)), 8, 64);
  struct Near { const Tri* tri; bool flat; float slope; };   // slope: dy/dx along the play plane
  std::vector<Near> near;
  // Only triangles over the line's span and within `window` of its height can count (keeps a stage's
  // check to a few milliseconds: it runs for every stage skin when the catalog is read).
  const float lx0 = std::min(l.x0, l.x1), lx1 = std::max(l.x0, l.x1);
  const float ly0 = std::min(l.y0, l.y1) - window, ly1 = std::max(l.y0, l.y1) + window;
  for (const Tri& tri : tris) {
    const Vec& a = tri.v[0]; const Vec& b = tri.v[1]; const Vec& c = tri.v[2];
    if (std::min({a.z, b.z, c.z}) > 40.0f || std::max({a.z, b.z, c.z}) < -40.0f) continue;
    if (std::max({a.x, b.x, c.x}) < lx0 || std::min({a.x, b.x, c.x}) > lx1) continue;
    if (std::max({a.y, b.y, c.y}) < ly0 || std::min({a.y, b.y, c.y}) > ly1) continue;
    const float ux = b.x - a.x, uy = b.y - a.y, uz = b.z - a.z, vx = c.x - a.x, vy = c.y - a.y, vz = c.z - a.z;
    const float nx = uy * vz - uz * vy, ny = uz * vx - ux * vz, nz = ux * vy - uy * vx;
    const float len = std::sqrt(nx * nx + ny * ny + nz * nz);
    if (!(len > 0)) continue;
    const bool flat = std::fabs(ny) >= 0.7f * len;
    near.push_back({&tri, flat, flat ? -nx / ny : 0.0f});
  }
  const float line_slope = l.x1 != l.x0 ? (l.y1 - l.y0) / (l.x1 - l.x0) : 0.0f;
  const bool debug = std::getenv("MELEE_FLOOR_DEBUG") != nullptr;   // --floor-line in the stage test
  int good = 0;
  for (int s = 0; s < n; ++s) {
    const float t = (s + 0.5f) / n, x = l.x0 + (l.x1 - l.x0) * t, y = l.y0 + (l.y1 - l.y0) * t;
    float best = window, best_slope = 0, top = 0;
    bool any_top = false;
    for (const Near& f : near) {
      const Vec& a = f.tri->v[0]; const Vec& b = f.tri->v[1]; const Vec& c = f.tri->v[2];
      if (x < std::min({a.x, b.x, c.x}) || x > std::max({a.x, b.x, c.x})) continue;
      const Vec* edge[3][2] = {{&a, &b}, {&b, &c}, {&c, &a}};
      for (const auto& e : edge) {
        const Vec& p = *e[0]; const Vec& q = *e[1];
        if (p.x == q.x || (p.x - x) * (q.x - x) > 0) continue;
        const float yy = p.y + (q.y - p.y) * (x - p.x) / (q.x - p.x), d = std::fabs(yy - y);
        if (f.flat && d < best) { best = d; best_slope = f.slope; }
        if (d <= window && (!any_top || yy > top)) { top = yy; any_top = true; }
      }
    }
    const bool surface = best <= tolerance && std::fabs(best_slope - line_slope) <= 0.09f;
    const bool edge_top = any_top && std::fabs(top - y) <= tolerance;
    good += surface || edge_top;
    if (debug)
      std::fprintf(stderr, "  x %.1f y %.1f: nearest %.2f slope %.3f (line %.3f), top %.2f\n", x, y, best, best_slope,
                   line_slope, any_top ? top - y : window);
  }
  return float(good) / float(n);
}

inline bool scores(const std::vector<uint8_t>& file, std::vector<float>* out, float tolerance) {
  Archive z{file};
  if (!z.parse()) return false;
  std::vector<Line> lines;
  if (!floors(z, &lines)) return false;
  Mesh mesh{z};
  if (!mesh.build()) return false;
  // A line shorter than 20 units (a ledge's edge, a step) is not judged (-1): skins round and reshape
  // those, and a player cannot stand on one long enough to be misled. Platforms are 30 units and more.
  // A line with another floor over at least half of it within 10 units above is not judged either:
  // nobody stands there while that floor is over it (Pokemon Stadium's transformation floors sit
  // under the main floor at rest).
  auto covered = [&](const Line& l) {
    const float lo = std::min(l.x0, l.x1), hi = std::max(l.x0, l.x1), y = (l.y0 + l.y1) / 2;
    for (const Line& o : lines) {
      if (&o == &l) continue;
      const float overlap = std::min(hi, std::max(o.x0, o.x1)) - std::max(lo, std::min(o.x0, o.x1));
      const float above = (o.y0 + o.y1) / 2 - y;
      if (overlap >= 0.5f * (hi - lo) && above > 0.0f && above <= 10.0f) return true;
    }
    return false;
  };
  for (const Line& l : lines)
    out->push_back(std::hypot(l.x1 - l.x0, l.y1 - l.y0) < 20.0f || covered(l) ? -1.0f : score(mesh.tris, l, tolerance));
  return true;
}

}  // namespace floor_fit

constexpr float kFloorFitTolerance = 3.0f;

// False (with `why`) when the original draws a floor's art on its line at rest and `form` does not
// for most of that line. A file this cannot read is not judged here (true): the other checks decide.
inline bool floor_art_fits(const std::vector<uint8_t>& clean, const std::vector<uint8_t>& form, std::string* why,
                           float tolerance = kFloorFitTolerance) {
  // The original's scores are the same for every skin of a stage: kept by content (FNV-1a of the
  // bytes) and tolerance, so reading a catalog of stage skins decodes each original once.
  static std::mutex cache_mutex;
  static std::map<std::pair<uint64_t, float>, std::pair<bool, std::vector<float>>> cache;
  uint64_t key = 1469598103934665603ull ^ clean.size();
  for (uint8_t byte : clean) key = (key ^ byte) * 1099511628211ull;
  std::vector<float> a, b;
  bool read_clean;
  {
    std::lock_guard<std::mutex> lock(cache_mutex);
    const auto hit = cache.find({key, tolerance});
    if (hit != cache.end()) { read_clean = hit->second.first; a = hit->second.second; }
    else {
      read_clean = floor_fit::scores(clean, &a, tolerance);
      if (cache.size() < 256) cache[{key, tolerance}] = {read_clean, a};
    }
  }
  if (!read_clean || !floor_fit::scores(form, &b, tolerance) || a.size() != b.size()) return true;
  for (size_t i = 0; i < a.size(); ++i) {
    if (a[i] >= 0.95f && b[i] < 0.5f) {
      if (why) *why = "the skin draws floor " + std::to_string(i) + " away from its collision (" +
                      std::to_string(int(b[i] * 100)) + "% of it on the line)";
      return false;
    }
  }
  return true;
}

}  // namespace host::cosmetics::stage_safety
