// stacking.h
#pragma once

#include "exception.h"
#include "frame_processor.h"

#include <vector>
#include <memory>
#include <string>
#include <functional>
#include <opencv2/core.hpp>

namespace serastro {

// Forward declaration
class QualityAssessor;

// Stacking method enum
enum class StackingMethod {
    Mean,               // Average stacking
    Median,             // Median stacking
    MaximumValue,       // Maximum value stacking
    MinimumValue,       // Minimum value stacking
    SigmaClipping,      // Sigma clipping stacking
    WeightedAverage     // Weighted average stacking
};

// Frame weight calculator interface
class FrameWeightCalculator {
public:
    virtual ~FrameWeightCalculator() = default;
    
    // Calculate weight for a single frame
    virtual double calculateWeight(const cv::Mat& frame) = 0;
    
    // Calculate weights for multiple frames
    virtual std::vector<double> calculateWeights(const std::vector<cv::Mat>& frames);
};

// Quality-based weight calculator
class QualityWeightCalculator : public FrameWeightCalculator {
public:
    explicit QualityWeightCalculator(std::shared_ptr<QualityAssessor> assessor = nullptr);
    
    double calculateWeight(const cv::Mat& frame) override;
    std::vector<double> calculateWeights(const std::vector<cv::Mat>& frames) override;
    
    void setQualityAssessor(std::shared_ptr<QualityAssessor> assessor);
    std::shared_ptr<QualityAssessor> getQualityAssessor() const;

private:
    std::shared_ptr<QualityAssessor> qualityAssessor;
};

// Stacking parameters
struct StackingParameters {
    StackingMethod method = StackingMethod::Mean;
    double sigmaLow = 2.0;          // For sigma clipping
    double sigmaHigh = 2.0;         // For sigma clipping
    int iterations = 2;             // For sigma clipping
    bool normalizeBeforeStacking = true;
    bool normalizeResult = true;
    std::shared_ptr<FrameWeightCalculator> weightCalculator;
    bool maskHotPixels = false;
    double hotPixelThreshold = 0.95;
    bool maskColdPixels = false;
    double coldPixelThreshold = 0.05;
};

// Frame stacker class
class FrameStacker : public CustomizableProcessor {
public:
    FrameStacker();
    explicit FrameStacker(const StackingParameters& params);
    
    // Stack multiple frames
    cv::Mat stackFrames(const std::vector<cv::Mat>& frames);
    
    // Stack with explicit weights
    cv::Mat stackFramesWithWeights(const std::vector<cv::Mat>& frames, 
                                 const std::vector<double>& weights);
    
    // CustomizableProcessor interface implementation
    cv::Mat process(const cv::Mat& frame) override;
    std::string getName() const override;
    void setParameter(const std::string& name, double value) override;
    double getParameter(const std::string& name) const override;
    std::vector<std::string> getParameterNames() const override;
    bool hasParameter(const std::string& name) const override;
    
    // Set/get stacking parameters
    void setStackingParameters(const StackingParameters& params);
    const StackingParameters& getStackingParameters() const;
    
    // Set/get weight calculator
    void setWeightCalculator(std::shared_ptr<FrameWeightCalculator> calculator);
    std::shared_ptr<FrameWeightCalculator> getWeightCalculator() const;
    
    // Buffer management
    void addFrameToBuffer(const cv::Mat& frame);
    void clearBuffer();
    size_t getBufferSize() const;
    void setMaxBufferSize(size_t size);
    size_t getMaxBufferSize() const;

private:
    StackingParameters parameters;
    std::vector<cv::Mat> frameBuffer;
    size_t maxBufferSize = 100;
    
    // Implementation methods for different stacking algorithms
    cv::Mat stackMean(const std::vector<cv::Mat>& frames) const;
    cv::Mat stackMedian(const std::vector<cv::Mat>& frames) const;
    cv::Mat stackMaximum(const std::vector<cv::Mat>& frames) const;
    cv::Mat stackMinimum(const std::vector<cv::Mat>& frames) const;
    cv::Mat stackSigmaClipping(const std::vector<cv::Mat>& frames) const;
    cv::Mat stackWeightedAverage(const std::vector<cv::Mat>& frames,
                               const std::vector<double>& weights) const;
    
    // Prepare frames for stacking (convert to float, normalize, etc.)
    std::vector<cv::Mat> prepareFrames(const std::vector<cv::Mat>& frames) const;
    
    // Normalize result after stacking
    cv::Mat normalizeResult(const cv::Mat& stacked) const;
};

} // namespace serastro