// ser_writer.cpp
#include "ser_writer.h"
#include <atomic>
#include <mutex>
#include <opencv2/imgproc.hpp>


namespace serastro {

struct SERWriter::Impl {
    std::filesystem::path filePath;
    SERHeader header;
    std::ofstream file;
    std::mutex writeMutex;
    std::atomic<size_t> currentFrameCount;
    bool finalized;
    std::vector<uint64_t> timestamps;

    Impl(const std::filesystem::path& path, const SERHeader& hdr)
        : filePath(path), header(hdr), currentFrameCount(0), finalized(false) {
        // Open file for writing
        file.open(path, std::ios::binary | std::ios::trunc);
        if (!file) {
            throw SERIOException(
                std::format("Failed to create SER file: {}", path.string()));
        }

        // Write header (will be updated later with frame count)
        file.write(reinterpret_cast<const char*>(&header), sizeof(SERHeader));
        if (!file) {
            throw SERIOException(std::format(
                "Failed to write SER header to: {}", path.string()));
        }
    }

    // Convert OpenCV Mat to raw data
    std::vector<uint8_t> convertMatToRaw(const cv::Mat& frame,
                                         const WriteOptions& options) {
        // Verify frame dimensions
        if (frame.rows != static_cast<int>(header.imageHeight) ||
            frame.cols != static_cast<int>(header.imageWidth)) {
            throw InvalidParameterException(std::format(
                "Frame dimensions ({} x {}) do not match SER header ({} x {})",
                frame.cols, frame.rows, header.imageWidth, header.imageHeight));
        }

        // Prepare frame in the right format
        cv::Mat prepared;

        // Handle color space
        if (header.isColor() && frame.channels() == 1) {
            // Convert grayscale to color
            if (header.colorID == static_cast<uint32_t>(SERColorID::RGB)) {
                cv::cvtColor(frame, prepared, cv::COLOR_GRAY2RGB);
            } else if (header.colorID ==
                       static_cast<uint32_t>(SERColorID::BGR)) {
                cv::cvtColor(frame, prepared, cv::COLOR_GRAY2BGR);
            } else {
                // Can't convert grayscale to Bayer pattern
                throw InvalidParameterException(
                    "Cannot convert grayscale frame to Bayer pattern for SER "
                    "file");
            }
        } else if (!header.isColor() && frame.channels() == 3) {
            // Convert color to grayscale
            cv::cvtColor(frame, prepared, cv::COLOR_BGR2GRAY);
        } else {
            prepared = frame;
        }

        // Handle bit depth conversion
        if (options.preserveOriginalBitDepth) {
            int targetDepth = -1;
            if (header.pixelDepth == 8) {
                targetDepth = CV_8U;
            } else if (header.pixelDepth == 16) {
                targetDepth = CV_16U;
            } else if (header.pixelDepth == 32) {
                targetDepth = CV_32F;
            }

            if (targetDepth != -1 && prepared.depth() != targetDepth) {
                cv::Mat converted;
                prepared.convertTo(
                    converted, CV_MAKETYPE(targetDepth, prepared.channels()));
                prepared = converted;
            }
        }

        // Convert to raw bytes
        std::vector<uint8_t> rawData(prepared.total() * prepared.elemSize());
        if (prepared.isContinuous()) {
            std::memcpy(rawData.data(), prepared.data, rawData.size());
        } else {
            size_t rowSize = prepared.cols * prepared.elemSize();
            for (int i = 0; i < prepared.rows; ++i) {
                std::memcpy(rawData.data() + i * rowSize, prepared.ptr(i),
                            rowSize);
            }
        }

        return rawData;
    }

