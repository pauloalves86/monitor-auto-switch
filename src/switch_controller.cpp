#include "switch_controller.hpp"

#include <iostream>

namespace mas {

std::chrono::milliseconds SwitchController::on_target(std::uint8_t input) {
  desired_ = input;
  if (delay_.count() == 0) on_timer();
  return delay_;
}

void SwitchController::on_timer() {
  if (!desired_) return;
  const std::uint8_t target = *desired_;
  desired_.reset();

  std::optional<std::uint8_t> current = display_.get_input();
  const char* source = "monitor";
  if (!current) {
    current = applied_;
    source = "cache";
  }

  if (current == target) {
    std::cout << "Input source already " << format_input(target) << " (" << source << "), skipping" << std::endl;
    return;
  }

  std::cout << "Setting input source to " << format_input(target) << std::endl;
  if (display_.set_input(target)) applied_ = target;
}

}  // namespace mas
