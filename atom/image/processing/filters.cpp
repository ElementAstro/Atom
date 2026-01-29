#include "filters.hpp"
#include <algorithm>
#include <cmath>
#include <complex>
#include <execution>
#include <numeric>
#include <random>
#include <stdexcept>
#include "gpu_acceleration.hpp"

#ifdef ATOM_IMAGE_HAS_OPENCV
#include <opencv2/imgproc.hpp>
#include <opencv2/opencv.hpp>
#endif

// Define error macros to avoid atom error system namespace pollution
#define THROW_RUNTIME_ERROR(msg) throw std::runtime_error(msg)
#define THROW_INVALID_ARGUMENT(msg) throw std::invalid_argument(msg)

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace atom::image {

blob ImageFilter::applyFilter(const blob& input, FilterType filterType,
                              const FilterParams& params) const {
    if (input.isEmpty()) {
        return blob{};
    }

    // Get image dimensions (assuming they're stored in the blob)
    int width = input.getCols();
    int height = input.getRows();
    int channels = input.getChannels();

    std::vector<std::byte> inputData(input.begin(), input.end());
    std::vector<std::byte> outputData;

    switch (filterType) {
        case FilterType::GAUSSIAN_BLUR: {
            auto kernel = createGaussianKernel(params.kernelSize, params.sigma);
            outputData = convolve(inputData, kernel, width, height, channels);
            break;
        }
        case FilterType::BOX_BLUR: {
            auto kernel =
                getPredefinedKernel(FilterType::BOX_BLUR, params.kernelSize);
            outputData = convolve(inputData, kernel, width, height, channels);
            break;
        }
        case FilterType::MOTION_BLUR: {
            auto kernel = createMotionBlurKernel(params.kernelSize,
                                                 params.angle, params.distance);
            outputData = convolve(inputData, kernel, width, height, channels);
            break;
        }
        case FilterType::SHARPEN: {
            auto kernel =
                getPredefinedKernel(FilterType::SHARPEN, params.kernelSize);
            outputData = convolve(inputData, kernel, width, height, channels);
            break;
        }
        case FilterType::SOBEL: {
            // Apply Sobel edge detection (simplified implementation)
            auto kernelX = getPredefinedKernel(FilterType::SOBEL, 3);
            auto kernelY = kernelX;  // Transpose for Y direction
            std::reverse(kernelY.begin(), kernelY.end());

            auto edgesX = convolve(inputData, kernelX, width, height, channels);
            auto edgesY = convolve(inputData, kernelY, width, height, channels);

            // Combine X and Y gradients
            outputData.resize(edgesX.size());
            for (size_t i = 0; i < edgesX.size(); ++i) {
                double gx =
                    static_cast<double>(static_cast<uint8_t>(edgesX[i]));
                double gy =
                    static_cast<double>(static_cast<uint8_t>(edgesY[i]));
                double magnitude = std::sqrt(gx * gx + gy * gy);
                outputData[i] =
                    static_cast<std::byte>(std::min(255.0, magnitude));
            }
            break;
        }
        case FilterType::MEDIAN: {
            outputData = medianFilter(inputData, params.kernelSize, width,
                                      height, channels);
            break;
        }
        case FilterType::BILATERAL: {
            outputData =
                bilateralFilter(inputData, params, width, height, channels);
            break;
        }
        case FilterType::EMBOSS: {
            auto kernel = getPredefinedKernel(FilterType::EMBOSS, 3);
            outputData = convolve(inputData, kernel, width, height, channels);
            break;
        }
        case FilterType::LAPLACIAN: {
            auto kernel = getPredefinedKernel(FilterType::LAPLACIAN, 3);
            outputData = convolve(inputData, kernel, width, height, channels);
            break;
        }
        case FilterType::PREWITT: {
            // Prewitt edge detection
            std::vector<std::vector<double>> kernelX = {
                {-1, 0, 1}, {-1, 0, 1}, {-1, 0, 1}};
            std::vector<std::vector<double>> kernelY = {
                {-1, -1, -1}, {0, 0, 0}, {1, 1, 1}};
            auto edgesX = convolve(inputData, kernelX, width, height, channels);
            auto edgesY = convolve(inputData, kernelY, width, height, channels);
            outputData.resize(edgesX.size());
            for (size_t i = 0; i < edgesX.size(); ++i) {
                double gx =
                    static_cast<double>(static_cast<uint8_t>(edgesX[i]));
                double gy =
                    static_cast<double>(static_cast<uint8_t>(edgesY[i]));
                double magnitude = std::sqrt(gx * gx + gy * gy);
                outputData[i] =
                    static_cast<std::byte>(std::min(255.0, magnitude));
            }
            break;
        }
        case FilterType::SCHARR: {
            // Scharr edge detection (more accurate than Sobel)
            std::vector<std::vector<double>> kernelX = {
                {-3, 0, 3}, {-10, 0, 10}, {-3, 0, 3}};
            std::vector<std::vector<double>> kernelY = {
                {-3, -10, -3}, {0, 0, 0}, {3, 10, 3}};
            auto edgesX = convolve(inputData, kernelX, width, height, channels);
            auto edgesY = convolve(inputData, kernelY, width, height, channels);
            outputData.resize(edgesX.size());
            for (size_t i = 0; i < edgesX.size(); ++i) {
                double gx =
                    static_cast<double>(static_cast<uint8_t>(edgesX[i])) / 16.0;
                double gy =
                    static_cast<double>(static_cast<uint8_t>(edgesY[i])) / 16.0;
                double magnitude = std::sqrt(gx * gx + gy * gy);
                outputData[i] =
                    static_cast<std::byte>(std::min(255.0, magnitude));
            }
            break;
        }
        case FilterType::ROBERTS: {
            // Roberts cross edge detection
            outputData.resize(inputData.size());
            for (int y = 0; y < height - 1; ++y) {
                for (int x = 0; x < width - 1; ++x) {
                    for (int c = 0; c < channels; ++c) {
                        int idx00 = (y * width + x) * channels + c;
                        int idx01 = (y * width + x + 1) * channels + c;
                        int idx10 = ((y + 1) * width + x) * channels + c;
                        int idx11 = ((y + 1) * width + x + 1) * channels + c;

                        double p00 = static_cast<uint8_t>(inputData[idx00]);
                        double p01 = static_cast<uint8_t>(inputData[idx01]);
                        double p10 = static_cast<uint8_t>(inputData[idx10]);
                        double p11 = static_cast<uint8_t>(inputData[idx11]);

                        double gx = p00 - p11;
                        double gy = p01 - p10;
                        double magnitude = std::sqrt(gx * gx + gy * gy);
                        outputData[idx00] =
                            static_cast<std::byte>(std::min(255.0, magnitude));
                    }
                }
            }
            break;
        }
        case FilterType::CANNY: {
            // Simplified Canny edge detection
            // 1. Gaussian blur
            auto blurKernel = createGaussianKernel(5, 1.4);
            auto blurred =
                convolve(inputData, blurKernel, width, height, channels);

            // 2. Sobel gradients
            auto kernelX = getPredefinedKernel(FilterType::SOBEL, 3);
            std::vector<std::vector<double>> kernelY = {
                {-1, -2, -1}, {0, 0, 0}, {1, 2, 1}};
            auto gx = convolve(blurred, kernelX, width, height, channels);
            auto gy = convolve(blurred, kernelY, width, height, channels);

            // 3. Gradient magnitude and direction
            outputData.resize(inputData.size());
            double lowThresh = params.threshold1 > 0 ? params.threshold1 : 50.0;
            double highThresh =
                params.threshold2 > 0 ? params.threshold2 : 150.0;

            for (size_t i = 0; i < gx.size(); ++i) {
                double gradX = static_cast<double>(static_cast<uint8_t>(gx[i]));
                double gradY = static_cast<double>(static_cast<uint8_t>(gy[i]));
                double magnitude = std::sqrt(gradX * gradX + gradY * gradY);

                // Simple thresholding (full Canny would include NMS and
                // hysteresis)
                if (magnitude > highThresh) {
                    outputData[i] = static_cast<std::byte>(255);
                } else if (magnitude > lowThresh) {
                    outputData[i] = static_cast<std::byte>(128);
                } else {
                    outputData[i] = static_cast<std::byte>(0);
                }
            }
            break;
        }
        case FilterType::UNSHARP_MASK: {
            // Unsharp masking
            auto blurKernel =
                createGaussianKernel(params.kernelSize, params.sigma);
            auto blurred =
                convolve(inputData, blurKernel, width, height, channels);
            double amount = params.strength > 0 ? params.strength : 1.5;

            outputData.resize(inputData.size());
            for (size_t i = 0; i < inputData.size(); ++i) {
                double original =
                    static_cast<double>(static_cast<uint8_t>(inputData[i]));
                double blur =
                    static_cast<double>(static_cast<uint8_t>(blurred[i]));
                double sharpened = original + amount * (original - blur);
                outputData[i] =
                    static_cast<std::byte>(std::clamp(sharpened, 0.0, 255.0));
            }
            break;
        }
        case FilterType::HIGH_PASS: {
            // High-pass filter (original - low pass)
            auto blurKernel =
                createGaussianKernel(params.kernelSize, params.sigma);
            auto blurred =
                convolve(inputData, blurKernel, width, height, channels);

            outputData.resize(inputData.size());
            for (size_t i = 0; i < inputData.size(); ++i) {
                double original =
                    static_cast<double>(static_cast<uint8_t>(inputData[i]));
                double blur =
                    static_cast<double>(static_cast<uint8_t>(blurred[i]));
                double highPass = 128 + (original - blur);
                outputData[i] =
                    static_cast<std::byte>(std::clamp(highPass, 0.0, 255.0));
            }
            break;
        }
        case FilterType::LOW_PASS: {
            // Low-pass is essentially Gaussian blur
            auto kernel = createGaussianKernel(params.kernelSize, params.sigma);
            outputData = convolve(inputData, kernel, width, height, channels);
            break;
        }
        case FilterType::MEAN: {
            // Mean filter (box blur)
            auto kernel =
                getPredefinedKernel(FilterType::BOX_BLUR, params.kernelSize);
            outputData = convolve(inputData, kernel, width, height, channels);
            break;
        }
        case FilterType::MIN_FILTER: {
            // Minimum (erosion-like) filter
            outputData = minMaxFilter(inputData, params.kernelSize, width,
                                      height, channels, true);
            break;
        }
        case FilterType::MAX_FILTER: {
            // Maximum (dilation-like) filter
            outputData = minMaxFilter(inputData, params.kernelSize, width,
                                      height, channels, false);
            break;
        }
        case FilterType::OIL_PAINT: {
            outputData = oilPaintFilter(inputData, params.kernelSize, width,
                                        height, channels);
            break;
        }
        case FilterType::PENCIL_SKETCH: {
            outputData = pencilSketchFilter(inputData, width, height, channels);
            break;
        }
        case FilterType::CARTOON: {
            outputData = cartoonFilter(inputData, width, height, channels);
            break;
        }
        case FilterType::POSTERIZE: {
            int levels = params.levels > 0 ? params.levels : 4;
            outputData = posterizeFilter(inputData, levels);
            break;
        }
        case FilterType::PIXELATE: {
            int blockSize = params.kernelSize > 0 ? params.kernelSize : 8;
            outputData =
                pixelateFilter(inputData, blockSize, width, height, channels);
            break;
        }
        case FilterType::VIGNETTE: {
            outputData = vignetteFilter(inputData, width, height, channels,
                                        params.strength);
            break;
        }
        case FilterType::SEPIA: {
            outputData = sepiaFilter(inputData, channels);
            break;
        }
        default: {
            // Default to identity (copy input)
            outputData = inputData;
            break;
        }
    }

    // Create output blob
    blob result;
    // Copy the output data to the blob
    for (const auto& byte : outputData) {
        result.append(&byte, 1);
    }
    return result;
}

blob ImageFilter::applyFilterV2(const blob& input, FilterType filterType,
                                const FilterParamMap& params) const {
    // Convert FilterParamMap to FilterParams
    FilterParams filterParams;

    // Extract parameters from variant map
    auto getInt = [&params](const std::string& key, int defaultVal) -> int {
        auto it = params.find(key);
        if (it != params.end()) {
            if (auto* val = std::get_if<int>(&it->second)) {
                return *val;
            }
        }
        return defaultVal;
    };

    auto getDouble = [&params](const std::string& key,
                               double defaultVal) -> double {
        auto it = params.find(key);
        if (it != params.end()) {
            if (auto* val = std::get_if<double>(&it->second)) {
                return *val;
            }
        }
        return defaultVal;
    };

    filterParams.kernelSize = getInt("kernelSize", 3);
    filterParams.sigma = getDouble("sigma", 1.0);
    filterParams.sigmaColor = getDouble("sigmaColor", 75.0);
    filterParams.sigmaSpace = getDouble("sigmaSpace", 75.0);
    filterParams.threshold1 = getDouble("threshold1", 50.0);
    filterParams.threshold2 = getDouble("threshold2", 150.0);
    filterParams.angle = getDouble("angle", 0.0);
    filterParams.distance = getDouble("distance", 10.0);
    filterParams.strength = getDouble("strength", 1.0);

    return applyFilter(input, filterType, filterParams);
}

blob ImageFilter::applyCustomKernel(
    const blob& input, const std::vector<std::vector<double>>& kernel,
    bool normalize) const {
    if (input.isEmpty() || kernel.empty()) {
        return blob{};
    }

    auto normalizedKernel = kernel;
    if (normalize) {
        double sum = 0.0;
        for (const auto& row : kernel) {
            sum += std::accumulate(row.begin(), row.end(), 0.0);
        }
        if (sum != 0.0) {
            for (auto& row : normalizedKernel) {
                for (auto& val : row) {
                    val /= sum;
                }
            }
        }
    }

    int width = input.getCols();
    int height = input.getRows();
    int channels = input.getChannels();

    std::vector<std::byte> inputData(input.begin(), input.end());
    auto outputData =
        convolve(inputData, normalizedKernel, width, height, channels);

    // Create output blob
    blob result;
    // Copy the output data to the blob
    for (const auto& byte : outputData) {
        result.append(&byte, 1);
    }
    return result;
}

blob ImageFilter::applySeparableFilter(
    const blob& input, const std::vector<double>& kernelX,
    const std::vector<double>& kernelY) const {
    if (input.isEmpty() || kernelX.empty() || kernelY.empty()) {
        return blob{};
    }

    // First apply horizontal kernel
    std::vector<std::vector<double>> hKernel(1, kernelX);
    auto intermediate = applyCustomKernel(input, hKernel, false);

    // Then apply vertical kernel
    std::vector<std::vector<double>> vKernel;
    for (double val : kernelY) {
        vKernel.push_back({val});
    }

    return applyCustomKernel(intermediate, vKernel, false);
}

std::vector<std::vector<double>> ImageFilter::getPredefinedKernel(
    FilterType filterType, int size) {
    std::vector<std::vector<double>> kernel;

    switch (filterType) {
        case FilterType::BOX_BLUR: {
            double value = 1.0 / (size * size);
            kernel.resize(size, std::vector<double>(size, value));
            break;
        }
        case FilterType::SHARPEN: {
            if (size == 3) {
                kernel = {{0, -1, 0}, {-1, 5, -1}, {0, -1, 0}};
            } else {
                // Default 3x3 sharpen kernel
                kernel = {{0, -1, 0}, {-1, 5, -1}, {0, -1, 0}};
            }
            break;
        }
        case FilterType::SOBEL: {
            kernel = {{-1, 0, 1}, {-2, 0, 2}, {-1, 0, 1}};
            break;
        }
        case FilterType::LAPLACIAN: {
            kernel = {{0, -1, 0}, {-1, 4, -1}, {0, -1, 0}};
            break;
        }
        case FilterType::EMBOSS: {
            kernel = {{-2, -1, 0}, {-1, 1, 1}, {0, 1, 2}};
            break;
        }
        case FilterType::EDGE_ENHANCE: {
            kernel = {{0, 0, 0}, {-1, 1, 0}, {0, 0, 0}};
            break;
        }
        default: {
            // Identity kernel
            kernel.resize(size, std::vector<double>(size, 0.0));
            kernel[size / 2][size / 2] = 1.0;
            break;
        }
    }

    return kernel;
}

std::vector<std::vector<double>> ImageFilter::createGaussianKernel(
    int size, double sigma) {
    if (size % 2 == 0)
        size++;  // Ensure odd size

    std::vector<std::vector<double>> kernel(size, std::vector<double>(size));
    double sum = 0.0;
    int center = size / 2;
    double twoSigmaSquared = 2.0 * sigma * sigma;

    for (int i = 0; i < size; ++i) {
        for (int j = 0; j < size; ++j) {
            int x = i - center;
            int y = j - center;
            double value = std::exp(-(x * x + y * y) / twoSigmaSquared);
            kernel[i][j] = value;
            sum += value;
        }
    }

    // Normalize
    for (auto& row : kernel) {
        for (auto& val : row) {
            val /= sum;
        }
    }

    return kernel;
}

std::vector<std::vector<double>> ImageFilter::createMotionBlurKernel(
    int size, double angle, int distance) {
    std::vector<std::vector<double>> kernel(size,
                                            std::vector<double>(size, 0.0));

    double radians = angle * M_PI / 180.0;
    double dx = std::cos(radians);
    double dy = std::sin(radians);

    int center = size / 2;
    int count = 0;

    for (int i = 0; i <= distance; ++i) {
        int x = center + static_cast<int>(i * dx);
        int y = center + static_cast<int>(i * dy);

        if (x >= 0 && x < size && y >= 0 && y < size) {
            kernel[y][x] = 1.0;
            count++;
        }
    }

    // Normalize
    if (count > 0) {
        for (auto& row : kernel) {
            for (auto& val : row) {
                val /= count;
            }
        }
    }

    return kernel;
}

std::vector<std::byte> ImageFilter::convolve(
    const std::vector<std::byte>& input,
    const std::vector<std::vector<double>>& kernel, int width, int height,
    int channels) const {
    std::vector<std::byte> output(input.size());
    int kernelSize = kernel.size();
    int kernelCenter = kernelSize / 2;

    // Sequential convolution (replace parallel for now to avoid
    // counting_iterator issues)
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            for (int c = 0; c < channels; ++c) {
                double sum = 0.0;

                for (int ky = 0; ky < kernelSize; ++ky) {
                    for (int kx = 0; kx < kernelSize; ++kx) {
                        int px = x + kx - kernelCenter;
                        int py = y + ky - kernelCenter;

                        // Handle boundaries by clamping
                        px = std::clamp(px, 0, width - 1);
                        py = std::clamp(py, 0, height - 1);

                        int inputIdx = (py * width + px) * channels + c;
                        double pixelValue = static_cast<double>(
                            static_cast<uint8_t>(input[inputIdx]));
                        sum += pixelValue * kernel[ky][kx];
                    }
                }

                int outputIdx = (y * width + x) * channels + c;
                output[outputIdx] =
                    static_cast<std::byte>(std::clamp(sum, 0.0, 255.0));
            }
        }
    }

    return output;
}

