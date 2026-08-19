#include "display.hpp"

#include <ddcutil_c_api.h>
#include <ddcutil_status_codes.h>

#include <format>
#include <iostream>
#include <mutex>

namespace mas {
namespace {

constexpr DDCA_Vcp_Feature_Code kInputSource = 0x60;

std::string status_name(DDCA_Status status) {
  const char* name = ddca_rc_name(status);
  return name ? name : std::to_string(status);
}

void init_library() {
  static std::once_flag once;
  std::call_once(once, [] {
    const DDCA_Status rc = ddca_init2(nullptr, DDCA_SYSLOG_NOT_SET, DDCA_INIT_OPTIONS_NONE, nullptr);
    if (rc != DDCRC_OK) std::cerr << "libddcutil init failed: " << status_name(rc) << '\n';
  });
}

}  // namespace

std::string format_input(std::uint8_t input) { return std::format("0x{:02x}", input); }

DdcDisplay::DdcDisplay(int display_number) : display_number_(display_number) { init_library(); }

DdcDisplay::~DdcDisplay() { close(); }

bool DdcDisplay::open() {
  if (handle_) return true;

  DDCA_Display_Identifier did = nullptr;
  DDCA_Status rc = ddca_create_dispno_display_identifier(display_number_, &did);
  if (rc != DDCRC_OK) {
    std::cerr << "Invalid display number " << display_number_ << ": " << status_name(rc) << '\n';
    return false;
  }

  DDCA_Display_Ref ref = nullptr;
  rc = ddca_get_display_ref(did, &ref);
  ddca_free_display_identifier(did);
  if (rc != DDCRC_OK) {
    std::cerr << "Display " << display_number_ << " not found: " << status_name(rc) << '\n';
    return false;
  }

  DDCA_Display_Handle handle = nullptr;
  rc = ddca_open_display2(ref, /*wait=*/true, &handle);
  if (rc != DDCRC_OK) {
    std::cerr << "Cannot open display " << display_number_ << ": " << status_name(rc) << '\n';
    return false;
  }
  handle_ = handle;
  return true;
}

void DdcDisplay::close() {
  if (!handle_) return;
  ddca_close_display(static_cast<DDCA_Display_Handle>(handle_));
  handle_ = nullptr;
}

std::optional<std::uint8_t> DdcDisplay::get_input() {
  if (!open()) return std::nullopt;

  DDCA_Non_Table_Vcp_Value value{};
  const DDCA_Status rc = ddca_get_non_table_vcp_value(static_cast<DDCA_Display_Handle>(handle_), kInputSource, &value);
  if (rc != DDCRC_OK) {
    std::cerr << "Cannot read input source: " << status_name(rc) << '\n';
    close();
    return std::nullopt;
  }
  return value.sl;
}

bool DdcDisplay::set_input(std::uint8_t input) {
  if (!open()) return false;

  // The *2 variant never reads the value back: many monitors stop answering
  // DDC right after they switch to another input.
  const DDCA_Status rc =
      ddca_set_non_table_vcp_value2(static_cast<DDCA_Display_Handle>(handle_), kInputSource, 0, input);
  if (rc != DDCRC_OK) {
    std::cerr << "Cannot set input source to " << format_input(input) << ": " << status_name(rc) << '\n';
    close();
    return false;
  }
  return true;
}

std::optional<std::uint8_t> DryRunDisplay::get_input() { return current_; }

bool DryRunDisplay::set_input(std::uint8_t input) {
  std::cout << "[dry-run] would set input source to " << format_input(input) << std::endl;
  current_ = input;
  return true;
}

}  // namespace mas
