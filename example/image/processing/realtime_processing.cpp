/**
 * @file realtime_processing.cpp
 * @brief Real-time image processing demonstration
 *
 * This example demonstrates:
 * - Live camera capture and processing
 * - Real-time filtering and effects
 * - Frame rate optimization techniques
 * - Multi-threaded processing pipeline
 * - Adaptive quality control
 * - Performance monitoring and statistics
 *
 * @author Atom Image Module
 * @date 2025
 */

#include <iostream>
#include <vector>
#include <string>
#include <chrono>
#include <thread>
#include <atomic>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <iomanip>

#include "atom/image/core/image_blob.hpp"
#include "atom/image/processing/image_processor.hpp"
#include "atom/image/io/camera_capture.hpp"
#include "atom/image/processing/realtime_processor.hpp"

using namespace atom::image;
using namespace std::chrono;

/**
 * @brief Thread-safe frame buffer for real-time processing
 */
class FrameBuffer {
private:
    std::queue<blob> frames_;
    std::mutex mutex_;
    std::condition_variable condition_;
    size_t maxSize_;
    std::atomic<bool> shutdown_{false};

public:
    explicit FrameBuffer(size_t maxSize = 10) : maxSize_(maxSize) {}
    
    void push(const blob& frame) {
        std::unique_lock<std::mutex> lock(mutex_);
        
        // Drop oldest frames if buffer is full
        while (frames_.size() >= maxSize_ && !shutdown_) {
            frames_.pop();
        }
        
        if (!shutdown_) {
            frames_.push(frame);
            condition_.notify_one();
        }
    }
    
    bool pop(blob& frame, std::chrono::milliseconds timeout = std::chrono::milliseconds(100)) {
        std::unique_lock<std::mutex> lock(mutex_);
        
        if (condition_.wait_for(lock, timeout, [this] { return !frames_.empty() || shutdown_; })) {
            if (!frames_.empty()) {
                frame = frames_.front();
                frames_.pop();
                return true;
            }
        }
        return false;
    }
    
    void shutdown() {
        shutdown_ = true;
        condition_.notify_all();
    }
    
    size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return frames_.size();
    }
};

/**
 * @brief Performance statistics tracker
 */
class PerformanceTracker {
private:
    std::vector<double> frameTimes_;
    std::vector<double> processingTimes_;
    std::mutex mutex_;
    steady_clock::time_point lastFrameTime_;
    size_t maxSamples_;

public:
    explicit PerformanceTracker(size_t maxSamples = 100) : maxSamples_(maxSamples) {
        lastFrameTime_ = steady_clock::now();
    }
    
    void recordFrame() {
        std::lock_guard<std::mutex> lock(mutex_);
        auto now = steady_clock::now();
        auto frameTime = duration_cast<microseconds>(now - lastFrameTime_).count() / 1000.0;
        
        frameTimes_.push_back(frameTime);
        if (frameTimes_.size() > maxSamples_) {
            frameTimes_.erase(frameTimes_.begin());
        }
        
        lastFrameTime_ = now;
    }
    
    void recordProcessingTime(double processingTimeMs) {
        std::lock_guard<std::mutex> lock(mutex_);
        processingTimes_.push_back(processingTimeMs);
        if (processingTimes_.size() > maxSamples_) {
            processingTimes_.erase(processingTimes_.begin());
        }
    }
    
    struct Stats {
        double avgFrameTime = 0.0;
        double avgFPS = 0.0;
        double avgProcessingTime = 0.0;
        double maxProcessingTime = 0.0;
        double processingLoad = 0.0; // Processing time / frame time
        size_t droppedFrames = 0;
    };
    
    Stats getStats() const {
        std::lock_guard<std::mutex> lock(mutex_);
        Stats stats;
        
        if (!frameTimes_.empty()) {
            double sum = 0.0;
            for (double time : frameTimes_) {
                sum += time;
            }
            stats.avgFrameTime = sum / frameTimes_.size();
            stats.avgFPS = 1000.0 / stats.avgFrameTime;
        }
        
        if (!processingTimes_.empty()) {
            double sum = 0.0;
            double maxTime = 0.0;
            for (double time : processingTimes_) {
                sum += time;
                maxTime = std::max(maxTime, time);
            }
            stats.avgProcessingTime = sum / processingTimes_.size();
            stats.maxProcessingTime = maxTime;
            
            if (stats.avgFrameTime > 0) {
                stats.processingLoad = stats.avgProcessingTime / stats.avgFrameTime;
            }
        }
        
        return stats;
    }
};

