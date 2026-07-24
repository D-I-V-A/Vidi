cmake_minimum_required(VERSION 3.15)
project(Vidi VERSION 0.1)


set(PROJECT_VERSION_SUFFIX "-alpha")
set(PROJECT_VERSION_FULL "${PROJECT_VERSION}...${PROJECT_VERSION_SUFFIX}")
message(STATUS "Building ${PROJECT_NAME} v${PROJECT_VERSION_FULL}")


# adding complie definition
add_compile_definitions(UNICODE _UNICODE)

# setup c++ dengan versi 20
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# sekarang executable
add_executable(${PROJECT_NAME} WIN32 src/main.cc)
target_compile_definitions(${PROJECT_NAME} PRIVATE UNICODE _UNICODE)

target_link_libraries(${PROJECT_NAME} user32 gdi32 comctl32)

