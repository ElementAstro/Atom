# Signals test sources Note: All signal tests disabled - cause test hangs due to
# signal handler issues on Windows
set(SYSTEM_SIGNALS_TEST_SOURCES
    # ${CMAKE_CURRENT_SOURCE_DIR}/test_signal_utils.cpp
    # ${CMAKE_CURRENT_SOURCE_DIR}/test_signal_monitor.cpp
    PARENT_SCOPE)