/**
 * @brief Simulate camera capture for demonstration
 */
class SimulatedCamera {
private:
    std::atomic<bool> running_{false};
    std::thread captureThread_;
    FrameBuffer* outputBuffer_;
    int width_, height_;
    double targetFPS_;

public:
    SimulatedCamera(int width, int height, double targetFPS = 30.0) 
        : width_(width), height_(height), targetFPS_(targetFPS) {}
    
    void start(FrameBuffer& buffer) {
        outputBuffer_ = &buffer;
        running_ = true;
        
        captureThread_ = std::thread([this]() {
            auto frameInterval = microseconds(static_cast<long>(1000000.0 / targetFPS_));
            auto nextFrameTime = steady_clock::now();
            int frameCounter = 0;
            
            while (running_) {
                // Generate synthetic frame
                std::vector<uint8_t> frameData(width_ * height_ * 3);
                
                // Create animated pattern
                for (int y = 0; y < height_; ++y) {
                    for (int x = 0; x < width_; ++x) {
                        int index = (y * width_ + x) * 3;
                        
                        // Moving pattern based on frame counter
                        int r = ((x + frameCounter) % 256);
                        int g = ((y + frameCounter / 2) % 256);
                        int b = ((x + y + frameCounter) % 256);
                        
                        frameData[index] = static_cast<uint8_t>(r);
                        frameData[index + 1] = static_cast<uint8_t>(g);
                        frameData[index + 2] = static_cast<uint8_t>(b);
                    }
                }
                
                blob frame(frameData.data(), frameData.size());
                outputBuffer_->push(frame);
                
                frameCounter++;
                
                // Wait for next frame time
                nextFrameTime += frameInterval;
                std::this_thread::sleep_until(nextFrameTime);
            }
        });
    }
    
    void stop() {
        running_ = false;
        if (captureThread_.joinable()) {
            captureThread_.join();
        }
    }
};

/**
 * @brief Demonstrate basic real-time processing
 */
