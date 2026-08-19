#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace mas {

struct SwitchEvent {
  std::string from;
  std::string to;

  bool operator==(const SwitchEvent&) const = default;
};

// Parses a single Synergy log line (JSONL or legacy plain text) and returns
// the screen switch it describes, if any.
std::optional<SwitchEvent> parse_switch_event(std::string_view line);

}  // namespace mas
