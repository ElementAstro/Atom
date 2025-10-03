#include "image_processor.hpp"

#include <algorithm>
#include <cmath>
#include <execution>
#include <numeric>
#include <thread>

#ifdef ATOM_IMAGE_HAS_OPENCV
#include <opencv2/opencv.hpp>
#include <opencv2/imgproc.hpp>
#endif

// Use standard exceptions to avoid atom error system namespace pollution
#include <stdexcept>
#undef THROW_RUNTIME_ERROR  // Remove the simple definition
#define THROW_RUNTIME_ERROR(msg) throw std::runtime_error(msg)



namespace atom::image {

ImageProcessor::ImageProcessor(const ProcessingOptions& options)
    : m_options(options) {}

blob ImageProcessor::convertFormat(const blob& input, [[maybe_unused]] ImageFormat targetFormat) const {
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
    THROW_RUNTIME_ERROR("Format conversion requires OpenCV support");
#endif
}

blob ImageProcessor::resize(const blob& input [[maybe_unused]], int newWidth [[maybe_unused]], int newHeight [[maybe_unused]],
                           const std::string& algorithm [[maybe_unused]]) const {
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

    cv::resize(inputMat, outputMat, cv::Size(newWidth, newHeight), 0, 0, interpolation);
    return blob(outputMat);
#else
    THROW_RUNTIME_ERROR("Resize operation requires OpenCV support");
#endif
}

blob ImageProcessor::rotate(const blob& input [[maybe_unused]], double angle [[maybe_unused]], bool expandCanvas [[maybe_unused]]) const {
#ifdef ATOM_IMAGE_HAS_OPENCV
    cv::Mat inputMat = input.to_mat();
    cv::Mat outputMat;

    cv::Point2f center(inputMat.cols / 2.0f, inputMat.rows / 2.0f);
    cv::Mat rotationMatrix = cv::getRotationMatrix2D(center, angle, 1.0);

    if (expandCanvas) {
        // Calculate new image size to fit rotated image
        cv::Rect2f bbox = cv::RotatedRect(center, inputMat.size(), angle).boundingRect2f();

        // Adjust transformation matrix
        rotationMatrix.at<double>(0, 2) += bbox.width / 2.0 - center.x;
        rotationMatrix.at<double>(1, 2) += bbox.height / 2.0 - center.y;

        cv::warpAffine(inputMat, outputMat, rotationMatrix, bbox.size());
    } else {
        cv::warpAffine(inputMat, outputMat, rotationMatrix, inputMat.size());
    }

    return blob(outputMat);
#else
    THROW_RUNTIME_ERROR("Rotate operation requires OpenCV support");
#endif
}

blob ImageProcessor::crop(const blob& input, int x, int y, int width, int height) const {
    validateCropParameters(input, x, y, width, height);

#ifdef ATOM_IMAGE_HAS_OPENCV
    cv::Mat inputMat = input.to_mat();
    cv::Rect cropRect(x, y, width, height);
    cv::Mat croppedMat = inputMat(cropRect);
    return blob(croppedMat);
#else
    THROW_RUNTIME_ERROR("Crop operation requires OpenCV support");
#endif
}

blob ImageProcessor::applyFilter(const blob& input [[maybe_unused]], FilterType filterType [[maybe_unused]],
                                const std::unordered_map<std::string, double>& parameters [[maybe_unused]]) const {
#ifdef ATOM_IMAGE_HAS_OPENCV
    switch (filterType) {
        case FilterType::GAUSSIAN_BLUR: {
            double sigma = parameters.count("sigma") ? parameters.at("sigma") : 1.0;
            return applyGaussianBlur(input, sigma);
        }
        case FilterType::SHARPEN: {
            double strength = parameters.count("strength") ? parameters.at("strength") : 1.0;
            return applySharpen(input, strength);
        }
        case FilterType::MEDIAN: {
            int kernelSize = parameters.count("kernelSize") ?
                           static_cast<int>(parameters.at("kernelSize")) : 5;
            return applyMedianFilter(input, kernelSize);
        }
        case FilterType::EDGE_DETECT: {
            return detectEdges(input, "canny");
        }
        default:
            THROW_RUNTIME_ERROR("Unsupported filter type");
    }
#else
    THROW_RUNTIME_ERROR("Filter operations require OpenCV support");
#endif
}

blob ImageProcessor::applyCustomKernel(const blob& input [[maybe_unused]],
                                      const std::vector<float>& kernel [[maybe_unused]],
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

blob ImageProcessor::adjustBrightnessContrast(const blob& input [[maybe_unused]],
                                             double brightness [[maybe_unused]],
                                             double contrast [[maybe_unused]]) const {
#ifdef ATOM_IMAGE_HAS_OPENCV
    cv::Mat inputMat = input.to_mat();
    cv::Mat outputMat;

    // Convert brightness and contrast to OpenCV format
    double alpha = (contrast + 100.0) / 100.0;  // Contrast multiplier
    double beta = brightness;                     // Brightness offset

    inputMat.convertTo(outputMat, -1, alpha, beta);
    return blob(outputMat);
#else
    THROW_RUNTIME_ERROR("Brightness/contrast adjustment requires OpenCV support");
#endif
}

blob ImageProcessor::adjustGamma(const blob& input [[maybe_unused]], double gamma [[maybe_unused]]) const {
    if (gamma <= 0.0) {
        THROW_RUNTIME_ERROR("Gamma value must be positive");
    }

#ifdef ATOM_IMAGE_HAS_OPENCV
    cv::Mat inputMat = input.to_mat();
    cv::Mat outputMat;

    // Create lookup table for gamma correction
    cv::Mat lookupTable(1, 256, CV_8U);
    uchar* p = lookupTable.ptr();
    for (int i = 0; i < 256; ++i) {
        p[i] = cv::saturate_cast<uchar>(std::pow(i / 255.0, 1.0 / gamma) * 255.0);
    }

    cv::LUT(inputMat, lookupTable, outputMat);
    return blob(outputMat);
#else
    THROW_RUNTIME_ERROR("Gamma adjustment requires OpenCV support");
#endif
}

blob ImageProcessor::enhanceHistogram(const blob& input [[maybe_unused]], bool adaptive [[maybe_unused]]) const {
#ifdef ATOM_IMAGE_HAS_OPENCV
    cv::Mat inputMat = input.to_mat();
    cv::Mat outputMat;

    if (inputMat.channels() == 1) {
        // Grayscale image
        if (adaptive) {
            cv::Ptr<cv::CLAHE> clahe = cv::createCLAHE();
            clahe->setClipLimit(2.0);
            clahe->apply(inputMat, outputMat);
        } else {
            cv::equalizeHist(inputMat, outputMat);
        }
    } else {
        // Color image - convert to LAB and equalize L channel
        cv::Mat labImage;
        cv::cvtColor(inputMat, labImage, cv::COLOR_BGR2Lab);

        std::vector<cv::Mat> labChannels;
        cv::split(labImage, labChannels);

        if (adaptive) {
            cv::Ptr<cv::CLAHE> clahe = cv::createCLAHE();
            clahe->setClipLimit(2.0);
            clahe->apply(labChannels[0], labChannels[0]);
        } else {
            cv::equalizeHist(labChannels[0], labChannels[0]);
        }

        cv::merge(labChannels, labImage);
        cv::cvtColor(labImage, outputMat, cv::COLOR_Lab2BGR);
    }

    return blob(outputMat);
#else
    THROW_RUNTIME_ERROR("Histogram enhancement requires OpenCV support");
#endif
}

std::vector<blob> ImageProcessor::processBatch(
    const std::vector<blob>& inputs,
    std::function<blob(const blob&)> operation) const {

    std::vector<blob> results(inputs.size());

    if (m_options.useMultithreading && inputs.size() > 1) {
        // Parallel processing
        std::transform(std::execution::par_unseq,
                      inputs.begin(), inputs.end(),
                      results.begin(), operation);
    } else {
        // Sequential processing
        std::transform(inputs.begin(), inputs.end(),
                      results.begin(), operation);
    }

    return results;
}

std::unordered_map<std::string, double> ImageProcessor::getStatistics(const blob& input [[maybe_unused]]) const {
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

void ImageProcessor::setOptions(const ProcessingOptions& options) {
    m_options = options;
}

const ProcessingOptions& ImageProcessor::getOptions() const noexcept {
    return m_options;
}

// Private helper methods
blob ImageProcessor::applyGaussianBlur(const blob& input [[maybe_unused]], double sigma [[maybe_unused]]) const {
#ifdef ATOM_IMAGE_HAS_OPENCV
    cv::Mat inputMat = input.to_mat();
    cv::Mat outputMat;

    int kernelSize = static_cast<int>(2 * std::ceil(3 * sigma) + 1);
    if (kernelSize % 2 == 0) kernelSize++;  // Ensure odd kernel size

    cv::GaussianBlur(inputMat, outputMat, cv::Size(kernelSize, kernelSize), sigma);
    return blob(outputMat);
#else
    THROW_RUNTIME_ERROR("Gaussian blur requires OpenCV support");
#endif
}

blob ImageProcessor::applySharpen(const blob& input [[maybe_unused]], double strength [[maybe_unused]]) const {
#ifdef ATOM_IMAGE_HAS_OPENCV
    cv::Mat inputMat = input.to_mat();
    cv::Mat blurred, outputMat;

    cv::GaussianBlur(inputMat, blurred, cv::Size(0, 0), 1.0);
    cv::addWeighted(inputMat, 1.0 + strength, blurred, -strength, 0, outputMat);

    return blob(outputMat);
#else
    THROW_RUNTIME_ERROR("Sharpen filter requires OpenCV support");
#endif
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
    std::vector<int> params = {cv::IMWRITE_TIFF_COMPRESSION, 1}; // LZW compression

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

void ImageProcessor::validateKernel(const std::vector<float>& kernel, int kernelSize) const {
    if (kernelSize <= 0 || kernelSize % 2 == 0) {
        THROW_RUNTIME_ERROR("Kernel size must be positive and odd");
    }
    if (kernel.size() != static_cast<size_t>(kernelSize * kernelSize)) {
        THROW_RUNTIME_ERROR("Kernel size mismatch");
    }
}

void ImageProcessor::validateCropParameters(const blob& input, int x, int y, int width, int height) const {
    if (x < 0 || y < 0 || width <= 0 || height <= 0) {
        THROW_RUNTIME_ERROR("Invalid crop parameters");
    }
    if (x + width > input.getCols() || y + height > input.getRows()) {
        THROW_RUNTIME_ERROR("Crop area exceeds image bounds");
    }
}

std::unique_ptr<ImageProcessor> createOptimalProcessor(bool useGPU [[maybe_unused]]) {
    ProcessingOptions options;
    options.useMultithreading = true;
    options.enableSIMD = true;
    options.maxMemoryUsage = std::thread::hardware_concurrency() * 256 * 1024 * 1024; // 256MB per thread

    return std::make_unique<ImageProcessor>(options);
}

} // namespace atom::image
