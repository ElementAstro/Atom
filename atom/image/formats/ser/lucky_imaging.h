// lucky_imaging.h
#pragma once

#include "exception.h"
#include "frame_processor.h"
#include "quality.h"
#include "registration.h"
#include "stacking.h"

#include <filesystem>
#include <functional>
#include <memory>
#include <opencv2/core.hpp>
#include <string>
#include <vector>

namespace serastro {

/**
 * @enum SelectionMethod
 * @brief Methods for frame selection in lucky imaging
 */
enum class SelectionMethod {
    Percentage,  ///< Select top N percent of frames
    Count,       ///< Select top N frames
    Threshold,   ///< Select frames above quality threshold
    Adaptive     ///< Adaptive selection based on quality distribution
};

/**
 * @struct LuckyImagingParams
 * @brief Parameters for lucky imaging processing
 */
struct LuckyImagingParams {
    // Frame selection
    SelectionMethod selectionMethod = SelectionMethod::Percentage;
    double selectionPercentage = 10.0;  ///< Top percentage to keep
    size_t selectionCount = 100;        ///< Top N frames to keep
    double qualityThreshold = 0.5;      ///< Minimum quality threshold

    // Quality assessment
    QualityParameters qualityParams;

    // Registration
    RegistrationParameters registrationParams;
    bool enableRegistration = true;

    // Stacking
    StackingParameters stackingParams;

    // ROI-based processing
    bool useROI = false;
    cv::Rect roi;               ///< Region of interest
    bool autoDetectROI = true;  ///< Auto-detect best region

    // Multi-point alignment (for planetary)
    bool multiPointAlignment = false;
    int alignmentPoints = 4;  ///< Number of alignment points

    // Wavelet sharpening
    bool applyWaveletSharpening = false;
    double waveletStrength = 1.0;
    int waveletLayers = 3;

    // Deconvolution
    bool applyDeconvolution = false;
    double psfSigma = 1.5;  ///< PSF Gaussian sigma
    int deconvIterations = 10;
};

/**
 * @struct LuckyImagingResult
 * @brief Result of lucky imaging processing
 */
struct LuckyImagingResult {
    cv::Mat stackedImage;    ///< Final stacked image
    cv::Mat referenceFrame;  ///< Reference frame used

    size_t totalFrames = 0;     ///< Total input frames
    size_t selectedFrames = 0;  ///< Frames used in stack
    size_t rejectedFrames = 0;  ///< Frames rejected

    std::vector<size_t> selectedIndices;  ///< Indices of selected frames
    std::vector<double> qualityScores;    ///< Quality scores of all frames

    double averageQuality = 0.0;        ///< Average quality of selected
    double bestQuality = 0.0;           ///< Best frame quality
    double worstSelectedQuality = 0.0;  ///< Worst selected frame quality

    double processingTimeMs = 0.0;    ///< Total processing time
    double qualityTimeMs = 0.0;       ///< Time for quality assessment
    double registrationTimeMs = 0.0;  ///< Time for registration
    double stackingTimeMs = 0.0;      ///< Time for stacking
};

/**
 * @class LuckyImaging
 * @brief Lucky imaging processor for planetary/lunar imaging
 *
 * Implements the lucky imaging technique:
 * 1. Assess quality of all frames
 * 2. Select best frames based on quality
 * 3. Align selected frames
 * 4. Stack aligned frames
 * 5. Optional post-processing (sharpening, deconvolution)
 */
class LuckyImaging {
public:
    using ProgressCallback = std::function<void(float, const std::string&)>;

    /**
     * @brief Default constructor
     */
    LuckyImaging();

    /**
     * @brief Construct with parameters
     * @param params Lucky imaging parameters
     */
    explicit LuckyImaging(const LuckyImagingParams& params);

    /**
     * @brief Process frames from memory
     * @param frames Vector of frames
     * @param progressCallback Progress callback
     * @return Lucky imaging result
     */
    LuckyImagingResult process(const std::vector<cv::Mat>& frames,
                               ProgressCallback progressCallback = nullptr);

