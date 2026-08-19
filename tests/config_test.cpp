#include "config.hpp"

#include <gtest/gtest.h>

#include <stdexcept>

#include "test_util.hpp"

using namespace std::chrono_literals;

namespace {

mas::Config load(const std::string& content) {
  TempDir dir;
  const auto path = dir.path() / "config.yaml";
  write_file(path, content);
  return mas::load_config(path);
}

}  // namespace

TEST(Config, ParsesMonitorInputsAndActions) {
  const auto config = load(R"(version: 1
monitors:
  asus-vg248qe:
    inputs:
      mustang: "0x0f"
      cachyos-nitro: "0x11"
      macbook-m5: "0x0f"
actions:
  ddcutil:
    command: "ddcutil setvcp 60 {{input}}"
)");

  EXPECT_EQ(config.version, 1);
  EXPECT_EQ(config.default_monitor, "asus-vg248qe");
  EXPECT_EQ(config.switch_delay, 1000ms);
  EXPECT_EQ(config.resolve_input("asus-vg248qe", "mustang"), 0x0f);
  EXPECT_EQ(config.resolve_input("", "cachyos-nitro"), 0x11);
  ASSERT_EQ(config.actions.size(), 1u);
  EXPECT_EQ(config.actions[0].second.command, "ddcutil setvcp 60 {{input}}");

  const auto* monitor = config.find_monitor("");
  ASSERT_NE(monitor, nullptr);
  EXPECT_EQ(monitor->display, 1);
}

TEST(Config, ParsesDelayDisplayAndDefaultMonitor) {
  const auto config = load(R"(switch_delay_ms: 250
default_monitor: second
monitors:
  first:
    inputs:
      a: 15
  second:
    display: 2
    inputs:
      a: "0x11"
)");

  EXPECT_EQ(config.switch_delay, 250ms);
  EXPECT_EQ(config.default_monitor, "second");
  EXPECT_EQ(config.resolve_input("", "a"), 0x11);
  EXPECT_EQ(config.resolve_input("first", "a"), 15);
  EXPECT_EQ(config.find_monitor("")->display, 2);
}

TEST(Config, ResolveInputFailsForUnknownDeviceOrMonitor) {
  const auto config = load("monitors:\n  m:\n    inputs:\n      mustang: \"0x0f\"\n");
  EXPECT_FALSE(config.resolve_input("", "unknown-device"));
  EXPECT_FALSE(config.resolve_input("other", "mustang"));
}

TEST(Config, RejectsInvalidInputValue) {
  EXPECT_THROW(load("monitors:\n  m:\n    inputs:\n      a: \"0x1ff\"\n"), std::runtime_error);
  EXPECT_THROW(load("monitors:\n  m:\n    inputs:\n      a: hdmi\n"), std::runtime_error);
}

TEST(Config, RejectsUnknownFieldsAndNegativeDelay) {
  EXPECT_THROW(load("bogus: 1\n"), std::runtime_error);
  EXPECT_THROW(load("switch_delay_ms: -5\n"), std::runtime_error);
}

TEST(Config, MissingFileThrows) {
  EXPECT_THROW(mas::load_config("/nonexistent/config.yaml"), std::runtime_error);
}

TEST(Config, ParseInputValue) {
  EXPECT_EQ(mas::parse_input_value("0x0f"), 0x0f);
  EXPECT_EQ(mas::parse_input_value("0X11"), 0x11);
  EXPECT_EQ(mas::parse_input_value("17"), 17);
  EXPECT_FALSE(mas::parse_input_value("0x"));
  EXPECT_FALSE(mas::parse_input_value("256"));
  EXPECT_FALSE(mas::parse_input_value("0x0g"));
  EXPECT_FALSE(mas::parse_input_value(""));
}
