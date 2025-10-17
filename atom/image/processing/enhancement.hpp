#ifndef ATOM_IMAGE_ENHANCEMENT_HPP
#define ATOM_IMAGE_ENHANCEMENT_HPP

/**
 * @file enhancement.hpp
 * @brief Advanced image enhancement operations
 *
 * This module provides comprehensive image enhancement capabilities including
 * histogram operations, color corrections, contrast enhancement, and
 * advanced tone mapping algorithms.
 *
 * @author Atom Framework Team
 * @date 2025
 * @version 1.0.0
 */

#include <array>
#include <functional>
#include <memory>
#include <vector>
#include "../core/image_blob.hpp"

namespace atom::image {

/**
 * @brief Color space types for enhancement operations
 */
enum class ColorSpace {
    RGB,  // Red, Green, Blue
    HSV,  // Hue, Saturation, Value
    HSL,  // Hue, Saturation, Lightness
    LAB,  // L*a*b* color space
    YUV,  // Luminance, Chrominance
    XYZ,  // CIE XYZ
    GRAY  // Grayscale
};

/**
 * @brief Histogram equalization methods
 */
enum class HistogramMethod {
    GLOBAL,      // Global histogram equalization
    ADAPTIVE,    // Adaptive histogram equalization (AHE)
    CLAHE,       // Contrast Limited AHE
    LOCAL,       // Local histogram equalization
    MULTI_SCALE  // Multi-scale histogram equalization
};

/**
 * @brief Tone mapping operators
 */
enum class ToneMappingOperator {
    REINHARD,    // Reinhard tone mapping
    DRAGO,       // Drago tone mapping
    MANTIUK,     // Mantiuk tone mapping
    FATTAL,      // Fattal tone mapping
    DURAND,      // Durand tone mapping
    GAMMA,       // Simple gamma correction
    LINEAR,      // Linear tone mapping
    LOGARITHMIC  // Logarithmic tone mapping
};

/**
 * @brief Color correction methods
 */
enum class ColorCorrectionMethod {
    WHITE_BALANCE,     // White balance correction
    COLOR_CAST,        // Color cast removal
    GAMMA_CORRECTION,  // Gamma correction
    CURVES,            // Tone curves adjustment
    LEVELS,            // Levels adjustment
    COLOR_GRADING,     // Professional color grading
    AUTO_LEVELS,       // Automatic levels adjustment
    AUTO_COLOR         // Automatic color correction
};

/**
 * @brief Enhancement parameters container
 */
struct EnhancementParams {
    // Histogram parameters
    double clipLimit = 2.0;  // CLAHE clip limit
    int tileGridSize = 8;    // CLAHE tile grid size

    // Tone mapping parameters
    double gamma = 2.2;            // Gamma value
    double exposure = 0.0;         // Exposure adjustment
    double saturation = 1.0;       // Saturation multiplier
    double intensity = 1.0;        // Intensity multiplier
    double lightAdaptation = 1.0;  // Light adaptation
    double colorAdaptation = 0.0;  // Color adaptation

    // Color correction parameters
    double temperature = 6500.0;  // Color temperature (K)
    double tint = 0.0;            // Tint adjustment
    std::array<double, 3> whitePoint = {1.0, 1.0, 1.0};  // White point
    std::array<double, 3> blackPoint = {0.0, 0.0, 0.0};  // Black point

    // Contrast and brightness
    double contrast = 1.0;    // Contrast multiplier
    double brightness = 0.0;  // Brightness offset
    double highlights = 0.0;  // Highlights adjustment
    double shadows = 0.0;     // Shadows adjustment
    double midtones = 0.0;    // Midtones adjustment

    // Advanced parameters
    double vibrance = 0.0;          // Vibrance adjustment
    double clarity = 0.0;           // Clarity/structure enhancement
    double dehaze = 0.0;            // Dehaze strength
    bool preserveLuminance = true;  // Preserve luminance during color ops
};

/**
 * @brief Advanced image enhancement processor
 */
class ImageEnhancement {
public:
    ImageEnhancement() = default;
    virtual ~ImageEnhancement() = default;

