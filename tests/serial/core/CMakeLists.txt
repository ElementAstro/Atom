# Core serial module tests These tests cover scanner.hpp and serial_port.hpp
# functionality

set(CORE_TEST_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/test_scanner.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test_serial_port.cpp
    PARENT_SCOPE)
