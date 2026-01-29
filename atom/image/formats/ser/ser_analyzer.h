// ser_analyzer.h
#pragma once

#include "exception.h"
#include "quality.h"
#include "ser_format.h"
#include "ser_reader.h"

#include <chrono>
#include <filesystem>
#include <functional>
#include <memory>
#include <opencv2/core.hpp>
#include <optional>
#include <string>
#include <vector>

namespace serastro {

/**
 * @struct FrameStatistics
 * @brief Statistics for a single frame
 */
struct FrameStatistics {
    size_t frameIndex = 0;
    double mean = 0.0;
    double stdDev = 0.0;
    double min = 0.0;
    double max = 0.0;
    double median = 0.0;
    double quality = 0.0;
    double sharpness = 0.0;
    double snr = 0.0;
    std::optional<SERTimestamp> timestamp;
};

/**
 * @struct FileStatistics
 * @brief Overall statistics for a SER file
 */
struct FileStatistics {
    // File info
    std::filesystem::path filePath;
    size_t fileSize = 0;
    size_t frameCount = 0;
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t bitDepth = 0;
    SERColorID colorID = SERColorID::Mono;

    // Timing info
    std::optional<std::chrono::system_clock::time_point> startTime;
    std::optional<std::chrono::system_clock::time_point> endTime;
    std::optional<double> durationSeconds;
    std::optional<double> frameRate;

    // Data statistics
    double globalMean = 0.0;
    double globalStdDev = 0.0;
    double globalMin = 0.0;
    double globalMax = 0.0;

    // Quality statistics
    double meanQuality = 0.0;
    double bestQuality = 0.0;
    double worstQuality = 0.0;
    size_t bestFrameIndex = 0;
    size_t worstFrameIndex = 0;

    // Frame-by-frame statistics
    std::vector<FrameStatistics> frameStats;

    // Histogram data
    std::vector<double> histogram;
    int histogramBins = 256;
};

/**
 * @struct AnalyzerOptions
 * @brief Options for file analysis
 */
struct AnalyzerOptions {
    bool calculateQuality = true;     ///< Calculate quality metrics
    bool calculateHistogram = true;   ///< Calculate histogram
    bool analyzeAllFrames = false;    ///< Analyze every frame (slow)
    size_t sampleFrameCount = 10;     ///< Number of frames to sample
    bool detectBadFrames = true;      ///< Detect bad/corrupted frames
    double badFrameThreshold = 3.0;   ///< Sigma threshold for bad frames
    bool estimateFrameRate = true;    ///< Estimate frame rate from timestamps
    QualityParameters qualityParams;  ///< Quality assessment parameters
};

/**
 * @struct BadFrameInfo
 * @brief Information about a detected bad frame
 */
struct BadFrameInfo {
    size_t frameIndex;
    std::string reason;
    double score;
};

/**
 * @class SERAnalyzer
 * @brief Comprehensive SER file analyzer
 *
 * Provides detailed analysis of SER files including:
 * - File structure validation
 * - Frame quality assessment
 * - Statistical analysis
 * - Bad frame detection
 * - Timing analysis
 */
class SERAnalyzer {
public:
    using ProgressCallback = std::function<void(float, const std::string&)>;

    /**
     * @brief Default constructor
     */
    SERAnalyzer();

    /**
     * @brief Construct with options
     * @param options Analysis options
     */
    explicit SERAnalyzer(const AnalyzerOptions& options);

    /**
     * @brief Analyze a SER file
     * @param filePath Path to SER file
     * @param progressCallback Progress callback
     * @return File statistics
     */
    FileStatistics analyze(const std::filesystem::path& filePath,
                           ProgressCallback progressCallback = nullptr);

    /**
     * @brief Quick analysis (header only)
     * @param filePath Path to SER file
     * @return Basic file statistics
     */
    FileStatistics quickAnalyze(const std::filesystem::path& filePath);

    /**
     * @brief Analyze a single frame
     * @param frame Frame to analyze
     * @param frameIndex Frame index
     * @return Frame statistics
     */
    FrameStatistics analyzeFrame(const cv::Mat& frame, size_t frameIndex = 0);

    /**
     * @brief Detect bad frames in a file
     * @param filePath Path to SER file
     * @param progressCallback Progress callback
     * @return List of bad frame information
     */
    std::vector<BadFrameInfo> detectBadFrames(
        const std::filesystem::path& filePath,
        ProgressCallback progressCallback = nullptr);

    /**
     * @brief Compare two SER files
     * @param file1 First file path
     * @param file2 Second file path
     * @return Comparison summary string
     */
    std::string compareFiles(const std::filesystem::path& file1,
                             const std::filesystem::path& file2);

    /**
     * @brief Generate analysis report
     * @param stats File statistics
     * @return Report as string
     */
    std::string generateReport(const FileStatistics& stats) const;

    /**
     * @brief Export statistics to JSON
     * @param stats File statistics
     * @return JSON string
     */
    std::string exportToJSON(const FileStatistics& stats) const;

    /**
     * @brief Get best frames by quality
     * @param stats File statistics
     * @param count Number of frames to return
     * @return Indices of best frames
     */
    std::vector<size_t> getBestFrames(const FileStatistics& stats,
                                      size_t count) const;

    /**
     * @brief Set analysis options
     * @param options New options
     */
    void setOptions(const AnalyzerOptions& options) { options_ = options; }

    /**
     * @brief Get current options
     * @return Current options
     */
    const AnalyzerOptions& getOptions() const { return options_; }

    /**
     * @brief Validate SER file structure
     * @param filePath Path to file
     * @return True if valid
     */
    bool validateFile(const std::filesystem::path& filePath) const;

    /**
     * @brief Get file integrity check result
     * @param filePath Path to file
     * @return Pair of (valid, error message)
     */
    std::pair<bool, std::string> checkIntegrity(
        const std::filesystem::path& filePath) const;

private:
    AnalyzerOptions options_;
    std::unique_ptr<QualityAssessor> qualityAssessor_;

    /**
     * @brief Calculate histogram for image data
     */
    std::vector<double> calculateHistogram(const cv::Mat& image,
                                           int bins) const;

    /**
     * @brief Estimate frame rate from timestamps
     */
    std::optional<double> estimateFrameRate(
        const std::vector<SERTimestamp>& timestamps) const;

    /**
     * @brief Select sample frame indices
     */
    std::vector<size_t> selectSampleFrames(size_t totalFrames,
                                           size_t sampleCount) const;
};

/**
 * @brief Get SER file info string
 * @param filePath Path to SER file
 * @return Info string
 */
std::string getSerFileInfo(const std::filesystem::path& filePath);

/**
 * @brief Validate SER file
 * @param filePath Path to file
 * @return True if valid
 */
bool isValidSerFile(const std::filesystem::path& filePath);

}  // namespace serastro
