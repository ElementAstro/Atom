#ifndef ATOM_IMAGE_TRANSFORMS_HPP
#define ATOM_IMAGE_TRANSFORMS_HPP

/**
 * @file transforms.hpp
 * @brief Advanced image transformation operations
 *
 * This module provides comprehensive image transformation capabilities
 * including geometric transformations, perspective corrections, image
 * registration, and advanced warping operations.
 *
 * @author Atom Framework Team
 * @date 2025
 * @version 1.0.0
 */

#include <array>
#include <functional>
#include <memory>
#include <vector>
#include "../core/image_blob.hpp"

namespace atom::image {

/**
 * @brief Interpolation methods for image transformations
 */
enum class InterpolationMethod {
    NEAREST,        // Nearest neighbor
    LINEAR,         // Bilinear interpolation
    CUBIC,          // Bicubic interpolation
    LANCZOS,        // Lanczos resampling
    AREA,           // Area-based resampling
    SUPER_SAMPLING  // Super-sampling anti-aliasing
};

/**
 * @brief Border handling modes for transformations
 */
enum class BorderMode {
    CONSTANT,    // Fill with constant value
    REPLICATE,   // Replicate edge pixels
    REFLECT,     // Reflect across edge
    WRAP,        // Wrap around
    TRANSPARENT  // Transparent (for alpha channel)
};

/**
 * @brief 2D transformation matrix (3x3 homogeneous coordinates)
 */
using TransformMatrix = std::array<std::array<double, 3>, 3>;

/**
 * @brief 2D point structure
 */
struct Point2D {
    double x, y;
    Point2D(double x = 0, double y = 0) : x(x), y(y) {}
};

/**
 * @brief Rectangle structure
 */
struct Rectangle {
    double x, y, width, height;
    Rectangle(double x = 0, double y = 0, double w = 0, double h = 0)
        : x(x), y(y), width(w), height(h) {}
};

/**
 * @brief Advanced image transformation processor
 */
class ImageTransform {
public:
    ImageTransform() = default;
    virtual ~ImageTransform() = default;

    /**
     * @brief Resize image with advanced interpolation
     * @param input Input image blob
     * @param newWidth Target width
     * @param newHeight Target height
     * @param method Interpolation method
     * @param preserveAspect Whether to preserve aspect ratio
     * @return Resized image blob
     */
    virtual blob resize(
        const blob& input, int newWidth, int newHeight,
        InterpolationMethod method = InterpolationMethod::LINEAR,
        bool preserveAspect = false) const;

    /**
     * @brief Rotate image by arbitrary angle
     * @param input Input image blob
     * @param angle Rotation angle in degrees (positive = clockwise)
     * @param center Rotation center (if empty, use image center)
     * @param expandCanvas Whether to expand canvas to fit rotated image
     * @param method Interpolation method
     * @param borderMode Border handling mode
     * @param fillValue Fill value for constant border mode
     * @return Rotated image blob
     */
    virtual blob rotate(
        const blob& input, double angle, const Point2D& center = {},
        bool expandCanvas = true,
        InterpolationMethod method = InterpolationMethod::LINEAR,
        BorderMode borderMode = BorderMode::CONSTANT,
        uint8_t fillValue = 0) const;

    /**
     * @brief Apply affine transformation
     * @param input Input image blob
     * @param matrix 2x3 affine transformation matrix
     * @param outputSize Output image size (if empty, use input size)
     * @param method Interpolation method
     * @param borderMode Border handling mode
     * @return Transformed image blob
     */
    virtual blob affineTransform(
        const blob& input, const std::array<std::array<double, 3>, 2>& matrix,
        const Point2D& outputSize = {},
        InterpolationMethod method = InterpolationMethod::LINEAR,
        BorderMode borderMode = BorderMode::CONSTANT) const;

    /**
     * @brief Apply perspective transformation
     * @param input Input image blob
     * @param matrix 3x3 perspective transformation matrix
     * @param outputSize Output image size
     * @param method Interpolation method
     * @return Transformed image blob
     */
    virtual blob perspectiveTransform(
        const blob& input, const TransformMatrix& matrix,
        const Point2D& outputSize,
        InterpolationMethod method = InterpolationMethod::LINEAR) const;

    /**
     * @brief Correct perspective distortion using four corner points
     * @param input Input image blob
     * @param srcPoints Four source corner points (clockwise from top-left)
     * @param dstPoints Four destination corner points
     * @param outputSize Output image size
     * @return Perspective-corrected image blob
     */
    virtual blob correctPerspective(const blob& input,
                                    const std::array<Point2D, 4>& srcPoints,
                                    const std::array<Point2D, 4>& dstPoints,
                                    const Point2D& outputSize) const;

    /**
     * @brief Apply barrel/pincushion distortion correction
     * @param input Input image blob
     * @param k1 Radial distortion coefficient 1
     * @param k2 Radial distortion coefficient 2
     * @param k3 Radial distortion coefficient 3
     * @param p1 Tangential distortion coefficient 1
     * @param p2 Tangential distortion coefficient 2
     * @param center Distortion center (if empty, use image center)
     * @return Distortion-corrected image blob
     */
    virtual blob correctDistortion(const blob& input, double k1, double k2 = 0,
                                   double k3 = 0, double p1 = 0, double p2 = 0,
                                   const Point2D& center = {}) const;

    /**
     * @brief Apply elastic deformation (rubber sheet transformation)
     * @param input Input image blob
     * @param displacementX X displacement field
     * @param displacementY Y displacement field
     * @param method Interpolation method
     * @return Deformed image blob
     */
    virtual blob elasticDeform(
        const blob& input,
        const std::vector<std::vector<double>>& displacementX,
        const std::vector<std::vector<double>>& displacementY,
        InterpolationMethod method = InterpolationMethod::LINEAR) const;

