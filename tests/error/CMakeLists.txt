# Error module tests

# Test sources
set(TEST_SOURCES
    comprehensive_test.cpp
    test_comprehensive_error_system.cpp
    test_error_formatting.cpp
    test_error_integration.cpp
    test_enhanced_stacktrace.cpp
)

# Create test executables
foreach(TEST_SOURCE ${TEST_SOURCES})
    get_filename_component(TEST_NAME ${TEST_SOURCE} NAME_WE)
    
    add_executable(${TEST_NAME} ${TEST_SOURCE})
    
    target_link_libraries(${TEST_NAME}
        PRIVATE
            atom-error
            GTest::gtest
            GTest::gtest_main
            ${CMAKE_THREAD_LIBS_INIT}
    )
    
    target_include_directories(${TEST_NAME}
        PRIVATE
            ${CMAKE_SOURCE_DIR}
            ${CMAKE_SOURCE_DIR}/atom
    )
    
    # Add test to CTest
    add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})
    
    # Set test properties
    set_tests_properties(${TEST_NAME} PROPERTIES
        TIMEOUT 300
        LABELS "error;unit"
    )
endforeach()

# Add integration test with longer timeout
set_tests_properties(test_error_integration PROPERTIES
    TIMEOUT 600
    LABELS "error;integration"
)

# Add performance test markers
set_tests_properties(test_error_integration PROPERTIES
    LABELS "error;integration;performance"
)
