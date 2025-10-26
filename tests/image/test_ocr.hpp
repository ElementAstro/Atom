#pragma once

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#ifdef ATOM_IMAGE_HAS_OCR
#include <opencv2/imgproc.hpp>
#include <opencv2/opencv.hpp>
#include "atom/image/processing/ocr/ocr.hpp"
#endif

namespace fs = std::filesystem;

class OCRTest : public ::testing::Test {
protected:
    void SetUp() override {
#ifdef ATOM_IMAGE_HAS_OCR
        // Create test configuration
        config.language = "eng";
        config.enableDeskew = true;
        config.enablePerspectiveCorrection =
            false;  // Disable for simpler tests
        config.enableNoiseRemoval = true;
        config.enableTextDetection = false;  // Use simple OCR for tests
        config.enableSpellCheck = false;     // Disable for predictable results
        config.cacheResults = false;         // Disable caching for tests

        // Create test images
        createTestImages();

        // Create test dictionary file
        createTestDictionary();
#endif
    }

    void TearDown() override {
#ifdef ATOM_IMAGE_HAS_OCR
        // Clean up test files
        for (const auto& path : test_files) {
            std::remove(path.c_str());
        }
        std::remove(test_dict_path.c_str());
#endif
    }

#ifdef ATOM_IMAGE_HAS_OCR
    void createTestImages() {
        // Create simple text image
        cv::Mat simple_text(100, 300, CV_8UC3, cv::Scalar(255, 255, 255));
        cv::putText(simple_text, "Hello World", cv::Point(10, 50),
                    cv::FONT_HERSHEY_SIMPLEX, 1, cv::Scalar(0, 0, 0), 2);

        simple_text_path = "test_simple_text.png";
        cv::imwrite(simple_text_path, simple_text);
        test_files.push_back(simple_text_path);

        // Create noisy text image
        cv::Mat noisy_text(100, 300, CV_8UC3, cv::Scalar(255, 255, 255));
        cv::putText(noisy_text, "Noisy Text", cv::Point(10, 50),
                    cv::FONT_HERSHEY_SIMPLEX, 1, cv::Scalar(0, 0, 0), 2);

        // Add noise
        cv::Mat noise(noisy_text.size(), CV_8UC3);
        cv::randu(noise, cv::Scalar(0, 0, 0), cv::Scalar(50, 50, 50));
        cv::addWeighted(noisy_text, 0.8, noise, 0.2, 0, noisy_text);

        noisy_text_path = "test_noisy_text.png";
        cv::imwrite(noisy_text_path, noisy_text);
        test_files.push_back(noisy_text_path);

        // Create rotated text image
        cv::Mat rotated_text(150, 300, CV_8UC3, cv::Scalar(255, 255, 255));
        cv::putText(rotated_text, "Rotated", cv::Point(50, 75),
                    cv::FONT_HERSHEY_SIMPLEX, 1, cv::Scalar(0, 0, 0), 2);

        // Rotate the image
        cv::Point2f center(rotated_text.cols / 2.0f, rotated_text.rows / 2.0f);
        cv::Mat rotation_matrix = cv::getRotationMatrix2D(center, 15, 1.0);
        cv::warpAffine(rotated_text, rotated_text, rotation_matrix,
                       rotated_text.size());

        rotated_text_path = "test_rotated_text.png";
        cv::imwrite(rotated_text_path, rotated_text);
        test_files.push_back(rotated_text_path);

        // Create empty image
        cv::Mat empty_image(100, 300, CV_8UC3, cv::Scalar(255, 255, 255));
        empty_image_path = "test_empty.png";
        cv::imwrite(empty_image_path, empty_image);
        test_files.push_back(empty_image_path);
    }

    void createTestDictionary() {
        test_dict_path = "test_dictionary.txt";
        std::ofstream dict_file(test_dict_path);
        dict_file << "hello\n";
        dict_file << "world\n";
        dict_file << "test\n";
        dict_file << "text\n";
        dict_file << "noisy\n";
        dict_file << "rotated\n";
        dict_file << "image\n";
        dict_file << "processing\n";
        dict_file.close();
    }

    OCRConfig config;
    std::string simple_text_path;
    std::string noisy_text_path;
    std::string rotated_text_path;
    std::string empty_image_path;
    std::string test_dict_path;
    std::vector<std::string> test_files;
#endif
};

#ifdef ATOM_IMAGE_HAS_OCR

