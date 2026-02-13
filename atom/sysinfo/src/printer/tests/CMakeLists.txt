# CMakeLists.txt for sysinfo_printer tests
cmake_minimum_required(VERSION 3.20)

# Find required packages for testing
find_package(GTest QUIET)

if(NOT GTest_FOUND)
  # If GTest is not found, try to use a simple test framework
  message(STATUS "GTest not found, creating simple tests")

  set(TEST_SOURCES test_compatibility.cpp test_formatters.cpp
                   test_exporters.cpp test_reports.cpp)

  # Create test executables
  foreach(TEST_SOURCE ${TEST_SOURCES})
    get_filename_component(TEST_NAME ${TEST_SOURCE} NAME_WE)
    add_executable(${TEST_NAME} ${TEST_SOURCE})

    target_link_libraries(${TEST_NAME} PRIVATE atom_sysinfo_printer
                                               spdlog::spdlog)

    set_target_properties(
      ${TEST_NAME}
      PROPERTIES CXX_STANDARD 20
                 CXX_STANDARD_REQUIRED ON
                 CXX_EXTENSIONS OFF)

    # Add as test
    add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})
  endforeach()

else()
  # Use GTest if available
  message(STATUS "GTest found, creating GTest-based tests")

  set(GTEST_SOURCES gtest_main.cpp gtest_formatters.cpp gtest_exporters.cpp
                    gtest_reports.cpp gtest_compatibility.cpp)

  add_executable(sysinfo_printer_tests ${GTEST_SOURCES})

  target_link_libraries(
    sysinfo_printer_tests PRIVATE atom_sysinfo_printer spdlog::spdlog
                                  GTest::gtest GTest::gtest_main)

  set_target_properties(
    sysinfo_printer_tests
    PROPERTIES CXX_STANDARD 20
               CXX_STANDARD_REQUIRED ON
               CXX_EXTENSIONS OFF)

  # Discover tests
  include(GoogleTest)
  gtest_discover_tests(sysinfo_printer_tests)
endif()

# Install test files
install(
  FILES ${TEST_SOURCES}
  DESTINATION share/atom/sysinfo_printer/tests
  OPTIONAL)