    /**
     * @brief Apply polar transformation (Cartesian to polar coordinates)
     * @param input Input image blob
     * @param center Center point for polar transformation
     * @param maxRadius Maximum radius (if 0, use image diagonal)
     * @param angleRange Angle range in degrees (default: full circle)
     * @return Polar-transformed image blob
     */
    virtual blob toPolar(const blob& input, const Point2D& center = {},
                         double maxRadius = 0, double angleRange = 360.0) const;

    /**
     * @brief Apply inverse polar transformation (polar to Cartesian)
     * @param input Input polar image blob
     * @param outputSize Output image size
     * @param center Center point for transformation
     * @return Cartesian-transformed image blob
     */
    virtual blob fromPolar(const blob& input, const Point2D& outputSize,
                           const Point2D& center = {}) const;

    /**
     * @brief Register two images using feature matching
     * @param reference Reference image
     * @param target Target image to register
     * @param method Registration method
     * @return Transformation matrix to align target with reference
     */
    virtual TransformMatrix registerImages(
        const blob& reference, const blob& target,
        const std::string& method = "orb") const;

    /**
     * @brief Apply image warping using control points
     * @param input Input image blob
     * @param srcPoints Source control points
     * @param dstPoints Destination control points
     * @param method Warping method ("thin_plate_spline", "rbf", "polynomial")
     * @return Warped image blob
     */
    virtual blob warpControlPoints(
        const blob& input, const std::vector<Point2D>& srcPoints,
        const std::vector<Point2D>& dstPoints,
        const std::string& method = "thin_plate_spline") const;

    /**
     * @brief Create seamless panorama from multiple images
     * @param images Vector of input images
     * @param method Stitching method ("cylindrical", "spherical", "planar")
     * @param blendMode Blending mode ("linear", "multiband", "feather")
     * @return Stitched panorama image
     */
    virtual blob stitchPanorama(
        const std::vector<blob>& images,
        const std::string& method = "cylindrical",
        const std::string& blendMode = "multiband") const;

    // Static utility functions

    /**
     * @brief Create identity transformation matrix
     * @return 3x3 identity matrix
     */
    static TransformMatrix createIdentityMatrix();

    /**
     * @brief Create translation matrix
     * @param dx Translation in X direction
     * @param dy Translation in Y direction
     * @return Translation matrix
     */
    static TransformMatrix createTranslationMatrix(double dx, double dy);

    /**
     * @brief Create rotation matrix
     * @param angle Rotation angle in degrees
     * @param center Rotation center
     * @return Rotation matrix
     */
    static TransformMatrix createRotationMatrix(double angle,
                                                const Point2D& center = {});

    /**
     * @brief Create scaling matrix
     * @param sx Scale factor in X direction
     * @param sy Scale factor in Y direction
     * @param center Scaling center
     * @return Scaling matrix
     */
    static TransformMatrix createScalingMatrix(double sx, double sy,
                                               const Point2D& center = {});

    /**
     * @brief Create shear matrix
     * @param shx Shear factor in X direction
     * @param shy Shear factor in Y direction
     * @return Shear matrix
     */
    static TransformMatrix createShearMatrix(double shx, double shy);

    /**
     * @brief Multiply two transformation matrices
     * @param a First matrix
     * @param b Second matrix
     * @return Product matrix (a * b)
     */
    static TransformMatrix multiplyMatrices(const TransformMatrix& a,
                                            const TransformMatrix& b);

    /**
     * @brief Invert transformation matrix
     * @param matrix Input matrix
     * @return Inverted matrix
     */
    static TransformMatrix invertMatrix(const TransformMatrix& matrix);

    /**
     * @brief Transform point using transformation matrix
     * @param point Input point
     * @param matrix Transformation matrix
     * @return Transformed point
     */
    static Point2D transformPoint(const Point2D& point,
                                  const TransformMatrix& matrix);

protected:
    /**
     * @brief Apply generic transformation with custom mapping function
     * @param input Input image blob
     * @param outputSize Output image size
     * @param mapFunction Function that maps output coordinates to input
     * coordinates
     * @param method Interpolation method
     * @param borderMode Border handling mode
     * @return Transformed image blob
     */
    virtual blob applyTransformation(
        const blob& input, const Point2D& outputSize,
        std::function<Point2D(const Point2D&)> mapFunction,
        InterpolationMethod method = InterpolationMethod::LINEAR,
        BorderMode borderMode = BorderMode::CONSTANT) const;

    /**
     * @brief Interpolate pixel value at fractional coordinates
     * @param input Input image data
     * @param x X coordinate (can be fractional)
     * @param y Y coordinate (can be fractional)
     * @param width Image width
     * @param height Image height
     * @param channels Number of channels
     * @param method Interpolation method
     * @param borderMode Border handling mode
     * @return Interpolated pixel values
     */
    virtual std::vector<uint8_t> interpolatePixel(
        const std::vector<std::byte>& input, double x, double y, int width,
        int height, int channels, InterpolationMethod method,
        BorderMode borderMode) const;
};

/**
 * @brief Factory function to create optimal transform processor
 * @param useGPU Whether to use GPU acceleration if available
 * @return Unique pointer to transform processor
 */
std::unique_ptr<ImageTransform> createOptimalTransform(bool useGPU = false);

}  // namespace atom::image

#endif  // ATOM_IMAGE_TRANSFORMS_HPP
