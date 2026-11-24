"""GitHub #135: TM-CE's lab Stadium choice is read only inside a TM-CE event, and the stage timers it
rewrites go back at the next stage start. Read anywhere else it skipped the random transformation pick the
other player's game makes: every online Stadium match after the lab desynced at its first change."""
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[2]
PATCH = (ROOT / 'sourceport/patches/melee-native.patch').read_text(encoding='utf-8', errors='replace')
START = PATCH.index('diff --git a/src/melee/gr/grpstadium.c')
STADIUM = PATCH[START:PATCH.index(chr(10) + 'diff --git ', START + 10)]


class StadiumLabLeftover(unittest.TestCase):
    def test_the_lab_choice_is_read_only_in_a_tmce_event(self):
        at = [m.start() for m in re.finditer(r'if \(mu_tmce_stadium_override', STADIUM)]
        self.assertEqual(len(at), 1)
        read = STADIUM[at[0]:at[0] + 200]
        self.assertIn('mu_tmce_active()', read)
        self.assertIn('gm_GetCurrentGameMode() == GM_EVENT', read)

    def test_stage_timers_go_back_at_stage_start(self):
        init = STADIUM[STADIUM.index('yakumono_param = Ground_GetYakumonoParam();'):]
        self.assertLess(init.index('mu_tmce_stadium_restore();'), init.index('mu_hp_stadium_param('))
        save = STADIUM[STADIUM.index('+void mu_tmce_stadium_transform('):]
        self.assertLess(save.index('mu_tmce_stadium_saved[0] = yakumono_param->x8;'),
                        save.index('yakumono_param->x8 = 0x1000001;'))


if __name__ == '__main__':
    unittest.main()
