#include "config.hpp"

#include <yaml-cpp/yaml.h>

#include <charconv>
#include <cstdlib>
#include <stdexcept>

namespace mas {
namespace {

[[noreturn]] void fail(const std::string& what, const YAML::Node& node) {
  const auto mark = node.Mark();
  if (mark.is_null()) throw std::runtime_error("config: " + what);
  throw std::runtime_error("config: " + what + " at line " + std::to_string(mark.line + 1));
}

template <typename T>
T scalar(const YAML::Node& node, const std::string& field) {
  if (!node.IsScalar()) fail(field + " must be a scalar", node);
  try {
    return node.as<T>();
  } catch (const YAML::Exception&) {
    fail("invalid value for " + field, node);
  }
}

MonitorConfig parse_monitor(const std::string& name, const YAML::Node& node) {
  MonitorConfig monitor;
  if (node.IsNull()) return monitor;
  if (!node.IsMap()) fail("monitor " + name + " must be a mapping", node);

  for (const auto& field : node) {
    const auto key = field.first.as<std::string>();
    if (key == "display") {
      monitor.display = scalar<int>(field.second, "display");
    } else if (key == "inputs") {
      if (field.second.IsNull()) continue;
      if (!field.second.IsMap()) fail("inputs of " + name + " must be a mapping", field.second);
      for (const auto& input : field.second) {
        const auto device = input.first.as<std::string>();
        const auto raw = scalar<std::string>(input.second, "input " + device);
        const auto value = parse_input_value(raw);
        if (!value) fail("invalid input value \"" + raw + "\" for " + device, input.second);
        monitor.inputs.emplace_back(device, *value);
      }
    } else {
      fail("unsupported monitor field \"" + key + "\"", field.first);
    }
  }
  return monitor;
}

}  // namespace

std::optional<std::uint8_t> parse_input_value(std::string_view text) {
  int base = 10;
  if (text.starts_with("0x") || text.starts_with("0X")) {
    text.remove_prefix(2);
    base = 16;
  }
  if (text.empty()) return std::nullopt;

  unsigned value = 0;
  const auto [end, ec] = std::from_chars(text.data(), text.data() + text.size(), value, base);
  if (ec != std::errc{} || end != text.data() + text.size() || value > 0xff) return std::nullopt;
  return static_cast<std::uint8_t>(value);
}

const MonitorConfig* Config::find_monitor(std::string_view name) const {
  if (name.empty()) name = default_monitor;
  for (const auto& [monitor_name, monitor] : monitors) {
    if (monitor_name == name) return &monitor;
  }
  return nullptr;
}

std::optional<std::uint8_t> Config::resolve_input(std::string_view monitor, std::string_view device) const {
  const MonitorConfig* config = find_monitor(monitor);
  if (!config) return std::nullopt;
  for (const auto& [name, value] : config->inputs) {
    if (name == device) return value;
  }
  return std::nullopt;
}

Config load_config(const std::filesystem::path& path) {
  YAML::Node root;
  try {
    root = YAML::LoadFile(path.string());
  } catch (const YAML::BadFile&) {
    throw std::runtime_error("config: cannot read " + path.string());
  } catch (const YAML::ParserException& e) {
    throw std::runtime_error("config: " + std::string(e.what()));
  }

  Config config;
  if (root.IsNull()) return config;
  if (!root.IsMap()) fail("top level must be a mapping", root);

  for (const auto& field : root) {
    const auto key = field.first.as<std::string>();
    const YAML::Node& value = field.second;

    if (key == "version") {
      config.version = scalar<int>(value, "version");
    } else if (key == "default_monitor") {
      config.default_monitor = scalar<std::string>(value, "default_monitor");
    } else if (key == "switch_delay_ms") {
      const auto ms = scalar<long>(value, "switch_delay_ms");
      if (ms < 0) fail("switch_delay_ms must not be negative", value);
      config.switch_delay = std::chrono::milliseconds(ms);
    } else if (key == "monitors") {
      if (value.IsNull()) continue;
      if (!value.IsMap()) fail("monitors must be a mapping", value);
      for (const auto& monitor : value) {
        const auto name = monitor.first.as<std::string>();
        config.monitors.emplace_back(name, parse_monitor(name, monitor.second));
      }
    } else if (key == "actions") {
      if (value.IsNull()) continue;
      if (!value.IsMap()) fail("actions must be a mapping", value);
      for (const auto& action : value) {
        ActionConfig parsed;
        if (action.second.IsMap() && action.second["command"]) {
          parsed.command = scalar<std::string>(action.second["command"], "command");
        }
        config.actions.emplace_back(action.first.as<std::string>(), std::move(parsed));
      }
    } else {
      fail("unsupported config field \"" + key + "\"", field.first);
    }
  }

  if (config.default_monitor.empty() && !config.monitors.empty()) {
    config.default_monitor = config.monitors.front().first;
  }
  return config;
}

std::filesystem::path default_config_path() {
  std::filesystem::path base;
  if (const char* xdg = std::getenv("XDG_CONFIG_HOME"); xdg && *xdg) {
    base = xdg;
  } else if (const char* home = std::getenv("HOME")) {
    base = std::filesystem::path(home) / ".config";
  }
  return base / "monitor-auto-switch" / "config.yaml";
}

}  // namespace mas
