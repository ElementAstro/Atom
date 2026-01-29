// video_export.cpp
#include "video_export.h"

#include <chrono>
#include <iomanip>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <sstream>

namespace serastro {

VideoExporter::VideoExporter()
    : debayer_(std::make_unique<DebayerProcessor>()) {}

VideoExporter::VideoExporter(const VideoExportParams& params)
    : params_(params),
      debayer_(std::make_unique<DebayerProcessor>(params.debayerParams)) {}

ExportResult VideoExporter::exportToVideo(
    const std::filesystem::path& inputPath,
    const std::filesystem::path& outputPath,
    ProgressCallback progressCallback) {
    ExportResult result;
    result.outputPath = outputPath;
    cancelRequested_ = false;

    auto startTime = std::chrono::high_resolution_clock::now();

    try {
        // Open SER file
        SERReader reader(inputPath);
        const auto& header = reader.getHeader();

        // Determine frame range
        size_t totalFrames = reader.getFrameCount();
        size_t startFrame = params_.startFrame;
        size_t endFrame = params_.endFrame > 0
                              ? std::min(params_.endFrame, totalFrames)
                              : totalFrames;

        if (startFrame >= endFrame) {
            result.errorMessage = "Invalid frame range";
            return result;
        }

        // Calculate output size
        cv::Size inputSize(header.imageWidth, header.imageHeight);
        cv::Size outputSize = getOutputSize(inputSize);

        // Create video writer
        cv::VideoWriter writer = createWriter(outputPath, outputSize);

        if (!writer.isOpened()) {
            result.errorMessage = "Failed to create video writer";
            return result;
        }

        // Setup debayer if needed
        if (params_.applyDebayer && header.isBayerPattern()) {
            debayer_->setBayerPattern(header.getColorIDEnum());
        }

        // Export frames
        size_t framesWritten = 0;
        size_t framesToProcess =
            (endFrame - startFrame + params_.frameStep - 1) / params_.frameStep;

        for (size_t i = startFrame; i < endFrame; i += params_.frameStep) {
            if (cancelRequested_) {
                result.errorMessage = "Export cancelled";
                break;
            }

            // Read frame
            cv::Mat frame = reader.readFrame(i);

            // Get timestamp if needed
            std::optional<SERTimestamp> timestamp;
            if (params_.addTimestamps && reader.hasTimestamps()) {
                timestamp = reader.getTimestamp(i);
            }

            // Process frame
            frame = processFrame(frame);

            // Add annotations
            if (params_.addTimestamps || params_.addFrameNumbers) {
                frame = addAnnotations(frame, i, timestamp);
            }

            // Resize if needed
            if (frame.size() != outputSize) {
                cv::resize(frame, frame, outputSize, 0, 0, cv::INTER_LANCZOS4);
            }

            // Ensure BGR format for video
            if (frame.channels() == 1) {
                cv::cvtColor(frame, frame, cv::COLOR_GRAY2BGR);
            }

            // Convert to 8-bit if needed
            if (frame.depth() != CV_8U) {
                frame.convertTo(frame, CV_8U, 255.0 / 65535.0);
            }

            // Write frame
            writer.write(frame);
            ++framesWritten;

            // Report progress
            if (progressCallback) {
                ExportProgress progress;
                progress.currentFrame = framesWritten;
                progress.totalFrames = framesToProcess;

                auto now = std::chrono::high_resolution_clock::now();
                progress.elapsedSeconds =
                    std::chrono::duration<double>(now - startTime).count();

                if (framesWritten > 0) {
                    progress.fps = framesWritten / progress.elapsedSeconds;
                    progress.estimatedRemaining =
                        (framesToProcess - framesWritten) / progress.fps;
                }

                progress.status = "Exporting frame " + std::to_string(i);
                progressCallback(progress);
            }
        }

        writer.release();

        result.success = !cancelRequested_;
        result.framesExported = framesWritten;
        result.durationSeconds = framesWritten / params_.frameRate;

        if (std::filesystem::exists(outputPath)) {
            result.fileSizeBytes = std::filesystem::file_size(outputPath);
        }

    } catch (const std::exception& e) {
        result.errorMessage = e.what();
    }

    auto endTime = std::chrono::high_resolution_clock::now();
    result.processingTimeSeconds =
        std::chrono::duration<double>(endTime - startTime).count();

    return result;
}

ExportResult VideoExporter::exportFrames(
    const std::vector<cv::Mat>& frames, const std::filesystem::path& outputPath,
    ProgressCallback progressCallback) {
    ExportResult result;
    result.outputPath = outputPath;
    cancelRequested_ = false;

    if (frames.empty()) {
        result.errorMessage = "No frames to export";
        return result;
    }

    auto startTime = std::chrono::high_resolution_clock::now();

    try {
        cv::Size outputSize = getOutputSize(frames[0].size());
        cv::VideoWriter writer = createWriter(outputPath, outputSize);

        if (!writer.isOpened()) {
            result.errorMessage = "Failed to create video writer";
            return result;
        }

        for (size_t i = 0; i < frames.size(); ++i) {
            if (cancelRequested_) {
                break;
            }

            cv::Mat frame = processFrame(frames[i]);

            if (frame.size() != outputSize) {
                cv::resize(frame, frame, outputSize, 0, 0, cv::INTER_LANCZOS4);
            }

            if (frame.channels() == 1) {
                cv::cvtColor(frame, frame, cv::COLOR_GRAY2BGR);
            }

            if (frame.depth() != CV_8U) {
                frame.convertTo(frame, CV_8U, 255.0 / 65535.0);
            }

            writer.write(frame);

            if (progressCallback) {
                ExportProgress progress;
                progress.currentFrame = i + 1;
                progress.totalFrames = frames.size();
                progress.status = "Writing frame " + std::to_string(i + 1);
                progressCallback(progress);
            }
        }

        writer.release();

        result.success = !cancelRequested_;
        result.framesExported = frames.size();
        result.durationSeconds = frames.size() / params_.frameRate;

        if (std::filesystem::exists(outputPath)) {
            result.fileSizeBytes = std::filesystem::file_size(outputPath);
        }

    } catch (const std::exception& e) {
        result.errorMessage = e.what();
    }

    auto endTime = std::chrono::high_resolution_clock::now();
    result.processingTimeSeconds =
        std::chrono::duration<double>(endTime - startTime).count();

    return result;
}

ExportResult VideoExporter::exportWithProcessing(
    const std::filesystem::path& inputPath,
    const std::filesystem::path& outputPath,
    std::shared_ptr<FrameProcessor> processor,
    ProgressCallback progressCallback) {
    auto originalProcessor = params_.processor;
    params_.processor = processor;
    params_.applyProcessing = true;

    auto result = exportToVideo(inputPath, outputPath, progressCallback);

    params_.processor = originalProcessor;
    params_.applyProcessing = (originalProcessor != nullptr);

    return result;
}

ExportResult VideoExporter::createTimeLapse(
    const std::filesystem::path& inputPath,
    const std::filesystem::path& outputPath, double speedFactor,
    ProgressCallback progressCallback) {
    auto originalStep = params_.frameStep;
    auto originalRate = params_.frameRate;

    params_.frameStep = std::max<size_t>(1, static_cast<size_t>(speedFactor));
    params_.frameRate = originalRate;  // Keep output frame rate

    auto result = exportToVideo(inputPath, outputPath, progressCallback);

    params_.frameStep = originalStep;
    params_.frameRate = originalRate;

    return result;
}

bool VideoExporter::exportFrame(const std::filesystem::path& inputPath,
                                const std::filesystem::path& outputPath,
                                size_t frameIndex) {
    try {
        SERReader reader(inputPath);

        if (frameIndex >= reader.getFrameCount()) {
            return false;
        }

        cv::Mat frame = reader.readFrame(frameIndex);
        frame = processFrame(frame);

        return cv::imwrite(outputPath.string(), frame);

    } catch (...) {
        return false;
    }
}

size_t VideoExporter::exportImageSequence(
    const std::filesystem::path& inputPath,
    const std::filesystem::path& outputDir, const std::string& format,
    ProgressCallback progressCallback) {
    cancelRequested_ = false;

    try {
        // Create output directory
        std::filesystem::create_directories(outputDir);

        SERReader reader(inputPath);
        size_t totalFrames = reader.getFrameCount();

        size_t startFrame = params_.startFrame;
        size_t endFrame = params_.endFrame > 0
                              ? std::min(params_.endFrame, totalFrames)
                              : totalFrames;

        size_t exported = 0;

        for (size_t i = startFrame; i < endFrame; i += params_.frameStep) {
            if (cancelRequested_) {
                break;
            }

            cv::Mat frame = reader.readFrame(i);
            frame = processFrame(frame);

            // Build output filename
            std::ostringstream filename;
            filename << "frame_" << std::setw(6) << std::setfill('0') << i
                     << "." << format;

            auto framePath = outputDir / filename.str();
            cv::imwrite(framePath.string(), frame);
            ++exported;

            if (progressCallback) {
                ExportProgress progress;
                progress.currentFrame = exported;
                progress.totalFrames =
                    (endFrame - startFrame) / params_.frameStep;
                progress.status = "Exporting " + filename.str();
                progressCallback(progress);
            }
        }

        return exported;

    } catch (...) {
        return 0;
    }
}

void VideoExporter::setParameters(const VideoExportParams& params) {
    params_ = params;
    debayer_ = std::make_unique<DebayerProcessor>(params.debayerParams);
}

std::vector<std::string> VideoExporter::getSupportedCodecs() {
    return {"H264", "H265", "VP9", "AV1", "MJPEG", "XVID", "FFV1", "RAW"};
}

int VideoExporter::getFourCC(VideoCodec codec) {
    switch (codec) {
        case VideoCodec::H264:
            return cv::VideoWriter::fourcc('a', 'v', 'c', '1');
        case VideoCodec::H265:
            return cv::VideoWriter::fourcc('h', 'e', 'v', '1');
        case VideoCodec::VP9:
            return cv::VideoWriter::fourcc('V', 'P', '9', '0');
        case VideoCodec::AV1:
            return cv::VideoWriter::fourcc('a', 'v', '0', '1');
        case VideoCodec::MJPEG:
            return cv::VideoWriter::fourcc('M', 'J', 'P', 'G');
        case VideoCodec::XVID:
            return cv::VideoWriter::fourcc('X', 'V', 'I', 'D');
        case VideoCodec::FFV1:
            return cv::VideoWriter::fourcc('F', 'F', 'V', '1');
        case VideoCodec::RAW:
            return 0;
        case VideoCodec::AUTO:
        default:
            return cv::VideoWriter::fourcc('a', 'v', 'c', '1');
    }
}

bool VideoExporter::isCodecAvailable(VideoCodec codec) {
    // Create a test writer to check codec availability
    cv::VideoWriter testWriter;
    int fourcc = getFourCC(codec);

    // Try to open with a small test size
    std::string testPath = std::filesystem::temp_directory_path().string() +
                           "/codec_test" + getRecommendedExtension(codec);

    bool available = testWriter.open(testPath, fourcc, 30.0, cv::Size(64, 64));

    testWriter.release();
    std::filesystem::remove(testPath);

    return available;
}

std::string VideoExporter::getRecommendedExtension(VideoCodec codec) {
    switch (codec) {
        case VideoCodec::H264:
        case VideoCodec::H265:
            return ".mp4";
        case VideoCodec::VP9:
        case VideoCodec::AV1:
            return ".webm";
        case VideoCodec::MJPEG:
        case VideoCodec::XVID:
            return ".avi";
        case VideoCodec::FFV1:
            return ".mkv";
        case VideoCodec::RAW:
            return ".avi";
        default:
            return ".mp4";
    }
}

cv::Mat VideoExporter::processFrame(const cv::Mat& frame) const {
    cv::Mat result = frame.clone();

    // Apply debayering if needed
    if (params_.applyDebayer && frame.channels() == 1) {
        result = debayer_->process(result);
    }

    // Apply custom processing
    if (params_.applyProcessing && params_.processor) {
        result = params_.processor->process(result);
    }

    return result;
}

cv::Mat VideoExporter::addAnnotations(
    const cv::Mat& frame, size_t frameIndex,
    const std::optional<SERTimestamp>& timestamp) const {
    cv::Mat result = frame.clone();

    // Ensure color for text overlay
    if (result.channels() == 1) {
        cv::cvtColor(result, result, cv::COLOR_GRAY2BGR);
    }

    int fontFace = cv::FONT_HERSHEY_SIMPLEX;
    double fontScale = params_.fontSize;
    int thickness = std::max(1, static_cast<int>(params_.fontSize * 2));

    int yPos = 30;
    int xPos = 10;

    // Add frame number
    if (params_.addFrameNumbers) {
        std::string text = "Frame: " + std::to_string(frameIndex);
        cv::putText(result, text, cv::Point(xPos, yPos), fontFace, fontScale,
                    params_.fontColor, thickness);
        yPos += static_cast<int>(30 * fontScale);
    }

    // Add timestamp
    if (params_.addTimestamps && timestamp) {
        auto tp = timestamp->toTimePoint();
        auto time = std::chrono::system_clock::to_time_t(tp);

        std::ostringstream oss;
        oss << std::put_time(std::localtime(&time), "%Y-%m-%d %H:%M:%S");

        cv::putText(result, oss.str(), cv::Point(xPos, yPos), fontFace,
                    fontScale, params_.fontColor, thickness);
    }

    return result;
}

cv::Size VideoExporter::getOutputSize(const cv::Size& inputSize) const {
    if (params_.preserveResolution) {
        return inputSize;
    }

    if (params_.outputWidth > 0 && params_.outputHeight > 0) {
        if (params_.maintainAspectRatio) {
            double inputAspect =
                static_cast<double>(inputSize.width) / inputSize.height;
            double outputAspect =
                static_cast<double>(params_.outputWidth) / params_.outputHeight;

            if (inputAspect > outputAspect) {
                return cv::Size(
                    params_.outputWidth,
                    static_cast<int>(params_.outputWidth / inputAspect));
            } else {
                return cv::Size(
                    static_cast<int>(params_.outputHeight * inputAspect),
                    params_.outputHeight);
            }
        }
        return cv::Size(params_.outputWidth, params_.outputHeight);
    }

    return inputSize;
}

cv::VideoWriter VideoExporter::createWriter(
    const std::filesystem::path& outputPath, const cv::Size& frameSize) const {
    int fourcc = getFourCC(params_.codec);

    cv::VideoWriter writer;
    writer.open(outputPath.string(), fourcc, params_.frameRate, frameSize,
                true);

    return writer;
}

// Utility functions

bool quickExportToVideo(const std::filesystem::path& serPath,
                        const std::filesystem::path& videoPath) {
    VideoExporter exporter;
    auto result = exporter.exportToVideo(serPath, videoPath, nullptr);
    return result.success;
}

std::string videoCodecToString(VideoCodec codec) {
    switch (codec) {
        case VideoCodec::H264:
            return "H264";
        case VideoCodec::H265:
            return "H265";
        case VideoCodec::VP9:
            return "VP9";
        case VideoCodec::AV1:
            return "AV1";
        case VideoCodec::MJPEG:
            return "MJPEG";
        case VideoCodec::XVID:
            return "XVID";
        case VideoCodec::FFV1:
            return "FFV1";
        case VideoCodec::RAW:
            return "RAW";
        case VideoCodec::AUTO:
            return "AUTO";
    }
    return "Unknown";
}

VideoCodec videoCodecFromString(const std::string& name) {
    if (name == "H264" || name == "h264")
        return VideoCodec::H264;
    if (name == "H265" || name == "h265" || name == "HEVC")
        return VideoCodec::H265;
    if (name == "VP9" || name == "vp9")
        return VideoCodec::VP9;
    if (name == "AV1" || name == "av1")
        return VideoCodec::AV1;
    if (name == "MJPEG" || name == "mjpeg")
        return VideoCodec::MJPEG;
    if (name == "XVID" || name == "xvid")
        return VideoCodec::XVID;
    if (name == "FFV1" || name == "ffv1")
        return VideoCodec::FFV1;
    if (name == "RAW" || name == "raw")
        return VideoCodec::RAW;
    return VideoCodec::AUTO;
}

}  // namespace serastro
