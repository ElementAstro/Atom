cmake_minimum_required(VERSION 3.20)

# Find required packages
find_package(spdlog REQUIRED)

# Check if we have a testing framework
find_package(GTest QUIET)
find_package(Catch2 QUIET)

if(GTest_FOUND)
  message(STATUS "Using Google Test for battery tests")

  # Test files - only use existing files
  set(TEST_SOURCES simple_test.cpp)

  # Create test executable
  add_executable(battery_tests ${TEST_SOURCES})

  target_link_libraries(
    battery_tests PRIVATE atom_sysinfo_battery spdlog::spdlog GTest::gtest
                          GTest::gtest_main)

  target_compile_features(battery_tests PRIVATE cxx_std_20)

  # Add to CTest
  include(GoogleTest)
  gtest_discover_tests(battery_tests)

elseif(Catch2_FOUND)
  message(STATUS "Using Catch2 for battery tests")

  # Test files for Catch2
  set(TEST_SOURCES test_battery_catch2.cpp)

  add_executable(battery_tests_catch2 ${TEST_SOURCES})

  target_link_libraries(
    battery_tests_catch2 PRIVATE atom_sysinfo_battery spdlog::spdlog
                                 Catch2::Catch2WithMain)

  target_compile_features(battery_tests_catch2 PRIVATE cxx_std_20)

  # Add to CTest
  include(CTest)
  include(Catch)
  catch_discover_tests(battery_tests_catch2)

else()
  message(STATUS "No testing framework found, creating simple test executable")

  # Simple test without framework
  add_executable(simple_battery_test simple_test.cpp)

  target_link_libraries(simple_battery_test PRIVATE atom_sysinfo_battery
                                                    spdlog::spdlog)

  target_compile_features(simple_battery_test PRIVATE cxx_std_20)

  # Add as a test
  add_test(NAME SimpleBatteryTest COMMAND simple_battery_test)
endif()

# Set output directory
if(TARGET battery_tests)
  set_target_properties(battery_tests PROPERTIES RUNTIME_OUTPUT_DIRECTORY
                                                 ${CMAKE_BINARY_DIR}/tests)
endif()

if(TARGET battery_tests_catch2)
  set_target_properties(
    battery_tests_catch2 PROPERTIES RUNTIME_OUTPUT_DIRECTORY
                                    ${CMAKE_BINARY_DIR}/tests)
endif()

if(TARGET simple_battery_test)
  set_target_properties(
    simple_battery_test PROPERTIES RUNTIME_OUTPUT_DIRECTORY
                                   ${CMAKE_BINARY_DIR}/tests)
endif()