    /**
     * @brief Apply histogram equalization
     * @param input Input image blob
     * @param method Histogram equalization method
     * @param params Enhancement parameters
     * @return Enhanced image blob
     */
    virtual blob equalizeHistogram(
        const blob& input, HistogramMethod method = HistogramMethod::CLAHE,
        const EnhancementParams& params = {}) const;

    /**
     * @brief Apply tone mapping for HDR images
     * @param input Input HDR image blob
     * @param operator_ Tone mapping operator
     * @param params Tone mapping parameters
     * @return Tone-mapped LDR image blob
     */
    virtual blob toneMapping(
        const blob& input,
        ToneMappingOperator operator_ = ToneMappingOperator::REINHARD,
        const EnhancementParams& params = {}) const;

    /**
     * @brief Apply color correction
     * @param input Input image blob
     * @param method Color correction method
     * @param params Correction parameters
     * @return Color-corrected image blob
     */
    virtual blob colorCorrection(
        const blob& input,
        ColorCorrectionMethod method = ColorCorrectionMethod::AUTO_COLOR,
        const EnhancementParams& params = {}) const;

    /**
     * @brief Adjust brightness and contrast
     * @param input Input image blob
     * @param brightness Brightness adjustment (-100 to 100)
     * @param contrast Contrast adjustment (0.0 to 3.0)
     * @param preserveDetails Whether to preserve fine details
     * @return Adjusted image blob
     */
    virtual blob adjustBrightnessContrast(const blob& input,
                                          double brightness = 0.0,
                                          double contrast = 1.0,
                                          bool preserveDetails = true) const;

    /**
     * @brief Apply gamma correction
     * @param input Input image blob
     * @param gamma Gamma value (0.1 to 3.0)
     * @param colorSpace Color space for gamma correction
     * @return Gamma-corrected image blob
     */
    virtual blob gammaCorrection(const blob& input, double gamma = 2.2,
                                 ColorSpace colorSpace = ColorSpace::RGB) const;

    /**
     * @brief Enhance image sharpness
     * @param input Input image blob
     * @param strength Sharpening strength (0.0 to 2.0)
     * @param radius Sharpening radius
     * @param threshold Sharpening threshold
     * @param method Sharpening method ("unsharp_mask", "high_pass", "clarity")
     * @return Sharpened image blob
     */
    virtual blob sharpen(const blob& input, double strength = 1.0,
                         double radius = 1.0, double threshold = 0.0,
                         const std::string& method = "unsharp_mask") const;

    /**
     * @brief Reduce image noise
     * @param input Input image blob
     * @param strength Denoising strength (0.0 to 1.0)
     * @param method Denoising method ("bilateral", "nlm", "bm3d", "dct")
     * @param preserveEdges Whether to preserve edges
     * @return Denoised image blob
     */
    virtual blob denoise(const blob& input, double strength = 0.5,
                         const std::string& method = "bilateral",
                         bool preserveEdges = true) const;

    /**
     * @brief Apply shadow/highlight adjustment
     * @param input Input image blob
     * @param shadows Shadow adjustment (-100 to 100)
     * @param highlights Highlight adjustment (-100 to 100)
     * @param radius Adjustment radius
     * @return Adjusted image blob
     */
    virtual blob shadowHighlight(const blob& input, double shadows = 0.0,
                                 double highlights = 0.0,
                                 double radius = 30.0) const;

    /**
     * @brief Apply vibrance and saturation adjustment
     * @param input Input image blob
     * @param vibrance Vibrance adjustment (-100 to 100)
     * @param saturation Saturation adjustment (-100 to 100)
     * @return Adjusted image blob
     */
    virtual blob vibranceSaturation(const blob& input, double vibrance = 0.0,
                                    double saturation = 0.0) const;

    /**
     * @brief Apply clarity/structure enhancement
     * @param input Input image blob
     * @param clarity Clarity amount (-100 to 100)
     * @param radius Clarity radius
     * @param preserveSkin Whether to preserve skin tones
     * @return Enhanced image blob
     */
    virtual blob clarity(const blob& input, double clarity = 0.0,
                         double radius = 20.0, bool preserveSkin = true) const;

