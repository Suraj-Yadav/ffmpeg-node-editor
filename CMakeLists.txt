cmake_minimum_required(VERSION 3.8 FATAL_ERROR)
set(VCPKG_OVERLAY_PORTS "./overlays" CACHE INTERNAL "")
project(ffmpeg_node_editor LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON CACHE INTERNAL "")

find_package(absl CONFIG REQUIRED)
find_package(glfw3 REQUIRED)
find_package(imgui CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(OpenGL REQUIRED)
find_package(re2 CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(tiny-process-library CONFIG REQUIRED)
find_package(unofficial-imgui-node-editor CONFIG REQUIRED)
find_package(unofficial-nativefiledialog CONFIG REQUIRED)

add_executable(ffmpeg_node_editor 
    src/backend.cpp
    src/ffmpeg/filter_graph.cpp
    src/ffmpeg/profile.cpp
    src/ffmpeg/runner.cpp
    src/imgui_extras.cpp
    src/main.cpp
    src/node_editor.cpp
)

target_include_directories(ffmpeg_node_editor PUBLIC "${CMAKE_SOURCE_DIR}/include")

target_link_libraries(ffmpeg_node_editor PRIVATE 
    ${OPENGL_gl_LIBRARY}
    absl::strings 
    glfw 
    imgui::imgui 
    nlohmann_json::nlohmann_json
    re2::re2
    spdlog::spdlog
    tiny-process-library::tiny-process-library 
    unofficial::imgui-node-editor::imgui-node-editor 
    unofficial::nativefiledialog::nfd
)
