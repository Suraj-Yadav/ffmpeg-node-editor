include(FetchContent)

FetchContent_Declare(imgui_stack
GIT_REPOSITORY https://github.com/thedmd/imgui.git
GIT_TAG feature/layout
)
FetchContent_Populate(imgui_stack)

FetchContent_Declare(imnodes
    GIT_REPOSITORY https://github.com/Nelarius/imnodes.git
    GIT_TAG master
)
FetchContent_Populate(imnodes)


add_library(imgui STATIC)
target_sources(
    imgui PRIVATE
    ${imgui_stack_SOURCE_DIR}/imgui.cpp
    ${imgui_stack_SOURCE_DIR}/imgui_demo.cpp
    ${imgui_stack_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_stack_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_stack_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_stack_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
    ${imgui_stack_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
    ${imgui_stack_SOURCE_DIR}/misc/cpp/imgui_stdlib.cpp
    ${imnodes_SOURCE_DIR}/imnodes.cpp
)
    
target_include_directories(
    imgui PUBLIC 
    ${imgui_stack_SOURCE_DIR}
    ${imgui_stack_SOURCE_DIR}/backends
    ${imgui_stack_SOURCE_DIR}/misc/cpp
    ${imnodes_SOURCE_DIR}
)

find_package(glfw3 REQUIRED)
target_link_libraries(imgui PRIVATE glfw)
