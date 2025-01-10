include(FetchContent)

# imgui
FetchContent_Populate(
  imgui_stack
  GIT_REPOSITORY https://github.com/thedmd/imgui.git
  GIT_TAG feature/layout)

FetchContent_Populate(
  imnodes
  GIT_REPOSITORY https://github.com/Nelarius/imnodes.git
  GIT_TAG master
  GIT_SUBMODULES img)

add_library(imgui STATIC)
target_sources(
  imgui
  PRIVATE ${imgui_stack_SOURCE_DIR}/imgui.cpp
          ${imgui_stack_SOURCE_DIR}/imgui_demo.cpp
          ${imgui_stack_SOURCE_DIR}/imgui_draw.cpp
          ${imgui_stack_SOURCE_DIR}/imgui_tables.cpp
          ${imgui_stack_SOURCE_DIR}/imgui_widgets.cpp
          ${imgui_stack_SOURCE_DIR}/misc/cpp/imgui_stdlib.cpp
          ${imnodes_SOURCE_DIR}/imnodes.cpp)

target_include_directories(
  imgui SYSTEM
  PUBLIC ${imgui_stack_SOURCE_DIR} ${imgui_stack_SOURCE_DIR}/backends
         ${imgui_stack_SOURCE_DIR}/misc/cpp ${imnodes_SOURCE_DIR})

if(MSVC AND FALSE)
  target_link_libraries(imgui PRIVATE d3d12.lib d3dcompiler.lib dxgi.lib)
  target_compile_definitions(imgui PUBLIC IMGUI_USE_WIN32_D3D12)
  target_sources(
    imgui PRIVATE ${imgui_stack_SOURCE_DIR}/backends/imgui_impl_dx12.cpp
                  ${imgui_stack_SOURCE_DIR}/backends/imgui_impl_win32.cpp)
else()
  set(OpenGL_GL_PREFERENCE
      GLVND
      CACHE INTERNAL "")
  find_package(OpenGL REQUIRED)
  find_package(glfw3 REQUIRED)
  target_link_libraries(imgui PRIVATE glfw OpenGL::GL)
  target_compile_definitions(imgui PUBLIC IMGUI_USE_GLFW_OPENGL)
  target_sources(
    imgui PRIVATE ${imgui_stack_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
                  ${imgui_stack_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp)
endif()

# Icons
FetchContent_Populate(
  icon_fonts
  GIT_REPOSITORY https://github.com/juliettef/IconFontCppHeaders.git
  GIT_TAG main
  PATCH_COMMAND git apply ${CMAKE_CURRENT_SOURCE_DIR}/IconFontCppHeaders.patch
  UPDATE_DISCONNECTED 1)

execute_process(
  COMMAND python "${icon_fonts_SOURCE_DIR}/GenerateIconFontCppHeaders.py"
  WORKING_DIRECTORY "${icon_fonts_SOURCE_DIR}")

add_library(IconFontCppHeaders INTERFACE)
target_include_directories(IconFontCppHeaders SYSTEM
                           INTERFACE "${icon_fonts_SOURCE_DIR}")

# tinyfiledialogs
FetchContent_Populate(
  tfd
  GIT_REPOSITORY https://git.code.sf.net/p/tinyfiledialogs/code
  GIT_TAG master)

add_library(tinyfiledialogs STATIC)
target_sources(tinyfiledialogs PRIVATE ${tfd_SOURCE_DIR}/tinyfiledialogs.c)
target_include_directories(tinyfiledialogs SYSTEM PUBLIC ${tfd_SOURCE_DIR})

# subprocess
FetchContent_Populate(
  subproc
  GIT_REPOSITORY https://github.com/sheredom/subprocess.h
  GIT_TAG master)

add_library(subprocess INTERFACE)
target_include_directories(subprocess SYSTEM INTERFACE "${subproc_SOURCE_DIR}")
