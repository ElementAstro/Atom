#ifndef ATOM_ALGORITHM_GRAPHICS_IMAGE_OPS_HPP
#define ATOM_ALGORITHM_GRAPHICS_IMAGE_OPS_HPP

#include <algorithm>
#include <array>
#include <cmath>
#include <concepts>
#include <span>
#include <vector>

#include "../rust_numeric.hpp"

#ifdef ATOM_USE_SIMD
#include <immintrin.h>
#endif

namespace atom::algorithm {

/**
 * @brief Basic image processing operations
 *
 * This class provides fundamental image processing algorithms including:
 * - Convolution with custom kernels
 * - Gaussian blur
 * - Edge detection (Sobel, Laplacian)
 * - Brightness and contrast adjustment
 * - Histogram equalization
 */
class ImageOps {
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

    /**
     * @brief Apply Sobel edge detection
     * @param image Input image data
     * @param width Image width
     * @param height Image height
     * @return Edge-detected image
     */
    template <typename T>
    [[nodiscard]] static auto sobelEdgeDetection(std::span<const T> image,
                                                 i32 width,
                                                 i32 height) -> std::vector<T> {
        // Sobel X kernel
        constexpr std::array<f32, 9> sobel_x = {-1, 0, 1, -2, 0, 2, -1, 0, 1};

        // Sobel Y kernel
        constexpr std::array<f32, 9> sobel_y = {-1, -2, -1, 0, 0, 0, 1, 2, 1};

        auto grad_x = convolve(image, width, height, sobel_x, 3);
        auto grad_y = convolve(image, width, height, sobel_y, 3);

        std::vector<T> result(image.size());

        for (usize i = 0; i < image.size(); ++i) {
            f32 magnitude = std::sqrt(static_cast<f32>(grad_x[i] * grad_x[i] +
                                                       grad_y[i] * grad_y[i]));
            result[i] = static_cast<T>(std::clamp(
                magnitude, static_cast<f32>(std::numeric_limits<T>::min()),
                static_cast<f32>(std::numeric_limits<T>::max())));
        }

        return result;
    }

    /**
     * @brief Apply Laplacian edge detection
     * @param image Input image data
     * @param width Image width
     * @param height Image height
     * @return Edge-detected image
     */
    template <typename T>
    [[nodiscard]] static auto laplacianEdgeDetection(
        std::span<const T> image, i32 width, i32 height) -> std::vector<T> {
        constexpr std::array<f32, 9> laplacian = {0,  -1, 0,  -1, 4,
                                                  -1, 0,  -1, 0};

        return convolve(image, width, height, laplacian, 3);
    }

    /**
     * @brief Adjust brightness and contrast
     * @param image Input image data
     * @param brightness Brightness adjustment (-255 to 255)
     * @param contrast Contrast multiplier (0.0 to 3.0, 1.0 = no change)
     * @return Adjusted image
     */
    template <typename T>
    [[nodiscard]] static auto adjustBrightnessContrast(
        std::span<const T> image, f32 brightness,
        f32 contrast) -> std::vector<T> {
        std::vector<T> result(image.size());

#ifdef ATOM_USE_SIMD
        if constexpr (std::same_as<T, u8>) {
            // SIMD implementation for u8
            __m256 brightness_vec = _mm256_set1_ps(brightness);
            __m256 contrast_vec = _mm256_set1_ps(contrast);

            usize simd_end = (image.size() / 8) * 8;

            for (usize i = 0; i < simd_end; i += 8) {
                // Load 8 bytes and convert to float
                __m128i bytes = _mm_loadl_epi64(
                    reinterpret_cast<const __m128i*>(&image[i]));
                __m256i bytes_256 = _mm256_cvtepu8_epi32(bytes);
                __m256 floats = _mm256_cvtepi32_ps(bytes_256);

                // Apply brightness and contrast
                floats = _mm256_fmadd_ps(floats, contrast_vec, brightness_vec);

                // Clamp to [0, 255] and convert back to bytes
                floats = _mm256_max_ps(floats, _mm256_setzero_ps());
                floats = _mm256_min_ps(floats, _mm256_set1_ps(255.0f));
                __m256i ints = _mm256_cvtps_epi32(floats);

                // Pack back to bytes (this is simplified - full implementation
                // would need proper packing)
                for (i32 j = 0; j < 8; ++j) {
                    result[i + j] =
                        static_cast<u8>(_mm256_extract_epi32(ints, j));
                }
            }

            // Handle remaining elements
            for (usize i = simd_end; i < image.size(); ++i) {
                f32 value = static_cast<f32>(image[i]) * contrast + brightness;
                result[i] = static_cast<T>(std::clamp(value, 0.0f, 255.0f));
            }
        } else
#endif
        {
            // Scalar implementation
            for (usize i = 0; i < image.size(); ++i) {
                f32 value = static_cast<f32>(image[i]) * contrast + brightness;
                result[i] = static_cast<T>(std::clamp(
                    value, static_cast<f32>(std::numeric_limits<T>::min()),
                    static_cast<f32>(std::numeric_limits<T>::max())));
            }
        }

        return result;
    }

