#include "image_saver.hpp"
#include <fstream>
#include <thread>
#include <algorithm>
#include <chrono>
#include <execution>
#include <future>
#include <stdexcept>

// Define error macros to avoid atom error system namespace pollution
#define THROW_RUNTIME_ERROR(msg) throw std::runtime_error(msg)
#define THROW_FAIL_TO_OPEN_FILE(msg, path) throw std::runtime_error(std::string(msg) + std::string(path))
#define THROW_FAIL_TO_WRITE_FILE(msg, path) throw std::runtime_error(std::string(msg) + std::string(path))
#define THROW_INVALID_ARGUMENT(msg) throw std::invalid_argument(msg)

#ifdef ATOM_IMAGE_HAS_OPENCV
#include <opencv2/opencv.hpp>
#include <opencv2/imgcodecs.hpp>
#endif

#ifdef ATOM_IMAGE_HAS_STB
#include <stb_image_write.h>
#endif

namespace atom::image {

ImageSaver::ImageSaver() : formatDetector_(createFormatDetector()) {}

SaveResult ImageSaver::saveToFile(const blob& imageData,
                                 const std::filesystem::path& filePath,
                                 const SaveOptions& options,
                                 SaveProgressCallback progressCallback) const {
    auto startTime = std::chrono::high_resolution_clock::now();
    SaveResult result;
    
    try {
        if (progressCallback) {
            progressCallback(0.0f, "Starting file save");
        }
        
        // Validate input
        if (imageData.isEmpty()) {
            result.errorMessage = "Cannot save empty image data";
            return result;
        }
        
        // Create directories if needed
        if (options.createDirectories) {
            std::filesystem::create_directories(filePath.parent_path());
        }
        
        // Handle existing file backup
        if (std::filesystem::exists(filePath) && !options.overwriteExisting) {
            if (!createBackup(filePath, options.backupSuffix)) {
                result.errorMessage = "Failed to create backup of existing file";
                return result;
            }
        }
        
        // Determine target format
        ImageFormat targetFormat = options.targetFormat;
        if (targetFormat == ImageFormat::UNKNOWN) {
            std::string extension = filePath.extension().string();
            std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);
            
            if (extension == ".jpg" || extension == ".jpeg") targetFormat = ImageFormat::JPEG;
            else if (extension == ".png") targetFormat = ImageFormat::PNG;
            else if (extension == ".bmp") targetFormat = ImageFormat::BMP;
            else if (extension == ".tiff" || extension == ".tif") targetFormat = ImageFormat::TIFF;
            else if (extension == ".tga") targetFormat = ImageFormat::TGA;
            else if (extension == ".webp") targetFormat = ImageFormat::WEBP;
            else if (extension == ".fits") targetFormat = ImageFormat::FITS;
            else {
                result.errorMessage = "Unsupported file extension: " + extension;
                return result;
            }
        }
        
        if (progressCallback) {
            progressCallback(0.2f, "Format determined");
        }
        
        // Check if format is supported
        if (!canSave(targetFormat)) {
            result.errorMessage = "Format not supported for saving: " + std::to_string(static_cast<int>(targetFormat));
            return result;
        }
        
        // Apply preprocessing
        blob processedData = applyPreProcessing(imageData, options);
        
        if (progressCallback) {
            progressCallback(0.4f, "Preprocessing complete");
        }
        
        // Encode image data
        auto [encodedData, encodeResult] = saveFormat(processedData, targetFormat, options);
        if (!encodeResult.success) {
            result = encodeResult;
            return result;
        }
        
        if (progressCallback) {
            progressCallback(0.7f, "Encoding complete");
        }
        
        // Embed metadata if requested
        if (options.preserveMetadata && !options.customMetadata.empty()) {
            encodedData = embedMetadata(encodedData, targetFormat, options.customMetadata);
        }
        
        if (progressCallback) {
            progressCallback(0.8f, "Metadata embedded");
        }
        
        // Write to file
        std::ofstream file(filePath, std::ios::binary);
        if (!file) {
            THROW_FAIL_TO_OPEN_FILE("Cannot open file for writing: ", filePath.string());
        }

        file.write(reinterpret_cast<const char*>(encodedData.data()), encodedData.size());
        if (!file) {
            THROW_FAIL_TO_WRITE_FILE("Failed to write image data to file: ", filePath.string());
        }
        
        if (progressCallback) {
            progressCallback(1.0f, "Save complete");
        }
        
        // Calculate results
        auto endTime = std::chrono::high_resolution_clock::now();
        result.savedPath = filePath;
        result.usedFormat = targetFormat;
        result.outputSize = encodedData.size();
        result.success = true;
        result.saveTime = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
        result.compressionRatio = static_cast<float>(encodedData.size()) / static_cast<float>(imageData.size());
        
    } catch (const std::exception& e) {
        result.errorMessage = e.what();
        result.success = false;
    }
    