    // Write frame implementation
    void writeFrameImpl(const cv::Mat& frame, std::optional<uint64_t> timestamp,
                        const WriteOptions& options) {
        if (finalized) {
            throw ProcessingException(
                "Cannot write frames after finalizing the SER file");
        }

        std::vector<uint8_t> rawData = convertMatToRaw(frame, options);

        {
            std::lock_guard<std::mutex> lock(writeMutex);

            // Write frame data
            file.write(reinterpret_cast<const char*>(rawData.data()),
                       rawData.size());

            if (!file) {
                throw SERIOException(std::format(
                    "Failed to write frame data to: {}", filePath.string()));
            }

            // Store timestamp if provided
            if (timestamp.has_value() && options.appendTimestamps) {
                timestamps.push_back(timestamp.value());
            } else if (options.appendTimestamps) {
                // Add current time if no timestamp provided
                timestamps.push_back(SERTimestamp::now().nanoseconds);
            }

            currentFrameCount++;
        }
    }
};

// Constructor implementation
SERWriter::SERWriter(const std::filesystem::path& filePath,
                     const SERHeader& header)
    : pImpl(std::make_unique<Impl>(filePath, header)) {}

// Destructor implementation with automatic finalization
SERWriter::~SERWriter() {
    try {
        if (!pImpl->finalized) {
            finalize();
        }
    } catch (...) {
        // Silently absorb exceptions in destructor
    }
}

// Write a frame to the file
void SERWriter::writeFrame(const cv::Mat& frame, const WriteOptions& options) {
    pImpl->writeFrameImpl(frame, std::nullopt, options);
}

// Write a frame with a timestamp
void SERWriter::writeFrameWithTimestamp(const cv::Mat& frame,
                                        uint64_t timestamp,
                                        const WriteOptions& options) {
    pImpl->writeFrameImpl(frame, timestamp, options);
}

// Write multiple frames
void SERWriter::writeFrames(const std::vector<cv::Mat>& frames,
                            const WriteOptions& options) {
    for (const auto& frame : frames) {
        writeFrame(frame, options);
    }
}

// Write raw frame data
void SERWriter::writeRawFrame(const std::vector<uint8_t>& frameData) {
    if (pImpl->finalized) {
        throw ProcessingException(
            "Cannot write frames after finalizing the SER file");
    }

    // Check frame size
    if (frameData.size() != pImpl->header.getFrameSize()) {
        throw InvalidParameterException(std::format(
            "Raw frame data size {} does not match expected size {}",
            frameData.size(), pImpl->header.getFrameSize()));
    }

    {
        std::lock_guard<std::mutex> lock(pImpl->writeMutex);

        // Write frame data
        pImpl->file.write(reinterpret_cast<const char*>(frameData.data()),
                          frameData.size());

        if (!pImpl->file) {
            throw SERIOException(
                std::format("Failed to write raw frame data to: {}",
                            pImpl->filePath.string()));
        }

        // Add timestamp
        if (!pImpl->timestamps.empty()) {
            pImpl->timestamps.push_back(SERTimestamp::now().nanoseconds);
        }

        pImpl->currentFrameCount++;
    }
}

// Finalize the file
void SERWriter::finalize() {
    std::lock_guard<std::mutex> lock(pImpl->writeMutex);

    if (pImpl->finalized) {
        return;
    }

    // Update frame count in header
    pImpl->header.frameCount = pImpl->currentFrameCount;

    // Go back to start of file and write updated header
    pImpl->file.seekp(0);
    pImpl->file.write(reinterpret_cast<const char*>(&pImpl->header),
                      sizeof(SERHeader));

    // Write timestamps if we collected any
    if (!pImpl->timestamps.empty()) {
        // Seek to end of frame data
        pImpl->file.seekp(sizeof(SERHeader) + pImpl->header.getFrameSize() *
                                                  pImpl->currentFrameCount);

        // Write all timestamps
        pImpl->file.write(
            reinterpret_cast<const char*>(pImpl->timestamps.data()),
            pImpl->timestamps.size() * sizeof(uint64_t));
    }

    // Flush and mark as finalized
    pImpl->file.flush();
    pImpl->finalized = true;
}

// Get current number of frames written
size_t SERWriter::getFrameCount() const { return pImpl->currentFrameCount; }

}  // namespace serastro