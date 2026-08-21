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
)

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