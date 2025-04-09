/**
 * @file ocr.hpp
 * @brief Enhanced Optical Character Recognition (OCR) processing with Tesseract
 * and OpenCV
 *
 * This header defines classes for performing advanced OCR operations including:
 * - Image preprocessing (deskewing, noise removal, perspective correction)
 * - Text detection and recognition
 * - Super resolution enhancement
 * - Result caching and spell checking
 * - Batch and video processing
 */

#pragma once

#include <algorithm>
#include <atomic>
#include <chrono>
#include <execution>
#include <filesystem>
#include <format>
#include <fstream>
#include <future>
#include <iostream>
#include <memory_resource>
#include <mutex>
#include <numeric>
#include <optional>
#include <ranges>
#include <span>
#include <string>
#include <thread>
#include <unordered_map>
#include <variant>
#include <vector>

#include <opencv2/dnn.hpp>
#include <opencv2/dnn_superres.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/opencv.hpp>
#include <opencv2/text.hpp>

#include <leptonica/allheaders.h>
#include <tesseract/baseapi.h>

namespace fs = std::filesystem;
namespace views = std::ranges::views;

/**
 * @struct OCRConfig
 * @brief Configuration parameters for OCR processing
 *
 * Contains all tunable parameters for the OCR pipeline including:
 * - Language settings
 * - Feature toggles
 * - Preprocessing parameters
 * - Super resolution settings
 * - Text detection parameters
 * - Caching behavior
 */
struct OCRConfig {
    std::string language = "eng";  ///< Default language for OCR (English)
    bool enableDeskew = true;  ///< Enable automatic deskewing of rotated text
    bool enablePerspectiveCorrection = true;  ///< Enable perspective correction
    bool enableNoiseRemoval = true;   ///< Enable noise removal preprocessing
    bool enableTextDetection = true;  ///< Enable text region detection
    bool enableSpellCheck = false;    ///< Enable spell checking of results
    bool enableSuperResolution =
        false;                 ///< Enable super resolution enhancement
    bool cacheResults = true;  ///< Enable result caching
    size_t maxThreads =
        std::thread::hardware_concurrency();  ///< Max parallel threads

    /**
     * @struct PreprocessingParams
     * @brief Parameters for image preprocessing
     */
    struct PreprocessingParams {
        bool applyGaussianBlur =
            true;  ///< Apply Gaussian blur for noise reduction
        int gaussianKernelSize = 3;        ///< Kernel size for Gaussian blur
        bool applyThreshold = true;        ///< Apply thresholding
        bool useAdaptiveThreshold = true;  ///< Use adaptive thresholding
        int blockSize = 11;      ///< Block size for adaptive threshold
        double constantC = 2;    ///< Constant for adaptive threshold
        int medianBlurSize = 3;  ///< Size for median blur
        bool applyClahe =
            false;  ///< Apply Contrast Limited Adaptive Histogram Equalization
        double clipLimit = 2.0;      ///< CLAHE clip limit
        int binarizationMethod = 0;  ///< 0: Otsu, 1: Adaptive, 2: Sauvola
    } preprocessing;

    /**
     * @struct SuperResolutionParams
     * @brief Parameters for super resolution enhancement
     */
    struct SuperResolutionParams {
        std::string modelPath = "models/ESPCN_x4.pb";  ///< Path to SR model
        std::string modelName = "espcn";               ///< Model name
        int scale = 4;                                 ///< Upscaling factor
    } superResolution;

    /**
     * @struct TextDetectionParams
     * @brief Parameters for text detection
     */
    struct TextDetectionParams {
        float confThreshold = 0.5f;  ///< Confidence threshold for detection
        float nmsThreshold = 0.4f;   ///< Non-maximum suppression threshold
        int detectionSize = 320;     ///< Detection window size
        std::string modelPath =
            "models/east_text_detection.pb";  ///< Path to detection model
    } textDetection;

    /**
     * @struct CacheParams
     * @brief Parameters for result caching
     */
    struct CacheParams {
        size_t maxCacheSize =
            100 * 1024 * 1024;  ///< Max cache size in bytes (100MB)
        std::string cacheDir = ".ocr_cache";  ///< Cache directory path
    } cache;

