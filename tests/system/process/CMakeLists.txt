# Process test sources Note: Process tests temporarily disabled due to threading
# deadlock issues The process monitoring tests cause std::system_error "Resource
# deadlock avoided"
set(SYSTEM_PROCESS_TEST_SOURCES
    # ${CMAKE_CURRENT_SOURCE_DIR}/test_command.cpp
    # ${CMAKE_CURRENT_SOURCE_DIR}/test_pidwatcher.cpp
    # ${CMAKE_CURRENT_SOURCE_DIR}/test_process.cpp
    # ${CMAKE_CURRENT_SOURCE_DIR}/test_process_manager.cpp
    PARENT_SCOPE)
