/**
 * @file image_loader_example.cpp
 * @brief Example demonstrating image loading capabilities
 *
 * This example covers:
 * - Loading images from files
 * - Loading with various options
 * - Batch loading multiple images
 * - Asynchronous loading
 * - Cache management
 * - Format-specific loading
 * - Progress tracking
 *
 * @author Atom Image Module
 * @date 2025
 */

#include <future>
#include <iostream>
#include <vector>

#include "atom/image/core/image_blob.hpp"
#include "atom/image/io/image_loader.hpp"

using namespace atom::image;
using namespace std;

/**
 * @brief Demonstrate basic image loading
 */
void demonstrateBasicLoading() {
    cout << "\n=== Basic Image Loading ===\n";

    ImageLoader loader;

    // Load image with default options
    auto result = loader.loadFromFile("test.jpg");

    if (result.success) {
        cout << "Successfully loaded image\n";
        cout << "  Size: " << result.image.width() << "x"
             << result.image.height() << "\n";
        cout << "  Channels: " << result.image.channels() << "\n";
        cout << "  Format: " << static_cast<int>(result.detectedFormat) << "\n";
        cout << "  Load time: " << result.loadTime.count() << " ms\n";
    } else {
        cout << "Failed to load image: " << result.errorMessage << "\n";
    }
}

/**
 * @brief Demonstrate loading with options
 */
void demonstrateLoadingOptions() {
    cout << "\n=== Loading with Options ===\n";

    ImageLoader loader;

    LoadOptions options;
    options.convertToRGB = true;
    options.normalizePixels = true;
    options.targetWidth = 800;
    options.targetHeight = 600;
    options.preserveAspectRatio = true;

    auto result = loader.loadFromFile("test.jpg", options);

    if (result.success) {
        cout << "Loaded with options:\n";
        cout << "  Final size: " << result.image.width() << "x"
             << result.image.height() << "\n";
        cout << "  Normalized: " << (options.normalizePixels ? "Yes" : "No")
             << "\n";
    }
}

/**
 * @brief Demonstrate batch loading
 */
void demonstrateBatchLoading() {
    cout << "\n=== Batch Loading ===\n";

    ImageLoader loader;

    vector<filesystem::path> files = {"image1.jpg", "image2.png",
                                      "image3.tiff"};

    auto batchResult = loader.loadBatch(files);

    cout << "Batch loading results:\n";
    cout << "  Success: " << batchResult.successCount << "\n";
    cout << "  Failures: " << batchResult.failureCount << "\n";
    cout << "  Total time: " << batchResult.totalTime.count() << " ms\n";

    for (size_t i = 0; i < batchResult.results.size(); ++i) {
        const auto& result = batchResult.results[i];
        cout << "  File " << i << ": " << (result.success ? "OK" : "FAILED")
             << "\n";
    }
}

/**
 * @brief Demonstrate asynchronous loading
 */
void demonstrateAsyncLoading() {
    cout << "\n=== Asynchronous Loading ===\n";

    ImageLoader loader;

    // Start async load
    auto future = loader.loadAsync("large_image.jpg");

    cout << "Loading in background...\n";

    // Do other work while loading
    cout << "Doing other work...\n";

    // Wait for result
    auto result = future.get();

    if (result.success) {
        cout << "Async load completed: " << result.image.width() << "x"
             << result.image.height() << "\n";
    }
}

/**
 * @brief Demonstrate cache management
 */
void demonstrateCacheManagement() {
    cout << "\n=== Cache Management ===\n";

    ImageLoader loader;

    // Set cache size
    loader.setCacheSize(100 * 1024 * 1024);  // 100MB

    // Load same image multiple times
    for (int i = 0; i < 3; ++i) {
        auto result = loader.loadFromFile("test.jpg");
        cout << "Load " << (i + 1) << " completed\n";
    }

    // Get cache statistics
    auto stats = loader.getCacheStats();
    cout << "\nCache statistics:\n";
    for (const auto& [key, value] : stats) {
        cout << "  " << key << ": " << value << "\n";
    }

    // Clear cache
    loader.clearCache();
    cout << "Cache cleared\n";
}

/**
 * @brief Demonstrate progress tracking
 */
void demonstrateProgressTracking() {
    cout << "\n=== Progress Tracking ===\n";

    ImageLoader loader;

    auto progressCallback = [](double progress, const string& message) {
        cout << "Progress: " << int(progress * 100) << "% - " << message
             << "\n";
    };

    LoadOptions options;
    auto result = loader.loadFromFile("test.jpg", options, progressCallback);

    cout << "Loading completed\n";
}

/**
 * @brief Demonstrate quick loading functions
 */
void demonstrateQuickLoading() {
    cout << "\n=== Quick Loading Functions ===\n";

    // Quick single image load
    auto img = quickLoadImage("test.jpg");
    if (!img.empty()) {
        cout << "Quick loaded: " << img.width() << "x" << img.height() << "\n";
    }

    // Quick batch load
    vector<filesystem::path> files = {"image1.jpg", "image2.png"};
    auto images = quickLoadBatch(files, 2);
    cout << "Quick batch loaded " << images.size() << " images\n";
}

/**
 * @brief Demonstrate format checking
 */
void demonstrateFormatChecking() {
    cout << "\n=== Format Checking ===\n";

    ImageLoader loader;

    // Check if file can be loaded
    if (loader.canLoad("test.jpg")) {
        cout << "test.jpg can be loaded\n";
    }

    // Get supported formats
    auto formats = loader.getSupportedFormats();
    cout << "Supported formats: " << formats.size() << "\n";
}

int main() {
    demonstrateBasicLoading();
    demonstrateLoadingOptions();
    demonstrateBatchLoading();
    demonstrateAsyncLoading();
    demonstrateCacheManagement();
    demonstrateProgressTracking();
    demonstrateQuickLoading();
    demonstrateFormatChecking();

    return 0;
}
