#ifndef ATOM_ALGORITHM_GRAPHICS_IMAGE_ADJUST_HPP
#define ATOM_ALGORITHM_GRAPHICS_IMAGE_ADJUST_HPP

#include <algorithm>
#include <concepts>
#include <span>
#include <vector>

#include "atom/algorithm/core/rust_numeric.hpp"
#include "atom/algorithm/core/simd_utils.hpp"  // ATOM_SIMD_* macros + SIMD headers

namespace atom::algorithm {

/**
 * @brief Brightness, contrast, threshold, and inversion operations
 *
 * Provides pixel-level image adjustment algorithms.
 */
class ImageAdjust {
public:
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
};

}  // namespace atom::algorithm

#endif  // ATOM_ALGORITHM_GRAPHICS_IMAGE_ADJUST_HPP
