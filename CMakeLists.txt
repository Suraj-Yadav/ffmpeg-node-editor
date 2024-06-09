cmake_minimum_required(VERSION 3.8 FATAL_ERROR)
project(ffmpeg_node_editor LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_EXPORT_COMPILE_COMMANDS
    ON
    CACHE INTERNAL "")

find_package(Backward CONFIG REQUIRED)
find_package(reproc CONFIG REQUIRED)
find_package(reproc++ CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)

# Import non vcpkg stuff
add_subdirectory(./third_party)

add_library(
  core STATIC
  src/ffmpeg/filter_graph.cpp
  src/ffmpeg/profile.cpp
  src/ffmpeg/runner.cpp
  src/file_utils.cpp
  src/imgui_extras.cpp
  src/node_editor.cpp
  src/pref.cpp
  src/string_utils.cpp)

target_include_directories(core PUBLIC "${CMAKE_SOURCE_DIR}/include")
target_link_libraries(
  core
  PUBLIC imgui
         spdlog::spdlog
         tinyfiledialogs
         nlohmann_json::nlohmann_json
         IconFontCppHeaders
         reproc
         reproc++)

# Main Executable
add_executable(ffmpeg_node_editor src/backend.cpp src/backend_glfw_opengl.cpp
                                  src/backend_win32_d3d12.cpp src/main.cpp)
target_link_libraries(ffmpeg_node_editor PRIVATE core Backward::Interface)

# Setup Test, coverage and benchmarks
find_package(GTest CONFIG REQUIRED)

add_executable(tests src/ffmpeg/runner_test.cpp src/imgui_extras_test.cpp
                     src/util_test.cpp)

target_link_libraries(tests PRIVATE GTest::gtest GTest::gtest_main core)
