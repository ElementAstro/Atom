/**
 * @file calibration.hpp
 * @brief Astronomical image calibration frame processing
 *
 * This file provides functionality for:
 * - Creating master calibration frames (bias, dark, flat)
 * - Applying calibration to science images
 * - Bad pixel detection and correction
 * - Hot/cold pixel removal
 *
 * @copyright Copyright (C) 2023-2025
 */

#ifndef ATOM_IMAGE_CALIBRATION_HPP
#define ATOM_IMAGE_CALIBRATION_HPP

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "fits_file.hpp"
#include "fits_image_types.hpp"
#include "hdu.hpp"

namespace atom::image::fits {

/**
 * @enum StackingMethod
 * @brief Methods for combining multiple frames
 */
enum class StackingMethod {
    AVERAGE,     ///< Simple arithmetic mean
    MEDIAN,      ///< Median (robust to outliers)
    SIGMA_CLIP,  ///< Sigma-clipped mean
    WINSORIZED,  ///< Winsorized mean (trimmed)
    MIN,         ///< Minimum value
    MAX,         ///< Maximum value
    SUM          ///< Sum of all values
};

/**
 * @enum NormalizationMethod
 * @brief Methods for normalizing flat fields
 */
enum class NormalizationMethod {
    MEAN,       ///< Normalize by mean value
    MEDIAN,     ///< Normalize by median value
    MODE,       ///< Normalize by mode (most common value)
    PERCENTILE  ///< Normalize by specific percentile
};

/**
 * @struct CalibrationParams
 * @brief Parameters for calibration operations
 */
struct CalibrationParams {
    // Stacking parameters
    StackingMethod stackMethod = StackingMethod::MEDIAN;
    double sigmaLow = 3.0;      ///< Lower sigma for clipping
    double sigmaHigh = 3.0;     ///< Upper sigma for clipping
    int maxIterations = 5;      ///< Max iterations for sigma clipping
    double trimFraction = 0.1;  ///< Fraction to trim (winsorized)

    // Flat normalization
    NormalizationMethod flatNorm = NormalizationMethod::MEAN;
    double normPercentile = 50.0;  ///< Percentile for normalization

    // Bad pixel detection
    double hotPixelThreshold = 5.0;    ///< Sigma for hot pixel detection
    double coldPixelThreshold = 5.0;   ///< Sigma for cold pixel detection
    double cosmicRayThreshold = 10.0;  ///< Threshold for cosmic ray detection

    // Dark scaling
    bool scaleDarks = true;  ///< Scale darks by exposure time

    // Output options
    bool preserveOriginal = true;  ///< Keep original data
    bool updateHeader = true;      ///< Update header with calibration info
};

/**
 * @struct CalibrationStats
 * @brief Statistics from calibration operations
 */
struct CalibrationStats {
    double meanValue = 0.0;    ///< Mean of calibrated image
    double medianValue = 0.0;  ///< Median of calibrated image
    double stdDev = 0.0;       ///< Standard deviation
    double minValue = 0.0;     ///< Minimum value
    double maxValue = 0.0;     ///< Maximum value

    int badPixelCount = 0;   ///< Number of bad pixels detected
    int hotPixelCount = 0;   ///< Number of hot pixels
    int coldPixelCount = 0;  ///< Number of cold pixels
    int cosmicRayCount = 0;  ///< Number of cosmic rays removed

    int framesStacked = 0;        ///< Number of frames stacked
    double processingTime = 0.0;  ///< Processing time in seconds
};

/**
 * @struct BadPixelMap
 * @brief Map of bad pixel locations
 */
struct BadPixelMap {
    int width = 0;
    int height = 0;
    std::vector<bool> pixels;  ///< True = bad pixel

    // Convenience methods
    [[nodiscard]] bool isBad(int x, int y) const {
        if (x < 0 || x >= width || y < 0 || y >= height)
            return false;
        return pixels[y * width + x];
    }

    void setBad(int x, int y, bool bad = true) {
        if (x >= 0 && x < width && y >= 0 && y < height) {
            pixels[y * width + x] = bad;
        }
    }

