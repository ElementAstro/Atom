#ifndef ATOM_ALGORITHM_GRAPHICS_CONVOLUTION_HPP
#define ATOM_ALGORITHM_GRAPHICS_CONVOLUTION_HPP

#include <algorithm>
#include <cmath>
#include <concepts>
#include <span>
#include <vector>

#include "atom/algorithm/core/rust_numeric.hpp"

namespace atom::algorithm {

/**
 * @brief Lightweight convolution operations for pixel-level image processing.
 *
 * Provides convolution with custom kernels and Gaussian blur using flat
 * std::span<T> image representations (row-major, single-channel).
 *
 * @note For advanced 2D matrix convolution with padding modes, stride,
 *       OpenCL acceleration, and DFT-based fast convolution, use the
 *       signal processing module instead:
 *       - signal/convolution_2d.hpp  (full 2D convolution pipeline)
 *       - signal/gaussian_filter.hpp (Gaussian kernel + filter)
 *       - signal/convolution_filters.hpp (Sobel, Laplacian via signal pipeline)
 */
class Convolution {
public:
    /**
     * @brief Apply a convolution kernel to an image
     * @param image Input image data (row-major order)
     * @param width Image width
     * @param height Image height
     * @param kernel Convolution kernel
     * @param kernel_size Size of the square kernel (must be odd)
     * @return Convolved image
     */
    template <typename T>
    [[nodiscard]] static auto convolve(std::span<const T> image, i32 width,
                                       i32 height, std::span<const f32> kernel,
                                       i32 kernel_size) -> std::vector<T> {
        if (kernel_size % 2 == 0) {
            throw std::invalid_argument("Kernel size must be odd");
        }

        std::vector<T> result(image.size());
        i32 half_kernel = kernel_size / 2;

        for (i32 y = 0; y < height; ++y) {
            for (i32 x = 0; x < width; ++x) {
                f32 sum = 0.0f;

                for (i32 ky = -half_kernel; ky <= half_kernel; ++ky) {
                    for (i32 kx = -half_kernel; kx <= half_kernel; ++kx) {
                        i32 px = std::clamp(x + kx, 0, width - 1);
                        i32 py = std::clamp(y + ky, 0, height - 1);

                        i32 kernel_idx = (ky + half_kernel) * kernel_size +
                                         (kx + half_kernel);
                        sum += static_cast<f32>(image[py * width + px]) *
                               kernel[kernel_idx];
                    }
                }

                result[y * width + x] = static_cast<T>(std::clamp(
                    sum, static_cast<f32>(std::numeric_limits<T>::min()),
                    static_cast<f32>(std::numeric_limits<T>::max())));
            }
        }

        return result;
    }

    /**
     * @brief Apply Gaussian blur to an image
     * @param image Input image data
     * @param width Image width
     * @param height Image height
     * @param sigma Standard deviation for Gaussian kernel
     * @return Blurred image
     */
    template <typename T>
    [[nodiscard]] static auto gaussianBlur(std::span<const T> image, i32 width,
                                           i32 height,
                                           f32 sigma) -> std::vector<T> {
        // Generate Gaussian kernel
        i32 kernel_size =
            static_cast<i32>(std::ceil(6 * sigma)) | 1;  // Ensure odd size
        std::vector<f32> kernel(kernel_size * kernel_size);

        f32 sum = 0.0f;
        i32 half_size = kernel_size / 2;

        for (i32 y = -half_size; y <= half_size; ++y) {
            for (i32 x = -half_size; x <= half_size; ++x) {
                f32 value = std::exp(-(x * x + y * y) / (2 * sigma * sigma));
                kernel[(y + half_size) * kernel_size + (x + half_size)] = value;
                sum += value;
            }
        }

        // Normalize kernel
        for (auto& val : kernel) {
            val /= sum;
        }

        return convolve(image, width, height, kernel, kernel_size);
    }
};

}  // namespace atom::algorithm

#endif  // ATOM_ALGORITHM_GRAPHICS_CONVOLUTION_HPP
