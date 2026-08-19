cmake_minimum_required(VERSION 3.24)
project(monitor-auto-switch LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()

option(MAS_BUILD_TESTS "Build unit tests" ON)

include(FetchContent)
find_package(PkgConfig REQUIRED)
pkg_check_modules(DDCUTIL REQUIRED IMPORTED_TARGET ddcutil>=2.2.6)
find_package(yaml-cpp REQUIRED)

find_package(nlohmann_json 3.11 QUIET)
if(NOT nlohmann_json_FOUND)
  message(STATUS "nlohmann_json not found, fetching it")
  FetchContent_Declare(json
    URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
    DOWNLOAD_EXTRACT_TIMESTAMP ON)
  FetchContent_MakeAvailable(json)
endif()

add_library(mas_core STATIC
  src/config.cpp
  src/display.cpp
  src/event_loop.cpp
  src/log_parser.cpp
  src/log_tailer.cpp
  src/switch_controller.cpp
)
target_include_directories(mas_core PUBLIC src)
target_compile_options(mas_core PUBLIC -Wall -Wextra -Wpedantic)
target_link_libraries(mas_core PUBLIC PkgConfig::DDCUTIL yaml-cpp::yaml-cpp nlohmann_json::nlohmann_json)

add_executable(mas src/main.cpp)
target_link_libraries(mas PRIVATE mas_core)
install(TARGETS mas)

if(MAS_BUILD_TESTS)
  enable_testing()
  find_package(GTest REQUIRED)
  add_executable(mas_tests
    tests/config_test.cpp
    tests/log_parser_test.cpp
    tests/log_tailer_test.cpp
    tests/switch_controller_test.cpp
  )
  target_link_libraries(mas_tests PRIVATE mas_core GTest::gtest_main)
  include(GoogleTest)
  gtest_discover_tests(mas_tests)
endif()
