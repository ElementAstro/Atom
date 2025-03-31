// utils.h
#pragma once

#include "ser_format.h"
#include "exception.h"

#include <vector>
#include <string>
#include <optional>
#include <filesystem>
#include <span>
#include <opencv2/core.hpp>

namespace serastro {
namespace utils {

// Bit depth conversion
cv::Mat convertBitDepth(const cv::Mat& src, int targetDepth, bool normalize = true);

// Color conversion
cv::Mat convertToGrayscale(const cv::Mat& src);
cv::Mat convertToBGR(const cv::Mat& src);
cv::Mat convertToRGB(const cv::Mat& src);

// Normalization
cv::Mat normalize(const cv::Mat& src, double alpha = 0.0, double beta = 1.0);
cv::Mat normalizeMinMax(const cv::Mat& src);
cv::Mat normalizePercentile(const cv::Mat& src, double lowPercentile = 0.5, 
                           double highPercentile = 99.5);

// File utilities
std::vector<std::filesystem::path> findSerFiles(const std::filesystem::path& directory, 
                                               bool recursive = false);
std::optional<size_t> estimateFrameCount(const std::filesystem::path& serFile);
bool isValidSerFile(const std::filesystem::path& serFile);

// SER header utilities
SERHeader readSerHeader(const std::filesystem::path& serFile);
std::string serColorIdToString(SERColorID colorId);
bool writeSerHeader(const std::filesystem::path& serFile, const SERHeader& header);

// Mathematical utilities
std::vector<double> calculateHistogram(const cv::Mat& image, int bins = 256);
double calculatePSNR(const cv::Mat& reference, const cv::Mat& target);
double calculateSSIM(const cv::Mat& reference, const cv::Mat& target);

// Image statistics
struct ImageStatistics {
    double mean;
    double stdDev;
    double min;
    double max;
    double median;
    double percentile05;
    double percentile95;
};

ImageStatistics calculateImageStatistics(const cv::Mat& image);

// Hot/cold pixel detection
std::vector<cv::Point> detectHotPixels(const cv::Mat& image, double threshold = 0.95);
std::vector<cv::Point> detectColdPixels(const cv::Mat& image, double threshold = 0.05);

// Create bad pixel map
cv::Mat createBadPixelMask(const cv::Mat& image, double hotThreshold = 0.95, 
                           double coldThreshold = 0.05);

// Fix bad pixels
cv::Mat fixBadPixels(const cv::Mat& image, const cv::Mat& badPixelMask, int method = 0);

// Progress reporting
using ProgressCallback = std::function<void(double progress, const std::string& message)>;

// Version information
std::string getLibraryVersion();
std::string getOpenCVVersion();

} // namespace utils
} // namespace serastro