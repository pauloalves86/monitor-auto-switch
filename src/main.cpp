#include <filesystem>
#include <iostream>
#include <memory>
#include <optional>
#include <string_view>
#include <system_error>

#include "config.hpp"
#include "display.hpp"
#include "event_loop.hpp"
#include "log_tailer.hpp"
#include "switch_controller.hpp"

namespace fs = std::filesystem;

namespace {

void usage(std::ostream& out) {
  out << "Usage: mas [--config PATH] [--log PATH] [--dry-run]\n"
         "\n"
         "Switches the monitor input when Synergy moves to another screen.\n"
         "\n"
         "  --config PATH  configuration file (default: " << mas::default_config_path().string() << ")\n"
         "  --log PATH     Synergy log to follow (default: " << mas::resolve_log_path().string() << ")\n"
         "  --dry-run      log the switches instead of talking to the monitor\n";
}

}  // namespace

int main(int argc, char** argv) {
  fs::path config_path = mas::default_config_path();
  std::optional<fs::path> log_path;
  bool dry_run = false;

  for (int i = 1; i < argc; ++i) {
    const std::string_view arg = argv[i];
    if (arg == "--config" && i + 1 < argc) {
      config_path = argv[++i];
    } else if (arg == "--log" && i + 1 < argc) {
      log_path = argv[++i];
    } else if (arg == "--dry-run") {
      dry_run = true;
    } else if (arg == "-h" || arg == "--help") {
      usage(std::cout);
      return 0;
    } else {
      usage(std::cerr);
      return 2;
    }
  }
  if (!log_path) log_path = mas::resolve_log_path();

  std::optional<mas::Config> config;
  try {
    config = mas::load_config(config_path);
  } catch (const std::exception& e) {
    std::cerr << "Config not loaded, every device will be ignored: " << e.what() << '\n';
  }
  if (config && !config->actions.empty()) {
    std::cerr << "Note: the actions section is ignored (libddcutil is used)\n";
  }

  const std::string monitor_name = config ? config->default_monitor : "";
  const mas::MonitorConfig* monitor = config ? config->find_monitor(monitor_name) : nullptr;
  const auto delay = config ? config->switch_delay : mas::Config{}.switch_delay;

  std::unique_ptr<mas::Display> display;
  if (dry_run) {
    display = std::make_unique<mas::DryRunDisplay>();
  } else {
    display = std::make_unique<mas::DdcDisplay>(monitor ? monitor->display : 1);
  }

  std::error_code ec;
  if (!fs::exists(*log_path, ec)) {
    std::cerr << "Log file does not exist yet: " << log_path->string() << '\n'
              << "Watching anyway in case it gets created later...\n";
  }

  mas::LogTailer tailer(*log_path);
  mas::SwitchController controller(*display, delay);
  mas::EventLoop loop(tailer, config ? &*config : nullptr, monitor_name, controller);

  std::cout << "Monitoring Synergy log file: " << log_path->string() << '\n'
            << "Configuration file: " << config_path.string() << '\n'
            << "Switch delay: " << delay.count() << " ms" << (dry_run ? " (dry run)" : "") << '\n'
            << "Press Ctrl+C to exit" << std::endl;

  try {
    loop.run();
  } catch (const std::exception& e) {
    std::cerr << "Error: " << e.what() << '\n';
    return 1;
  }
  return 0;
}
