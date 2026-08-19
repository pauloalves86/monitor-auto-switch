#include "event_loop.hpp"

#include <poll.h>
#include <signal.h>
#include <sys/inotify.h>
#include <sys/signalfd.h>
#include <sys/timerfd.h>
#include <unistd.h>

#include <array>
#include <cerrno>
#include <iostream>
#include <system_error>

namespace mas {
namespace {

class Fd {
 public:
  explicit Fd(int fd, const char* what) : fd_(fd) {
    if (fd_ < 0) throw std::system_error(errno, std::generic_category(), what);
  }
  ~Fd() { ::close(fd_); }
  Fd(const Fd&) = delete;
  Fd& operator=(const Fd&) = delete;
  int get() const { return fd_; }

 private:
  int fd_;
};

void arm(const Fd& timer, std::chrono::milliseconds delay) {
  itimerspec spec{};
  const auto secs = std::chrono::duration_cast<std::chrono::seconds>(delay);
  spec.it_value.tv_sec = secs.count();
  spec.it_value.tv_nsec = std::chrono::duration_cast<std::chrono::nanoseconds>(delay - secs).count();
  if (spec.it_value.tv_sec == 0 && spec.it_value.tv_nsec == 0) spec.it_value.tv_nsec = 1;  // 0 would disarm
  if (::timerfd_settime(timer.get(), 0, &spec, nullptr) < 0) {
    throw std::system_error(errno, std::generic_category(), "timerfd_settime");
  }
}

}  // namespace

EventLoop::EventLoop(LogTailer& tailer, const Config* config, std::string monitor_name, SwitchController& controller)
    : tailer_(tailer), config_(config), monitor_name_(std::move(monitor_name)), controller_(controller) {}

std::optional<std::chrono::milliseconds> EventLoop::handle(const std::vector<SwitchEvent>& events) {
  std::optional<std::chrono::milliseconds> delay;
  for (const auto& event : events) {
    const auto input = config_ ? config_->resolve_input(monitor_name_, event.to) : std::nullopt;
    if (!input) {
      std::cerr << "Ignoring unmapped device \"" << event.to << "\" for monitor \"" << monitor_name_ << "\""
                << (config_ ? "" : ": no config loaded") << '\n';
      continue;
    }
    std::cout << "Switch detected: " << event.from << " -> " << event.to << " (input " << format_input(*input) << ")"
              << std::endl;
    delay = controller_.on_target(*input);
  }
  if (!controller_.pending()) return std::nullopt;
  return delay;
}

void EventLoop::run() {
  const auto dir = tailer_.path().parent_path();
  const auto name = tailer_.path().filename().string();

  Fd inotify(::inotify_init1(IN_NONBLOCK | IN_CLOEXEC), "inotify_init1");
  if (::inotify_add_watch(inotify.get(), dir.c_str(), IN_MODIFY | IN_CREATE | IN_MOVED_TO) < 0) {
    throw std::system_error(errno, std::generic_category(), "watching " + dir.string());
  }

  Fd timer(::timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC), "timerfd_create");

  sigset_t signals;
  sigemptyset(&signals);
  sigaddset(&signals, SIGINT);
  sigaddset(&signals, SIGTERM);
  if (::sigprocmask(SIG_BLOCK, &signals, nullptr) < 0) {
    throw std::system_error(errno, std::generic_category(), "sigprocmask");
  }
  Fd signal(::signalfd(-1, &signals, SFD_NONBLOCK | SFD_CLOEXEC), "signalfd");

  auto process = [&](std::vector<SwitchEvent> events) {
    if (const auto delay = handle(events)) arm(timer, *delay);
  };

  process(tailer_.read_new());

  std::array<pollfd, 3> fds{{
      {inotify.get(), POLLIN, 0},
      {timer.get(), POLLIN, 0},
      {signal.get(), POLLIN, 0},
  }};
  alignas(inotify_event) char buffer[4096];

  for (;;) {
    if (::poll(fds.data(), fds.size(), -1) < 0) {
      if (errno == EINTR) continue;
      throw std::system_error(errno, std::generic_category(), "poll");
    }

    if (fds[2].revents & POLLIN) {
      std::cout << "Shutting down" << std::endl;
      return;
    }

    if (fds[0].revents & POLLIN) {
      bool modified = false;
      ssize_t len;
      while ((len = ::read(inotify.get(), buffer, sizeof buffer)) > 0) {
        for (char* p = buffer; p < buffer + len;) {
          const auto* event = reinterpret_cast<const inotify_event*>(p);
          if (event->len > 0 && name == event->name) {
            if (event->mask & (IN_CREATE | IN_MOVED_TO)) {
              std::cout << "Log file created, starting to monitor..." << std::endl;
              tailer_.reset();
            }
            modified = true;
          }
          p += sizeof(inotify_event) + event->len;
        }
      }
      if (modified) process(tailer_.read_new());
    }

    if (fds[1].revents & POLLIN) {
      std::uint64_t expirations;
      if (::read(timer.get(), &expirations, sizeof expirations) == sizeof expirations) controller_.on_timer();
    }
  }
}

}  // namespace mas
