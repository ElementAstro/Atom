#ifndef ATOM_IMAGE_FILTERS_HPP
#define ATOM_IMAGE_FILTERS_HPP

/**
 * @file filters.hpp
 * @brief Advanced image filtering operations
 *
 * This module provides comprehensive image filtering capabilities including
 * convolution filters, morphological operations, frequency domain filters,
 * and advanced denoising algorithms.
 *
 * @author Atom Framework Team
 * @date 2025
 * @version 1.0.0
 */

#include <array>
#include <concepts>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <variant>
#include <vector>
#include "../core/image_blob.hpp"

namespace atom::image {

/**
 * @brief Filter types for image processing
 */
enum class FilterType {
    // Basic filters
    GAUSSIAN_BLUR,
    BOX_BLUR,
    MOTION_BLUR,
    RADIAL_BLUR,

    // Sharpening filters
    SHARPEN,
    UNSHARP_MASK,
    HIGH_PASS,

    // Edge detection
    SOBEL,
    SCHARR,
    PREWITT,
    ROBERTS,
    CANNY,
    LAPLACIAN,

    // Noise reduction
    MEDIAN,
    MEAN,
    BILATERAL,
    NON_LOCAL_MEANS,
    WIENER,
    MIN_FILTER,
    MAX_FILTER,

    // Morphological operations
    EROSION,
    DILATION,
    OPENING,
    CLOSING,
    GRADIENT,
    TOP_HAT,
    BLACK_HAT,

    // Frequency domain
    LOW_PASS,
    HIGH_PASS_FREQ,
    BAND_PASS,
    BAND_STOP,
    NOTCH,

    // Artistic filters
    EMBOSS,
    EDGE_ENHANCE,
    FIND_EDGES,
    SMOOTH,
    SMOOTH_MORE,
    OIL_PAINT,
    PENCIL_SKETCH,
    CARTOON,
    POSTERIZE,
    PIXELATE,
    VIGNETTE,
    SEPIA,

    // Custom
    CUSTOM_KERNEL
};

/**
 * @brief Morphological structuring element shapes
 */
enum class StructuringElement { RECTANGLE, ELLIPSE, CROSS, DIAMOND, CUSTOM };

/**
 * @brief Modern variant-based parameter value
 */
using FilterParamValue =
    std::variant<int, double, std::string, bool, std::vector<double>>;

/**
 * @brief Modern parameter map using variant
 */
using FilterParamMap = std::unordered_map<std::string, FilterParamValue>;

/**
 * @brief Filter parameters container (legacy struct-based)
 */
struct FilterParams {
    // Common parameters
    double sigma = 1.0;     // Standard deviation for Gaussian filters
    int kernelSize = 3;     // Kernel size (must be odd)
    double strength = 1.0;  // Filter strength/intensity

    // Specific parameters
    double threshold1 = 100.0;  // Lower threshold (Canny)
    double threshold2 = 200.0;  // Upper threshold (Canny)
    double angle = 0.0;         // Motion blur angle
    int distance = 5;           // Motion blur distance

    // Bilateral filter
    double sigmaColor = 75.0;  // Color sigma
    double sigmaSpace = 75.0;  // Space sigma

    // Non-local means
    double h = 10.0;             // Filter strength
    int templateWindowSize = 7;  // Template patch size
    int searchWindowSize = 21;   // Search window size

    // Morphological operations
    StructuringElement structElement = StructuringElement::RECTANGLE;
    std::vector<std::vector<int>> customKernel;

    // Frequency domain
    double cutoffFreq = 0.5;  // Cutoff frequency (0-1)
    double bandwidth = 0.1;   // Bandwidth for band filters

    // Artistic filter parameters
    int levels = 4;     // Posterize levels
    int blockSize = 8;  // Pixelate block size
    int radius = 3;     // Oil paint radius

    // Custom parameters
    std::unordered_map<std::string, double> custom;

    /**
     * @brief Convert to modern variant-based parameter map
     * @return FilterParamMap with all parameters
     */
    [[nodiscard]] FilterParamMap toParamMap() const;

