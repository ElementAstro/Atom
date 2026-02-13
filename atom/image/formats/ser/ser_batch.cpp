// ser_batch.cpp
#include "ser_batch.h"

#include <chrono>
#include <fstream>
#include <iomanip>
#include <opencv2/imgcodecs.hpp>
#include <sstream>

namespace serastro {

BatchProcessor::BatchProcessor()
    : analyzer_(std::make_unique<SERAnalyzer>()),
      luckyImaging_(std::make_unique<LuckyImaging>()),
      videoExporter_(std::make_unique<VideoExporter>()) {}

BatchProcessor::BatchProcessor(const BatchConfig& config)
    : config_(config),
      analyzer_(std::make_unique<SERAnalyzer>(config.analyzerOptions)),
      luckyImaging_(std::make_unique<LuckyImaging>(config.luckyParams)),
      videoExporter_(std::make_unique<VideoExporter>(config.exportParams)) {}

BatchProcessor::~BatchProcessor() { cancel(); }

void BatchProcessor::addJob(const BatchJob& job) {
    std::lock_guard<std::mutex> lock(queueMutex_);
    jobQueue_.push(job);
}

void BatchProcessor::addJobs(const std::vector<BatchJob>& jobs) {
    std::lock_guard<std::mutex> lock(queueMutex_);
    for (const auto& job : jobs) {
        jobQueue_.push(job);
    }
}

size_t BatchProcessor::createJobsFromDirectory(
    const std::filesystem::path& directory, BatchOperation operation,
    bool recursive) {
    size_t count = 0;

    auto processEntry = [&](const std::filesystem::directory_entry& entry) {
        if (entry.is_regular_file()) {
            auto ext = entry.path().extension().string();
            std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

            if (ext == ".ser") {
                BatchJob job;
                job.inputPath = entry.path();
                job.operation = operation;
                addJob(job);
                ++count;
            }
        }
    };

    if (recursive) {
        for (const auto& entry :
             std::filesystem::recursive_directory_iterator(directory)) {
            processEntry(entry);
        }
    } else {
        for (const auto& entry :
             std::filesystem::directory_iterator(directory)) {
            processEntry(entry);
        }
    }

    return count;
}

std::vector<BatchResult> BatchProcessor::processAll(
    ProgressCallback progressCallback, CompletionCallback completionCallback) {
    std::vector<BatchResult> results;
    cancelled_ = false;
    processing_ = true;

    auto startTime = std::chrono::high_resolution_clock::now();

    // Get all jobs from queue
    std::vector<BatchJob> jobs;
    {
        std::lock_guard<std::mutex> lock(queueMutex_);
        while (!jobQueue_.empty()) {
            jobs.push_back(jobQueue_.top());
            jobQueue_.pop();
        }
    }

    size_t totalJobs = jobs.size();
    size_t completedJobs = 0;
    size_t failedJobs = 0;

    for (size_t i = 0; i < jobs.size() && !cancelled_; ++i) {
        const auto& job = jobs[i];

        // Report progress
        if (progressCallback) {
            auto now = std::chrono::high_resolution_clock::now();
            double elapsed =
                std::chrono::duration<double>(now - startTime).count();

            BatchProgress progress;
            progress.totalJobs = totalJobs;
            progress.completedJobs = completedJobs;
            progress.failedJobs = failedJobs;
            progress.currentJobIndex = i;
            progress.currentFile = job.inputPath.filename().string();
            progress.currentOperation = batchOperationToString(job.operation);
            progress.overallProgress = static_cast<float>(i) / totalJobs;
            progress.elapsedSeconds = elapsed;

            if (completedJobs > 0) {
                double avgTime = elapsed / completedJobs;
                progress.estimatedRemaining =
                    avgTime * (totalJobs - completedJobs);
            }

            progressCallback(progress);
        }

        // Check if output exists and skip if configured
        if (config_.skipExisting && !job.outputPath.empty() &&
            std::filesystem::exists(job.outputPath)) {
            BatchResult result;
            result.inputPath = job.inputPath;
            result.outputPath = job.outputPath;
            result.success = true;
            result.errorMessage = "Skipped (output exists)";
            results.push_back(result);
            ++completedJobs;
            continue;
        }

        // Process job
        log("Processing: " + job.inputPath.string());

        BatchResult result = processJob(job);
        results.push_back(result);

        if (result.success) {
            ++completedJobs;
            log("Completed: " + job.inputPath.string());
        } else {
            ++failedJobs;
            log("Failed: " + job.inputPath.string() + " - " +
                result.errorMessage);

            if (config_.stopOnError) {
                break;
            }
        }

        // Call completion callback
        if (completionCallback) {
            completionCallback(result);
        }
    }

    processing_ = false;

    return results;
}

std::future<std::vector<BatchResult>> BatchProcessor::processAllAsync(
    ProgressCallback progressCallback, CompletionCallback completionCallback) {
    return std::async(
        std::launch::async, [this, progressCallback, completionCallback]() {
            return processAll(progressCallback, completionCallback);
        });
}

BatchResult BatchProcessor::processJob(const BatchJob& job) {
    auto startTime = std::chrono::high_resolution_clock::now();

    BatchResult result;
    result.inputPath = job.inputPath;

    try {
        switch (job.operation) {
            case BatchOperation::Analyze:
                result = processAnalyzeJob(job);
                break;

            case BatchOperation::LuckyImage:
                result = processLuckyJob(job);
                break;

            case BatchOperation::ExportVideo:
                result = processExportJob(job);
                break;

            case BatchOperation::ExportFrames:
                result = processFrameExportJob(job);
                break;

            case BatchOperation::Convert:
            case BatchOperation::Custom:
                // Would need custom implementation
                result.errorMessage = "Operation not implemented";
                break;
        }
    } catch (const std::exception& e) {
        result.errorMessage = e.what();
    }

    auto endTime = std::chrono::high_resolution_clock::now();
    result.processingTimeSeconds =
        std::chrono::duration<double>(endTime - startTime).count();

    return result;
}

void BatchProcessor::cancel() { cancelled_ = true; }

void BatchProcessor::clearQueue() {
    std::lock_guard<std::mutex> lock(queueMutex_);
    while (!jobQueue_.empty()) {
        jobQueue_.pop();
    }
}

size_t BatchProcessor::getPendingJobCount() const {
    std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(queueMutex_));
    return jobQueue_.size();
}

