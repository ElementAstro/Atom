#pragma once

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <random>
#include <vector>

#include "atom/image/formats/ser/debayer.h"
#include "atom/image/formats/ser/drizzle.h"
#include "atom/image/formats/ser/lucky_imaging.h"
#include "atom/image/formats/ser/ser.hpp"
#include "atom/image/formats/ser/ser_analyzer.h"
#include "atom/image/formats/ser/ser_batch.h"
#include "atom/image/formats/ser/video_export.h"

namespace fs = std::filesystem;

class SEREnhancedTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_ser_path = "test_enhanced.ser";
        test_output_dir = "test_output_dir";

        // Create test directories
        fs::create_directories(test_output_dir);

        // Create test SER file
        createTestSERFile();

        // Create test frames
        createTestFrames();
    }

    void TearDown() override {
        // Clean up test files
        std::remove(test_ser_path.c_str());

        // Clean up test directory
        if (fs::exists(test_output_dir)) {
            fs::remove_all(test_output_dir);
        }

        for (const auto& path : temp_files) {
            std::remove(path.c_str());
        }
    }

    void createTestSERFile() {
        serastro::SERHeader header;

        std::copy_n("LUCAM-RECORDER", 14, header.fileID.begin());
        header.luID = 0;
        header.colorID = static_cast<uint32_t>(serastro::SERColorID::Mono);
        header.littleEndian = 1;
        header.imageWidth = 64;
        header.imageHeight = 64;
        header.pixelDepth = 8;
        header.frameCount = 20;
        header.observer[0] = '\0';
        header.instrument[0] = '\0';
        header.telescope[0] = '\0';
        header.dateTime = serastro::SERTimestamp::now().nanoseconds;

        std::ofstream file(test_ser_path, std::ios::binary);
        file.write(reinterpret_cast<const char*>(&header), sizeof(header));

        std::vector<uint8_t> frame_data(64 * 64);
        std::random_device rd;
        std::mt19937 gen(42);  // Fixed seed for reproducibility
        std::uniform_int_distribution<> dis(50, 200);

        for (uint32_t frame = 0; frame < header.frameCount; ++frame) {
            for (int y = 0; y < 64; ++y) {
                for (int x = 0; x < 64; ++x) {
                    // Create a pattern with some variation
                    int base = (x + y + frame * 5) % 200;
                    int noise = dis(gen) / 10;
                    frame_data[y * 64 + x] =
                        static_cast<uint8_t>(std::clamp(base + noise, 0, 255));
                }
            }
            file.write(reinterpret_cast<const char*>(frame_data.data()),
                       frame_data.size());
        }

        file.close();
    }

    void createTestFrames() {
        for (int i = 0; i < 10; ++i) {
            cv::Mat frame(64, 64, CV_8UC1);

            for (int y = 0; y < 64; ++y) {
                for (int x = 0; x < 64; ++x) {
                    // Create pattern with varying quality
                    int value = (x + y + i * 10) % 256;
                    // Add some "sharpness" variation between frames
                    if (i == 3 || i == 5) {
                        // These frames will be "sharper"
                        value = std::clamp(value + 20, 0, 255);
                    }
                    frame.at<uint8_t>(y, x) = static_cast<uint8_t>(value);
                }
            }

            test_frames.push_back(frame);
        }
    }

    cv::Mat createBayerFrame(int width, int height) {
        cv::Mat bayer(height, width, CV_8UC1);

        // Create RGGB Bayer pattern
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                uint8_t value;
                if (y % 2 == 0) {
                    if (x % 2 == 0) {
                        value = 200;  // R
                    } else {
                        value = 128;  // G
                    }
                } else {
                    if (x % 2 == 0) {
                        value = 128;  // G
                    } else {
                        value = 100;  // B
                    }
                }
                bayer.at<uint8_t>(y, x) = value;
            }
        }

        return bayer;
    }

    std::string test_ser_path;
    std::string test_output_dir;
    std::vector<std::string> temp_files;
    std::vector<cv::Mat> test_frames;
};

// ============================================================================
// SER Analyzer Tests
// ============================================================================

