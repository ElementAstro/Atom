cmake_minimum_required(VERSION 3.20)

project(atom-image-tests VERSION 1.0.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find required packages
find_package(GTest REQUIRED)
find_package(Threads REQUIRED)
find_package(TBB QUIET)
find_package(ZLIB REQUIRED)

# Find optional packages (same as main image module)
find_package(OpenCV QUIET)
find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(CFITSIO cfitsio QUIET)
    pkg_check_modules(TESSERACT tesseract QUIET)
    pkg_check_modules(LEPTONICA lept QUIET)
endif()

# Include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../../
    ${CMAKE_CURRENT_SOURCE_DIR}/../../atom/image
)

# Define test sources
set(TEST_SOURCES
    test_runner.cpp
)

# Define test headers (for IDE support)
set(TEST_HEADERS
    test_image_blob.hpp
    test_image_processor.hpp
    test_hdu.hpp
    test_ocr.hpp
    test_ser.hpp
)

# Create test executable
add_executable(atom-image-tests ${TEST_SOURCES} ${TEST_HEADERS})

# Set target properties
set_target_properties(atom-image-tests PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    OUTPUT_NAME "atom_image_tests"
)

# Link libraries
target_link_libraries(atom-image-tests
    PRIVATE
        atom-image
        GTest::GTest
        GTest::Main
        Threads::Threads
)

# Add TBB if available
if(TBB_FOUND)
    target_link_libraries(atom-image-tests PRIVATE TBB::tbb)
endif()

# Add ZLIB (required for FITS compression)
target_link_libraries(atom-image-tests PRIVATE ZLIB::ZLIB)

# Add conditional compilation flags based on available dependencies
if(OpenCV_FOUND)
    target_compile_definitions(atom-image-tests PRIVATE ATOM_IMAGE_HAS_OPENCV)
    target_link_libraries(atom-image-tests PRIVATE ${OpenCV_LIBS})
    target_include_directories(atom-image-tests PRIVATE ${OpenCV_INCLUDE_DIRS})
endif()

if(CFITSIO_FOUND)
    target_compile_definitions(atom-image-tests PRIVATE ATOM_IMAGE_HAS_CFITSIO)
    target_link_libraries(atom-image-tests PRIVATE ${CFITSIO_LIBRARIES})
    target_include_directories(atom-image-tests PRIVATE ${CFITSIO_INCLUDE_DIRS})
endif()

if(TESSERACT_FOUND AND LEPTONICA_FOUND)
    target_compile_definitions(atom-image-tests PRIVATE ATOM_IMAGE_HAS_OCR)
    target_link_libraries(atom-image-tests PRIVATE ${TESSERACT_LIBRARIES} ${LEPTONICA_LIBRARIES})
    target_include_directories(atom-image-tests PRIVATE ${TESSERACT_INCLUDE_DIRS} ${LEPTONICA_INCLUDE_DIRS})
endif()

# Compiler-specific options
if(MSVC)
    target_compile_options(atom-image-tests PRIVATE /W4)