std::vector<std::byte> ImageFilter::medianFilter(
    const std::vector<std::byte>& input, int kernelSize, int width, int height,
    int channels) const {
    std::vector<std::byte> output(input.size());
    int kernelCenter = kernelSize / 2;

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            for (int c = 0; c < channels; ++c) {
                std::vector<uint8_t> neighborhood;

                for (int ky = -kernelCenter; ky <= kernelCenter; ++ky) {
                    for (int kx = -kernelCenter; kx <= kernelCenter; ++kx) {
                        int px = std::clamp(x + kx, 0, width - 1);
                        int py = std::clamp(y + ky, 0, height - 1);

                        int inputIdx = (py * width + px) * channels + c;
                        neighborhood.push_back(
                            static_cast<uint8_t>(input[inputIdx]));
                    }
                }

                std::sort(neighborhood.begin(), neighborhood.end());
                uint8_t median = neighborhood[neighborhood.size() / 2];

                int outputIdx = (y * width + x) * channels + c;
                output[outputIdx] = static_cast<std::byte>(median);
            }
        }
    }

    return output;
}

std::vector<std::byte> ImageFilter::bilateralFilter(
    const std::vector<std::byte>& input, const FilterParams& params, int width,
    int height, int channels) const {
    std::vector<std::byte> output(input.size());
    int kernelSize = params.kernelSize;
    int kernelCenter = kernelSize / 2;
    double sigmaSpace = params.sigma;
    double sigmaColor = params.sigmaColor > 0 ? params.sigmaColor : sigmaSpace;

    // Precompute spatial weights
    std::vector<std::vector<double>> spatialWeights(
        kernelSize, std::vector<double>(kernelSize));
    for (int dy = -kernelCenter; dy <= kernelCenter; ++dy) {
        for (int dx = -kernelCenter; dx <= kernelCenter; ++dx) {
            double distance = std::sqrt(dx * dx + dy * dy);
            spatialWeights[dy + kernelCenter][dx + kernelCenter] = std::exp(
                -(distance * distance) / (2 * sigmaSpace * sigmaSpace));
        }
    }

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            for (int c = 0; c < channels; ++c) {
                int centerIdx = (y * width + x) * channels + c;
                uint8_t centerValue = static_cast<uint8_t>(input[centerIdx]);

                double weightSum = 0.0;
                double valueSum = 0.0;

                for (int dy = -kernelCenter; dy <= kernelCenter; ++dy) {
                    for (int dx = -kernelCenter; dx <= kernelCenter; ++dx) {
                        int ny = y + dy;
                        int nx = x + dx;

                        if (ny >= 0 && ny < height && nx >= 0 && nx < width) {
                            int neighborIdx = (ny * width + nx) * channels + c;
                            uint8_t neighborValue =
                                static_cast<uint8_t>(input[neighborIdx]);

                            double colorDiff =
                                std::abs(static_cast<int>(centerValue) -
                                         static_cast<int>(neighborValue));
                            double colorWeight =
                                std::exp(-(colorDiff * colorDiff) /
                                         (2 * sigmaColor * sigmaColor));
                            double spatialWeight =
                                spatialWeights[dy + kernelCenter]
                                              [dx + kernelCenter];

                            double totalWeight = spatialWeight * colorWeight;
                            weightSum += totalWeight;
                            valueSum += totalWeight * neighborValue;
                        }
                    }
                }

                output[centerIdx] = static_cast<std::byte>(
                    weightSum > 0 ? static_cast<uint8_t>(valueSum / weightSum)
                                  : centerValue);
            }
        }
    }

    return output;
}

