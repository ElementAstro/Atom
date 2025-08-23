#ifndef ATOM_IMAGE_REALTIME_HPP
#define ATOM_IMAGE_REALTIME_HPP

/**
 * @file realtime.hpp
 * @brief Real-time image processing capabilities
 * 
 * This module provides real-time image processing for video streams,
 * live camera feeds, and interactive applications with low-latency
 * processing pipelines.
 * 
 * @author Atom Framework Team
 * @date 2025
 * @version 1.0.0
 */

#include "atom/image/core/image_blob.hpp"
#include <vector>
#include <string>
#include <memory>
#include <functional>
#include <atomic>
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>

namespace atom::image {

/**
 * @brief Video capture sources
 */
enum class CaptureSource {
    CAMERA,         // Camera device
    FILE,           // Video file
    STREAM,         // Network stream (RTSP, HTTP, etc.)
    SCREEN,         // Screen capture
    SYNTHETIC,      // Synthetic/generated frames
    CUSTOM          // Custom source
};

/**
 * @brief Real-time processing modes
 */
enum class ProcessingMode {
    PASSTHROUGH,    // No processing (passthrough)
    FILTER,         // Apply filters
    ENHANCE,        // Image enhancement
    DETECT,         // Object/feature detection
    TRACK,          // Object tracking
    ANALYZE,        // Image analysis
    CUSTOM          // Custom processing pipeline
};

/**
 * @brief Frame information
 */
struct FrameInfo {
    int64_t timestamp;              // Frame timestamp (microseconds)
    int frameNumber;                // Frame sequence number
    double fps;                     // Current FPS
    int width, height;              // Frame dimensions
    int channels;                   // Number of channels
    std::string format;             // Pixel format
    std::unordered_map<std::string, double> metadata; // Additional metadata
};

/**
 * @brief Processing statistics
 */
struct ProcessingStats {
    double averageFPS = 0.0;        // Average processing FPS
    double currentFPS = 0.0;        // Current processing FPS
    double averageLatency = 0.0;    // Average processing latency (ms)
    double currentLatency = 0.0;    // Current processing latency (ms)
    int64_t framesProcessed = 0;    // Total frames processed
    int64_t framesDropped = 0;      // Total frames dropped
    double cpuUsage = 0.0;          // CPU usage percentage
    double memoryUsage = 0.0;       // Memory usage (MB)
    double gpuUsage = 0.0;          // GPU usage percentage
    std::string status = "idle";    // Current status
};

/**
 * @brief Real-time processing parameters
 */
struct RealtimeParams {
    // Performance parameters
    int maxBufferSize = 5;          // Maximum frame buffer size
    int numThreads = 0;             // Number of processing threads (0 = auto)
    bool useGPU = true;             // Use GPU acceleration
    bool dropFrames = true;         // Drop frames if processing is slow
    double targetFPS = 30.0;        // Target processing FPS
    
    // Quality parameters
    int maxWidth = 1920;            // Maximum frame width
    int maxHeight = 1080;           // Maximum frame height
    bool maintainAspectRatio = true; // Maintain aspect ratio when resizing
    std::string pixelFormat = "RGB"; // Preferred pixel format
    
    // Processing parameters
    ProcessingMode mode = ProcessingMode::PASSTHROUGH;
    std::vector<std::string> filters; // Filters to apply
    std::unordered_map<std::string, double> filterParams; // Filter parameters
    
    // Callback parameters
    bool enablePreview = true;      // Enable preview callbacks
    bool enableAnalysis = false;    // Enable analysis callbacks
    bool enableRecording = false;   // Enable recording
    std::string recordingPath;      // Recording output path
};

/**
 * @brief Frame processing callback function type
 */
using FrameCallback = std::function<void(const blob&, const FrameInfo&)>;

/**
 * @brief Analysis callback function type
 */
using AnalysisCallback = std::function<void(const std::unordered_map<std::string, double>&)>;

/**
 * @brief Real-time image processor
 */
class RealtimeProcessor {
public:
    RealtimeProcessor() = default;
    virtual ~RealtimeProcessor() = default;

    /**
     * @brief Initialize real-time processor
     * @param params Processing parameters
     * @return Success status
     */
    virtual bool initialize(const RealtimeParams& params = {});

    /**
     * @brief Start processing from capture source
     * @param source Capture source type
     * @param sourcePath Source path (camera index, file path, stream URL)
     * @param frameCallback Callback for processed frames
     * @param analysisCallback Callback for analysis results
     * @return Success status
     */
    virtual bool startCapture(CaptureSource source,
                             const std::string& sourcePath,
                             FrameCallback frameCallback = nullptr,
                             AnalysisCallback analysisCallback = nullptr);

    /**
     * @brief Stop processing and capture
     */
    virtual void stop();

    /**
     * @brief Process single frame
     * @param input Input frame
     * @param frameInfo Frame information
     * @return Processed frame
     */
    virtual blob processFrame(const blob& input, const FrameInfo& frameInfo = {});

    /**
     * @brief Add frame to processing queue
     * @param frame Input frame
     * @param frameInfo Frame information
     * @return True if frame was added (false if queue is full)
     */
    virtual bool addFrame(const blob& frame, const FrameInfo& frameInfo = {});

    /**
     * @brief Set processing mode
     * @param mode Processing mode
     * @param params Mode-specific parameters
     */
    virtual void setProcessingMode(ProcessingMode mode,
                                  const std::unordered_map<std::string, double>& params = {});