    /**
     * @brief Load configuration from JSON file
     * @param filename Path to JSON configuration file
     * @return OCRConfig instance populated from file
     */
    static OCRConfig fromFile(const std::string& filename);
};

/**
 * @class ProgressReporter
 * @brief Tracks and reports progress of OCR operations
 *
 * Provides thread-safe progress tracking and reporting for long-running
 * OCR operations with timing information.
 */
class ProgressReporter {
private:
    std::string m_taskName;            ///< Name of the task being tracked
    size_t m_total;                    ///< Total work units
    std::atomic<size_t> m_current{0};  ///< Current progress
    std::chrono::time_point<std::chrono::steady_clock>
        m_startTime;             ///< Start time
    mutable std::mutex m_mutex;  ///< Mutex for thread safety

public:
    /**
     * @brief Construct a new ProgressReporter
     * @param taskName Name of the task
     * @param total Total work units expected
     */
    ProgressReporter(std::string taskName, size_t total);

    /**
     * @brief Update progress counter
     * @param increment Number of work units completed (default 1)
     */
    void update(size_t increment = 1);

    /**
     * @brief Set new total work units
     * @param total New total work units
     */
    void setTotal(size_t total);

    /**
     * @brief Print current progress to stdout
     */
    void reportProgress() const;
};

/**
 * @class OCRCache
 * @brief Caching system for OCR results
 *
 * Implements both in-memory and filesystem caching of OCR results
 * to improve performance on repeated processing of the same images.
 */
class OCRCache {
private:
    std::unordered_map<std::string, std::string>
        m_memoryCache;        ///< In-memory cache
    std::string m_cacheDir;   ///< Filesystem cache directory
    size_t m_maxCacheSize;    ///< Maximum cache size in bytes
    std::mutex m_cacheMutex;  ///< Mutex for thread safety

    /**
     * @brief Calculate hash of image for cache key
     * @param img Input image
     * @return SHA-256 hash string
     */
    std::string calculateHash(const cv::Mat& img) const;

    /**
     * @brief Get filesystem path for cached result
     * @param key Cache key (image hash)
     * @return Full filesystem path to cache file
     */
    fs::path getCacheFilePath(const std::string& key) const;

public:
    /**
     * @brief Construct a new OCRCache
     * @param cacheDir Cache directory path
     * @param maxCacheSize Maximum cache size in bytes
     */
    OCRCache(const std::string& cacheDir, size_t maxCacheSize);

    /**
     * @brief Get cached result if available
     * @param img Input image to check
     * @return Optional containing result if found, empty otherwise
     */
    std::optional<std::string> get(const cv::Mat& img);

    /**
     * @brief Store result in cache
     * @param img Input image
     * @param result OCR result to cache
     */
    void store(const cv::Mat& img, const std::string& result);

    /**
     * @brief Clean cache if over size limit
     *
     * Removes oldest cache entries until under size limit
     */
    void cleanCacheIfNeeded();

    /**
     * @brief Clear all cached results
     */
    void clear();
};

/**
 * @class SpellChecker
 * @brief Spell checking and correction for OCR results
 *
 * Implements dictionary-based spell checking with Levenshtein distance
 * for suggestion generation.
 */
class SpellChecker {
private:
    std::unordered_map<std::string, int> m_dictionary;  ///< Word dictionary

    /**
     * @brief Calculate Levenshtein edit distance
     * @param s1 First string
     * @param s2 Second string
     * @return Edit distance between strings
     */
    int levenshteinDistance(const std::string& s1, const std::string& s2);

public:
    /**
     * @brief Construct a new SpellChecker
     * @param dictionaryPath Path to dictionary file (one word per line)
     */
    SpellChecker(const std::string& dictionaryPath = "");

    /**
     * @brief Load dictionary from file
     * @param filePath Path to dictionary file
     */
    void loadDictionary(const std::string& filePath);

    /**
     * @brief Add word to dictionary
     * @param word Word to add
     */
    void addWord(const std::string& word);

    /**
     * @brief Check if word is in dictionary
     * @param word Word to check
     * @return True if word is in dictionary
     */
    bool isCorrect(const std::string& word);

