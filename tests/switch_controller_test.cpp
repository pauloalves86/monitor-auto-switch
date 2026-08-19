#include "switch_controller.hpp"

#include <gtest/gtest.h>

#include <vector>

using namespace std::chrono_literals;

namespace {

class FakeDisplay : public mas::Display {
 public:
  std::optional<std::uint8_t> get_input() override {
    ++reads;
    return readable ? current : std::nullopt;
  }
  bool set_input(std::uint8_t input) override {
    sets.push_back(input);
    if (!set_succeeds) return false;
    current = input;
    return true;
  }

  std::optional<std::uint8_t> current;
  bool readable = true;
  bool set_succeeds = true;
  int reads = 0;
  std::vector<std::uint8_t> sets;
};

constexpr std::uint8_t kA = 0x0f;
constexpr std::uint8_t kB = 0x11;

}  // namespace

TEST(SwitchController, SwitchesOnlyAfterTimer) {
  FakeDisplay display;
  display.current = kB;
  mas::SwitchController controller(display, 1000ms);

  EXPECT_EQ(controller.on_target(kA), 1000ms);
  EXPECT_TRUE(display.sets.empty());
  EXPECT_TRUE(controller.pending());

  controller.on_timer();
  EXPECT_EQ(display.sets, std::vector<std::uint8_t>{kA});
  EXPECT_EQ(controller.applied(), kA);
  EXPECT_FALSE(controller.pending());
}

TEST(SwitchController, QuickBounceBackDoesNothing) {
  FakeDisplay display;
  display.current = kB;
  mas::SwitchController controller(display, 1000ms);

  controller.on_target(kA);
  controller.on_target(kB);
  controller.on_timer();

  EXPECT_TRUE(display.sets.empty());
}

TEST(SwitchController, OnlyFinalTargetIsApplied) {
  FakeDisplay display;
  display.current = kB;
  mas::SwitchController controller(display, 1000ms);

  controller.on_target(kA);
  controller.on_target(kB);
  controller.on_target(kA);
  controller.on_timer();

  EXPECT_EQ(display.sets, std::vector<std::uint8_t>{kA});
}

TEST(SwitchController, SkipsWhenMonitorAlreadyOnTarget) {
  FakeDisplay display;
  display.current = kA;  // e.g. switched by hand
  mas::SwitchController controller(display, 1000ms);

  controller.on_target(kA);
  controller.on_timer();

  EXPECT_TRUE(display.sets.empty());
}

TEST(SwitchController, FallsBackToCacheWhenReadFails) {
  FakeDisplay display;
  display.current = kB;
  mas::SwitchController controller(display, 1000ms);
  controller.on_target(kA);
  controller.on_timer();
  ASSERT_EQ(controller.applied(), kA);

  display.readable = false;
  controller.on_target(kA);
  controller.on_timer();

  EXPECT_EQ(display.sets, std::vector<std::uint8_t>{kA});
}

TEST(SwitchController, SetsWhenReadFailsAndNothingCached) {
  FakeDisplay display;
  display.readable = false;
  mas::SwitchController controller(display, 1000ms);

  controller.on_target(kA);
  controller.on_timer();

  EXPECT_EQ(display.sets, std::vector<std::uint8_t>{kA});
}

TEST(SwitchController, FailedSetDoesNotUpdateCache) {
  FakeDisplay display;
  display.readable = false;
  display.set_succeeds = false;
  mas::SwitchController controller(display, 1000ms);

  controller.on_target(kA);
  controller.on_timer();
  EXPECT_FALSE(controller.applied());

  controller.on_target(kA);
  controller.on_timer();
  EXPECT_EQ(display.sets.size(), 2u);
}

TEST(SwitchController, ZeroDelaySwitchesImmediately) {
  FakeDisplay display;
  display.current = kB;
  mas::SwitchController controller(display, 0ms);

  EXPECT_EQ(controller.on_target(kA), 0ms);
  EXPECT_EQ(display.sets, std::vector<std::uint8_t>{kA});
  EXPECT_FALSE(controller.pending());
}

TEST(SwitchController, TimerWithoutTargetIsNoop) {
  FakeDisplay display;
  mas::SwitchController controller(display, 1000ms);
  controller.on_timer();
  EXPECT_EQ(display.reads, 0);
  EXPECT_TRUE(display.sets.empty());
}