    /**
     * @brief Create from modern variant-based parameter map
     * @param params Parameter map
     * @return FilterParams instance
     */
    [[nodiscard]] static FilterParams fromParamMap(
        const FilterParamMap& params);
};

/**
 * @brief Advanced image filter processor
 */
class ImageFilter {
public:
    ImageFilter() = default;
    virtual ~ImageFilter() = default;

    /**
     * @brief Apply a filter to an image
     * @param input Input image blob
     * @param filterType Type of filter to apply
     * @param params Filter parameters
     * @return Filtered image blob
     */
    [[nodiscard]] virtual blob applyFilter(
        const blob& input, FilterType filterType,
        const FilterParams& params = {}) const;

    /**
     * @brief Apply a filter using modern variant-based parameters
     * @param input Input image blob
     * @param filterType Type of filter to apply
     * @param params Modern parameter map
     * @return Filtered image blob
     */
    [[nodiscard]] virtual blob applyFilterV2(
        const blob& input, FilterType filterType,
        const FilterParamMap& params) const;

    /**
     * @brief Compile-time filter selection (C++20)
     * @tparam Filter FilterType enum value
     * @param input Input image blob
     * @param params Filter parameters
     * @return Filtered image blob
     * @note Uses compile-time constant for potential optimization
     */
    template <FilterType Filter>
    [[nodiscard]] blob applyFilterCompileTime(
        const blob& input, const FilterParams& params = {}) const {
        // Compile-time constant allows compiler to optimize the call
        return applyFilter(input, Filter, params);
    }

    /**
     * @brief Apply a custom convolution kernel
     * @param input Input image blob
     * @param kernel Convolution kernel
     * @param normalize Whether to normalize the kernel
     * @return Filtered image blob
     */
    virtual blob applyCustomKernel(
        const blob& input, const std::vector<std::vector<double>>& kernel,
        bool normalize = true) const;

    /**
     * @brief Apply separable filter (more efficient for separable kernels)
     * @param input Input image blob
     * @param kernelX Horizontal kernel
     * @param kernelY Vertical kernel
     * @return Filtered image blob
     */
    virtual blob applySeparableFilter(const blob& input,
                                      const std::vector<double>& kernelX,
                                      const std::vector<double>& kernelY) const;

    /**
     * @brief Apply morphological operation
     * @param input Input image blob
     * @param operation Morphological operation type
     * @param structElement Structuring element
     * @param size Element size
     * @return Processed image blob
     */
    virtual blob applyMorphological(const blob& input, FilterType operation,
                                    StructuringElement structElement,
                                    int size = 3) const;

    /**
     * @brief Apply frequency domain filter
     * @param input Input image blob
     * @param filterType Frequency filter type
     * @param params Filter parameters
     * @return Filtered image blob
     */
    virtual blob applyFrequencyFilter(const blob& input, FilterType filterType,
                                      const FilterParams& params = {}) const;

    /**
     * @brief Apply adaptive filter based on local image statistics
     * @param input Input image blob
     * @param filterType Base filter type
     * @param windowSize Local analysis window size
     * @param params Filter parameters
     * @return Filtered image blob
     */
    virtual blob applyAdaptiveFilter(const blob& input, FilterType filterType,
                                     int windowSize = 7,
                                     const FilterParams& params = {}) const;

    /**
     * @brief Apply multi-scale filter (pyramid processing)
     * @param input Input image blob
     * @param filterType Filter to apply at each scale
     * @param scales Number of pyramid levels
     * @param params Filter parameters
     * @return Filtered image blob
     */
    virtual blob applyMultiScaleFilter(const blob& input, FilterType filterType,
                                       int scales = 3,
                                       const FilterParams& params = {}) const;

    /**
     * @brief Combine multiple filters in sequence
     * @param input Input image blob
     * @param filters Vector of filter types to apply
     * @param params Vector of parameters for each filter
     * @return Filtered image blob
     */
    virtual blob applyFilterChain(
        const blob& input, const std::vector<FilterType>& filters,
        const std::vector<FilterParams>& params = {}) const;

