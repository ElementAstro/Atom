/**
 * @file realtime_stub.cpp
 * @brief Stub implementation of RealtimeProcessor for minimal builds
 *
 * This file provides a minimal, non-functional stub implementation of the
 * RealtimeProcessor class. It is intended for builds where real-time video
 * processing capabilities are not required or where dependencies (OpenCV video
 * capture, etc.) are not available.
 *
 * **IMPORTANT**: This is an intentional stub implementation. The following
 * methods are no-ops or return minimal/synthetic data:
 * - initializeCapture() - Only accepts SYNTHETIC source
 * - cleanupCapture() - No-op
 * - resizeFrame() - Returns input unchanged
 * - convertFormat() - Returns input unchanged
 * - processingThread() - No-op (no actual thread processing)
 * - captureThread() - No-op (no actual frame capture)
 * - applyProcessingPipeline() - Returns input unchanged
 *
 * For full real-time processing functionality, use the complete implementation
 * with OpenCV support enabled.
 */

#include "realtime.hpp"

#include <algorithm>
#include <chrono>
#include <limits>
#include <unordered_map>
#include <utility>

namespace atom::image {

namespace {

constexpr double kMillisInSecond = 1000.0;

auto clampPositive(double value) -> double { return value < 0.0 ? 0.0 : value; }

auto currentTimeMilliseconds() -> double {
    using clock = std::chrono::high_resolution_clock;
    const auto now = clock::now();
    const auto epoch = now.time_since_epoch();
    return std::chrono::duration<double, std::milli>(epoch).count();
}

}  // namespace

bool RealtimeProcessor::initialize(const RealtimeParams& params) {
    {
        std::scoped_lock lock(bufferMutex_);
        while (!frameBuffer_.empty()) {
            frameBuffer_.pop();
        }
    }

    {
        std::scoped_lock lock(statsMutex_);
        stats_ = {};
        stats_.status = "initialized";
    }

    params_ = params;
    running_.store(false);
    paused_.store(false);

    return true;
}

bool RealtimeProcessor::startCapture(CaptureSource source,
                                     const std::string& sourcePath,
                                     FrameCallback frameCallback,
                                     AnalysisCallback analysisCallback) {
    if (frameCallback) {
        frameCallback_ = std::move(frameCallback);
    }
    if (analysisCallback) {
        analysisCallback_ = std::move(analysisCallback);
    }

    if (!initializeCapture(source, sourcePath)) {
        return false;
    }

    running_.store(true);
    paused_.store(false);

    {
        std::scoped_lock lock(statsMutex_);
        stats_.status = "capturing";
    }

    return true;
}

void RealtimeProcessor::stop() {
    running_.store(false);
    paused_.store(false);

    if (processingThread_.joinable()) {
        bufferCondition_.notify_all();
        processingThread_.join();
    }
    if (captureThread_.joinable()) {
        bufferCondition_.notify_all();
        captureThread_.join();
    }

    {
        std::scoped_lock lock(statsMutex_);
        stats_.status = "stopped";
    }
}

blob RealtimeProcessor::processFrame(const blob& input,
                                     const FrameInfo& frameInfo) {
    if (input.isEmpty()) {
        return {};
    }

    const auto start = currentTimeMilliseconds();

    blob output = applyProcessingPipeline(input, frameInfo);

    if (frameCallback_) {
        frameCallback_(output, frameInfo);
    }

    if (analysisCallback_) {
        std::unordered_map<std::string, double> analysis;
        analysis.emplace("fps", frameInfo.fps);
        analysis.emplace("width", static_cast<double>(frameInfo.width));
        analysis.emplace("height", static_cast<double>(frameInfo.height));
        analysisCallback_(analysis);
    }

    const auto end = currentTimeMilliseconds();
    updateStatistics(end - start);

    return output;
}

bool RealtimeProcessor::addFrame(const blob& frame,
                                 const FrameInfo& frameInfo) {
    if (frame.isEmpty()) {
        return false;
    }

    std::unique_lock lock(bufferMutex_);
    const auto maxSize = params_.maxBufferSize > 0
                             ? static_cast<std::size_t>(params_.maxBufferSize)
                             : std::numeric_limits<std::size_t>::max();

    if (frameBuffer_.size() >= maxSize) {
        if (params_.dropFrames) {
            {
                std::scoped_lock statsLock(statsMutex_);
                stats_.framesDropped++;
            }
            return false;
        }
    }

    frameBuffer_.emplace(frame, frameInfo);

    auto queued = frameBuffer_.front();
    frameBuffer_.pop();
    lock.unlock();

    bufferCondition_.notify_all();

    processFrame(queued.first, queued.second);
    return true;
}

void RealtimeProcessor::setProcessingMode(
    ProcessingMode mode,
    const std::unordered_map<std::string, double>& params) {
    params_.mode = mode;
    for (const auto& [key, value] : params) {
        params_.filterParams[key] = value;
    }
}

void RealtimeProcessor::addFilter(
    const std::string& filterName,
    const std::unordered_map<std::string, double>& params) {
    params_.filters.push_back(filterName);
    for (const auto& [key, value] : params) {
        params_.filterParams[key] = value;
    }
}

void RealtimeProcessor::removeFilter(const std::string& filterName) {
    auto& filters = params_.filters;
    filters.erase(std::remove(filters.begin(), filters.end(), filterName),
                  filters.end());
}

void RealtimeProcessor::clearFilters() {
    params_.filters.clear();
    params_.filterParams.clear();
}

void RealtimeProcessor::setFrameCallback(FrameCallback callback) {
    frameCallback_ = std::move(callback);
}

void RealtimeProcessor::setAnalysisCallback(AnalysisCallback callback) {
    analysisCallback_ = std::move(callback);
}

ProcessingStats RealtimeProcessor::getStatistics() const {
    std::scoped_lock lock(statsMutex_);
    return stats_;
}

double RealtimeProcessor::getCurrentFPS() const {
    std::scoped_lock lock(statsMutex_);
    return stats_.currentFPS;
}

double RealtimeProcessor::getLatency() const {
    std::scoped_lock lock(statsMutex_);
    return stats_.currentLatency;
}

bool RealtimeProcessor::isRunning() const { return running_.load(); }

void RealtimeProcessor::pause() {
    paused_.store(true);
    std::scoped_lock lock(statsMutex_);
    stats_.status = "paused";
}

void RealtimeProcessor::resume() {
    paused_.store(false);
    std::scoped_lock lock(statsMutex_);
    stats_.status = running_.load() ? "capturing" : "idle";
}

bool RealtimeProcessor::isPaused() const { return paused_.load(); }

bool RealtimeProcessor::startRecording(const std::string& outputPath,
                                       const std::string& /*codec*/,
                                       int /*quality*/) {
    if (outputPath.empty()) {
        return false;
    }

    recording_.store(true);
    recordingPath_ = outputPath;

    std::scoped_lock lock(statsMutex_);
    stats_.status = "recording";

    return true;
}

void RealtimeProcessor::stopRecording() {
    recording_.store(false);
    std::scoped_lock lock(statsMutex_);
    stats_.status = running_.load() ? "capturing" : "idle";
}

bool RealtimeProcessor::isRecording() const { return recording_.load(); }

bool RealtimeProcessor::takeSnapshot(const std::string& outputPath) {
    return !outputPath.empty();
}

void RealtimeProcessor::setTargetFPS(double fps) { params_.targetFPS = fps; }

void RealtimeProcessor::setMaxBufferSize(int size) {
    params_.maxBufferSize = size;
}

void RealtimeProcessor::setFrameDropping(bool enable) {
    params_.dropFrames = enable;
}

std::vector<std::string> RealtimeProcessor::getAvailableDevices() const {
    return {"synthetic"};
}

std::vector<std::string> RealtimeProcessor::getSupportedFormats(
    const std::string& /*devicePath*/) const {
    return {"RGB", "BGR"};
}

bool RealtimeProcessor::setCaptureResolution(int width, int height) {
    params_.maxWidth = width;
    params_.maxHeight = height;
    return true;
}

bool RealtimeProcessor::setCaptureFPS(double fps) {
    params_.targetFPS = fps;
    return true;
}

/**
 * @brief Stub processing thread (no-op)
 * @note This is a stub implementation. No actual frame processing occurs.
 */
void RealtimeProcessor::processingThread() {
    // STUB: No actual processing thread in minimal build
}

/**
 * @brief Stub capture thread (no-op)
 * @note This is a stub implementation. No actual frame capture occurs.
 */
void RealtimeProcessor::captureThread() {
    // STUB: No actual capture thread in minimal build
}

/**
 * @brief Stub processing pipeline (returns input unchanged)
 * @param input Input frame
 * @param frameInfo Frame information (unused in stub)
 * @return Input frame unchanged
 * @note This is a stub implementation. No actual processing is applied.
 */
blob RealtimeProcessor::applyProcessingPipeline(
    const blob& input, const FrameInfo& /*frameInfo*/) {
    // STUB: Return input unchanged - no processing in minimal build
    return input;
}

void RealtimeProcessor::updateStatistics(double processingTime) {
    std::scoped_lock lock(statsMutex_);

    stats_.framesProcessed++;
    stats_.currentLatency = clampPositive(processingTime);

    const auto count = static_cast<double>(stats_.framesProcessed);
    if (count <= 1.0) {
        stats_.averageLatency = stats_.currentLatency;
    } else {
        stats_.averageLatency =
            ((stats_.averageLatency * (count - 1.0)) + stats_.currentLatency) /
            count;
    }

    stats_.currentFPS = stats_.currentLatency > 0.0
                            ? kMillisInSecond / stats_.currentLatency
                            : 0.0;
    stats_.averageFPS = stats_.framesProcessed > 0 ? stats_.currentFPS : 0.0;
    stats_.status = running_.load()         ? "capturing"
                    : stats_.status.empty() ? "idle"
                                            : stats_.status;
}

/**
 * @brief Stub capture initialization (only accepts SYNTHETIC source)
 * @param source Capture source type
 * @param sourcePath Source path (unused in stub)
 * @return True only if source is SYNTHETIC
 * @note This is a stub implementation. Only SYNTHETIC source is supported.
 */
bool RealtimeProcessor::initializeCapture(CaptureSource source,
                                          const std::string& /*sourcePath*/) {
    // STUB: Only accept synthetic source in minimal build
    return source == CaptureSource::SYNTHETIC;
}

/**
 * @brief Stub capture cleanup (no-op)
 * @note This is a stub implementation. No cleanup is performed.
 */
void RealtimeProcessor::cleanupCapture() {
    // STUB: No cleanup needed in minimal build
}

/**
 * @brief Stub frame resize (returns input unchanged)
 * @param input Input frame
 * @return Input frame unchanged
 * @note This is a stub implementation. No resizing is performed.
 */
blob RealtimeProcessor::resizeFrame(const blob& input) {
    if (input.isEmpty()) {
        return {};
    }
    // STUB: Return input unchanged - no resizing in minimal build
    return input;
}

/**
 * @brief Stub format conversion (returns input unchanged)
 * @param input Input frame
 * @param targetFormat Target format (unused in stub)
 * @return Input frame unchanged
 * @note This is a stub implementation. No format conversion is performed.
 */
blob RealtimeProcessor::convertFormat(const blob& input,
                                      const std::string& /*targetFormat*/) {
    if (input.isEmpty()) {
        return {};
    }
    // STUB: Return input unchanged - no format conversion in minimal build
    return input;
}

std::unique_ptr<RealtimeProcessor> createOptimalRealtimeProcessor(
    bool /*useGPU*/, int /*numThreads*/) {
    return std::make_unique<RealtimeProcessor>();
}

}  // namespace atom::image
