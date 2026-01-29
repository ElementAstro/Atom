#include "image_processor.hpp"

#include <algorithm>
#include <cmath>
#include <execution>
#include <limits>
#include <thread>

#ifdef ATOM_IMAGE_HAS_OPENCV
#include <opencv2/imgproc.hpp>
#include <opencv2/opencv.hpp>
#endif

// Use standard exceptions to avoid atom error system namespace pollution
#include <stdexcept>
#undef THROW_RUNTIME_ERROR  // Remove the simple definition
#define THROW_RUNTIME_ERROR(msg) throw std::runtime_error(msg)

namespace atom::image {

ImageProcessor::ImageProcessor(const ProcessingOptions& options)
    : m_options(options) {}

blob ImageProcessor::convertFormat(const blob& input,
                                   ImageFormat targetFormat) const {
    if (input.size() == 0) {
        THROW_RUNTIME_ERROR("Cannot convert empty image");
    }

#ifdef ATOM_IMAGE_HAS_OPENCV
    // Convert blob to cv::Mat for processing
    cv::Mat inputMat = input.to_mat();

    switch (targetFormat) {
        case ImageFormat::JPEG:
            return convertToJPEG(input);
        case ImageFormat::PNG:
            return convertToPNG(input);
        case ImageFormat::TIFF:
            return convertToTIFF(input);
        case ImageFormat::BMP: {
            std::vector<uint8_t> buffer;
            cv::imencode(".bmp", inputMat, buffer);
            return blob(buffer.data(), buffer.size());
        }
        case ImageFormat::TGA: {
            std::vector<uint8_t> buffer;
            cv::imencode(".tga", inputMat, buffer);
            return blob(buffer.data(), buffer.size());
        }
        default:
            THROW_RUNTIME_ERROR("Unsupported target format");
    }
#else
    (void)targetFormat;  // Suppress unused parameter warning
    THROW_RUNTIME_ERROR("Format conversion requires OpenCV support");
#endif
}

blob ImageProcessor::resize(const blob& input, int newWidth, int newHeight,
                            const std::string& algorithm) const {
    validateImageDimensions(newWidth, newHeight);

#ifdef ATOM_IMAGE_HAS_OPENCV
    cv::Mat inputMat = input.to_mat();
    cv::Mat outputMat;

    int interpolation = cv::INTER_CUBIC;
    if (algorithm == "nearest") {
        interpolation = cv::INTER_NEAREST;
    } else if (algorithm == "linear") {
        interpolation = cv::INTER_LINEAR;
    } else if (algorithm == "lanczos") {
        interpolation = cv::INTER_LANCZOS4;
    }

    cv::resize(inputMat, outputMat, cv::Size(newWidth, newHeight), 0, 0,
               interpolation);
    return blob(outputMat);
#else
    (void)input;
    (void)algorithm;
    THROW_RUNTIME_ERROR("Resize operation requires OpenCV support");
#endif
}

blob ImageProcessor::rotate(const blob& input, double angle,
                            bool expandCanvas) const {
#ifdef ATOM_IMAGE_HAS_OPENCV
    cv::Mat inputMat = input.to_mat();
    cv::Mat outputMat;

    auto [centerX, centerY] =
        std::make_pair(inputMat.cols / 2.0f, inputMat.rows / 2.0f);
    cv::Point2f center(centerX, centerY);
    cv::Mat rotationMatrix = cv::getRotationMatrix2D(center, angle, 1.0);

    if (expandCanvas) {
        // Calculate new image size to fit rotated image
        cv::Rect2f bbox =
            cv::RotatedRect(center, inputMat.size(), angle).boundingRect2f();

        // Adjust transformation matrix
        rotationMatrix.at<double>(0, 2) += bbox.width / 2.0 - center.x;
        rotationMatrix.at<double>(1, 2) += bbox.height / 2.0 - center.y;

        cv::warpAffine(inputMat, outputMat, rotationMatrix, bbox.size());
    } else {
        cv::warpAffine(inputMat, outputMat, rotationMatrix, inputMat.size());
    }

    return blob(outputMat);
#else
    (void)input;
    (void)angle;
    (void)expandCanvas;
    THROW_RUNTIME_ERROR("Rotate operation requires OpenCV support");
#endif
}

blob ImageProcessor::crop(const blob& input, int x, int y, int width,
                          int height) const {
    validateCropParameters(input, x, y, width, height);

#ifdef ATOM_IMAGE_HAS_OPENCV
    cv::Mat inputMat = input.to_mat();
    cv::Rect cropRect(x, y, width, height);
    cv::Mat croppedMat = inputMat(cropRect);
    return blob(croppedMat);
#else
    (void)input;
    THROW_RUNTIME_ERROR("Crop operation requires OpenCV support");
#endif
}

blob ImageProcessor::applyFilter(
    const blob& input, FilterType filterType,
    const std::unordered_map<std::string, double>& parameters) const {
    // Delegate to ImageFilter to avoid code duplication
    FilterParams params;
    if (parameters.count("sigma")) {
        params.sigma = parameters.at("sigma");
    }
    if (parameters.count("strength")) {
        params.strength = parameters.at("strength");
    }
    if (parameters.count("kernelSize")) {
        params.kernelSize = static_cast<int>(parameters.at("kernelSize"));
    }
    if (parameters.count("threshold1")) {
        params.threshold1 = parameters.at("threshold1");
    }
    if (parameters.count("threshold2")) {
        params.threshold2 = parameters.at("threshold2");
    }
    return m_filter.applyFilter(input, filterType, params);
}

blob ImageProcessor::applyCustomKernel(const blob& input [[maybe_unused]],
                                       const std::vector<float>& kernel
                                       [[maybe_unused]],
                                       int kernelSize [[maybe_unused]]) const {
    validateKernel(kernel, kernelSize);

#ifdef ATOM_IMAGE_HAS_OPENCV
    cv::Mat inputMat = input.to_mat();
    cv::Mat outputMat;

    // Create kernel matrix
    cv::Mat kernelMat(kernelSize, kernelSize, CV_32F);
    std::memcpy(kernelMat.data, kernel.data(), kernel.size() * sizeof(float));

    cv::filter2D(inputMat, outputMat, -1, kernelMat);
    return blob(outputMat);
#else
    THROW_RUNTIME_ERROR("Custom kernel operation requires OpenCV support");
#endif
}

blob ImageProcessor::adjustBrightnessContrast(const blob& input,
                                              double brightness,
                                              double contrast) const {
    // Delegate to ImageEnhancement to avoid code duplication
    // Convert contrast from [-100, 100] to [0.0, 2.0] range
    double contrastNorm = (contrast + 100.0) / 100.0;
    return m_enhancement.adjustBrightnessContrast(input, brightness,
                                                  contrastNorm);
}

blob ImageProcessor::adjustGamma(const blob& input, double gamma) const {
    // Delegate to ImageEnhancement to avoid code duplication
    return m_enhancement.gammaCorrection(input, gamma);
}

blob ImageProcessor::enhanceHistogram(const blob& input, bool adaptive) const {
    // Delegate to ImageEnhancement to avoid code duplication
    HistogramMethod method =
        adaptive ? HistogramMethod::CLAHE : HistogramMethod::GLOBAL;
    return m_enhancement.equalizeHistogram(input, method);
}

blob ImageProcessor::detectEdges(const blob& input,
                                 const std::string& algorithm,
                                 const std::vector<double>& threshold) const {
    // Delegate to ImageFilter to avoid code duplication
    FilterParams params;
    if (!threshold.empty()) {
        params.threshold1 = threshold[0];
    }
    if (threshold.size() > 1) {
        params.threshold2 = threshold[1];
    }

    // Map algorithm name to FilterType
    FilterType filterType = FilterType::CANNY;
    if (algorithm == "sobel") {
        filterType = FilterType::SOBEL;
    } else if (algorithm == "laplacian") {
        filterType = FilterType::LAPLACIAN;
    } else if (algorithm == "prewitt") {
        filterType = FilterType::PREWITT;
    }

    return m_filter.applyFilter(input, filterType, params);
}

blob ImageProcessor::denoise(const blob& input, const std::string& algorithm,
                             double strength) const {
    // Delegate to ImageEnhancement to avoid code duplication
    return m_enhancement.denoise(input, strength, algorithm);
}

std::vector<blob> ImageProcessor::processBatch(
    const std::vector<blob>& inputs,
    std::function<blob(const blob&)> operation) const {
    std::vector<blob> results(inputs.size());

    if (m_options.useMultithreading && inputs.size() > 1) {
        // Parallel processing
        std::transform(std::execution::par_unseq, inputs.begin(), inputs.end(),
                       results.begin(), operation);
    } else {
        // Sequential processing
        std::transform(inputs.begin(), inputs.end(), results.begin(),
                       operation);
    }

    return results;
}

std::unordered_map<std::string, double> ImageProcessor::getStatistics(
    const blob& input [[maybe_unused]]) const {
    std::unordered_map<std::string, double> stats;

#ifdef ATOM_IMAGE_HAS_OPENCV
    cv::Mat inputMat = input.to_mat();

    cv::Scalar mean, stddev;
    cv::meanStdDev(inputMat, mean, stddev);

    double minVal, maxVal;
    cv::minMaxLoc(inputMat, &minVal, &maxVal);

    stats["mean"] = mean[0];
    stats["stddev"] = stddev[0];
    stats["min"] = minVal;
    stats["max"] = maxVal;
    stats["width"] = static_cast<double>(inputMat.cols);
    stats["height"] = static_cast<double>(inputMat.rows);
    stats["channels"] = static_cast<double>(inputMat.channels());

#else
    THROW_RUNTIME_ERROR("Statistics calculation requires OpenCV support");
#endif

    return stats;
}

std::unordered_map<std::string, double> ImageProcessor::calculateQualityMetrics(
    const blob& input [[maybe_unused]],
    const blob* reference [[maybe_unused]]) const {
    std::unordered_map<std::string, double> metrics;
#ifdef ATOM_IMAGE_HAS_OPENCV
    cv::Mat inputMat = input.to_mat();

    cv::Scalar meanScalar;
    cv::Scalar stdScalar;
    cv::meanStdDev(inputMat, meanScalar, stdScalar);

    double meanValue = 0.0;
    double stdValue = 0.0;
    const int channelCount = std::max(1, inputMat.channels());
    for (int channel = 0; channel < channelCount; ++channel) {
        meanValue += meanScalar[channel];
        stdValue += stdScalar[channel];
    }
    meanValue /= static_cast<double>(channelCount);
    stdValue /= static_cast<double>(channelCount);

    metrics["mean"] = meanValue;
    metrics["stddev"] = stdValue;
    metrics["variance"] = stdValue * stdValue;
    metrics["energy"] = cv::norm(inputMat, cv::NORM_L2SQR) /
                        static_cast<double>(inputMat.total() * channelCount);

    if (reference != nullptr) {
        cv::Mat referenceMat = reference->to_mat();
        if (referenceMat.size() != inputMat.size() ||
            referenceMat.type() != inputMat.type()) {
            THROW_RUNTIME_ERROR(
                "Reference image must match input dimensions and type");
        }

        cv::Mat diff;
        cv::absdiff(inputMat, referenceMat, diff);
        diff.convertTo(diff, CV_32F);

        const int diffChannels = std::max(1, diff.channels());
        const double mse = cv::norm(diff, cv::NORM_L2SQR) /
                           static_cast<double>(diff.total() * diffChannels);
        metrics["mse"] = mse;

        const double psnr = (mse <= std::numeric_limits<double>::epsilon())
                                ? std::numeric_limits<double>::infinity()
                                : 10.0 * std::log10((255.0 * 255.0) / mse);
        metrics["psnr"] = psnr;

        cv::Scalar diffMean;
        cv::Scalar diffStd;
        cv::meanStdDev(diff, diffMean, diffStd);
        double mae = 0.0;
        for (int channel = 0; channel < diffChannels; ++channel) {
            mae += std::abs(diffMean[channel]);
        }
        metrics["mae"] = mae / static_cast<double>(diffChannels);
        metrics["rmse"] = std::sqrt(mse);
    }

    return metrics;
#else
    THROW_RUNTIME_ERROR("Quality metric calculation requires OpenCV support");
#endif
}

void ImageProcessor::setOptions(const ProcessingOptions& options) {
    m_options = options;
}

const ProcessingOptions& ImageProcessor::getOptions() const noexcept {
    return m_options;
}

// Format-specific converters
blob ImageProcessor::convertToJPEG(const blob& input) const {
#ifdef ATOM_IMAGE_HAS_OPENCV
    cv::Mat inputMat = input.to_mat();
    std::vector<uint8_t> buffer;
    std::vector<int> params = {cv::IMWRITE_JPEG_QUALITY, 95};

    if (!cv::imencode(".jpg", inputMat, buffer, params)) {
        THROW_RUNTIME_ERROR("Failed to encode image as JPEG");
    }

    return blob(buffer.data(), buffer.size());
#else
    THROW_RUNTIME_ERROR("JPEG conversion requires OpenCV support");
#endif
}

blob ImageProcessor::convertToPNG(const blob& input) const {
#ifdef ATOM_IMAGE_HAS_OPENCV
    cv::Mat inputMat = input.to_mat();
    std::vector<uint8_t> buffer;
    std::vector<int> params = {cv::IMWRITE_PNG_COMPRESSION, 6};

    if (!cv::imencode(".png", inputMat, buffer, params)) {
        THROW_RUNTIME_ERROR("Failed to encode image as PNG");
    }

    return blob(buffer.data(), buffer.size());
#else
    THROW_RUNTIME_ERROR("PNG conversion requires OpenCV support");
#endif
}

blob ImageProcessor::convertToTIFF(const blob& input) const {
#ifdef ATOM_IMAGE_HAS_OPENCV
    cv::Mat inputMat = input.to_mat();
    std::vector<uint8_t> buffer;
    std::vector<int> params = {cv::IMWRITE_TIFF_COMPRESSION,
                               1};  // LZW compression

    if (!cv::imencode(".tiff", inputMat, buffer, params)) {
        THROW_RUNTIME_ERROR("Failed to encode image as TIFF");
    }

    return blob(buffer.data(), buffer.size());
#else
    THROW_RUNTIME_ERROR("TIFF conversion requires OpenCV support");
#endif
}

// Validation helpers
void ImageProcessor::validateImageDimensions(int width, int height) const {
    if (width <= 0 || height <= 0) {
        THROW_RUNTIME_ERROR("Image dimensions must be positive");
    }
    if (width > 65535 || height > 65535) {
        THROW_RUNTIME_ERROR("Image dimensions too large");
    }
}

void ImageProcessor::validateKernel(const std::vector<float>& kernel,
                                    int kernelSize) const {
    if (kernelSize <= 0 || kernelSize % 2 == 0) {
        THROW_RUNTIME_ERROR("Kernel size must be positive and odd");
    }
    if (kernel.size() != static_cast<size_t>(kernelSize * kernelSize)) {
        THROW_RUNTIME_ERROR("Kernel size mismatch");
    }
}

void ImageProcessor::validateCropParameters(const blob& input, int x, int y,
                                            int width, int height) const {
    if (x < 0 || y < 0 || width <= 0 || height <= 0) {
        THROW_RUNTIME_ERROR("Invalid crop parameters");
    }
    if (x + width > input.getCols() || y + height > input.getRows()) {
        THROW_RUNTIME_ERROR("Crop area exceeds image bounds");
    }
}

std::unique_ptr<ImageProcessor> createOptimalProcessor(bool useGPU
                                                       [[maybe_unused]]) {
    ProcessingOptions options;
    options.useMultithreading = true;
    options.enableSIMD = true;
    options.maxMemoryUsage = std::thread::hardware_concurrency() * 256 * 1024 *
                             1024;  // 256MB per thread

    return std::make_unique<ImageProcessor>(options);
}

}  // namespace atom::image