    /**
     * @brief Get predefined kernel for common filters
     * @param filterType Filter type
     * @param size Kernel size
     * @return Convolution kernel
     */
    static std::vector<std::vector<double>> getPredefinedKernel(
        FilterType filterType, int size = 3);

    /**
     * @brief Create Gaussian kernel
     * @param size Kernel size (must be odd)
     * @param sigma Standard deviation
     * @return Gaussian kernel
     */
    static std::vector<std::vector<double>> createGaussianKernel(int size,
                                                                 double sigma);

    /**
     * @brief Create motion blur kernel
     * @param size Kernel size
     * @param angle Motion angle in degrees
     * @param distance Motion distance
     * @return Motion blur kernel
     */
    static std::vector<std::vector<double>> createMotionBlurKernel(
        int size, double angle, int distance);

    /**
     * @brief Create structuring element for morphological operations
     * @param shape Element shape
     * @param size Element size
     * @return Structuring element as binary mask
     */
    static std::vector<std::vector<int>> createStructuringElement(
        StructuringElement shape, int size);

protected:
    /**
     * @brief Apply convolution operation
     * @param input Input image data
     * @param kernel Convolution kernel
     * @param width Image width
     * @param height Image height
     * @param channels Number of channels
     * @return Convolved image data
     */
    virtual std::vector<std::byte> convolve(
        const std::vector<std::byte>& input,
        const std::vector<std::vector<double>>& kernel, int width, int height,
        int channels) const;

    /**
     * @brief Apply median filter
     * @param input Input image data
     * @param kernelSize Filter kernel size
     * @param width Image width
     * @param height Image height
     * @param channels Number of channels
     * @return Filtered image data
     */
    virtual std::vector<std::byte> medianFilter(
        const std::vector<std::byte>& input, int kernelSize, int width,
        int height, int channels) const;

    /**
     * @brief Apply bilateral filter
     * @param input Input image data
     * @param params Filter parameters
     * @param width Image width
     * @param height Image height
     * @param channels Number of channels
     * @return Filtered image data
     */
    virtual std::vector<std::byte> bilateralFilter(
        const std::vector<std::byte>& input, const FilterParams& params,
        int width, int height, int channels) const;

    /**
     * @brief Apply min/max filter for morphological operations
     */
    std::vector<std::byte> minMaxFilter(const std::vector<std::byte>& input,
                                        int kernelSize, int width, int height,
                                        int channels, bool isMin) const;

    /**
     * @brief Apply oil paint artistic effect
     */
    std::vector<std::byte> oilPaintFilter(const std::vector<std::byte>& input,
                                          int radius, int width, int height,
                                          int channels) const;

    /**
     * @brief Apply pencil sketch artistic effect
     */
    std::vector<std::byte> pencilSketchFilter(
        const std::vector<std::byte>& input, int width, int height,
        int channels) const;

    /**
     * @brief Apply cartoon artistic effect
     */
    std::vector<std::byte> cartoonFilter(const std::vector<std::byte>& input,
                                         int width, int height,
                                         int channels) const;

    /**
     * @brief Apply posterize effect (reduce color levels)
     */
    std::vector<std::byte> posterizeFilter(const std::vector<std::byte>& input,
                                           int levels) const;

    /**
     * @brief Apply pixelate effect
     */
    std::vector<std::byte> pixelateFilter(const std::vector<std::byte>& input,
                                          int blockSize, int width, int height,
                                          int channels) const;

    /**
     * @brief Apply vignette effect
     */
    std::vector<std::byte> vignetteFilter(const std::vector<std::byte>& input,
                                          int width, int height, int channels,
                                          double strength) const;

    /**
     * @brief Apply sepia tone effect
     */
    std::vector<std::byte> sepiaFilter(const std::vector<std::byte>& input,
                                       int channels) const;
};

/**
 * @brief Factory function to create optimal filter processor
 * @param useGPU Whether to use GPU acceleration if available
 * @return Unique pointer to filter processor
 */
std::unique_ptr<ImageFilter> createOptimalFilter(bool useGPU = false);

}  // namespace atom::image

#endif  // ATOM_IMAGE_FILTERS_HPP