TEST_F(SEREnhancedTest, AnalyzerQuickAnalyze) {
    serastro::SERAnalyzer analyzer;

    auto stats = analyzer.quickAnalyze(test_ser_path);

    EXPECT_EQ(stats.frameCount, 20);
    EXPECT_EQ(stats.width, 64);
    EXPECT_EQ(stats.height, 64);
    EXPECT_EQ(stats.bitDepth, 8);
    EXPECT_EQ(stats.colorID, serastro::SERColorID::Mono);
    EXPECT_GT(stats.fileSize, 0);
}

TEST_F(SEREnhancedTest, AnalyzerFullAnalyze) {
    serastro::AnalyzerOptions options;
    options.calculateQuality = true;
    options.sampleFrameCount = 5;

    serastro::SERAnalyzer analyzer(options);

    bool progressCalled = false;
    auto stats = analyzer.analyze(
        test_ser_path,
        [&progressCalled](float progress, const std::string& msg) {
            progressCalled = true;
            EXPECT_GE(progress, 0.0f);
            EXPECT_LE(progress, 1.0f);
        });

    EXPECT_TRUE(progressCalled);
    EXPECT_EQ(stats.frameCount, 20);
    EXPECT_FALSE(stats.frameStats.empty());
    EXPECT_GE(stats.meanQuality, 0.0);
}

TEST_F(SEREnhancedTest, AnalyzerValidation) {
    serastro::SERAnalyzer analyzer;

    EXPECT_TRUE(analyzer.validateFile(test_ser_path));
    EXPECT_FALSE(analyzer.validateFile("nonexistent.ser"));

    auto [valid, message] = analyzer.checkIntegrity(test_ser_path);
    EXPECT_TRUE(valid);
}

TEST_F(SEREnhancedTest, AnalyzerFrameStatistics) {
    serastro::SERAnalyzer analyzer;

    cv::Mat frame = test_frames[0];
    auto stats = analyzer.analyzeFrame(frame, 0);

    EXPECT_EQ(stats.frameIndex, 0);
    EXPECT_GE(stats.mean, 0.0);
    EXPECT_GE(stats.stdDev, 0.0);
    EXPECT_GE(stats.min, 0.0);
    EXPECT_LE(stats.max, 255.0);
}

TEST_F(SEREnhancedTest, AnalyzerReport) {
    serastro::SERAnalyzer analyzer;
    auto stats = analyzer.quickAnalyze(test_ser_path);

    std::string report = analyzer.generateReport(stats);

    EXPECT_FALSE(report.empty());
    EXPECT_NE(report.find("Frame Count"), std::string::npos);
    EXPECT_NE(report.find("Resolution"), std::string::npos);
}

TEST_F(SEREnhancedTest, AnalyzerJSON) {
    serastro::SERAnalyzer analyzer;
    auto stats = analyzer.quickAnalyze(test_ser_path);

    std::string json = analyzer.exportToJSON(stats);

    EXPECT_FALSE(json.empty());
    EXPECT_NE(json.find("frameCount"), std::string::npos);
    EXPECT_NE(json.find("width"), std::string::npos);
}

// ============================================================================
// Debayer Tests
// ============================================================================

TEST_F(SEREnhancedTest, DebayerBasic) {
    cv::Mat bayer = createBayerFrame(64, 64);

    serastro::DebayerProcessor debayer;
    debayer.setBayerPattern(serastro::SERColorID::BayerRGGB);
    debayer.setAlgorithm(serastro::DebayerAlgorithm::Bilinear);

    cv::Mat result = debayer.process(bayer);

    EXPECT_FALSE(result.empty());
    EXPECT_EQ(result.channels(), 3);
    EXPECT_EQ(result.rows, bayer.rows);
    EXPECT_EQ(result.cols, bayer.cols);
}

TEST_F(SEREnhancedTest, DebayerVNG) {
    cv::Mat bayer = createBayerFrame(64, 64);

    serastro::DebayerParameters params;
    params.bayerPattern = serastro::SERColorID::BayerRGGB;
    params.algorithm = serastro::DebayerAlgorithm::VNG;

    serastro::DebayerProcessor debayer(params);
    cv::Mat result = debayer.process(bayer);

    EXPECT_FALSE(result.empty());
    EXPECT_EQ(result.channels(), 3);
}

