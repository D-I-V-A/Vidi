cmake_minimum_required(VERSION 3.15)
project(Vidi VERSION 0.1)

set(PROJECT_VERSION_SUFFIX "beta")
set(PROJECT_VERSION_FULL "${PROJECT_VERSION}-${PROJECT_VERSION_SUFFIX}")
message(STATUS "Building ${PROJECT_NAME} v${PROJECT_VERSION_FULL}")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_compile_definitions(UNICODE _UNICODE)

set(SOURCES
    src/main.cc
    src/gui/gui.cc
    src/gui/gui_windowproc.cc
    src/gui/gui_layout.cc
    src/gui/gui_progressbar.cc
    src/gui/gui_volumebar.cc
    src/gui/gui_controls.cc
    src/gui/gui_fullscreen.cc
    src/gui/gui_subtitle.cc
    src/kernels/directShowPlayer.cc
    src/kernels/subtitleReader.cc
    src/kernels/assRenderer.cc
    src/resources/vidi.rc
    src/resources/vidi.manifest
)

add_executable(${PROJECT_NAME} WIN32 ${SOURCES})

target_include_directories(${PROJECT_NAME} PRIVATE
    include
    src
    src/gui
    src/kernels
)

find_library(LIBASS_LIBRARY NAMES ass libass
    HINTS "$ENV{VCPKG_ROOT}/installed/x64-windows/lib"
)
find_path(LIBASS_INCLUDE_DIR NAMES ass/ass.h
    HINTS "$ENV{VCPKG_ROOT}/installed/x64-windows/include"
)
find_file(LIBASS_DLL NAMES ass-9.dll
    HINTS "$ENV{VCPKG_ROOT}/installed/x64-windows/bin"
          "${CMAKE_BINARY_DIR}/vcpkg_installed/x64-windows/bin"
)
if(NOT LIBASS_LIBRARY OR NOT LIBASS_INCLUDE_DIR OR NOT LIBASS_DLL)
    message(FATAL_ERROR "libass not found. Set VCPKG_ROOT env var.")
endif()
add_library(libass::libass SHARED IMPORTED)
set_target_properties(libass::libass PROPERTIES
    IMPORTED_IMPLIB "${LIBASS_LIBRARY}"
    IMPORTED_LOCATION "${LIBASS_DLL}"
    INTERFACE_INCLUDE_DIRECTORIES "${LIBASS_INCLUDE_DIR}"
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    user32
    gdi32
    comctl32
    comdlg32
    strmiids
    ole32
    oleaut32
    uxtheme
    wtsapi32
    dwmapi
    libass::libass
)

# Manifest di-embed manual lewat src/app.rc (RT_MANIFEST) -> matikan
# /MANIFEST bawaan linker agar tidak terjadi duplikat resource (CVT1100).
if(MSVC)
    target_link_options(${PROJECT_NAME} PRIVATE "/MANIFEST:NO")
endif()

# ==============================================================================
# PERINTAH SALIN ASSET LOKAL (HANYA BERJALAN SAAT BUILD BIASA, BUKAN SAAT CPACK)
# ==============================================================================
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/assets
        $<TARGET_FILE_DIR:${PROJECT_NAME}>/assets
)

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/filters
        $<TARGET_FILE_DIR:${PROJECT_NAME}>/filters
)

# Salin semua runtime DLL (libass + dependencies: harfbuzz, freetype, fribidi, dll)
get_filename_component(VCPKG_BIN_DIR "${LIBASS_DLL}" DIRECTORY)
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${VCPKG_BIN_DIR}"
        $<TARGET_FILE_DIR:${PROJECT_NAME}>
)

# ==============================================================================
# KONFIGURASI PACKAGING & INSTALLER (WIX / MSI)
# ==============================================================================

install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION bin)
install(DIRECTORY "${VCPKG_BIN_DIR}/" DESTINATION bin FILES_MATCHING PATTERN "*.dll")
install(DIRECTORY ${CMAKE_SOURCE_DIR}/assets/ DESTINATION bin/assets)
install(DIRECTORY ${CMAKE_SOURCE_DIR}/filters/ DESTINATION bin/filters)

set(CPACK_PACKAGE_NAME "${PROJECT_NAME}")
set(CPACK_PACKAGE_VENDOR "Vidi Developer")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}.0")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Vidi Video & Audio Player")

set(CPACK_GENERATOR "WIX")
set(CPACK_WIX_UPGRADE_GUID "A1B2C3D4-E5F6-7890-ABCD-EF1234567890")
set(CPACK_PACKAGE_EXECUTABLES "Vidi" "Vidi Video & Audio Player")
set(CPACK_WIX_PROGRAM_MENU_FOLDER "Vidi")
set(CPACK_CREATE_DESKTOP_LINKS "Vidi")
# ==============================================================================
# UNIT TESTS
# ==============================================================================
enable_testing()
add_subdirectory(tests)

include(CPack)
