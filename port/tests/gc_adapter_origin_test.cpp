// A port the adapter drops for one report and brings straight back, with the stick held, must keep
// the neutral point taken at rest: the released stick reads centre, not a push the other way.
#define GC_ADAPTER_TEST_HOOK
#include "../runtime/host/gc_adapter.cpp"
#include <cstdio>

namespace yougame {
bool stdio_bridge() { return false; }
uintptr_t pad_mirror_handle() { return 0; }
}

namespace host {
Options options;
TickTiming& tick_timing() { static TickTiming timing; return timing; }
void log(const char*, ...) {}
int lagtest_flash_mode() { return 0; }
void lagtest_flash_report(int) {}
}

using namespace host;
static int failures = 0;
static void check(bool ok, const char* what) {
  std::printf("%s %s\n", ok ? "PASS" : "FAIL", what);
  if (!ok) ++failures;
}

// One report with socket 1 at these raw bytes (status 0x10 = plugged in, 0 = dropped).
static PadState poll(uint8_t status, uint8_t sx, uint8_t sy, uint8_t tl = 0) {
  {
    std::lock_guard<std::mutex> lk(g_mutex);
    std::memset(g_report, 0, sizeof g_report);
    g_report[0] = 0x21;
    uint8_t* c = g_report + 1;
    c[0] = status; c[3] = sx; c[4] = sy; c[5] = 128; c[6] = 128; c[7] = tl; c[8] = 0;
    g_have_report = true;
  }
  PadState out[4]{};
  gcadapter_poll(out);
  return out[0];
}

int main() {
  options.no_gc_adapter = false;
  ensure_scanner_started();
  g_running = true;

  // A controller whose resting stick is a little off centre, as real ones are.
  PadState p = poll(0x10, 124, 151);
  check(p.stick_x == 0 && p.stick_y == 0, "first neutral is taken at rest");
  p = poll(0x10, 229, 117);
  check(p.stick_x == 105, "a held stick reads its push");

  // The adapter drops the port for one report and brings it back with the stick still held.
  poll(0x00, 229, 117);
  p = poll(0x10, 229, 117);
  check(p.stick_x == 105 && p.stick_y == -34, "after the drop the held stick still reads its push");
  p = poll(0x10, 124, 151);
  check(p.stick_x == 0 && p.stick_y == 0, "the released stick reads centre (no drift)");

  // The same with the stick held the other way and a trigger held.
  poll(0x00, 27, 127, 200);
  p = poll(0x10, 27, 127, 200);
  p = poll(0x10, 124, 151, 0);
  check(p.stick_x == 0 && p.stick_y == 0 && p.trig_l == 0, "a drop with the stick left and L held: released reads centre");

  // A reconnect at rest still takes the new neutral (a different controller in the socket).
  poll(0x00, 128, 128);
  p = poll(0x10, 130, 126);
  p = poll(0x10, 130, 126);
  check(p.stick_x == 0 && p.stick_y == 0, "a reconnect at rest takes its own neutral");

  // A controller first plugged in with the stick held, no neutral yet: the centre.
  for (auto& o : g_rest_origin) o = Origin{};
  poll(0x00, 128, 128);
  poll(0x10, 229, 117);
  p = poll(0x10, 128, 128);
  check(p.stick_x == 0 && p.stick_y == 0, "first plugged in with the stick held: neutral is the centre");

  // PADRecalibrate at rest still re-reads the neutral.
  gcadapter_recalibrate(0);
  p = poll(0x10, 120, 140);
  p = poll(0x10, 120, 140);
  check(p.stick_x == 0 && p.stick_y == 0, "recalibrate at rest takes the new neutral");

  g_running = false;
  g_scanner_run = false;
  std::printf("%s\n", failures ? "FAILED" : "all passed");
  return failures ? 1 : 0;
}
