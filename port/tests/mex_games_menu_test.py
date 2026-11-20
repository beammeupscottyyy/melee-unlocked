"""m-ex's own menu under VS Mode (ACE 2.0.1 "Games"): the native menu reaches it, only Volleyball runs.

The menu rows come from the disc's MxMn.dat (shim/mu_mex_menu.c); the game side is menu kind 34 in
mnmain.c and the Volleyball session in gmvsmode.c, both carried by the native patch."""
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[2]


def source(path):
    return (ROOT / path).read_text(encoding="utf-8-sig")


def patch_section(name):
    patch = source("sourceport/patches/melee-native.patch")
    start = patch.index("+++ b/src/melee/" + name)
    end = patch.find("\ndiff --git ", start)
    return patch[start:end if end >= 0 else len(patch)]


class MexGamesMenuTest(unittest.TestCase):
    def test_vs_second_row_opens_the_added_menu_before_tournament(self):
        mn = patch_section("mn/mnmain.c")
        row = mn.index("+            if (mu_mex_games() > 0) {")
        self.assertLess(row, mn.index("data->pending_mode = mu_general_codes() && (mu_hp_loaded() || mu_te_general())"))
        self.assertIn("+                mn_804A04F0.cur_menu = MENU_KIND_34;", mn)
        # Only the array bound is conditional (one opening brace, for tools/gecko_targets.py).
        bound = "\n+#ifdef MU_NATIVE\n+    [0x23]\n+#else\n+    [0x22]\n+#endif\n+    "
        self.assertIn("MenuKindData mn_803EB6B0" + bound + "= {", mn)
        self.assertIn("static AnimLoopSettings mn_803EAE8C" + bound + "[3] = {", mn)
        self.assertIn("+    case MENU_KIND_34:", mn)   # light hue: no undefined return

    def test_labels_words_and_session_are_wired(self):
        mn = patch_section("mn/mnmain.c")
        self.assertEqual(mn.count("+    mu_mex_games_label("), 2)        # hover and unhover
        self.assertEqual(mn.count("+            mu_mex_games_label(jobj.value, menu_kind, i);"), 1)
        self.assertIn("+        mu_mex_games_describe(text, menu_kind, selection);", mn)
        self.assertIn("+        mu_mex_menu_load();", mn)
        self.assertIn("+    mu_mex_volleyball_end();", mn)
        think = mn[mn.index("+static void mu_mn_games_think(HSD_GObj* gp)\n+{"):]
        think = think[:think.index("+#endif")]
        self.assertIn("if (mu_mex_games_start(mn_804A04F0.hovered_selection))", think)
        self.assertIn("lbAudioAx_80024030(3);", think)   # a locked row: error sound, no scene change
        self.assertIn("mn_804A04F0.hovered_selection = SEL_VS_TOURNAMENT;", think)
        gm = patch_section("gm/gmvsmode.c")
        self.assertIn("+            gm_SetNextGameModeStateId(gmVsMode_State_Vs);", gm)
        self.assertIn("+            mu_mex_volleyball_end();", gm)
        self.assertIn("+        mu_mex_volleyball_rules(gm_GetGameModeStateEnterData(state));", gm)

    def test_only_volleyball_runs_natively(self):
        shim = source("sourceport/game/shim/mu_mex_menu.c")
        native = re.findall(r'strcmp\(file, "([^"]+)"\) == 0', shim)
        self.assertEqual(native, ["/GrMVb.dat", "scVb.dat"])
        self.assertIn('ROW_LOCKED', shim)
        self.assertIn('mu_mex_stage_usable(internal) ? ext : -1', shim)
        rules = shim[shim.index("void mu_mex_volleyball_rules("):]
        for line in ("start->rules.timer_enabled = 0;", "start->rules.timer_counts_up = 1;",
                     "start->rules.x5_2 = 1;", "start->rules.stkind = (u16) volleyball_ext;"):
            self.assertIn(line, rules)


if __name__ == "__main__":
    unittest.main()
