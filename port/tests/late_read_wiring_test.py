"""Online late controller read: the game is given one frame less than the chosen delay only when the
connection has room, each frame then waits between sending its pad and reading the other player's
inputs, and the wait counts as waiting. 1v1 on the Source Port only, never with a Friends lobby's
shared delay, and MELEE_ONLINE_LATE_READ=0 turns it off."""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[2]


def source(name):
    return (ROOT / name).read_text(encoding="utf-8-sig").replace("\r\n", "\n")


class LateReadWiringTest(unittest.TestCase):
    def setUp(self):
        self.online = source("port/runtime/hle/slippi_online.cpp")
        self.block = self.online[self.online.index("namespace late_read {"):self.online.index("}  // namespace late_read")]
        self.control = source("port/runtime/host/late_read.h")

    def test_can_be_turned_off(self):
        # Off unless the player turns on "Variable online delay" (owner 11-24); the env variable overrides it.
        self.assertIn("static bool enabled() { return host::late_read::enabled(); }", self.block)
        self.assertIn("inline std::atomic<bool> g_setting{false};", self.control)
        self.assertIn("return env >= 0 ? env == 1 : g_setting.load(std::memory_order_relaxed);", self.control)
        self.assertIn("const bool late_on = host::late_read::enabled();", source("port/app/source_p2p.cpp"))
        settings = source("port/runtime/gx/pc_settings.cpp")
        self.assertIn('settings_toggle("Variable online delay", &options.online_late_read)', settings)
        self.assertIn("bool online_late_read = false;", source("port/runtime/gx/render_options.h"))
        self.assertIn("if (!enabled() || !host::game_image || chosen < 2 || !g_netplay) return chosen;", self.block)

    def test_only_one_remote_player_and_no_shared_delay(self):
        self.assertIn("if (const auto lobby = yougame::friends_lobby(); lobby.open && lobby.plan && lobby.plan->ok) return chosen;", self.block)
        self.assertIn("g_netplay->RemoteMachineCount() != 1 || g_netplay->RemotePortCount() != 1 || g_netplay->SecondLocalPort() >= 0 || opponent_runahead()", self.block)

    def test_needs_a_ping_with_room(self):
        self.assertIn("if (ping <= 0) return chosen;", self.block)
        self.assertIn("on = has_room(chosen, ping, work) && host::recent_sim_frames_steady();", self.block)
        self.assertIn("const double work = host::recent_sim_frame_work_ms();", self.block)
        p2p = source("port/app/source_p2p.cpp")
        self.assertIn("host::late_read::has_room(delay, (double)ping_us / 1000.0, host::recent_sim_frame_work_ms()) &&", p2p)
        self.assertIn("host::recent_sim_frames_steady());", p2p)
        host_cpp = source("port/runtime/host/host.cpp")
        self.assertIn("note_recent_late(g_last_sim_late);", host_cpp)
        self.assertIn("return late * 20 <= have;", host_cpp)
        self.assertIn("if (!on && !measured) return chosen;", self.block)
        self.assertIn("const double needed = kGuardMs - ((chosen_delay - 1) * kFrameMs - ping_ms / 2.0);", self.control)
        self.assertIn("return chosen_delay >= 2 && needed <= longest - kRoomReserveMs;", self.control)

    def test_engaged_only_when_the_game_took_the_lower_delay(self):
        self.assertIn("engaged = enabled() && host::game_image && g_netplay && chosen >= 2 && delay == chosen - 1;", self.block)
        self.assertIn("if (g_in_online_match && late_read::engaged) return late_read::base_delay - 1;", self.online)
        start = self.online[self.online.index("const int chosen_for_game = chosen_delay();"):]
        self.assertLess(start.index("g_latched_delay = (int)delay;"), start.index("late_read::begin((int)delay, chosen_for_game);"))
        self.assertIn("return late_read::offer(chosen_delay());", self.online)

    def test_waits_after_the_send_and_before_the_remote_inputs(self):
        handler = self.online[self.online.index("void handle_online_inputs("):]
        send = handler.index("g_netplay->SendSlippiPad(std::make_unique<Pad>(frame + delay, finalized, finalized_checksum, inputs)")
        hold = handler.index("late_read::hold(frame, g_currently_advancing || g_frames_to_advance > 0);")
        read = handler.index("prepare_opponent_inputs(frame, skip, q);")
        self.assertLess(send, hold)
        self.assertLess(hold, read)

    def test_wait_keeps_remote_pads_early_and_leaves_room_for_the_frame(self):
        self.assertIn("target = std::clamp(kGuardMs - least_early_, 0.0, longest);", self.control)
        self.assertIn("const double longest = std::clamp(kFrameMs - kWorkSpareMs - work_now_ - late_cut_, 0.0, kMaxHoldMs);", self.control)
        self.assertIn("const bool wait_made_it_late = late && last_waited_ + work_ms > kFrameMs - 0.5;", self.control)
        self.assertIn("control.note_waited(waited);", self.block)
        self.assertIn("hold_ = target > hold_ ? std::min(target, hold_ + 1.0) : std::max(target, hold_ - 0.05);", self.control)
        self.assertIn("if (have_early && frame >= kFirstSampleFrame)", self.control)

    def test_wait_is_counted_as_waiting(self):
        host = source("port/runtime/host/host.cpp")
        body = host[host.index("double tick_hold(double seconds) {"):host.index("// How deep the game thread's stack has been")]
        self.assertIn("perf::Scope wait_cost(perf::Stage::Wait);", body)
        self.assertIn("sim_cost_add(SIM_WAIT, waited);", body)
        self.assertIn("const auto deadline = g_next_frame + std::chrono::microseconds((long long)(seconds * 1e6));", body)

    def test_a_game_that_went_badly_is_not_repeated_on_that_connection(self):
        self.assertIn("if (soured == g_netplay.get()) return chosen;", self.block)
        self.assertIn("if (engaged && g_netplay && control.went_badly()) {", self.block)
        self.assertIn("return frames_ >= 600 && (late_arrivals_ * 600 > 6 * frames_ || short_frames_ * 10 > frames_);", self.control)
        p2p = source("port/app/source_p2p.cpp")
        self.assertIn("if (late_soured) return false;", p2p)

    def test_remote_pad_arrival_is_kept(self):
        net = source("port/runtime/hle/slippi_net.cpp")
        self.assertIn("if (frame > remote_arrival_[pidx].frame) remote_arrival_[pidx] = {frame, cur_time};", net)
        self.assertIn("for (auto& a : remote_arrival_) a = {};", net)


if __name__ == "__main__":
    unittest.main()