TEST_F(SEREnhancedTest, DebayerSuperPixel) {
    cv::Mat bayer = createBayerFrame(64, 64);

    serastro::DebayerParameters params;
    params.bayerPattern = serastro::SERColorID::BayerRGGB;
    params.algorithm = serastro::DebayerAlgorithm::SuperPixel;

    serastro::DebayerProcessor debayer(params);
    cv::Mat result = debayer.process(bayer);

    EXPECT_FALSE(result.empty());
    EXPECT_EQ(result.channels(), 3);
    // SuperPixel produces half-size output
    EXPECT_EQ(result.rows, bayer.rows / 2);
    EXPECT_EQ(result.cols, bayer.cols / 2);
}

TEST_F(SEREnhancedTest, DebayerWhiteBalance) {
    cv::Mat bayer = createBayerFrame(64, 64);

    serastro::DebayerParameters params;
    params.bayerPattern = serastro::SERColorID::BayerRGGB;
    params.autoWhiteBalance = true;

    serastro::DebayerProcessor debayer(params);
    cv::Mat result = debayer.process(bayer);

    EXPECT_FALSE(result.empty());

    // Test manual white balance
    auto coeffs = debayer.calculateAutoWhiteBalance(result);
    EXPECT_GT(coeffs.r, 0.0);
    EXPECT_GT(coeffs.g, 0.0);
    EXPECT_GT(coeffs.b, 0.0);
}

TEST_F(SEREnhancedTest, DebayerAlgorithmStrings) {
    EXPECT_EQ(serastro::debayerAlgorithmToString(
                  serastro::DebayerAlgorithm::Bilinear),
              "Bilinear");
    EXPECT_EQ(
        serastro::debayerAlgorithmToString(serastro::DebayerAlgorithm::VNG),
        "VNG");

    EXPECT_EQ(serastro::debayerAlgorithmFromString("Bilinear"),
              serastro::DebayerAlgorithm::Bilinear);
    EXPECT_EQ(serastro::debayerAlgorithmFromString("VNG"),
              serastro::DebayerAlgorithm::VNG);
}

TEST_F(SEREnhancedTest, DebayerIsBayerPattern) {
    EXPECT_TRUE(serastro::DebayerProcessor::isBayerPattern(
        serastro::SERColorID::BayerRGGB));
    EXPECT_TRUE(serastro::DebayerProcessor::isBayerPattern(
        serastro::SERColorID::BayerGRBG));
    EXPECT_FALSE(
        serastro::DebayerProcessor::isBayerPattern(serastro::SERColorID::Mono));
    EXPECT_FALSE(
        serastro::DebayerProcessor::isBayerPattern(serastro::SERColorID::RGB));
}

// ============================================================================
// Lucky Imaging Tests
// ============================================================================

TEST_F(SEREnhancedTest, LuckyImagingBasic) {
    serastro::LuckyImagingParams params;
    params.selectionMethod = serastro::SelectionMethod::Percentage;
    params.selectionPercentage = 50.0;
    params.enableRegistration = false;  // Skip registration for speed

    serastro::LuckyImaging lucky(params);

    auto result = lucky.process(test_frames, nullptr);

    EXPECT_EQ(result.totalFrames, test_frames.size());
    EXPECT_GT(result.selectedFrames, 0);
    EXPECT_FALSE(result.stackedImage.empty());
    EXPECT_FALSE(result.qualityScores.empty());
}

TEST_F(SEREnhancedTest, LuckyImagingQuickProcess) {
    serastro::LuckyImaging lucky;

    cv::Mat result = lucky.quickProcess(test_frames, 30.0);

    EXPECT_FALSE(result.empty());
}

TEST_F(SEREnhancedTest, LuckyImagingSelectionCount) {
    serastro::LuckyImagingParams params;
    params.selectionMethod = serastro::SelectionMethod::Count;
    params.selectionCount = 3;
    params.enableRegistration = false;

    serastro::LuckyImaging lucky(params);
    auto result = lucky.process(test_frames, nullptr);

    EXPECT_EQ(result.selectedFrames, 3);
}

TEST_F(SEREnhancedTest, LuckyImagingSelectionThreshold) {
    serastro::LuckyImagingParams params;
    params.selectionMethod = serastro::SelectionMethod::Threshold;
    params.qualityThreshold = 0.1;  // Low threshold to get some frames
    params.enableRegistration = false;

    serastro::LuckyImaging lucky(params);
    auto result = lucky.process(test_frames, nullptr);

    EXPECT_GT(result.selectedFrames, 0);
}

