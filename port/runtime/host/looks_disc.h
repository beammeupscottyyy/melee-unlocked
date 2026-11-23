#pragma once
// The game disc Settings' looks installs patch (Diet Melee, Animelee, the mod artwork).
//
// The launcher opens Settings on its own with --settings-window and no --iso, so in that window the
// game was given no disc and Install said "this game was started without one" (11-23 report). The
// launcher keeps the disc it plays with in launcher.ini: beside the game, or two folders up when the
// game runs from Versions\<version>, and a copy in %LOCALAPPDATA%\MeleeUnlocked. The first of those
// that names a file that is there is the disc the next game starts with, so it is the one to use.
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace host::looks_disc {

inline std::string iso_line(const std::filesystem::path& ini) {
  std::ifstream f(ini);
  std::string line;
  while (std::getline(f, line)) {
    if (!line.empty() && line.back() == '\r') line.pop_back();
    if (line.rfind("iso=", 0) == 0) return line.substr(4);
  }
  return "";
}

// mod_base_iso and iso: the game's own disc options. exe_dir: the folder of the running game.
// local_app_data: %LOCALAPPDATA% (empty when unknown).
inline std::string pick(const std::string& mod_base_iso, const std::string& iso, const std::filesystem::path& exe_dir,
                        const std::filesystem::path& local_app_data) {
  if (!mod_base_iso.empty()) return mod_base_iso;
  if (!iso.empty()) return iso;
  std::vector<std::filesystem::path> inis;
  if (!exe_dir.empty()) {
    inis.push_back(exe_dir / "launcher.ini");
    if (exe_dir.has_parent_path() && exe_dir.parent_path().filename() == "Versions")
      inis.push_back(exe_dir.parent_path().parent_path() / "launcher.ini");
  }
  if (!local_app_data.empty()) inis.push_back(local_app_data / "MeleeUnlocked" / "launcher.ini");
  for (const auto& ini : inis) {
    const std::string disc = iso_line(ini);
    std::error_code ec;
    if (!disc.empty() && std::filesystem::is_regular_file(std::filesystem::u8path(disc), ec)) return disc;
  }
  return "";
}

}  // namespace host::looks_disc
