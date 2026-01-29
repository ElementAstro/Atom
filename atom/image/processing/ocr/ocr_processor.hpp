/**
 * @file ocr_processor.hpp
 * @brief Main OCR processing class with advanced features
 */

#pragma once

#include "ocr_cache.hpp"
#include "ocr_config.hpp"
#include "spell_checker.hpp"

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include <opencv2/core.hpp>
#include <opencv2/dnn.hpp>

#include <tesseract/baseapi.h>

// Feature detection for super resolution
#ifndef ATOM_OCR_HAS_SUPERRES
#if __has_include(<opencv2/dnn_superres.hpp>)
#define ATOM_OCR_HAS_SUPERRES 1
#else
#define ATOM_OCR_HAS_SUPERRES 0
#endif
#endif

#if ATOM_OCR_HAS_SUPERRES
#include <opencv2/dnn_superres.hpp>
#endif

namespace atom::image::ocr {

/**
 * @struct OCRResult
 * @brief Container for OCR results with metadata
 */
struct OCRResult {
    std::string text;  ///< Extracted text
    float confidence;  ///< Average confidence score (0-100)
    std::unordered_map<std::string, std::string>
        structuredData;    ///< Extracted fields
    std::string language;  ///< Detected language

    OCRResult() : confidence(0.0f) {}

    OCRResult(std::string t, float conf,
              const std::unordered_map<std::string, std::string>& data = {},
              std::string lang = "eng")
        : text(std::move(t)),
          confidence(conf),
          structuredData(data),
          language(std::move(lang)) {}
};

/**
 * @class OCRProcessor
 * @brief Main OCR processing class with advanced features
 *
 * Provides comprehensive OCR capabilities including preprocessing,
 * text detection, recognition, and post-processing.
 */
class OCRProcessor {
public:
    /**
     * @brief Construct a new OCRProcessor
     * @param config Configuration parameters
     */
    explicit OCRProcessor(const OCRConfig& config = OCRConfig());

    ~OCRProcessor();

    // Disable copy
    OCRProcessor(const OCRProcessor&) = delete;
    OCRProcessor& operator=(const OCRProcessor&) = delete;

    /**
     * @brief Process single image
     * @param image Input image
     * @return OCRResult containing extracted text and metadata
     */
    OCRResult processImage(const cv::Mat& image);

    /**
     * @brief Process batch of images in parallel
     * @param images Vector of input images
     * @return Vector of OCRResults for each image
     */
    std::vector<OCRResult> processBatchParallel(
        const std::vector<cv::Mat>& images);

    /**
     * @brief Process video file and extract text from frames
     * @param videoPath Path to video file
     * @param frameInterval Process every Nth frame
     * @return Vector of frame number/OCRResult pairs
     */
    std::vector<std::pair<int, OCRResult>> processVideo(
        const std::string& videoPath, int frameInterval = 30);

    /**
     * @brief Process PDF document
     * @param pdfPath Path to PDF file
     * @return Vector of OCRResults for each page
     */
    std::vector<OCRResult> processPDF(const std::string& pdfPath);

    /**
     * @brief Export results to file
     * @param results Vector of OCRResults
     * @param outputPath Output file path
     * @param format Output format ("txt", "json", "csv")
     * @return True if export succeeded
     */
    bool exportResults(const std::vector<OCRResult>& results,
                       const std::string& outputPath,
                       const std::string& format = "txt");

    /**
     * @brief Get current configuration
     * @return Reference to configuration
     */
    const OCRConfig& getConfig() const { return m_config; }

    /**
     * @brief Update configuration
     * @param config New configuration
     */
    void setConfig(const OCRConfig& config);

    /**
     * @brief Clean up resources
     */
    void cleanup();

private:
    tesseract::TessBaseAPI m_tessApi;              ///< Tesseract OCR engine
    OCRConfig m_config;                            ///< Configuration
    std::unique_ptr<OCRCache> m_cache;             ///< Result cache
    std::unique_ptr<SpellChecker> m_spellChecker;  ///< Spell checker
    cv::dnn::Net m_textDetector;                   ///< Text detection model
    bool m_superResAvailable = false;              ///< Super resolution status

#if ATOM_OCR_HAS_SUPERRES
    std::unique_ptr<cv::dnn_superres::DnnSuperResImpl> m_superRes;
#endif

    // Preprocessing methods
    cv::Mat enhancedPreprocess(const cv::Mat& inputImage);
    cv::Mat deskew(const cv::Mat& image);
    cv::Mat applyPerspectiveCorrection(const cv::Mat& image);
    cv::Mat removeNoise(const cv::Mat& image);
    cv::Mat sauvolaBinarization(const cv::Mat& grayImage, int windowSize = 21,
                                double k = 0.34);
    cv::Mat applySuperResolution(const cv::Mat& image);

    // Detection methods
    bool detectLanguage(const cv::Mat& image, std::string& detectedLanguage);
    std::vector<cv::Rect> detectTextRegions(const cv::Mat& image);
    std::string processTextRegions(const cv::Mat& image);

    // Post-processing methods
    float calculateConfidence(tesseract::TessBaseAPI& api);
    std::unordered_map<std::string, std::string> extractStructuredData(
        const std::string& text);
};

/**
 * @brief Create an OCR processor with default configuration
 * @return Unique pointer to OCRProcessor
 */
std::unique_ptr<OCRProcessor> createOCRProcessor();

/**
 * @brief Create an OCR processor from configuration file
 * @param configPath Path to configuration file
 * @return Unique pointer to OCRProcessor
 */
std::unique_ptr<OCRProcessor> createOCRProcessor(const std::string& configPath);

}  // namespace atom::image::ocr
