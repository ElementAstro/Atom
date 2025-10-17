#ifndef ATOM_IMAGE_PROCESSOR_HPP
#define ATOM_IMAGE_PROCESSOR_HPP

#include <concepts>
#include <functional>
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>

#include "../core/image_blob.hpp"
#include "../io/format_detector.hpp"

// Forward declare error macros for header
#ifndef THROW_RUNTIME_ERROR
#define THROW_RUNTIME_ERROR(msg) throw std::runtime_error(msg)
#endif

namespace atom::image {

/**
 * @enum FilterType
 * @brief Types of image filters available
 */
enum class FilterType {
    BLUR,
    GAUSSIAN_BLUR,
    SHARPEN,
    EDGE_DETECT,
    EMBOSS,
    MEDIAN,
    BILATERAL,
    CUSTOM
};

/**
 * @struct ProcessingOptions
 * @brief Configuration options for image processing operations
 */
struct ProcessingOptions {
    bool preserveAspectRatio = true;
    bool useMultithreading = true;
    int quality = 95;  // For lossy formats
    bool enableSIMD = true;
    size_t maxMemoryUsage = 1024 * 1024 * 1024;  // 1GB default
};

/**
 * @class ImageProcessor
 * @brief High-performance image processing pipeline with format conversion and filtering
 */
class ImageProcessor {
public:
    /**
     * @brief Construct a new ImageProcessor
     * @param options Processing configuration options
     */
    explicit ImageProcessor(const ProcessingOptions& options = ProcessingOptions{});

    /**
     * @brief Destructor
     */
    ~ImageProcessor() = default;

    // Disable copy operations for performance
    ImageProcessor(const ImageProcessor&) = delete;
    ImageProcessor& operator=(const ImageProcessor&) = delete;

    // Enable move operations
    ImageProcessor(ImageProcessor&&) noexcept = default;
    ImageProcessor& operator=(ImageProcessor&&) noexcept = default;

    /**
     * @brief Convert image format
     * @param input Input image blob
     * @param targetFormat Target format
     * @return Converted image blob
     */
    [[nodiscard]] blob convertFormat(const blob& input, ImageFormat targetFormat) const;

    /**
     * @brief Resize image with various algorithms
     * @param input Input image blob
     * @param newWidth Target width
     * @param newHeight Target height
     * @param algorithm Resize algorithm ("nearest", "linear", "cubic", "lanczos")
     * @return Resized image blob
     */
    [[nodiscard]] blob resize(const blob& input, int newWidth, int newHeight,
                             const std::string& algorithm = "cubic") const;

    /**
     * @brief Rotate image by specified angle
     * @param input Input image blob
     * @param angle Rotation angle in degrees
     * @param expandCanvas Whether to expand canvas to fit rotated image
     * @return Rotated image blob
     */
    [[nodiscard]] blob rotate(const blob& input, double angle, bool expandCanvas = true) const;

    /**
     * @brief Crop image to specified rectangle
     * @param input Input image blob
     * @param x X coordinate of top-left corner
     * @param y Y coordinate of top-left corner
     * @param width Width of crop area
     * @param height Height of crop area
     * @return Cropped image blob
     */
    [[nodiscard]] blob crop(const blob& input, int x, int y, int width, int height) const;

    /**
     * @brief Apply filter to image
     * @param input Input image blob
     * @param filterType Type of filter to apply
     * @param parameters Filter-specific parameters
     * @return Filtered image blob
     */
    [[nodiscard]] blob applyFilter(const blob& input, FilterType filterType,
                                  const std::unordered_map<std::string, double>& parameters = {}) const;

    /**
     * @brief Apply custom convolution kernel
     * @param input Input image blob
     * @param kernel Convolution kernel (must be square matrix)
     * @param kernelSize Size of kernel (e.g., 3 for 3x3)
     * @return Filtered image blob
     */
    [[nodiscard]] blob applyCustomKernel(const blob& input,
                                        const std::vector<float>& kernel,
                                        int kernelSize) const;

