#include "log_tailer.hpp"

#include <gtest/gtest.h>

#include <cstdlib>

#include "test_util.hpp"

using mas::LogTailer;
using mas::SwitchEvent;

namespace {

const std::string kSwitchToMustang =
    "core [2025-10-13T15:03:04] - INFO - switch from \"desktop\" to \"mustang-2487237f\" at 10,20\n";

}  // namespace

TEST(LogTailer, StartsAtEndOfExistingFile) {
  TempDir dir;
  const auto path = dir.path() / "synergy.log";
  write_file(path, kSwitchToMustang);

  LogTailer tailer(path);
  EXPECT_EQ(tailer.position(), kSwitchToMustang.size());
  EXPECT_TRUE(tailer.read_new().empty());
}

TEST(LogTailer, ReadsOnlyAppendedLines) {
  TempDir dir;
  const auto path = dir.path() / "synergy.log";
  write_file(path, kSwitchToMustang);

  LogTailer tailer(path);
  append_file(path, "core [2025-10-13T15:03:05] - INFO - switch from \"mustang-2487237f\" to \"desktop\" at 1,2\n");

  const auto events = tailer.read_new();
  ASSERT_EQ(events.size(), 1u);
  EXPECT_EQ(events[0], (SwitchEvent{"mustang-2487237f", "desktop"}));
  EXPECT_TRUE(tailer.read_new().empty());
}

TEST(LogTailer, SkipsNonSwitchLines) {
  TempDir dir;
  const auto path = dir.path() / "synergy.log";
  write_file(path, "");

  LogTailer tailer(path);
  append_file(path, "core [2025-10-13T15:03:04] - INFO - mouse moved at 10,20\n" + kSwitchToMustang);

  const auto events = tailer.read_new();
  ASSERT_EQ(events.size(), 1u);
  EXPECT_EQ(events[0].to, "mustang-2487237f");
}

TEST(LogTailer, ResetsPositionWhenTruncated) {
  TempDir dir;
  const auto path = dir.path() / "synergy.log";
  write_file(path, kSwitchToMustang + kSwitchToMustang);

  LogTailer tailer(path);
  write_file(path, "core [2025-10-13T15:03:04] - INFO - switch from \"desktop\" to \"alienware-c3bbfb01\" at 11,21\n");

  const auto events = tailer.read_new();
  ASSERT_EQ(events.size(), 1u);
  EXPECT_EQ(events[0].to, "alienware-c3bbfb01");
}

TEST(LogTailer, BuffersPartialLines) {
  TempDir dir;
  const auto path = dir.path() / "synergy.log";
  write_file(path, "");

  LogTailer tailer(path);
  append_file(path, kSwitchToMustang.substr(0, 40));
  EXPECT_TRUE(tailer.read_new().empty());

  append_file(path, kSwitchToMustang.substr(40));
  const auto events = tailer.read_new();
  ASSERT_EQ(events.size(), 1u);
  EXPECT_EQ(events[0].to, "mustang-2487237f");
}

TEST(LogTailer, ResetReadsRecreatedFileFromStart) {
  TempDir dir;
  const auto path = dir.path() / "synergy.log";
  write_file(path, kSwitchToMustang);

  LogTailer tailer(path);
  std::filesystem::remove(path);
  write_file(path, kSwitchToMustang + kSwitchToMustang);  // larger than before: no truncation detected
  tailer.reset();

  EXPECT_EQ(tailer.read_new().size(), 2u);
}

TEST(LogTailer, MissingFileYieldsNothing) {
  TempDir dir;
  LogTailer tailer(dir.path() / "missing.jsonl");
  EXPECT_EQ(tailer.position(), 0u);
  EXPECT_TRUE(tailer.read_new().empty());
}

TEST(LogTailer, ReadsJsonlMixedLines) {
  TempDir dir;
  const auto path = dir.path() / "synergy.jsonl";
  write_file(path, "");

  LogTailer tailer(path);
  const std::string content =
      R"({"ts":"2026-10-05T11:22:40.400Z","source":"core","level":"INFO","msg":"[2026-10-05T08:22:40.400] INFO: starting core"})"
      "\n"
      R"({"ts":"2026-10-05T11:22:40.408Z","source":"core","level":"INFO","msg":"[2026-10-05T08:22:40.408] INFO: switch from \"desktop\" to \"mustang-2487237f\" at 10,20"})"
      "\n"
      R"({"ts":"2026-10-05T11:22:40.500Z","source":"service","level":"INFO","msg":"heartbeat"})"
      "\n";
  append_file(path, content);

  const auto events = tailer.read_new();
  ASSERT_EQ(events.size(), 1u);
  EXPECT_EQ(events[0], (SwitchEvent{"desktop", "mustang-2487237f"}));
  EXPECT_EQ(tailer.position(), content.size());
}

class ResolveLogPath : public ::testing::Test {
 protected:
  void SetUp() override {
    if (const char* home = std::getenv("HOME")) old_home_ = home;
    setenv("HOME", home_.path().c_str(), 1);
    synergy_ = home_.path() / ".local/state/Synergy";
  }
  void TearDown() override { setenv("HOME", old_home_.c_str(), 1); }

  TempDir home_;
  std::filesystem::path synergy_;
  std::string old_home_;
};

TEST_F(ResolveLogPath, PrefersJsonlWhenBothExist) {
  std::filesystem::create_directories(synergy_);
  write_file(synergy_ / "synergy.jsonl", "");
  write_file(synergy_ / "synergy.log", "");
  EXPECT_EQ(mas::resolve_log_path(), synergy_ / "synergy.jsonl");
}

TEST_F(ResolveLogPath, FallsBackToLegacyLog) {
  std::filesystem::create_directories(synergy_);
  write_file(synergy_ / "synergy.log", "");
  EXPECT_EQ(mas::resolve_log_path(), synergy_ / "synergy.log");
}

TEST_F(ResolveLogPath, DefaultsToJsonlWhenNeitherExists) {
  EXPECT_EQ(mas::resolve_log_path(), synergy_ / "synergy.jsonl");
}
