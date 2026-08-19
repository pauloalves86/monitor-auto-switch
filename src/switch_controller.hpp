#pragma once

#include <chrono>
#include <cstdint>
#include <optional>

#include "display.hpp"

namespace mas {

// Debounces input switch requests and only touches the monitor when the
// final target differs from what it is showing.
//
// The controller has no clock: on_target() returns how long to wait, the
// caller arms a timer, and calls on_timer() when it expires.
class SwitchController {
 public:
  SwitchController(Display& display, std::chrono::milliseconds delay) : display_(display), delay_(delay) {}

  // Records the latest wanted input. Returns the delay after which on_timer()
  // must be called; any earlier pending timer is superseded. With a zero
  // delay the switch is reconciled immediately and zero is returned.
  std::chrono::milliseconds on_target(std::uint8_t input);

  // Applies the pending target unless the monitor is already on it.
  void on_timer();

  bool pending() const { return desired_.has_value(); }
  std::optional<std::uint8_t> applied() const { return applied_; }

 private:
  Display& display_;
  std::chrono::milliseconds delay_;
  std::optional<std::uint8_t> desired_;
  std::optional<std::uint8_t> applied_;  // last value set successfully
};

}  // namespace mas
