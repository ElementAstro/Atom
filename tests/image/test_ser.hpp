#pragma once

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <filesystem>
#include <fstream>
#include <random>
#include <vector>

#include "atom/image/formats/ser/ser.hpp"
#include "atom/image/formats/ser/ser_reader.h"
#include "atom/image/formats/ser/ser_writer.h"
#include "atom/image/formats/ser/frame_processor.h"
#include "atom/image/formats/ser/quality.h"

namespace fs = std::filesystem;

class SERTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create test SER file paths
        test_ser_path = "test_video.ser";
        test_output_path = "test_output.ser";
        
        // Create test data
        createTestSERFile();
    }
    
    void TearDown() override {
        // Clean up test files
        std::remove(test_ser_path.c_str());
        std::remove(test_output_path.c_str());
        
        for (const auto& path : temp_files) {
            std::remove(path.c_str());
        }
    }
    
    void createTestSERFile() {
        // Create a simple test SER file with synthetic data
        serastro::SERHeader header;
        
        // Initialize header with test values
        std::memcpy(header.fileId, "LUCAM-RECORDER", 14);
        header.luId = 0;
        header.colorId = serastro::ColorFormat::MONO;
        header.littleEndian = 1;
        header.imageWidth = 64;
        header.imageHeight = 64;
        header.pixelDepthPerPlane = 8;
        header.frameCount = 10;
        header.observer[0] = '\0';
        header.instrument[0] = '\0';
        header.telescope[0] = '\0';
        header.dateTime = 1234567890; // Unix timestamp
        header.dateTimeUTC = 1234567890;
        
        // Create test file
        std::ofstream file(test_ser_path, std::ios::binary);
        
        // Write header
        file.write(reinterpret_cast<const char*>(&header), sizeof(header));
        
        // Write frame data
        std::vector<uint8_t> frame_data(64 * 64);
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(0, 255);
        
        for (uint32_t frame = 0; frame < header.frameCount; ++frame) {
            // Generate test pattern for each frame
            for (int y = 0; y < 64; ++y) {
                for (int x = 0; x < 64; ++x) {
                    // Create a simple pattern that changes per frame
                    uint8_t value = static_cast<uint8_t>((x + y + frame * 10) % 256);
                    frame_data[y * 64 + x] = value;
                }
            }
            
            file.write(reinterpret_cast<const char*>(frame_data.data()), frame_data.size());
        }
        
        file.close();
    }
    
    std::string test_ser_path;
    std::string test_output_path;
    std::vector<std::string> temp_files;
};

// Test SER header validation
TEST_F(SERTest, HeaderValidation) {
    serastro::SERHeader header;
    
    // Test invalid header (empty)
    EXPECT_FALSE(header.isValid());
    
    // Create valid header
    std::memcpy(header.fileId, "LUCAM-RECORDER", 14);
    header.luId = 0;
    header.colorId = serastro::ColorFormat::MONO;
    header.littleEndian = 1;
    header.imageWidth = 640;
    header.imageHeight = 480;
    header.pixelDepthPerPlane = 8;
    header.frameCount = 100;
    
    EXPECT_TRUE(header.isValid());
    
    // Test invalid dimensions
    header.imageWidth = 0;
    EXPECT_FALSE(header.isValid());
    
    header.imageWidth = 640;
    header.imageHeight = 0;
    EXPECT_FALSE(header.isValid());
    
    // Test invalid pixel depth
    header.imageHeight = 480;
    header.pixelDepthPerPlane = 0;
    EXPECT_FALSE(header.isValid());
    
    header.pixelDepthPerPlane = 7; // Not 8 or 16
    EXPECT_FALSE(header.isValid());
    
    // Test valid pixel depths
    header.pixelDepthPerPlane = 8;
    EXPECT_TRUE(header.isValid());
    
    header.pixelDepthPerPlane = 16;
    EXPECT_TRUE(header.isValid());
}

// Test SER reader initialization
TEST_F(SERTest, ReaderInitialization) {
    // Test with valid file
    EXPECT_NO_THROW({
        serastro::SERReader reader(test_ser_path);
        auto header = reader.getHeader();
        
        EXPECT_EQ(header.imageWidth, 64);
        EXPECT_EQ(header.imageHeight, 64);
        EXPECT_EQ(header.frameCount, 10);
        EXPECT_EQ(header.pixelDepthPerPlane, 8);
    });
    
    // Test with non-existent file
    EXPECT_THROW({
        serastro::SERReader reader("non_existent.ser");
    }, serastro::SERIOException);
    
    // Test with invalid file
    std::string invalid_path = "invalid.ser";
    std::ofstream invalid_file(invalid_path);
    invalid_file << "This is not a SER file";
    invalid_file.close();
    temp_files.push_back(invalid_path);
    
    EXPECT_THROW({
        serastro::SERReader reader(invalid_path);
    }, serastro::SERFormatException);
}

