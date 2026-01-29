// video_export.h
#pragma once

#include "debayer.h"
#include "exception.h"
#include "frame_processor.h"
#include "ser_format.h"
#include "ser_reader.h"

#include <filesystem>
#include <functional>
#include <memory>
#include <opencv2/core.hpp>
#include <opencv2/videoio.hpp>
#include <string>
#include <vector>

namespace serastro {

/**
 * @enum VideoCodec
 * @brief Supported video codecs
 */
enum class VideoCodec {
    H264,   ///< H.264/AVC (most compatible)
    H265,   ///< H.265/HEVC (better compression)
    VP9,    ///< VP9 (WebM)
    AV1,    ///< AV1 (newest, best compression)
    MJPEG,  ///< Motion JPEG (high quality)
    XVID,   ///< XVID (legacy)
    FFV1,   ///< FFV1 (lossless)
    RAW,    ///< Uncompressed
    AUTO    ///< Auto-select based on extension
};

/**
 * @enum VideoContainer
 * @brief Video container formats
 */
enum class VideoContainer {
    MP4,   ///< MPEG-4 Part 14
    AVI,   ///< Audio Video Interleave
    MKV,   ///< Matroska
    MOV,   ///< QuickTime
    WEBM,  ///< WebM
    AUTO   ///< Auto-detect from extension
};

/**
 * @struct VideoExportParams
 * @brief Parameters for video export
 */
struct VideoExportParams {
    // Output settings
    VideoCodec codec = VideoCodec::H264;
    VideoContainer container = VideoContainer::MP4;
    double frameRate = 30.0;  ///< Output frame rate
    int quality = 85;         ///< Quality (0-100, codec dependent)
    int bitrate = 0;          ///< Bitrate in kbps (0 = auto)

    // Resolution
    bool preserveResolution = true;
    int outputWidth = 0;   ///< Target width (0 = original)
    int outputHeight = 0;  ///< Target height (0 = original)
    bool maintainAspectRatio = true;

    // Frame selection
    size_t startFrame = 0;  ///< Start frame index
    size_t endFrame = 0;    ///< End frame (0 = all)
    size_t frameStep = 1;   ///< Frame step (for time-lapse)

    // Processing
    bool applyDebayer = true;  ///< Apply debayering if needed
    DebayerParameters debayerParams;
    bool applyProcessing = false;  ///< Apply frame processor
    std::shared_ptr<FrameProcessor> processor;

    // Annotations
    bool addTimestamps = false;    ///< Overlay timestamps
    bool addFrameNumbers = false;  ///< Overlay frame numbers
    std::string fontFace = "Arial";
    double fontSize = 1.0;
    cv::Scalar fontColor = cv::Scalar(255, 255, 255);

    // Advanced
    bool useHardwareAccel = true;  ///< Use hardware encoding if available
    int threads = 0;               ///< Encoding threads (0 = auto)
};

/**
 * @struct ExportProgress
 * @brief Progress information during export
 */
struct ExportProgress {
    size_t currentFrame = 0;
    size_t totalFrames = 0;
    double elapsedSeconds = 0.0;
    double estimatedRemaining = 0.0;
    double fps = 0.0;  ///< Current processing FPS
    std::string status;
};

/**
 * @struct ExportResult
 * @brief Result of video export
 */
struct ExportResult {
    bool success = false;
    std::filesystem::path outputPath;
    size_t framesExported = 0;
    double durationSeconds = 0.0;
    size_t fileSizeBytes = 0;
    double processingTimeSeconds = 0.0;
    std::string errorMessage;
};

/**
 * @class VideoExporter
 * @brief Export SER files to video formats
 *
 * Converts SER astronomical video files to standard video
 * formats for sharing and playback.
 */
class VideoExporter {
public:
    using ProgressCallback = std::function<void(const ExportProgress&)>;

    /**
     * @brief Default constructor
     */
    VideoExporter();

    /**
     * @brief Construct with parameters
     * @param params Export parameters
     */
    explicit VideoExporter(const VideoExportParams& params);

    /**
     * @brief Export SER file to video
     * @param inputPath Path to SER file
     * @param outputPath Output video path
     * @param progressCallback Progress callback
     * @return Export result
     */
    ExportResult exportToVideo(const std::filesystem::path& inputPath,
                               const std::filesystem::path& outputPath,
                               ProgressCallback progressCallback = nullptr);

