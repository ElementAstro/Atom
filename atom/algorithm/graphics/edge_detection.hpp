#ifndef ATOM_ALGORITHM_GRAPHICS_EDGE_DETECTION_HPP
#define ATOM_ALGORITHM_GRAPHICS_EDGE_DETECTION_HPP

#include <array>
#include <cmath>
#include <span>
#include <vector>

#include "atom/algorithm/core/rust_numeric.hpp"
#include "convolution.hpp"

namespace atom::algorithm {

/**
 * @brief Lightweight edge detection operations for pixel-level image processing.
 *
 * Provides Sobel and Laplacian edge detection using the lightweight
 * graphics/Convolution class (flat std::span<T> images).
 *
 * @note For edge detection on 2D matrix representations with advanced options
 *       (padding modes, stride, multi-threading, OpenCL), use the signal
 *       processing module instead:
 *       - signal/convolution_filters.hpp (ConvolutionFilters::sobelEdge, etc.)
 */
class EdgeDetection {
public:
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

        auto grad_x = Convolution::convolve(image, width, height, sobel_x, 3);
        auto grad_y = Convolution::convolve(image, width, height, sobel_y, 3);

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

        return Convolution::convolve(image, width, height, laplacian, 3);
    }
};

}  // namespace atom::algorithm

#endif  // ATOM_ALGORITHM_GRAPHICS_EDGE_DETECTION_HPP