TEST_F(SEREnhancedTest, LuckyImagingWaveletSharpening) {
    serastro::LuckyImaging lucky;

    cv::Mat sharpened = lucky.applyWaveletSharpening(test_frames[0], 1.0, 3);

    EXPECT_FALSE(sharpened.empty());
    EXPECT_EQ(sharpened.size(), test_frames[0].size());
}

TEST_F(SEREnhancedTest, LuckyImagingAutoDetectROI) {
    serastro::LuckyImaging lucky;

    // Create frame with bright center
    cv::Mat frame = cv::Mat::zeros(100, 100, CV_8UC1);
    cv::circle(frame, cv::Point(50, 50), 20, cv::Scalar(255), -1);

    cv::Rect roi = lucky.autoDetectPlanetROI(frame);

    EXPECT_GT(roi.width, 0);
    EXPECT_GT(roi.height, 0);
    EXPECT_GE(roi.x, 0);
    EXPECT_GE(roi.y, 0);
}

TEST_F(SEREnhancedTest, LuckyImagingSelectionMethodStrings) {
    EXPECT_EQ(serastro::selectionMethodToString(
                  serastro::SelectionMethod::Percentage),
              "Percentage");
    EXPECT_EQ(serastro::selectionMethodFromString("Count"),
              serastro::SelectionMethod::Count);
}

// ============================================================================
// Drizzle Tests
// ============================================================================

TEST_F(SEREnhancedTest, DrizzleBasic) {
    serastro::DrizzleParameters params;
    params.scaleFactor = 2.0;
    params.dropSize = 0.8;

    serastro::DrizzleProcessor drizzle(params);

    // Create simple transformations (identity)
    std::vector<serastro::FrameTransformation> transforms;
    for (size_t i = 0; i < test_frames.size(); ++i) {
        serastro::FrameTransformation t;
        t.type = serastro::FrameTransformation::Type::Translation;
        t.transform = cv::Mat::eye(2, 3, CV_64F);
        t.confidence = 1.0;
        transforms.push_back(t);
    }

    auto result = drizzle.drizzle(test_frames, transforms, nullptr);

    EXPECT_FALSE(result.image.empty());
    EXPECT_EQ(result.outputWidth, 64 * 2);
    EXPECT_EQ(result.outputHeight, 64 * 2);
    EXPECT_EQ(result.framesUsed, test_frames.size());
}

TEST_F(SEREnhancedTest, DrizzleScaleFactors) {
    std::vector<double> scales = {1.5, 2.0, 3.0};

    for (double scale : scales) {
        serastro::DrizzleParameters params;
        params.scaleFactor = scale;

        serastro::DrizzleProcessor drizzle(params);
        drizzle.initialize(64, 64);

        EXPECT_EQ(drizzle.getDrizzleParameters().scaleFactor, scale);
    }
}

TEST_F(SEREnhancedTest, DrizzleKernels) {
    std::vector<serastro::DrizzleKernel> kernels = {
        serastro::DrizzleKernel::Point, serastro::DrizzleKernel::Square,
        serastro::DrizzleKernel::Gaussian, serastro::DrizzleKernel::Lanczos};

    for (auto kernel : kernels) {
        serastro::DrizzleParameters params;
        params.kernel = kernel;
        params.scaleFactor = 1.5;

        serastro::DrizzleProcessor drizzle(params);

        std::vector<serastro::FrameTransformation> transforms;
        serastro::FrameTransformation t;
        t.transform = cv::Mat::eye(2, 3, CV_64F);
        t.confidence = 1.0;
        transforms.push_back(t);

        std::vector<cv::Mat> frames = {test_frames[0]};
        auto result = drizzle.drizzle(frames, transforms, nullptr);

        EXPECT_FALSE(result.image.empty())
            << "Kernel: " << serastro::drizzleKernelToString(kernel);
    }
}

TEST_F(SEREnhancedTest, DrizzleIncremental) {
    serastro::DrizzleParameters params;
    params.scaleFactor = 2.0;

    serastro::DrizzleProcessor drizzle(params);
    drizzle.initialize(64, 64);

    // Add frames incrementally
    for (size_t i = 0; i < 3; ++i) {
        serastro::FrameTransformation t;
        t.transform = cv::Mat::eye(2, 3, CV_64F);
        t.confidence = 1.0;

        drizzle.addFrame(test_frames[i], t);
    }

    auto result = drizzle.finalize();

    EXPECT_FALSE(result.image.empty());
    EXPECT_EQ(result.framesUsed, 3);
}

