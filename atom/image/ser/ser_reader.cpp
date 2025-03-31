// ser_reader.cpp
#include "ser_reader.h"
#include <algorithm>
#include <atomic>
#include <deque>
#include <opencv2/imgproc.hpp>


namespace serastro {

struct SERReader::Impl {
    std::filesystem::path filePath;
    SERHeader header;
    mutable std::ifstream file;
    size_t dataOffset;
    bool hasTimestamps_;

    // Frame cache
    struct CacheEntry {
        cv::Mat frame;
        size_t frameIndex;
        size_t sizeInBytes;
        std::chrono::steady_clock::time_point lastAccess;
    };

    mutable std::mutex cacheMutex;
    mutable std::unordered_map<size_t, CacheEntry> frameCache;
    mutable size_t currentCacheSize = 0;

    Impl(const std::filesystem::path& path)
        : filePath(path), hasTimestamps_(false) {
        file.open(path, std::ios::binary);
        if (!file) {
            throw SERIOException(
                std::format("Failed to open SER file: {}", path.string()));
        }

        // Read header
        file.read(reinterpret_cast<char*>(&header), sizeof(SERHeader));
        if (!file) {
            throw SERFormatException(std::format(
                "Failed to read SER header from: {}", path.string()));
        }

        // Validate header
        if (!header.isValid()) {
            throw SERFormatException(
                std::format("Invalid SER file format: {}", path.string()));
        }

        // Calculate data offset
        dataOffset = sizeof(SERHeader);

        // Check for timestamps
        auto fileSize = std::filesystem::file_size(path);
        auto expectedDataSize =
            dataOffset + header.getFrameSize() * header.frameCount;

        // If file is larger than expected data, it likely has timestamps
        if (fileSize >=
            expectedDataSize + header.frameCount * sizeof(uint64_t)) {
            hasTimestamps_ = true;
        }
    }

    // Get timestamp offset in the file
    std::streampos getTimestampOffset(size_t frameIndex) const {
        if (!hasTimestamps_) {
            return -1;
        }

        return static_cast<std::streampos>(
            dataOffset + header.getFrameSize() * header.frameCount +
            frameIndex * sizeof(uint64_t));
    }

    // Get frame offset in the file
    std::streampos getFrameOffset(size_t frameIndex) const {
        return static_cast<std::streampos>(dataOffset +
                                           frameIndex * header.getFrameSize());
    }

    // Add frame to cache
    void addToCache(size_t frameIndex, const cv::Mat& frame,
                    const ReadOptions& options) const {
        if (!options.enableCache) {
            return;
        }

        std::lock_guard<std::mutex> lock(cacheMutex);

        // If frame is already in cache, update it
        auto it = frameCache.find(frameIndex);
        if (it != frameCache.end()) {
            currentCacheSize -= it->second.sizeInBytes;
            it->second.frame = frame.clone();
            it->second.sizeInBytes = frame.total() * frame.elemSize();
            it->second.lastAccess = std::chrono::steady_clock::now();
            currentCacheSize += it->second.sizeInBytes;
            return;
        }

        // Check cache size and evict if necessary
        size_t frameSizeBytes = frame.total() * frame.elemSize();
        size_t maxCacheSizeBytes = options.maxCacheSize * 1024 * 1024;

        while (currentCacheSize + frameSizeBytes > maxCacheSizeBytes &&
               !frameCache.empty()) {
            // Find least recently used frame
            auto lru = std::min_element(frameCache.begin(), frameCache.end(),
                                        [](const auto& a, const auto& b) {
                                            return a.second.lastAccess <
                                                   b.second.lastAccess;
                                        });

            currentCacheSize -= lru->second.sizeInBytes;
            frameCache.erase(lru);
        }

        // Add frame to cache
        CacheEntry entry;
        entry.frame = frame.clone();
        entry.frameIndex = frameIndex;
        entry.sizeInBytes = frameSizeBytes;
        entry.lastAccess = std::chrono::steady_clock::now();

        frameCache[frameIndex] = std::move(entry);
        currentCacheSize += frameSizeBytes;
    }

    // Get frame from cache if available
    std::optional<cv::Mat> getFromCache(size_t frameIndex) const {
        std::lock_guard<std::mutex> lock(cacheMutex);

        auto it = frameCache.find(frameIndex);
        if (it != frameCache.end()) {
            it->second.lastAccess = std::chrono::steady_clock::now();
            return it->second.frame.clone();
        }

        return std::nullopt;
    }

