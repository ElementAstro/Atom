// drizzle.h
#pragma once

#include "exception.h"
#include "frame_processor.h"
#include "registration.h"

#include <functional>
#include <memory>
#include <opencv2/core.hpp>
#include <string>
#include <vector>

namespace serastro {

/**
 * @enum DrizzleKernel
 * @brief Drizzle kernel types
 */
enum class DrizzleKernel {
    Point,     ///< Point kernel (fastest)
    Square,    ///< Square kernel
    Gaussian,  ///< Gaussian kernel
    Lanczos,   ///< Lanczos kernel (best quality)
    Turbo      ///< Turbo kernel (optimized)
};

/**
 * @struct DrizzleParameters
 * @brief Parameters for drizzle algorithm
 */
struct DrizzleParameters {
    // Scale factor
    double scaleFactor = 2.0;  ///< Output scale (1.5, 2.0, 3.0, etc.)

    // Drizzle drop parameters
    double dropSize = 0.8;  ///< Drop shrink factor (pixfrac, 0.0-1.0)
    DrizzleKernel kernel = DrizzleKernel::Square;

    // Weight handling
    bool useWeights = true;        ///< Use per-frame weights
    bool qualityWeighting = true;  ///< Weight by frame quality
    double minWeight = 0.01;       ///< Minimum weight threshold

    // Bad pixel handling
    bool maskBadPixels = true;
    double badPixelThreshold = 5.0;  ///< Sigma for bad pixel detection

    // Normalization
    bool normalizeOutput = true;
    bool preserveBitDepth = false;  ///< Keep original bit depth

    // Memory optimization
    bool incrementalMode = false;    ///< Process frames one at a time
    size_t maxFramesInMemory = 100;  ///< Max frames to load at once
};

/**
 * @struct DrizzleResult
 * @brief Result of drizzle processing
 */
struct DrizzleResult {
    cv::Mat image;        ///< Drizzled output image
    cv::Mat weightMap;    ///< Weight/coverage map
    cv::Mat varianceMap;  ///< Variance map (optional)

    int outputWidth = 0;          ///< Output image width
    int outputHeight = 0;         ///< Output image height
    size_t framesUsed = 0;        ///< Number of frames drizzled
    double effectiveScale = 1.0;  ///< Actual scale achieved
    double processingTimeMs = 0.0;
};

/**
 * @class DrizzleProcessor
 * @brief Drizzle algorithm for super-resolution stacking
 *
 * Implements the Drizzle (Variable-Pixel Linear Reconstruction)
 * algorithm for combining dithered/sub-pixel shifted images
 * into a higher resolution output.
 *
 * The algorithm works by:
 * 1. Creating an output grid at higher resolution
 * 2. Mapping each input pixel to the output grid
 * 3. Using "drop" sizes smaller than input pixels
 * 4. Accumulating contributions with proper weighting
 */
class DrizzleProcessor : public CustomizableProcessor {
public:
    using ProgressCallback = std::function<void(float, const std::string&)>;

    /**
     * @brief Default constructor
     */
    DrizzleProcessor();

    /**
     * @brief Construct with parameters
     * @param params Drizzle parameters
     */
    explicit DrizzleProcessor(const DrizzleParameters& params);

    /**
     * @brief Process multiple frames
     * @param frames Input frames
     * @param transforms Frame transformations (from registration)
     * @param progressCallback Progress callback
     * @return Drizzle result
     */
    DrizzleResult drizzle(const std::vector<cv::Mat>& frames,
                          const std::vector<FrameTransformation>& transforms,
                          ProgressCallback progressCallback = nullptr);

    /**
     * @brief Process with auto-registration
     * @param frames Input frames
     * @param progressCallback Progress callback
     * @return Drizzle result
     */
    DrizzleResult drizzleWithRegistration(
        const std::vector<cv::Mat>& frames,
        ProgressCallback progressCallback = nullptr);

    /**
     * @brief Add single frame to drizzle (incremental mode)
     * @param frame Input frame
     * @param transform Frame transformation
     */
    void addFrame(const cv::Mat& frame, const FrameTransformation& transform);

    /**
     * @brief Add frame with weight
     * @param frame Input frame
     * @param transform Frame transformation
     * @param weight Frame weight
     */
    void addFrame(const cv::Mat& frame, const FrameTransformation& transform,
                  double weight);

    /**
     * @brief Finalize incremental drizzle
     * @return Final drizzle result
     */
    DrizzleResult finalize();

    /**
     * @brief Reset incremental state
     */
    void reset();

    /**
     * @brief Initialize output buffers
     * @param inputWidth Input frame width
     * @param inputHeight Input frame height
     */
    void initialize(int inputWidth, int inputHeight);

    // CustomizableProcessor interface
    cv::Mat process(const cv::Mat& frame) override;
    std::string getName() const override { return "DrizzleProcessor"; }
    void setParameter(const std::string& name, double value) override;
    double getParameter(const std::string& name) const override;
    std::vector<std::string> getParameterNames() const override;
    bool hasParameter(const std::string& name) const override;

    /**
     * @brief Set drizzle parameters
     * @param params New parameters
     */
    void setDrizzleParameters(const DrizzleParameters& params);

    /**
     * @brief Get current parameters
     * @return Current parameters
     */
    const DrizzleParameters& getDrizzleParameters() const { return params_; }

    /**
     * @brief Set frame registrar for auto-registration
     * @param registrar Frame registrar
     */
    void setRegistrar(std::shared_ptr<FrameRegistrar> registrar);

    /**
     * @brief Get weight map
     * @return Current weight accumulation map
     */
    cv::Mat getWeightMap() const { return weightSum_.clone(); }

    /**
     * @brief Get coverage map (normalized weights)
     * @return Coverage map (0-1 range)
     */
    cv::Mat getCoverageMap() const;

private:
    DrizzleParameters params_;
    std::shared_ptr<FrameRegistrar> registrar_;

    // Accumulation buffers
    cv::Mat outputSum_;    ///< Accumulated weighted values
    cv::Mat weightSum_;    ///< Accumulated weights
    cv::Mat varianceSum_;  ///< Accumulated variance (optional)

    int inputWidth_ = 0;
    int inputHeight_ = 0;
    int outputWidth_ = 0;
    int outputHeight_ = 0;
    size_t framesAdded_ = 0;
    bool initialized_ = false;

    /**
     * @brief Drizzle a single frame onto output
     */
    void drizzleFrame(const cv::Mat& frame,
                      const FrameTransformation& transform, double weight);

    /**
     * @brief Calculate kernel weight for drop
     */
    double calculateKernelWeight(double dx, double dy) const;

    /**
     * @brief Create bad pixel mask
     */
    cv::Mat createBadPixelMask(const cv::Mat& frame) const;

    /**
     * @brief Normalize output by weights
     */
    cv::Mat normalizeOutput() const;
};

/**
 * @brief Quick drizzle function
 * @param frames Input frames
 * @param scale Scale factor
 * @param dropSize Drop size (pixfrac)
 * @return Drizzled image
 */
cv::Mat quickDrizzle(const std::vector<cv::Mat>& frames, double scale = 2.0,
                     double dropSize = 0.8);

/**
 * @brief Get kernel name
 */
std::string drizzleKernelToString(DrizzleKernel kernel);

/**
 * @brief Parse kernel from string
 */
DrizzleKernel drizzleKernelFromString(const std::string& name);

}  // namespace serastro