    /**
     * @brief Adjust image brightness and contrast
     * @param input Input image blob
     * @param brightness Brightness adjustment (-100 to 100)
     * @param contrast Contrast adjustment (-100 to 100)
     * @return Adjusted image blob
     */
    [[nodiscard]] blob adjustBrightnessContrast(const blob& input,
                                               double brightness,
                                               double contrast) const;

    /**
     * @brief Apply gamma correction
     * @param input Input image blob
     * @param gamma Gamma value (typically 0.1 to 3.0)
     * @return Gamma-corrected image blob
     */
    [[nodiscard]] blob adjustGamma(const blob& input, double gamma) const;

    /**
     * @brief Enhance image using histogram equalization
     * @param input Input image blob
     * @param adaptive Whether to use adaptive histogram equalization
     * @return Enhanced image blob
     */
    [[nodiscard]] blob enhanceHistogram(const blob& input, bool adaptive = false) const;

    /**
     * @brief Detect edges in image
     * @param input Input image blob
     * @param algorithm Edge detection algorithm ("sobel", "canny", "laplacian")
     * @param threshold Threshold values for edge detection
     * @return Edge-detected image blob
     */
    [[nodiscard]] blob detectEdges(const blob& input,
                                  const std::string& algorithm = "canny",
                                  const std::vector<double>& threshold = {50.0, 150.0}) const;

    /**
     * @brief Remove noise from image
     * @param input Input image blob
     * @param algorithm Denoising algorithm ("gaussian", "median", "bilateral", "nlmeans")
     * @param strength Denoising strength (0.0 to 1.0)
     * @return Denoised image blob
     */
    [[nodiscard]] blob denoise(const blob& input,
                              const std::string& algorithm = "bilateral",
                              double strength = 0.5) const;

    /**
     * @brief Process batch of images with the same operation
     * @param inputs Vector of input image blobs
     * @param operation Processing function to apply
     * @return Vector of processed image blobs
     */
    [[nodiscard]] std::vector<blob> processBatch(
        const std::vector<blob>& inputs,
        std::function<blob(const blob&)> operation) const;

    /**
     * @brief Get image statistics
     * @param input Input image blob
     * @return Map of statistics (mean, std, min, max, etc.)
     */
    [[nodiscard]] std::unordered_map<std::string, double> getStatistics(const blob& input) const;

    /**
     * @brief Calculate image quality metrics
     * @param input Input image blob
     * @param reference Optional reference image for comparison metrics
     * @return Map of quality metrics
     */
    [[nodiscard]] std::unordered_map<std::string, double> calculateQualityMetrics(
        const blob& input,
        const blob* reference = nullptr) const;

    /**
     * @brief Set processing options
     * @param options New processing options
     */
    void setOptions(const ProcessingOptions& options);

    /**
     * @brief Get current processing options
     * @return Current processing options
     */
    [[nodiscard]] const ProcessingOptions& getOptions() const noexcept;

private:
    ProcessingOptions m_options;

    // Internal helper methods
    [[nodiscard]] blob applyGaussianBlur(const blob& input, double sigma) const;
    [[nodiscard]] blob applySharpen(const blob& input, double strength) const;
    [[nodiscard]] blob applyMedianFilter(const blob& input, int kernelSize) const;

    // Format-specific converters
    [[nodiscard]] blob convertToJPEG(const blob& input) const;
    [[nodiscard]] blob convertToPNG(const blob& input) const;
    [[nodiscard]] blob convertToTIFF(const blob& input) const;

    // Validation helpers
    void validateImageDimensions(int width, int height) const;
    void validateKernel(const std::vector<float>& kernel, int kernelSize) const;
    void validateCropParameters(const blob& input, int x, int y, int width, int height) const;
};

/**
 * @brief Factory function to create ImageProcessor with optimal settings
 * @param useGPU Whether to enable GPU acceleration if available
 * @return Configured ImageProcessor instance
 */
[[nodiscard]] std::unique_ptr<ImageProcessor> createOptimalProcessor(bool useGPU = false);

} // namespace atom::image

#endif // ATOM_IMAGE_PROCESSOR_HPP
