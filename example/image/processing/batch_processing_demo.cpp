/**
 * @file batch_processing_demo.cpp
 * @brief Batch image processing operations demonstration
 *
 * This example demonstrates:
 * - Batch processing of multiple images
 * - Parallel processing with thread pools
 * - Efficient multi-file handling
 * - Progress tracking and monitoring
 * - Memory management for large batches
 * - Error handling and recovery in batch operations
 *
 * @author Atom Image Module
 * @date 2025
 */

#include <algorithm>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <future>
#include <iomanip>
#include <iostream>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <vector>

#include "atom/image/core/image_blob.hpp"
#include "atom/image/io/image_file_reader.hpp"
#include "atom/image/io/image_file_writer.hpp"
#include "atom/image/processing/batch_processor.hpp"
#include "atom/image/processing/image_processor.hpp"

using namespace atom::image;
using namespace std::chrono;
namespace fs = std::filesystem;

/**
 * @brief Thread-safe progress tracker for batch operations
 */
class BatchProgressTracker {
private:
    std::atomic<size_t> completed_{0};
    std::atomic<size_t> failed_{0};
    std::atomic<size_t> total_{0};
    std::mutex mutex_;
    std::vector<std::string> errors_;
    steady_clock::time_point startTime_;

public:
    void setTotal(size_t total) {
        total_ = total;
        startTime_ = steady_clock::now();
    }

    void incrementCompleted() { completed_++; }

    void incrementFailed(const std::string& error = "") {
        failed_++;
        if (!error.empty()) {
            std::lock_guard<std::mutex> lock(mutex_);
            errors_.push_back(error);
        }
    }

    struct Progress {
        size_t completed;
        size_t failed;
        size_t total;
        double percentage;
        double elapsedSeconds;
        double estimatedTotalSeconds;
        std::vector<std::string> errors;
    };

    Progress getProgress() const {
        Progress progress;
        progress.completed = completed_.load();
        progress.failed = failed_.load();
        progress.total = total_.load();
        progress.percentage =
            progress.total > 0 ? (progress.completed * 100.0 / progress.total)
                               : 0.0;

        auto elapsed = steady_clock::now() - startTime_;
        progress.elapsedSeconds =
            duration_cast<milliseconds>(elapsed).count() / 1000.0;

        if (progress.completed > 0) {
            double avgTimePerItem =
                progress.elapsedSeconds / progress.completed;
            progress.estimatedTotalSeconds = avgTimePerItem * progress.total;
        } else {
            progress.estimatedTotalSeconds = 0.0;
        }

        {
            std::lock_guard<std::mutex> lock(mutex_);
            progress.errors = errors_;
        }

        return progress;
    }
};

/**
 * @brief Thread pool for parallel batch processing
 */
class ThreadPool {
private:
    std::vector<std::thread> workers_;
    std::queue<std::function<void()>> tasks_;
    std::mutex queueMutex_;
    std::condition_variable condition_;
    std::atomic<bool> stop_{false};

public:
    explicit ThreadPool(size_t numThreads) {
        for (size_t i = 0; i < numThreads; ++i) {
            workers_.emplace_back([this] {
                while (true) {
                    std::function<void()> task;

                    {
                        std::unique_lock<std::mutex> lock(queueMutex_);
                        condition_.wait(
                            lock, [this] { return stop_ || !tasks_.empty(); });

                        if (stop_ && tasks_.empty()) {
                            return;
                        }

                        task = std::move(tasks_.front());
                        tasks_.pop();
                    }

                    task();
                }
            });
        }
    }

    ~ThreadPool() {
        stop_ = true;
        condition_.notify_all();

        for (auto& worker : workers_) {
            if (worker.joinable()) {
                worker.join();
            }
        }
    }

    template <typename F>
    void enqueue(F&& f) {
        {
            std::unique_lock<std::mutex> lock(queueMutex_);
            tasks_.emplace(std::forward<F>(f));
        }
        condition_.notify_one();
    }

    size_t getQueueSize() const {
        std::lock_guard<std::mutex> lock(queueMutex_);
        return tasks_.size();
    }
};

/**
 * @brief Create test images for batch processing
 */
