# CMakeLists.txt for Atom Image Tests (Custom Configuration)
# This project is licensed under the terms of the GPL3 license.
#
# Project Name: Atom-Image-Tests
# Description: Comprehensive Test Suite for Atom Image Module
# Author: Max Qian
# License: GPL3

cmake_minimum_required(VERSION 3.20)
project(atom-image-tests VERSION 1.0.0 LANGUAGES CXX)

# =============================================================================
# Find Required Packages
# =============================================================================
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

# =============================================================================
# Include Test Infrastructure (Optional for Custom Modules)
# =============================================================================
# Include the standardized test infrastructure if available
if(EXISTS "${CMAKE_SOURCE_DIR}/tests/tests/CMakeLists.txt")
    add_subdirectory("${CMAKE_SOURCE_DIR}/tests/tests" "${CMAKE_CURRENT_BINARY_DIR}/test_infrastructure")
endif()

# =============================================================================
# Verify Module Dependencies
# =============================================================================
# Ensure the target module is available
if(NOT TARGET atom-image)
    message(FATAL_ERROR "atom-image target not found. Make sure ATOM_BUILD_IMAGE is enabled.")
endif()

# =============================================================================
# Custom Configuration (Preserved)
# =============================================================================
# Include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../../
    ${CMAKE_CURRENT_SOURCE_DIR}/../../atom/image
)

# =============================================================================
# Test Source Configuration
# =============================================================================
# Define test sources
set(TEST_SOURCES
    test_runner.cpp
)

# Define test headers (for IDE support)
set(TEST_HEADERS
    test_utils.hpp
    test_image_blob.hpp
    test_image_processor.hpp
    test_format_detector.hpp
    test_image_loader.hpp
    test_image_saver.hpp
    test_exif.hpp
    test_fits_file.hpp
    test_fits_data.hpp
    test_fits_utils.hpp
    test_fits_header.hpp
    test_hdu.hpp
    test_filters.hpp
    test_transforms.hpp
    test_enhancement.hpp
    test_ml_processing.hpp
    test_performance.hpp
    test_ocr.hpp
    test_ser.hpp
    test_computer_vision.hpp
    test_gpu_acceleration.hpp
    test_realtime.hpp
    test_advanced_formats.hpp
)

# =============================================================================
# Create Test Executable
# =============================================================================
add_executable(atom_image_tests ${TEST_SOURCES} ${TEST_HEADERS})

# =============================================================================
# Target Configuration
# =============================================================================
# Set target properties
set_target_properties(atom_image_tests PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    OUTPUT_NAME "atom_image_tests"
)

# =============================================================================
# Link Libraries
# =============================================================================
target_link_libraries(atom_image_tests
    PRIVATE
        # Test infrastructure (if available)
        $<TARGET_NAME_IF_EXISTS:atom-test-common>

        # Module under test
        atom-image

        # GoogleTest
        GTest::GTest
        GTest::Main

        # System libraries
        Threads::Threads
)

# Add TBB if available
if(TBB_FOUND)
    target_link_libraries(atom_image_tests PRIVATE TBB::tbb)
endif()

# Add ZLIB (required for FITS compression)
target_link_libraries(atom_image_tests PRIVATE ZLIB::ZLIB)

# =============================================================================
# Optional Dependencies (Custom Configuration)
# =============================================================================
# Add conditional compilation flags based on available dependencies
if(OpenCV_FOUND)
    target_compile_definitions(atom_image_tests PRIVATE ATOM_IMAGE_HAS_OPENCV)
    target_link_libraries(atom_image_tests PRIVATE ${OpenCV_LIBS})
    target_include_directories(atom_image_tests PRIVATE ${OpenCV_INCLUDE_DIRS})
endif()

if(CFITSIO_FOUND)
    target_compile_definitions(atom_image_tests PRIVATE ATOM_IMAGE_HAS_CFITSIO)
    target_link_libraries(atom_image_tests PRIVATE ${CFITSIO_LIBRARIES})
    target_include_directories(atom_image_tests PRIVATE ${CFITSIO_INCLUDE_DIRS})
endif()

if(TESSERACT_FOUND AND LEPTONICA_FOUND)
    target_compile_definitions(atom_image_tests PRIVATE ATOM_IMAGE_HAS_OCR)
    target_link_libraries(atom_image_tests PRIVATE ${TESSERACT_LIBRARIES} ${LEPTONICA_LIBRARIES})
    target_include_directories(atom_image_tests PRIVATE ${TESSERACT_INCLUDE_DIRS} ${LEPTONICA_INCLUDE_DIRS})
endif()

# Compiler-specific options
if(MSVC)
    target_compile_options(atom_image_tests PRIVATE /W4)