    [[nodiscard]] int countBad() const {
        return std::count(pixels.begin(), pixels.end(), true);
    }
};

/**
 * @class CalibrationProcessor
 * @brief Processes astronomical calibration frames
 *
 * Provides comprehensive calibration workflow for astronomical
 * imaging including master frame creation and application.
 */
class CalibrationProcessor {
public:
    using ProgressCallback = std::function<void(float, const std::string&)>;

    /**
     * @brief Default constructor
     */
    CalibrationProcessor() = default;

    /**
     * @brief Construct with parameters
     * @param params Calibration parameters
     */
    explicit CalibrationProcessor(const CalibrationParams& params);

    /**
     * @brief Set calibration parameters
     * @param params New parameters
     */
    void setParams(const CalibrationParams& params) { params_ = params; }

    /**
     * @brief Get current parameters
     * @return Current parameters
     */
    [[nodiscard]] const CalibrationParams& getParams() const { return params_; }

    // Master frame creation

    /**
     * @brief Create master bias from multiple bias frames
     * @param biasFrames Vector of bias frame paths
     * @param progressCallback Progress callback
     * @return Master bias HDU
     */
    [[nodiscard]] std::unique_ptr<ImageHDU> createMasterBias(
        const std::vector<std::string>& biasFrames,
        ProgressCallback progressCallback = nullptr);

    /**
     * @brief Create master dark from multiple dark frames
     * @param darkFrames Vector of dark frame paths
     * @param masterBias Optional master bias for subtraction
     * @param progressCallback Progress callback
     * @return Master dark HDU
     */
    [[nodiscard]] std::unique_ptr<ImageHDU> createMasterDark(
        const std::vector<std::string>& darkFrames,
        const ImageHDU* masterBias = nullptr,
        ProgressCallback progressCallback = nullptr);

    /**
     * @brief Create master flat from multiple flat frames
     * @param flatFrames Vector of flat frame paths
     * @param masterBias Optional master bias
     * @param masterDark Optional master dark
     * @param progressCallback Progress callback
     * @return Master flat HDU
     */
    [[nodiscard]] std::unique_ptr<ImageHDU> createMasterFlat(
        const std::vector<std::string>& flatFrames,
        const ImageHDU* masterBias = nullptr,
        const ImageHDU* masterDark = nullptr,
        ProgressCallback progressCallback = nullptr);

    // Calibration application

    /**
     * @brief Calibrate a science image
     * @param image Image to calibrate
     * @param masterBias Master bias (optional)
     * @param masterDark Master dark (optional)
     * @param masterFlat Master flat (optional)
     * @return Calibrated image HDU
     */
    [[nodiscard]] std::unique_ptr<ImageHDU> calibrateImage(
        const ImageHDU& image, const ImageHDU* masterBias = nullptr,
        const ImageHDU* masterDark = nullptr,
        const ImageHDU* masterFlat = nullptr);

    /**
     * @brief Subtract bias from an image
     * @param image Image to process
     * @param bias Bias frame
     * @return Bias-subtracted image
     */
    [[nodiscard]] std::unique_ptr<ImageHDU> subtractBias(const ImageHDU& image,
                                                         const ImageHDU& bias);

    /**
     * @brief Subtract dark from an image
     * @param image Image to process
     * @param dark Dark frame
     * @param imageExpTime Image exposure time
     * @param darkExpTime Dark exposure time
     * @return Dark-subtracted image
     */
    [[nodiscard]] std::unique_ptr<ImageHDU> subtractDark(
        const ImageHDU& image, const ImageHDU& dark, double imageExpTime = 0.0,
        double darkExpTime = 0.0);

    /**
     * @brief Divide by flat field
     * @param image Image to process
     * @param flat Flat field
     * @return Flat-corrected image
     */
    [[nodiscard]] std::unique_ptr<ImageHDU> divideFlat(const ImageHDU& image,
                                                       const ImageHDU& flat);

    // Bad pixel handling

    /**
     * @brief Create bad pixel map from dark frame
     * @param dark Dark frame to analyze
     * @return Bad pixel map
     */
    [[nodiscard]] BadPixelMap createBadPixelMap(const ImageHDU& dark);