// Min/Max filter implementation
std::vector<std::byte> ImageFilter::minMaxFilter(
    const std::vector<std::byte>& input, int kernelSize, int width, int height,
    int channels, bool isMin) const {
    std::vector<std::byte> output(input.size());
    int kernelCenter = kernelSize / 2;

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            for (int c = 0; c < channels; ++c) {
                uint8_t extremeVal = isMin ? 255 : 0;

                for (int ky = -kernelCenter; ky <= kernelCenter; ++ky) {
                    for (int kx = -kernelCenter; kx <= kernelCenter; ++kx) {
                        int px = std::clamp(x + kx, 0, width - 1);
                        int py = std::clamp(y + ky, 0, height - 1);
                        int idx = (py * width + px) * channels + c;
                        uint8_t val = static_cast<uint8_t>(input[idx]);

                        if (isMin) {
                            extremeVal = std::min(extremeVal, val);
                        } else {
                            extremeVal = std::max(extremeVal, val);
                        }
                    }
                }

                int outputIdx = (y * width + x) * channels + c;
                output[outputIdx] = static_cast<std::byte>(extremeVal);
            }
        }
    }
    return output;
}

// Oil paint effect filter
std::vector<std::byte> ImageFilter::oilPaintFilter(
    const std::vector<std::byte>& input, int radius, int width, int height,
    int channels) const {
    std::vector<std::byte> output(input.size());
    int intensityLevels = 20;

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            std::vector<int> intensityCount(intensityLevels, 0);
            std::vector<std::vector<int>> sumRGB(intensityLevels,
                                                 std::vector<int>(channels, 0));

            for (int ky = -radius; ky <= radius; ++ky) {
                for (int kx = -radius; kx <= radius; ++kx) {
                    int px = std::clamp(x + kx, 0, width - 1);
                    int py = std::clamp(y + ky, 0, height - 1);
                    int idx = (py * width + px) * channels;

                    // Calculate intensity
                    int intensity = 0;
                    for (int c = 0; c < channels; ++c) {
                        intensity += static_cast<uint8_t>(input[idx + c]);
                    }
                    intensity = (intensity / channels) * intensityLevels / 256;
                    intensity = std::clamp(intensity, 0, intensityLevels - 1);

                    intensityCount[intensity]++;
                    for (int c = 0; c < channels; ++c) {
                        sumRGB[intensity][c] +=
                            static_cast<uint8_t>(input[idx + c]);
                    }
                }
            }

            // Find most common intensity
            int maxCount = 0;
            int maxIntensity = 0;
            for (int i = 0; i < intensityLevels; ++i) {
                if (intensityCount[i] > maxCount) {
                    maxCount = intensityCount[i];
                    maxIntensity = i;
                }
            }

            int outIdx = (y * width + x) * channels;
            for (int c = 0; c < channels; ++c) {
                if (maxCount > 0) {
                    output[outIdx + c] = static_cast<std::byte>(
                        sumRGB[maxIntensity][c] / maxCount);
                } else {
                    output[outIdx + c] = input[outIdx + c];
                }
            }
        }
    }
    return output;
}

