// quality.h
#pragma once

#include "exception.h"

#include <vector>
#include <string>
#include <functional>
#include <memory>
#include <unordered_map>
#include <opencv2/core.hpp>

namespace serastro {

// Quality metric type
enum class QualityMetric {
    Sharpness,          // Edge sharpness/contrast
    SNR,                // Signal-to-noise ratio
    Entropy,            // Information entropy
    Brightness,         // Overall brightness
    Contrast,           // Overall contrast
    StarCount,          // Number of detected stars
    Composite           // Combined metric
};

// Quality assessment parameters
struct QualityParameters {
    QualityMetric primaryMetric = QualityMetric::Composite;
    double noiseThreshold = 0.1;    // For SNR calculation
    double starDetectionThreshold = 0.2; // For star detection
    std::vector<double> metricWeights = {1.0, 1.0, 1.0, 0.5, 1.0, 0.5}; // For composite
    bool normalizeMetrics = true;   // Normalize metrics to 0-1 range
    std::string roiSelector = "centered"; // Region of interest selection method
    double roiSize = 0.75;          // Size of ROI relative to full frame
};

// Quality metric function type
using QualityMetricFunction = std::function<double(const cv::Mat&)>;

// Frame quality assessor
class QualityAssessor {
public:
    QualityAssessor();
    explicit QualityAssessor(const QualityParameters& params);

    // Assess quality of a single frame
    double assessQuality(const cv::Mat& frame) const;

    // Get quality scores as vector
    std::vector<double> getQualityScores(const std::vector<cv::Mat>& frames) const;

    // Sort frames by quality (returns indices of frames in descending order)
    std::vector<size_t> sortFramesByQuality(const std::vector<cv::Mat>& frames) const;

    // Select best N frames
    std::vector<cv::Mat> selectBestFrames(const std::vector<cv::Mat>& frames, size_t count) const;

    // Add custom quality metric
    void addCustomMetric(const std::string& name,
                        QualityMetricFunction metricFunction,
                        double weight = 1.0);

    // Remove custom metric
    void removeCustomMetric(const std::string& name);

    // Get/set parameters
    void setParameters(const QualityParameters& params);
    const QualityParameters& getParameters() const;

    // Get value of specific metric
    double getMetricValue(const cv::Mat& frame, QualityMetric metric) const;
    double getCustomMetricValue(const cv::Mat& frame, const std::string& metricName) const;

    // Get details of all metrics for a frame
    struct MetricDetails {
        std::string name;
        double rawValue;
        double normalizedValue;
        double weight;
    };

    std::vector<MetricDetails> getDetailedMetrics(const cv::Mat& frame) const;

private:
    QualityParameters parameters;
    std::unordered_map<std::string, std::pair<QualityMetricFunction, double>> customMetrics;

    // Calculate ROI for quality assessment
    cv::Rect calculateROI(const cv::Mat& frame) const;

    // Internal implementations for standard metrics
    double calculateSharpness(const cv::Mat& frame) const;
    double calculateSNR(const cv::Mat& frame) const;
    double calculateEntropy(const cv::Mat& frame) const;
    double calculateBrightness(const cv::Mat& frame) const;
    double calculateContrast(const cv::Mat& frame) const;
    double calculateStarCount(const cv::Mat& frame) const;
    double calculateCompositeScore(const cv::Mat& frame) const;
};

} // namespace serastro
