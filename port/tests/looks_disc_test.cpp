// Settings opened alone from the launcher has no disc of its own; its looks installs use the
// launcher's (looks_disc.h).
#include "looks_disc.h"

#include <cstdio>
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;
static int failures = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL line %d: %s\n", __LINE__, #c); ++failures; } } while (0)

static void write(const fs::path& p, const std::string& text) {
  fs::create_directories(p.parent_path());
  std::ofstream(p, std::ios::binary) << text;
}

int main() {
  const fs::path root = fs::temp_directory_path() / "mu_looks_disc_test";
  std::error_code ec;
  fs::remove_all(root, ec);
  const fs::path disc = root / "discs" / "Melee 1.02.iso";
  write(disc, "GALE01");
  const std::string d = disc.u8string();
  const fs::path game = root / "game", local = root / "local";
  using host::looks_disc::pick;

  // The game's own disc options win, the vanilla base of a mod disc first.
  CHECK(pick("base.iso", "mod.iso", game, local) == "base.iso");
  CHECK(pick("", "mine.iso", game, local) == "mine.iso");
  // Settings alone, nothing saved anywhere: still empty (the install then says so).
  CHECK(pick("", "", game, local).empty());
  // The launcher's ini beside the game (Windows line ends).
  write(game / "launcher.ini", "iso=" + d + "\r\nengine=1\r\n");
  CHECK(pick("", "", game, local) == d);
  // A version folder: the launcher's ini is two folders up.
  const fs::path version = game / "Versions" / "1.0.15";
  fs::create_directories(version);
  CHECK(pick("", "", version, local) == d);
  // A saved disc that is gone is skipped for the copy in LOCALAPPDATA.
  write(game / "launcher.ini", "iso=" + (root / "gone.iso").u8string() + "\n");
  CHECK(pick("", "", game, local).empty());
  write(local / "MeleeUnlocked" / "launcher.ini", "iso=" + d + "\n");
  CHECK(pick("", "", game, local) == d);
  CHECK(pick("", "", game, "").empty());

  fs::remove_all(root, ec);
  std::printf(failures ? "looks_disc: %d failed\n" : "looks_disc: ok\n", failures);
  return failures ? 1 : 0;
}
