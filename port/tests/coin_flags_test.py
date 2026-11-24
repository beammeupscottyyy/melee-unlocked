"""Coin Mode (Report #878): the two "may be collected" flags of a coin are declared with the console's bit
layout on both sides. The coin's own code writes them through UnkFlagStruct; the fighter's pickup read them
as plain bit fields, which a PC build puts at the other end of the byte, so fighters walked through coins."""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[2]
PATCH = (ROOT / 'sourceport/patches/melee-native.patch').read_text(encoding='utf-8', errors='replace')


class CoinFlags(unittest.TestCase):
    def test_fighter_side_flags_use_the_console_layout(self):
        start = PATCH.index('diff --git a/src/melee/it/itCharItems.h')
        section = PATCH[start:PATCH.index(chr(10) + 'diff --git ', start + 10)]
        lines = ['+    struct {', '+        /*  +18 ip+DEC */ u8 xDEC_b0 : 1;', '+        /*  +18 ip+DEC */ u8 xDEC_b1 : 1;', '+    } DISC_STRUCT;']
        at = 0
        for line in lines:
            at = section.index(line, at) + 1


if __name__ == '__main__':
    unittest.main()
