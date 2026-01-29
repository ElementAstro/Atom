#include "realtime.hpp"
#include <stdexcept>

// Define error macros to avoid atom error system namespace pollution
#define THROW_RUNTIME_ERROR(msg) throw std::runtime_error(msg)
#define THROW_INVALID_ARGUMENT(msg) throw std::invalid_argument(msg)
#include <algorithm>
#include <chrono>
#include <thread>

#ifdef ATOM_IMAGE_HAS_OPENCV
#include <opencv2/imgproc.hpp>
#include <opencv2/opencv.hpp>
#include <opencv2/videoio.hpp>
#endif

namespace atom::image {

bool RealtimeProcessor::initialize(const RealtimeParams& params) {
    try {
        // Store parameters
        targetFPS_ = params.targetFPS;
        maxBufferSize_ = params.maxBufferSize;
        enableFrameDropping_ = params.dropFrames;
        processingMode_ = params.mode;

        // Initialize statistics
        stats_ = ProcessingStats{};

        return true;
    } catch (const std::exception&) {
        return false;
    }
}

bool RealtimeProcessor::startCapture(CaptureSource source,
                                     const std::string& sourcePath,
                                     FrameCallback frameCallback,
                                     AnalysisCallback analysisCallback) {
    if (running_.load()) {
        return false;  // Already running
    }

    // Store callbacks
    frameCallback_ = frameCallback;
    analysisCallback_ = analysisCallback;

    // Initialize capture source
    if (!initializeCapture(source, sourcePath)) {
        return false;
    }

    // Start processing
    running_.store(true);
    paused_.store(false);

    // Start threads
    captureThread_ = std::thread(&RealtimeProcessor::captureThread, this);
    processingThread_ = std::thread(&RealtimeProcessor::processingThread, this);

    return true;
}

void RealtimeProcessor::stop() {
    if (!running_.load()) {
        return;
    }

    running_.store(false);

    // Notify threads to wake up
    frameCondition_.notify_all();

    // Wait for threads to finish
    if (captureThread_.joinable()) {
        captureThread_.join();
    }
    if (processingThread_.joinable()) {
        processingThread_.join();
    }

    // Cleanup resources
    cleanupCapture();

    // Clear frame buffer
    std::lock_guard<std::mutex> lock(frameMutex_);
    while (!frameBuffer_.empty())
        frameBuffer_.pop();
}

blob RealtimeProcessor::processFrame(const blob& input,
                                     const FrameInfo& frameInfo) {
    if (input.isEmpty()) {
        return blob{};
    }

    auto startTime = std::chrono::high_resolution_clock::now();

    // Apply processing pipeline
    blob result = applyProcessingPipeline(input, frameInfo);

    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        endTime - startTime);

    // Update statistics
    updateStatistics(static_cast<double>(duration.count()));

    return result;
}

bool RealtimeProcessor::addFrame(const blob& frame,
                                 const FrameInfo& frameInfo) {
    if (frame.isEmpty()) {
        return false;
    }

    std::lock_guard<std::mutex> lock(frameMutex_);

    // Check buffer size
    if (static_cast<int>(frameBuffer_.size()) >= maxBufferSize_) {
        if (enableFrameDropping_) {
            // Drop oldest frame
            frameBuffer_.pop();
            stats_.framesDropped++;
        } else {
            return false;  // Buffer full
        }
    }

    // Add frame to buffer
    frameBuffer_.push({frame, frameInfo});
    frameCondition_.notify_one();

    return true;
}

void RealtimeProcessor::setProcessingMode(
    ProcessingMode mode,
    const std::unordered_map<std::string, double>& params) {
    processingMode_ = mode;
    modeParams_ = params;
}

void RealtimeProcessor::addFilter(
    const std::string& filterName,
    const std::unordered_map<std::string, double>& params) {
    std::lock_guard<std::mutex> lock(filterMutex_);
    filters_[filterName] = params;
}

void RealtimeProcessor::removeFilter(const std::string& filterName) {
    std::lock_guard<std::mutex> lock(filterMutex_);
    filters_.erase(filterName);
}

void RealtimeProcessor::clearFilters() {
    std::lock_guard<std::mutex> lock(filterMutex_);
    filters_.clear();
}

void RealtimeProcessor::setFrameCallback(FrameCallback callback) {
    frameCallback_ = callback;
}