    return result;
}

std::pair<std::vector<uint8_t>, SaveResult> ImageSaver::saveToMemory(
    const blob& imageData,
    ImageFormat format,
    const SaveOptions& options,
    SaveProgressCallback progressCallback) const {
    
    SaveResult result;
    std::vector<uint8_t> encodedData;
    
    try {
        if (progressCallback) {
            progressCallback(0.0f, "Starting memory save");
        }
        
        // Validate input
        if (imageData.isEmpty()) {
            result.errorMessage = "Cannot save empty image data";
            return {encodedData, result};
        }
        
        // Check if format is supported
        if (!canSave(format)) {
            result.errorMessage = "Format not supported for saving";
            return {encodedData, result};
        }
        
        if (progressCallback) {
            progressCallback(0.2f, "Validation complete");
        }
        
        // Apply preprocessing
        blob processedData = applyPreProcessing(imageData, options);
        
        if (progressCallback) {
            progressCallback(0.4f, "Preprocessing complete");
        }
        
        // Encode image data
        auto [data, encodeResult] = saveFormat(processedData, format, options);
        if (!encodeResult.success) {
            return {encodedData, encodeResult};
        }
        
        encodedData = std::move(data);
        
        if (progressCallback) {
            progressCallback(0.8f, "Encoding complete");
        }
        
        // Embed metadata if requested
        if (options.preserveMetadata && !options.customMetadata.empty()) {
            encodedData = embedMetadata(encodedData, format, options.customMetadata);
        }
        
        if (progressCallback) {
            progressCallback(1.0f, "Save complete");
        }
        
        result.usedFormat = format;
        result.outputSize = encodedData.size();
        result.success = true;
        result.compressionRatio = static_cast<float>(encodedData.size()) / static_cast<float>(imageData.size());
        
    } catch (const std::exception& e) {
        result.errorMessage = e.what();
        result.success = false;
    }
    
    return {encodedData, result};
}

BatchSaveResult ImageSaver::saveBatch(const std::vector<blob>& imageData,
                                     const std::vector<std::filesystem::path>& filePaths,
                                     const SaveOptions& options,
                                     size_t maxConcurrency,
                                     SaveProgressCallback progressCallback) const {
    BatchSaveResult batchResult;
    auto startTime = std::chrono::high_resolution_clock::now();
    
    if (imageData.size() != filePaths.size()) {
        THROW_INVALID_ARGUMENT("Image data and file paths vectors must have the same size");
    }
    
    if (imageData.empty()) {
        return batchResult;
    }
    
    // Limit concurrency to reasonable bounds
    maxConcurrency = (maxConcurrency < std::thread::hardware_concurrency()) ? maxConcurrency : std::thread::hardware_concurrency();
    maxConcurrency = (maxConcurrency > size_t(1)) ? maxConcurrency : size_t(1);
    
    batchResult.results.resize(imageData.size());
    std::atomic<size_t> completedCount{0};
    
    // Process in batches
    std::vector<std::future<void>> futures;
    futures.reserve(maxConcurrency);
    
    for (size_t i = 0; i < imageData.size(); i += maxConcurrency) {
        size_t batchEnd = std::min(i + maxConcurrency, imageData.size());
        
        for (size_t j = i; j < batchEnd; ++j) {
            futures.emplace_back(std::async(std::launch::async, [this, &imageData, &filePaths, &options, &batchResult, &completedCount, &progressCallback, j]() {
                batchResult.results[j] = saveToFile(imageData[j], filePaths[j], options);
                
                size_t completed = ++completedCount;
                if (progressCallback) {
                    float progress = static_cast<float>(completed) / static_cast<float>(imageData.size());
                    progressCallback(progress, "Processed " + std::to_string(completed) + "/" + std::to_string(imageData.size()));
                }
            }));
        }
        
        // Wait for current batch to complete
        for (auto& future : futures) {
            future.wait();
        }
        futures.clear();
    }
    
    // Calculate batch statistics
    for (const auto& result : batchResult.results) {
        if (result.success) {
            batchResult.successCount++;
            batchResult.totalOutputSize += result.outputSize;
        } else {
            batchResult.failureCount++;
        }
    }
    
    auto endTime = std::chrono::high_resolution_clock::now();
    batchResult.totalTime = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
    
    return batchResult;
}

