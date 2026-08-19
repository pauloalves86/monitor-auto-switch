#pragma once

#include <cstdint>
#include <optional>
#include <string>

namespace mas {

// The monitor's input source selector (VCP feature 0x60).
class Display {
 public:
  virtual ~Display() = default;
  // Current input source, or nullopt when the monitor doesn't answer.
  virtual std::optional<std::uint8_t> get_input() = 0;
  virtual bool set_input(std::uint8_t input) = 0;
};

std::string format_input(std::uint8_t input);

// Talks to the monitor over DDC/CI through libddcutil. The display is opened
// lazily and reopened after any error.
class DdcDisplay final : public Display {
 public:
  explicit DdcDisplay(int display_number);
  ~DdcDisplay() override;
  DdcDisplay(const DdcDisplay&) = delete;
  DdcDisplay& operator=(const DdcDisplay&) = delete;

  std::optional<std::uint8_t> get_input() override;
  bool set_input(std::uint8_t input) override;

 private:
  bool open();
  void close();

  int display_number_;
  void* handle_ = nullptr;  // DDCA_Display_Handle
};

// Logs what it would do and remembers the last value set.
class DryRunDisplay final : public Display {
 public:
  std::optional<std::uint8_t> get_input() override;
  bool set_input(std::uint8_t input) override;

 private:
  std::optional<std::uint8_t> current_;
};

}  // namespace mas