void demonstrateBasicRealtimeProcessing() {
    std::cout << "\n=== Basic Real-time Processing ===\n";
    
    try {
        const int width = 640;
        const int height = 480;
        const double targetFPS = 30.0;
        const int durationSeconds = 5;
        
        std::cout << "Starting " << durationSeconds << "s real-time processing demo\n";
        std::cout << "Target: " << targetFPS << " FPS, " << width << "x" << height << "\n";
        
        // Create components
        FrameBuffer inputBuffer(5);
        FrameBuffer outputBuffer(5);
        SimulatedCamera camera(width, height, targetFPS);
        ImageProcessor processor;
        PerformanceTracker tracker;
        
        std::atomic<bool> processingActive{true};
        std::atomic<size_t> framesProcessed{0};
        std::atomic<size_t> framesDropped{0};
        
        // Start camera
        camera.start(inputBuffer);
        
        // Processing thread
        std::thread processingThread([&]() {
            blob frame;
            while (processingActive) {
                if (inputBuffer.pop(frame, std::chrono::milliseconds(50))) {
                    auto start = steady_clock::now();
                    
                    // Apply simple filter
                    auto processed = processor.applyFilter(frame, FilterType::GAUSSIAN_BLUR,
                                                         {{"sigma", 1.0}, {"kernel_size", 3}});
                    
                    auto processingTime = duration_cast<microseconds>(steady_clock::now() - start);
                    double processingTimeMs = processingTime.count() / 1000.0;
                    
                    tracker.recordProcessingTime(processingTimeMs);
                    outputBuffer.push(processed);
                    framesProcessed++;
                } else {
                    // Check for dropped frames
                    if (inputBuffer.size() > 3) {
                        framesDropped++;
                    }
                }
            }
        });
        
        // Statistics thread
        std::thread statsThread([&]() {
            while (processingActive) {
                std::this_thread::sleep_for(std::chrono::seconds(1));
                
                auto stats = tracker.getStats();
                std::cout << "FPS: " << std::fixed << std::setprecision(1) << stats.avgFPS
                         << ", Processing: " << std::setprecision(2) << stats.avgProcessingTime << "ms"
                         << ", Load: " << (stats.processingLoad * 100) << "%"
                         << ", Processed: " << framesProcessed.load()
                         << ", Dropped: " << framesDropped.load() << "\n";
                
                tracker.recordFrame();
            }
        });
        
        // Run for specified duration
        std::this_thread::sleep_for(std::chrono::seconds(durationSeconds));
        
        // Cleanup
        processingActive = false;
        camera.stop();
        inputBuffer.shutdown();
        outputBuffer.shutdown();
        
        if (processingThread.joinable()) processingThread.join();
        if (statsThread.joinable()) statsThread.join();
        
        // Final statistics
        auto finalStats = tracker.getStats();
        std::cout << "\nFinal Statistics:\n";
        std::cout << "  Average FPS: " << std::fixed << std::setprecision(1) << finalStats.avgFPS << "\n";
        std::cout << "  Average processing time: " << std::setprecision(2) << finalStats.avgProcessingTime << "ms\n";
        std::cout << "  Max processing time: " << finalStats.maxProcessingTime << "ms\n";
        std::cout << "  Processing load: " << (finalStats.processingLoad * 100) << "%\n";
        std::cout << "  Total frames processed: " << framesProcessed.load() << "\n";
        std::cout << "  Frames dropped: " << framesDropped.load() << "\n";
        
    } catch (const std::exception& e) {
        std::cerr << "Error in basic real-time processing: " << e.what() << "\n";
    }
}

/**
 * @brief Demonstrate adaptive quality control
 */