    /**
     * @brief Adjust brightness only
     * @param image Input image data
     * @param brightness Brightness adjustment (-255 to 255)
     * @return Adjusted image
     */
    template <typename T>
    [[nodiscard]] static auto adjustBrightness(
        std::span<const T> image, f32 brightness) -> std::vector<T> {
        return adjustBrightnessContrast(image, brightness, 1.0f);
    }

    /**
     * @brief Adjust contrast only
     * @param image Input image data
     * @param contrast Contrast multiplier (0.0 to 3.0, 1.0 = no change)
     * @return Adjusted image
     */
    template <typename T>
    [[nodiscard]] static auto adjustContrast(std::span<const T> image,
                                             f32 contrast) -> std::vector<T> {
        return adjustBrightnessContrast(image, 0.0f, contrast);
    }

    /**
     * @brief Apply threshold to image
     * @param image Input image data
     * @param threshold_value Threshold value
     * @return Binary image (0 or max value)
     */
    template <typename T>
    [[nodiscard]] static auto threshold(std::span<const T> image,
                                        T threshold_value) -> std::vector<T> {
        std::vector<T> result(image.size());
        for (usize i = 0; i < image.size(); ++i) {
            result[i] = image[i] >= threshold_value
                            ? std::numeric_limits<T>::max()
                            : T{0};
        }
        return result;
    }

    /**
     * @brief Invert image colors
     * @param image Input image data
     * @return Inverted image
     */
    template <typename T>
    [[nodiscard]] static auto invert(std::span<const T> image)
        -> std::vector<T> {
        std::vector<T> result(image.size());
        for (usize i = 0; i < image.size(); ++i) {
            result[i] = std::numeric_limits<T>::max() - image[i];
        }
        return result;
    }

    /**
     * @brief Compute histogram of image intensities
     * @param image Input image data
     * @param bins Number of histogram bins
     * @return Histogram as vector of counts
     */
    template <typename T>
    [[nodiscard]] static auto computeHistogram(
        std::span<const T> image, i32 bins = 256) -> std::vector<u32> {
        std::vector<u32> histogram(bins, 0);

        T min_val = *std::min_element(image.begin(), image.end());
        T max_val = *std::max_element(image.begin(), image.end());
        f32 scale =
            static_cast<f32>(bins - 1) / static_cast<f32>(max_val - min_val);

        for (T pixel : image) {
            i32 bin = static_cast<i32>((pixel - min_val) * scale);
            bin = std::clamp(bin, 0, bins - 1);
            histogram[bin]++;
        }

        return histogram;
    }

    /**
     * @brief Apply histogram equalization
     * @param image Input image data
     * @return Equalized image
     */
    template <typename T>
    [[nodiscard]] static auto histogramEqualization(std::span<const T> image)
        -> std::vector<T> {
        constexpr i32 LEVELS = 256;
        auto histogram = computeHistogram(image, LEVELS);

        // Compute cumulative distribution function
        std::vector<u32> cdf(LEVELS);
        cdf[0] = histogram[0];
        for (i32 i = 1; i < LEVELS; ++i) {
            cdf[i] = cdf[i - 1] + histogram[i];
        }

        // Create lookup table
        std::vector<T> lut(LEVELS);
        u32 total_pixels = static_cast<u32>(image.size());

        for (i32 i = 0; i < LEVELS; ++i) {
            lut[i] = static_cast<T>((cdf[i] * (LEVELS - 1)) / total_pixels);
        }

        // Apply lookup table
        std::vector<T> result(image.size());
        for (usize i = 0; i < image.size(); ++i) {
            result[i] = lut[image[i]];
        }

        return result;
    }
};

}  // namespace atom::algorithm

#endif  // ATOM_ALGORITHM_GRAPHICS_IMAGE_OPS_HPP