void RealtimeProcessor::setAnalysisCallback(AnalysisCallback callback) {
    analysisCallback_ = callback;
}

ProcessingStats RealtimeProcessor::getStatistics() const { return stats_; }

double RealtimeProcessor::getCurrentFPS() const { return stats_.currentFPS; }

double RealtimeProcessor::getLatency() const { return stats_.averageLatency; }

bool RealtimeProcessor::isRunning() const { return running_.load(); }

void RealtimeProcessor::pause() { paused_.store(true); }

void RealtimeProcessor::resume() {
    paused_.store(false);
    frameCondition_.notify_all();
}

bool RealtimeProcessor::isPaused() const { return paused_.load(); }

bool RealtimeProcessor::startRecording(const std::string& outputPath,
                                       const std::string& codec, int quality) {
    if (recording_.load()) {
        return false;  // Already recording
    }

    try {
#ifdef ATOM_IMAGE_HAS_OPENCV
        // Initialize video writer
        int fourcc =
            cv::VideoWriter::fourcc('H', '2', '6', '4');  // Default to H264
        if (codec == "h265") {
            fourcc = cv::VideoWriter::fourcc('H', '2', '6', '5');
        } else if (codec == "vp9") {
            fourcc = cv::VideoWriter::fourcc('V', 'P', '0', '9');
        }

        // Use current capture resolution or default
        cv::Size frameSize(1920, 1080);  // Default resolution

        videoWriter_ = std::make_unique<cv::VideoWriter>(outputPath, fourcc,
                                                         targetFPS_, frameSize);

        if (!videoWriter_->isOpened()) {
            return false;
        }

        recording_.store(true);
        return true;
#else
        return false;  // OpenCV required for recording
#endif
    } catch (const std::exception&) {
        return false;
    }
}

void RealtimeProcessor::stopRecording() {
    if (!recording_.load()) {
        return;
    }

    recording_.store(false);

#ifdef ATOM_IMAGE_HAS_OPENCV
    if (videoWriter_) {
        videoWriter_->release();
        videoWriter_.reset();
    }
#endif
}

bool RealtimeProcessor::isRecording() const { return recording_.load(); }

bool RealtimeProcessor::takeSnapshot(const std::string& outputPath) {
    if (!running_.load() || frameBuffer_.empty()) {
        return false;
    }

    try {
        std::lock_guard<std::mutex> lock(frameMutex_);
        if (!frameBuffer_.empty()) {
            const auto& frameData = frameBuffer_.back();

#ifdef ATOM_IMAGE_HAS_OPENCV
            cv::Mat frame = frameData.first.to_mat();
            return cv::imwrite(outputPath, frame);
#else
            return false;  // OpenCV required for snapshot
#endif
        }
    } catch (const std::exception&) {
        return false;
    }

    return false;
}

void RealtimeProcessor::setTargetFPS(double fps) { targetFPS_ = fps; }

void RealtimeProcessor::setMaxBufferSize(int size) { maxBufferSize_ = size; }

void RealtimeProcessor::setFrameDropping(bool enable) {
    enableFrameDropping_ = enable;
}

std::vector<std::string> RealtimeProcessor::getAvailableDevices() const {
    std::vector<std::string> devices;

#ifdef ATOM_IMAGE_HAS_OPENCV
    // Try to enumerate camera devices
    for (int i = 0; i < 10; ++i) {
        cv::VideoCapture cap(i);
        if (cap.isOpened()) {
            devices.push_back("Camera " + std::to_string(i));
            cap.release();
        }
    }
#endif

    return devices;
}

std::vector<std::string> RealtimeProcessor::getSupportedFormats(
    const std::string& devicePath) const {
    // Return common video formats
    return {"BGR", "RGB", "GRAY", "YUV420", "MJPEG", "H264"};
}

bool RealtimeProcessor::setCaptureResolution(int width, int height) {
    captureWidth_ = width;
    captureHeight_ = height;

#ifdef ATOM_IMAGE_HAS_OPENCV
    if (capture_) {
        capture_->set(cv::CAP_PROP_FRAME_WIDTH, width);
        capture_->set(cv::CAP_PROP_FRAME_HEIGHT, height);
        return true;
    }
#endif

    return false;
}

bool RealtimeProcessor::setCaptureFPS(double fps) {
    captureFPS_ = fps;

#ifdef ATOM_IMAGE_HAS_OPENCV
    if (capture_) {
        capture_->set(cv::CAP_PROP_FPS, fps);
        return true;
    }
#endif

    return false;
}

