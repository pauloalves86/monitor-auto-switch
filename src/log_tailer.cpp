#include "log_tailer.hpp"

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string_view>
#include <system_error>

namespace mas {

namespace fs = std::filesystem;

fs::path resolve_log_path() {
  const char* home = std::getenv("HOME");
  const fs::path dir = fs::path(home ? home : "") / ".local/state/Synergy";
  const fs::path jsonl = dir / "synergy.jsonl";
  const fs::path legacy = dir / "synergy.log";

  std::error_code ec;
  if (fs::exists(jsonl, ec)) return jsonl;
  if (fs::exists(legacy, ec)) return legacy;
  return jsonl;
}

LogTailer::LogTailer(fs::path path) : path_(std::move(path)) {
  std::error_code ec;
  const auto size = fs::file_size(path_, ec);
  if (!ec) position_ = size;
}

void LogTailer::reset() {
  position_ = 0;
  partial_.clear();
}

std::vector<SwitchEvent> LogTailer::read_new() {
  std::vector<SwitchEvent> events;

  std::ifstream file(path_, std::ios::binary);
  if (!file) {
    std::cerr << "Error opening log file: " << path_ << '\n';
    return events;
  }

  file.seekg(0, std::ios::end);
  const auto end = file.tellg();
  if (end < 0) {
    std::cerr << "Error getting file size: " << path_ << '\n';
    return events;
  }
  const auto size = static_cast<std::uint64_t>(end);
  if (size < position_) reset();  // truncated
  if (size == position_) return events;

  file.seekg(static_cast<std::streamoff>(position_));
  std::string chunk(size - position_, '\0');
  file.read(chunk.data(), static_cast<std::streamsize>(chunk.size()));
  chunk.resize(static_cast<std::size_t>(file.gcount()));
  position_ += chunk.size();

  std::string buffer = std::move(partial_);
  buffer += chunk;
  partial_.clear();

  std::string_view rest = buffer;
  for (auto nl = rest.find('\n'); nl != std::string_view::npos; nl = rest.find('\n')) {
    if (auto event = parse_switch_event(rest.substr(0, nl))) events.push_back(std::move(*event));
    rest.remove_prefix(nl + 1);
  }
  partial_ = rest;

  return events;
}

}  // namespace mas
