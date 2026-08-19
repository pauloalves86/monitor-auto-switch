#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include "log_parser.hpp"

namespace mas {

// Returns ~/.local/state/Synergy/synergy.jsonl, falling back to the legacy
// synergy.log when only that one exists.
std::filesystem::path resolve_log_path();

// Incrementally reads lines appended to a log file and extracts switch events.
class LogTailer {
 public:
  // Starts at the current end of the file so history is not replayed.
  explicit LogTailer(std::filesystem::path path);

  // Reads everything appended since the last call. An incomplete trailing
  // line is kept until its newline arrives.
  std::vector<SwitchEvent> read_new();

  // Starts over from the beginning, e.g. after the file was recreated.
  void reset();

  const std::filesystem::path& path() const { return path_; }
  std::uint64_t position() const { return position_; }
  void set_position(std::uint64_t position) { position_ = position; }

 private:
  std::filesystem::path path_;
  std::uint64_t position_ = 0;
  std::string partial_;
};

}  // namespace mas