void RealtimeProcessor::processingThread() {
    while (running_.load()) {
        if (paused_.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }

        std::unique_lock<std::mutex> lock(frameMutex_);
        frameCondition_.wait(
            lock, [this] { return !frameBuffer_.empty() || !running_.load(); });

        if (!running_.load()) {
            break;
        }

        if (frameBuffer_.empty()) {
            continue;
        }

        // Get frame from buffer
        auto frameData = frameBuffer_.front();
        frameBuffer_.pop();
        lock.unlock();

        // Process frame
        blob processedFrame = processFrame(frameData.first, frameData.second);

        // Call frame callback if set
        if (frameCallback_) {
            frameCallback_(processedFrame, frameData.second);
        }

        // Record frame if recording
        if (recording_.load() && videoWriter_) {
#ifdef ATOM_IMAGE_HAS_OPENCV
            cv::Mat frame = processedFrame.to_mat();
            videoWriter_->write(frame);
#endif
        }

        // Update frame count
        stats_.processedFrames++;
    }
}

void RealtimeProcessor::captureThread() {
    auto lastFrameTime = std::chrono::high_resolution_clock::now();
    double frameInterval = 1000.0 / targetFPS_;  // milliseconds

    while (running_.load()) {
        if (paused_.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }

        auto currentTime = std::chrono::high_resolution_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            currentTime - lastFrameTime);

        if (elapsed.count() < frameInterval) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            continue;
        }

#ifdef ATOM_IMAGE_HAS_OPENCV
        if (capture_ && capture_->isOpened()) {
            cv::Mat frame;
            if (capture_->read(frame)) {
                if (!frame.empty()) {
                    FrameInfo frameInfo;
                    frameInfo.timestamp =
                        std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::system_clock::now().time_since_epoch())
                            .count();
                    frameInfo.frameNumber = stats_.capturedFrames;

                    blob frameBlob(frame);
                    addFrame(frameBlob, frameInfo);

                    stats_.capturedFrames++;
                    lastFrameTime = currentTime;
                }
            }
        }
#endif
    }
}

blob RealtimeProcessor::applyProcessingPipeline(const blob& input,
                                                const FrameInfo& frameInfo) {
    if (input.isEmpty()) {
        return blob{};
    }

    blob result = input;

    // Apply processing based on mode
    switch (processingMode_) {
        case ProcessingMode::PASSTHROUGH:
            // No processing
            break;

        case ProcessingMode::FILTER:
            // Apply filters
            {
                std::lock_guard<std::mutex> lock(filterMutex_);
                for (const auto& filter : filters_) {
                    result = applyFilter(result, filter.first, filter.second);
                }
            }
            break;

        case ProcessingMode::ENHANCE:
            // Apply enhancement
            result = applyEnhancement(result);
            break;

        case ProcessingMode::DETECT:
            // Apply detection
            result = applyDetection(result, frameInfo);
            break;

        case ProcessingMode::TRACK:
            // Apply tracking
            result = applyTracking(result, frameInfo);
            break;

        default:
            break;
    }

    return result;
}

void RealtimeProcessor::updateStatistics(double processingTime) {
    stats_.totalProcessingTime += processingTime;
    stats_.averageLatency =
        stats_.totalProcessingTime /
        std::max(static_cast<int64_t>(1), stats_.processedFrames);

    // Calculate FPS
    auto currentTime = std::chrono::high_resolution_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
        currentTime - stats_.startTime);
    if (elapsed.count() > 0) {
        stats_.currentFPS =
            static_cast<double>(stats_.processedFrames) / elapsed.count();
    }
}