// Test OCR configuration loading
TEST_F(OCRTest, ConfigurationLoading) {
    // Test default configuration
    OCRConfig default_config;
    EXPECT_EQ(default_config.language, "eng");
    EXPECT_TRUE(default_config.enableDeskew);
    EXPECT_TRUE(default_config.enablePerspectiveCorrection);
    EXPECT_TRUE(default_config.enableNoiseRemoval);

    // Test loading from non-existent file (should use defaults)
    auto loaded_config = OCRConfig::fromFile("non_existent_config.json");
    EXPECT_EQ(loaded_config.language, "eng");
}

// Test progress reporter
TEST_F(OCRTest, ProgressReporter) {
    ProgressReporter reporter("Test Task", 100);

    // Test initial state
    reporter.reportProgress();

    // Test updates
    for (int i = 0; i < 10; ++i) {
        reporter.update(10);
    }

    // Test setting new total
    reporter.setTotal(200);
    reporter.update(50);
}

// Test OCR cache functionality
TEST_F(OCRTest, OCRCache) {
    OCRCache cache("test_cache", 1024 * 1024);  // 1MB cache

    // Load test image
    cv::Mat test_image = cv::imread(simple_text_path);
    ASSERT_FALSE(test_image.empty());

    // Test cache miss
    auto cached_result = cache.get(test_image);
    EXPECT_FALSE(cached_result.has_value());

    // Store result
    std::string test_result = "Hello World";
    cache.store(test_image, test_result);

    // Test cache hit
    cached_result = cache.get(test_image);
    ASSERT_TRUE(cached_result.has_value());
    EXPECT_EQ(cached_result.value(), test_result);

    // Test cache clearing
    cache.clear();
    cached_result = cache.get(test_image);
    EXPECT_FALSE(cached_result.has_value());
}

// Test spell checker
TEST_F(OCRTest, SpellChecker) {
    SpellChecker checker(test_dict_path);

    // Test correct words
    EXPECT_TRUE(checker.isCorrect("hello"));
    EXPECT_TRUE(checker.isCorrect("world"));
    EXPECT_TRUE(checker.isCorrect("test"));

    // Test incorrect words
    EXPECT_FALSE(checker.isCorrect("helo"));   // Missing 'l'
    EXPECT_FALSE(checker.isCorrect("wrold"));  // Transposed letters
    EXPECT_FALSE(checker.isCorrect("unknown"));

    // Test suggestions
    std::string suggestion = checker.suggest("helo");
    EXPECT_EQ(suggestion, "hello");

    suggestion = checker.suggest("wrold");
    EXPECT_EQ(suggestion, "world");

    // Test correct text processing
    std::string text = "helo wrold, this is a tset.";
    std::string corrected = checker.correctText(text);

    // Should contain corrected words
    EXPECT_NE(corrected.find("hello"), std::string::npos);
    EXPECT_NE(corrected.find("world"), std::string::npos);
    EXPECT_NE(corrected.find("test"), std::string::npos);

    // Test adding words
    checker.addWord("newword");
    EXPECT_TRUE(checker.isCorrect("newword"));
}

// Test enhanced OCR processor initialization
TEST_F(OCRTest, OCRProcessorInitialization) {
    // Test with valid configuration
    EXPECT_NO_THROW({ EnhancedOCRProcessor processor(config); });

    // Test with invalid language (should throw)
    OCRConfig invalid_config = config;
    invalid_config.language = "invalid_language_code";

    EXPECT_THROW(
        { EnhancedOCRProcessor processor(invalid_config); },
        std::runtime_error);
}

// Test basic OCR processing
TEST_F(OCRTest, BasicOCRProcessing) {
    EnhancedOCRProcessor processor(config);

    // Load test image
    cv::Mat test_image = cv::imread(simple_text_path);
    ASSERT_FALSE(test_image.empty());

    // Process image
    auto result = processor.processImage(test_image);

    // Check that we got some text
    EXPECT_FALSE(result.text.empty());
    EXPECT_GT(result.confidence, 0.0f);
    EXPECT_LE(result.confidence, 100.0f);

    // The text should contain "Hello" and "World" (case insensitive)
    std::string lower_text = result.text;
    std::transform(lower_text.begin(), lower_text.end(), lower_text.begin(),
                   ::tolower);
    EXPECT_NE(lower_text.find("hello"), std::string::npos);
    EXPECT_NE(lower_text.find("world"), std::string::npos);
}

// Test OCR with preprocessing
TEST_F(OCRTest, OCRWithPreprocessing) {
    // Enable preprocessing options
    config.enableDeskew = true;
    config.enableNoiseRemoval = true;

    EnhancedOCRProcessor processor(config);

    // Test with noisy image
    cv::Mat noisy_image = cv::imread(noisy_text_path);
    ASSERT_FALSE(noisy_image.empty());

    auto result = processor.processImage(noisy_image);

    // Should still be able to extract some text
    EXPECT_FALSE(result.text.empty());
    EXPECT_GT(result.confidence, 0.0f);

    // Test with rotated image
    cv::Mat rotated_image = cv::imread(rotated_text_path);
    ASSERT_FALSE(rotated_image.empty());

    auto rotated_result = processor.processImage(rotated_image);

    // Should handle rotation reasonably well
    EXPECT_FALSE(rotated_result.text.empty());
}

