cmake_minimum_required(VERSION 3.15)
project(Vidi VERSION 0.1)

set(PROJECT_VERSION_SUFFIX "alpha")
set(PROJECT_VERSION_FULL "${PROJECT_VERSION}-${PROJECT_VERSION_SUFFIX}") # FIX: satu titik/dash, bukan "..."
message(STATUS "Building ${PROJECT_NAME} v${PROJECT_VERSION_FULL}")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_compile_definitions(UNICODE _UNICODE)

set(SOURCES
    src/main.cc
    src/gui/gui.cc
)

add_executable(${PROJECT_NAME} WIN32 ${SOURCES})

target_include_directories(${PROJECT_NAME} PRIVATE include)

target_link_libraries(${PROJECT_NAME} PRIVATE
    user32
    gdi32
    comctl32
)