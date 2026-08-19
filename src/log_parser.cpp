#include "log_parser.hpp"

#include <nlohmann/json.hpp>
#include <regex>

namespace mas {
namespace {

const std::regex& switch_msg_pattern() {
  static const std::regex re(R"re((?:switch|jump)\s+from\s+"([^"]+)"\s+to\s+"([^"]+)"\s+at\s+\d+,\d+)re");
  return re;
}

const std::regex& legacy_switch_pattern() {
  static const std::regex re(
      R"re(core\s+\[[^\]]+\]\s*-\s*INFO\s*-\s*(?:switch|jump)\s+from\s+"([^"]+)"\s+to\s+"([^"]+)"\s+at\s+\d+,\d+)re");
  return re;
}

std::string_view trim(std::string_view s) {
  constexpr std::string_view ws = " \t\r\n";
  const auto first = s.find_first_not_of(ws);
  if (first == std::string_view::npos) return {};
  return s.substr(first, s.find_last_not_of(ws) - first + 1);
}

std::optional<SwitchEvent> match(const std::string& text, const std::regex& re) {
  std::smatch m;
  if (!std::regex_search(text, m, re)) return std::nullopt;
  return SwitchEvent{m[1].str(), m[2].str()};
}

}  // namespace

std::optional<SwitchEvent> parse_switch_event(std::string_view line) {
  const std::string_view trimmed = trim(line);

  if (trimmed.starts_with('{') && trimmed.ends_with('}')) {
    const auto entry = nlohmann::json::parse(trimmed, nullptr, /*allow_exceptions=*/false);
    if (entry.is_object()) {
      const auto source = entry.find("source");
      if (source != entry.end() && source->is_string()) {
        const auto& name = source->get_ref<const std::string&>();
        if (!name.empty() && name != "core") return std::nullopt;
      }
      const auto msg = entry.find("msg");
      if (msg == entry.end() || !msg->is_string()) return std::nullopt;
      return match(msg->get<std::string>(), switch_msg_pattern());
    }
  }

  return match(std::string(trimmed), legacy_switch_pattern());
}

}  // namespace mas