// Pencil sketch effect filter
std::vector<std::byte> ImageFilter::pencilSketchFilter(
    const std::vector<std::byte>& input, int width, int height,
    int channels) const {
    // Convert to grayscale first
    std::vector<std::byte> gray(width * height);
    for (int i = 0; i < width * height; ++i) {
        int idx = i * channels;
        double grayVal = 0;
        if (channels >= 3) {
            grayVal = 0.299 * static_cast<uint8_t>(input[idx]) +
                      0.587 * static_cast<uint8_t>(input[idx + 1]) +
                      0.114 * static_cast<uint8_t>(input[idx + 2]);
        } else {
            grayVal = static_cast<uint8_t>(input[idx]);
        }
        gray[i] = static_cast<std::byte>(static_cast<uint8_t>(grayVal));
    }

    // Invert
    std::vector<std::byte> inverted(gray.size());
    for (size_t i = 0; i < gray.size(); ++i) {
        inverted[i] =
            static_cast<std::byte>(255 - static_cast<uint8_t>(gray[i]));
    }

    // Blur the inverted image
    auto blurKernel = createGaussianKernel(21, 5.0);
    auto blurred = convolve(inverted, blurKernel, width, height, 1);

    // Color dodge blend
    std::vector<std::byte> output(input.size());
    for (int i = 0; i < width * height; ++i) {
        double a = static_cast<uint8_t>(gray[i]);
        double b = static_cast<uint8_t>(blurred[i]);
        double result =
            (b == 255) ? 255 : std::min(255.0, (a * 256.0) / (256.0 - b));

        int outIdx = i * channels;
        for (int c = 0; c < channels; ++c) {
            output[outIdx + c] =
                static_cast<std::byte>(static_cast<uint8_t>(result));
        }
    }
    return output;
}

