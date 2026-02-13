/**
 * @file image_saver_example.cpp
 * @brief Example demonstrating image saving capabilities
 *
 * This example covers:
 * - Saving images to files
 * - Saving with various options and quality settings
 * - Batch saving multiple images
 * - Asynchronous saving
 * - Format conversion
 * - Compression options
 * - Progress tracking
 *
 * @author Atom Image Module
 * @date 2025
 */

#include <future>
#include <iostream>
#include <vector>

#include "atom/image/core/image_blob.hpp"
#include "atom/image/io/image_saver.hpp"

using namespace atom::image;
using namespace std;

/**
 * @brief Create a test image
 */
blob createTestImage() {
    blob img(480, 640, 3);
    // Fill with test pattern
    return img;
}

/**
 * @brief Demonstrate basic image saving
 */
void demonstrateBasicSaving() {
    cout << "\n=== Basic Image Saving ===\n";

    ImageSaver saver;
    auto img = createTestImage();

    auto result = saver.saveToFile(img, "output.jpg");

    if (result.success) {
        cout << "Successfully saved image\n";
        cout << "  Output size: " << result.outputSize << " bytes\n";
        cout << "  Save time: " << result.saveTime.count() << " ms\n";
        cout << "  Compression ratio: " << result.compressionRatio << "\n";
    } else {
        cout << "Failed to save image: " << result.errorMessage << "\n";
    }
}

/**
 * @brief Demonstrate saving with options
 */
void demonstrateSavingOptions() {
    cout << "\n=== Saving with Options ===\n";

    ImageSaver saver;
    auto img = createTestImage();

    SaveOptions options;
    options.quality = 95;
    options.format = ImageFormat::JPEG;
    options.enableCompression = true;
    options.compressionLevel = 9;
    options.preserveMetadata = true;

    auto result = saver.saveToFile(img, "output_quality.jpg", options);

    if (result.success) {
        cout << "Saved with quality " << options.quality << "\n";
        cout << "  Output size: " << result.outputSize << " bytes\n";
    }
}

/**
 * @brief Demonstrate format conversion
 */
void demonstrateFormatConversion() {
    cout << "\n=== Format Conversion ===\n";

    ImageSaver saver;
    auto img = createTestImage();

    // Save as different formats
    vector<pair<string, ImageFormat>> formats = {
        {"output.jpg", ImageFormat::JPEG},
        {"output.png", ImageFormat::PNG},
        {"output.tiff", ImageFormat::TIFF},
        {"output.bmp", ImageFormat::BMP}};

    for (const auto& [filename, format] : formats) {
        SaveOptions options;
        options.format = format;

        auto result = saver.saveToFile(img, filename, options);

        if (result.success) {
            cout << filename << ": " << result.outputSize << " bytes\n";
        }
    }
}

/**
 * @brief Demonstrate batch saving
 */
void demonstrateBatchSaving() {
    cout << "\n=== Batch Saving ===\n";

    ImageSaver saver;

    // Create multiple images
    vector<blob> images;
    vector<filesystem::path> filenames;

    for (int i = 0; i < 3; ++i) {
        images.push_back(createTestImage());
        filenames.push_back("output_" + to_string(i) + ".jpg");
    }

    auto batchResult = saver.saveBatch(images, filenames);

    cout << "Batch saving results:\n";
    cout << "  Success: " << batchResult.successCount << "\n";
    cout << "  Failures: " << batchResult.failureCount << "\n";
    cout << "  Total time: " << batchResult.totalTime.count() << " ms\n";
    cout << "  Total output size: " << batchResult.totalOutputSize
         << " bytes\n";
}

/**
 * @brief Demonstrate asynchronous saving
 */
void demonstrateAsyncSaving() {
    cout << "\n=== Asynchronous Saving ===\n";

    ImageSaver saver;
    auto img = createTestImage();

    // Start async save
    auto future = saver.saveAsync(img, "output_async.jpg");

    cout << "Saving in background...\n";

    // Do other work
    cout << "Doing other work...\n";

    // Wait for result
    auto result = future.get();

    if (result.success) {
        cout << "Async save completed: " << result.outputSize << " bytes\n";
    }
}

/**
 * @brief Demonstrate compression levels
 */
void demonstrateCompressionLevels() {
    cout << "\n=== Compression Levels ===\n";

    ImageSaver saver;
    auto img = createTestImage();

    vector<int> compressionLevels = {1, 5, 9};

    for (int level : compressionLevels) {
        SaveOptions options;
        options.format = ImageFormat::PNG;
        options.compressionLevel = level;

        auto result = saver.saveToFile(
            img, "output_comp_" + to_string(level) + ".png", options);

        if (result.success) {
            cout << "Compression level " << level << ": " << result.outputSize
                 << " bytes\n";
        }
    }
}

/**
 * @brief Demonstrate progress tracking
 */
void demonstrateProgressTracking() {
    cout << "\n=== Progress Tracking ===\n";

    ImageSaver saver;
    auto img = createTestImage();

    auto progressCallback = [](double progress, const string& message) {
        cout << "Progress: " << int(progress * 100) << "% - " << message
             << "\n";
    };

    SaveOptions options;
    auto result =
        saver.saveToFile(img, "output_progress.jpg", options, progressCallback);

    cout << "Saving completed\n";
}

/**
 * @brief Demonstrate quick saving functions
 */
void demonstrateQuickSaving() {
    cout << "\n=== Quick Saving Functions ===\n";

    auto img = createTestImage();

    // Quick single image save
    bool success = quickSaveImage(img, "output_quick.jpg", 90);
    cout << "Quick save: " << (success ? "Success" : "Failed") << "\n";

    // Quick batch save
    vector<blob> images = {createTestImage(), createTestImage()};
    vector<filesystem::path> files = {"quick1.jpg", "quick2.jpg"};
    size_t saved = quickSaveBatch(images, files, 90, 2);
    cout << "Quick batch saved " << saved << " images\n";
}

int main() {
    demonstrateBasicSaving();
    demonstrateSavingOptions();
    demonstrateFormatConversion();
    demonstrateBatchSaving();
    demonstrateAsyncSaving();
    demonstrateCompressionLevels();
    demonstrateProgressTracking();
    demonstrateQuickSaving();

    return 0;
}