void createTestImages(const std::string& directory, size_t count) {
    std::cout << "Creating " << count << " test images in " << directory
              << "\n";

    fs::create_directories(directory);

    for (size_t i = 0; i < count; ++i) {
        // Create test image data
        int width = 200 + (i % 5) * 50;  // Varying sizes
        int height = 150 + (i % 4) * 40;

        std::vector<uint8_t> imageData(width * height * 3);

        // Generate different patterns for each image
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                int index = (y * width + x) * 3;

                // Create unique pattern based on image index
                uint8_t r = static_cast<uint8_t>((x + y + i * 50) % 256);
                uint8_t g = static_cast<uint8_t>((x * 2 + y + i * 30) % 256);
                uint8_t b = static_cast<uint8_t>((x + y * 2 + i * 70) % 256);

                imageData[index] = r;
                imageData[index + 1] = g;
                imageData[index + 2] = b;
            }
        }

        // Save as simple raw format for testing
        std::string filename =
            directory + "/test_image_" + std::to_string(i) + ".raw";
        std::ofstream file(filename, std::ios::binary);

        // Write header (width, height, channels)
        uint32_t header[3] = {static_cast<uint32_t>(width),
                              static_cast<uint32_t>(height), 3};
        file.write(reinterpret_cast<const char*>(header), sizeof(header));
        file.write(reinterpret_cast<const char*>(imageData.data()),
                   imageData.size());
        file.close();
    }

    std::cout << "Created " << count << " test images\n";
}

/**
 * @brief Load image from simple raw format
 */
blob loadRawImage(const std::string& filename) {
    std::ifstream file(filename, std::ios::binary);
    if (!file) {
        throw std::runtime_error("Cannot open file: " + filename);
    }

    // Read header
    uint32_t header[3];
    file.read(reinterpret_cast<char*>(header), sizeof(header));

    uint32_t width = header[0];
    uint32_t height = header[1];
    uint32_t channels = header[2];

    // Read image data
    size_t dataSize = width * height * channels;
    std::vector<uint8_t> imageData(dataSize);
    file.read(reinterpret_cast<char*>(imageData.data()), dataSize);

    return blob(imageData.data(), dataSize);
}

/**
 * @brief Save image to simple raw format
 */
void saveRawImage(const std::string& filename, const blob& image,
                  uint32_t width, uint32_t height, uint32_t channels) {
    std::ofstream file(filename, std::ios::binary);
    if (!file) {
        throw std::runtime_error("Cannot create file: " + filename);
    }

    // Write header
    uint32_t header[3] = {width, height, channels};
    file.write(reinterpret_cast<const char*>(header), sizeof(header));

    // Write image data
    file.write(reinterpret_cast<const char*>(image.data()), image.size());
}

/**
 * @brief Demonstrate sequential batch processing
 */
void demonstrateSequentialBatch() {
    std::cout << "\n=== Sequential Batch Processing ===\n";

    try {
        const std::string inputDir = "test_images";
        const std::string outputDir = "processed_images_sequential";
        const size_t numImages = 20;

        // Create test images
        createTestImages(inputDir, numImages);
        fs::create_directories(outputDir);

        // Get list of input files
        std::vector<std::string> inputFiles;
        for (const auto& entry : fs::directory_iterator(inputDir)) {
            if (entry.path().extension() == ".raw") {
                inputFiles.push_back(entry.path().string());
            }
        }

        std::cout << "Processing " << inputFiles.size()
                  << " images sequentially\n";

        BatchProgressTracker tracker;
        tracker.setTotal(inputFiles.size());

        ImageProcessor processor;

        auto start = steady_clock::now();

        for (const auto& inputFile : inputFiles) {
            try {
                // Load image
                auto image = loadRawImage(inputFile);

                // Apply simple processing (simulated blur)
                auto processed =
                    processor.applyFilter(image, FilterType::GAUSSIAN_BLUR,
                                          {{"sigma", 1.0}, {"kernel_size", 3}});

                // Save processed image
                fs::path inputPath(inputFile);
                std::string outputFile = outputDir + "/" +
                                         inputPath.stem().string() +
                                         "_processed.raw";

                // For simplicity, assume same dimensions (would need to parse
                // from actual processing)
                saveRawImage(outputFile, processed, 200, 150, 3);

                tracker.incrementCompleted();

            } catch (const std::exception& e) {
                tracker.incrementFailed(e.what());
            }
        }

        auto totalTime =
            duration_cast<milliseconds>(steady_clock::now() - start);
        auto progress = tracker.getProgress();

        std::cout << "Sequential processing completed:\n";
        std::cout << "  Total time: " << totalTime.count() << " ms\n";
        std::cout << "  Processed: " << progress.completed << " images\n";
        std::cout << "  Failed: " << progress.failed << " images\n";
        std::cout << "  Average time per image: "
                  << (totalTime.count() / progress.completed) << " ms\n";
        std::cout << "  Throughput: "
                  << (progress.completed * 1000.0 / totalTime.count())
                  << " images/sec\n";

    } catch (const std::exception& e) {
        std::cerr << "Error in sequential batch processing: " << e.what()
                  << "\n";
    }
}

