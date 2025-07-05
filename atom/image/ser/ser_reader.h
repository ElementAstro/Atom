// ser_reader.h
#pragma once

#include "exception.h"
#include "ser_format.h"


#include <concepts>
#include <filesystem>
#include <fstream>
#include <memory>
#include <mutex>
#include <opencv2/core.hpp>
#include <optional>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>


namespace serastro {

// Options for frame reading
struct ReadOptions {
    bool convertToFloat = false;   // Convert to floating point
    bool normalizeFrame = false;   // Normalize values to 0.0-1.0 range
    bool applyBayerDecode = true;  // Apply Bayer pattern decoding
    int bayerMethod = -1;          // OpenCV debayer method, -1 = auto
    bool readAsGrayscale = false;  // Force grayscale reading
    bool enableCache = true;       // Cache frames in memory
    size_t maxCacheSize = 1000;    // Maximum cache size in MB
    bool flipChannels = false;     // Flip BGR to RGB or vice versa
};

// Customization point for frame creation
template <typename T>
concept FrameFactory =
    requires(T t, cv::Mat& mat, const SERHeader& header, size_t index) {
        { t.createFrame(mat, header, index) } -> std::same_as<void>;
    };

// Default frame factory
class DefaultFrameFactory {
public:
    void createFrame(cv::Mat& /*frame*/, const SERHeader& /*header*/,
                     size_t /*frameIndex*/) const {
        // Default implementation does nothing
    }
};

// SER File Reader class
class SERReader {
public:
    // Constructor with file path
    explicit SERReader(const std::filesystem::path& filePath);

    // Destructor
    ~SERReader();

    // Get header information
    const SERHeader& getHeader() const;

    // Get file properties
    std::filesystem::path getFilePath() const;
    size_t getFrameCount() const;
    uint32_t getWidth() const;
    uint32_t getHeight() const;
    uint32_t getBitDepth() const;
    SERColorID getColorID() const;
    bool isColor() const;

    // Read a single frame
    cv::Mat readFrame(size_t frameIndex, const ReadOptions& options = {}) const;

    // Read multiple frames
    std::vector<cv::Mat> readFrames(std::span<const size_t> frameIndices,
                                    const ReadOptions& options = {}) const;

    // Read a range of frames
    std::vector<cv::Mat> readFrameRange(size_t startFrame, size_t endFrame,
                                        const ReadOptions& options = {}) const;

    // Read raw frame data (for custom processing)
    std::vector<uint8_t> readRawFrame(size_t frameIndex) const;

    // Get timestamps if available
    bool hasTimestamps() const;
    std::optional<SERTimestamp> getTimestamp(size_t frameIndex) const;
    std::vector<SERTimestamp> getAllTimestamps() const;

    // Clear the frame cache
    void clearCache() const;

    // Advanced: Use custom frame factory
    template <FrameFactory Factory>
    cv::Mat readFrameWithFactory(size_t frameIndex, const Factory& factory,
                                 const ReadOptions& options = {}) const {
        cv::Mat frame = readFrame(frameIndex, options);
        factory.createFrame(frame, header_, frameIndex);
        return frame;
    }

private:
    struct Impl;
    std::unique_ptr<Impl> pImpl;
};

}  // namespace serastro