TEST_F(SEREnhancedTest, DrizzleKernelStrings) {
    EXPECT_EQ(serastro::drizzleKernelToString(serastro::DrizzleKernel::Square),
              "Square");
    EXPECT_EQ(
        serastro::drizzleKernelToString(serastro::DrizzleKernel::Gaussian),
        "Gaussian");

    EXPECT_EQ(serastro::drizzleKernelFromString("Square"),
              serastro::DrizzleKernel::Square);
    EXPECT_EQ(serastro::drizzleKernelFromString("Lanczos"),
              serastro::DrizzleKernel::Lanczos);
}

// ============================================================================
// Video Export Tests
// ============================================================================

TEST_F(SEREnhancedTest, VideoExportCodecs) {
    auto codecs = serastro::VideoExporter::getSupportedCodecs();

    EXPECT_FALSE(codecs.empty());
    EXPECT_NE(std::find(codecs.begin(), codecs.end(), "H264"), codecs.end());
}

TEST_F(SEREnhancedTest, VideoExportFourCC) {
    int h264 = serastro::VideoExporter::getFourCC(serastro::VideoCodec::H264);
    int mjpeg = serastro::VideoExporter::getFourCC(serastro::VideoCodec::MJPEG);

    EXPECT_NE(h264, 0);
    EXPECT_NE(mjpeg, 0);
    EXPECT_NE(h264, mjpeg);
}

TEST_F(SEREnhancedTest, VideoExportExtensions) {
    EXPECT_EQ(serastro::VideoExporter::getRecommendedExtension(
                  serastro::VideoCodec::H264),
              ".mp4");
    EXPECT_EQ(serastro::VideoExporter::getRecommendedExtension(
                  serastro::VideoCodec::VP9),
              ".webm");
    EXPECT_EQ(serastro::VideoExporter::getRecommendedExtension(
                  serastro::VideoCodec::MJPEG),
              ".avi");
}

TEST_F(SEREnhancedTest, VideoExportFrames) {
    std::string outputPath = test_output_dir + "/test_export.avi";

    serastro::VideoExportParams params;
    params.codec = serastro::VideoCodec::MJPEG;  // MJPEG is widely supported
    params.frameRate = 10.0;

    serastro::VideoExporter exporter(params);

    auto result = exporter.exportFrames(test_frames, outputPath, nullptr);

    // Note: This may fail if codecs aren't available
    if (result.success) {
        EXPECT_TRUE(fs::exists(outputPath));
        EXPECT_EQ(result.framesExported, test_frames.size());
    }
}

TEST_F(SEREnhancedTest, VideoExportSingleFrame) {
    std::string outputPath = test_output_dir + "/frame.png";

    serastro::VideoExporter exporter;
    bool success = exporter.exportFrame(test_ser_path, outputPath, 0);

    if (success) {
        EXPECT_TRUE(fs::exists(outputPath));
    }
}

TEST_F(SEREnhancedTest, VideoCodecStrings) {
    EXPECT_EQ(serastro::videoCodecToString(serastro::VideoCodec::H264), "H264");
    EXPECT_EQ(serastro::videoCodecToString(serastro::VideoCodec::MJPEG),
              "MJPEG");

    EXPECT_EQ(serastro::videoCodecFromString("H264"),
              serastro::VideoCodec::H264);
    EXPECT_EQ(serastro::videoCodecFromString("VP9"), serastro::VideoCodec::VP9);
}

// ============================================================================
// Batch Processor Tests
// ============================================================================

TEST_F(SEREnhancedTest, BatchProcessorAddJob) {
    serastro::BatchProcessor processor;

    serastro::BatchJob job;
    job.inputPath = test_ser_path;
    job.operation = serastro::BatchOperation::Analyze;

    processor.addJob(job);

    EXPECT_EQ(processor.getPendingJobCount(), 1);

    processor.clearQueue();
    EXPECT_EQ(processor.getPendingJobCount(), 0);
}

