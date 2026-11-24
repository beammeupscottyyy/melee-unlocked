"""Bakes the skin's filled icons (24x24 SVG paths, the shapes the owner picked in the B4 mockup; most are Material
Icons shapes, Apache-2.0) into port/app/launcher_skin_icons.inc as cubic Bezier outlines for GDI+.
Each icon: {subpath count, then per subpath: segment count, x0, y0, then 6 floats per cubic segment}."""
import math, re
from pathlib import Path

ICONS = {
    "eye": "M12 4.5C7 4.5 2.7 7.6 1 12c1.7 4.4 6 7.5 11 7.5s9.3-3.1 11-7.5c-1.7-4.4-6-7.5-11-7.5zM12 17a5 5 0 1 1 0-10 5 5 0 0 1 0 10zm0-8a3 3 0 1 0 0 6 3 3 0 0 0 0-6z",
    "play": "M8 5v14l11-7z",
    "film": "M18 4l2 4h-3l-2-4h-2l2 4h-3l-2-4H8l2 4H7L5 4H4a2 2 0 0 0-2 2v12a2 2 0 0 0 2 2h16a2 2 0 0 0 2-2V4z",
    "mods": "M20.5 11H19V7a2 2 0 0 0-2-2h-4V3.5a2.5 2.5 0 0 0-5 0V5H4a2 2 0 0 0-2 2v3.8h1.5a2.7 2.7 0 0 1 0 5.4H2V20a2 2 0 0 0 2 2h3.8v-1.5a2.7 2.7 0 0 1 5.4 0V22H17a2 2 0 0 0 2-2v-4h1.5a2.5 2.5 0 0 0 0-5z",
    "build": "M22.7 19l-9.1-9.1c.9-2.3.4-5-1.5-6.9-2-2-5-2.4-7.4-1.3L9 6 6 9 1.6 4.7C.4 7.1.9 10.1 2.9 12.1c1.9 1.9 4.6 2.4 6.9 1.5l9.1 9.1c.4.4 1 .4 1.4 0l2.3-2.3c.5-.4.5-1.1.1-1.4z",
    "gear": "M19.4 13a7.5 7.5 0 0 0 0-2l2.1-1.6-2-3.5-2.5 1a7.3 7.3 0 0 0-1.7-1l-.4-2.6h-4l-.4 2.6a7.3 7.3 0 0 0-1.7 1l-2.5-1-2 3.5L4.6 11a7.5 7.5 0 0 0 0 2l-2.1 1.6 2 3.5 2.5-1c.5.4 1.1.7 1.7 1l.4 2.6h4l.4-2.6c.6-.3 1.2-.6 1.7-1l2.5 1 2-3.5zM12 15.5A3.5 3.5 0 1 1 12 8.5a3.5 3.5 0 0 1 0 7z",
    "palette": "M12 3a9 9 0 0 0 0 18c.8 0 1.5-.7 1.5-1.5 0-.4-.1-.7-.4-1-.2-.3-.4-.6-.4-1 0-.8.7-1.5 1.5-1.5H16a5 5 0 0 0 5-5c0-4.4-4-8-9-8zm-5.5 9a1.5 1.5 0 1 1 0-3 1.5 1.5 0 0 1 0 3zm3-4a1.5 1.5 0 1 1 0-3 1.5 1.5 0 0 1 0 3zm5 0a1.5 1.5 0 1 1 0-3 1.5 1.5 0 0 1 0 3zm3 4a1.5 1.5 0 1 1 0-3 1.5 1.5 0 0 1 0 3z",
    "bug": "M20 8h-2.8a6 6 0 0 0-1.8-2l1.6-1.6L15.6 3l-2.2 2.2a6 6 0 0 0-2.8 0L8.4 3 7 4.4 8.6 6a6 6 0 0 0-1.8 2H4v2h2.1a6 6 0 0 0 0 1H4v2h2.1v1H4v2h2.8a6 6 0 0 0 10.4 0H20v-2h-2.1v-1H20v-2h-2.1a6 6 0 0 0 0-1H20zm-6 8h-4v-2h4zm0-4h-4v-2h4z",
    "people": "M16 11a3 3 0 1 0 0-6 3 3 0 0 0 0 6zm-8 0a3 3 0 1 0 0-6 3 3 0 0 0 0 6zm0 2c-2.3 0-7 1.2-7 3.5V19h14v-2.5C15 14.2 10.3 13 8 13zm8 0h-1c1.2.9 2 2 2 3.5V19h6v-2.5c0-2.3-4.7-3.5-7-3.5z",
    "rules": "M14 2H6a2 2 0 0 0-2 2v16a2 2 0 0 0 2 2h12a2 2 0 0 0 2-2V8zm2 16H8v-2h8zm0-4H8v-2h8zm-3-5V3.5L18.5 9z",
    "search": "M15.5 14h-.79l-.28-.27C15.41 12.59 16 11.11 16 9.5 16 5.91 13.09 3 9.5 3S3 5.91 3 9.5 5.91 16 9.5 16c1.61 0 3.09-.59 4.23-1.57l.27.28v.79l5 4.99L20.49 19l-4.99-5zm-6 0C7.01 14 5 11.99 5 9.5S7.01 5 9.5 5 14 7.01 14 9.5 11.99 14 9.5 14z",
    "friend": "M15 12a4 4 0 1 0 0-8 4 4 0 0 0 0 8zm-9-2V7H4v3H1v2h3v3h2v-3h3v-2zm9 4c-2.7 0-8 1.3-8 4v2h16v-2c0-2.7-5.3-4-8-4z",
    "person": "M12 12a4 4 0 1 0 0-8 4 4 0 0 0 0 8zm0 2c-2.7 0-8 1.3-8 4v2h16v-2c0-2.7-5.3-4-8-4z",
}
# globe is long; the mockup's full path
ICONS["globe"] = ("M12 2a10 10 0 1 0 0 20 10 10 0 0 0 0-20zm6.9 6h-3a15.6 15.6 0 0 0-1.4-3.6A8 8 0 0 1 18.9 8zM12 4c.8 1.2 1.5 2.5 "
                  "1.9 4h-3.8c.4-1.4 1.1-2.8 1.9-4zM4.3 14a8 8 0 0 1 0-4h3.4a16.5 16.5 0 0 0 0 4zm.8 2h3a15.6 15.6 0 0 0 1.4 3.6A8 8 0 0 "
                  "1 5.1 16zm3-8h-3a8 8 0 0 1 4.3-3.6C7.8 5.5 7.4 6.7 7.1 8zM12 20c-.8-1.2-1.5-2.5-1.9-4h3.8c-.4 1.4-1.1 2.8-1.9 4zm2.3-6H9.7a14.7 "
                  "14.7 0 0 1 0-4h4.6a14.7 14.7 0 0 1 0 4zm.3 5.6c.6-1.1 1.1-2.3 1.4-3.6h3a8 8 0 0 1-4.4 3.6zm1.8-5.6a16.5 16.5 0 0 0 0-4h3.4a8 8 0 0 1 0 4z")