// Test frame reading
TEST_F(SERTest, FrameReading) {
    serastro::SERReader reader(test_ser_path);
    auto header = reader.getHeader();
    
    // Test reading valid frames
    for (uint32_t i = 0; i < header.frameCount; ++i) {
        auto frame = reader.readFrame(i);
        
        EXPECT_FALSE(frame.empty());
        EXPECT_EQ(frame.cols, static_cast<int>(header.imageWidth));
        EXPECT_EQ(frame.rows, static_cast<int>(header.imageHeight));
        
        // Check that frames are different (due to our test pattern)
        if (i > 0) {
            auto prev_frame = reader.readFrame(i - 1);
            cv::Mat diff;
            cv::absdiff(frame, prev_frame, diff);
            cv::Scalar mean_diff = cv::mean(diff);
            EXPECT_GT(mean_diff[0], 0); // Should be different
        }
    }
    
    // Test reading invalid frame indices
    EXPECT_THROW(reader.readFrame(header.frameCount), serastro::SERIOException);
    EXPECT_THROW(reader.readFrame(header.frameCount + 100), serastro::SERIOException);
}

// Test frame range reading
TEST_F(SERTest, FrameRangeReading) {
    serastro::SERReader reader(test_ser_path);
    auto header = reader.getHeader();
    
    // Test reading a range of frames
    uint32_t start_frame = 2;
    uint32_t end_frame = 7;
    auto frames = reader.readFrameRange(start_frame, end_frame);
    
    EXPECT_EQ(frames.size(), end_frame - start_frame + 1);
    
    // Verify each frame
    for (size_t i = 0; i < frames.size(); ++i) {
        EXPECT_FALSE(frames[i].empty());
        EXPECT_EQ(frames[i].cols, static_cast<int>(header.imageWidth));
        EXPECT_EQ(frames[i].rows, static_cast<int>(header.imageHeight));
    }
    
    // Test invalid ranges
    EXPECT_THROW(reader.readFrameRange(5, 3), serastro::SERIOException); // start > end
    EXPECT_THROW(reader.readFrameRange(0, header.frameCount), serastro::SERIOException); // end >= frameCount
}

// Test SER writer
TEST_F(SERTest, SERWriter) {
    // Create test frames
    std::vector<cv::Mat> test_frames;
    for (int i = 0; i < 5; ++i) {
        cv::Mat frame(32, 32, CV_8UC1);
        
        // Fill with test pattern
        for (int y = 0; y < 32; ++y) {
            for (int x = 0; x < 32; ++x) {
                frame.at<uint8_t>(y, x) = static_cast<uint8_t>((x + y + i * 5) % 256);
            }
        }
        
        test_frames.push_back(frame);
    }
    
    // Write SER file
    {
        serastro::SERWriter writer(test_output_path);
        
        // Configure header
        serastro::SERHeader header;
        std::memcpy(header.fileId, "LUCAM-RECORDER", 14);
        header.luId = 0;
        header.colorId = serastro::ColorFormat::MONO;
        header.littleEndian = 1;
        header.imageWidth = 32;
        header.imageHeight = 32;
        header.pixelDepthPerPlane = 8;
        header.frameCount = static_cast<uint32_t>(test_frames.size());
        header.dateTime = 1234567890;
        header.dateTimeUTC = 1234567890;
        
        writer.writeHeader(header);
        
        // Write frames
        for (const auto& frame : test_frames) {
            writer.writeFrame(frame);
        }
    } // Writer destructor should finalize the file
    
    // Verify written file by reading it back
    serastro::SERReader reader(test_output_path);
    auto read_header = reader.getHeader();
    
    EXPECT_EQ(read_header.imageWidth, 32);
    EXPECT_EQ(read_header.imageHeight, 32);
    EXPECT_EQ(read_header.frameCount, test_frames.size());
    
    // Verify frame data
    for (size_t i = 0; i < test_frames.size(); ++i) {
        auto read_frame = reader.readFrame(static_cast<uint32_t>(i));
        
        // Compare with original
        cv::Mat diff;
        cv::absdiff(test_frames[i], read_frame, diff);
        cv::Scalar mean_diff = cv::mean(diff);
        EXPECT_LT(mean_diff[0], 1.0); // Should be very similar (allowing for minor differences)
    }
}