    /**
     * @brief Get spelling suggestion for word
     * @param word Word to get suggestion for
     * @return Suggested correction
     */
    std::string suggest(const std::string& word);

    /**
     * @brief Correct spelling in text
     * @param text Input text
     * @return Corrected text
     */
    std::string correctText(const std::string& text);
};

/**
 * @class EnhancedOCRProcessor
 * @brief Main OCR processing class with advanced features
 *
 * Provides comprehensive OCR capabilities including preprocessing,
 * text detection, recognition, and post-processing.
 */
class EnhancedOCRProcessor {
private:
    tesseract::TessBaseAPI m_tessApi;              ///< Tesseract OCR engine
    OCRConfig m_config;                            ///< Configuration parameters
    std::unique_ptr<OCRCache> m_cache;             ///< Result cache
    std::unique_ptr<Logger> m_logger;              ///< Logging system
    std::unique_ptr<SpellChecker> m_spellChecker;  ///< Spell checker
    std::unique_ptr<cv::dnn_superres::DnnSuperResImpl>
        m_superRes;  ///< Super resolution

    cv::dnn::Net m_textDetector;  ///< Text detection model

    /**
     * @brief Detect language of text in image
     * @param image Input image
     * @param detectedLanguage Output detected language
     * @return True if language detected successfully
     */
    bool detectLanguage(const cv::Mat& image, std::string& detectedLanguage);

    /**
     * @brief Apply super resolution to image
     * @param image Input image
     * @return Enhanced high-resolution image
     */
    cv::Mat applySuperResolution(const cv::Mat& image);

    /**
     * @brief Deskew rotated text
     * @param image Input image
     * @return Deskewed image
     */
    cv::Mat deskew(const cv::Mat& image);

    /**
     * @brief Detect text regions in image
     * @param image Input image
     * @return Vector of bounding rectangles for text regions
     */
    std::vector<cv::Rect> detectTextRegions(const cv::Mat& image);

    /**
     * @brief Correct perspective distortion
     * @param image Input image
     * @return Corrected image
     */
    cv::Mat applyPerspectiveCorrection(const cv::Mat& image);

    /**
     * @brief Remove noise from image
     * @param image Input image
     * @return Cleaned image
     */
    cv::Mat removeNoise(const cv::Mat& image);

    /**
     * @brief Apply Sauvola binarization
     * @param grayImage Grayscale input image
     * @param windowSize Window size for local thresholding
     * @param k Parameter for Sauvola algorithm
     * @return Binarized image
     */
    cv::Mat sauvolaBinarization(const cv::Mat& grayImage, int windowSize = 21,
                                double k = 0.34);

    /**
     * @brief Full preprocessing pipeline
     * @param inputImage Raw input image
     * @return Preprocessed image ready for OCR
     */
    cv::Mat enhancedPreprocess(const cv::Mat& inputImage);

    /**
     * @brief Calculate confidence score for OCR result
     * @param api Tesseract API instance
     * @return Confidence score (0-100)
     */
    float calculateConfidence(const tesseract::TessBaseAPI& api);

    /**
     * @brief Extract structured data from OCR text
     * @param text Raw OCR text
     * @return Map of structured data fields
     */
    std::unordered_map<std::string, std::string> extractStructuredData(
        const std::string& text);

    /**
     * @brief Process text regions separately
     * @param image Input image
     * @return Combined OCR results from all regions
     */
    std::string processTextRegions(const cv::Mat& image);

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

        /**
         * @brief Construct a new OCRResult
         * @param t Extracted text
         * @param conf Confidence score
         * @param data Structured data fields
         * @param lang Detected language
         */
        OCRResult(std::string t, float conf,
                  const std::unordered_map<std::string, std::string>& data = {},
                  std::string lang = "eng")
            : text(std::move(t)),
              confidence(conf),
              structuredData(data),
              language(std::move(lang)) {}
    };

public:
    /**
     * @brief Construct a new EnhancedOCRProcessor
     * @param config Configuration parameters
     */
    EnhancedOCRProcessor(const OCRConfig& config = OCRConfig());

    ~EnhancedOCRProcessor();

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
     * @brief Clean up resources
     */
    void cleanup();
};