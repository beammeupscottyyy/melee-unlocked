"""The recipe that turns an NKit image of Melee NTSC 1.02 back into the retail disc (port/app/disc_rvz.cpp,
NKit). Run once against a clean retail ISO; it writes port/app/nkit_melee102.inc.

An NKit image keeps the disc's files and drops the padding between them (Nintendo's mastering tool filled
it with a lagged Fibonacci generator's output, restarted every 0x8000 bytes). The recipe says where each
part of the retail disc goes and how to make its padding again: zero runs, and for each padding run the
generator's 17 seed words, found by running the generator backwards from the padding itself (each is
checked by generating it again). A few short runs hold no whole generator cycle and are kept as bytes.
It holds no game data: the files, the DOL, the apploader and the header come from the player's image, and
the result must have the retail disc's MD5 or it is thrown away.

usage: py -3 tools/nkit_recipe.py <retail Melee NTSC 1.02 .iso> [port/app/nkit_melee102.inc]
"""
import hashlib, os, struct, sys


K, J, SEED = 521, 32, 17
M = 0xFFFFFFFF


def _forward(b):
    for i in range(J):
        b[i] ^= b[i + K - J]
    for i in range(J, K):
        b[i] ^= b[i - J]


def _backward(b):
    for i in range(K - 1, J - 1, -1):
        b[i] ^= b[i - J]
    for i in range(J):
        b[i] ^= b[i + K - J]


def generate(seed_words, count, skip=0):
    """`count` junk bytes from block offset `skip`, for these 17 seed words."""
    b = list(seed_words) + [0] * (K - SEED)
    for i in range(SEED, K):
        b[i] = ((b[i - 17] << 23) & M) ^ (b[i - 16] >> 9) ^ b[i - 1]
    b = [(x & 0xFF00FFFF) | ((x >> 2) & 0x00FF0000) for x in b]
    for _ in range(4):
        _forward(b)
    out = bytearray()
    pos = skip
    while pos >= K * 4:
        _forward(b)
        pos -= K * 4
    while len(out) < count:
        chunk = struct.pack(">%dI" % K, *b)[pos:]
        out += chunk[: count - len(out)]
        _forward(b)
        pos = 0
    return bytes(out)


def seed_of(data):
    """The seed whose output starts with these bytes (at least K*4), or None."""
    if len(data) < K * 4:
        return None
    b = list(struct.unpack(">%dI" % K, data[: K * 4]))
    for _ in range(4):
        _backward(b)
    # The output keeps bits 18..25 of each word in its byte 2 (shifted down by 2): undo that, and the
    # two bits it drops (16 and 17) follow from the recurrence (word k+17 holds word k+1 >> 9).
    seed = []
    for i in range(SEED):
        x = (b[i] & 0xFF00FFFF) | ((b[i] << 2) & 0x00FC0000)
        if i >= 1:
            x |= (((b[i + 16] ^ b[i + 15]) >> 7) & 3) << 16
        seed.append(x & M)
    if generate(seed, K * 4) != data[: K * 4]:
        return None
    return seed


def seed_of_words(tail, start):
    """As seed_of_tail, from any 521 consecutive generator words (start on a word boundary): the cycle they
    begin in is completed from the next one's words (word i of cycle n+1 is word i of cycle n xor word i-32
    of cycle n+1, or word i+489 of cycle n for i < 32)."""
    if start % 4 or len(tail) < K * 4:
        return None
    w0 = start // 4
    n, s = divmod(w0, K)
    words = struct.unpack(">%dI" % K, tail[: K * 4])
    cur = [None] * K
    nxt = [None] * K
    for k in range(K):
        if s + k < K:
            cur[s + k] = words[k]
        else:
            nxt[s + k - K] = words[k]
    for i in range(32, s):
        cur[i] = nxt[i] ^ nxt[i - 32]
    for i in range(min(32, s)):
        cur[i] = nxt[i] ^ cur[i + K - J]
    if any(x is None for x in cur):
        return None
    b = cur
    for _ in range(4 + n):
        _backward(b)
    seed = []
    for i in range(SEED):
        x = (b[i] & 0xFF00FFFF) | ((b[i] << 2) & 0x00FC0000)
        if i >= 1:
            x |= (((b[i + 16] ^ b[i + 15]) >> 7) & 3) << 16
        seed.append(x & M)
    if generate(seed, len(tail), start) != bytes(tail):
        return None
    return seed