    /**
     * @brief Process frames from SER file
     * @param serFile Path to SER file
     * @param progressCallback Progress callback
     * @return Lucky imaging result
     */
    LuckyImagingResult processFile(const std::filesystem::path& serFile,
                                   ProgressCallback progressCallback = nullptr);

    /**
     * @brief Quick process with default settings
     * @param frames Vector of frames
     * @param topPercent Top percentage to use (default 10%)
     * @return Stacked result image
     */
    cv::Mat quickProcess(const std::vector<cv::Mat>& frames,
                         double topPercent = 10.0);

    /**
     * @brief Set processing parameters
     * @param params New parameters
     */
    void setParameters(const LuckyImagingParams& params);

    /**
     * @brief Get current parameters
     * @return Current parameters
     */
    const LuckyImagingParams& getParameters() const { return params_; }

    /**
     * @brief Set quality assessor
     * @param assessor Quality assessor
     */
    void setQualityAssessor(std::shared_ptr<QualityAssessor> assessor);

    /**
     * @brief Set frame registrar
     * @param registrar Frame registrar
     */
    void setRegistrar(std::shared_ptr<FrameRegistrar> registrar);

    /**
     * @brief Set frame stacker
     * @param stacker Frame stacker
     */
    void setStacker(std::shared_ptr<FrameStacker> stacker);

    /**
     * @brief Auto-detect ROI for planetary imaging
     * @param frame Reference frame
     * @return Detected ROI
     */
    cv::Rect autoDetectPlanetROI(const cv::Mat& frame) const;

    /**
     * @brief Apply wavelet sharpening
     * @param image Input image
     * @param strength Sharpening strength
     * @param layers Number of wavelet layers
     * @return Sharpened image
     */
    cv::Mat applyWaveletSharpening(const cv::Mat& image, double strength = 1.0,
                                   int layers = 3) const;

    /**
     * @brief Apply Richardson-Lucy deconvolution
     * @param image Input image
     * @param psfSigma PSF sigma (Gaussian)
     * @param iterations Number of iterations
     * @return Deconvolved image
     */
    cv::Mat applyDeconvolution(const cv::Mat& image, double psfSigma = 1.5,
                               int iterations = 10) const;

private:
    LuckyImagingParams params_;
    std::shared_ptr<QualityAssessor> qualityAssessor_;
    std::shared_ptr<FrameRegistrar> registrar_;
    std::shared_ptr<FrameStacker> stacker_;

    /**
     * @brief Assess quality of all frames
     */
    std::vector<double> assessFrameQuality(
        const std::vector<cv::Mat>& frames,
        ProgressCallback progressCallback) const;

    /**
     * @brief Select best frames based on quality
     */
    std::vector<size_t> selectFrames(
        const std::vector<double>& qualities) const;

    /**
     * @brief Align selected frames
     */
    std::vector<cv::Mat> alignFrames(const std::vector<cv::Mat>& frames,
                                     const std::vector<size_t>& indices,
                                     ProgressCallback progressCallback);

    /**
     * @brief Stack aligned frames
     */
    cv::Mat stackFrames(const std::vector<cv::Mat>& alignedFrames,
                        ProgressCallback progressCallback);

    /**
     * @brief Apply post-processing
     */
    cv::Mat applyPostProcessing(const cv::Mat& stacked) const;

    /**
     * @brief Create Gaussian PSF kernel
     */
    cv::Mat createGaussianPSF(double sigma, int size) const;
};

/**
 * @brief Quick lucky imaging function
 * @param frames Input frames
 * @param topPercent Top percentage to use
 * @return Stacked result
 */
cv::Mat luckyStack(const std::vector<cv::Mat>& frames,
                   double topPercent = 10.0);

/**
 * @brief Get selection method name
 */
std::string selectionMethodToString(SelectionMethod method);

/**
 * @brief Parse selection method from string
 */
SelectionMethod selectionMethodFromString(const std::string& name);

}  // namespace serastro
