"""Every game-library call to the host's command channel offers at least the host's minimum reply
capacity: the host refuses a smaller buffer outright, so the call fails and its feature silently stays off
(VS Mode Pro AI never played, 1.0.18)."""
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[2]
SHIM = ROOT / 'sourceport/game/shim'
# Answered by the host before the command channel's capacity rule (port/app/source_host.cpp):
# Doubles' frozen rules, 0xE6 query 11, reply exactly 10 bytes.
EXEMPT = {('mu_doubles.c', 'frozen')}
CALL = re.compile(r'mu_online_abi_command\(\s*[^,]+,[^,]+,[^,]+,\s*(\w+)\s*,\s*sizeof\s*\(?\s*(\w+)')


def minimum():
    for path in (ROOT / 'port/runtime/abi').glob('*.h'):
        m = re.search(r'#define\s+MU_SLIPPI_RESPONSE_CAPACITY\s+(\w+)', path.read_text(errors='replace'))
        if m:
            return int(m.group(1).rstrip('uU'), 0)
    raise AssertionError('MU_SLIPPI_RESPONSE_CAPACITY not found')


class ShimReplyCapacity(unittest.TestCase):
    def test_every_reply_buffer_meets_the_host_minimum(self):
        need = minimum()
        checked = 0
        for path in sorted(SHIM.glob('*.c')):
            text = path.read_text(errors='replace')
            for m in CALL.finditer(text):
                buf = m.group(1)
                if m.group(2) != buf or (path.name, buf) in EXEMPT:
                    continue
                before = text[:m.start()]
                decls = list(re.finditer(r'unsigned char\s+' + re.escape(buf) + r'\[(\w+)\]', before))
                if not decls:
                    continue
                size = decls[-1].group(1)
                value = int(size, 0) if size[0].isdigit() else None
                if value is None:
                    continue
                checked += 1
                self.assertGreaterEqual(value, need, f'{path.name}: {buf}[{size}] is below the host minimum {need}')
        self.assertGreater(checked, 5)


if __name__ == '__main__':
    unittest.main()
