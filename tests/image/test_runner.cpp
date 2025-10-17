#include <gtest/gtest.h>
#include <filesystem>
#include <iostream>
#include <string>

// Include all test headers
#include "test_hdu.hpp"
#include "test_image_blob.hpp"
#include "test_image_processor.hpp"
#include "test_ocr.hpp"
#include "test_performance.hpp"
#include "test_utils.hpp"
#ifdef ATOM_IMAGE_HAS_OPENCV
#include "test_computer_vision.hpp"
#include "test_realtime.hpp"
#include "test_ser.hpp"
#endif
#include "test_advanced_formats.hpp"
#include "test_gpu_acceleration.hpp"

// Custom test listener for better output formatting
class ImageTestListener : public ::testing::EmptyTestEventListener {
public:
    void OnTestStart(const ::testing::TestInfo& test_info) override {
        std::cout << "[ RUN      ] " << test_info.test_case_name() << "."
                  << test_info.name() << std::endl;
    }

    void OnTestEnd(const ::testing::TestInfo& test_info) override {
        if (test_info.result()->Passed()) {
            std::cout << "[       OK ] " << test_info.test_case_name() << "."
                      << test_info.name() << " ("
                      << test_info.result()->elapsed_time() << " ms)"
                      << std::endl;
        } else {
            std::cout << "[  FAILED  ] " << test_info.test_case_name() << "."
                      << test_info.name() << " ("
                      << test_info.result()->elapsed_time() << " ms)"
                      << std::endl;
        }
    }

    void OnTestCaseStart(const ::testing::TestCase& test_case) override {
        std::cout << "[----------] " << test_case.test_to_run_count()
                  << " tests from " << test_case.name() << std::endl;
    }

    void OnTestCaseEnd(const ::testing::TestCase& test_case) override {
        std::cout << "[----------] " << test_case.test_to_run_count()
                  << " tests from " << test_case.name() << " ("
                  << test_case.elapsed_time() << " ms total)" << std::endl;
    }
};

// Custom main function for better control
int main(int argc, char** argv) {
    std::cout << "==========================================================="
              << std::endl;
    std::cout << "           Atom Image Processing Module Tests             "
              << std::endl;
    std::cout << "==========================================================="
              << std::endl;

    // Initialize Google Test
    ::testing::InitGoogleTest(&argc, argv);

    // Add custom listener
    ::testing::TestEventListeners& listeners =
        ::testing::UnitTest::GetInstance()->listeners();
    delete listeners.Release(listeners.default_result_printer());
    listeners.Append(new ImageTestListener);

    // Print configuration information
    std::cout << "\nTest Configuration:" << std::endl;

#ifdef ATOM_IMAGE_HAS_OPENCV
    std::cout << "  OpenCV: ENABLED" << std::endl;
#else
    std::cout << "  OpenCV: DISABLED" << std::endl;
#endif

#ifdef ATOM_IMAGE_HAS_CFITSIO
    std::cout << "  CFITSIO: ENABLED" << std::endl;
#else
    std::cout << "  CFITSIO: DISABLED" << std::endl;
#endif

#ifdef ATOM_IMAGE_HAS_OCR
    std::cout << "  OCR (Tesseract): ENABLED" << std::endl;
#else
    std::cout << "  OCR (Tesseract): DISABLED" << std::endl;
#endif

    std::cout << "\n==========================================================="
              << std::endl;

    // Run tests
    int result = RUN_ALL_TESTS();

    // Print summary
    std::cout << "\n==========================================================="
              << std::endl;

    auto* unit_test = ::testing::UnitTest::GetInstance();

    std::cout << "Test Summary:" << std::endl;
    std::cout << "  Total test cases: " << unit_test->total_test_case_count()
              << std::endl;
    std::cout << "  Total tests: " << unit_test->total_test_count()
              << std::endl;
    std::cout << "  Successful tests: " << unit_test->successful_test_count()
              << std::endl;
    std::cout << "  Failed tests: " << unit_test->failed_test_count()
              << std::endl;
    std::cout << "  Disabled tests: " << unit_test->disabled_test_count()
              << std::endl;
    std::cout << "  Total time: " << unit_test->elapsed_time() << " ms"
              << std::endl;

    if (result == 0) {
        std::cout << "\n🎉 ALL TESTS PASSED! 🎉" << std::endl;
    } else {
        std::cout << "\n❌ SOME TESTS FAILED ❌" << std::endl;
    }

    std::cout << "==========================================================="
              << std::endl;

    return result;
}

