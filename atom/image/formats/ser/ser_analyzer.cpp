// ser_analyzer.cpp
#include "ser_analyzer.h"
#include "utils.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <numeric>
#include <opencv2/imgproc.hpp>
#include <sstream>

namespace serastro {

SERAnalyzer::SERAnalyzer()
    : qualityAssessor_(std::make_unique<QualityAssessor>()) {}

SERAnalyzer::SERAnalyzer(const AnalyzerOptions& options)
    : options_(options),
      qualityAssessor_(
          std::make_unique<QualityAssessor>(options.qualityParams)) {}

FileStatistics SERAnalyzer::analyze(const std::filesystem::path& filePath,
                                    ProgressCallback progressCallback) {
    FileStatistics stats;
    stats.filePath = filePath;

    auto reportProgress = [&progressCallback](float p, const std::string& msg) {
        if (progressCallback)
            progressCallback(p, msg);
    };

    reportProgress(0.0f, "Opening file");

    // Get file size
    stats.fileSize = std::filesystem::file_size(filePath);

    // Open reader
    SERReader reader(filePath);
    const auto& header = reader.getHeader();

    // Basic info
    stats.frameCount = reader.getFrameCount();
    stats.width = header.imageWidth;
    stats.height = header.imageHeight;
    stats.bitDepth = header.pixelDepth;
    stats.colorID = header.getColorIDEnum();

    reportProgress(0.1f, "Analyzing timestamps");

    // Timing analysis
    if (reader.hasTimestamps()) {
        auto timestamps = reader.getAllTimestamps();
        if (!timestamps.empty()) {
            stats.startTime = timestamps.front().toTimePoint();
            stats.endTime = timestamps.back().toTimePoint();

            auto duration = *stats.endTime - *stats.startTime;
            stats.durationSeconds =
                std::chrono::duration<double>(duration).count();

            if (options_.estimateFrameRate) {
                stats.frameRate = estimateFrameRate(timestamps);
            }
        }
    }

    reportProgress(0.2f, "Selecting frames to analyze");

    // Select frames for analysis
    std::vector<size_t> framesToAnalyze;
    if (options_.analyzeAllFrames) {
        framesToAnalyze.resize(stats.frameCount);
        std::iota(framesToAnalyze.begin(), framesToAnalyze.end(), 0);
    } else {
        framesToAnalyze =
            selectSampleFrames(stats.frameCount, options_.sampleFrameCount);
    }

    // Analyze selected frames
    stats.frameStats.reserve(framesToAnalyze.size());

    double totalMean = 0.0;
    double totalMin = std::numeric_limits<double>::max();
    double totalMax = std::numeric_limits<double>::lowest();
    double totalQuality = 0.0;
    double bestQuality = std::numeric_limits<double>::lowest();
    double worstQuality = std::numeric_limits<double>::max();

    cv::Mat accumulatedHist;

    for (size_t i = 0; i < framesToAnalyze.size(); ++i) {
        size_t frameIdx = framesToAnalyze[i];

        float progress =
            0.2f + 0.7f * static_cast<float>(i) / framesToAnalyze.size();
        reportProgress(progress, "Analyzing frame " + std::to_string(frameIdx));

        cv::Mat frame = reader.readFrame(frameIdx);
        FrameStatistics frameStats = analyzeFrame(frame, frameIdx);

        // Get timestamp if available
        if (reader.hasTimestamps()) {
            frameStats.timestamp = reader.getTimestamp(frameIdx);
        }

        stats.frameStats.push_back(frameStats);

        // Update global stats
        totalMean += frameStats.mean;
        totalMin = std::min(totalMin, frameStats.min);
        totalMax = std::max(totalMax, frameStats.max);
        totalQuality += frameStats.quality;

        if (frameStats.quality > bestQuality) {
            bestQuality = frameStats.quality;
            stats.bestFrameIndex = frameIdx;
        }
        if (frameStats.quality < worstQuality) {
            worstQuality = frameStats.quality;
            stats.worstFrameIndex = frameIdx;
        }

        // Accumulate histogram
        if (options_.calculateHistogram) {
            auto hist = calculateHistogram(
                frame,
                options_.qualityParams.metricWeights.size() > 0 ? 256 : 256);
            if (accumulatedHist.empty()) {
                accumulatedHist = cv::Mat::zeros(1, 256, CV_64F);
            }
            for (int j = 0; j < 256; ++j) {
                accumulatedHist.at<double>(0, j) += hist[j];
            }
        }
    }

    // Calculate global statistics
    if (!framesToAnalyze.empty()) {
        stats.globalMean = totalMean / framesToAnalyze.size();
        stats.globalMin = totalMin;
        stats.globalMax = totalMax;
        stats.meanQuality = totalQuality / framesToAnalyze.size();
        stats.bestQuality = bestQuality;
        stats.worstQuality = worstQuality;

        // Calculate global standard deviation
        double sqSum = 0.0;
        for (const auto& fs : stats.frameStats) {
            double diff = fs.mean - stats.globalMean;
            sqSum += diff * diff;
        }
        stats.globalStdDev = std::sqrt(sqSum / framesToAnalyze.size());

        // Average histogram
        if (options_.calculateHistogram && !accumulatedHist.empty()) {
            stats.histogram.resize(256);
            for (int i = 0; i < 256; ++i) {
                stats.histogram[i] =
                    accumulatedHist.at<double>(0, i) / framesToAnalyze.size();
            }
            stats.histogramBins = 256;
        }
    }

    reportProgress(1.0f, "Analysis complete");

    return stats;
}

FileStatistics SERAnalyzer::quickAnalyze(
    const std::filesystem::path& filePath) {
    FileStatistics stats;
    stats.filePath = filePath;
    stats.fileSize = std::filesystem::file_size(filePath);

    // Read header only
    SERHeader header = utils::readSerHeader(filePath);

    stats.frameCount = header.frameCount;
    stats.width = header.imageWidth;
    stats.height = header.imageHeight;
    stats.bitDepth = header.pixelDepth;
    stats.colorID = header.getColorIDEnum();
    stats.startTime = header.getDateTime();

    return stats;
}

FrameStatistics SERAnalyzer::analyzeFrame(const cv::Mat& frame,
                                          size_t frameIndex) {
    FrameStatistics stats;
    stats.frameIndex = frameIndex;

    // Convert to grayscale for analysis if needed
    cv::Mat grayFrame;
    if (frame.channels() > 1) {
        cv::cvtColor(frame, grayFrame, cv::COLOR_BGR2GRAY);
    } else {
        grayFrame = frame;
    }

    // Convert to float for calculations
    cv::Mat floatFrame;
    grayFrame.convertTo(floatFrame, CV_64F);

    // Calculate basic statistics
    cv::Scalar meanVal, stdDevVal;
    cv::meanStdDev(floatFrame, meanVal, stdDevVal);
    stats.mean = meanVal[0];
    stats.stdDev = stdDevVal[0];

    double minVal, maxVal;
    cv::minMaxLoc(floatFrame, &minVal, &maxVal);
    stats.min = minVal;
    stats.max = maxVal;

    // Calculate median
    std::vector<double> values;
    values.reserve(floatFrame.total());
    for (int i = 0; i < floatFrame.rows; ++i) {
        for (int j = 0; j < floatFrame.cols; ++j) {
            values.push_back(floatFrame.at<double>(i, j));
        }
    }
    std::sort(values.begin(), values.end());
    stats.median = values[values.size() / 2];

    // Calculate quality metrics
    if (options_.calculateQuality && qualityAssessor_) {
        stats.quality = qualityAssessor_->assessQuality(frame);
        stats.sharpness =
            qualityAssessor_->getMetricValue(frame, QualityMetric::Sharpness);
        stats.snr = qualityAssessor_->getMetricValue(frame, QualityMetric::SNR);
    }

    return stats;
}

std::vector<BadFrameInfo> SERAnalyzer::detectBadFrames(
    const std::filesystem::path& filePath, ProgressCallback progressCallback) {
    std::vector<BadFrameInfo> badFrames;

    auto reportProgress = [&progressCallback](float p, const std::string& msg) {
        if (progressCallback)
            progressCallback(p, msg);
    };

    reportProgress(0.0f, "Opening file");

    SERReader reader(filePath);
    size_t frameCount = reader.getFrameCount();

    // First pass: calculate overall statistics
    std::vector<double> qualities;
    qualities.reserve(frameCount);

    for (size_t i = 0; i < frameCount; ++i) {
        float progress = 0.5f * static_cast<float>(i) / frameCount;
        reportProgress(progress, "Analyzing frame " + std::to_string(i));

        cv::Mat frame = reader.readFrame(i);
        double quality = qualityAssessor_->assessQuality(frame);
        qualities.push_back(quality);
    }

    // Calculate mean and stddev
    double sum = std::accumulate(qualities.begin(), qualities.end(), 0.0);
    double mean = sum / qualities.size();

    double sqSum = 0.0;
    for (double q : qualities) {
        sqSum += (q - mean) * (q - mean);
    }
    double stdDev = std::sqrt(sqSum / qualities.size());

    // Second pass: detect outliers
    double threshold = options_.badFrameThreshold;

    for (size_t i = 0; i < frameCount; ++i) {
        float progress = 0.5f + 0.5f * static_cast<float>(i) / frameCount;
        reportProgress(progress, "Checking frame " + std::to_string(i));

        double quality = qualities[i];
        double zscore = std::abs(quality - mean) / stdDev;

        if (zscore > threshold) {
            BadFrameInfo info;
            info.frameIndex = i;
            info.score = quality;

            if (quality < mean) {
                info.reason =
                    "Low quality (z-score: " + std::to_string(zscore) + ")";
            } else {
                info.reason =
                    "Anomalous quality (z-score: " + std::to_string(zscore) +
                    ")";
            }

            badFrames.push_back(info);
        }
    }

    reportProgress(1.0f, "Detection complete");

    return badFrames;
}

std::string SERAnalyzer::compareFiles(const std::filesystem::path& file1,
                                      const std::filesystem::path& file2) {
    auto stats1 = quickAnalyze(file1);
    auto stats2 = quickAnalyze(file2);

    std::ostringstream oss;
    oss << "=== SER File Comparison ===\n\n";

    oss << std::left << std::setw(20) << "Property" << std::setw(25)
        << file1.filename().string() << std::setw(25)
        << file2.filename().string() << "\n";
    oss << std::string(70, '-') << "\n";

    oss << std::setw(20) << "Frame Count" << std::setw(25) << stats1.frameCount
        << std::setw(25) << stats2.frameCount << "\n";

    oss << std::setw(20) << "Resolution" << std::setw(25)
        << (std::to_string(stats1.width) + "x" + std::to_string(stats1.height))
        << std::setw(25)
        << (std::to_string(stats2.width) + "x" + std::to_string(stats2.height))
        << "\n";

    oss << std::setw(20) << "Bit Depth" << std::setw(25) << stats1.bitDepth
        << std::setw(25) << stats2.bitDepth << "\n";

    oss << std::setw(20) << "File Size (MB)" << std::setw(25) << std::fixed
        << std::setprecision(2) << (stats1.fileSize / 1024.0 / 1024.0)
        << std::setw(25) << (stats2.fileSize / 1024.0 / 1024.0) << "\n";

    return oss.str();
}

std::string SERAnalyzer::generateReport(const FileStatistics& stats) const {
    std::ostringstream oss;

    oss << "=== SER File Analysis Report ===\n\n";

    // File info
    oss << "File: " << stats.filePath.filename().string() << "\n";
    oss << "Size: " << std::fixed << std::setprecision(2)
        << (stats.fileSize / 1024.0 / 1024.0) << " MB\n\n";

    // Video properties
    oss << "--- Video Properties ---\n";
    oss << "Frame Count: " << stats.frameCount << "\n";
    oss << "Resolution: " << stats.width << " x " << stats.height << "\n";
    oss << "Bit Depth: " << stats.bitDepth << " bits\n";
    oss << "Color: " << utils::serColorIdToString(stats.colorID) << "\n";

    if (stats.frameRate) {
        oss << "Frame Rate: " << std::fixed << std::setprecision(2)
            << *stats.frameRate << " fps\n";
    }
    if (stats.durationSeconds) {
        oss << "Duration: " << std::fixed << std::setprecision(2)
            << *stats.durationSeconds << " seconds\n";
    }
    oss << "\n";

    // Statistics
    oss << "--- Image Statistics ---\n";
    oss << "Mean: " << std::fixed << std::setprecision(2) << stats.globalMean
        << "\n";
    oss << "Std Dev: " << stats.globalStdDev << "\n";
    oss << "Min: " << stats.globalMin << "\n";
    oss << "Max: " << stats.globalMax << "\n\n";

    // Quality
    oss << "--- Quality Analysis ---\n";
    oss << "Mean Quality: " << std::fixed << std::setprecision(4)
        << stats.meanQuality << "\n";
    oss << "Best Quality: " << stats.bestQuality << " (frame "
        << stats.bestFrameIndex << ")\n";
    oss << "Worst Quality: " << stats.worstQuality << " (frame "
        << stats.worstFrameIndex << ")\n";

    return oss.str();
}

std::string SERAnalyzer::exportToJSON(const FileStatistics& stats) const {
    std::ostringstream oss;

    oss << "{\n";
    oss << "  \"file\": \"" << stats.filePath.filename().string() << "\",\n";
    oss << "  \"fileSize\": " << stats.fileSize << ",\n";
    oss << "  \"frameCount\": " << stats.frameCount << ",\n";
    oss << "  \"width\": " << stats.width << ",\n";
    oss << "  \"height\": " << stats.height << ",\n";
    oss << "  \"bitDepth\": " << stats.bitDepth << ",\n";
    oss << "  \"colorID\": " << static_cast<int>(stats.colorID) << ",\n";

    if (stats.frameRate) {
        oss << "  \"frameRate\": " << *stats.frameRate << ",\n";
    }
    if (stats.durationSeconds) {
        oss << "  \"durationSeconds\": " << *stats.durationSeconds << ",\n";
    }

    oss << "  \"statistics\": {\n";
    oss << "    \"mean\": " << stats.globalMean << ",\n";
    oss << "    \"stdDev\": " << stats.globalStdDev << ",\n";
    oss << "    \"min\": " << stats.globalMin << ",\n";
    oss << "    \"max\": " << stats.globalMax << "\n";
    oss << "  },\n";

    oss << "  \"quality\": {\n";
    oss << "    \"mean\": " << stats.meanQuality << ",\n";
    oss << "    \"best\": " << stats.bestQuality << ",\n";
    oss << "    \"bestFrame\": " << stats.bestFrameIndex << ",\n";
    oss << "    \"worst\": " << stats.worstQuality << ",\n";
    oss << "    \"worstFrame\": " << stats.worstFrameIndex << "\n";
    oss << "  }\n";
    oss << "}\n";

    return oss.str();
}

std::vector<size_t> SERAnalyzer::getBestFrames(const FileStatistics& stats,
                                               size_t count) const {
    if (stats.frameStats.empty()) {
        return {};
    }

    // Create index-quality pairs
    std::vector<std::pair<size_t, double>> indexed;
    indexed.reserve(stats.frameStats.size());

    for (const auto& fs : stats.frameStats) {
        indexed.emplace_back(fs.frameIndex, fs.quality);
    }

    // Sort by quality descending
    std::sort(indexed.begin(), indexed.end(),
              [](const auto& a, const auto& b) { return a.second > b.second; });

    // Return top N indices
    count = std::min(count, indexed.size());
    std::vector<size_t> result;
    result.reserve(count);

    for (size_t i = 0; i < count; ++i) {
        result.push_back(indexed[i].first);
    }

    return result;
}

bool SERAnalyzer::validateFile(const std::filesystem::path& filePath) const {
    auto [valid, _] = checkIntegrity(filePath);
    return valid;
}

std::pair<bool, std::string> SERAnalyzer::checkIntegrity(
    const std::filesystem::path& filePath) const {
    if (!std::filesystem::exists(filePath)) {
        return {false, "File does not exist"};
    }

    std::ifstream file(filePath, std::ios::binary);
    if (!file) {
        return {false, "Cannot open file"};
    }

    // Read header
    SERHeader header;
    file.read(reinterpret_cast<char*>(&header), sizeof(SERHeader));

    if (!header.isValid()) {
        return {false, "Invalid SER header"};
    }

    // Check file size
    file.seekg(0, std::ios::end);
    size_t fileSize = file.tellg();

    size_t expectedSize =
        sizeof(SERHeader) + header.getFrameSize() * header.frameCount;

    // Account for optional timestamp trailer
    size_t expectedSizeWithTimestamps =
        expectedSize + sizeof(uint64_t) * header.frameCount;

    if (fileSize < expectedSize) {
        return {false, "File appears truncated"};
    }

    if (fileSize != expectedSize && fileSize != expectedSizeWithTimestamps) {
        return {false, "Unexpected file size"};
    }

    return {true, "File is valid"};
}

std::vector<double> SERAnalyzer::calculateHistogram(const cv::Mat& image,
                                                    int bins) const {
    cv::Mat grayImage;
    if (image.channels() > 1) {
        cv::cvtColor(image, grayImage, cv::COLOR_BGR2GRAY);
    } else {
        grayImage = image;
    }

    // Convert to 8-bit if needed
    cv::Mat image8bit;
    if (grayImage.depth() != CV_8U) {
        double minVal, maxVal;
        cv::minMaxLoc(grayImage, &minVal, &maxVal);
        grayImage.convertTo(image8bit, CV_8U, 255.0 / (maxVal - minVal),
                            -minVal * 255.0 / (maxVal - minVal));
    } else {
        image8bit = grayImage;
    }

    // Calculate histogram
    cv::Mat hist;
    int histSize = bins;
    float range[] = {0, 256};
    const float* histRange = {range};
    cv::calcHist(&image8bit, 1, nullptr, cv::Mat(), hist, 1, &histSize,
                 &histRange);

    // Convert to vector
    std::vector<double> result(bins);
    for (int i = 0; i < bins; ++i) {
        result[i] = hist.at<float>(i);
    }

    return result;
}

std::optional<double> SERAnalyzer::estimateFrameRate(
    const std::vector<SERTimestamp>& timestamps) const {
    if (timestamps.size() < 2) {
        return std::nullopt;
    }

    // Calculate time differences
    std::vector<double> deltas;
    deltas.reserve(timestamps.size() - 1);

    for (size_t i = 1; i < timestamps.size(); ++i) {
        double delta = static_cast<double>(timestamps[i].nanoseconds -
                                           timestamps[i - 1].nanoseconds) /
                       1e9;
        if (delta > 0) {
            deltas.push_back(delta);
        }
    }

    if (deltas.empty()) {
        return std::nullopt;
    }

    // Calculate median delta
    std::sort(deltas.begin(), deltas.end());
    double medianDelta = deltas[deltas.size() / 2];

    if (medianDelta > 0) {
        return 1.0 / medianDelta;
    }

    return std::nullopt;
}

std::vector<size_t> SERAnalyzer::selectSampleFrames(size_t totalFrames,
                                                    size_t sampleCount) const {
    if (sampleCount >= totalFrames) {
        std::vector<size_t> all(totalFrames);
        std::iota(all.begin(), all.end(), 0);
        return all;
    }

    std::vector<size_t> samples;
    samples.reserve(sampleCount);

    // Evenly distributed samples
    double step = static_cast<double>(totalFrames - 1) / (sampleCount - 1);
    for (size_t i = 0; i < sampleCount; ++i) {
        samples.push_back(static_cast<size_t>(i * step));
    }

    return samples;
}

// Utility functions

std::string getSerFileInfo(const std::filesystem::path& filePath) {
    SERAnalyzer analyzer;
    auto stats = analyzer.quickAnalyze(filePath);
    return analyzer.generateReport(stats);
}

bool isValidSerFile(const std::filesystem::path& filePath) {
    SERAnalyzer analyzer;
    return analyzer.validateFile(filePath);
}

}  // namespace serastro
