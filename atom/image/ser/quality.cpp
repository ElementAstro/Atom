// quality.cpp
#include "quality.h"
#include <opencv2/imgproc.hpp>
#include <numeric>
#include <algorithm>
#include <cmath>

namespace serastro {

QualityAssessor::QualityAssessor() = default;

QualityAssessor::QualityAssessor(const QualityParameters& params)
    : parameters(params) {
}

double QualityAssessor::assessQuality(const cv::Mat& frame) const {
    // Apply the selected primary metric
    switch (parameters.primaryMetric) {
        case QualityMetric::Sharpness:
            return calculateSharpness(frame);
        case QualityMetric::SNR:
            return calculateSNR(frame);
        case QualityMetric::Entropy:
            return calculateEntropy(frame);
        case QualityMetric::Brightness:
            return calculateBrightness(frame);
        case QualityMetric::Contrast:
            return calculateContrast(frame);
        case QualityMetric::StarCount:
            return calculateStarCount(frame);
        case QualityMetric::Composite:
        default:
            return calculateCompositeScore(frame);
    }
}

std::vector<double> QualityAssessor::getQualityScores(const std::vector<cv::Mat>& frames) const {
    std::vector<double> scores;
    scores.reserve(frames.size());
    
    for (const auto& frame : frames) {
        scores.push_back(assessQuality(frame));
    }
    
    return scores;
}

std::vector<size_t> QualityAssessor::sortFramesByQuality(const std::vector<cv::Mat>& frames) const {
    // Calculate quality scores
    std::vector<double> scores = getQualityScores(frames);
    
    // Create index vector
    std::vector<size_t> indices(frames.size());
    std::iota(indices.begin(), indices.end(), 0);
    
    // Sort indices by scores (descending order)
    std::sort(indices.begin(), indices.end(), [&scores](size_t a, size_t b) {
        return scores[a] > scores[b];
    });
    
    return indices;
}

std::vector<cv::Mat> QualityAssessor::selectBestFrames(const std::vector<cv::Mat>& frames, size_t count) const {
    if (frames.empty()) {
        return {};
    }
    
    // Get sorted indices
    auto sortedIndices = sortFramesByQuality(frames);
    
    // Limit count to available frames
    count = std::min(count, frames.size());
    
    // Select best frames
    std::vector<cv::Mat> bestFrames;
    bestFrames.reserve(count);
    
    for (size_t i = 0; i < count; ++i) {
        bestFrames.push_back(frames[sortedIndices[i]]);
    }
    
    return bestFrames;
}

void QualityAssessor::addCustomMetric(const std::string& name, 
                                    QualityMetricFunction metricFunction,
                                    double weight) {
    if (weight <= 0.0) {
        throw InvalidParameterException("Metric weight must be greater than zero");
    }
    
    customMetrics[name] = std::make_pair(std::move(metricFunction), weight);
}

void QualityAssessor::removeCustomMetric(const std::string& name) {
    auto it = customMetrics.find(name);
    if (it != customMetrics.end()) {
        customMetrics.erase(it);
    }
}

void QualityAssessor::setParameters(const QualityParameters& params) {
    parameters = params;
}

const QualityParameters& QualityAssessor::getParameters() const {
    return parameters;
}

double QualityAssessor::getMetricValue(const cv::Mat& frame, QualityMetric metric) const {
    switch (metric) {
        case QualityMetric::Sharpness:
            return calculateSharpness(frame);
        case QualityMetric::SNR:
            return calculateSNR(frame);
        case QualityMetric::Entropy:
            return calculateEntropy(frame);
        case QualityMetric::Brightness:
            return calculateBrightness(frame);
        case QualityMetric::Contrast:
            return calculateContrast(frame);
        case QualityMetric::StarCount:
            return calculateStarCount(frame);
        case QualityMetric::Composite:
            return calculateCompositeScore(frame);
        default:
            throw InvalidParameterException("Unknown quality metric");
    }
}

double QualityAssessor::getCustomMetricValue(const cv::Mat& frame, const std::string& metricName) const {
    auto it = customMetrics.find(metricName);
    if (it == customMetrics.end()) {
        throw InvalidParameterException(std::format("Unknown custom metric: {}", metricName));
    }
    
    return it->second.first(frame);
}

std::vector<QualityAssessor::MetricDetails> QualityAssessor::getDetailedMetrics(const cv::Mat& frame) const {
    std::vector<MetricDetails> details;
    
    // Add standard metrics
    struct StdMetric {
        QualityMetric metric;
        std::string name;
        double weight;
    };
    
    std::vector<StdMetric> stdMetrics = {
        {QualityMetric::Sharpness, "Sharpness", parameters.metricWeights[0]},
        {QualityMetric::SNR, "SNR", parameters.metricWeights[1]},
        {QualityMetric::Entropy, "Entropy", parameters.metricWeights[2]},
        {QualityMetric::Brightness, "Brightness", parameters.metricWeights[3]},
        {QualityMetric::Contrast, "Contrast", parameters.metricWeights[4]},
        {QualityMetric::StarCount, "StarCount", parameters.metricWeights[5]}
    };
    
    // Calculate raw values
    std::vector<double> rawValues;
    rawValues.reserve(stdMetrics.size() + customMetrics.size());
    
    for (const auto& metric : stdMetrics) {
        double value = getMetricValue(frame, metric.metric);
        rawValues.push_back(value);
        details.push_back({metric.name, value, 0.0, metric.weight});
    }
    
    // Add custom metrics
    for (const auto& [name, metricPair] : customMetrics) {
        const auto& [metricFunc, weight] = metricPair;
        double value = metricFunc(frame);
        rawValues.push_back(value);
        details.push_back({name, value, 0.0, weight});
    }
    
    // Normalize if requested
    if (parameters.normalizeMetrics) {
        // Find min and max for each metric
        for (size_t i = 0; i < rawValues.size(); ++i) {
            // Simple normalization to 0-1 range - in a real implementation,
            // this would be more sophisticated based on known metric ranges
            double normalizedValue = std::clamp(rawValues[i], 0.0, 1.0);
            details[i].normalizedValue = normalizedValue;
        }
    } else {
        // Just copy raw values
        for (size_t i = 0; i < rawValues.size(); ++i) {
            details[i].normalizedValue = details[i].rawValue;
        }
    }
    
    return details;
}

cv::Rect QualityAssessor::calculateROI(const cv::Mat& frame) const {
    // Calculate ROI based on selected method
    int width = frame.cols;
    int height = frame.rows;
    
    int roiWidth = static_cast<int>(width * parameters.roiSize);
    int roiHeight = static_cast<int>(height * parameters.roiSize);
    
    if (parameters.roiSelector == "centered") {
        // Centered ROI
        int x = (width - roiWidth) / 2;
        int y = (height - roiHeight) / 2;
        return cv::Rect(x, y, roiWidth, roiHeight);
    } else if (parameters.roiSelector == "brightest") {
        // Find brightest region (simplified)
        cv::Mat blurred;
        cv::GaussianBlur(frame, blurred, cv::Size(21, 21), 5);
        
        cv::Point maxLoc;
        cv::minMaxLoc(blurred, nullptr, nullptr, nullptr, &maxLoc);
        
        int x = std::clamp(maxLoc.x - roiWidth/2, 0, width - roiWidth);
        int y = std::clamp(maxLoc.y - roiHeight/2, 0, height - roiHeight);
        
        return cv::Rect(x, y, roiWidth, roiHeight);
    } else {
        // Default to full frame
        return cv::Rect(0, 0, width, height);
    }
}

double QualityAssessor::calculateSharpness(const cv::Mat& frame) const {
    // Convert to grayscale if needed
    cv::Mat gray;
    if (frame.channels() > 1) {
        cv::cvtColor(frame, gray, cv::COLOR_BGR2GRAY);
    } else {
        gray = frame;
    }
    
    // Convert to float if needed
    cv::Mat floatImg;
    if (gray.depth() != CV_32F) {
        gray.convertTo(floatImg, CV_32F);
    } else {
        floatImg = gray;
    }
    
    // Calculate ROI
    cv::Rect roi = calculateROI(floatImg);
    cv::Mat roiImg = floatImg(roi);
    
    // Apply Laplacian
    cv::Mat laplacian;
    cv::Laplacian(roiImg, laplacian, CV_32F, 3);
    
    // Calculate variance of Laplacian (measure of sharpness)
    cv::Scalar mean, stddev;
    cv::meanStdDev(laplacian, mean, stddev);
    
    double variance = stddev[0] * stddev[0];
    
    // Normalize to a reasonable range (empirical)
    return std::min(variance / 100.0, 1.0);
}

double QualityAssessor::calculateSNR(const cv::Mat& frame) const {
    // Convert to grayscale if needed
    cv::Mat gray;
    if (frame.channels() > 1) {
        cv::cvtColor(frame, gray, cv::COLOR_BGR2GRAY);
    } else {
        gray = frame;
    }
    
    // Convert to float
    cv::Mat floatImg;
    gray.convertTo(floatImg, CV_32F);
    
    // Calculate ROI
    cv::Rect roi = calculateROI(floatImg);
    cv::Mat roiImg = floatImg(roi);
    
    // Apply Gaussian blur to estimate signal
    cv::Mat blurred;
    cv::GaussianBlur(roiImg, blurred, cv::Size(0, 0), 3);
    
    // Estimate noise as difference between original and blurred
    cv::Mat noise = roiImg - blurred;
    
    // Calculate statistics
    cv::Scalar signalMean, signalStdDev, noiseStdDev;
    cv::meanStdDev(blurred, signalMean, signalStdDev);
    cv::meanStdDev(noise, cv::Scalar(), noiseStdDev);
    
    // SNR = signal / noise
    double snr = signalMean[0] / (noiseStdDev[0] + 1e-6);
    
    // Normalize to a reasonable range (empirical)
    return std::min(snr / 20.0, 1.0);
}

double QualityAssessor::calculateEntropy(const cv::Mat& frame) const {
    // Convert to grayscale if needed
    cv::Mat gray;
    if (frame.channels() > 1) {
        cv::cvtColor(frame, gray, cv::COLOR_BGR2GRAY);
    } else {
        gray = frame;
    }
    
    // Ensure 8-bit for histogram
    cv::Mat img8bit;
    if (gray.depth() != CV_8U) {
        gray.convertTo(img8bit, CV_8U, 255.0);
    } else {
        img8bit = gray;
    }
    
    // Calculate ROI
    cv::Rect roi = calculateROI(img8bit);
    cv::Mat roiImg = img8bit(roi);
    
    // Calculate histogram
    cv::Mat hist;
    int histSize = 256;
    float range[] = {0, 256};
    const float* histRange = {range};
    cv::calcHist(&roiImg, 1, 0, cv::Mat(), hist, 1, &histSize, &histRange);
    
    // Normalize histogram
    double pixelCount = roiImg.total();
    hist /= pixelCount;
    
    // Calculate entropy
    double entropy = 0.0;
    for (int i = 0; i < histSize; i++) {
        float binVal = hist.at<float>(i);
        if (binVal > 0) {
            entropy -= binVal * std::log2(binVal);
        }
    }
    
    // Normalize to 0-1 range (max entropy for 8-bit is 8)
    return std::min(entropy / 8.0, 1.0);
}

double QualityAssessor::calculateBrightness(const cv::Mat& frame) const {
    // Convert to grayscale if needed
    cv::Mat gray;
    if (frame.channels() > 1) {
        cv::cvtColor(frame, gray, cv::COLOR_BGR2GRAY);
    } else {
        gray = frame;
    }
    
    // Calculate ROI
    cv::Rect roi = calculateROI(gray);
    cv::Mat roiImg = gray(roi);
    
    // Calculate mean brightness
    cv::Scalar meanVal = cv::mean(roiImg);
    
    // Normalize based on bit depth
    double normFactor = 1.0;
    if (gray.depth() == CV_8U) {
        normFactor = 255.0;
    } else if (gray.depth() == CV_16U) {
        normFactor = 65535.0;
    }
    
    return meanVal[0] / normFactor;
}

double QualityAssessor::calculateContrast(const cv::Mat& frame) const {
    // Convert to grayscale if needed
    cv::Mat gray;
    if (frame.channels() > 1) {
        cv::cvtColor(frame, gray, cv::COLOR_BGR2GRAY);
    } else {
        gray = frame;
    }
    
    // Convert to float
    cv::Mat floatImg;
    gray.convertTo(floatImg, CV_32F);
    
    // Calculate ROI
    cv::Rect roi = calculateROI(floatImg);
    cv::Mat roiImg = floatImg(roi);
    
    // Calculate standard deviation (measure of contrast)
    cv::Scalar mean, stddev;
    cv::meanStdDev(roiImg, mean, stddev);
    
    // Normalize by maximum possible standard deviation
    double maxStdDev = 0.5; // For normalized [0,1] image
    if (gray.depth() == CV_8U) {
        maxStdDev = 127.5;
    } else if (gray.depth() == CV_16U) {
        maxStdDev = 32767.5;
    }
    
    return std::min(stddev[0] / maxStdDev, 1.0);
}

double QualityAssessor::calculateStarCount(const cv::Mat& frame) const {
    // Convert to grayscale if needed
    cv::Mat gray;
    if (frame.channels() > 1) {
        cv::cvtColor(frame, gray, cv::COLOR_BGR2GRAY);
    } else {
        gray = frame;
    }
    
    // Ensure 8-bit for blob detection
    cv::Mat img8bit;
    if (gray.depth() != CV_8U) {
        gray.convertTo(img8bit, CV_8U, 255.0);
    } else {
        img8bit = gray;
    }
    
    // Calculate ROI
    cv::Rect roi = calculateROI(img8bit);
    cv::Mat roiImg = img8bit(roi);
    
    // Threshold the image to find bright points
    cv::Mat thresholded;
    double thresh = parameters.starDetectionThreshold * 255.0;
    cv::threshold(roiImg, thresholded, thresh, 255, cv::THRESH_BINARY);
    
    // Find contours
    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(thresholded, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
    
    // Filter contours by size and shape to find star-like objects
    int starCount = 0;
    for (const auto& contour : contours) {
        double area = cv::contourArea(contour);
        
        // Stars are typically small and roughly circular
        if (area > 3 && area < 100) {
            // Check circularity
            double perimeter = cv::arcLength(contour, true);
            double circularity = 4 * M_PI * area / (perimeter * perimeter);
            
            if (circularity > 0.7) { // More circular than not
                starCount++;
            }
        }
    }
    
    // Normalize star count to 0-1 (assuming max ~100 stars in frame)
    return std::min(static_cast<double>(starCount) / 100.0, 1.0);
}

double QualityAssessor::calculateCompositeScore(const cv::Mat& frame) const {
    // Get all individual metrics
    double sharpness = calculateSharpness(frame);
    double snr = calculateSNR(frame);
    double entropy = calculateEntropy(frame);
    double brightness = calculateBrightness(frame);
    double contrast = calculateContrast(frame);
    double starCount = calculateStarCount(frame);
    
    // Calculate weighted sum
    double weightSum = 0;
    double score = 0;
    
    // Standard metrics
    const std::vector<double> values = {sharpness, snr, entropy, brightness, contrast, starCount};
    for (size_t i = 0; i < values.size(); ++i) {
        if (i < parameters.metricWeights.size()) {
            score += values[i] * parameters.metricWeights[i];
            weightSum += parameters.metricWeights[i];
        }
    }
    
    // Add custom metrics
    for (const auto& [name, metricPair] : customMetrics) {
        const auto& [metricFunc, weight] = metricPair;
        double value = metricFunc(frame);
        score += value * weight;
        weightSum += weight;
    }
    
    // Normalize by sum of weights
    if (weightSum > 0) {
        score /= weightSum;
    }
    
    return score;
}

} // namespace serastro