bool RealtimeProcessor::initializeCapture(CaptureSource source,
                                          const std::string& sourcePath) {
#ifdef ATOM_IMAGE_HAS_OPENCV
    try {
        switch (source) {
            case CaptureSource::CAMERA: {
                int deviceIndex = 0;
                try {
                    deviceIndex = std::stoi(sourcePath);
                } catch (...) {
                    deviceIndex = 0;
                }
                capture_ = std::make_unique<cv::VideoCapture>(deviceIndex);
                break;
            }
            case CaptureSource::FILE:
                capture_ = std::make_unique<cv::VideoCapture>(sourcePath);
                break;
            case CaptureSource::STREAM:
                capture_ = std::make_unique<cv::VideoCapture>(sourcePath);
                break;
            default:
                return false;
        }

        if (!capture_ || !capture_->isOpened()) {
            return false;
        }

        // Set capture properties if specified
        if (captureWidth_ > 0 && captureHeight_ > 0) {
            capture_->set(cv::CAP_PROP_FRAME_WIDTH, captureWidth_);
            capture_->set(cv::CAP_PROP_FRAME_HEIGHT, captureHeight_);
        }

        if (captureFPS_ > 0) {
            capture_->set(cv::CAP_PROP_FPS, captureFPS_);
        }

        // Initialize statistics
        stats_.startTime = std::chrono::high_resolution_clock::now();

        return true;
    } catch (const std::exception&) {
        return false;
    }
#else
    return false;  // OpenCV required for capture
#endif
}

void RealtimeProcessor::cleanupCapture() {
#ifdef ATOM_IMAGE_HAS_OPENCV
    if (capture_) {
        capture_->release();
        capture_.reset();
    }
#endif
}

blob RealtimeProcessor::resizeFrame(const blob& input) {
    if (input.isEmpty() || (captureWidth_ <= 0 && captureHeight_ <= 0)) {
        return input;
    }

#ifdef ATOM_IMAGE_HAS_OPENCV
    cv::Mat src = input.to_mat();
    cv::Mat dst;
    cv::Size targetSize(captureWidth_ > 0 ? captureWidth_ : src.cols,
                        captureHeight_ > 0 ? captureHeight_ : src.rows);
    cv::resize(src, dst, targetSize);
    return blob(dst);
#else
    return input;
#endif
}

blob RealtimeProcessor::convertFormat(const blob& input,
                                      const std::string& targetFormat) {
    if (input.isEmpty()) {
        return input;
    }

#ifdef ATOM_IMAGE_HAS_OPENCV
    cv::Mat src = input.to_mat();
    cv::Mat dst;

    if (targetFormat == "GRAY" && src.channels() > 1) {
        cv::cvtColor(src, dst, cv::COLOR_BGR2GRAY);
    } else if (targetFormat == "RGB" && src.channels() == 3) {
        cv::cvtColor(src, dst, cv::COLOR_BGR2RGB);
    } else {
        dst = src.clone();
    }

    return blob(dst);
#else
    return input;
#endif
}

// Helper methods for processing pipeline
blob RealtimeProcessor::applyFilter(
    const blob& input, const std::string& filterName,
    const std::unordered_map<std::string, double>& params) {
    if (input.isEmpty()) {
        return input;
    }

#ifdef ATOM_IMAGE_HAS_OPENCV
    cv::Mat src = input.to_mat();
    cv::Mat dst;

    if (filterName == "blur") {
        int kernelSize = static_cast<int>(
            params.count("kernel_size") ? params.at("kernel_size") : 5);
        cv::blur(src, dst, cv::Size(kernelSize, kernelSize));
    } else if (filterName == "gaussian_blur") {
        int kernelSize = static_cast<int>(
            params.count("kernel_size") ? params.at("kernel_size") : 5);
        double sigma = params.count("sigma") ? params.at("sigma") : 1.0;
        cv::GaussianBlur(src, dst, cv::Size(kernelSize, kernelSize), sigma);
    } else if (filterName == "sharpen") {
        cv::Mat kernel =
            (cv::Mat_<float>(3, 3) << 0, -1, 0, -1, 5, -1, 0, -1, 0);
        cv::filter2D(src, dst, -1, kernel);
    } else {
        dst = src.clone();
    }

    return blob(dst);
#else
    return input;
#endif
}

blob RealtimeProcessor::applyEnhancement(const blob& input) {
    if (input.isEmpty()) {
        return input;
    }

#ifdef ATOM_IMAGE_HAS_OPENCV
    cv::Mat src = input.to_mat();
    cv::Mat dst;

    // Apply CLAHE for enhancement
    if (src.channels() == 3) {
        cv::Mat lab;
        cv::cvtColor(src, lab, cv::COLOR_BGR2Lab);
        std::vector<cv::Mat> channels;
        cv::split(lab, channels);

        auto clahe = cv::createCLAHE(2.0, cv::Size(8, 8));
        clahe->apply(channels[0], channels[0]);

        cv::merge(channels, lab);
        cv::cvtColor(lab, dst, cv::COLOR_Lab2BGR);
    } else {
        auto clahe = cv::createCLAHE(2.0, cv::Size(8, 8));
        clahe->apply(src, dst);
    }

    return blob(dst);
#else
    return input;
#endif
}

