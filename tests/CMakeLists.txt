# tests/CMakeLists.txt
# Unit tests for Vidi

cmake_minimum_required(VERSION 3.15)

# ============================================================
# Test: LAV Module Cache
# ============================================================
add_executable(test_lav_module_cache
    test_lav_module_cache.cc
)

target_include_directories(test_lav_module_cache PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(test_lav_module_cache PRIVATE
    kernel32
)

# Copy filters ke directory test agar test bisa akses
add_custom_command(TARGET test_lav_module_cache POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/filters
        $<TARGET_FILE_DIR:test_lav_module_cache>/filters
)

# Register as CTest target
add_test(
    NAME LAVModuleCacheTest
    COMMAND test_lav_module_cache
    WORKING_DIRECTORY $<TARGET_FILE_DIR:test_lav_module_cache>
)

# ============================================================
# Test: Video Open Integration
# ============================================================
add_executable(test_video_open
    test_video_open.cc
)

target_include_directories(test_video_open PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src/kernels
)

target_link_libraries(test_video_open PRIVATE
    kernel32
    ole32
    oleaut32
    strmiids
)

# Copy filters ke directory test
add_custom_command(TARGET test_video_open POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/filters
        $<TARGET_FILE_DIR:test_video_open>/filters
)

# Copy env.dev.example ke directory test
add_custom_command(TARGET test_video_open POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/env.dev.example
        $<TARGET_FILE_DIR:test_video_open>/env.dev.example
)

add_test(
    NAME VideoOpenTest
    COMMAND test_video_open
    WORKING_DIRECTORY $<TARGET_FILE_DIR:test_video_open>
)