    /**
     * @brief Export frames to video
     * @param frames Vector of frames
     * @param outputPath Output video path
     * @param progressCallback Progress callback
     * @return Export result
     */
    ExportResult exportFrames(const std::vector<cv::Mat>& frames,
                              const std::filesystem::path& outputPath,
                              ProgressCallback progressCallback = nullptr);

    /**
     * @brief Export with frame-by-frame processing
     * @param inputPath Input SER file
     * @param outputPath Output video path
     * @param frameProcessor Frame processor to apply
     * @param progressCallback Progress callback
     * @return Export result
     */
    ExportResult exportWithProcessing(
        const std::filesystem::path& inputPath,
        const std::filesystem::path& outputPath,
        std::shared_ptr<FrameProcessor> processor,
        ProgressCallback progressCallback = nullptr);

    /**
     * @brief Create time-lapse video
     * @param inputPath Input SER file
     * @param outputPath Output video path
     * @param speedFactor Speed multiplier
     * @param progressCallback Progress callback
     * @return Export result
     */
    ExportResult createTimeLapse(const std::filesystem::path& inputPath,
                                 const std::filesystem::path& outputPath,
                                 double speedFactor = 10.0,
                                 ProgressCallback progressCallback = nullptr);

    /**
     * @brief Export single frame as image
     * @param inputPath Input SER file
     * @param outputPath Output image path
     * @param frameIndex Frame to export
     * @return True if successful
     */
    bool exportFrame(const std::filesystem::path& inputPath,
                     const std::filesystem::path& outputPath,
                     size_t frameIndex);

    /**
     * @brief Export multiple frames as image sequence
     * @param inputPath Input SER file
     * @param outputDir Output directory
     * @param format Image format (png, jpg, tiff)
     * @param progressCallback Progress callback
     * @return Number of frames exported
     */
    size_t exportImageSequence(const std::filesystem::path& inputPath,
                               const std::filesystem::path& outputDir,
                               const std::string& format = "png",
                               ProgressCallback progressCallback = nullptr);

    /**
     * @brief Set export parameters
     * @param params New parameters
     */
    void setParameters(const VideoExportParams& params);

    /**
     * @brief Get current parameters
     * @return Current parameters
     */
    const VideoExportParams& getParameters() const { return params_; }

    /**
     * @brief Get supported codecs
     * @return Vector of supported codec names
     */
    static std::vector<std::string> getSupportedCodecs();

    /**
     * @brief Get FourCC code for codec
     * @param codec Codec enum
     * @return FourCC code
     */
    static int getFourCC(VideoCodec codec);

    /**
     * @brief Check if codec is available
     * @param codec Codec to check
     * @return True if available
     */
    static bool isCodecAvailable(VideoCodec codec);

    /**
     * @brief Get recommended extension for codec
     * @param codec Codec
     * @return File extension (e.g., ".mp4")
     */
    static std::string getRecommendedExtension(VideoCodec codec);

    /**
     * @brief Cancel ongoing export
     */
    void cancel() { cancelRequested_ = true; }

    /**
     * @brief Check if export was cancelled
     */
    bool isCancelled() const { return cancelRequested_; }

private:
    VideoExportParams params_;
    bool cancelRequested_ = false;
    std::unique_ptr<DebayerProcessor> debayer_;

    /**
     * @brief Process frame before writing
     */
    cv::Mat processFrame(const cv::Mat& frame) const;

    /**
     * @brief Add annotations to frame
     */
    cv::Mat addAnnotations(const cv::Mat& frame, size_t frameIndex,
                           const std::optional<SERTimestamp>& timestamp) const;

    /**
     * @brief Get output size
     */
    cv::Size getOutputSize(const cv::Size& inputSize) const;

    /**
     * @brief Create video writer
     */
    cv::VideoWriter createWriter(const std::filesystem::path& outputPath,
                                 const cv::Size& frameSize) const;
};

/**
 * @brief Quick export function
 * @param serPath SER file path
 * @param videoPath Output video path
 * @return True if successful
 */
bool quickExportToVideo(const std::filesystem::path& serPath,
                        const std::filesystem::path& videoPath);

/**
 * @brief Get codec name
 */
std::string videoCodecToString(VideoCodec codec);

/**
 * @brief Parse codec from string
 */
VideoCodec videoCodecFromString(const std::string& name);

}  // namespace serastro
