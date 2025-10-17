#include "image_loader.hpp"
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <thread>

// Define error macros to avoid atom error system namespace pollution
#define THROW_RUNTIME_ERROR(msg) throw std::runtime_error(msg)
#include <execution>

#ifdef ATOM_IMAGE_HAS_OPENCV
#include <opencv2/imgcodecs.hpp>
#include <opencv2/opencv.hpp>
#endif

#ifdef ATOM_IMAGE_HAS_STB
#include <stb_image.h>
#endif

namespace atom::image {

ImageLoader::ImageLoader() : formatDetector_(createFormatDetector()) {}

LoadResult ImageLoader::loadFromFile(const std::filesystem::path& filePath,
                                     const LoadOptions& options,
                                     ProgressCallback progressCallback) const {
    auto startTime = std::chrono::high_resolution_clock::now();
    LoadResult result;

    try {
        if (progressCallback) {
            progressCallback(0.0f, "Starting file load");
        }

        // Check if file exists
        if (!std::filesystem::exists(filePath)) {
            result.errorMessage = "File does not exist: " + filePath.string();
            return result;
        }

        // Check cache first
        std::string cacheKey =
            options.cacheKey.empty() ? filePath.string() : options.cacheKey;
        if (options.enableCaching) {
            auto cacheIt = imageCache_.find(cacheKey);
            if (cacheIt != imageCache_.end()) {
                result.imageData = cacheIt->second;
                result.success = true;
                result.detectedFormat =
                    formatDetector_->detectFromFile(filePath).format;
                cacheHits_++;

                if (progressCallback) {
                    progressCallback(1.0f, "Loaded from cache");
                }
                return result;
            }
            cacheMisses_++;
        }

        if (progressCallback) {
            progressCallback(0.1f, "Reading file");
        }

        // Read file into memory
        std::ifstream file(filePath, std::ios::binary | std::ios::ate);
        if (!file.is_open()) {
            result.errorMessage = "Cannot open file: " + filePath.string();
            return result;
        }

        size_t fileSize = static_cast<size_t>(file.tellg());
        file.seekg(0, std::ios::beg);

        std::vector<uint8_t> fileData(fileSize);
        file.read(reinterpret_cast<char*>(fileData.data()), fileSize);

        if (progressCallback) {
            progressCallback(0.3f, "Detecting format");
        }

        // Detect format
        auto detectionResult =
            formatDetector_->detectFromMemory(fileData.data(), fileSize);
        result.detectedFormat = detectionResult.format;
        result.metadata = detectionResult.metadata;

        if (detectionResult.format == ImageFormat::UNKNOWN) {
            result.errorMessage = "Unknown or unsupported image format";
            return result;
        }

        if (progressCallback) {
            progressCallback(0.5f, "Loading image data");
        }

        // Load the image
        auto loadResult = loadFormat(fileData.data(), fileSize,
                                     detectionResult.format, options);
        result.imageData = loadResult.imageData;
        result.success = loadResult.success;
        result.errorMessage = loadResult.errorMessage;

        if (result.success) {
            if (progressCallback) {
                progressCallback(0.8f, "Applying post-processing");
            }

            // Apply post-processing
            result.imageData = applyPostProcessing(result.imageData, options);

            // Cache the result
            if (options.enableCaching &&
                cacheSize_ + result.imageData.size() <= maxCacheSize_) {
                imageCache_[cacheKey] = result.imageData;
                cacheSize_ += result.imageData.size();
            }

            if (progressCallback) {
                progressCallback(1.0f, "Load complete");
            }
        }

    } catch (const std::exception& e) {
        result.errorMessage = e.what();
        result.success = false;
    }

    auto endTime = std::chrono::high_resolution_clock::now();
    result.loadTime = std::chrono::duration_cast<std::chrono::milliseconds>(
        endTime - startTime);

    return result;
}

LoadResult ImageLoader::loadFromMemory(
    const void* data, size_t size, const LoadOptions& options,
    ProgressCallback progressCallback) const {
    auto startTime = std::chrono::high_resolution_clock::now();
    LoadResult result;

    try {
        if (progressCallback) {
            progressCallback(0.0f, "Starting memory load");
        }

        if (!data || size == 0) {
            result.errorMessage = "Invalid data or size";
            return result;
        }

        if (progressCallback) {
            progressCallback(0.2f, "Detecting format");
        }

        // Detect format
        auto detectionResult = formatDetector_->detectFromMemory(data, size);
        result.detectedFormat = detectionResult.format;
        result.metadata = detectionResult.metadata;

        if (detectionResult.format == ImageFormat::UNKNOWN) {
            result.errorMessage = "Unknown or unsupported image format";
            return result;
        }

        if (progressCallback) {
            progressCallback(0.5f, "Loading image data");
        }

        // Load the image
        auto loadResult =
            loadFormat(data, size, detectionResult.format, options);
        result.imageData = loadResult.imageData;
        result.success = loadResult.success;
        result.errorMessage = loadResult.errorMessage;

        if (result.success) {
            if (progressCallback) {
                progressCallback(0.8f, "Applying post-processing");
            }

            // Apply post-processing
            result.imageData = applyPostProcessing(result.imageData, options);

            if (progressCallback) {
                progressCallback(1.0f, "Load complete");
            }
        }

    } catch (const std::exception& e) {
        result.errorMessage = e.what();
        result.success = false;
    }

    auto endTime = std::chrono::high_resolution_clock::now();
    result.loadTime = std::chrono::duration_cast<std::chrono::milliseconds>(
        endTime - startTime);

    return result;
}

LoadResult ImageLoader::loadFromURL(const std::string& url,
                                    const LoadOptions& options,
                                    ProgressCallback progressCallback) const {
    LoadResult result;
    result.errorMessage = "URL loading not implemented yet";
    result.success = false;
    return result;
}

BatchLoadResult ImageLoader::loadBatch(
    const std::vector<std::filesystem::path>& filePaths,
    const LoadOptions& options, size_t maxConcurrency,
    ProgressCallback progressCallback) const {
    auto startTime = std::chrono::high_resolution_clock::now();
    BatchLoadResult batchResult;
    batchResult.results.resize(filePaths.size());

    // Limit concurrency to reasonable bounds
    maxConcurrency = (maxConcurrency < std::thread::hardware_concurrency())
                         ? maxConcurrency
                         : std::thread::hardware_concurrency();
    maxConcurrency = (maxConcurrency > size_t(1)) ? maxConcurrency : size_t(1);

    std::atomic<size_t> completedCount{0};
    std::atomic<size_t> successCount{0};
    std::atomic<size_t> failureCount{0};

    // Process files in parallel
    std::for_each(
        std::execution::par_unseq, filePaths.begin(), filePaths.end(),
        [&](const auto& filePath) {
            size_t index = &filePath - &filePaths[0];

            auto result = loadFromFile(filePath, options);
            batchResult.results[index] = result;

            if (result.success) {
                successCount++;
            } else {
                failureCount++;
            }

            completedCount++;

            if (progressCallback) {
                float progress = static_cast<float>(completedCount.load()) /
                                 filePaths.size();
                progressCallback(
                    progress,
                    "Processed " + std::to_string(completedCount.load()) +
                        " of " + std::to_string(filePaths.size()) + " files");
            }
        });

    batchResult.successCount = successCount;
    batchResult.failureCount = failureCount;

    auto endTime = std::chrono::high_resolution_clock::now();
    batchResult.totalTime =
        std::chrono::duration_cast<std::chrono::milliseconds>(endTime -
                                                              startTime);

    return batchResult;
}

std::future<LoadResult> ImageLoader::loadAsync(
    const std::filesystem::path& filePath, const LoadOptions& options,
    ProgressCallback progressCallback) const {
    return std::async(
        std::launch::async, [this, filePath, options, progressCallback]() {
            return loadFromFile(filePath, options, progressCallback);
        });
}

bool ImageLoader::canLoad(const std::filesystem::path& filePath) const {
    if (!std::filesystem::exists(filePath)) {
        return false;
    }

    auto detectionResult = formatDetector_->detectFromFile(filePath);
    return detectionResult.format != ImageFormat::UNKNOWN &&
           formatDetector_->isReadSupported(detectionResult.format);
}

std::vector<ImageFormat> ImageLoader::getSupportedFormats() const {
    return formatDetector_->getSupportedFormats();
}

void ImageLoader::setCacheSize(size_t maxSize) {
    maxCacheSize_ = maxSize;

    // Clear cache if current size exceeds new limit
    if (cacheSize_ > maxCacheSize_) {
        clearCache();
    }
}

void ImageLoader::clearCache() {
    imageCache_.clear();
    cacheSize_ = 0;
}

std::unordered_map<std::string, size_t> ImageLoader::getCacheStats() const {
    return {{"cache_hits", cacheHits_},
            {"cache_misses", cacheMisses_},
            {"cache_size_bytes", cacheSize_},
            {"cache_entries", imageCache_.size()},
            {"max_cache_size_bytes", maxCacheSize_}};
}

void ImageLoader::registerCustomLoader(
    ImageFormat format,
    std::function<LoadResult(const void*, size_t, const LoadOptions&)> loader) {
    customLoaders_[format] = loader;
}

LoadResult ImageLoader::loadFormat(const void* data, size_t size,
                                   ImageFormat format,
                                   const LoadOptions& options) const {
    LoadResult result;

    // Check for custom loader first
    auto customIt = customLoaders_.find(format);
    if (customIt != customLoaders_.end()) {
        return customIt->second(data, size, options);
    }

    // Use built-in loaders
    switch (format) {
#ifdef ATOM_IMAGE_HAS_OPENCV
        case ImageFormat::JPEG:
        case ImageFormat::PNG:
        case ImageFormat::BMP:
        case ImageFormat::TIFF: {
            std::vector<uint8_t> buffer(
                static_cast<const uint8_t*>(data),
                static_cast<const uint8_t*>(data) + size);
            cv::Mat mat = cv::imdecode(buffer, cv::IMREAD_UNCHANGED);

            if (mat.empty()) {
                result.errorMessage = "Failed to decode image with OpenCV";
                return result;
            }

            result.imageData = blob(mat);
            result.success = true;
            break;
        }
#endif

#ifdef ATOM_IMAGE_HAS_STB
        case ImageFormat::JPEG:
        case ImageFormat::PNG:
        case ImageFormat::BMP:
        case ImageFormat::TGA: {
            int width, height, channels;
            unsigned char* pixels = stbi_load_from_memory(
                static_cast<const unsigned char*>(data), static_cast<int>(size),
                &width, &height, &channels, 0);

            if (!pixels) {
                result.errorMessage =
                    "Failed to decode image with stb_image: " +
                    std::string(stbi_failure_reason());
                return result;
            }

            size_t imageSize = static_cast<size_t>(width) *
                               static_cast<size_t>(height) *
                               static_cast<size_t>(channels);
            result.imageData = blob(pixels, imageSize, height, width, channels);
            stbi_image_free(pixels);
            result.success = true;
            break;
        }
#endif

        default:
            result.errorMessage = "Unsupported format for loading: " +
                                  formatDetector_->getFormatName(format);
            break;
    }

    return result;
}

blob ImageLoader::applyPostProcessing(const blob& imageData,
                                      const LoadOptions& options) const {
    blob processed = imageData;

    // Apply resizing if requested
    if (options.targetWidth > 0 || options.targetHeight > 0) {
        int newWidth = options.targetWidth > 0 ? options.targetWidth
                                               : processed.getWidth();
        int newHeight = options.targetHeight > 0 ? options.targetHeight
                                                 : processed.getHeight();

        if (options.preserveAspectRatio && options.targetWidth > 0 &&
            options.targetHeight > 0) {
            float aspectRatio = static_cast<float>(processed.getWidth()) /
                                processed.getHeight();
            if (newWidth / aspectRatio <= newHeight) {
                newHeight = static_cast<int>(newWidth / aspectRatio);
            } else {
                newWidth = static_cast<int>(newHeight * aspectRatio);
            }
        }

#ifdef ATOM_IMAGE_HAS_OPENCV
        processed.resize(newHeight, newWidth);
#endif
    }

    return processed;
}

std::unordered_map<std::string, std::string> ImageLoader::extractMetadata(
    const void* data, size_t size, ImageFormat format) const {
    std::unordered_map<std::string, std::string> metadata;
    metadata["format"] = formatDetector_->getFormatName(format);
    metadata["size_bytes"] = std::to_string(size);

    // Format-specific metadata extraction could be added here

    return metadata;
}

std::unique_ptr<ImageLoader> createImageLoader() {
    return std::make_unique<ImageLoader>();
}

blob quickLoadImage(const std::filesystem::path& filePath) {
    auto loader = createImageLoader();
    auto result = loader->loadFromFile(filePath);
    return result.success ? result.imageData : blob{};
}

std::vector<blob> quickLoadBatch(
    const std::vector<std::filesystem::path>& filePaths,
    size_t maxConcurrency) {
    auto loader = createImageLoader();
    auto batchResult = loader->loadBatch(filePaths, {}, maxConcurrency);

    std::vector<blob> images;
    images.reserve(batchResult.results.size());

    for (const auto& result : batchResult.results) {
        images.push_back(result.success ? result.imageData : blob{});
    }

    return images;
}

}  // namespace atom::image