def seed_of_tail(tail, start):
    """The block's seed from junk bytes that begin `start` bytes into the block and run to its end
    (or further), when they hold one whole 521-word generation of the generator; else None."""
    gen = -(-start // (K * 4))   # first generation that starts inside the known bytes
    lo = gen * K * 4 - start
    if lo + K * 4 > len(tail):
        return None
    b = list(struct.unpack(">%dI" % K, tail[lo: lo + K * 4]))
    for _ in range(4 + gen):
        _backward(b)
    seed = []
    for i in range(SEED):
        x = (b[i] & 0xFF00FFFF) | ((b[i] << 2) & 0x00FC0000)
        if i >= 1:
            x |= (((b[i + 16] ^ b[i + 15]) >> 7) & 3) << 16
        seed.append(x & M)
    if generate(seed, len(tail), start) != bytes(tail):
        return None
    return seed


BLOCK = 0x8000
T_BOOT, T_BI2, T_APP, T_DOL, T_FST, T_FILE, T_ZERO, T_JUNK, T_LIT = range(9)


def read_fst(f, fst_off, fst_size):
    f.seek(fst_off)
    raw = f.read(fst_size)
    n = struct.unpack(">I", raw[8:12])[0]
    ents = []
    for i in range(n):
        fl = raw[i * 12]
        a, b = struct.unpack(">II", raw[i * 12 + 4: i * 12 + 12])
        ents.append((fl, a, b))
    return raw, ents


def dol_size(f, off):
    f.seek(off)
    h = f.read(0x100)
    offs = struct.unpack(">18I", h[0:72])
    sizes = struct.unpack(">18I", h[0x90:0x90 + 72])
    return max(o + s for o, s in zip(offs, sizes) if s)


def apploader_size(f):
    f.seek(0x2440 + 0x14)
    a, b = struct.unpack(">II", f.read(8))
    return 0x20 + a + b


def make(iso_path, out_path):
    f = open(iso_path, "rb")
    size = os.fstat(f.fileno()).st_size
    f.seek(0x420)
    dol_off, fst_off, fst_size, fst_max = struct.unpack(">IIII", f.read(16))
    raw, ents = read_fst(f, fst_off, fst_size)
    parts = [(0, 0x440, T_BOOT, 0), (0x440, 0x2000, T_BI2, 0), (0x2440, apploader_size(f), T_APP, 0),
             (dol_off, dol_size(f, dol_off), T_DOL, 0), (fst_off, fst_size, T_FST, 0)]
    for i, (fl, a, b) in enumerate(ents):
        if i and not fl and b:
            parts.append((a, b, T_FILE, i))
    parts.sort()
    segs = []
    junk_bytes = lit_bytes = 0

    def gap(a, b):
        nonlocal junk_bytes, lit_bytes
        while a < b:
            end = min(b, (a // BLOCK + 1) * BLOCK)   # one block at a time: the generator restarts at each
            f.seek(a)
            d = f.read(end - a)
            z = 0
            while z < len(d) and d[z] == 0:
                z += 1
            if z:
                segs.append((T_ZERO, z))
            # The padding may itself begin with zero bytes: it starts at the 4-byte boundary at or below the
            # first nonzero byte (generator words are 4 bytes).
            back = (a + z) % 4
            if back and z >= back:
                z -= back
                segs.pop() if segs and segs[-1] == (T_ZERO, z + back) else None
                if z:
                    segs.append((T_ZERO, z))
            rest = d[z:]
            # Padding that begins inside a generator word (the file before it ended there): the word's
            # last bytes as they are, the rest from the generator.
            lead = (4 - (a + z) % 4) % 4
            if rest and lead and len(rest) > lead:
                segs.append((T_LIT, bytes(rest[:lead])))
                lit_bytes += lead
                rest = rest[lead:]
                z += lead
            if rest:
                start = (a + z) % BLOCK
                seed = (seed_of_tail(rest, start) or seed_of_words(rest, start)) if start % 4 == 0 else None
                if seed is None:
                    # the seed may be known from the start of this block's padding run if it runs to the
                    # block's end; otherwise the bytes themselves
                    segs.append((T_LIT, bytes(rest)))
                    lit_bytes += len(rest)
                else:
                    segs.append((T_JUNK, len(rest), start, seed))
                    junk_bytes += len(rest)
            a = end

    pos = 0
    for a, b, t, i in parts:
        if a < pos:
            raise SystemExit("overlap at %x" % a)
        gap(pos, a)
        segs.append((t, b, i))
        pos = a + b
    gap(pos, size)
    f.seek(0)
    md5 = hashlib.md5()
    while True:
        c = f.read(1 << 24)
        if not c:
            break
        md5.update(c)
    f.seek(0x200)
    boot200 = f.read(0x20)
    offsets = [a if (i and not fl) else 0 for i, (fl, a, b) in enumerate(ents)]
    out = bytearray(b"MUNK" + struct.pack(">IQ", 1, size) + md5.digest())
    out += struct.pack(">IIII", dol_off, fst_off, fst_size, fst_max) + boot200
    out += struct.pack(">I", len(offsets)) + b"".join(struct.pack(">I", o) for o in offsets)
    out += struct.pack(">I", len(segs))
    for s in segs:
        t = s[0]
        if t in (T_BOOT, T_BI2, T_APP, T_DOL, T_FST, T_FILE):
            out += struct.pack(">BII", t, s[1], s[2])
        elif t == T_ZERO:
            out += struct.pack(">BI", t, s[1])
        elif t == T_JUNK:
            out += struct.pack(">BII", t, s[1], s[2]) + struct.pack(">17I", *s[3])
        else:
            out += struct.pack(">BI", t, len(s[1])) + s[1]
    open(out_path, "wb").write(out)
    print("segments", len(segs), "junk", junk_bytes, "literal", lit_bytes, "recipe bytes", len(out))




def write_inc(recipe, path):
    lines = ["// Generated by tools/nkit_recipe.py from a clean Melee NTSC 1.02 disc: the layout of the retail disc",
             "// and its padding's generator seeds (no game data). See disc_rvz.cpp, NKit.",
             "static const unsigned char kMelee102Recipe[] = {"]
    for i in range(0, len(recipe), 24):
        lines.append("  " + ",".join(str(b) for b in recipe[i:i + 24]) + ",")
    lines.append("};")
    nl = chr(10)
    open(path, "w", newline=nl).write(nl.join(lines) + nl)


if __name__ == "__main__":
    out = sys.argv[2] if len(sys.argv) > 2 else os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "port", "app", "nkit_melee102.inc")
    tmp = out + ".recipe"
    make(sys.argv[1], tmp)
    write_inc(open(tmp, "rb").read(), out)
    os.remove(tmp)