    /**
     * @brief Apply dehaze filter
     * @param input Input image blob
     * @param strength Dehaze strength (0.0 to 1.0)
     * @param preserveColors Whether to preserve color balance
     * @return Dehazed image blob
     */
    virtual blob dehaze(const blob& input, double strength = 0.5,
                        bool preserveColors = true) const;

    /**
     * @brief Apply automatic enhancement
     * @param input Input image blob
     * @param mode Enhancement mode ("auto", "portrait", "landscape", "night")
     * @param strength Enhancement strength (0.0 to 1.0)
     * @return Auto-enhanced image blob
     */
    virtual blob autoEnhance(const blob& input,
                             const std::string& mode = "auto",
                             double strength = 0.8) const;

    /**
     * @brief Convert between color spaces
     * @param input Input image blob
     * @param fromSpace Source color space
     * @param toSpace Target color space
     * @return Converted image blob
     */
    virtual blob convertColorSpace(const blob& input, ColorSpace fromSpace,
                                   ColorSpace toSpace) const;

    /**
     * @brief Calculate image histogram
     * @param input Input image blob
     * @param channel Channel index (-1 for all channels)
     * @param bins Number of histogram bins
     * @return Histogram data
     */
    virtual std::vector<std::vector<double>> calculateHistogram(
        const blob& input, int channel = -1, int bins = 256) const;

    /**
     * @brief Apply tone curve adjustment
     * @param input Input image blob
     * @param curve Tone curve points (input -> output mapping)
     * @param channel Channel to apply curve (-1 for all)
     * @return Curve-adjusted image blob
     */
    virtual blob applyCurve(const blob& input,
                            const std::vector<std::pair<double, double>>& curve,
                            int channel = -1) const;

    /**
     * @brief Apply levels adjustment
     * @param input Input image blob
     * @param blackPoint Black point (0-255)
     * @param whitePoint White point (0-255)
     * @param gamma Gamma value
     * @param outputBlack Output black point (0-255)
     * @param outputWhite Output white point (0-255)
     * @return Levels-adjusted image blob
     */
    virtual blob adjustLevels(const blob& input, double blackPoint = 0.0,
                              double whitePoint = 255.0, double gamma = 1.0,
                              double outputBlack = 0.0,
                              double outputWhite = 255.0) const;

protected:
    /**
     * @brief Apply enhancement in specific color space
     * @param input Input image data
     * @param width Image width
     * @param height Image height
     * @param channels Number of channels
     * @param colorSpace Target color space
     * @param enhanceFunction Enhancement function to apply
     * @return Enhanced image data
     */
    virtual std::vector<std::byte> enhanceInColorSpace(
        const std::vector<std::byte>& input, int width, int height,
        int channels, ColorSpace colorSpace,
        std::function<std::vector<std::byte>(const std::vector<std::byte>&, int,
                                             int, int)>
            enhanceFunction) const;

    /**
     * @brief Convert RGB to specified color space
     * @param rgb RGB values (0-255)
     * @param colorSpace Target color space
     * @return Converted values
     */
    virtual std::array<double, 3> rgbToColorSpace(
        const std::array<uint8_t, 3>& rgb, ColorSpace colorSpace) const;

    /**
     * @brief Convert from specified color space to RGB
     * @param values Color space values
     * @param colorSpace Source color space
     * @return RGB values (0-255)
     */
    virtual std::array<uint8_t, 3> colorSpaceToRgb(
        const std::array<double, 3>& values, ColorSpace colorSpace) const;

    /**
     * @brief Helper for HSL to RGB conversion
     */
    double hueToRgb(double p, double q, double t) const;
};

/**
 * @brief Factory function to create optimal enhancement processor
 * @param useGPU Whether to use GPU acceleration if available
 * @return Unique pointer to enhancement processor
 */
std::unique_ptr<ImageEnhancement> createOptimalEnhancement(bool useGPU = false);

}  // namespace atom::image

#endif  // ATOM_IMAGE_ENHANCEMENT_HPP