std::future<SaveResult> ImageSaver::saveAsync(const blob& imageData,
                                             const std::filesystem::path& filePath,
                                             const SaveOptions& options,
                                             SaveProgressCallback progressCallback) const {
    return std::async(std::launch::async, [this, imageData, filePath, options, progressCallback]() {
        return saveToFile(imageData, filePath, options, progressCallback);
    });
}

bool ImageSaver::canSave(ImageFormat format) const {
    switch (format) {
        case ImageFormat::JPEG:
        case ImageFormat::PNG:
        case ImageFormat::BMP:
        case ImageFormat::TGA:
#ifdef ATOM_IMAGE_HAS_OPENCV
        case ImageFormat::TIFF:
        case ImageFormat::WEBP:
#endif
#ifdef ATOM_IMAGE_HAS_CFITSIO
        case ImageFormat::FITS:
#endif
            return true;
        default:
            return customSavers_.find(format) != customSavers_.end();
    }
}

std::vector<ImageFormat> ImageSaver::getSupportedFormats() const {
    std::vector<ImageFormat> formats = {
        ImageFormat::JPEG,
        ImageFormat::PNG,
        ImageFormat::BMP,
        ImageFormat::TGA
    };

#ifdef ATOM_IMAGE_HAS_OPENCV
    formats.push_back(ImageFormat::TIFF);
    formats.push_back(ImageFormat::WEBP);
#endif

#ifdef ATOM_IMAGE_HAS_CFITSIO
    formats.push_back(ImageFormat::FITS);
#endif

    // Add custom formats
    for (const auto& [format, _] : customSavers_) {
        formats.push_back(format);
    }

    return formats;
}

ImageFormat ImageSaver::getOptimalFormat(const blob& imageData, bool preferLossless) const {
    if (imageData.isEmpty()) {
        return ImageFormat::UNKNOWN;
    }

    // Analyze image characteristics
    int channels = imageData.getChannels();
    bool hasAlpha = (channels == 4);

    if (preferLossless) {
        if (hasAlpha) {
            return ImageFormat::PNG;  // PNG supports alpha
        } else {
            return ImageFormat::PNG;  // PNG is generally good for lossless
        }
    } else {
        if (hasAlpha) {
            return ImageFormat::PNG;  // JPEG doesn't support alpha
        } else {
            return ImageFormat::JPEG; // JPEG is good for photos
        }
    }
}

size_t ImageSaver::estimateOutputSize(const blob& imageData,
                                     ImageFormat format,
                                     const SaveOptions& options) const {
    if (imageData.isEmpty()) {
        return 0;
    }

    size_t inputSize = imageData.size();

    switch (format) {
        case ImageFormat::JPEG: {
            // JPEG compression ratio depends on quality
            float compressionRatio = 0.1f + (static_cast<float>(options.quality) / 100.0f) * 0.4f;
            return static_cast<size_t>(inputSize * compressionRatio);
        }
        case ImageFormat::PNG: {
            // PNG compression varies, estimate conservatively
            return static_cast<size_t>(inputSize * 0.7f);
        }
        case ImageFormat::BMP: {
            // BMP is typically uncompressed
            return inputSize + 54; // BMP header size
        }
        case ImageFormat::TIFF: {
            // TIFF can be compressed or uncompressed
            if (options.compression == CompressionType::NONE) {
                return inputSize + 1024; // Estimate header overhead
            } else {
                return static_cast<size_t>(inputSize * 0.8f);
            }
        }
        default:
            return inputSize; // Conservative estimate
    }
}

void ImageSaver::registerCustomSaver(ImageFormat format,
                                    std::function<SaveResult(const blob&, const SaveOptions&)> saver) {
    customSavers_[format] = std::move(saver);
}