void demonstrateAdaptiveQuality() {
    std::cout << "\n=== Adaptive Quality Control ===\n";
    
    try {
        const int width = 800;
        const int height = 600;
        const double targetFPS = 30.0;
        const int durationSeconds = 10;
        
        std::cout << "Testing adaptive quality control for " << durationSeconds << "s\n";
        
        FrameBuffer inputBuffer(3);
        SimulatedCamera camera(width, height, targetFPS);
        ImageProcessor processor;
        PerformanceTracker tracker;
        
        std::atomic<bool> active{true};
        std::atomic<int> qualityLevel{3}; // 1=low, 2=medium, 3=high
        std::atomic<size_t> qualityChanges{0};
        
        // Start camera
        camera.start(inputBuffer);
        
        // Adaptive processing thread
        std::thread processingThread([&]() {
            blob frame;
            std::vector<std::string> qualitySettings = {"low", "medium", "high"};
            
            while (active) {
                if (inputBuffer.pop(frame, std::chrono::milliseconds(33))) { // ~30 FPS timeout
                    auto start = steady_clock::now();
                    
                    // Apply processing based on current quality level
                    blob processed;
                    int currentQuality = qualityLevel.load();
                    
                    switch (currentQuality) {
                        case 1: // Low quality - fast processing
                            processed = processor.applyFilter(frame, FilterType::GAUSSIAN_BLUR,
                                                            {{"sigma", 0.5}, {"kernel_size", 3}});
                            break;
                        case 2: // Medium quality
                            processed = processor.applyFilter(frame, FilterType::GAUSSIAN_BLUR,
                                                            {{"sigma", 1.0}, {"kernel_size", 5}});
                            break;
                        case 3: // High quality - slower processing
                            processed = processor.applyFilter(frame, FilterType::GAUSSIAN_BLUR,
                                                            {{"sigma", 2.0}, {"kernel_size", 7}});
                            // Add additional processing
                            processed = processor.applyFilter(processed, FilterType::SHARPEN,
                                                            {{"strength", 0.5}});
                            break;
                    }
                    
                    auto processingTime = duration_cast<microseconds>(steady_clock::now() - start);
                    double processingTimeMs = processingTime.count() / 1000.0;
                    
                    tracker.recordProcessingTime(processingTimeMs);
                    tracker.recordFrame();
                }
            }
        });
        
        // Quality adaptation thread
        std::thread adaptationThread([&]() {
            const double targetFrameTime = 1000.0 / targetFPS; // ms
            const double lowThreshold = targetFrameTime * 0.8;   // 80% of target
            const double highThreshold = targetFrameTime * 1.2;  // 120% of target
            
            while (active) {
                std::this_thread::sleep_for(std::chrono::seconds(1));
                
                auto stats = tracker.getStats();
                int currentQuality = qualityLevel.load();
                
                std::cout << "Quality: " << currentQuality << " (" 
                         << (currentQuality == 1 ? "Low" : currentQuality == 2 ? "Medium" : "High")
                         << "), FPS: " << std::fixed << std::setprecision(1) << stats.avgFPS
                         << ", Processing: " << std::setprecision(2) << stats.avgProcessingTime << "ms\n";
                
                // Adapt quality based on performance
                if (stats.avgProcessingTime > highThreshold && currentQuality > 1) {
                    // Reduce quality to maintain frame rate
                    qualityLevel = currentQuality - 1;
                    qualityChanges++;
                    std::cout << "  -> Reducing quality to maintain frame rate\n";
                } else if (stats.avgProcessingTime < lowThreshold && currentQuality < 3) {
                    // Increase quality if we have headroom
                    qualityLevel = currentQuality + 1;
                    qualityChanges++;
                    std::cout << "  -> Increasing quality (performance headroom available)\n";
                }
            }
        });
        
        // Run for specified duration
        std::this_thread::sleep_for(std::chrono::seconds(durationSeconds));
        
        // Cleanup
        active = false;
        camera.stop();
        inputBuffer.shutdown();
        
        if (processingThread.joinable()) processingThread.join();
        if (adaptationThread.joinable()) adaptationThread.join();
        
        // Final statistics
        auto finalStats = tracker.getStats();
        std::cout << "\nAdaptive Quality Results:\n";
        std::cout << "  Final quality level: " << qualityLevel.load() << "\n";
        std::cout << "  Quality changes: " << qualityChanges.load() << "\n";
        std::cout << "  Average FPS: " << std::fixed << std::setprecision(1) << finalStats.avgFPS << "\n";
        std::cout << "  Average processing time: " << std::setprecision(2) << finalStats.avgProcessingTime << "ms\n";
        std::cout << "  Processing efficiency: " << (finalStats.processingLoad < 1.0 ? "Good" : "Overloaded") << "\n";
        
    } catch (const std::exception& e) {
        std::cerr << "Error in adaptive quality control: " << e.what() << "\n";
    }
}

/**
 * @brief Demonstrate multi-threaded pipeline
 */
