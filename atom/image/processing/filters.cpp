#include "filters.hpp"
#include <algorithm>
#include <cmath>
#include <numeric>
#include <execution>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace atom::image {

blob ImageFilter::applyFilter(const blob& input, FilterType filterType, const FilterParams& params) const {
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
            auto kernel = getPredefinedKernel(FilterType::BOX_BLUR, params.kernelSize);
            outputData = convolve(inputData, kernel, width, height, channels);
            break;
        }
        case FilterType::MOTION_BLUR: {
            auto kernel = createMotionBlurKernel(params.kernelSize, params.angle, params.distance);
            outputData = convolve(inputData, kernel, width, height, channels);
            break;
        }
        case FilterType::SHARPEN: {
            auto kernel = getPredefinedKernel(FilterType::SHARPEN, params.kernelSize);
            outputData = convolve(inputData, kernel, width, height, channels);
            break;
        }
        case FilterType::SOBEL: {
            // Apply Sobel edge detection (simplified implementation)
            auto kernelX = getPredefinedKernel(FilterType::SOBEL, 3);
            auto kernelY = kernelX; // Transpose for Y direction
            std::reverse(kernelY.begin(), kernelY.end());

            auto edgesX = convolve(inputData, kernelX, width, height, channels);
            auto edgesY = convolve(inputData, kernelY, width, height, channels);

            // Combine X and Y gradients
            outputData.resize(edgesX.size());
            for (size_t i = 0; i < edgesX.size(); ++i) {
                double gx = static_cast<double>(static_cast<uint8_t>(edgesX[i]));
                double gy = static_cast<double>(static_cast<uint8_t>(edgesY[i]));
                double magnitude = std::sqrt(gx * gx + gy * gy);
                outputData[i] = static_cast<std::byte>(std::min(255.0, magnitude));
            }
            break;
        }
        case FilterType::MEDIAN: {
            outputData = medianFilter(inputData, params.kernelSize, width, height, channels);
            break;
        }
        case FilterType::BILATERAL: {
            outputData = bilateralFilter(inputData, params, width, height, channels);
            break;
        }
        case FilterType::EMBOSS: {
            auto kernel = getPredefinedKernel(FilterType::EMBOSS, 3);
            outputData = convolve(inputData, kernel, width, height, channels);
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

blob ImageFilter::applyCustomKernel(const blob& input,
                                   const std::vector<std::vector<double>>& kernel,
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
    auto outputData = convolve(inputData, normalizedKernel, width, height, channels);

    // Create output blob
    blob result;
    // Copy the output data to the blob
    for (const auto& byte : outputData) {
        result.append(&byte, 1);
    }
    return result;
}

blob ImageFilter::applySeparableFilter(const blob& input,
                                      const std::vector<double>& kernelX,
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

std::vector<std::vector<double>> ImageFilter::getPredefinedKernel(FilterType filterType, int size) {
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
            kernel[size/2][size/2] = 1.0;
            break;
        }
    }

    return kernel;
}

std::vector<std::vector<double>> ImageFilter::createGaussianKernel(int size, double sigma) {
    if (size % 2 == 0) size++; // Ensure odd size

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

std::vector<std::vector<double>> ImageFilter::createMotionBlurKernel(int size, double angle, int distance) {
    std::vector<std::vector<double>> kernel(size, std::vector<double>(size, 0.0));

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

std::vector<std::byte> ImageFilter::convolve(const std::vector<std::byte>& input,
                                           const std::vector<std::vector<double>>& kernel,
                                           int width, int height, int channels) const {
    std::vector<std::byte> output(input.size());
    int kernelSize = kernel.size();
    int kernelCenter = kernelSize / 2;

    // Sequential convolution (replace parallel for now to avoid counting_iterator issues)
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
                        double pixelValue = static_cast<double>(static_cast<uint8_t>(input[inputIdx]));
                        sum += pixelValue * kernel[ky][kx];
                    }
                }

                int outputIdx = (y * width + x) * channels + c;
                output[outputIdx] = static_cast<std::byte>(std::clamp(sum, 0.0, 255.0));
            }
        }
    }

    return output;
}

std::vector<std::byte> ImageFilter::medianFilter(const std::vector<std::byte>& input,
                                                int kernelSize,
                                                int width, int height, int channels) const {
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
                        neighborhood.push_back(static_cast<uint8_t>(input[inputIdx]));
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

std::unique_ptr<ImageFilter> createOptimalFilter(bool useGPU) {
    // For now, return basic CPU implementation
    // TODO: Add GPU implementation when CUDA/OpenCL support is added
    return std::make_unique<ImageFilter>();
}

} // namespace atom::image
