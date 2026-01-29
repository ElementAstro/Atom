#include "batch_processor.hpp"

#include <algorithm>
#include <fstream>
#include <thread>

#include "../exif/exif_reader.hpp"
#include "../exif/exif_writer.hpp"
#include "privacy_sanitizer.hpp"

namespace atom::image::metadata {

namespace {
const std::vector<std::string> SUPPORTED_EXTENSIONS = {".jpg", ".jpeg", ".tif",
                                                       ".tiff"};

bool isSupportedFile(const std::filesystem::path& path) {
    auto ext = path.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    return std::find(SUPPORTED_EXTENSIONS.begin(), SUPPORTED_EXTENSIONS.end(),
                     ext) != SUPPORTED_EXTENSIONS.end();
}
}  // namespace

BatchSummary BatchProcessor::processDirectory(
    const std::filesystem::path& directory, MetadataProcessor processor,
    bool recursive) {
    auto files = collectFiles(directory, recursive);
    return processFiles(files, processor);
}

BatchSummary BatchProcessor::processFiles(
    const std::vector<std::filesystem::path>& files,
    MetadataProcessor processor) {
    BatchSummary summary;
    summary.totalFiles = static_cast<int>(files.size());
    cancelled_ = false;

    int current = 0;
    for (const auto& file : files) {
        if (cancelled_)
            break;

        ++current;
        if (progressCallback_) {
            progressCallback_(current, summary.totalFiles, file.string());
        }

        auto result = processFile(file, processor);
        summary.results.push_back(result);

        if (result.success) {
            ++summary.successCount;
        } else {
            ++summary.failureCount;
            if (stopOnError_)
                break;
        }
    }

    return summary;
}

BatchSummary BatchProcessor::readMetadata(
    const std::vector<std::filesystem::path>& files,
    std::function<void(const std::filesystem::path&, const ExifData&)>
        handler) {
    BatchSummary summary;
    summary.totalFiles = static_cast<int>(files.size());
    cancelled_ = false;

    int current = 0;
    for (const auto& file : files) {
        if (cancelled_)
            break;

        ++current;
        if (progressCallback_) {
            progressCallback_(current, summary.totalFiles, file.string());
        }

        BatchResult result;
        result.file = file;

        ExifReader reader(file);
        if (reader.parse()) {
            handler(file, reader.getExifData());
            result.success = true;
            ++summary.successCount;
        } else {
            result.error = reader.lastError();
            ++summary.failureCount;
        }

        summary.results.push_back(result);
    }

    return summary;
}

BatchSummary BatchProcessor::copyMetadata(
    const std::filesystem::path& sourceFile,
    const std::vector<std::filesystem::path>& targetFiles) {
    BatchSummary summary;
    summary.totalFiles = static_cast<int>(targetFiles.size());
    cancelled_ = false;

    // Read source metadata
    ExifReader sourceReader(sourceFile);
    if (!sourceReader.parse()) {
        BatchResult result;
        result.file = sourceFile;
        result.success = false;
        result.error = "Failed to read source: " + sourceReader.lastError();
        summary.results.push_back(result);
        summary.failureCount = summary.totalFiles;
        return summary;
    }

    const auto& sourceExif = sourceReader.getExifData();

    int current = 0;
    for (const auto& targetFile : targetFiles) {
        if (cancelled_)
            break;

        ++current;
        if (progressCallback_) {
            progressCallback_(current, summary.totalFiles, targetFile.string());
        }

        BatchResult result;
        result.file = targetFile;

        ExifWriter writer(sourceExif);
        if (writer.writeToFile(targetFile)) {
            result.success = true;
            ++summary.successCount;
        } else {
            result.error = writer.lastError();
            ++summary.failureCount;
        }

        summary.results.push_back(result);
    }

    return summary;
}

BatchSummary BatchProcessor::stripMetadata(
    const std::vector<std::filesystem::path>& files,
    const std::filesystem::path& outputDir) {
    BatchSummary summary;
    summary.totalFiles = static_cast<int>(files.size());
    cancelled_ = false;

    int current = 0;
    for (const auto& file : files) {
        if (cancelled_)
            break;

        ++current;
        if (progressCallback_) {
            progressCallback_(current, summary.totalFiles, file.string());
        }

        BatchResult result;
        result.file = file;

        try {
            // Read file
            std::ifstream inFile(file, std::ios::binary);
            if (!inFile.is_open()) {
                result.error = "Cannot open file";
                ++summary.failureCount;
                summary.results.push_back(result);
                continue;
            }

            std::vector<uint8_t> data((std::istreambuf_iterator<char>(inFile)),
                                      std::istreambuf_iterator<char>());
            inFile.close();

            // Strip metadata
            auto stripped = PrivacySanitizer::stripAllMetadata(data);

            // Determine output path
            std::filesystem::path outPath;
            if (outputDir.empty()) {
                outPath = file;
            } else {
                std::filesystem::create_directories(outputDir);
                outPath = outputDir / file.filename();
            }

            // Write output
            std::ofstream outFile(outPath, std::ios::binary);
            if (!outFile.is_open()) {
                result.error = "Cannot write output file";
                ++summary.failureCount;
                summary.results.push_back(result);
                continue;
            }

            outFile.write(reinterpret_cast<const char*>(stripped.data()),
                          static_cast<std::streamsize>(stripped.size()));

            result.success = true;
            ++summary.successCount;
        } catch (const std::exception& e) {
            result.error = e.what();
            ++summary.failureCount;
        }

        summary.results.push_back(result);
    }

    return summary;
}

BatchSummary BatchProcessor::sanitizeMetadata(
    const std::vector<std::filesystem::path>& files,
    const std::filesystem::path& outputDir) {
    BatchSummary summary;
    summary.totalFiles = static_cast<int>(files.size());
    cancelled_ = false;

    PrivacySanitizer sanitizer(PrivacyLevel::MODERATE);

    int current = 0;
    for (const auto& file : files) {
        if (cancelled_)
            break;

        ++current;
        if (progressCallback_) {
            progressCallback_(current, summary.totalFiles, file.string());
        }

        BatchResult result;
        result.file = file;

        std::filesystem::path outPath;
        if (outputDir.empty()) {
            outPath = file;
        } else {
            std::filesystem::create_directories(outputDir);
            outPath = outputDir / file.filename();
        }

        if (sanitizer.sanitizeFile(file, outPath)) {
            result.success = true;
            ++summary.successCount;
        } else {
            result.error = sanitizer.lastError();
            ++summary.failureCount;
        }

        summary.results.push_back(result);
    }

    return summary;
}

std::vector<std::filesystem::path> BatchProcessor::collectFiles(
    const std::filesystem::path& directory, bool recursive) {
    std::vector<std::filesystem::path> files;

    if (!std::filesystem::exists(directory) ||
        !std::filesystem::is_directory(directory)) {
        return files;
    }

    if (recursive) {
        for (const auto& entry :
             std::filesystem::recursive_directory_iterator(directory)) {
            if (entry.is_regular_file() && isSupportedFile(entry.path())) {
                files.push_back(entry.path());
            }
        }
    } else {
        for (const auto& entry :
             std::filesystem::directory_iterator(directory)) {
            if (entry.is_regular_file() && isSupportedFile(entry.path())) {
                files.push_back(entry.path());
            }
        }
    }

    return files;
}

BatchResult BatchProcessor::processFile(const std::filesystem::path& file,
                                        MetadataProcessor processor) {
    BatchResult result;
    result.file = file;

    ExifReader reader(file);
    if (!reader.parse()) {
        result.error = reader.lastError();
        return result;
    }

    auto& exifData = reader.getExifData();
    if (!processor(exifData)) {
        result.error = "Processor returned false";
        return result;
    }

    ExifWriter writer(exifData);
    if (!writer.writeToFile(file)) {
        result.error = writer.lastError();
        return result;
    }

    result.success = true;
    return result;
}

}  // namespace atom::image::metadata
