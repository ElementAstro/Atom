/*
 * convolution_filters.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-11-10

Description: Convolution-based image filters including Sobel edge detection,
Laplacian edge detection, and custom filter application.

Note: These filters operate on std::vector<std::vector<T>> matrices using
the full signal processing convolution pipeline (padding, stride, OpenCL).

For lightweight Sobel/Laplacian on flat std::span<T> pixel arrays, see
graphics/edge_detection.hpp instead.

**************************************************/

#ifndef ATOM_ALGORITHM_SIGNAL_CONVOLUTION_FILTERS_HPP
#define ATOM_ALGORITHM_SIGNAL_CONVOLUTION_FILTERS_HPP

#include <cmath>

#include "convolve_common.hpp"
#include "convolution_2d.hpp"

namespace atom::algorithm {

/**
 * @brief Class providing static methods for applying various convolution
 * filters
 *
 * @tparam T Type of the data
 */
template <ConvolutionNumeric T = f64>
class ConvolutionFilters {
public:
    /**
     * @brief Apply a Sobel edge detection filter
     *
     * @param image Input image as 2D matrix
     * @param options Configuration options for the operation
     * @return std::vector<std::vector<T>> Edge detection result
     */
    static auto applySobel(const std::vector<std::vector<T>>& image,
                           const ConvolutionOptions<T>& options = {})
        -> std::vector<std::vector<T>>;

    /**
     * @brief Apply a Laplacian edge detection filter
     *
     * @param image Input image as 2D matrix
     * @param options Configuration options for the operation
     * @return std::vector<std::vector<T>> Edge detection result
     */
    static auto applyLaplacian(const std::vector<std::vector<T>>& image,
                               const ConvolutionOptions<T>& options = {})
        -> std::vector<std::vector<T>>;

    /**
     * @brief Apply a custom filter with the specified kernel
     *
     * @param image Input image as 2D matrix
     * @param kernel Custom convolution kernel
     * @param options Configuration options for the operation
     * @return std::vector<std::vector<T>> Filtered image
     */
    static auto applyCustomFilter(const std::vector<std::vector<T>>& image,
                                  const std::vector<std::vector<T>>& kernel,
                                  const ConvolutionOptions<T>& options = {})
        -> std::vector<std::vector<T>>;
};

// Template implementations

template <ConvolutionNumeric T>
auto ConvolutionFilters<T>::applySobel(const std::vector<std::vector<T>>& image,
                                       const ConvolutionOptions<T>& options)
    -> std::vector<std::vector<T>> {
    (void)options;  // Suppress unused parameter warning

    if (image.empty() || image[0].empty()) {
        return {};
    }

    // Sobel kernels
    std::vector<std::vector<T>> sobelX = {
        {T{-1}, T{0}, T{1}}, {T{-2}, T{0}, T{2}}, {T{-1}, T{0}, T{1}}};

    std::vector<std::vector<T>> sobelY = {
        {T{-1}, T{-2}, T{-1}}, {T{0}, T{0}, T{0}}, {T{1}, T{2}, T{1}}};

    // Use the available convolve2D function
    if constexpr (std::is_same_v<T, f64>) {
        auto gradX = atom::algorithm::convolve2D(
            reinterpret_cast<const std::vector<std::vector<f64>>&>(image),
            reinterpret_cast<const std::vector<std::vector<f64>>&>(sobelX));
        auto gradY = atom::algorithm::convolve2D(
            reinterpret_cast<const std::vector<std::vector<f64>>&>(image),
            reinterpret_cast<const std::vector<std::vector<f64>>&>(sobelY));

        // Compute magnitude
        std::vector<std::vector<T>> result(gradX.size());
        for (usize i = 0; i < gradX.size(); ++i) {
            result[i].resize(gradX[i].size());
            for (usize j = 0; j < gradX[i].size(); ++j) {
                T gx = static_cast<T>(gradX[i][j]);
                T gy = static_cast<T>(gradY[i][j]);
                result[i][j] = static_cast<T>(std::sqrt(gx * gx + gy * gy));
            }
        }
        return result;
    } else {
        // Convert to f64, process, and convert back
        std::vector<std::vector<f64>> image_f64;
        image_f64.reserve(image.size());
        for (const auto& row : image) {
            image_f64.emplace_back(row.begin(), row.end());
        }

        std::vector<std::vector<f64>> sobelX_f64 = {
            {-1.0, 0.0, 1.0}, {-2.0, 0.0, 2.0}, {-1.0, 0.0, 1.0}};

        std::vector<std::vector<f64>> sobelY_f64 = {
            {-1.0, -2.0, -1.0}, {0.0, 0.0, 0.0}, {1.0, 2.0, 1.0}};

        auto gradX = atom::algorithm::convolve2D(image_f64, sobelX_f64);
        auto gradY = atom::algorithm::convolve2D(image_f64, sobelY_f64);

        std::vector<std::vector<T>> result(gradX.size());
        for (usize i = 0; i < gradX.size(); ++i) {
            result[i].resize(gradX[i].size());
            for (usize j = 0; j < gradX[i].size(); ++j) {
                f64 gx = gradX[i][j];
                f64 gy = gradY[i][j];
                result[i][j] = static_cast<T>(std::sqrt(gx * gx + gy * gy));
            }
        }
        return result;
    }
}

template <ConvolutionNumeric T>
auto ConvolutionFilters<T>::applyLaplacian(
    const std::vector<std::vector<T>>& image,
    const ConvolutionOptions<T>& options) -> std::vector<std::vector<T>> {
    (void)options;  // Suppress unused parameter warning

    if (image.empty() || image[0].empty()) {
        return {};
    }

    // Laplacian kernel
    std::vector<std::vector<T>> laplacian = {
        {T{0}, T{-1}, T{0}}, {T{-1}, T{4}, T{-1}}, {T{0}, T{-1}, T{0}}};

    // Use the available convolve2D function
    if constexpr (std::is_same_v<T, f64>) {
        return atom::algorithm::convolve2D(
            reinterpret_cast<const std::vector<std::vector<f64>>&>(image),
            reinterpret_cast<const std::vector<std::vector<f64>>&>(laplacian));
    } else {
        // Convert to f64, process, and convert back
        std::vector<std::vector<f64>> image_f64;
        image_f64.reserve(image.size());
        for (const auto& row : image) {
            image_f64.emplace_back(row.begin(), row.end());
        }

        std::vector<std::vector<f64>> laplacian_f64 = {
            {0.0, -1.0, 0.0}, {-1.0, 4.0, -1.0}, {0.0, -1.0, 0.0}};

        auto result_f64 = atom::algorithm::convolve2D(image_f64, laplacian_f64);

        std::vector<std::vector<T>> result;
        result.reserve(result_f64.size());
        for (const auto& row : result_f64) {
            result.emplace_back(row.begin(), row.end());
        }
        return result;
    }
}

template <ConvolutionNumeric T>
auto ConvolutionFilters<T>::applyCustomFilter(
    const std::vector<std::vector<T>>& image,
    const std::vector<std::vector<T>>& kernel,
    const ConvolutionOptions<T>& options) -> std::vector<std::vector<T>> {
    return atom::algorithm::convolve2D(image, kernel, options);
}

}  // namespace atom::algorithm

#endif  // ATOM_ALGORITHM_SIGNAL_CONVOLUTION_FILTERS_HPP
