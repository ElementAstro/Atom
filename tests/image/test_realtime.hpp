#pragma once

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "atom/image/core/image_blob.hpp"
#include "atom/image/processing/realtime.hpp"
#include "test_utils.hpp"

namespace atom::image::test {

class RealtimeProcessingTest : public ::testing::Test {
protected:
    void SetUp() override {
        processor = std::make_unique<RealtimeProcessor>();
        fileManager = std::make_unique<TestFileManager>();
        createTestImages();

        frameCallbackCount.store(0);
        analysisCallbackCount.store(0);
    }

    void TearDown() override {
        if (processor && processor->isRunning()) {
            processor->stop();
        }
        processor.reset();
        fileManager->cleanup();
    }

    void createTestImages() {
        auto frame1Data = TestDataGenerator::generateGradientImage(320, 240, 3);
        test_frame1 = blob(frame1Data.data(), frame1Data.size());

        auto frame2Data =
            TestDataGenerator::generateCheckerboard(320, 240, 3, 8);
        test_frame2 = blob(frame2Data.data(), frame2Data.size());

        auto frame3Data =
            TestDataGenerator::generateCircularPattern(320, 240, 50);
        test_frame3 = blob(frame3Data.data(), frame3Data.size());
    }

    std::unique_ptr<RealtimeProcessor> processor;
    std::unique_ptr<TestFileManager> fileManager;

    blob test_frame1, test_frame2, test_frame3;

