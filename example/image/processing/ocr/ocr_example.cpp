/**
 * @file ocr_example.cpp
 * @brief Example demonstrating OCR (Optical Character Recognition) capabilities
 *
 * This example covers:
 * - Basic text recognition from images
 * - Image preprocessing for better OCR results
 * - Text detection and region extraction
 * - Batch OCR processing
 * - OCR configuration and optimization
 * - Result caching
 *
 * @author Atom Image Module
 * @date 2025
 */

#include <iostream>
#include <opencv2/core.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>

#ifdef ATOM_IMAGE_HAS_TESSERACT
#include "atom/image/processing/ocr/ocr.hpp"
#endif

using namespace std;

/**
 * @brief Create a test image with text
 */
cv::Mat createTextImage() {
    cv::Mat img(200, 600, CV_8UC1, cv::Scalar(255));

    // Add text to image
    cv::putText(img, "Hello OCR World!", cv::Point(50, 100),
                cv::FONT_HERSHEY_SIMPLEX, 1.5, cv::Scalar(0), 2);

    return img;
}

/**
 * @brief Demonstrate basic OCR
 */
void demonstrateBasicOCR() {
    cout << "\n=== Basic OCR ===\n";

#ifdef ATOM_IMAGE_HAS_TESSERACT
    try {
        // Create test image
        cv::Mat img = createTextImage();

        // Configure OCR
        OCRConfig config;
        config.language = "eng";
        config.enableDeskew = true;
        config.enableNoiseRemoval = true;

        // Create OCR processor
        OCRProcessor processor(config);

        // Perform OCR
        auto result = processor.processImage(img);

        cout << "Recognized text: " << result.text << "\n";
        cout << "Confidence: " << result.confidence << "\n";

    } catch (const exception& e) {
        cerr << "Error: " << e.what() << "\n";
    }
#else
    cout << "Tesseract OCR not available (ATOM_IMAGE_HAS_TESSERACT not "
            "defined)\n";
#endif
}

/**
 * @brief Demonstrate OCR with preprocessing
 */
void demonstratePreprocessing() {
    cout << "\n=== OCR with Preprocessing ===\n";

#ifdef ATOM_IMAGE_HAS_TESSERACT
    try {
        cv::Mat img = createTextImage();

        // Add noise to make it challenging
        cv::Mat noise(img.size(), img.type());
        cv::randn(noise, 0, 10);
        img += noise;

        // Configure with preprocessing
        OCRConfig config;
        config.preprocessing.applyGaussianBlur = true;
        config.preprocessing.gaussianKernelSize = 3;
        config.preprocessing.applyThreshold = true;
        config.preprocessing.useAdaptiveThreshold = true;

        OCRProcessor processor(config);
        auto result = processor.processImage(img);

        cout << "Text (with preprocessing): " << result.text << "\n";
        cout << "Confidence: " << result.confidence << "\n";

    } catch (const exception& e) {
        cerr << "Error: " << e.what() << "\n";
    }
#else
    cout << "Tesseract OCR not available\n";
#endif
}

/**
 * @brief Demonstrate text detection
 */
void demonstrateTextDetection() {
    cout << "\n=== Text Detection ===\n";

#ifdef ATOM_IMAGE_HAS_TESSERACT
    try {
        cv::Mat img = createTextImage();

        OCRConfig config;
        config.enableTextDetection = true;

        OCRProcessor processor(config);
        auto regions = processor.detectTextRegions(img);

        cout << "Detected " << regions.size() << " text regions\n";

        for (size_t i = 0; i < regions.size(); ++i) {
            cout << "Region " << i << ": "
                 << "x=" << regions[i].x << ", "
                 << "y=" << regions[i].y << ", "
                 << "w=" << regions[i].width << ", "
                 << "h=" << regions[i].height << "\n";
        }

    } catch (const exception& e) {
        cerr << "Error: " << e.what() << "\n";
    }
#else
    cout << "Tesseract OCR not available\n";
#endif
}

/**
 * @brief Demonstrate batch processing
 */
void demonstrateBatchProcessing() {
    cout << "\n=== Batch OCR Processing ===\n";

#ifdef ATOM_IMAGE_HAS_TESSERACT
    try {
        // Create multiple test images
        vector<cv::Mat> images;
        for (int i = 0; i < 3; ++i) {
            images.push_back(createTextImage());
        }

        OCRConfig config;
        config.maxThreads = 2;

        OCRProcessor processor(config);
        auto results = processor.processBatch(images);

        cout << "Processed " << results.size() << " images\n";

        for (size_t i = 0; i < results.size(); ++i) {
            cout << "Image " << i << ": " << results[i].text << "\n";
        }

    } catch (const exception& e) {
        cerr << "Error: " << e.what() << "\n";
    }
#else
    cout << "Tesseract OCR not available\n";
#endif
}

int main() {
    demonstrateBasicOCR();
    demonstratePreprocessing();
    demonstrateTextDetection();
    demonstrateBatchProcessing();

    return 0;
}