// Cartoon effect filter
std::vector<std::byte> ImageFilter::cartoonFilter(
    const std::vector<std::byte>& input, int width, int height,
    int channels) const {
    // 1. Bilateral filter for smoothing while preserving edges
    FilterParams bilateralParams;
    bilateralParams.kernelSize = 9;
    bilateralParams.sigma = 75;
    bilateralParams.sigmaColor = 75;
    auto smoothed =
        bilateralFilter(input, bilateralParams, width, height, channels);

    // 2. Edge detection
    std::vector<std::byte> gray(width * height);
    for (int i = 0; i < width * height; ++i) {
        int idx = i * channels;
        double grayVal = 0;
        if (channels >= 3) {
            grayVal = 0.299 * static_cast<uint8_t>(smoothed[idx]) +
                      0.587 * static_cast<uint8_t>(smoothed[idx + 1]) +
                      0.114 * static_cast<uint8_t>(smoothed[idx + 2]);
        } else {
            grayVal = static_cast<uint8_t>(smoothed[idx]);
        }
        gray[i] = static_cast<std::byte>(static_cast<uint8_t>(grayVal));
    }

    auto edgeKernel = getPredefinedKernel(FilterType::LAPLACIAN, 3);
    auto edges = convolve(gray, edgeKernel, width, height, 1);

    // 3. Threshold edges and combine with smoothed image
    std::vector<std::byte> output(input.size());
    for (int i = 0; i < width * height; ++i) {
        uint8_t edgeVal = static_cast<uint8_t>(edges[i]);
        int idx = i * channels;

        if (edgeVal > 30) {
            // Edge pixel - make it dark
            for (int c = 0; c < channels; ++c) {
                output[idx + c] = static_cast<std::byte>(0);
            }
        } else {
            // Quantize colors for cartoon effect
            for (int c = 0; c < channels; ++c) {
                uint8_t val = static_cast<uint8_t>(smoothed[idx + c]);
                val = (val / 32) * 32 + 16;  // Quantize to 8 levels
                output[idx + c] = static_cast<std::byte>(val);
            }
        }
    }
    return output;
}