void demonstrateMultiThreadedPipeline() {
    std::cout << "\n=== Multi-threaded Processing Pipeline ===\n";
    
    try {
        const int width = 640;
        const int height = 480;
        const double targetFPS = 60.0;
        const int durationSeconds = 5;
        const int numProcessingThreads = 3;
        
        std::cout << "Testing " << numProcessingThreads << "-thread pipeline for " << durationSeconds << "s\n";
        
        FrameBuffer inputBuffer(10);
        FrameBuffer outputBuffer(10);
        SimulatedCamera camera(width, height, targetFPS);
        
        std::vector<std::unique_ptr<ImageProcessor>> processors;
        for (int i = 0; i < numProcessingThreads; ++i) {
            processors.push_back(std::make_unique<ImageProcessor>());
        }
        
        PerformanceTracker tracker;
        std::atomic<bool> active{true};
        std::atomic<size_t> totalProcessed{0};
        
        // Start camera
        camera.start(inputBuffer);
        
        // Processing threads
        std::vector<std::thread> processingThreads;
        for (int i = 0; i < numProcessingThreads; ++i) {
            processingThreads.emplace_back([&, i]() {
                blob frame;
                size_t threadProcessed = 0;
                
                while (active) {
                    if (inputBuffer.pop(frame, std::chrono::milliseconds(50))) {
                        auto start = steady_clock::now();
                        
                        // Apply different processing on each thread
                        blob processed;
                        switch (i % 3) {
                            case 0:
                                processed = processors[i]->applyFilter(frame, FilterType::GAUSSIAN_BLUR,
                                                                     {{"sigma", 1.0}, {"kernel_size", 5}});
                                break;
                            case 1:
                                processed = processors[i]->applyFilter(frame, FilterType::SHARPEN,
                                                                     {{"strength", 0.3}});
                                break;
                            case 2:
                                processed = processors[i]->applyFilter(frame, FilterType::EDGE_DETECTION,
                                                                     {{"threshold", 100}});
                                break;
                        }
                        
                        auto processingTime = duration_cast<microseconds>(steady_clock::now() - start);
                        double processingTimeMs = processingTime.count() / 1000.0;
                        
                        tracker.recordProcessingTime(processingTimeMs);
                        outputBuffer.push(processed);
                        
                        threadProcessed++;
                        totalProcessed++;
                    }
                }
                
                std::cout << "Thread " << i << " processed " << threadProcessed << " frames\n";
            });
        }
        
        // Statistics thread
        std::thread statsThread([&]() {
            while (active) {
                std::this_thread::sleep_for(std::chrono::seconds(1));
                
                auto stats = tracker.getStats();
                std::cout << "Pipeline: " << std::fixed << std::setprecision(1) << stats.avgFPS << " FPS"
                         << ", Avg processing: " << std::setprecision(2) << stats.avgProcessingTime << "ms"
                         << ", Total processed: " << totalProcessed.load()
                         << ", Input buffer: " << inputBuffer.size()
                         << ", Output buffer: " << outputBuffer.size() << "\n";
                
                tracker.recordFrame();
            }
        });
        
        // Run for specified duration
        std::this_thread::sleep_for(std::chrono::seconds(durationSeconds));
        
        // Cleanup
        active = false;
        camera.stop();
        inputBuffer.shutdown();
        outputBuffer.shutdown();
        
        for (auto& thread : processingThreads) {
            if (thread.joinable()) thread.join();
        }
        if (statsThread.joinable()) statsThread.join();
        
        // Final statistics
        auto finalStats = tracker.getStats();
        std::cout << "\nMulti-threaded Pipeline Results:\n";
        std::cout << "  Processing threads: " << numProcessingThreads << "\n";
        std::cout << "  Total frames processed: " << totalProcessed.load() << "\n";
        std::cout << "  Average throughput: " << std::fixed << std::setprecision(1) << finalStats.avgFPS << " FPS\n";
        std::cout << "  Average processing time: " << std::setprecision(2) << finalStats.avgProcessingTime << "ms\n";
        std::cout << "  Theoretical speedup: " << (finalStats.avgFPS / (targetFPS / numProcessingThreads)) << "x\n";
        
    } catch (const std::exception& e) {
        std::cerr << "Error in multi-threaded pipeline: " << e.what() << "\n";
    }
}

/**
 * @brief Demonstrate performance optimization techniques
 */
