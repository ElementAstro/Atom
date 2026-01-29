// ser_batch.h
#pragma once

#include "exception.h"
#include "frame_processor.h"
#include "lucky_imaging.h"
#include "ser_analyzer.h"
#include "video_export.h"

#include <atomic>
#include <filesystem>
#include <functional>
#include <future>
#include <memory>
#include <queue>
#include <string>
#include <thread>
#include <vector>

namespace serastro {

/**
 * @enum BatchOperation
 * @brief Types of batch operations
 */
enum class BatchOperation {
    Analyze,       ///< Analyze files
    LuckyImage,    ///< Lucky imaging stack
    ExportVideo,   ///< Export to video
    ExportFrames,  ///< Export as image sequence
    Convert,       ///< Convert format/settings
    Custom         ///< Custom processor
};

/**
 * @struct BatchJob
 * @brief A single batch processing job
 */
struct BatchJob {
    std::filesystem::path inputPath;
    std::filesystem::path outputPath;
    BatchOperation operation = BatchOperation::Analyze;
    std::string customParams;
    int priority = 0;

    bool operator<(const BatchJob& other) const {
        return priority < other.priority;
    }
};

/**
 * @struct BatchResult
 * @brief Result of a single batch job
 */
struct BatchResult {
    std::filesystem::path inputPath;
    std::filesystem::path outputPath;
    bool success = false;
    std::string errorMessage;
    double processingTimeSeconds = 0.0;

    // Operation-specific results
    std::optional<FileStatistics> analysisResult;
    std::optional<LuckyImagingResult> luckyResult;
    std::optional<ExportResult> exportResult;
};

/**
 * @struct BatchProgress
 * @brief Progress information for batch processing
 */
struct BatchProgress {
    size_t totalJobs = 0;
    size_t completedJobs = 0;
    size_t failedJobs = 0;
    size_t currentJobIndex = 0;
    std::string currentFile;
    std::string currentOperation;
    float overallProgress = 0.0f;
    float currentJobProgress = 0.0f;
    double elapsedSeconds = 0.0;
    double estimatedRemaining = 0.0;
};

/**
 * @struct BatchConfig
 * @brief Configuration for batch processing
 */
struct BatchConfig {
    // Parallelism
    int maxThreads = 1;          ///< Max parallel jobs (0 = auto)
    bool useThreadPool = false;  ///< Use thread pool

    // Error handling
    bool stopOnError = false;  ///< Stop batch on first error
    bool skipExisting = true;  ///< Skip if output exists

    // Output options
    std::filesystem::path outputDir;
    std::string outputSuffix = "_processed";
    bool preserveSubdirs = true;  ///< Preserve input directory structure

    // Processing options
    LuckyImagingParams luckyParams;
    VideoExportParams exportParams;
    AnalyzerOptions analyzerOptions;

    // Logging
    bool enableLogging = true;
    std::filesystem::path logFile;
};

/**
 * @class BatchProcessor
 * @brief Process multiple SER files in batch
 *
 * Supports parallel processing of multiple SER files with
 * various operations like analysis, stacking, and export.
 */
class BatchProcessor {
public:
    using ProgressCallback = std::function<void(const BatchProgress&)>;
    using CompletionCallback = std::function<void(const BatchResult&)>;

    /**
     * @brief Default constructor
     */
    BatchProcessor();

    /**
     * @brief Construct with configuration
     * @param config Batch configuration
     */
    explicit BatchProcessor(const BatchConfig& config);

    /**
     * @brief Destructor
     */
    ~BatchProcessor();

    /**
     * @brief Add a job to the queue
     * @param job Job to add
     */
    void addJob(const BatchJob& job);

    /**
     * @brief Add multiple jobs
     * @param jobs Vector of jobs
     */
    void addJobs(const std::vector<BatchJob>& jobs);

    /**
     * @brief Create jobs from directory
     * @param directory Input directory
     * @param operation Operation to perform
     * @param recursive Search recursively
     * @return Number of jobs created
     */
    size_t createJobsFromDirectory(const std::filesystem::path& directory,
                                   BatchOperation operation,
                                   bool recursive = true);

