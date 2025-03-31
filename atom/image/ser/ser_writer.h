// ser_writer.h
#pragma once

#include "ser_format.h"
#include "exception.h"

#include <string>
#include <fstream>
#include <memory>
#include <vector>
#include <filesystem>
#include <opencv2/core.hpp>

namespace serastro {

// Options for frame writing
struct WriteOptions {
    bool preserveOriginalBitDepth = true;  // Maintain original bit depth
    bool appendTimestamps = true;          // Include timestamps if available
    bool compressOutput = false;           // Experimental compression
};

// SER File Writer class
class SERWriter {
public:
    // Create a new SER file
    explicit SERWriter(const std::filesystem::path& filePath, const SERHeader& header);
    
    // Destructor
    ~SERWriter();
    
    // Write a frame to the file
    void writeFrame(const cv::Mat& frame, const WriteOptions& options = {});
    
    // Write a frame with a timestamp
    void writeFrameWithTimestamp(const cv::Mat& frame, uint64_t timestamp,
                                const WriteOptions& options = {});
    
    // Write multiple frames
    void writeFrames(const std::vector<cv::Mat>& frames, const WriteOptions& options = {});
    
    // Finalize the file (updates header with frame count)
    void finalize();
    
    // Get current number of frames written
    size_t getFrameCount() const;
    
    // Write custom raw frame data (advanced)
    void writeRawFrame(const std::vector<uint8_t>& frameData);

private:
    struct Impl;
    std::unique_ptr<Impl> pImpl;
};

} // namespace serastro