#ifndef ATOM_IMAGE_METADATA_BATCH_PROCESSOR_HPP
#define ATOM_IMAGE_METADATA_BATCH_PROCESSOR_HPP

#include <filesystem>
#include <functional>
#include <future>
#include <string>
#include <vector>

#include "../types/exif_types.hpp"

namespace atom::image::metadata {

struct BatchResult {
    std::filesystem::path file;
    bool success = false;
    std::string error;
    int fieldsProcessed = 0;
};

struct BatchSummary {
    int totalFiles = 0;
    int successCount = 0;
    int failureCount = 0;
    std::vector<BatchResult> results;

    [[nodiscard]] double successRate() const noexcept {
        return totalFiles > 0 ? static_cast<double>(successCount) / totalFiles
                              : 0;
    }
};

using ProgressCallback =
    std::function<void(int current, int total, const std::string& file)>;
using MetadataProcessor = std::function<bool(ExifData&)>;

class BatchProcessor {
public:
    BatchProcessor() = default;

    void setProgressCallback(ProgressCallback callback) {
        progressCallback_ = std::move(callback);
    }
    void setMaxThreads(int threads) { maxThreads_ = threads > 0 ? threads : 1; }
    void setStopOnError(bool stop) { stopOnError_ = stop; }

    [[nodiscard]] BatchSummary processDirectory(
        const std::filesystem::path& directory, MetadataProcessor processor,
        bool recursive = false);

    [[nodiscard]] BatchSummary processFiles(
        const std::vector<std::filesystem::path>& files,
        MetadataProcessor processor);

    [[nodiscard]] BatchSummary readMetadata(
        const std::vector<std::filesystem::path>& files,
        std::function<void(const std::filesystem::path&, const ExifData&)>
            handler);

    [[nodiscard]] BatchSummary copyMetadata(
        const std::filesystem::path& sourceFile,
        const std::vector<std::filesystem::path>& targetFiles);

    [[nodiscard]] BatchSummary stripMetadata(
        const std::vector<std::filesystem::path>& files,
        const std::filesystem::path& outputDir = "");

    [[nodiscard]] BatchSummary sanitizeMetadata(
        const std::vector<std::filesystem::path>& files,
        const std::filesystem::path& outputDir = "");

    void cancel() { cancelled_ = true; }
    [[nodiscard]] bool isCancelled() const noexcept { return cancelled_; }

private:
    ProgressCallback progressCallback_;
    int maxThreads_ = 4;
    bool stopOnError_ = false;
    std::atomic<bool> cancelled_{false};

    [[nodiscard]] std::vector<std::filesystem::path> collectFiles(
        const std::filesystem::path& directory, bool recursive);

    BatchResult processFile(const std::filesystem::path& file,
                            MetadataProcessor processor);
};

}  // namespace atom::image::metadata

#endif  // ATOM_IMAGE_METADATA_BATCH_PROCESSOR_HPP