TOK = re.compile(r"[MmLlHhVvCcSsQqAaZz]|-?(?:\d+\.?\d*|\.\d+)(?:[eE][-+]?\d+)?")

def arc_to_cubics(x1, y1, rx, ry, phi, fa, fs, x2, y2):
    if rx == 0 or ry == 0: return [(x1, y1, x2, y2, x2, y2)]
    rx, ry = abs(rx), abs(ry); cp, sp = math.cos(math.radians(phi)), math.sin(math.radians(phi))
    dx, dy = (x1 - x2) / 2, (y1 - y2) / 2
    x1p, y1p = cp * dx + sp * dy, -sp * dx + cp * dy
    lam = x1p ** 2 / rx ** 2 + y1p ** 2 / ry ** 2
    if lam > 1: rx *= math.sqrt(lam); ry *= math.sqrt(lam)
    num = rx * rx * ry * ry - rx * rx * y1p * y1p - ry * ry * x1p * x1p
    den = rx * rx * y1p * y1p + ry * ry * x1p * x1p
    co = math.sqrt(max(0, num / den)) * (-1 if fa == fs else 1)
    cxp, cyp = co * rx * y1p / ry, -co * ry * x1p / rx
    cx, cy = cp * cxp - sp * cyp + (x1 + x2) / 2, sp * cxp + cp * cyp + (y1 + y2) / 2
    def ang(ux, uy, vx, vy):
        a = math.atan2(ux * vy - uy * vx, ux * vx + uy * vy); return a
    t1 = ang(1, 0, (x1p - cxp) / rx, (y1p - cyp) / ry)
    dt = ang((x1p - cxp) / rx, (y1p - cyp) / ry, (-x1p - cxp) / rx, (-y1p - cyp) / ry)
    if not fs and dt > 0: dt -= 2 * math.pi
    if fs and dt < 0: dt += 2 * math.pi
    n = max(1, math.ceil(abs(dt) / (math.pi / 2))); d = dt / n; k = 4 / 3 * math.tan(d / 4)
    out = []
    for i in range(n):
        a1, a2 = t1 + i * d, t1 + (i + 1) * d
        def pt(a, r=1):
            x, y = rx * math.cos(a), ry * math.sin(a); return cp * x - sp * y + cx, sp * x + cp * y + cy
        def dv(a):
            x, y = -rx * math.sin(a), ry * math.cos(a); return cp * x - sp * y, sp * x + cp * y
        p1, p2 = pt(a1), pt(a2); d1, d2 = dv(a1), dv(a2)
        out.append((p1[0] + k * d1[0], p1[1] + k * d1[1], p2[0] - k * d2[0], p2[1] - k * d2[1], p2[0], p2[1]))
    return out