// Posterize filter
std::vector<std::byte> ImageFilter::posterizeFilter(
    const std::vector<std::byte>& input, int levels) const {
    std::vector<std::byte> output(input.size());
    double step = 256.0 / levels;

    for (size_t i = 0; i < input.size(); ++i) {
        double val = static_cast<uint8_t>(input[i]);
        int quantized =
            static_cast<int>(std::floor(val / step)) * static_cast<int>(step);
        quantized = std::clamp(quantized, 0, 255);
        output[i] = static_cast<std::byte>(quantized);
    }
    return output;
}

// Pixelate filter
std::vector<std::byte> ImageFilter::pixelateFilter(
    const std::vector<std::byte>& input, int blockSize, int width, int height,
    int channels) const {
    std::vector<std::byte> output(input.size());

    for (int by = 0; by < height; by += blockSize) {
        for (int bx = 0; bx < width; bx += blockSize) {
            // Calculate average color in block
            std::vector<double> avgColor(channels, 0.0);
            int count = 0;

            for (int y = by; y < std::min(by + blockSize, height); ++y) {
                for (int x = bx; x < std::min(bx + blockSize, width); ++x) {
                    int idx = (y * width + x) * channels;
                    for (int c = 0; c < channels; ++c) {
                        avgColor[c] += static_cast<uint8_t>(input[idx + c]);
                    }
                    count++;
                }
            }

            for (int c = 0; c < channels; ++c) {
                avgColor[c] /= count;
            }

            // Fill block with average color
            for (int y = by; y < std::min(by + blockSize, height); ++y) {
                for (int x = bx; x < std::min(bx + blockSize, width); ++x) {
                    int idx = (y * width + x) * channels;
                    for (int c = 0; c < channels; ++c) {
                        output[idx + c] = static_cast<std::byte>(
                            static_cast<uint8_t>(avgColor[c]));
                    }
                }
            }
        }
    }
    return output;
}

// Vignette filter
std::vector<std::byte> ImageFilter::vignetteFilter(
    const std::vector<std::byte>& input, int width, int height, int channels,
    double strength) const {
    std::vector<std::byte> output(input.size());
    double cx = width / 2.0;
    double cy = height / 2.0;
    double maxDist = std::sqrt(cx * cx + cy * cy);
    double vignetteStrength = strength > 0 ? strength : 0.5;

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            double dx = x - cx;
            double dy = y - cy;
            double dist = std::sqrt(dx * dx + dy * dy) / maxDist;
            double factor = 1.0 - vignetteStrength * dist * dist;
            factor = std::max(0.0, factor);

            int idx = (y * width + x) * channels;
            for (int c = 0; c < channels; ++c) {
                double val = static_cast<uint8_t>(input[idx + c]) * factor;
                output[idx + c] = static_cast<std::byte>(
                    static_cast<uint8_t>(std::clamp(val, 0.0, 255.0)));
            }
        }
    }
    return output;
}

// Sepia filter
std::vector<std::byte> ImageFilter::sepiaFilter(
    const std::vector<std::byte>& input, int channels) const {
    std::vector<std::byte> output(input.size());

    size_t pixelCount = input.size() / channels;
    for (size_t i = 0; i < pixelCount; ++i) {
        int idx = i * channels;
        double r = (channels >= 1) ? static_cast<uint8_t>(input[idx]) : 0;
        double g = (channels >= 2) ? static_cast<uint8_t>(input[idx + 1]) : r;
        double b = (channels >= 3) ? static_cast<uint8_t>(input[idx + 2]) : r;

        double newR = 0.393 * r + 0.769 * g + 0.189 * b;
        double newG = 0.349 * r + 0.686 * g + 0.168 * b;
        double newB = 0.272 * r + 0.534 * g + 0.131 * b;

        if (channels >= 1)
            output[idx] = static_cast<std::byte>(
                static_cast<uint8_t>(std::clamp(newR, 0.0, 255.0)));
        if (channels >= 2)
            output[idx + 1] = static_cast<std::byte>(
                static_cast<uint8_t>(std::clamp(newG, 0.0, 255.0)));
        if (channels >= 3)
            output[idx + 2] = static_cast<std::byte>(
                static_cast<uint8_t>(std::clamp(newB, 0.0, 255.0)));
        if (channels >= 4)
            output[idx + 3] = input[idx + 3];  // Preserve alpha
    }
    return output;
}