    /**
     * @brief Process all queued jobs
     * @param progressCallback Progress callback
     * @param completionCallback Per-job completion callback
     * @return Vector of results
     */
    std::vector<BatchResult> processAll(
        ProgressCallback progressCallback = nullptr,
        CompletionCallback completionCallback = nullptr);

    /**
     * @brief Process jobs asynchronously
     * @param progressCallback Progress callback
     * @param completionCallback Per-job completion callback
     * @return Future with results
     */
    std::future<std::vector<BatchResult>> processAllAsync(
        ProgressCallback progressCallback = nullptr,
        CompletionCallback completionCallback = nullptr);

    /**
     * @brief Process single job
     * @param job Job to process
     * @return Job result
     */
    BatchResult processJob(const BatchJob& job);

    /**
     * @brief Cancel processing
     */
    void cancel();

    /**
     * @brief Check if cancelled
     */
    bool isCancelled() const { return cancelled_.load(); }

    /**
     * @brief Check if processing
     */
    bool isProcessing() const { return processing_.load(); }

    /**
     * @brief Clear job queue
     */
    void clearQueue();

    /**
     * @brief Get number of pending jobs
     */
    size_t getPendingJobCount() const;

    /**
     * @brief Set configuration
     * @param config New configuration
     */
    void setConfig(const BatchConfig& config);

    /**
     * @brief Get current configuration
     */
    const BatchConfig& getConfig() const { return config_; }

    /**
     * @brief Set custom processor for Custom operation
     * @param processor Frame processor
     */
    void setCustomProcessor(std::shared_ptr<FrameProcessor> processor);

    /**
     * @brief Generate batch report
     * @param results Batch results
     * @return Report string
     */
    std::string generateReport(const std::vector<BatchResult>& results) const;

    /**
     * @brief Export results to CSV
     * @param results Batch results
     * @param outputPath Output CSV path
     */
    void exportResultsToCSV(const std::vector<BatchResult>& results,
                            const std::filesystem::path& outputPath) const;

private:
    BatchConfig config_;
    std::priority_queue<BatchJob> jobQueue_;
    std::mutex queueMutex_;
    std::atomic<bool> cancelled_{false};
    std::atomic<bool> processing_{false};
    std::shared_ptr<FrameProcessor> customProcessor_;

    // Processors
    std::unique_ptr<SERAnalyzer> analyzer_;
    std::unique_ptr<LuckyImaging> luckyImaging_;
    std::unique_ptr<VideoExporter> videoExporter_;

    /**
     * @brief Process analyze job
     */
    BatchResult processAnalyzeJob(const BatchJob& job);

    /**
     * @brief Process lucky imaging job
     */
    BatchResult processLuckyJob(const BatchJob& job);

    /**
     * @brief Process video export job
     */
    BatchResult processExportJob(const BatchJob& job);

    /**
     * @brief Process frame export job
     */
    BatchResult processFrameExportJob(const BatchJob& job);

    /**
     * @brief Get output path for job
     */
    std::filesystem::path getOutputPath(const BatchJob& job,
                                        const std::string& extension) const;

    /**
     * @brief Log message
     */
    void log(const std::string& message) const;
};

/**
 * @brief Quick batch analyze
 * @param directory Directory containing SER files
 * @return Vector of analysis results
 */
std::vector<FileStatistics> batchAnalyze(
    const std::filesystem::path& directory);

/**
 * @brief Quick batch lucky imaging
 * @param directory Input directory
 * @param outputDir Output directory
 * @return Number of files processed
 */
size_t batchLuckyImage(const std::filesystem::path& directory,
                       const std::filesystem::path& outputDir);

/**
 * @brief Get operation name
 */
std::string batchOperationToString(BatchOperation op);

/**
 * @brief Parse operation from string
 */
BatchOperation batchOperationFromString(const std::string& name);

}  // namespace serastro