def parse(d):
    toks = TOK.findall(d); i = 0; subs = []; cur = None; x = y = sx = sy = 0; cmd = None; lc = None
    def num():
        nonlocal i; v = float(toks[i]); i += 1; return v
    def line(nx, ny):
        cur["segs"].append((x, y, nx, ny, nx, ny))
    while i < len(toks):
        if toks[i].isalpha(): cmd = toks[i]; i += 1
        elif cmd in "Mm": cmd = "l" if cmd == "m" else "L"
        rel = cmd.islower(); C = cmd.upper()
        if C == "Z":
            if cur and (abs(x - sx) > 1e-9 or abs(y - sy) > 1e-9): line(sx, sy)
            x, y = sx, sy; lc = None; continue
        if C == "M":
            nx, ny = num(), num()
            if rel: nx += x; ny += y
            cur = {"x0": nx, "y0": ny, "segs": []}; subs.append(cur); x, y = sx, sy = nx, ny; lc = None; continue
        if C == "L":
            nx, ny = num(), num()
            if rel: nx += x; ny += y
            line(nx, ny); x, y = nx, ny; lc = None
        elif C == "H":
            nx = num() + (x if rel else 0); line(nx, y); x = nx; lc = None
        elif C == "V":
            ny = num() + (y if rel else 0); line(x, ny); y = ny; lc = None
        elif C in "CS":
            if C == "C":
                c1x, c1y = num(), num()
                if rel: c1x += x; c1y += y
            else:
                c1x, c1y = (2 * x - lc[0], 2 * y - lc[1]) if lc else (x, y)
            c2x, c2y, nx, ny = num(), num(), num(), num()
            if rel: c2x += x; c2y += y; nx += x; ny += y
            cur["segs"].append((c1x, c1y, c2x, c2y, nx, ny)); lc = (c2x, c2y); x, y = nx, ny
        elif C == "Q":
            qx, qy, nx, ny = num(), num(), num(), num()
            if rel: qx += x; qy += y; nx += x; ny += y
            cur["segs"].append((x + 2 / 3 * (qx - x), y + 2 / 3 * (qy - y), nx + 2 / 3 * (qx - nx), ny + 2 / 3 * (qy - ny), nx, ny)); x, y = nx, ny; lc = None
        elif C == "A":
            rx, ry, phi, fa, fs, nx, ny = num(), num(), num(), num(), num(), num(), num()
            if rel: nx += x; ny += y
            cur["segs"].extend(arc_to_cubics(x, y, rx, ry, phi, int(fa), int(fs), nx, ny)); x, y = nx, ny; lc = None
    return subs

out = ["// Generated by tools/bake_skin_icons.py: the skin's filled icons as cubic outlines on a 24x24 grid.",
       "// Layout per icon: subpaths, then for each: segments, x0, y0, and 6 floats per cubic. Shapes after Material Icons (Apache-2.0).",
       "// SPDX-License-Identifier: GPL-2.0-or-later", ""]
names = []
def cnum(v):
    t = f"{float(v):.5g}"
    if "." not in t and "e" not in t: t += "."
    return t + "f"
for name, d in ICONS.items():
    subs = parse(d); vals = [len(subs)]
    for s in subs:
        vals += [len(s["segs"]), s["x0"], s["y0"]]
        for seg in s["segs"]: vals += list(seg)
    out.append(f"const float kIcon_{name}[] = {{" + ",".join(cnum(v) for v in vals) + "};")
    names.append(name)
out.append("enum IconId { " + ", ".join(f"I_{n.upper()}" for n in names) + ", I_COUNT };")
out.append("const float* const kIcons[I_COUNT] = {" + ", ".join(f"kIcon_{n}" for n in names) + "};")
p = Path(__file__).resolve().parents[1] / "port/app/launcher_skin_icons.inc"
p.write_text("\n".join(out) + "\n", encoding="utf-8")
print("wrote", p, len(names), "icons")
