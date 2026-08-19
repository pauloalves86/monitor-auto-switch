#pragma once

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace mas {

struct MonitorConfig {
  int display = 1;  // ddcutil display number
  // Synergy screen name -> VCP 0x60 input source value, in file order.
  std::vector<std::pair<std::string, std::uint8_t>> inputs;
};

struct ActionConfig {
  std::string command;
};

struct Config {
  int version = 1;
  std::string default_monitor;
  std::chrono::milliseconds switch_delay{1000};
  std::vector<std::pair<std::string, MonitorConfig>> monitors;
  std::vector<std::pair<std::string, ActionConfig>> actions;

  const MonitorConfig* find_monitor(std::string_view name) const;

  // Looks up the input for a Synergy screen. An empty monitor name means the
  // default monitor.
  std::optional<std::uint8_t> resolve_input(std::string_view monitor, std::string_view device) const;
};

// Throws std::runtime_error with a readable message on any problem.
Config load_config(const std::filesystem::path& path);

// Parses "0x0f", "0X11" or "17" into a VCP byte.
std::optional<std::uint8_t> parse_input_value(std::string_view text);

// $XDG_CONFIG_HOME/monitor-auto-switch/config.yaml, or ~/.config/... .
std::filesystem::path default_config_path();

}  // namespace mas