void BatchProcessor::setConfig(const BatchConfig& config) {
    config_ = config;
    analyzer_ = std::make_unique<SERAnalyzer>(config.analyzerOptions);
    luckyImaging_ = std::make_unique<LuckyImaging>(config.luckyParams);
    videoExporter_ = std::make_unique<VideoExporter>(config.exportParams);
}

void BatchProcessor::setCustomProcessor(
    std::shared_ptr<FrameProcessor> processor) {
    customProcessor_ = processor;
}

std::string BatchProcessor::generateReport(
    const std::vector<BatchResult>& results) const {
    std::ostringstream oss;

    oss << "=== Batch Processing Report ===\n\n";

    size_t successful = 0;
    size_t failed = 0;
    double totalTime = 0.0;

    for (const auto& result : results) {
        if (result.success) {
            ++successful;
        } else {
            ++failed;
        }
        totalTime += result.processingTimeSeconds;
    }

    oss << "Summary:\n";
    oss << "  Total Jobs: " << results.size() << "\n";
    oss << "  Successful: " << successful << "\n";
    oss << "  Failed: " << failed << "\n";
    oss << "  Total Time: " << std::fixed << std::setprecision(2) << totalTime
        << " seconds\n\n";

    if (failed > 0) {
        oss << "Failed Jobs:\n";
        for (const auto& result : results) {
            if (!result.success) {
                oss << "  " << result.inputPath.filename().string() << ": "
                    << result.errorMessage << "\n";
            }
        }
        oss << "\n";
    }

    oss << "Details:\n";
    for (const auto& result : results) {
        oss << "  " << result.inputPath.filename().string() << ": "
            << (result.success ? "OK" : "FAILED") << " (" << std::fixed
            << std::setprecision(2) << result.processingTimeSeconds << "s)\n";
    }

    return oss.str();
}

void BatchProcessor::exportResultsToCSV(
    const std::vector<BatchResult>& results,
    const std::filesystem::path& outputPath) const {
    std::ofstream file(outputPath);

    if (!file.is_open()) {
        throw SERIOException("Cannot create CSV file: " + outputPath.string());
    }

    // Header
    file << "Input,Output,Success,Error,Time(s)\n";

    // Data
    for (const auto& result : results) {
        file << "\"" << result.inputPath.string() << "\","
             << "\"" << result.outputPath.string() << "\","
             << (result.success ? "true" : "false") << ","
             << "\"" << result.errorMessage << "\"," << std::fixed
             << std::setprecision(3) << result.processingTimeSeconds << "\n";
    }
}

BatchResult BatchProcessor::processAnalyzeJob(const BatchJob& job) {
    BatchResult result;
    result.inputPath = job.inputPath;

    try {
        auto stats = analyzer_->analyze(job.inputPath, nullptr);
        result.analysisResult = stats;
        result.success = true;

        // Write analysis to file if output path specified
        if (!job.outputPath.empty()) {
            result.outputPath = job.outputPath;
            std::ofstream file(job.outputPath);
            file << analyzer_->generateReport(stats);
        } else if (!config_.outputDir.empty()) {
            result.outputPath = getOutputPath(job, ".txt");
            std::ofstream file(result.outputPath);
            file << analyzer_->generateReport(stats);
        }

    } catch (const std::exception& e) {
        result.errorMessage = e.what();
    }

    return result;
}

BatchResult BatchProcessor::processLuckyJob(const BatchJob& job) {
    BatchResult result;
    result.inputPath = job.inputPath;

    try {
        auto luckyResult = luckyImaging_->processFile(job.inputPath, nullptr);
        result.luckyResult = luckyResult;

        // Determine output path
        std::filesystem::path outputPath = job.outputPath;
        if (outputPath.empty()) {
            outputPath = getOutputPath(job, ".tiff");
        }
        result.outputPath = outputPath;

        // Save result
        if (!luckyResult.stackedImage.empty()) {
            cv::imwrite(outputPath.string(), luckyResult.stackedImage);
            result.success = true;
        } else {
            result.errorMessage = "No stacked image produced";
        }

    } catch (const std::exception& e) {
        result.errorMessage = e.what();
    }

    return result;
}

