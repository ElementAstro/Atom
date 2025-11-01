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

void RealtimeProcessor::processingThread() {}

void RealtimeProcessor::captureThread() {}

blob RealtimeProcessor::applyProcessingPipeline(
    const blob& input, const FrameInfo& /*frameInfo*/) {
    blob output = input;
    return output;
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

bool RealtimeProcessor::initializeCapture(CaptureSource source,
                                          const std::string& /*sourcePath*/) {
    return source == CaptureSource::SYNTHETIC;
}

void RealtimeProcessor::cleanupCapture() {}

blob RealtimeProcessor::resizeFrame(const blob& input) {
    if (input.isEmpty()) {
        return {};
    }
    blob output = input;
    return output;
}

blob RealtimeProcessor::convertFormat(const blob& input,
                                      const std::string& /*targetFormat*/) {
    if (input.isEmpty()) {
        return {};
    }
    blob output = input;
    return output;
}

std::unique_ptr<RealtimeProcessor> createOptimalRealtimeProcessor(
    bool /*useGPU*/, int /*numThreads*/) {
    return std::make_unique<RealtimeProcessor>();
}

}  // namespace atom::image
