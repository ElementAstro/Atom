# USB serial module tests These tests cover usb.hpp functionality Only compiled
# when libusb is available

set(USB_TEST_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/test_usb.cpp
    PARENT_SCOPE)