BatchResult BatchProcessor::processExportJob(const BatchJob& job) {
    BatchResult result;
    result.inputPath = job.inputPath;

    try {
        std::filesystem::path outputPath = job.outputPath;
        if (outputPath.empty()) {
            outputPath = getOutputPath(job, ".mp4");
        }
        result.outputPath = outputPath;

        auto exportResult =
            videoExporter_->exportToVideo(job.inputPath, outputPath, nullptr);

        result.exportResult = exportResult;
        result.success = exportResult.success;
        result.errorMessage = exportResult.errorMessage;

    } catch (const std::exception& e) {
        result.errorMessage = e.what();
    }

    return result;
}

BatchResult BatchProcessor::processFrameExportJob(const BatchJob& job) {
    BatchResult result;
    result.inputPath = job.inputPath;

    try {
        std::filesystem::path outputDir = job.outputPath;
        if (outputDir.empty() && !config_.outputDir.empty()) {
            outputDir = config_.outputDir / job.inputPath.stem();
        } else if (outputDir.empty()) {
            outputDir = job.inputPath.parent_path() /
                        (job.inputPath.stem().string() + "_frames");
        }
        result.outputPath = outputDir;

        size_t count = videoExporter_->exportImageSequence(
            job.inputPath, outputDir, "png", nullptr);

        result.success = count > 0;
        if (!result.success) {
            result.errorMessage = "No frames exported";
        }

    } catch (const std::exception& e) {
        result.errorMessage = e.what();
    }

    return result;
}

std::filesystem::path BatchProcessor::getOutputPath(
    const BatchJob& job, const std::string& extension) const {
    std::filesystem::path outputPath;

    if (!config_.outputDir.empty()) {
        if (config_.preserveSubdirs) {
            // Try to preserve relative path structure
            // This is simplified - would need base path for full implementation
            outputPath = config_.outputDir / job.inputPath.filename();
        } else {
            outputPath = config_.outputDir / job.inputPath.filename();
        }
    } else {
        outputPath = job.inputPath;
    }

    // Change extension and add suffix
    std::string stem = outputPath.stem().string();
    outputPath =
        outputPath.parent_path() / (stem + config_.outputSuffix + extension);

    // Create directory if needed
    std::filesystem::create_directories(outputPath.parent_path());

    return outputPath;
}

void BatchProcessor::log(const std::string& message) const {
    if (!config_.enableLogging) {
        return;
    }

    if (!config_.logFile.empty()) {
        std::ofstream file(config_.logFile, std::ios::app);
        if (file.is_open()) {
            auto now = std::chrono::system_clock::now();
            auto time = std::chrono::system_clock::to_time_t(now);

            file << std::put_time(std::localtime(&time), "%Y-%m-%d %H:%M:%S")
                 << " - " << message << "\n";
        }
    }
}

// Utility functions

std::vector<FileStatistics> batchAnalyze(
    const std::filesystem::path& directory) {
    BatchProcessor processor;
    processor.createJobsFromDirectory(directory, BatchOperation::Analyze, true);

    auto results = processor.processAll(nullptr, nullptr);

    std::vector<FileStatistics> stats;
    for (const auto& result : results) {
        if (result.success && result.analysisResult) {
            stats.push_back(*result.analysisResult);
        }
    }

    return stats;
}

size_t batchLuckyImage(const std::filesystem::path& directory,
                       const std::filesystem::path& outputDir) {
    BatchConfig config;
    config.outputDir = outputDir;

    BatchProcessor processor(config);
    processor.createJobsFromDirectory(directory, BatchOperation::LuckyImage,
                                      true);

    auto results = processor.processAll(nullptr, nullptr);

    size_t successful = 0;
    for (const auto& result : results) {
        if (result.success) {
            ++successful;
        }
    }

    return successful;
}

std::string batchOperationToString(BatchOperation op) {
    switch (op) {
        case BatchOperation::Analyze:
            return "Analyze";
        case BatchOperation::LuckyImage:
            return "LuckyImage";
        case BatchOperation::ExportVideo:
            return "ExportVideo";
        case BatchOperation::ExportFrames:
            return "ExportFrames";
        case BatchOperation::Convert:
            return "Convert";
        case BatchOperation::Custom:
            return "Custom";
    }
    return "Unknown";
}

BatchOperation batchOperationFromString(const std::string& name) {
    if (name == "Analyze")
        return BatchOperation::Analyze;
    if (name == "LuckyImage")
        return BatchOperation::LuckyImage;
    if (name == "ExportVideo")
        return BatchOperation::ExportVideo;
    if (name == "ExportFrames")
        return BatchOperation::ExportFrames;
    if (name == "Convert")
        return BatchOperation::Convert;
    if (name == "Custom")
        return BatchOperation::Custom;
    return BatchOperation::Analyze;
}

}  // namespace serastro