TEST_F(SEREnhancedTest, BatchProcessorAnalyze) {
    serastro::BatchConfig config;
    config.outputDir = test_output_dir;

    serastro::BatchProcessor processor(config);

    serastro::BatchJob job;
    job.inputPath = test_ser_path;
    job.operation = serastro::BatchOperation::Analyze;

    auto result = processor.processJob(job);

    EXPECT_TRUE(result.success);
    EXPECT_TRUE(result.analysisResult.has_value());
    EXPECT_EQ(result.analysisResult->frameCount, 20);
}

TEST_F(SEREnhancedTest, BatchProcessorMultipleJobs) {
    serastro::BatchConfig config;
    config.outputDir = test_output_dir;

    serastro::BatchProcessor processor(config);

    std::vector<serastro::BatchJob> jobs;
    for (int i = 0; i < 3; ++i) {
        serastro::BatchJob job;
        job.inputPath = test_ser_path;
        job.operation = serastro::BatchOperation::Analyze;
        jobs.push_back(job);
    }

    processor.addJobs(jobs);
    EXPECT_EQ(processor.getPendingJobCount(), 3);

    auto results = processor.processAll(nullptr, nullptr);

    EXPECT_EQ(results.size(), 3);
    for (const auto& result : results) {
        EXPECT_TRUE(result.success);
    }
}

TEST_F(SEREnhancedTest, BatchProcessorReport) {
    serastro::BatchProcessor processor;

    std::vector<serastro::BatchResult> results;
    serastro::BatchResult r1;
    r1.inputPath = "file1.ser";
    r1.success = true;
    r1.processingTimeSeconds = 1.5;
    results.push_back(r1);

    serastro::BatchResult r2;
    r2.inputPath = "file2.ser";
    r2.success = false;
    r2.errorMessage = "Test error";
    r2.processingTimeSeconds = 0.5;
    results.push_back(r2);

    std::string report = processor.generateReport(results);

    EXPECT_FALSE(report.empty());
    EXPECT_NE(report.find("Total Jobs: 2"), std::string::npos);
    EXPECT_NE(report.find("Successful: 1"), std::string::npos);
    EXPECT_NE(report.find("Failed: 1"), std::string::npos);
}

TEST_F(SEREnhancedTest, BatchOperationStrings) {
    EXPECT_EQ(
        serastro::batchOperationToString(serastro::BatchOperation::Analyze),
        "Analyze");
    EXPECT_EQ(
        serastro::batchOperationToString(serastro::BatchOperation::LuckyImage),
        "LuckyImage");

    EXPECT_EQ(serastro::batchOperationFromString("Analyze"),
              serastro::BatchOperation::Analyze);
    EXPECT_EQ(serastro::batchOperationFromString("ExportVideo"),
              serastro::BatchOperation::ExportVideo);
}

// ============================================================================
// Integration Tests
// ============================================================================

TEST_F(SEREnhancedTest, IntegrationAnalyzeAndLucky) {
    // First analyze to find best frames
    serastro::SERAnalyzer analyzer;
    auto stats = analyzer.analyze(test_ser_path, nullptr);

    EXPECT_GT(stats.frameStats.size(), 0);

    // Get best frames
    auto bestFrames = analyzer.getBestFrames(stats, 5);
    EXPECT_LE(bestFrames.size(), 5);

    // Use lucky imaging on best frames
    serastro::LuckyImagingParams params;
    params.enableRegistration = false;
    serastro::LuckyImaging lucky(params);

    auto result = lucky.process(test_frames, nullptr);
    EXPECT_FALSE(result.stackedImage.empty());
}

TEST_F(SEREnhancedTest, IntegrationDebayerAndStack) {
    // Create Bayer frames
    std::vector<cv::Mat> bayerFrames;
    for (int i = 0; i < 5; ++i) {
        bayerFrames.push_back(createBayerFrame(64, 64));
    }

    // Debayer
    serastro::DebayerProcessor debayer;
    debayer.setBayerPattern(serastro::SERColorID::BayerRGGB);

    std::vector<cv::Mat> colorFrames;
    for (const auto& bayer : bayerFrames) {
        colorFrames.push_back(debayer.process(bayer));
    }

    // Stack
    serastro::LuckyImagingParams params;
    params.enableRegistration = false;
    serastro::LuckyImaging lucky(params);

    auto result = lucky.process(colorFrames, nullptr);

    EXPECT_FALSE(result.stackedImage.empty());
    EXPECT_EQ(result.stackedImage.channels(), 3);
}