// Morphological operations
blob ImageFilter::applyMorphological(const blob& input, FilterType operation,
                                     StructuringElement structElement,
                                     int size) const {
    if (input.isEmpty()) {
        return blob{};
    }

#ifdef ATOM_IMAGE_HAS_OPENCV
    cv::Mat src = input.to_mat();
    cv::Mat dst;

    // Map StructuringElement enum to OpenCV shape
    cv::MorphShapes shape = cv::MORPH_RECT;
    switch (structElement) {
        case StructuringElement::ELLIPSE:
            shape = cv::MORPH_ELLIPSE;
            break;
        case StructuringElement::CROSS:
            shape = cv::MORPH_CROSS;
            break;
        default:
            shape = cv::MORPH_RECT;
            break;
    }

    cv::Mat cvStructElement =
        cv::getStructuringElement(shape, cv::Size(size, size));

    // Map FilterType to OpenCV morphology operation
    switch (operation) {
        case FilterType::EROSION:
            cv::erode(src, dst, cvStructElement);
            break;
        case FilterType::DILATION:
            cv::dilate(src, dst, cvStructElement);
            break;
        case FilterType::OPENING:
            cv::morphologyEx(src, dst, cv::MORPH_OPEN, cvStructElement);
            break;
        case FilterType::CLOSING:
            cv::morphologyEx(src, dst, cv::MORPH_CLOSE, cvStructElement);
            break;
        case FilterType::GRADIENT:
            cv::morphologyEx(src, dst, cv::MORPH_GRADIENT, cvStructElement);
            break;
        case FilterType::TOP_HAT:
            cv::morphologyEx(src, dst, cv::MORPH_TOPHAT, cvStructElement);
            break;
        case FilterType::BLACK_HAT:
            cv::morphologyEx(src, dst, cv::MORPH_BLACKHAT, cvStructElement);
            break;
        default:
            dst = src;
            break;
    }

    return blob(dst);
#else
    (void)operation;
    (void)structElement;
    (void)size;
    // Without OpenCV, return input unchanged
    return input;
#endif
}

// Frequency domain filter
blob ImageFilter::applyFrequencyFilter(const blob& input, FilterType filterType,
                                       const FilterParams& params) const {
    if (input.isEmpty()) {
        return blob{};
    }

#ifdef ATOM_IMAGE_HAS_OPENCV
    cv::Mat src = input.to_mat();
    cv::Mat gray;

    if (src.channels() > 1) {
        cv::cvtColor(src, gray, cv::COLOR_BGR2GRAY);
    } else {
        gray = src;
    }

    // Pad to optimal DFT size
    cv::Mat padded;
    int m = cv::getOptimalDFTSize(gray.rows);
    int n = cv::getOptimalDFTSize(gray.cols);
    cv::copyMakeBorder(gray, padded, 0, m - gray.rows, 0, n - gray.cols,
                       cv::BORDER_CONSTANT, cv::Scalar::all(0));

    // Convert to float and add imaginary channel
    cv::Mat planes[] = {cv::Mat_<float>(padded),
                        cv::Mat::zeros(padded.size(), CV_32F)};
    cv::Mat complexImg;
    cv::merge(planes, 2, complexImg);

    // DFT
    cv::dft(complexImg, complexImg);

    // Create filter mask
    cv::Mat mask = cv::Mat::ones(complexImg.size(), CV_32F);
    int cx = mask.cols / 2;
    int cy = mask.rows / 2;

    // Use cutoffFreq from FilterParams struct
    double cutoff = params.cutoffFreq * std::min(cx, cy);

    for (int y = 0; y < mask.rows; ++y) {
        for (int x = 0; x < mask.cols; ++x) {
            double dist = std::sqrt((x - cx) * (x - cx) + (y - cy) * (y - cy));

            switch (filterType) {
                case FilterType::LOW_PASS:
                    mask.at<float>(y, x) = (dist <= cutoff) ? 1.0f : 0.0f;
                    break;
                case FilterType::HIGH_PASS_FREQ:
                    mask.at<float>(y, x) = (dist > cutoff) ? 1.0f : 0.0f;
                    break;
                case FilterType::BAND_PASS: {
                    double low = cutoff * (1.0 - params.bandwidth);
                    double high = cutoff * (1.0 + params.bandwidth);
                    mask.at<float>(y, x) =
                        (dist > low && dist < high) ? 1.0f : 0.0f;
                    break;
                }
                case FilterType::BAND_STOP: {
                    double low = cutoff * (1.0 - params.bandwidth);
                    double high = cutoff * (1.0 + params.bandwidth);
                    mask.at<float>(y, x) =
                        (dist <= low || dist >= high) ? 1.0f : 0.0f;
                    break;
                }
                default:
                    // For other filter types, use Gaussian low-pass
                    mask.at<float>(y, x) = static_cast<float>(
                        std::exp(-(dist * dist) / (2 * cutoff * cutoff)));
                    break;
            }
        }
    }

    // Shift quadrants
    cv::Mat maskQuads[] = {
        mask(cv::Rect(0, 0, cx, cy)), mask(cv::Rect(cx, 0, cx, cy)),
        mask(cv::Rect(0, cy, cx, cy)), mask(cv::Rect(cx, cy, cx, cy))};
    cv::Mat tmp;
    maskQuads[0].copyTo(tmp);
    maskQuads[3].copyTo(maskQuads[0]);
    tmp.copyTo(maskQuads[3]);
    maskQuads[1].copyTo(tmp);
    maskQuads[2].copyTo(maskQuads[1]);
    tmp.copyTo(maskQuads[2]);

    // Apply filter
    cv::split(complexImg, planes);
    planes[0] = planes[0].mul(mask);
    planes[1] = planes[1].mul(mask);
    cv::merge(planes, 2, complexImg);

    // Inverse DFT
    cv::idft(complexImg, complexImg);
    cv::split(complexImg, planes);
    cv::normalize(planes[0], planes[0], 0, 255, cv::NORM_MINMAX);
    planes[0].convertTo(gray, CV_8U);

    // Crop to original size
    cv::Mat result = gray(cv::Rect(0, 0, src.cols, src.rows));

    // Convert back to original channels if needed
    if (src.channels() > 1) {
        cv::Mat colorResult;
        cv::cvtColor(result, colorResult, cv::COLOR_GRAY2BGR);
        return blob(colorResult);
    }

    return blob(result);
#else
    (void)filterType;
    (void)params;
    // Without OpenCV, return input unchanged
    return input;
#endif
}