    /**
     * @brief Add processing filter
     * @param filterName Filter name
     * @param params Filter parameters
     */
    virtual void addFilter(const std::string& filterName,
                          const std::unordered_map<std::string, double>& params = {});

    /**
     * @brief Remove processing filter
     * @param filterName Filter name to remove
     */
    virtual void removeFilter(const std::string& filterName);

    /**
     * @brief Clear all filters
     */
    virtual void clearFilters();

    /**
     * @brief Set frame callback
     * @param callback Callback function for processed frames
     */
    virtual void setFrameCallback(FrameCallback callback);

    /**
     * @brief Set analysis callback
     * @param callback Callback function for analysis results
     */
    virtual void setAnalysisCallback(AnalysisCallback callback);

    /**
     * @brief Get current processing statistics
     * @return Processing statistics
     */
    virtual ProcessingStats getStatistics() const;

    /**
     * @brief Get current frame rate
     * @return Current FPS
     */
    virtual double getCurrentFPS() const;

    /**
     * @brief Get processing latency
     * @return Current latency in milliseconds
     */
    virtual double getLatency() const;

    /**
     * @brief Check if processor is running
     * @return True if running
     */
    virtual bool isRunning() const;

    /**
     * @brief Pause processing
     */
    virtual void pause();

    /**
     * @brief Resume processing
     */
    virtual void resume();

    /**
     * @brief Check if processor is paused
     * @return True if paused
     */
    virtual bool isPaused() const;

    /**
     * @brief Start recording processed frames
     * @param outputPath Output file path
     * @param codec Video codec ("h264", "h265", "vp9", etc.)
     * @param quality Quality setting (0-100)
     * @return Success status
     */
    virtual bool startRecording(const std::string& outputPath,
                               const std::string& codec = "h264",
                               int quality = 80);

    /**
     * @brief Stop recording
     */
    virtual void stopRecording();

    /**
     * @brief Check if recording is active
     * @return True if recording
     */
    virtual bool isRecording() const;

    /**
     * @brief Take snapshot of current frame
     * @param outputPath Output image path
     * @return Success status
     */
    virtual bool takeSnapshot(const std::string& outputPath);

    /**
     * @brief Set target FPS
     * @param fps Target frames per second
     */
    virtual void setTargetFPS(double fps);

    /**
     * @brief Set maximum buffer size
     * @param size Maximum number of frames in buffer
     */
    virtual void setMaxBufferSize(int size);

    /**
     * @brief Enable/disable frame dropping
     * @param enable Whether to drop frames when processing is slow
     */
    virtual void setFrameDropping(bool enable);

    /**
     * @brief Get available capture devices
     * @return Vector of device names/paths
     */
    virtual std::vector<std::string> getAvailableDevices() const;

    /**
     * @brief Get supported video formats for device
     * @param devicePath Device path or index
     * @return Vector of supported formats
     */
    virtual std::vector<std::string> getSupportedFormats(const std::string& devicePath) const;

    /**
     * @brief Set capture resolution
     * @param width Frame width
     * @param height Frame height
     * @return Success status
     */
    virtual bool setCaptureResolution(int width, int height);

    /**
     * @brief Set capture frame rate
     * @param fps Capture frame rate
     * @return Success status
     */
    virtual bool setCaptureFPS(double fps);

protected:
    /**
     * @brief Main processing thread function
     */
    virtual void processingThread();

    /**
     * @brief Capture thread function
     */
    virtual void captureThread();

    /**
     * @brief Apply processing pipeline to frame
     * @param input Input frame
     * @param frameInfo Frame information
     * @return Processed frame
     */
    virtual blob applyProcessingPipeline(const blob& input, const FrameInfo& frameInfo);

    /**
     * @brief Update processing statistics
     * @param processingTime Time taken to process frame (ms)
     */
    virtual void updateStatistics(double processingTime);

    /**
     * @brief Initialize capture source
     * @param source Capture source type
     * @param sourcePath Source path
     * @return Success status
     */
    virtual bool initializeCapture(CaptureSource source, const std::string& sourcePath);

    /**
     * @brief Cleanup capture resources
     */
    virtual void cleanupCapture();

    /**
     * @brief Resize frame if needed
     * @param input Input frame
     * @return Resized frame
     */
    virtual blob resizeFrame(const blob& input);

    /**
     * @brief Convert frame format if needed
     * @param input Input frame
     * @param targetFormat Target pixel format
     * @return Converted frame
     */
    virtual blob convertFormat(const blob& input, const std::string& targetFormat);

private:
    // Thread management
    std::atomic<bool> running_{false};
    std::atomic<bool> paused_{false};
    std::thread processingThread_;
    std::thread captureThread_;

    // Frame buffer
    std::queue<std::pair<blob, FrameInfo>> frameBuffer_;
    std::mutex bufferMutex_;
    std::condition_variable bufferCondition_;

    // Callbacks
    FrameCallback frameCallback_;
    AnalysisCallback analysisCallback_;

    // Statistics
    mutable std::mutex statsMutex_;
    ProcessingStats stats_;

    // Parameters
    RealtimeParams params_;

    // Capture state
    std::atomic<bool> recording_{false};
    std::string recordingPath_;
};

/**
 * @brief Factory function to create optimal real-time processor
 * @param useGPU Whether to use GPU acceleration
 * @param numThreads Number of processing threads (0 = auto)
 * @return Unique pointer to real-time processor
 */
std::unique_ptr<RealtimeProcessor> createOptimalRealtimeProcessor(bool useGPU = true, int numThreads = 0);

} // namespace atom::image

#endif // ATOM_IMAGE_REALTIME_HPP
