// debayer.h
#pragma once

#include "exception.h"
#include "frame_processor.h"
#include "ser_format.h"

#include <memory>
#include <opencv2/core.hpp>
#include <string>
#include <vector>

namespace serastro {

/**
 * @enum DebayerAlgorithm
 * @brief Available debayering algorithms
 */
enum class DebayerAlgorithm {
    Nearest,     ///< Nearest neighbor (fastest, lowest quality)
    Bilinear,    ///< Bilinear interpolation (OpenCV default)
    VNG,         ///< Variable Number of Gradients
    EdgeAware,   ///< Edge-aware (EA)
    AHD,         ///< Adaptive Homogeneity-Directed
    DCB,         ///< DCB algorithm
    AMAZE,       ///< AMaZE algorithm (high quality)
    SuperPixel,  ///< Super-pixel (2x2 binning, fast)
    HalfSize,    ///< Half-size output (fastest)
    IGV,         ///< Improved Gradient-based
    LMMSE        ///< Linear Minimum Mean Square Error
};

/**
 * @struct DebayerParameters
 * @brief Parameters for debayering
 */
struct DebayerParameters {
    DebayerAlgorithm algorithm = DebayerAlgorithm::Bilinear;
    SERColorID bayerPattern = SERColorID::BayerRGGB;

    // Color correction
    bool applyColorCorrection = false;
    double redMultiplier = 1.0;
    double greenMultiplier = 1.0;
    double blueMultiplier = 1.0;

    // White balance
    bool autoWhiteBalance = false;
    double colorTemperature = 6500.0;  ///< Kelvin

    // Noise reduction during debayer
    bool applyNoiseReduction = false;
    double noiseThreshold = 0.1;

    // Output format
    bool outputBGR = true;     ///< True for BGR, false for RGB
    bool output16bit = false;  ///< Output 16-bit instead of 8-bit

    // Advanced options
    int vngThreshold = 0;     ///< VNG gradient threshold
    int dcbIterations = 1;    ///< DCB enhancement iterations
    bool dcbEnhance = false;  ///< DCB color enhancement
};

/**
 * @struct WhiteBalanceCoeffs
 * @brief White balance coefficients
 */
struct WhiteBalanceCoeffs {
    double r = 1.0;
    double g = 1.0;
    double b = 1.0;

    WhiteBalanceCoeffs() = default;
    WhiteBalanceCoeffs(double r_, double g_, double b_) : r(r_), g(g_), b(b_) {}
};

/**
 * @class DebayerProcessor
 * @brief Advanced debayering processor for Bayer pattern images
 *
 * Supports multiple debayering algorithms with various quality/speed
 * tradeoffs, plus color correction and white balance options.
 */
class DebayerProcessor : public CustomizableProcessor {
public:
    /**
     * @brief Default constructor
     */
    DebayerProcessor();

    /**
     * @brief Construct with parameters
     * @param params Debayer parameters
     */
    explicit DebayerProcessor(const DebayerParameters& params);

    /**
     * @brief Construct with Bayer pattern
     * @param pattern Bayer pattern
     * @param algorithm Debayer algorithm
     */
    DebayerProcessor(SERColorID pattern,
                     DebayerAlgorithm algorithm = DebayerAlgorithm::Bilinear);

    /**
     * @brief Process a single frame
     * @param frame Input Bayer frame
     * @return Debayered color frame
     */
    cv::Mat process(const cv::Mat& frame) override;

    /**
     * @brief Get processor name
     */
    std::string getName() const override { return "DebayerProcessor"; }

    // CustomizableProcessor interface
    void setParameter(const std::string& name, double value) override;
    double getParameter(const std::string& name) const override;
    std::vector<std::string> getParameterNames() const override;
    bool hasParameter(const std::string& name) const override;

    /**
     * @brief Set debayer parameters
     * @param params New parameters
     */
    void setDebayerParameters(const DebayerParameters& params);

    /**
     * @brief Get current parameters
     * @return Current parameters
     */
    const DebayerParameters& getDebayerParameters() const { return params_; }

    /**
     * @brief Set Bayer pattern
     * @param pattern Bayer pattern
     */
    void setBayerPattern(SERColorID pattern);

    /**
     * @brief Set algorithm
     * @param algorithm Debayer algorithm
     */
    void setAlgorithm(DebayerAlgorithm algorithm);

    /**
     * @brief Calculate auto white balance from image
     * @param frame Input frame
     * @return White balance coefficients
     */
    WhiteBalanceCoeffs calculateAutoWhiteBalance(const cv::Mat& frame) const;

    /**
     * @brief Apply white balance to frame
     * @param frame Input frame
     * @param coeffs White balance coefficients
     * @return White-balanced frame
     */
    cv::Mat applyWhiteBalance(const cv::Mat& frame,
                              const WhiteBalanceCoeffs& coeffs) const;

    /**
     * @brief Get white balance coefficients for color temperature
     * @param kelvin Color temperature in Kelvin
     * @return White balance coefficients
     */
    static WhiteBalanceCoeffs getWhiteBalanceForTemperature(double kelvin);

    /**
     * @brief Check if pattern is a Bayer pattern
     * @param colorID Color ID
     * @return True if Bayer pattern
     */
    static bool isBayerPattern(SERColorID colorID);

    /**
     * @brief Get OpenCV Bayer code for pattern
     * @param pattern Bayer pattern
     * @return OpenCV color conversion code
     */
    static int getOpenCVBayerCode(SERColorID pattern);

private:
    DebayerParameters params_;

    /**
     * @brief Debayer using OpenCV
     */
    cv::Mat debayerOpenCV(const cv::Mat& frame) const;

    /**
     * @brief Debayer using VNG algorithm
     */
    cv::Mat debayerVNG(const cv::Mat& frame) const;

    /**
     * @brief Debayer using AHD algorithm
     */
    cv::Mat debayerAHD(const cv::Mat& frame) const;

    /**
     * @brief Debayer using SuperPixel (2x2 binning)
     */
    cv::Mat debayerSuperPixel(const cv::Mat& frame) const;

    /**
     * @brief Debayer using half-size method
     */
    cv::Mat debayerHalfSize(const cv::Mat& frame) const;

    /**
     * @brief Apply color matrix correction
     */
    cv::Mat applyColorMatrix(const cv::Mat& frame) const;

    /**
     * @brief Apply noise reduction during debayer
     */
    cv::Mat applyNoiseReduction(const cv::Mat& frame) const;
};

/**
 * @brief Get algorithm name
 * @param algorithm Algorithm enum
 * @return Algorithm name string
 */
std::string debayerAlgorithmToString(DebayerAlgorithm algorithm);

/**
 * @brief Parse algorithm from string
 * @param name Algorithm name
 * @return Algorithm enum
 */
DebayerAlgorithm debayerAlgorithmFromString(const std::string& name);

/**
 * @brief Get Bayer pattern name
 * @param pattern Bayer pattern
 * @return Pattern name
 */
std::string bayerPatternToString(SERColorID pattern);

/**
 * @brief Detect Bayer pattern from image
 * @param image Input image
 * @return Detected pattern or Mono if not detected
 */
SERColorID detectBayerPattern(const cv::Mat& image);

}  // namespace serastro