// Test suite information
namespace {
// Register test suites for documentation
struct TestSuiteInfo {
    std::string name;
    std::string description;
    std::vector<std::string> dependencies;
};

[[maybe_unused]] std::vector<TestSuiteInfo> getTestSuites() {
    return {
        {"BlobTest",
         "Tests for the image blob container class including memory "
         "management, "
         "format conversion, and integration with various image libraries",
         {"atom-error"}},
        {"ImageProcessorTest",
         "Tests for the unified image processing pipeline including resize, "
         "rotation, filtering, and batch processing operations",
         {"atom-error", "OpenCV (optional)"}},
        {"ImageHDUTest",
         "Tests for FITS HDU (Header Data Unit) functionality including "
         "reading, writing, compression, and astronomical image processing",
         {"atom-error", "CFITSIO (optional)"}},
        {"OCRTest",
         "Tests for optical character recognition functionality including "
         "text detection, preprocessing, spell checking, and batch processing",
         {"atom-error", "OpenCV (optional)", "Tesseract (optional)",
          "Leptonica (optional)"}},
        {"SERTest",
         "Tests for SER (Simple Extensible Recorder) format support including "
         "reading, writing, frame processing, and quality assessment",
         {"atom-error", "OpenCV (optional)"}},
        {"ComputerVisionTest",
         "Tests for computer vision operations including feature detection, "
         "object detection, face detection, segmentation, tracking, and image "
         "analysis",
         {"atom-error", "OpenCV (optional)"}},
        {"GPUAccelerationTest",
         "Tests for GPU-accelerated image processing including buffer "
         "management, "
         "kernel execution, and GPU operations across multiple backends",
         {"atom-error", "CUDA/OpenCL/Vulkan (optional)"}},
        {"RealtimeProcessingTest",
         "Tests for real-time video processing including capture, frame "
         "processing, "
         "callbacks, threading, and performance monitoring",
         {"atom-error", "OpenCV (optional)"}},
        {"AdvancedFormatsTest",
         "Tests for advanced image format support including RAW, DICOM, HDR, "
         "animations, vector formats, and specialized scientific formats",
         {"atom-error", "LibRaw/DCMTK/OpenEXR (optional)"}}};
}
}  // namespace

// Utility functions for test setup
namespace test_utils {

// Check if required dependencies are available
bool checkDependencies() {
    bool all_good = true;

#ifndef ATOM_IMAGE_HAS_OPENCV
    std::cout << "Warning: OpenCV not available - some tests will be skipped"
              << std::endl;
#endif

#ifndef ATOM_IMAGE_HAS_CFITSIO
    std::cout << "Warning: CFITSIO not available - FITS tests will be limited"
              << std::endl;
#endif

#ifndef ATOM_IMAGE_HAS_OCR
    std::cout
        << "Warning: OCR dependencies not available - OCR tests will be skipped"
        << std::endl;
#endif

    return all_good;
}

// Create test data directory if needed
void setupTestEnvironment() {
    std::filesystem::path test_data_dir = "test_data";
    if (!std::filesystem::exists(test_data_dir)) {
        std::filesystem::create_directory(test_data_dir);
    }
}

// Clean up test environment
void cleanupTestEnvironment() {
    std::filesystem::path test_data_dir = "test_data";
    if (std::filesystem::exists(test_data_dir)) {
        std::filesystem::remove_all(test_data_dir);
    }
}
}  // namespace test_utils

// Global test setup and teardown
class ImageTestEnvironment : public ::testing::Environment {
public:
    void SetUp() override {
        std::cout << "Setting up test environment..." << std::endl;
        test_utils::setupTestEnvironment();
        test_utils::checkDependencies();
    }

    void TearDown() override {
        std::cout << "Cleaning up test environment..." << std::endl;
        test_utils::cleanupTestEnvironment();
    }
};

// Register global test environment
static ::testing::Environment* const test_env =
    ::testing::AddGlobalTestEnvironment(new ImageTestEnvironment);