// Test frame processor
TEST_F(SERTest, FrameProcessor) {
    serastro::SERReader reader(test_ser_path);
    auto header = reader.getHeader();
    
    serastro::FrameProcessor processor;
    
    // Test frame alignment
    auto frame1 = reader.readFrame(0);
    auto frame2 = reader.readFrame(1);
    
    auto aligned_frame = processor.alignFrames(frame1, frame2);
    
    EXPECT_FALSE(aligned_frame.empty());
    EXPECT_EQ(aligned_frame.size(), frame2.size());
    
    // Test frame stacking
    std::vector<cv::Mat> frames_to_stack;
    for (uint32_t i = 0; i < std::min(5u, header.frameCount); ++i) {
        frames_to_stack.push_back(reader.readFrame(i));
    }
    
    auto stacked_frame = processor.stackFrames(frames_to_stack);
    
    EXPECT_FALSE(stacked_frame.empty());
    EXPECT_EQ(stacked_frame.cols, frames_to_stack[0].cols);
    EXPECT_EQ(stacked_frame.rows, frames_to_stack[0].rows);
    
    // Test noise reduction
    auto denoised_frame = processor.reduceNoise(frame1);
    
    EXPECT_FALSE(denoised_frame.empty());
    EXPECT_EQ(denoised_frame.size(), frame1.size());
}

// Test quality assessment
TEST_F(SERTest, QualityAssessment) {
    serastro::SERReader reader(test_ser_path);
    auto header = reader.getHeader();
    
    serastro::QualityAssessment qa;
    
    // Test quality metrics for individual frames
    for (uint32_t i = 0; i < std::min(3u, header.frameCount); ++i) {
        auto frame = reader.readFrame(i);
        
        auto metrics = qa.assessFrame(frame);
        
        EXPECT_GE(metrics.sharpness, 0.0);
        EXPECT_GE(metrics.brightness, 0.0);
        EXPECT_GE(metrics.contrast, 0.0);
        EXPECT_GE(metrics.noise_level, 0.0);
        EXPECT_GE(metrics.overall_quality, 0.0);
        EXPECT_LE(metrics.overall_quality, 100.0);
    }
    
    // Test batch quality assessment
    std::vector<cv::Mat> frames;
    for (uint32_t i = 0; i < std::min(5u, header.frameCount); ++i) {
        frames.push_back(reader.readFrame(i));
    }
    
    auto batch_metrics = qa.assessFrames(frames);
    
    EXPECT_EQ(batch_metrics.size(), frames.size());
    
    for (const auto& metrics : batch_metrics) {
        EXPECT_GE(metrics.overall_quality, 0.0);
        EXPECT_LE(metrics.overall_quality, 100.0);
    }
    
    // Test frame ranking
    auto ranked_indices = qa.rankFramesByQuality(frames);
    
    EXPECT_EQ(ranked_indices.size(), frames.size());
    
    // Check that indices are valid
    for (auto idx : ranked_indices) {
        EXPECT_LT(idx, frames.size());
    }
    
    // Check that ranking is in descending order of quality
    if (ranked_indices.size() > 1) {
        auto first_quality = qa.assessFrame(frames[ranked_indices[0]]).overall_quality;
        auto last_quality = qa.assessFrame(frames[ranked_indices.back()]).overall_quality;
        EXPECT_GE(first_quality, last_quality);
    }
}

// Test error handling
TEST_F(SERTest, ErrorHandling) {
    // Test SER exceptions
    serastro::SERIOException io_ex("Test IO error");
    EXPECT_STREQ(io_ex.what(), "Test IO error");
    
    serastro::SERFormatException format_ex("Test format error");
    EXPECT_STREQ(format_ex.what(), "Test format error");
    
    // Test reader with corrupted file
    std::string corrupted_path = "corrupted.ser";
    {
        std::ofstream corrupted_file(corrupted_path, std::ios::binary);
        
        // Write partial header
        serastro::SERHeader header;
        std::memcpy(header.fileId, "LUCAM-RECORDER", 14);
        header.frameCount = 1000; // Claim many frames
        header.imageWidth = 640;
        header.imageHeight = 480;
        header.pixelDepthPerPlane = 8;
        
        corrupted_file.write(reinterpret_cast<const char*>(&header), sizeof(header));
        
        // Write insufficient frame data
        std::vector<uint8_t> partial_data(100); // Much less than needed
        corrupted_file.write(reinterpret_cast<const char*>(partial_data.data()), partial_data.size());
    }
    temp_files.push_back(corrupted_path);
    
    serastro::SERReader corrupted_reader(corrupted_path);
    
    // Should throw when trying to read frames that don't exist
    EXPECT_THROW(corrupted_reader.readFrame(0), serastro::SERIOException);
}

// Performance test (disabled by default)
TEST_F(SERTest, DISABLED_PerformanceTest) {
    serastro::SERReader reader(test_ser_path);
    auto header = reader.getHeader();
    
    // Measure frame reading performance
    auto start = std::chrono::high_resolution_clock::now();
    
    for (uint32_t i = 0; i < header.frameCount; ++i) {
        auto frame = reader.readFrame(i);
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    std::cout << "Read " << header.frameCount << " frames in " 
              << duration.count() << " ms" << std::endl;
    std::cout << "Average: " << (duration.count() / header.frameCount) 
              << " ms per frame" << std::endl;
}
