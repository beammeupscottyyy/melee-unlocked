"""The game window stays in front through startup, and the YouGame app's button over a game started on
its own gets a visible pointer and every click (the click that reaches the game asks for the app's menu)."""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[2]


def source(name):
    return (ROOT / name).read_text(encoding="utf-8-sig")


def body(text, marker):
    start = text.index("{", text.index(marker))
    depth, end = 1, start + 1
    while depth:
        depth += (text[end] == "{") - (text[end] == "}")
        end += 1
    return text[start + 1:end - 1]


class AppButtonWiring(unittest.TestCase):
    def setUp(self):
        self.window = source("port/runtime/host/window.cpp")
        self.link = source("port/runtime/hle/yougame_link.cpp")

    def test_no_ghost_window_during_startup(self):
        create = body(self.window, "void* window_create(")
        ghost = create.index("DisableProcessWindowsGhosting()")
        # Before the window exists, so the long DLSS and device steps never swap it for a ghost.
        self.assertLess(ghost, create.index("CreateWindowExW("))

    def test_renderer_starts_while_the_window_answers(self):
        resp = body(self.window, "void window_responsive(const std::function<void()>& work) {\n  if (!g_hwnd")
        # Gated to app-backed launches; every normal launch keeps the inline startup.
        gate = resp.index("const bool app_backed = yougame::embedded() || yougame::stdio_bridge() || (extension && extension[0] == '1');")
        self.assertLess(gate, resp.index("std::thread worker("))
        self.assertIn("if (!app_backed) { work(); return; }", resp)
        self.assertIn('std::getenv("MELEE_YOUGAME_EXTENSION")', resp)
        self.assertIn("std::thread worker(", resp)
        self.assertIn("MsgWaitForMultipleObjects(1, &finished, FALSE, 25, QS_ALLINPUT)", resp)
        self.assertIn("held.push_back(msg);", resp)
        self.assertIn("std::rethrow_exception(error)", resp)
        # The worker's panel steps are queued to the window's thread; its callbacks wait for the end.
        self.assertIn("g_loading_queue.push_back", body(self.window, "void loading_show(const wchar_t* what, size_t done, size_t total) {\n  if (!g_hwnd"))
        self.assertIn("if (!g_startup_busy.load())\n    if (g_on_message && g_on_message(h, m, w, l)) return 1;", self.window)
        self.assertIn("if (off_window_thread()) return;", body(self.window, "void window_pump()"))
        threaded = source("port/runtime/gx/threaded_backend.cpp")
        self.assertIn("host::window_responsive([&] {", threaded)
        self.assertLess(threaded.index("host::window_responsive([&] {"), threaded.index("host::window_set_resize_callback([&renderer]"))
        self.assertIn("host::window_responsive([&] { backend.reset(gx::create_render_backend(", source("port/app/main.cpp"))

    def test_button_shown_follows_channel_and_game_state(self):
        shown = body(self.link, "bool app_button_shown()")
        for part in ("!g_stdio", "g_channel_allowed", "g_channel_up.load()", "game_state::kPlaying"):
            self.assertIn(part, shown)
        self.assertIn("g_reported_game_state.store(state", body(self.link, "void report_game_state(int state)"))

    def test_corner_is_embed_free_and_dpi_scaled(self):
        corner = body(self.window, "bool in_app_button_area(HWND h, int x, int y)")
        self.assertIn("yougame::embedded()", corner)
        self.assertIn("GetDpiForWindow(h)", corner)
        self.assertIn("constexpr int kAppButtonArea = 72;", self.window)

    def test_cursor_and_click_before_hiding_rules(self):
        proc = body(self.window, "LRESULT CALLBACK wnd_proc(")
        arrow = proc.index("SetCursor(LoadCursor(nullptr, IDC_ARROW)); return TRUE;")
        click = proc.index("m == WM_LBUTTONDOWN && !g_ui_capture.load() && !g_pip && in_app_button_area(")
        rested = proc.index("if (m == WM_SETCURSOR && LOWORD(l) == HTCLIENT && g_pointer_rested) { SetCursor(nullptr); return TRUE; }")
        focused = proc.index("!g_ui_capture.load() && window_focused(h)) { SetCursor(nullptr); return TRUE; }")
        self.assertLess(arrow, rested)
        self.assertLess(arrow, focused)
        self.assertLess(click, proc.index("g_on_message && g_on_message(h, m, w, l)"))
        self.assertIn('yougame::channel_menu_key("keys")', proc[click:click + 400])
        # The rest rule never hides the pointer over the button's corner.
        pump = body(self.window, "void window_pump()")
        self.assertIn("!in_app_button_area(g_hwnd, at.x, at.y)", pump)


if __name__ == "__main__":
    unittest.main()
