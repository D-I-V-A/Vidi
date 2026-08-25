cmake_minimum_required(VERSION 3.15)
project(Vidi VERSION 0.1)

set(PROJECT_VERSION_SUFFIX "alpha")
set(PROJECT_VERSION_FULL "${PROJECT_VERSION}-${PROJECT_VERSION_SUFFIX}")
message(STATUS "Building ${PROJECT_NAME} v${PROJECT_VERSION_FULL}")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_compile_definitions(UNICODE _UNICODE)

set(SOURCES
    src/main.cc
    src/gui/gui.cc
    src/kernels/directShowPlayer.cc
    src/app.rc
)

add_executable(${PROJECT_NAME} WIN32 ${SOURCES})

target_include_directories(${PROJECT_NAME} PRIVATE
    include
    src
    src/gui
    src/kernels
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
)

# Manifest di-embed manual lewat src/app.rc (RT_MANIFEST) -> matikan
# /MANIFEST bawaan linker agar tidak terjadi duplikat resource (CVT1100).
if(MSVC)
    target_link_options(${PROJECT_NAME} PRIVATE "/MANIFEST:NO")
endif()

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/assets
        $<TARGET_FILE_DIR:${PROJECT_NAME}>/assets
)

# Salin folder filters (LAV portable) ke samping exe saat build
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/filters
        $<TARGET_FILE_DIR:${PROJECT_NAME}>/filters
)

install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION .)
install(DIRECTORY ${CMAKE_SOURCE_DIR}/assets DESTINATION .)
install(DIRECTORY ${CMAKE_SOURCE_DIR}/filters DESTINATION .)


set(CPACK_PACKAGE_NAME "${PROJECT_NAME}")
set(CPACK_PACKAGE_VENDOR "Vidi Developer")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION_FULL}")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Vidi Video & Audio Player")

set(CPACK_GENERATOR "NSIS")

set(CPACK_PACKAGE_INSTALL_DIRECTORY "${PROJECT_NAME}")
set(CPACK_NSIS_CREATE_ICONS_EXTRA "CreateShortCut '\\$SMPROGRAMS\\\\$STARTMENU_FOLDER\\\\${PROJECT_NAME}.lnk' '\\$INSTDIR\\\\${PROJECT_NAME}.exe'")

include(CPack)