else()
    target_compile_options(atom_image_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()

# =============================================================================
# Test Registration
# =============================================================================
enable_testing()

# Include GoogleTest for standardized test discovery
include(GoogleTest)
# Temporarily disabled due to DLL dependency issues during test discovery
# gtest_discover_tests(atom_image_tests
#     WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
#     PROPERTIES
#     LABELS "image"
#     TIMEOUT 300
# )

# Manual test registration as workaround
add_test(NAME atom_image_tests
    COMMAND atom_image_tests
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(atom_image_tests PROPERTIES
    LABELS "image"
    TIMEOUT 300
)

# =============================================================================
# Custom Test Cases (Preserved)
# =============================================================================
# Add test cases
add_test(NAME ImageBlobTests
         COMMAND atom-image-tests --gtest_filter="BlobTest.*")

add_test(NAME ImageProcessorTests
         COMMAND atom-image-tests --gtest_filter="ImageProcessorTest.*")

add_test(NAME FormatDetectorTests
         COMMAND atom-image-tests --gtest_filter="FormatDetectorTest.*")

add_test(NAME ImageLoaderTests
         COMMAND atom-image-tests --gtest_filter="ImageLoaderTest.*")

add_test(NAME ImageSaverTests
         COMMAND atom-image-tests --gtest_filter="ImageSaverTest.*")

add_test(NAME ExifTests
         COMMAND atom-image-tests --gtest_filter="ExifTest.*")

add_test(NAME FITSFileTests
         COMMAND atom-image-tests --gtest_filter="FITSFileTest.*")

add_test(NAME FITSDataTests
         COMMAND atom-image-tests --gtest_filter="FITSDataTest.*")

add_test(NAME FITSUtilsTests
         COMMAND atom-image-tests --gtest_filter="FITSUtilsTest.*")

add_test(NAME FilterTests
         COMMAND atom-image-tests --gtest_filter="FilterTest.*")

add_test(NAME TransformTests
         COMMAND atom-image-tests --gtest_filter="TransformTest.*")

add_test(NAME EnhancementTests
         COMMAND atom-image-tests --gtest_filter="EnhancementTest.*")

add_test(NAME MLProcessingTests
         COMMAND atom-image-tests --gtest_filter="MLProcessingTest.*")

add_test(NAME HDUTests
         COMMAND atom-image-tests --gtest_filter="ImageHDUTest.*")

add_test(NAME OCRTests
         COMMAND atom-image-tests --gtest_filter="OCRTest.*")

add_test(NAME SERTests
         COMMAND atom-image-tests --gtest_filter="SERTest.*")

add_test(NAME ComputerVisionTests
         COMMAND atom-image-tests --gtest_filter="ComputerVisionTest.*")

add_test(NAME GPUAccelerationTests
         COMMAND atom-image-tests --gtest_filter="GPUAccelerationTest.*")

add_test(NAME RealtimeProcessingTests
         COMMAND atom-image-tests --gtest_filter="RealtimeProcessingTest.*")

add_test(NAME AdvancedFormatsTests
         COMMAND atom-image-tests --gtest_filter="AdvancedFormatsTest.*")

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

set_tests_properties(FormatDetectorTests PROPERTIES
    LABELS "io;formats;detection"
    TIMEOUT 60
)

set_tests_properties(ImageLoaderTests PROPERTIES
    LABELS "io;loading;formats"
    TIMEOUT 90
)

set_tests_properties(ImageSaverTests PROPERTIES
    LABELS "io;saving;formats"
    TIMEOUT 90
)

set_tests_properties(ExifTests PROPERTIES
    LABELS "metadata;exif;gps"
    TIMEOUT 60
)

set_tests_properties(FITSFileTests PROPERTIES
    LABELS "fits;astronomy;cfitsio"
    TIMEOUT 120
)

set_tests_properties(FITSDataTests PROPERTIES
    LABELS "fits;data;astronomy"
    TIMEOUT 60
)

set_tests_properties(FITSUtilsTests PROPERTIES
    LABELS "fits;utils;astronomy"
    TIMEOUT 90
)

set_tests_properties(FilterTests PROPERTIES
    LABELS "processing;filters;opencv"
    TIMEOUT 120
)

set_tests_properties(TransformTests PROPERTIES
    LABELS "processing;transforms;opencv"
    TIMEOUT 120
)

set_tests_properties(EnhancementTests PROPERTIES
    LABELS "processing;enhancement;opencv"
    TIMEOUT 120
)

set_tests_properties(MLProcessingTests PROPERTIES
    LABELS "processing;ml;ai"
    TIMEOUT 300
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

set_tests_properties(ComputerVisionTests PROPERTIES
    LABELS "processing;cv;opencv;ai"
    TIMEOUT 180
)

set_tests_properties(GPUAccelerationTests PROPERTIES
    LABELS "processing;gpu;cuda;opencl"
    TIMEOUT 120
)

set_tests_properties(RealtimeProcessingTests PROPERTIES
    LABELS "processing;realtime;video"
    TIMEOUT 120
)

set_tests_properties(AdvancedFormatsTests PROPERTIES
    LABELS "formats;raw;dicom;hdr;advanced"
    TIMEOUT 180
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

add_custom_target(test-cv
    COMMAND atom-image-tests --gtest_filter="ComputerVisionTest.*"
    DEPENDS atom-image-tests
    COMMENT "Running computer vision tests"
)

add_custom_target(test-gpu
    COMMAND atom-image-tests --gtest_filter="GPUAccelerationTest.*"
    DEPENDS atom-image-tests
    COMMENT "Running GPU acceleration tests"
)

add_custom_target(test-realtime
    COMMAND atom-image-tests --gtest_filter="RealtimeProcessingTest.*"
    DEPENDS atom-image-tests
    COMMENT "Running real-time processing tests"
)

add_custom_target(test-advanced-formats
    COMMAND atom-image-tests --gtest_filter="AdvancedFormatsTest.*"
    DEPENDS atom-image-tests
    COMMENT "Running advanced formats tests"
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