else()
    target_compile_options(atom-image-tests PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Enable testing
enable_testing()

# Add test cases
add_test(NAME ImageBlobTests
         COMMAND atom-image-tests --gtest_filter="BlobTest.*")

add_test(NAME ImageProcessorTests
         COMMAND atom-image-tests --gtest_filter="ImageProcessorTest.*")

add_test(NAME HDUTests
         COMMAND atom-image-tests --gtest_filter="ImageHDUTest.*")

add_test(NAME OCRTests
         COMMAND atom-image-tests --gtest_filter="OCRTest.*")

add_test(NAME SERTests
         COMMAND atom-image-tests --gtest_filter="SERTest.*")

# Add comprehensive test that runs all tests
add_test(NAME AllImageTests
         COMMAND atom-image-tests)

# Set test properties
set_tests_properties(ImageBlobTests PROPERTIES
    LABELS "core;blob;memory"
    TIMEOUT 60
)

set_tests_properties(ImageProcessorTests PROPERTIES
    LABELS "core;processing;opencv"
    TIMEOUT 120
)

set_tests_properties(HDUTests PROPERTIES
    LABELS "fits;astronomy;cfitsio"
    TIMEOUT 90
)

set_tests_properties(OCRTests PROPERTIES
    LABELS "ocr;tesseract;text"
    TIMEOUT 180
)

set_tests_properties(SERTests PROPERTIES
    LABELS "ser;video;astronomy"
    TIMEOUT 90
)

set_tests_properties(AllImageTests PROPERTIES
    LABELS "comprehensive"
    TIMEOUT 300
)

# Performance tests (disabled by default)
add_test(NAME PerformanceTests
         COMMAND atom-image-tests --gtest_filter="*Performance*")

set_tests_properties(PerformanceTests PROPERTIES
    LABELS "performance;benchmark"
    TIMEOUT 600
    DISABLED TRUE
)

# Memory tests (if valgrind is available)
find_program(VALGRIND_PROGRAM valgrind)
if(VALGRIND_PROGRAM)
    add_test(NAME MemoryTests
             COMMAND ${VALGRIND_PROGRAM}
                     --tool=memcheck
                     --leak-check=full
                     --show-leak-kinds=all
                     --track-origins=yes
                     --error-exitcode=1
                     $<TARGET_FILE:atom-image-tests>
                     --gtest_filter="BlobTest.*:ImageProcessorTest.BasicOperations")

    set_tests_properties(MemoryTests PROPERTIES
        LABELS "memory;valgrind"
        TIMEOUT 600
    )
endif()

# Coverage target (if gcov is available)
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    find_program(GCOV_PROGRAM gcov)
    find_program(LCOV_PROGRAM lcov)
    find_program(GENHTML_PROGRAM genhtml)

    if(GCOV_PROGRAM AND LCOV_PROGRAM AND GENHTML_PROGRAM)
        target_compile_options(atom-image-tests PRIVATE --coverage)
        target_link_options(atom-image-tests PRIVATE --coverage)

        add_custom_target(coverage
            COMMAND ${LCOV_PROGRAM} --directory . --capture --output-file coverage.info
            COMMAND ${LCOV_PROGRAM} --remove coverage.info '/usr/*' --output-file coverage.info
            COMMAND ${LCOV_PROGRAM} --list coverage.info
            COMMAND ${GENHTML_PROGRAM} -o coverage_html coverage.info
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
            COMMENT "Generating code coverage report"
        )

        add_dependencies(coverage atom-image-tests)
    endif()
endif()

# Install test executable (optional)
if(ATOM_IMAGE_INSTALL_TESTS)
    install(TARGETS atom-image-tests
        RUNTIME DESTINATION bin/tests
        COMPONENT tests
    )

    # Install test data files if any
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_data)
        install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/test_data
            DESTINATION share/atom-image/tests
            COMPONENT tests
        )
    endif()
endif()

# Custom targets for running specific test suites
add_custom_target(test-blob
    COMMAND atom-image-tests --gtest_filter="BlobTest.*"
    DEPENDS atom-image-tests
    COMMENT "Running image blob tests"
)

add_custom_target(test-processor
    COMMAND atom-image-tests --gtest_filter="ImageProcessorTest.*"
    DEPENDS atom-image-tests
    COMMENT "Running image processor tests"
)

add_custom_target(test-fits
    COMMAND atom-image-tests --gtest_filter="ImageHDUTest.*"
    DEPENDS atom-image-tests
    COMMENT "Running FITS/HDU tests"
)

add_custom_target(test-ocr
    COMMAND atom-image-tests --gtest_filter="OCRTest.*"
    DEPENDS atom-image-tests
    COMMENT "Running OCR tests"
)

add_custom_target(test-ser
    COMMAND atom-image-tests --gtest_filter="SERTest.*"
    DEPENDS atom-image-tests
    COMMENT "Running SER format tests"
)

# Benchmark target
add_custom_target(benchmark
    COMMAND atom-image-tests --gtest_filter="*Performance*" --gtest_also_run_disabled_tests
    DEPENDS atom-image-tests
    COMMENT "Running performance benchmarks"
)

# Print configuration summary
message(STATUS "Atom Image Tests Configuration:")
message(STATUS "  OpenCV: ${OpenCV_FOUND}")
message(STATUS "  CFITSIO: ${CFITSIO_FOUND}")
message(STATUS "  Tesseract: ${TESSERACT_FOUND}")
message(STATUS "  Leptonica: ${LEPTONICA_FOUND}")
message(STATUS "  Valgrind: ${VALGRIND_PROGRAM}")
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    message(STATUS "  Coverage: ${GCOV_PROGRAM}")
endif()