blob RealtimeProcessor::applyDetection(const blob& input,
                                       const FrameInfo& frameInfo) {
    if (input.isEmpty()) {
        return input;
    }

#ifdef ATOM_IMAGE_HAS_OPENCV
    cv::Mat src = input.to_mat();
    cv::Mat dst = src.clone();

    // Get detection parameters
    double threshold =
        modeParams_.count("threshold") ? modeParams_.at("threshold") : 0.5;
    std::string detectionType = "face";  // Default to face detection
    if (modeParams_.count("detection_type")) {
        // Map numeric value to detection type
        int typeVal = static_cast<int>(modeParams_.at("detection_type"));
        switch (typeVal) {
            case 0:
                detectionType = "face";
                break;
            case 1:
                detectionType = "motion";
                break;
            case 2:
                detectionType = "contour";
                break;
            default:
                detectionType = "face";
                break;
        }
    }

    std::vector<cv::Rect> detections;

    if (detectionType == "face") {
        // Face detection using Haar cascade
        cv::CascadeClassifier faceCascade;
        std::string cascadePath = "haarcascade_frontalface_default.xml";

        // Try to load cascade from common locations
        std::vector<std::string> cascadePaths = {
            cascadePath, "/usr/share/opencv4/haarcascades/" + cascadePath,
            "/usr/share/opencv/haarcascades/" + cascadePath,
            "C:/opencv/data/haarcascades/" + cascadePath};

        bool loaded = false;
        for (const auto& path : cascadePaths) {
            if (faceCascade.load(path)) {
                loaded = true;
                break;
            }
        }

        if (loaded) {
            cv::Mat gray;
            if (src.channels() > 1) {
                cv::cvtColor(src, gray, cv::COLOR_BGR2GRAY);
            } else {
                gray = src.clone();
            }
            cv::equalizeHist(gray, gray);

            faceCascade.detectMultiScale(gray, detections, 1.1, 3, 0,
                                         cv::Size(30, 30));
        }
    } else if (detectionType == "motion") {
        // Motion detection using frame differencing
        static cv::Mat prevFrame;
        if (!prevFrame.empty() && prevFrame.size() == src.size()) {
            cv::Mat gray, prevGray, diff;
            cv::cvtColor(src, gray, cv::COLOR_BGR2GRAY);
            cv::cvtColor(prevFrame, prevGray, cv::COLOR_BGR2GRAY);
            cv::absdiff(gray, prevGray, diff);
            cv::threshold(diff, diff, 25, 255, cv::THRESH_BINARY);

            // Find contours of motion regions
            std::vector<std::vector<cv::Point>> contours;
            cv::findContours(diff, contours, cv::RETR_EXTERNAL,
                             cv::CHAIN_APPROX_SIMPLE);

            for (const auto& contour : contours) {
                double area = cv::contourArea(contour);
                if (area > 500) {  // Minimum area threshold
                    detections.push_back(cv::boundingRect(contour));
                }
            }
        }
        prevFrame = src.clone();
    } else if (detectionType == "contour") {
        // General contour detection
        cv::Mat gray, edges;
        cv::cvtColor(src, gray, cv::COLOR_BGR2GRAY);
        cv::Canny(gray, edges, 50, 150);

        std::vector<std::vector<cv::Point>> contours;
        cv::findContours(edges, contours, cv::RETR_EXTERNAL,
                         cv::CHAIN_APPROX_SIMPLE);

        for (const auto& contour : contours) {
            double area = cv::contourArea(contour);
            if (area > 1000) {
                detections.push_back(cv::boundingRect(contour));
            }
        }
    }

    // Draw detection results
    for (const auto& rect : detections) {
        cv::rectangle(dst, rect, cv::Scalar(0, 255, 0), 2);
    }

    // Call analysis callback if set
    if (analysisCallback_ && !detections.empty()) {
        std::unordered_map<std::string, double> analysisResult;
        analysisResult["detection_count"] =
            static_cast<double>(detections.size());
        analysisResult["frame_number"] =
            static_cast<double>(frameInfo.frameNumber);
        analysisCallback_(analysisResult);
    }

    return blob(dst);
#else
    return input;
#endif
}

