#pragma once

#include <filesystem>
#include <fstream>
#include <random>
#include <string>

// A directory under the system temp dir, removed at the end of the test.
class TempDir {
 public:
  TempDir() {
    std::random_device rd;
    path_ = std::filesystem::temp_directory_path() / ("mas-test-" + std::to_string(rd()));
    std::filesystem::create_directories(path_);
  }
  ~TempDir() { std::filesystem::remove_all(path_); }
  const std::filesystem::path& path() const { return path_; }

 private:
  std::filesystem::path path_;
};

inline void write_file(const std::filesystem::path& path, const std::string& content) {
  std::ofstream(path, std::ios::binary | std::ios::trunc) << content;
}

inline void append_file(const std::filesystem::path& path, const std::string& content) {
  std::ofstream(path, std::ios::binary | std::ios::app) << content;
}
