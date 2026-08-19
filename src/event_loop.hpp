#pragma once

#include <string>
#include <vector>

#include "config.hpp"
#include "log_parser.hpp"
#include "log_tailer.hpp"
#include "switch_controller.hpp"

namespace mas {

// Watches the Synergy log with inotify and drives the SwitchController with
// a timerfd, all from one poll() loop. Returns on SIGINT/SIGTERM.
class EventLoop {
 public:
  EventLoop(LogTailer& tailer, const Config* config, std::string monitor_name, SwitchController& controller);

  // Throws std::system_error when the watch cannot be set up.
  void run();

  // Maps switch events to inputs and feeds the controller. Returns the delay
  // to arm the timer with, or nullopt when nothing is pending.
  std::optional<std::chrono::milliseconds> handle(const std::vector<SwitchEvent>& events);

 private:
  LogTailer& tailer_;
  const Config* config_;
  std::string monitor_name_;
  SwitchController& controller_;
};

}  // namespace mas
