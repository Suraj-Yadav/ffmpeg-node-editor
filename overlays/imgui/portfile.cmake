vcpkg_check_linkage(ONLY_STATIC_LIBRARY)

vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO thedmd/imgui
    REF 4981fef6649c1c9204f39641fff7d06cc2b1acfe
    SHA512 c445bd6f0d1fcc58627aeb7f7e785292cb89d0a319be4f2295c83453c60a5f5427cb13c9143beb8a90d4f24cf60999ba8fe84529981b0dbbf4172a35db2faa8d
    HEAD_REF layouts
)

file(COPY "${CMAKE_CURRENT_LIST_DIR}/imgui-config.cmake.in" DESTINATION "${SOURCE_PATH}")
file(COPY "${CMAKE_CURRENT_LIST_DIR}/CMakeLists.txt" DESTINATION "${SOURCE_PATH}")

vcpkg_check_features(OUT_FEATURE_OPTIONS FEATURE_OPTIONS
    FEATURES 
    dx12-binding                IMGUI_BUILD_DX12_BINDING
    glfw-binding                IMGUI_BUILD_GLFW_BINDING
    metal-binding               IMGUI_BUILD_METAL_BINDING
    opengl3-binding             IMGUI_BUILD_OPENGL3_BINDING
    osx-binding                 IMGUI_BUILD_OSX_BINDING
    win32-binding               IMGUI_BUILD_WIN32_BINDING
)

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS
        ${FEATURE_OPTIONS}
    OPTIONS_DEBUG
        -DIMGUI_SKIP_HEADERS=ON
)

vcpkg_cmake_install()

vcpkg_copy_pdbs()
vcpkg_cmake_config_fixup()

vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE.txt")
