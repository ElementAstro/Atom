#ifndef ATOM_ALGORITHM_GRAPHICS_IMAGE_OPS_HPP
#define ATOM_ALGORITHM_GRAPHICS_IMAGE_OPS_HPP

#include "convolution.hpp"
#include "edge_detection.hpp"
#include "histogram.hpp"
#include "image_adjust.hpp"

namespace atom::algorithm {

/**
 * @brief Basic image processing operations
 *
 * This class provides fundamental image processing algorithms including:
 * - Convolution with custom kernels (from Convolution)
 * - Gaussian blur (from Convolution)
 * - Edge detection: Sobel, Laplacian (from EdgeDetection)
 * - Brightness and contrast adjustment (from ImageAdjust)
 * - Threshold and inversion (from ImageAdjust)
 * - Histogram equalization (from Histogram)
 *
 * This is a backward-compatible facade that aggregates all sub-components.
 * You may also use the individual classes directly:
 * Convolution, EdgeDetection, ImageAdjust, Histogram.
 */
class ImageOps : public Convolution,
                 public EdgeDetection,
                 public ImageAdjust,
                 public Histogram {};

}  // namespace atom::algorithm

#endif  // ATOM_ALGORITHM_GRAPHICS_IMAGE_OPS_HPP
