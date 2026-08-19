#include "log_parser.hpp"

#include <gtest/gtest.h>

using mas::parse_switch_event;
using mas::SwitchEvent;

TEST(LogParser, MatchesLegacySwitchLine) {
  const auto event = parse_switch_event(
      R"(core [2025-10-13T15:03:04] - INFO - switch from "mustang-2487237f" to "alienware-c3bbfb01" at 101,202)");
  ASSERT_TRUE(event);
  EXPECT_EQ(*event, (SwitchEvent{"mustang-2487237f", "alienware-c3bbfb01"}));
}

TEST(LogParser, MatchesLegacyJumpLine) {
  const auto event = parse_switch_event(R"(core [2025-10-13T15:04:00] - INFO - jump from "desktop" to "notebook" at 50,70)");
  ASSERT_TRUE(event);
  EXPECT_EQ(*event, (SwitchEvent{"desktop", "notebook"}));
}

TEST(LogParser, MatchesPaddedLegacyLine) {
  const auto event = parse_switch_event(
      R"(core       [2025-10-13T15:03:04] - INFO    - switch from "a" to "b" at 1,2)" "\r\n");
  ASSERT_TRUE(event);
  EXPECT_EQ(*event, (SwitchEvent{"a", "b"}));
}

TEST(LogParser, RejectsNonSwitchLine) {
  EXPECT_FALSE(parse_switch_event("core [2025-10-13T15:03:04] - INFO - mouse moved at 100,200"));
  EXPECT_FALSE(parse_switch_event(""));
}

TEST(LogParser, MatchesJsonlSwitch) {
  const auto event = parse_switch_event(
      R"({"ts":"2026-10-05T11:22:40.408Z","source":"core","level":"INFO","msg":"[2026-10-05T08:22:40.408] INFO: switch from \"mustang-2487237f\" to \"m5matsect-ef9a27d9\" at 3431,562"})");
  ASSERT_TRUE(event);
  EXPECT_EQ(*event, (SwitchEvent{"mustang-2487237f", "m5matsect-ef9a27d9"}));
}

TEST(LogParser, MatchesJsonlJump) {
  const auto event = parse_switch_event(
      R"({"ts":"2026-10-05T11:22:41.454Z","source":"core","level":"INFO","msg":"[2026-10-05T08:22:41.454] INFO: jump from \"m5matsect-ef9a27d9\" to \"mustang-2487237f\" at 1,574"})");
  ASSERT_TRUE(event);
  EXPECT_EQ(*event, (SwitchEvent{"m5matsect-ef9a27d9", "mustang-2487237f"}));
}

TEST(LogParser, RejectsNonCoreJsonl) {
  EXPECT_FALSE(parse_switch_event(
      R"({"ts":"2026-10-05T11:22:41.454Z","source":"service","level":"INFO","msg":"switch from \"a\" to \"b\" at 1,2"})"));
}

TEST(LogParser, RejectsNonSwitchJsonl) {
  EXPECT_FALSE(parse_switch_event(
      R"({"ts":"2026-10-05T11:22:41.454Z","source":"core","level":"INFO","msg":"[2026-10-05T08:22:41.454] INFO: starting core"})"));
}

TEST(LogParser, RejectsMalformedJsonl) {
  EXPECT_FALSE(parse_switch_event(R"({"source":"core","msg":)"));
  EXPECT_FALSE(parse_switch_event(R"({"source":"core","msg":42})"));
}
