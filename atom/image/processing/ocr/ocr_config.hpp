/**
 * @file ocr_config.hpp
 * @brief Configuration parameters for OCR processing
 */

#pragma once

#include <string>
#include <thread>

namespace atom::image::ocr {

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

}  // namespace atom::image::ocr
