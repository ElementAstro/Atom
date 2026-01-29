# Window Manager Tests CMakeLists.txt

cmake_minimum_required(VERSION 3.16)

# Find test framework (using a simple approach for now) In a real project, you
# might use Google Test, Catch2, etc.

# Test sources
set(TEST_SOURCES test_common.cpp test_system_info.cpp
                 test_window_management.cpp test_theme_detection.cpp)

# Create test executable
add_executable(wm_tests ${TEST_SOURCES})

# Link with the wm library
target_link_libraries(wm_tests PRIVATE atom_sysinfo_wm spdlog::spdlog)

# Include directories
target_include_directories(wm_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/..)

# Compiler options
if(MSVC)
  target_compile_options(wm_tests PRIVATE /W4)
else()
  target_compile_options(wm_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Add tests to CTest
add_test(NAME wm_basic_tests COMMAND wm_tests)

# Individual test executables for specific functionality
add_executable(test_system_info_only test_system_info.cpp)
target_link_libraries(test_system_info_only PRIVATE atom_sysinfo_wm
                                                    spdlog::spdlog)
target_include_directories(test_system_info_only
                           PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/..)
add_test(NAME system_info_test COMMAND test_system_info_only)

add_executable(test_theme_only test_theme_detection.cpp)
target_link_libraries(test_theme_only PRIVATE atom_sysinfo_wm spdlog::spdlog)
target_include_directories(test_theme_only
                           PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/..)
add_test(NAME theme_test COMMAND test_theme_only)

# Platform-specific tests
if(WIN32)
  add_executable(test_windows_specific test_windows_specific.cpp)
  target_link_libraries(test_windows_specific PRIVATE atom_sysinfo_wm
                                                      spdlog::spdlog)
  target_include_directories(test_windows_specific
                             PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/..)
  add_test(NAME windows_specific_test COMMAND test_windows_specific)
elseif(UNIX AND NOT APPLE)
  add_executable(test_linux_specific test_linux_specific.cpp)
  target_link_libraries(test_linux_specific PRIVATE atom_sysinfo_wm
                                                    spdlog::spdlog)
  target_include_directories(test_linux_specific
                             PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/..)
  add_test(NAME linux_specific_test COMMAND test_linux_specific)
elseif(APPLE)
  add_executable(test_macos_specific test_macos_specific.cpp)
  target_link_libraries(test_macos_specific PRIVATE atom_sysinfo_wm
                                                    spdlog::spdlog)
  target_include_directories(test_macos_specific
                             PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/..)
  add_test(NAME macos_specific_test COMMAND test_macos_specific)
endif()

# Set test properties
set_tests_properties(wm_basic_tests PROPERTIES TIMEOUT 30 LABELS "basic;unit")

set_tests_properties(system_info_test PROPERTIES TIMEOUT 10 LABELS
                                                 "system;unit")

set_tests_properties(theme_test PROPERTIES TIMEOUT 10 LABELS "theme;unit")