    // Read raw frame data from file
    std::vector<uint8_t> readRawFrameData(size_t frameIndex) const {
        // Check frame index
        if (frameIndex >= header.frameCount) {
            throw InvalidParameterException::outOfRange(
                "frameIndex", frameIndex, 0, header.frameCount - 1);
        }

        // Allocate buffer for frame data
        size_t frameSize = header.getFrameSize();
        std::vector<uint8_t> buffer(frameSize);

        // Position file to frame data
        std::lock_guard<std::mutex> lock(cacheMutex);  // Lock for file access
        file.seekg(getFrameOffset(frameIndex));

        if (!file) {
            throw SERIOException(
                std::format("Failed to seek to frame {} in file", frameIndex));
        }

        // Read frame data
        file.read(reinterpret_cast<char*>(buffer.data()), frameSize);

        if (!file) {
            throw SERIOException(
                std::format("Failed to read frame {} data", frameIndex));
        }

        return buffer;
    }

    // Convert raw data to OpenCV matrix
    cv::Mat convertRawData(const std::vector<uint8_t>& data,
                           const ReadOptions& options) const {
        // Determine OpenCV type
        int cvType = CV_8UC1;

        if (header.pixelDepth == 16) {
            cvType = CV_16UC1;
        } else if (header.pixelDepth == 32) {
            cvType = CV_32FC1;
        }

        // Adjust for color
        if (header.colorID == static_cast<uint32_t>(SERColorID::RGB)) {
            cvType = CV_MAKETYPE(CV_MAT_DEPTH(cvType), 3);
        } else if (header.colorID == static_cast<uint32_t>(SERColorID::BGR)) {
            cvType = CV_MAKETYPE(CV_MAT_DEPTH(cvType), 3);
        }

        // Create matrix from raw data
        cv::Mat frame(header.imageHeight, header.imageWidth, cvType,
                      const_cast<uint8_t*>(data.data()));
        cv::Mat result = frame.clone();  // Create deep copy of frame data

        // Apply Bayer decoding if needed
        if (options.applyBayerDecode && header.isBayerPattern() &&
            !options.readAsGrayscale) {
            int bayerCode;
            switch (header.getColorIDEnum()) {
                case SERColorID::BayerRGGB:
                    bayerCode = cv::COLOR_BayerBG2BGR;
                    break;
                case SERColorID::BayerGRBG:
                    bayerCode = cv::COLOR_BayerGB2BGR;
                    break;
                case SERColorID::BayerGBRG:
                    bayerCode = cv::COLOR_BayerGR2BGR;
                    break;
                case SERColorID::BayerBGGR:
                    bayerCode = cv::COLOR_BayerRG2BGR;
                    break;
                default:
                    bayerCode = cv::COLOR_BayerBG2BGR;  // Default
            }

            // Override with user-specified method if provided
            if (options.bayerMethod >= 0) {
                bayerCode = options.bayerMethod;
            }

            // Convert Bayer to color
            cv::Mat colorImg;
            cv::cvtColor(result, colorImg, bayerCode);
            result = colorImg;
        }

        // Flip channels if requested (BGR to RGB or vice versa)
        if (options.flipChannels && result.channels() == 3) {
            cv::Mat flipped;
            cv::cvtColor(result, flipped, cv::COLOR_BGR2RGB);
            result = flipped;
        }

        // Convert to grayscale if requested
        if (options.readAsGrayscale && result.channels() > 1) {
            cv::Mat gray;
            cv::cvtColor(result, gray, cv::COLOR_BGR2GRAY);
            result = gray;
        }

        // Convert to float if requested
        if (options.convertToFloat && result.depth() != CV_32F) {
            cv::Mat floatImg;
            result.convertTo(floatImg, CV_MAKETYPE(CV_32F, result.channels()));
            result = floatImg;

            if (options.normalizeFrame) {
                // Normalize based on bit depth
                double scale = 1.0;
                if (header.pixelDepth == 8) {
                    scale = 1.0 / 255.0;
                } else if (header.pixelDepth == 16) {
                    scale = 1.0 / 65535.0;
                }
                result *= scale;
            }
        }

        return result;
    }
};

// Constructor implementation
SERReader::SERReader(const std::filesystem::path& filePath)
    : pImpl(std::make_unique<Impl>(filePath)) {}

// Destructor implementation
SERReader::~SERReader() = default;

// Get header information
const SERHeader& SERReader::getHeader() const { return pImpl->header; }

// Get file properties
std::filesystem::path SERReader::getFilePath() const { return pImpl->filePath; }

size_t SERReader::getFrameCount() const { return pImpl->header.frameCount; }

uint32_t SERReader::getWidth() const { return pImpl->header.imageWidth; }

uint32_t SERReader::getHeight() const { return pImpl->header.imageHeight; }

uint32_t SERReader::getBitDepth() const { return pImpl->header.pixelDepth; }

SERColorID SERReader::getColorID() const {
    return pImpl->header.getColorIDEnum();
}

bool SERReader::isColor() const { return pImpl->header.isColor(); }

// Read a single frame
cv::Mat SERReader::readFrame(size_t frameIndex,
                             const ReadOptions& options) const {
    // Check cache first
    if (options.enableCache) {
        auto cachedFrame = pImpl->getFromCache(frameIndex);
        if (cachedFrame) {
            return *cachedFrame;
        }
    }

    // Read raw data and convert
    std::vector<uint8_t> rawData = pImpl->readRawFrameData(frameIndex);
    cv::Mat frame = pImpl->convertRawData(rawData, options);

    // Add to cache
    pImpl->addToCache(frameIndex, frame, options);

    return frame;
}

// Read multiple frames
std::vector<cv::Mat> SERReader::readFrames(std::span<const size_t> frameIndices,
                                           const ReadOptions& options) const {
    std::vector<cv::Mat> frames;
    frames.reserve(frameIndices.size());

    for (size_t index : frameIndices) {
        frames.push_back(readFrame(index, options));
    }

    return frames;
}

// Read a range of frames
std::vector<cv::Mat> SERReader::readFrameRange(
    size_t startFrame, size_t endFrame, const ReadOptions& options) const {
    if (startFrame > endFrame || endFrame >= getFrameCount()) {
        throw InvalidParameterException(
            std::format("Invalid frame range: [{}, {}], valid range is [0, {}]",
                        startFrame, endFrame, getFrameCount() - 1));
    }

    std::vector<cv::Mat> frames;
    frames.reserve(endFrame - startFrame + 1);

    for (size_t i = startFrame; i <= endFrame; ++i) {
        frames.push_back(readFrame(i, options));
    }

    return frames;
}

// Read raw frame data (for custom processing)
std::vector<uint8_t> SERReader::readRawFrame(size_t frameIndex) const {
    return pImpl->readRawFrameData(frameIndex);
}

// Check if file has timestamps
bool SERReader::hasTimestamps() const { return pImpl->hasTimestamps_; }

// Get timestamp for a specific frame
std::optional<SERTimestamp> SERReader::getTimestamp(size_t frameIndex) const {
    if (!hasTimestamps() || frameIndex >= getFrameCount()) {
        return std::nullopt;
    }

    std::lock_guard<std::mutex> lock(
        pImpl->cacheMutex);  // Lock for file access

    std::streampos pos = pImpl->getTimestampOffset(frameIndex);
    pImpl->file.seekg(pos);

    if (!pImpl->file) {
        return std::nullopt;
    }

    uint64_t timestamp;
    pImpl->file.read(reinterpret_cast<char*>(&timestamp), sizeof(timestamp));

    if (!pImpl->file) {
        return std::nullopt;
    }

    return SERTimestamp{timestamp};
}

// Get all timestamps
std::vector<SERTimestamp> SERReader::getAllTimestamps() const {
    if (!hasTimestamps()) {
        return {};
    }

    std::vector<SERTimestamp> timestamps;
    timestamps.reserve(getFrameCount());

    for (size_t i = 0; i < getFrameCount(); ++i) {
        auto ts = getTimestamp(i);
        if (ts) {
            timestamps.push_back(*ts);
        } else {
            timestamps.push_back(SERTimestamp{0});
        }
    }

    return timestamps;
}

// Clear the frame cache
void SERReader::clearCache() const {
    std::lock_guard<std::mutex> lock(pImpl->cacheMutex);
    pImpl->frameCache.clear();
    pImpl->currentCacheSize = 0;
}

}  // namespace serastro