blob RealtimeProcessor::applyTracking(const blob& input,
                                      const FrameInfo& frameInfo) {
    if (input.isEmpty()) {
        return input;
    }

#ifdef ATOM_IMAGE_HAS_OPENCV
    cv::Mat src = input.to_mat();
    cv::Mat dst = src.clone();

    // Simple tracking using optical flow
    static cv::Mat prevGray;
    static std::vector<cv::Point2f> prevPoints;
    static bool initialized = false;

    cv::Mat gray;
    if (src.channels() > 1) {
        cv::cvtColor(src, gray, cv::COLOR_BGR2GRAY);
    } else {
        gray = src.clone();
    }

    if (!initialized || prevPoints.empty()) {
        // Initialize tracking points using good features to track
        int maxCorners = modeParams_.count("max_points")
                             ? static_cast<int>(modeParams_.at("max_points"))
                             : 100;
        double qualityLevel = modeParams_.count("quality_level")
                                  ? modeParams_.at("quality_level")
                                  : 0.01;
        double minDistance = modeParams_.count("min_distance")
                                 ? modeParams_.at("min_distance")
                                 : 10.0;

        cv::goodFeaturesToTrack(gray, prevPoints, maxCorners, qualityLevel,
                                minDistance);
        prevGray = gray.clone();
        initialized = true;

        // Draw initial points
        for (const auto& pt : prevPoints) {
            cv::circle(dst, pt, 3, cv::Scalar(0, 255, 0), -1);
        }
    } else {
        // Track points using Lucas-Kanade optical flow
        std::vector<cv::Point2f> nextPoints;
        std::vector<uchar> status;
        std::vector<float> err;

        cv::calcOpticalFlowPyrLK(prevGray, gray, prevPoints, nextPoints, status,
                                 err);

        // Draw tracking results
        std::vector<cv::Point2f> goodNew;
        for (size_t i = 0; i < nextPoints.size(); ++i) {
            if (status[i]) {
                goodNew.push_back(nextPoints[i]);

                // Draw line from previous to current position
                cv::line(dst, prevPoints[i], nextPoints[i],
                         cv::Scalar(0, 255, 0), 2);
                cv::circle(dst, nextPoints[i], 3, cv::Scalar(0, 0, 255), -1);
            }
        }

        // Re-detect points if too few remain
        if (goodNew.size() < 20) {
            std::vector<cv::Point2f> newPoints;
            cv::goodFeaturesToTrack(gray, newPoints, 100, 0.01, 10);
            goodNew.insert(goodNew.end(), newPoints.begin(), newPoints.end());
        }

        prevPoints = goodNew;
        prevGray = gray.clone();

        // Call analysis callback with tracking info
        if (analysisCallback_) {
            std::unordered_map<std::string, double> analysisResult;
            analysisResult["tracked_points"] =
                static_cast<double>(goodNew.size());
            analysisResult["frame_number"] =
                static_cast<double>(frameInfo.frameNumber);

            // Calculate average motion
            double avgMotionX = 0, avgMotionY = 0;
            int validCount = 0;
            for (size_t i = 0;
                 i < std::min(prevPoints.size(), nextPoints.size()); ++i) {
                if (i < status.size() && status[i]) {
                    avgMotionX += nextPoints[i].x - prevPoints[i].x;
                    avgMotionY += nextPoints[i].y - prevPoints[i].y;
                    validCount++;
                }
            }
            if (validCount > 0) {
                analysisResult["avg_motion_x"] = avgMotionX / validCount;
                analysisResult["avg_motion_y"] = avgMotionY / validCount;
            }

            analysisCallback_(analysisResult);
        }
    }

    return blob(dst);
#else
    return input;
#endif
}

// Factory function
std::unique_ptr<RealtimeProcessor> createRealtimeProcessor(
    const RealtimeParams& params) {
    auto processor = std::make_unique<RealtimeProcessor>();
    if (processor->initialize(params)) {
        return processor;
    }
    return nullptr;
}

// Factory function for optimal processor configuration
std::unique_ptr<RealtimeProcessor> createOptimalRealtimeProcessor(
    bool useGPU, int numThreads) {
    RealtimeParams params;
    params.useGPU = useGPU;
    params.numThreads =
        numThreads > 0 ? numThreads : std::thread::hardware_concurrency();
    return createRealtimeProcessor(params);
}

}  // namespace atom::image
