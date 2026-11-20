// Fight a pro's roster in 0.9.990 is main's (owner): named players with their habit tables and
// costumes, every slot in its saved place, slot 2 never offered. Guards against a merge that swaps
// the roster for neutral placeholders again.
#include "practice_ghosts.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
static int checks = 0;
#define CHECK(x) do { ++checks; if (!(x)) { std::fprintf(stderr, "line %d: %s\n", __LINE__, #x); std::exit(1); } } while (0)
template<size_t N> static bool any(const uint8_t (&odds)[N]) { for (auto n : odds) if (n) return true; return false; }
int main() {
  using namespace practice_ghosts;
  CHECK(std::size(kGhosts) == 5);
  const int kinds[] = {22, 18, 15, 1, 14};
  const char* characters[] = {"Falco", "Marth", "Jigglypuff", "Fox", "Yoshi"};
  const bool released[] = {true, true, false, true, true};
  const uint8_t costumes[] = {0, 1, 0, 3, 1};
  const bool habits[] = {true, true, false, true, false};   // Yoshi's table is still zero on main
  for (size_t i = 0; i < std::size(kGhosts); ++i) {
    const auto& g = kGhosts[i];
    CHECK(g.name != nullptr && g.name[0] != 0);
    CHECK(std::strncmp(g.name, "Pro ", 4) != 0);
    CHECK(std::string(g.character) == characters[i]);
    CHECK(g.kind == kinds[i]);
    CHECK(kReleased[i] == released[i]);
    CHECK(kPreferredCostume[i] == costumes[i]);
    CHECK((g.games > 0) == habits[i]);
    CHECK(any(g.move) == habits[i] && any(g.attack) == habits[i] && any(g.defend) == habits[i]);
  }
  std::printf("ghost roster: %d/%d checks passed\n", checks, checks);
}