    /**
     * @brief Create bad pixel map from multiple darks
     * @param darkFrames Vector of dark frame paths
     * @param progressCallback Progress callback
     * @return Bad pixel map
     */
    [[nodiscard]] BadPixelMap createBadPixelMap(
        const std::vector<std::string>& darkFrames,
        ProgressCallback progressCallback = nullptr);

    /**
     * @brief Fix bad pixels using interpolation
     * @param image Image to fix
     * @param badPixels Bad pixel map
     * @return Fixed image
     */
    [[nodiscard]] std::unique_ptr<ImageHDU> fixBadPixels(
        const ImageHDU& image, const BadPixelMap& badPixels);

    /**
     * @brief Detect and remove cosmic rays
     * @param image Image to process
     * @return Image with cosmic rays removed
     */
    [[nodiscard]] std::unique_ptr<ImageHDU> removeCosmicRays(
        const ImageHDU& image);

    // Image stacking

    /**
     * @brief Stack multiple images
     * @param images Vector of image HDUs
     * @param method Stacking method
     * @return Stacked image
     */
    template <typename T>
    [[nodiscard]] std::unique_ptr<ImageHDU> stackImages(
        const std::vector<const ImageHDU*>& images,
        StackingMethod method = StackingMethod::MEDIAN);

    /**
     * @brief Stack images from files
     * @param filePaths Vector of file paths
     * @param method Stacking method
     * @param progressCallback Progress callback
     * @return Stacked image
     */
    [[nodiscard]] std::unique_ptr<ImageHDU> stackImagesFromFiles(
        const std::vector<std::string>& filePaths,
        StackingMethod method = StackingMethod::MEDIAN,
        ProgressCallback progressCallback = nullptr);

    // Statistics

    /**
     * @brief Get statistics from last operation
     * @return Calibration statistics
     */
    [[nodiscard]] const CalibrationStats& getStats() const { return stats_; }

    /**
     * @brief Calculate image statistics
     * @param image Image to analyze
     * @return Statistics
     */
    template <typename T>
    [[nodiscard]] CalibrationStats calculateStats(const ImageHDU& image);

private:
    CalibrationParams params_;
    mutable CalibrationStats stats_;

    /**
     * @brief Stack pixel values using specified method
     */
    template <typename T>
    [[nodiscard]] T stackPixel(const std::vector<T>& values,
                               StackingMethod method) const;

    /**
     * @brief Calculate median of a vector
     */
    template <typename T>
    [[nodiscard]] T calculateMedian(std::vector<T>& values) const;

    /**
     * @brief Calculate sigma-clipped mean
     */
    template <typename T>
    [[nodiscard]] T calculateSigmaClippedMean(std::vector<T>& values,
                                              double sigmaLow, double sigmaHigh,
                                              int maxIterations) const;

    /**
     * @brief Normalize flat field
     */
    template <typename T>
    void normalizeFlat(std::vector<T>& data, int width, int height) const;

    /**
     * @brief Get exposure time from header
     */
    [[nodiscard]] double getExposureTime(const ImageHDU& image) const;

    /**
     * @brief Interpolate value at a bad pixel
     */
    template <typename T>
    [[nodiscard]] T interpolatePixel(
        const std::vector<T>& data, int x, int y, int width, int height,
        const BadPixelMap* badPixels = nullptr) const;

    /**
     * @brief Load image HDU from file
     */
    [[nodiscard]] std::unique_ptr<ImageHDU> loadImageHDU(
        const std::string& filename) const;
};

/**
 * @brief Get stacking method name
 * @param method Stacking method
 * @return Method name
 */
[[nodiscard]] std::string stackingMethodToString(StackingMethod method);

/**
 * @brief Parse stacking method from string
 * @param name Method name
 * @return Stacking method
 */
[[nodiscard]] StackingMethod stackingMethodFromString(const std::string& name);

}  // namespace atom::image::fits

#endif  // ATOM_IMAGE_CALIBRATION_HPP