void demonstratePerformanceOptimization() {
    std::cout << "\n=== Performance Optimization Techniques ===\n";
    
    try {
        const int width = 1280;
        const int height = 720;
        const int testFrames = 100;
        
        std::cout << "Testing optimization techniques with " << testFrames << " frames (" << width << "x" << height << ")\n";
        
        // Create test frame
        std::vector<uint8_t> frameData(width * height * 3);
        for (size_t i = 0; i < frameData.size(); ++i) {
            frameData[i] = static_cast<uint8_t>(i % 256);
        }
        blob testFrame(frameData.data(), frameData.size());
        
        ImageProcessor processor;
        
        // Test 1: Memory pre-allocation
        std::cout << "\nTest 1: Memory pre-allocation\n";
        
        auto start = steady_clock::now();
        for (int i = 0; i < testFrames; ++i) {
            auto result = processor.applyFilter(testFrame, FilterType::GAUSSIAN_BLUR,
                                              {{"sigma", 1.0}, {"kernel_size", 5}});
        }
        auto withoutPrealloc = duration_cast<milliseconds>(steady_clock::now() - start);
        
        // Pre-allocate buffers
        processor.preallocateBuffers(width, height, 3);
        
        start = steady_clock::now();
        for (int i = 0; i < testFrames; ++i) {
            auto result = processor.applyFilter(testFrame, FilterType::GAUSSIAN_BLUR,
                                              {{"sigma", 1.0}, {"kernel_size", 5}});
        }
        auto withPrealloc = duration_cast<milliseconds>(steady_clock::now() - start);
        
        std::cout << "  Without pre-allocation: " << withoutPrealloc.count() << "ms\n";
        std::cout << "  With pre-allocation: " << withPrealloc.count() << "ms\n";
        std::cout << "  Speedup: " << (static_cast<double>(withoutPrealloc.count()) / withPrealloc.count()) << "x\n";
        
        // Test 2: SIMD optimization
        std::cout << "\nTest 2: SIMD optimization\n";
        
        processor.enableSIMD(false);
        start = steady_clock::now();
        for (int i = 0; i < testFrames; ++i) {
            auto result = processor.applyFilter(testFrame, FilterType::GAUSSIAN_BLUR,
                                              {{"sigma", 1.0}, {"kernel_size", 5}});
        }
        auto withoutSIMD = duration_cast<milliseconds>(steady_clock::now() - start);
        
        processor.enableSIMD(true);
        start = steady_clock::now();
        for (int i = 0; i < testFrames; ++i) {
            auto result = processor.applyFilter(testFrame, FilterType::GAUSSIAN_BLUR,
                                              {{"sigma", 1.0}, {"kernel_size", 5}});
        }
        auto withSIMD = duration_cast<milliseconds>(steady_clock::now() - start);
        
        std::cout << "  Without SIMD: " << withoutSIMD.count() << "ms\n";
        std::cout << "  With SIMD: " << withSIMD.count() << "ms\n";
        std::cout << "  SIMD speedup: " << (static_cast<double>(withoutSIMD.count()) / withSIMD.count()) << "x\n";
        
        // Test 3: Cache optimization
        std::cout << "\nTest 3: Cache-friendly processing\n";
        
        // Process in tiles for better cache locality
        processor.setTileSize(64, 64);
        
        start = steady_clock::now();
        for (int i = 0; i < testFrames; ++i) {
            auto result = processor.applyFilter(testFrame, FilterType::GAUSSIAN_BLUR,
                                              {{"sigma", 1.0}, {"kernel_size", 5}});
        }
        auto tiledProcessing = duration_cast<milliseconds>(steady_clock::now() - start);
        
        std::cout << "  Tiled processing: " << tiledProcessing.count() << "ms\n";
        std::cout << "  Cache optimization benefit: " << 
                     (static_cast<double>(withSIMD.count()) / tiledProcessing.count()) << "x\n";
        
        // Overall optimization summary
        std::cout << "\nOptimization Summary:\n";
        std::cout << "  Baseline: " << withoutPrealloc.count() << "ms\n";
        std::cout << "  Fully optimized: " << tiledProcessing.count() << "ms\n";
        std::cout << "  Total speedup: " << (static_cast<double>(withoutPrealloc.count()) / tiledProcessing.count()) << "x\n";
        
    } catch (const std::exception& e) {
        std::cerr << "Error in performance optimization: " << e.what() << "\n";
    }
}

int main() {
    std::cout << "=== Atom Image Real-time Processing Demo ===\n";
    std::cout << "This example demonstrates real-time image processing techniques\n";

    // Run all demonstrations
    demonstrateBasicRealtimeProcessing();
    demonstrateAdaptiveQuality();
    demonstrateMultiThreadedPipeline();
    demonstratePerformanceOptimization();

    std::cout << "\n=== Real-time processing demo completed ===\n";
    std::cout << "\nKey techniques demonstrated:\n";
    std::cout << "- Live camera capture simulation and processing\n";
    std::cout << "- Real-time filtering with frame rate monitoring\n";
    std::cout << "- Adaptive quality control based on performance\n";
    std::cout << "- Multi-threaded processing pipeline\n";
    std::cout << "- Performance optimization techniques (SIMD, caching, pre-allocation)\n";
    std::cout << "- Thread-safe frame buffering and statistics tracking\n";
    
    return 0;
}
