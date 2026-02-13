#ifndef ATOM_ALGORITHM_GRAPHICS_HISTOGRAM_HPP
#define ATOM_ALGORITHM_GRAPHICS_HISTOGRAM_HPP

#include <algorithm>
#include <span>
#include <vector>

#include "../core/rust_numeric.hpp"

namespace atom::algorithm {

/**
 * @brief Histogram computation and equalization operations
 *
 * Provides histogram analysis and equalization for image enhancement.
 */
class Histogram {
public:
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

#endif  // ATOM_ALGORITHM_GRAPHICS_HISTOGRAM_HPP
