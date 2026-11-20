"""Play while searching (a kept search): opt-in only, the main menu's cleanup keeps only a chosen search,
the game sees no session while away, and the way back goes through the select's own flow."""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[2]


def source(path):
    return (ROOT / path).read_text(encoding="utf-8-sig")


def body(text, start, end):
    a = text.index(start)
    return text[a:text.index(end, a)]


class SearchKeepWiringTest(unittest.TestCase):
    def test_default_is_ask_and_nothing_is_kept_without_a_choice(self):
        opts = source("port/runtime/gx/render_options.h")
        self.assertIn("int keep_search = 0;", opts)
        self.assertIn("static int& live_keep_search() { static int value = 0; return value; }", opts)
        self.assertIn("int search_banner = 0;", opts)   # the slim pill
        native = source("port/runtime/hle/native_practice.cpp")
        menu = body(native, "bool keep_search_at_menu() {", "void test_request_training(")
        # Kept only when the player said yes on the select, or chose "keep searching" in the settings.
        self.assertIn("const bool chose = g_keep_pending && steady_ms() - g_keep_pending_ms < 15000;", menu)
        self.assertIn("gx::RenderOptions::live_keep_search() == 1 && from_select", menu)
        self.assertIn("if (!(chose || always) || !kept_searching(s)", menu)

    def test_menu_cleanup_reason_and_static_fallback(self):
        shim = source("sourceport/game/shim/mu_slippi_menu.c")
        loaded = body(shim, "void mu_slippi_main_menu_loaded", "/* OnlineModeOptionSelected")
        self.assertIn("u8 menu = 3;", loaded)
        bridge = source("port/runtime/hle/native_slippi_bridge.h")
        self.assertIn("payload[0] >= 1 && payload[0] <= 3", bridge)
        online = source("port/runtime/hle/slippi_online.cpp")
        cleanup = body(online, "case CMD_CLEANUP_CONNECTION:", "case CMD_SEND_CHAT_MESSAGE:")
        self.assertIn("payload[0] == 3 && slippi::native_practice::keep_search_at_menu()", cleanup)
        self.assertIn("payload_len == 0 && g_search_away", cleanup)
        # Every cleanup ends a kept search; the game sees no session while away.
        self.assertIn("g_search_away = false;", body(online, "void cleanup_connection(", "// ---------------------------------------------------------------- per-frame online flow"))
        self.assertIn("if (g_search_away) return -1;", body(online, "int session_mode() {", "int session_mode_or_parked()"))

    def test_question_swallows_its_buttons_and_leaves_through_the_menu(self):
        native = source("port/runtime/hle/native_practice.cpp")
        pads = body(native, "void kept_select_pads(", "\n}\n")
        self.assertIn("pads[i].button &= (uint16_t)~(kA | kB | kX | kStart | kZ);", pads)
        # Z still stops the search while asked: the question goes and Z reaches the game.
        z = pads.index("if (fresh & kZ) {")
        self.assertLess(z, pads.index("pads[i].button &= (uint16_t)~(kA | kB | kX | kStart | kZ);"))
        self.assertIn("pads[i].button &= (uint16_t)~(kA | kB | kX | kStart);", pads[z:])
        self.assertIn('leave(true, "the player keeps searching and leaves the select")', pads)
        self.assertIn('leave(false, "the player stops the search and leaves the select")', pads)
        self.assertIn("request_major(kMainMenuMajor);", pads)
        self.assertIn("kept_select_pads(pads, new_frame);", native)

    def test_found_counts_down_then_goes_back_at_a_safe_moment(self):
        native = source("port/runtime/hle/native_practice.cpp")
        tick = body(native, "void kept_tick() {", "// The question's answers")
        self.assertIn("constexpr uint64_t kKeepCountdownMs = 3000;", native)
        self.assertIn("now - g_kept.found_ms >= kKeepCountdownMs", tick)
        self.assertIn("if (kept_safe_now(&why))", tick)
        back = body(native, "void kept_go_back(", "// Can the player be taken back right now?")
        self.assertIn("slippi::online::search_away(false);", back)
        self.assertIn("set_online_mode(g_kept.mode);", back)
        self.assertIn("is_match_mode_major(current_major())", back)   # the Source Port's KO star trap: menu first

    def test_banner_has_no_em_dash_and_the_disconnect_plate(self):
        for path in ("port/runtime/gx/practice_menu.cpp", "port/runtime/gx/pc_settings.cpp", "port/runtime/hle/native_practice.cpp"):
            self.assertNotIn("—", source(path), path)
            self.assertNotIn("\\xE2\\x80\\x94", source(path), path)
        ui = source("port/runtime/gx/pc_settings.cpp")
        self.assertIn('n.title = "DISCONNECTED";', ui)
        self.assertNotIn("##native_practice_failure", ui)


if __name__ == "__main__":
    unittest.main()