// Adaptive filter
blob ImageFilter::applyAdaptiveFilter(const blob& input, FilterType filterType,
                                      int windowSize,
                                      const FilterParams& params) const {
    (void)filterType;  // Used to determine filter behavior
    (void)params;      // Additional parameters
    if (input.isEmpty()) {
        return blob{};
    }

#ifdef ATOM_IMAGE_HAS_OPENCV
    // Use OpenCV's adaptive filtering capabilities
    cv::Mat src = input.to_mat();
    cv::Mat dst;

    // Apply bilateral filter as adaptive noise reduction
    cv::bilateralFilter(src, dst, windowSize, params.sigmaColor,
                        params.sigmaSpace);

    return blob(dst);
#else
    int width = input.getCols();
    int height = input.getRows();
    int channels = input.getChannels();

    std::vector<std::byte> inputData(input.begin(), input.end());
    std::vector<std::byte> outputData(inputData.size());
    int halfWin = windowSize / 2;

    // Adaptive mean filter (Wiener-like) - default implementation
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            for (int c = 0; c < channels; ++c) {
                double sum = 0, sumSq = 0;
                int count = 0;

                for (int wy = -halfWin; wy <= halfWin; ++wy) {
                    for (int wx = -halfWin; wx <= halfWin; ++wx) {
                        int px = std::clamp(x + wx, 0, width - 1);
                        int py = std::clamp(y + wy, 0, height - 1);
                        int idx = (py * width + px) * channels + c;
                        double val = static_cast<uint8_t>(inputData[idx]);
                        sum += val;
                        sumSq += val * val;
                        count++;
                    }
                }

                double mean = sum / count;
                double variance = (sumSq / count) - (mean * mean);
                double noiseVar = 500.0;  // Estimated noise variance

                int centerIdx = (y * width + x) * channels + c;
                double centerVal = static_cast<uint8_t>(inputData[centerIdx]);

                double result;
                if (variance > noiseVar) {
                    result = mean + (variance - noiseVar) / variance *
                                        (centerVal - mean);
                } else {
                    result = mean;
                }

                outputData[centerIdx] = static_cast<std::byte>(
                    static_cast<uint8_t>(std::clamp(result, 0.0, 255.0)));
            }
        }
    }

    blob resultBlob;
    for (const auto& byte : outputData) {
        resultBlob.append(&byte, 1);
    }
    return resultBlob;
#endif
}

// Multi-scale filter
blob ImageFilter::applyMultiScaleFilter(const blob& input,
                                        FilterType filterType, int scales,
                                        const FilterParams& params) const {
    int numScales = scales;
    double scaleFactor = 2.0;  // Default scale factor
    if (input.isEmpty() || numScales < 1) {
        return input;
    }

    int width = input.getCols();
    int height = input.getRows();
    int channels = input.getChannels();

    std::vector<std::byte> inputData(input.begin(), input.end());
    std::vector<double> accumulated(inputData.size(), 0.0);

    // Apply filter at multiple scales
    for (int s = 0; s < numScales; ++s) {
        FilterParams scaleParams = params;
        scaleParams.sigma = params.sigma * std::pow(scaleFactor, s);
        scaleParams.kernelSize =
            static_cast<int>(params.kernelSize * std::pow(scaleFactor, s));
        if (scaleParams.kernelSize % 2 == 0)
            scaleParams.kernelSize++;

        blob filtered = applyFilter(input, filterType, scaleParams);

        // Accumulate
        auto filteredData =
            std::vector<std::byte>(filtered.begin(), filtered.end());
        for (size_t i = 0; i < accumulated.size(); ++i) {
            accumulated[i] +=
                static_cast<double>(static_cast<uint8_t>(filteredData[i])) /
                numScales;
        }
    }

    // Convert back to bytes
    std::vector<std::byte> outputData(inputData.size());
    for (size_t i = 0; i < outputData.size(); ++i) {
        outputData[i] = static_cast<std::byte>(
            static_cast<uint8_t>(std::clamp(accumulated[i], 0.0, 255.0)));
    }

    blob result;
    for (const auto& byte : outputData) {
        result.append(&byte, 1);
    }
    return result;
}

// Filter chain
blob ImageFilter::applyFilterChain(
    const blob& input, const std::vector<FilterType>& filters,
    const std::vector<FilterParams>& params) const {
    if (input.isEmpty() || filters.empty()) {
        return input;
    }

    blob current = input;
    for (size_t i = 0; i < filters.size(); ++i) {
        FilterParams filterParams =
            (i < params.size()) ? params[i] : FilterParams{};
        current = applyFilter(current, filters[i], filterParams);
        if (current.isEmpty()) {
            return blob{};
        }
    }

    return current;
}

std::unique_ptr<ImageFilter> createOptimalFilter(bool useGPU) {
    // Check if GPU acceleration is requested and available
    if (useGPU) {
#if defined(ATOM_IMAGE_HAS_CUDA) || defined(ATOM_IMAGE_HAS_OPENCL)
        try {
            // Attempt to create GPU-accelerated processor
            auto gpuProcessor = std::make_unique<GPUImageProcessor>();

            // Try to initialize with auto-detection of best backend
            if (gpuProcessor->initialize(GPUBackend::AUTO, -1)) {
                // GPU initialization successful
                // Note: GPUImageProcessor doesn't directly inherit from
                // ImageFilter, so we return the CPU implementation but log that
                // GPU is available In a full implementation, we would create a
                // GPUImageFilter wrapper
                return std::make_unique<ImageFilter>();
            }
        } catch (const std::exception&) {
            // GPU initialization failed, fall through to CPU implementation
        }
#endif
    }

    // Return CPU implementation (default or fallback)
    return std::make_unique<ImageFilter>();
}

}  // namespace atom::image