// Test OCR with empty/invalid images
TEST_F(OCRTest, OCRWithInvalidImages) {
    EnhancedOCRProcessor processor(config);

    // Test with empty image
    cv::Mat empty_image = cv::imread(empty_image_path);
    ASSERT_FALSE(empty_image.empty());

    auto result = processor.processImage(empty_image);

    // Should handle empty image gracefully
    EXPECT_GE(result.confidence, 0.0f);

    // Test with completely empty matrix
    cv::Mat null_image;
    EXPECT_THROW(processor.processImage(null_image), std::runtime_error);

    // Test with invalid image data
    cv::Mat invalid_image(10, 10, CV_8UC1, cv::Scalar(0));
    auto invalid_result = processor.processImage(invalid_image);
    EXPECT_GE(invalid_result.confidence, 0.0f);
}

// Test language detection - DISABLED: uses private methods
TEST_F(OCRTest, DISABLED_LanguageDetection) {
    EnhancedOCRProcessor processor(config);

    cv::Mat test_image = cv::imread(simple_text_path);
    ASSERT_FALSE(test_image.empty());

    std::string detected_language;
    // Note: detectLanguage is private - this test needs refactoring
    // bool detection_success = processor.detectLanguage(test_image,
    // detected_language);

    // Should detect some language (even if it's just the default)
    // EXPECT_TRUE(detection_success);
    // EXPECT_FALSE(detected_language.empty());
}

// Test deskewing functionality - DISABLED: uses private methods
TEST_F(OCRTest, DISABLED_DeskewFunctionality) {
    EnhancedOCRProcessor processor(config);

    cv::Mat rotated_image = cv::imread(rotated_text_path);
    ASSERT_FALSE(rotated_image.empty());

    // Apply deskewing - Note: deskew is private
    // cv::Mat deskewed = processor.deskew(rotated_image);

    // Should return an image of the same size
    // EXPECT_EQ(deskewed.rows, rotated_image.rows);
    // EXPECT_EQ(deskewed.cols, rotated_image.cols);
    // EXPECT_EQ(deskewed.channels(), rotated_image.channels());

    // Should be different from the original rotated image
    // cv::Mat diff;
    // cv::absdiff(rotated_image, deskewed, diff);
    // cv::Scalar mean_diff = cv::mean(diff);
    // EXPECT_GT(mean_diff[0], 0); // Should have some difference
}

// Test batch processing - DISABLED: processBatch method not implemented
TEST_F(OCRTest, DISABLED_BatchProcessing) {
    EnhancedOCRProcessor processor(config);

    // Create batch of images
    std::vector<cv::Mat> images;
    images.push_back(cv::imread(simple_text_path));
    images.push_back(cv::imread(noisy_text_path));
    images.push_back(cv::imread(empty_image_path));

    // Remove any empty images
    images.erase(std::remove_if(images.begin(), images.end(),
                                [](const cv::Mat& img) { return img.empty(); }),
                 images.end());

    ASSERT_FALSE(images.empty());

    // Process batch - Note: processBatch not implemented
    // auto results = processor.processBatch(images);

    // EXPECT_EQ(results.size(), images.size());

    // Check that all results are valid
    // for (const auto& result : results) {
    //     EXPECT_GE(result.confidence, 0.0f);
    //     EXPECT_LE(result.confidence, 100.0f);
    // }
}

// Performance test (disabled by default)
TEST_F(OCRTest, DISABLED_PerformanceTest) {
    EnhancedOCRProcessor processor(config);

    cv::Mat test_image = cv::imread(simple_text_path);
    ASSERT_FALSE(test_image.empty());

    // Measure processing time
    auto start = std::chrono::high_resolution_clock::now();

    const int iterations = 10;
    for (int i = 0; i < iterations; ++i) {
        auto result = processor.processImage(test_image);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration =
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    std::cout << "Processed " << iterations << " images in " << duration.count()
              << " ms" << std::endl;
    std::cout << "Average: " << (duration.count() / iterations)
              << " ms per image" << std::endl;
}

#else

// Placeholder test when OCR is not available
TEST_F(OCRTest, OCRNotAvailable) {
    GTEST_SKIP()
        << "OCR functionality not available (missing Tesseract/OpenCV)";
}

#endif
