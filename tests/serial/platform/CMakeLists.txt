# Platform-specific serial port tests These tests cover serial_port_win.hpp and
# serial_port_unix.hpp functionality

set(PLATFORM_TEST_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/test_serial_port_platform.cpp
    PARENT_SCOPE)
