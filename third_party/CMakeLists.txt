include(FetchContent)

FetchContent_Declare(
  imgui_stack
  GIT_REPOSITORY https://github.com/thedmd/imgui.git
  GIT_TAG feature/layout)
FetchContent_Populate(imgui_stack)

FetchContent_Declare(
  imnodes
  GIT_REPOSITORY https://github.com/Nelarius/imnodes.git
  GIT_TAG master
  GIT_SUBMODULES img)
FetchContent_Populate(imnodes)

FetchContent_Declare(
  icon_fonts
  GIT_REPOSITORY https://github.com/juliettef/IconFontCppHeaders.git
  GIT_TAG main
  PATCH_COMMAND git apply ${CMAKE_CURRENT_SOURCE_DIR}/IconFontCppHeaders.patch
  UPDATE_DISCONNECTED 1)
FetchContent_Populate(icon_fonts)

add_library(imgui STATIC)
target_sources(
  imgui
  PRIVATE ${imgui_stack_SOURCE_DIR}/imgui.cpp
          ${imgui_stack_SOURCE_DIR}/imgui_demo.cpp
          ${imgui_stack_SOURCE_DIR}/imgui_draw.cpp
          ${imgui_stack_SOURCE_DIR}/imgui_tables.cpp
          ${imgui_stack_SOURCE_DIR}/imgui_widgets.cpp
          ${imgui_stack_SOURCE_DIR}/backends/imgui_impl_dx12.cpp
          ${imgui_stack_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
          ${imgui_stack_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
          ${imgui_stack_SOURCE_DIR}/backends/imgui_impl_win32.cpp
          ${imgui_stack_SOURCE_DIR}/misc/cpp/imgui_stdlib.cpp
          ${imnodes_SOURCE_DIR}/imnodes.cpp)

target_include_directories(
  imgui SYSTEM
  PUBLIC ${imgui_stack_SOURCE_DIR} ${imgui_stack_SOURCE_DIR}/backends
         ${imgui_stack_SOURCE_DIR}/misc/cpp ${imnodes_SOURCE_DIR})

find_package(glfw3 REQUIRED)
target_link_libraries(imgui PRIVATE glfw)

execute_process(
  COMMAND python3 "${icon_fonts_SOURCE_DIR}/GenerateIconFontCppHeaders.py"
  WORKING_DIRECTORY "${icon_fonts_SOURCE_DIR}")

add_library(IconFontCppHeaders INTERFACE)
target_include_directories(IconFontCppHeaders SYSTEM
                           INTERFACE "${icon_fonts_SOURCE_DIR}")