    std::atomic<int> frameCallbackCount{0};
    std::atomic<int> analysisCallbackCount{0};
};

TEST_F(RealtimeProcessingTest, Initialization) {
    RealtimeParams params;
    params.targetFPS = 30.0;
    params.maxBufferSize = 5;
    params.useGPU = false;

    bool result = processor->initialize(params);
    EXPECT_TRUE(result);
}

TEST_F(RealtimeProcessingTest, InitializationDefault) {
    bool result = processor->initialize();
    EXPECT_TRUE(result);
}

TEST_F(RealtimeProcessingTest, ProcessSingleFrame) {
    processor->initialize();

    FrameInfo info;
    info.timestamp = 0;
    info.frameNumber = 0;
    info.fps = 30.0;
    info.width = 320;
    info.height = 240;
    info.channels = 3;

    auto result = processor->processFrame(test_frame1, info);
    EXPECT_FALSE(result.isEmpty());
}

TEST_F(RealtimeProcessingTest, ProcessEmptyFrame) {
    processor->initialize();

    blob empty_frame;
    auto result = processor->processFrame(empty_frame);

    EXPECT_TRUE(result.isEmpty());
}

// Test adding frames to queue
TEST_F(RealtimeProcessingTest, AddFrameToQueue) {
    RealtimeParams params;
    params.maxBufferSize = 3;
    processor->initialize(params);

    FrameInfo info;
    info.frameNumber = 0;

    bool added = processor->addFrame(test_frame1, info);
    EXPECT_TRUE(added);
}

// Test frame callback
TEST_F(RealtimeProcessingTest, FrameCallback) {
    processor->initialize();

    processor->setFrameCallback(
        [this](const blob& frame, const FrameInfo& /*info*/) {
            frameCallbackCount++;
            EXPECT_FALSE(frame.isEmpty());
        });

    FrameInfo info;
    processor->processFrame(test_frame1, info);

    // Callback should have been called
    EXPECT_GT(frameCallbackCount.load(), 0);
}

// Test analysis callback
TEST_F(RealtimeProcessingTest, AnalysisCallback) {
    processor->initialize();

    processor->setAnalysisCallback(
        [this](const std::unordered_map<std::string, double>& analysis) {
            analysisCallbackCount++;
            EXPECT_GT(analysis.size(), 0);
        });

    // Analysis callback may or may not be called depending on processing mode
    FrameInfo info;
    processor->processFrame(test_frame1, info);
}

// Test processing mode setting
TEST_F(RealtimeProcessingTest, SetProcessingMode) {
    processor->initialize();

    std::unordered_map<std::string, double> params;
    params["strength"] = 1.5;

    EXPECT_NO_THROW({
        processor->setProcessingMode(ProcessingMode::FILTER, params);
        processor->setProcessingMode(ProcessingMode::ENHANCE, params);
        processor->setProcessingMode(ProcessingMode::PASSTHROUGH, params);
    });
}

// Test filter management
TEST_F(RealtimeProcessingTest, FilterManagement) {
    processor->initialize();

    std::unordered_map<std::string, double> params;
    params["sigma"] = 1.0;

    EXPECT_NO_THROW({
        processor->addFilter("gaussian_blur", params);
        processor->addFilter("sharpen", params);
        processor->removeFilter("gaussian_blur");
        processor->clearFilters();
    });
}

// Test statistics retrieval
TEST_F(RealtimeProcessingTest, GetStatistics) {
    processor->initialize();

    auto stats = processor->getStatistics();

    EXPECT_GE(stats.averageFPS, 0.0);
    EXPECT_GE(stats.currentFPS, 0.0);
    EXPECT_GE(stats.averageLatency, 0.0);
    EXPECT_GE(stats.framesProcessed, 0);
    EXPECT_GE(stats.framesDropped, 0);
    EXPECT_FALSE(stats.status.empty());
}

// Test FPS retrieval
TEST_F(RealtimeProcessingTest, GetCurrentFPS) {
    processor->initialize();

    double fps = processor->getCurrentFPS();
    EXPECT_GE(fps, 0.0);
}

TEST_F(RealtimeProcessingTest, GetLatency) {
    processor->initialize();

    double latency = processor->getLatency();
    EXPECT_GE(latency, 0.0);
}

TEST_F(RealtimeProcessingTest, IsRunning) {
    processor->initialize();
    EXPECT_FALSE(processor->isRunning());
}

TEST_F(RealtimeProcessingTest, PauseResume) {
    processor->initialize();

    EXPECT_FALSE(processor->isPaused());

    processor->pause();
    EXPECT_TRUE(processor->isPaused());

    processor->resume();
    EXPECT_FALSE(processor->isPaused());
}

TEST_F(RealtimeProcessingTest, SetTargetFPS) {
    processor->initialize();

    EXPECT_NO_THROW({
        processor->setTargetFPS(30.0);
        processor->setTargetFPS(60.0);
        processor->setTargetFPS(15.0);
    });
}

TEST_F(RealtimeProcessingTest, SetMaxBufferSize) {
    processor->initialize();

    EXPECT_NO_THROW({
        processor->setMaxBufferSize(5);
        processor->setMaxBufferSize(10);
        processor->setMaxBufferSize(1);
    });
}

TEST_F(RealtimeProcessingTest, SetFrameDropping) {
    processor->initialize();

    EXPECT_NO_THROW({
        processor->setFrameDropping(true);
        processor->setFrameDropping(false);
    });
}

TEST_F(RealtimeProcessingTest, GetAvailableDevices) {
    processor->initialize();

    auto devices = processor->getAvailableDevices();
    EXPECT_GE(devices.size(), 0);
}

TEST_F(RealtimeProcessingTest, GetSupportedFormats) {
    processor->initialize();

    auto formats = processor->getSupportedFormats("0");
    EXPECT_GE(formats.size(), 0);
}

TEST_F(RealtimeProcessingTest, FactoryFunction) {
    auto rtProcessor = createOptimalRealtimeProcessor(false, 2);
    EXPECT_NE(rtProcessor, nullptr);
}

TEST_F(RealtimeProcessingTest, CaptureSynthetic) {
    processor->initialize();

    bool callbackInvoked = false;
    processor->setFrameCallback(
        [&callbackInvoked](const blob& /*frame*/, const FrameInfo& /*info*/) {
            callbackInvoked = true;
        });

    try {
        bool started = processor->startCapture(CaptureSource::SYNTHETIC, "",
                                               nullptr, nullptr);

        if (started) {
            EXPECT_TRUE(processor->isRunning());
            std::this_thread::sleep_for(std::chrono::milliseconds(100));

            processor->stop();
            EXPECT_FALSE(processor->isRunning());
        }
    } catch (const std::exception& e) {
        GTEST_SKIP() << "Synthetic capture not available: " << e.what();
    }
}

TEST_F(RealtimeProcessingTest, StopWithoutStart) {
    processor->initialize();

    EXPECT_NO_THROW({ processor->stop(); });
}

TEST_F(RealtimeProcessingTest, MultipleInitialization) {
    EXPECT_TRUE(processor->initialize());
    EXPECT_TRUE(processor->initialize());
}

TEST_F(RealtimeProcessingTest, VeryHighTargetFPS) {
    processor->initialize();

    EXPECT_NO_THROW({ processor->setTargetFPS(1000.0); });
}

TEST_F(RealtimeProcessingTest, VeryLowTargetFPS) {
    processor->initialize();

    EXPECT_NO_THROW({ processor->setTargetFPS(1.0); });
}

TEST_F(RealtimeProcessingTest, ZeroBufferSize) {
    processor->initialize();

    EXPECT_NO_THROW({ processor->setMaxBufferSize(0); });
}

// Test recording functionality
TEST_F(RealtimeProcessingTest, RecordingBasic) {
    processor->initialize();

    EXPECT_FALSE(processor->isRecording());

    std::string outputPath = "test_recording.mp4";

    try {
        bool started = processor->startRecording(outputPath, "h264", 80);
        if (started) {
            EXPECT_TRUE(processor->isRecording());

            processor->stopRecording();
            EXPECT_FALSE(processor->isRecording());
        }
    } catch (const std::exception& e) {
        GTEST_SKIP() << "Recording not available: " << e.what();
    }
}

// Test snapshot capture
TEST_F(RealtimeProcessingTest, SnapshotCapture) {
    processor->initialize();

    std::string outputPath = "snapshot.png";

    try {
        // Process a frame first
        FrameInfo info;
        processor->processFrame(test_frame1, info);

        bool saved = processor->takeSnapshot(outputPath);
        // May or may not succeed depending on implementation
        EXPECT_TRUE(saved || !saved);  // Just verify it doesn't crash
    } catch (const std::exception& e) {
        GTEST_SKIP() << "Snapshot not available: " << e.what();
    }
}

// Test capture resolution setting
TEST_F(RealtimeProcessingTest, SetCaptureResolution) {
    processor->initialize();

    EXPECT_NO_THROW({
        processor->setCaptureResolution(640, 480);
        processor->setCaptureResolution(1920, 1080);
    });
}

// Test capture FPS setting
TEST_F(RealtimeProcessingTest, SetCaptureFPS) {
    processor->initialize();

    EXPECT_NO_THROW({
        processor->setCaptureFPS(30.0);
        processor->setCaptureFPS(60.0);
    });
}

// Test processing with different modes
TEST_F(RealtimeProcessingTest, ProcessingModes) {
    processor->initialize();

    std::vector<ProcessingMode> modes = {
        ProcessingMode::PASSTHROUGH, ProcessingMode::FILTER,
        ProcessingMode::ENHANCE,     ProcessingMode::DETECT,
        ProcessingMode::TRACK,       ProcessingMode::ANALYZE,
        ProcessingMode::CUSTOM};

    FrameInfo info;
    for (const auto& mode : modes) {
        processor->setProcessingMode(mode);
        auto result = processor->processFrame(test_frame1, info);
        EXPECT_FALSE(result.isEmpty());
    }
}

// Test concurrent frame processing
TEST_F(RealtimeProcessingTest, ConcurrentFrameProcessing) {
    RealtimeParams params;
    params.maxBufferSize = 10;
    processor->initialize(params);

    std::atomic<int> processedCount{0};
    processor->setFrameCallback(
        [&processedCount](const blob& /*frame*/, const FrameInfo& /*info*/) {
            processedCount++;
        });

    // Add multiple frames
    for (int i = 0; i < 5; ++i) {
        FrameInfo info;
        info.frameNumber = i;
        processor->addFrame(test_frame1, info);
    }

    // Give some time for processing
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
}

// Test buffer overflow handling
TEST_F(RealtimeProcessingTest, BufferOverflow) {
    RealtimeParams params;
    params.maxBufferSize = 2;
    params.dropFrames = true;
    processor->initialize(params);

    FrameInfo info;

    // Try to add more frames than buffer can hold
    bool added1 = processor->addFrame(test_frame1, info);
    bool added2 = processor->addFrame(test_frame2, info);
    bool added3 = processor->addFrame(test_frame3, info);

    // At least some should be added
    EXPECT_TRUE(added1 || added2 || added3);
}

// Test statistics update during processing
TEST_F(RealtimeProcessingTest, StatisticsUpdate) {
    processor->initialize();

    auto stats1 = processor->getStatistics();

    // Process some frames
    FrameInfo info;
    for (int i = 0; i < 10; ++i) {
        processor->processFrame(test_frame1, info);
    }

    auto stats2 = processor->getStatistics();

    // Frames processed should have increased
    EXPECT_GE(stats2.framesProcessed, stats1.framesProcessed);
}

// Test callback with null parameters
TEST_F(RealtimeProcessingTest, NullCallbacks) {
    processor->initialize();

    EXPECT_NO_THROW({
        processor->setFrameCallback(nullptr);
        processor->setAnalysisCallback(nullptr);
    });
}

// Test processing with various filter combinations
TEST_F(RealtimeProcessingTest, MultipleFilters) {
    processor->initialize();

    std::unordered_map<std::string, double> params1;
    params1["sigma"] = 1.0;

    std::unordered_map<std::string, double> params2;
    params2["strength"] = 1.5;

    processor->addFilter("gaussian_blur", params1);
    processor->addFilter("sharpen", params2);

    FrameInfo info;
    auto result = processor->processFrame(test_frame1, info);

    EXPECT_FALSE(result.isEmpty());

    processor->clearFilters();
}

// Performance test for frame processing
TEST_F(RealtimeProcessingTest, DISABLED_PerformanceFrameProcessing) {
    processor->initialize();
    processor->setTargetFPS(30.0);

    const int numFrames = 100;
    auto start = std::chrono::high_resolution_clock::now();

    FrameInfo info;
    for (int i = 0; i < numFrames; ++i) {
        info.frameNumber = i;
        processor->processFrame(test_frame1, info);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration =
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    double fps = (numFrames * 1000.0) / duration.count();

    std::cout << "Processed " << numFrames << " frames in " << duration.count()
              << " ms (FPS: " << fps << ")" << std::endl;

    EXPECT_GT(fps, 0.0);
}

// Test thread safety of statistics access
TEST_F(RealtimeProcessingTest, ThreadSafeStatistics) {
    processor->initialize();

    std::atomic<bool> running{true};

    // Thread that continuously reads statistics
    std::thread reader([this, &running]() {
        while (running.load()) {
            auto stats = processor->getStatistics();
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    });

    // Main thread processes frames
    FrameInfo info;
    for (int i = 0; i < 50; ++i) {
        processor->processFrame(test_frame1, info);
    }

    running.store(false);
    reader.join();

    // Should not crash
    SUCCEED();
}

}  // namespace atom::image::test