std::pair<std::vector<uint8_t>, SaveResult> ImageSaver::saveFormat(
    const blob& imageData, ImageFormat format, const SaveOptions& options) const {

    SaveResult result;
    std::vector<uint8_t> encodedData;

    try {
        // Check for custom saver first
        auto customIt = customSavers_.find(format);
        if (customIt != customSavers_.end()) {
            result = customIt->second(imageData, options);
            if (result.success) {
                // Custom savers should return data in result, but we need it as vector
                // This is a simplified approach - in practice, custom savers would need to return data
                encodedData.resize(result.outputSize);
            }
            return {encodedData, result};
        }

#ifdef ATOM_IMAGE_HAS_OPENCV
        cv::Mat mat = imageData.to_mat();
        if (mat.empty()) {
            result.errorMessage = "Cannot convert blob to OpenCV Mat";
            return {encodedData, result};
        }

        std::vector<int> params;

        switch (format) {
            case ImageFormat::JPEG: {
                params = {cv::IMWRITE_JPEG_QUALITY, options.quality};
                if (options.progressiveEncoding) {
                    params.push_back(cv::IMWRITE_JPEG_PROGRESSIVE);
                    params.push_back(1);
                }
                cv::imencode(".jpg", mat, encodedData, params);
                break;
            }
            case ImageFormat::PNG: {
                params = {cv::IMWRITE_PNG_COMPRESSION, options.compressionLevel};
                cv::imencode(".png", mat, encodedData, params);
                break;
            }
            case ImageFormat::BMP: {
                cv::imencode(".bmp", mat, encodedData);
                break;
            }
            case ImageFormat::TIFF: {
                cv::imencode(".tiff", mat, encodedData);
                break;
            }
            case ImageFormat::WEBP: {
                params = {cv::IMWRITE_WEBP_QUALITY, options.quality};
                cv::imencode(".webp", mat, encodedData, params);
                break;
            }
            default: {
                result.errorMessage = "Unsupported format for OpenCV encoding";
                return {encodedData, result};
            }
        }

        result.success = true;
        result.usedFormat = format;
        result.outputSize = encodedData.size();

#else
        // Fallback implementation using STB or basic encoding
        switch (format) {
            case ImageFormat::BMP: {
                // Simple BMP encoding fallback
                result.errorMessage = "BMP encoding requires OpenCV or STB";
                break;
            }
            default: {
                result.errorMessage = "Format encoding requires OpenCV";
                break;
            }
        }
#endif

    } catch (const std::exception& e) {
        result.errorMessage = e.what();
        result.success = false;
    }

    return {encodedData, result};
}

blob ImageSaver::applyPreProcessing(const blob& imageData, const SaveOptions& options) const {
    blob processedData = imageData;

    try {
        // Apply any preprocessing based on options
        if (options.optimizeSize) {
            // Could apply size optimization preprocessing here
            // For now, just return the original data
        }

        // Additional preprocessing could be added here based on format-specific options

    } catch (const std::exception&) {
        // If preprocessing fails, return original data
        return imageData;
    }

    return processedData;
}

std::vector<uint8_t> ImageSaver::embedMetadata(const std::vector<uint8_t>& encodedData,
                                              ImageFormat format,
                                              const std::unordered_map<std::string, std::string>& metadata) const {
    // For now, return the original data
    // In a full implementation, this would embed metadata based on the format
    // JPEG: EXIF data
    // PNG: tEXt chunks
    // TIFF: TIFF tags
    // etc.

    if (metadata.empty()) {
        return encodedData;
    }

    // Placeholder implementation - would need format-specific metadata embedding
    switch (format) {
        case ImageFormat::JPEG:
            // Would embed EXIF metadata
            break;
        case ImageFormat::PNG:
            // Would embed tEXt chunks
            break;
        case ImageFormat::TIFF:
            // Would embed TIFF tags
            break;
        default:
            // No metadata support for this format
            break;
    }

    return encodedData;
}

bool ImageSaver::createBackup(const std::filesystem::path& filePath, const std::string& backupSuffix) const {
    try {
        if (!std::filesystem::exists(filePath)) {
            return true; // No file to backup
        }

        std::filesystem::path backupPath = filePath;
        backupPath += backupSuffix;

        std::filesystem::copy_file(filePath, backupPath, std::filesystem::copy_options::overwrite_existing);
        return true;

    } catch (const std::exception&) {
        return false;
    }
}

// Factory functions
std::unique_ptr<ImageSaver> createImageSaver() {
    return std::make_unique<ImageSaver>();
}

bool quickSaveImage(const blob& imageData, const std::filesystem::path& filePath, int quality) {
    SaveOptions options;
    options.quality = quality;

    auto saver = createImageSaver();
    auto result = saver->saveToFile(imageData, filePath, options);
    return result.success;
}

size_t quickSaveBatch(const std::vector<blob>& imageData,
                     const std::vector<std::filesystem::path>& filePaths,
                     int quality,
                     size_t maxConcurrency) {
    SaveOptions options;
    options.quality = quality;

    auto saver = createImageSaver();
    auto result = saver->saveBatch(imageData, filePaths, options, maxConcurrency);
    return result.successCount;
}

}  // namespace atom::image