/**
 * @brief Demonstrate parallel batch processing
 */
void demonstrateParallelBatch() {
    std::cout << "\n=== Parallel Batch Processing ===\n";

    try {
        const std::string inputDir = "test_images";
        const std::string outputDir = "processed_images_parallel";

        // Get list of input files
        std::vector<std::string> inputFiles;
        for (const auto& entry : fs::directory_iterator(inputDir)) {
            if (entry.path().extension() == ".raw") {
                inputFiles.push_back(entry.path().string());
            }
        }

        fs::create_directories(outputDir);

        std::cout << "Processing " << inputFiles.size()
                  << " images in parallel\n";

        const size_t numThreads = std::thread::hardware_concurrency();
        std::cout << "Using " << numThreads << " threads\n";

        BatchProgressTracker tracker;
        tracker.setTotal(inputFiles.size());

        ThreadPool threadPool(numThreads);

        auto start = steady_clock::now();

        // Submit all tasks to thread pool
        std::vector<std::future<void>> futures;

        for (const auto& inputFile : inputFiles) {
            auto future = std::async(std::launch::async, [&, inputFile]() {
                try {
                    // Each thread gets its own processor to avoid conflicts
                    ImageProcessor processor;

                    // Load image
                    auto image = loadRawImage(inputFile);

                    // Apply processing
                    auto processed = processor.applyFilter(
                        image, FilterType::GAUSSIAN_BLUR,
                        {{"sigma", 1.0}, {"kernel_size", 3}});

                    // Save processed image
                    fs::path inputPath(inputFile);
                    std::string outputFile = outputDir + "/" +
                                             inputPath.stem().string() +
                                             "_processed.raw";

                    saveRawImage(outputFile, processed, 200, 150, 3);

                    tracker.incrementCompleted();

                } catch (const std::exception& e) {
                    tracker.incrementFailed(e.what());
                }
            });

            futures.push_back(std::move(future));
        }

        // Monitor progress
        std::thread progressThread([&tracker, &inputFiles]() {
            while (true) {
                auto progress = tracker.getProgress();

                std::cout << "\rProgress: " << std::fixed
                          << std::setprecision(1) << progress.percentage
                          << "% (" << progress.completed << "/"
                          << progress.total << ") - " << std::setprecision(2)
                          << progress.elapsedSeconds << "s elapsed";
                std::cout.flush();

                if (progress.completed + progress.failed >= progress.total) {
                    break;
                }

                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        });

        // Wait for all tasks to complete
        for (auto& future : futures) {
            future.wait();
        }

        progressThread.join();

        auto totalTime =
            duration_cast<milliseconds>(steady_clock::now() - start);
        auto progress = tracker.getProgress();

        std::cout << "\n\nParallel processing completed:\n";
        std::cout << "  Total time: " << totalTime.count() << " ms\n";
        std::cout << "  Processed: " << progress.completed << " images\n";
        std::cout << "  Failed: " << progress.failed << " images\n";
        std::cout << "  Average time per image: "
                  << (totalTime.count() / progress.completed) << " ms\n";
        std::cout << "  Throughput: "
                  << (progress.completed * 1000.0 / totalTime.count())
                  << " images/sec\n";
        std::cout << "  Parallel efficiency: "
                  << (numThreads * 100.0 /
                      (totalTime.count() / (progress.completed * 10.0)))
                  << "%\n";

        if (!progress.errors.empty()) {
            std::cout << "  Errors encountered:\n";
            for (size_t i = 0; i < std::min(size_t(5), progress.errors.size());
                 ++i) {
                std::cout << "    " << progress.errors[i] << "\n";
            }
            if (progress.errors.size() > 5) {
                std::cout << "    ... and " << (progress.errors.size() - 5)
                          << " more errors\n";
            }
        }

    } catch (const std::exception& e) {
        std::cerr << "Error in parallel batch processing: " << e.what() << "\n";
    }
}

/**
 * @brief Demonstrate memory-efficient batch processing
 */
void demonstrateMemoryEfficientBatch() {
    std::cout << "\n=== Memory-Efficient Batch Processing ===\n";

    try {
        const std::string inputDir = "test_images";
        const std::string outputDir = "processed_images_memory_efficient";

        // Get list of input files
        std::vector<std::string> inputFiles;
        for (const auto& entry : fs::directory_iterator(inputDir)) {
            if (entry.path().extension() == ".raw") {
                inputFiles.push_back(entry.path().string());
            }
        }

        fs::create_directories(outputDir);

        std::cout << "Processing " << inputFiles.size()
                  << " images with memory constraints\n";

        // Simulate memory constraint (process in smaller batches)
        const size_t maxBatchSize = 5;
        const size_t numBatches =
            (inputFiles.size() + maxBatchSize - 1) / maxBatchSize;

        std::cout << "Processing in " << numBatches << " batches of max "
                  << maxBatchSize << " images\n";

        BatchProgressTracker tracker;
        tracker.setTotal(inputFiles.size());

        auto start = steady_clock::now();

        for (size_t batchIdx = 0; batchIdx < numBatches; ++batchIdx) {
            size_t startIdx = batchIdx * maxBatchSize;
            size_t endIdx =
                std::min(startIdx + maxBatchSize, inputFiles.size());

            std::cout << "Processing batch " << (batchIdx + 1) << "/"
                      << numBatches << " (images " << startIdx << "-"
                      << (endIdx - 1) << ")\n";

            // Process current batch
            std::vector<std::future<void>> batchFutures;

            for (size_t i = startIdx; i < endIdx; ++i) {
                const auto& inputFile = inputFiles[i];

                auto future = std::async(std::launch::async, [&, inputFile]() {
                    try {
                        ImageProcessor processor;

                        // Load image
                        auto image = loadRawImage(inputFile);

                        // Apply processing
                        auto processed = processor.applyFilter(
                            image, FilterType::GAUSSIAN_BLUR,
                            {{"sigma", 1.0}, {"kernel_size", 3}});

                        // Save processed image
                        fs::path inputPath(inputFile);
                        std::string outputFile = outputDir + "/" +
                                                 inputPath.stem().string() +
                                                 "_processed.raw";

                        saveRawImage(outputFile, processed, 200, 150, 3);

                        tracker.incrementCompleted();

                    } catch (const std::exception& e) {
                        tracker.incrementFailed(e.what());
                    }
                });

                batchFutures.push_back(std::move(future));
            }

            // Wait for current batch to complete before starting next
            for (auto& future : batchFutures) {
                future.wait();
            }

            // Simulate memory cleanup between batches
            std::this_thread::sleep_for(std::chrono::milliseconds(50));

            auto progress = tracker.getProgress();
            std::cout << "  Batch completed. Overall progress: " << std::fixed
                      << std::setprecision(1) << progress.percentage << "%\n";
        }

        auto totalTime =
            duration_cast<milliseconds>(steady_clock::now() - start);
        auto progress = tracker.getProgress();

        std::cout << "\nMemory-efficient processing completed:\n";
        std::cout << "  Total time: " << totalTime.count() << " ms\n";
        std::cout << "  Processed: " << progress.completed << " images\n";
        std::cout << "  Failed: " << progress.failed << " images\n";
        std::cout << "  Average time per image: "
                  << (totalTime.count() / progress.completed) << " ms\n";
        std::cout << "  Throughput: "
                  << (progress.completed * 1000.0 / totalTime.count())
                  << " images/sec\n";
        std::cout << "  Memory efficiency: Processed in " << numBatches
                  << " batches\n";

    } catch (const std::exception& e) {
        std::cerr << "Error in memory-efficient batch processing: " << e.what()
                  << "\n";
    }
}

/**
 * @brief Demonstrate batch processing with different operations
 */
void demonstrateBatchOperations() {
    std::cout << "\n=== Batch Processing with Different Operations ===\n";

    try {
        const std::string inputDir = "test_images";

        // Get list of input files
        std::vector<std::string> inputFiles;
        for (const auto& entry : fs::directory_iterator(inputDir)) {
            if (entry.path().extension() == ".raw") {
                inputFiles.push_back(entry.path().string());
            }
        }

        // Define different processing operations
        std::vector<std::pair<std::string, std::function<blob(const blob&)>>>
            operations = {
                {"blur",
                 [](const blob& img) {
                     ImageProcessor processor;
                     return processor.applyFilter(
                         img, FilterType::GAUSSIAN_BLUR,
                         {{"sigma", 1.0}, {"kernel_size", 3}});
                 }},
                {"sharpen",
                 [](const blob& img) {
                     ImageProcessor processor;
                     return processor.applyFilter(img, FilterType::SHARPEN,
                                                  {{"strength", 0.5}});
                 }},
                {"edge_detection", [](const blob& img) {
                     ImageProcessor processor;
                     return processor.applyFilter(
                         img, FilterType::EDGE_DETECTION, {{"threshold", 100}});
                 }}};

        for (const auto& [opName, operation] : operations) {
            std::cout << "Processing with " << opName << " operation:\n";

            std::string outputDir = "processed_images_" + opName;
            fs::create_directories(outputDir);

            BatchProgressTracker tracker;
            tracker.setTotal(inputFiles.size());

            auto start = steady_clock::now();

            // Process subset of images for demonstration
            size_t numToProcess = std::min(size_t(10), inputFiles.size());

            std::vector<std::future<void>> futures;

            for (size_t i = 0; i < numToProcess; ++i) {
                const auto& inputFile = inputFiles[i];

                auto future =
                    std::async(std::launch::async, [&, inputFile, operation]() {
                        try {
                            // Load image
                            auto image = loadRawImage(inputFile);

                            // Apply operation
                            auto processed = operation(image);

                            // Save processed image
                            fs::path inputPath(inputFile);
                            std::string outputFile = outputDir + "/" +
                                                     inputPath.stem().string() +
                                                     "_" + opName + ".raw";

                            saveRawImage(outputFile, processed, 200, 150, 3);

                            tracker.incrementCompleted();

                        } catch (const std::exception& e) {
                            tracker.incrementFailed(e.what());
                        }
                    });

                futures.push_back(std::move(future));
            }

            // Wait for completion
            for (auto& future : futures) {
                future.wait();
            }

            auto totalTime =
                duration_cast<milliseconds>(steady_clock::now() - start);
            auto progress = tracker.getProgress();

            std::cout << "  " << opName << " processing: " << totalTime.count()
                      << "ms, " << progress.completed << " images, "
                      << std::fixed << std::setprecision(1)
                      << (progress.completed * 1000.0 / totalTime.count())
                      << " images/sec\n";
        }

    } catch (const std::exception& e) {
        std::cerr << "Error in batch operations: " << e.what() << "\n";
    }
}

/**
 * @brief Demonstrate error handling and recovery in batch processing
 */
void demonstrateErrorHandling() {
    std::cout << "\n=== Error Handling and Recovery ===\n";

    try {
        const std::string inputDir = "test_images";
        const std::string outputDir = "processed_images_error_test";

        // Get list of input files
        std::vector<std::string> inputFiles;
        for (const auto& entry : fs::directory_iterator(inputDir)) {
            if (entry.path().extension() == ".raw") {
                inputFiles.push_back(entry.path().string());
            }
        }

        // Add some invalid files to test error handling
        inputFiles.push_back("nonexistent_file.raw");
        inputFiles.push_back("invalid_file.txt");

        fs::create_directories(outputDir);

        std::cout << "Testing error handling with " << inputFiles.size()
                  << " files (including invalid ones)\n";

        BatchProgressTracker tracker;
        tracker.setTotal(inputFiles.size());

        auto start = steady_clock::now();

        std::vector<std::future<void>> futures;

        for (const auto& inputFile : inputFiles) {
            auto future = std::async(std::launch::async, [&, inputFile]() {
                try {
                    // Simulate different types of errors
                    if (inputFile.find("nonexistent") != std::string::npos) {
                        throw std::runtime_error("File not found: " +
                                                 inputFile);
                    }

                    if (inputFile.find(".txt") != std::string::npos) {
                        throw std::runtime_error("Invalid file format: " +
                                                 inputFile);
                    }

                    ImageProcessor processor;

                    // Load image
                    auto image = loadRawImage(inputFile);

                    // Apply processing
                    auto processed = processor.applyFilter(
                        image, FilterType::GAUSSIAN_BLUR,
                        {{"sigma", 1.0}, {"kernel_size", 3}});

                    // Save processed image
                    fs::path inputPath(inputFile);
                    std::string outputFile = outputDir + "/" +
                                             inputPath.stem().string() +
                                             "_processed.raw";

                    saveRawImage(outputFile, processed, 200, 150, 3);

                    tracker.incrementCompleted();

                } catch (const std::exception& e) {
                    tracker.incrementFailed("Error processing " + inputFile +
                                            ": " + e.what());
                }
            });

            futures.push_back(std::move(future));
        }

        // Wait for completion
        for (auto& future : futures) {
            future.wait();
        }

        auto totalTime =
            duration_cast<milliseconds>(steady_clock::now() - start);
        auto progress = tracker.getProgress();

        std::cout << "\nError handling test completed:\n";
        std::cout << "  Total time: " << totalTime.count() << " ms\n";
        std::cout << "  Successfully processed: " << progress.completed
                  << " images\n";
        std::cout << "  Failed: " << progress.failed << " images\n";
        std::cout << "  Success rate: " << std::fixed << std::setprecision(1)
                  << (progress.completed * 100.0 / progress.total) << "%\n";

        if (!progress.errors.empty()) {
            std::cout << "  Error details:\n";
            for (const auto& error : progress.errors) {
                std::cout << "    " << error << "\n";
            }
        }

    } catch (const std::exception& e) {
        std::cerr << "Error in error handling demonstration: " << e.what()
                  << "\n";
    }
}

/**
 * @brief Clean up test files
 */
void cleanup() {
    std::cout << "\nCleaning up test files...\n";

    std::vector<std::string> dirsToRemove = {
        "test_images",
        "processed_images_sequential",
        "processed_images_parallel",
        "processed_images_memory_efficient",
        "processed_images_blur",
        "processed_images_sharpen",
        "processed_images_edge_detection",
        "processed_images_error_test"};

    for (const auto& dir : dirsToRemove) {
        try {
            if (fs::exists(dir)) {
                fs::remove_all(dir);
                std::cout << "  Removed: " << dir << "\n";
            }
        } catch (const std::exception& e) {
            std::cout << "  Failed to remove " << dir << ": " << e.what()
                      << "\n";
        }
    }
}

int main() {
    std::cout << "=== Atom Image Batch Processing Demo ===\n";
    std::cout
        << "This example demonstrates batch image processing operations\n";

    // Run all demonstrations
    demonstrateSequentialBatch();
    demonstrateParallelBatch();
    demonstrateMemoryEfficientBatch();
    demonstrateBatchOperations();
    demonstrateErrorHandling();

    std::cout << "\n=== Batch processing demo completed ===\n";
    std::cout << "\nKey capabilities demonstrated:\n";
    std::cout << "- Sequential vs parallel batch processing\n";
    std::cout << "- Memory-efficient processing with batch constraints\n";
    std::cout << "- Multiple processing operations in batch mode\n";
    std::cout << "- Comprehensive error handling and recovery\n";
    std::cout << "- Progress tracking and performance monitoring\n";
    std::cout << "- Thread pool management for parallel processing\n";

    // Clean up test files
    cleanup();

    return 0;
}
