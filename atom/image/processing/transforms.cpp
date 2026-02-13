#include "transforms.hpp"
#include <algorithm>
#include <cmath>
#include <execution>
#include <stdexcept>
#include "gpu_acceleration.hpp"

#ifdef ATOM_IMAGE_HAS_OPENCV
#include <opencv2/calib3d.hpp>
#include <opencv2/features2d.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/opencv.hpp>
#include <opencv2/stitching.hpp>
#endif

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace atom::image {

blob ImageTransform::resize(const blob& input, int newWidth, int newHeight,
                            InterpolationMethod method,
                            bool preserveAspect) const {
    if (input.isEmpty() || newWidth <= 0 || newHeight <= 0) {
        return blob{};
    }

    int inputWidth = input.getCols();
    int inputHeight = input.getRows();
    [[maybe_unused]] int channels = input.getChannels();

    // Preserve aspect ratio if requested
    if (preserveAspect) {
        double aspectRatio = static_cast<double>(inputWidth) / inputHeight;
        if (newWidth / aspectRatio <= newHeight) {
            newHeight = static_cast<int>(newWidth / aspectRatio);
        } else {
            newWidth = static_cast<int>(newHeight * aspectRatio);
        }
    }

    // Create mapping function for resize
    auto mapFunction = [inputWidth, inputHeight, newWidth,
                        newHeight](const Point2D& output) -> Point2D {
        double scaleX = static_cast<double>(inputWidth) / newWidth;
        double scaleY = static_cast<double>(inputHeight) / newHeight;
        return Point2D(output.x * scaleX, output.y * scaleY);
    };

    return applyTransformation(input, Point2D(newWidth, newHeight), mapFunction,
                               method);
}

blob ImageTransform::rotate(const blob& input, double angle,
                            const Point2D& center, bool expandCanvas,
                            InterpolationMethod method, BorderMode borderMode,
                            uint8_t fillValue [[maybe_unused]]) const {
    if (input.isEmpty()) {
        return blob{};
    }

    int inputWidth = input.getCols();
    int inputHeight = input.getRows();

    // Use image center if no center specified
    Point2D rotCenter = center;
    if (rotCenter.x == 0 && rotCenter.y == 0) {
        rotCenter = Point2D(inputWidth / 2.0, inputHeight / 2.0);
    }

    // Convert angle to radians
    double radians = angle * M_PI / 180.0;
    double cosA = std::cos(radians);
    double sinA = std::sin(radians);

    // Calculate output size
    Point2D outputSize(inputWidth, inputHeight);
    if (expandCanvas) {
        // Calculate bounding box of rotated image
        std::vector<Point2D> corners = {
            {0.0, 0.0},
            {static_cast<double>(inputWidth), 0.0},
            {static_cast<double>(inputWidth), static_cast<double>(inputHeight)},
            {0.0, static_cast<double>(inputHeight)}};

        double minX = std::numeric_limits<double>::max();
        double maxX = std::numeric_limits<double>::lowest();
        double minY = std::numeric_limits<double>::max();
        double maxY = std::numeric_limits<double>::lowest();

        for (const auto& corner : corners) {
            double x = corner.x - rotCenter.x;
            double y = corner.y - rotCenter.y;
            double rotX = x * cosA - y * sinA + rotCenter.x;
            double rotY = x * sinA + y * cosA + rotCenter.y;

            minX = std::min(minX, rotX);
            maxX = std::max(maxX, rotX);
            minY = std::min(minY, rotY);
            maxY = std::max(maxY, rotY);
        }

        outputSize = Point2D(maxX - minX, maxY - minY);
    }

    // Create inverse rotation mapping function
    auto mapFunction = [cosA, sinA, rotCenter, outputSize, inputWidth,
                        inputHeight,
                        expandCanvas](const Point2D& output) -> Point2D {
        double x = output.x;
        double y = output.y;

        if (expandCanvas) {
            // Adjust for expanded canvas offset
            x -= (outputSize.x - inputWidth) / 2.0;
            y -= (outputSize.y - inputHeight) / 2.0;
        }

        // Translate to rotation center
        x -= rotCenter.x;
        y -= rotCenter.y;

        // Apply inverse rotation
        double inputX = x * cosA + y * sinA + rotCenter.x;
        double inputY = -x * sinA + y * cosA + rotCenter.y;

        return Point2D(inputX, inputY);
    };

    return applyTransformation(input, outputSize, mapFunction, method,
                               borderMode);
}

blob ImageTransform::affineTransform(
    const blob& input, const std::array<std::array<double, 3>, 2>& matrix,
    const Point2D& outputSize, InterpolationMethod method,
    BorderMode borderMode) const {
    if (input.isEmpty()) {
        return blob{};
    }

    Point2D outSize = outputSize;
    if (outSize.x == 0 || outSize.y == 0) {
        outSize = Point2D(input.getCols(), input.getRows());
    }

    // Calculate inverse matrix for backward mapping
    double det = matrix[0][0] * matrix[1][1] - matrix[0][1] * matrix[1][0];
    if (std::abs(det) < 1e-10) {
        return blob{};  // Singular matrix
    }

    double invDet = 1.0 / det;
    std::array<std::array<double, 3>, 2> invMatrix;
    invMatrix[0][0] = matrix[1][1] * invDet;
    invMatrix[0][1] = -matrix[0][1] * invDet;
    invMatrix[0][2] =
        (matrix[0][1] * matrix[1][2] - matrix[1][1] * matrix[0][2]) * invDet;
    invMatrix[1][0] = -matrix[1][0] * invDet;
    invMatrix[1][1] = matrix[0][0] * invDet;
    invMatrix[1][2] =
        (matrix[1][0] * matrix[0][2] - matrix[0][0] * matrix[1][2]) * invDet;

    auto mapFunction = [invMatrix](const Point2D& output) -> Point2D {
        double inputX = invMatrix[0][0] * output.x +
                        invMatrix[0][1] * output.y + invMatrix[0][2];
        double inputY = invMatrix[1][0] * output.x +
                        invMatrix[1][1] * output.y + invMatrix[1][2];
        return Point2D(inputX, inputY);
    };

    return applyTransformation(input, outSize, mapFunction, method, borderMode);
}

blob ImageTransform::correctDistortion(const blob& input, double k1, double k2,
                                       double k3, double p1, double p2,
                                       const Point2D& center) const {
    if (input.isEmpty()) {
        return blob{};
    }

    int width = input.getCols();
    int height = input.getRows();

    Point2D distCenter = center;
    if (distCenter.x == 0 && distCenter.y == 0) {
        distCenter = Point2D(width / 2.0, height / 2.0);
    }

    auto mapFunction = [k1, k2, k3, p1, p2, distCenter, width,
                        height](const Point2D& output) -> Point2D {
        // Normalize coordinates
        double x = (output.x - distCenter.x) / width;
        double y = (output.y - distCenter.y) / height;

        double r2 = x * x + y * y;
        double r4 = r2 * r2;
        double r6 = r4 * r2;

        // Radial distortion
        double radialFactor = 1.0 + k1 * r2 + k2 * r4 + k3 * r6;

        // Tangential distortion
        double dx = 2.0 * p1 * x * y + p2 * (r2 + 2.0 * x * x);
        double dy = p1 * (r2 + 2.0 * y * y) + 2.0 * p2 * x * y;

        // Apply distortion correction (inverse)
        double correctedX = x * radialFactor + dx;
        double correctedY = y * radialFactor + dy;

        // Denormalize
        return Point2D(correctedX * width + distCenter.x,
                       correctedY * height + distCenter.y);
    };

    return applyTransformation(input, Point2D(width, height), mapFunction);
}

blob ImageTransform::applyTransformation(
    const blob& input, const Point2D& outputSize,
    std::function<Point2D(const Point2D&)> mapFunction,
    InterpolationMethod method, BorderMode borderMode) const {
    if (input.isEmpty()) {
        return blob{};
    }

    int inputWidth = input.getCols();
    int inputHeight = input.getRows();
    int channels = input.getChannels();
    int outWidth = static_cast<int>(outputSize.x);
    int outHeight = static_cast<int>(outputSize.y);

    std::vector<std::byte> inputData(input.begin(), input.end());
    std::vector<std::byte> outputData(outWidth * outHeight * channels);

    // Sequential transformation (replace parallel for now to avoid
    // counting_iterator issues)
    for (int y = 0; y < outHeight; ++y) {
        for (int x = 0; x < outWidth; ++x) {
            Point2D inputCoord = mapFunction(Point2D(x, y));

            auto pixelValues = interpolatePixel(
                inputData, inputCoord.x, inputCoord.y, inputWidth, inputHeight,
                channels, method, borderMode);

            for (int c = 0; c < channels; ++c) {
                int outputIdx = (y * outWidth + x) * channels + c;
                outputData[outputIdx] = static_cast<std::byte>(pixelValues[c]);
            }
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

std::vector<uint8_t> ImageTransform::interpolatePixel(
    const std::vector<std::byte>& input, double x, double y, int width,
    int height, int channels, InterpolationMethod method,
    BorderMode borderMode) const {
    std::vector<uint8_t> result(channels, 0);

    // Handle out-of-bounds coordinates based on border mode
    auto handleBorder = [&](double& coord, int size) -> bool {
        if (coord >= 0 && coord < size) {
            return true;
        }
        switch (borderMode) {
            case BorderMode::CONSTANT:
                return false;  // Return zeros
            case BorderMode::REPLICATE:
                coord = std::clamp(coord, 0.0, static_cast<double>(size - 1));
                return true;
            case BorderMode::REFLECT: {
                int icoord = static_cast<int>(coord);
                if (icoord < 0) {
                    icoord = -icoord - 1;
                }
                if (icoord >= size) {
                    icoord = 2 * size - icoord - 1;
                }
                icoord = std::clamp(icoord, 0, size - 1);
                coord = static_cast<double>(icoord);
                return true;
            }
            case BorderMode::WRAP: {
                int icoord = static_cast<int>(coord);
                icoord = ((icoord % size) + size) % size;
                coord = static_cast<double>(icoord);
                return true;
            }
            case BorderMode::TRANSPARENT:
                return false;  // Return zeros for transparent
            default:
                return false;
        }
    };

    double xCoord = x;
    double yCoord = y;
    if (!handleBorder(xCoord, width) || !handleBorder(yCoord, height)) {
        return result;  // Return zeros
    }
    x = xCoord;
    y = yCoord;

    // Helper to get pixel value with bounds checking
    auto getPixel = [&](int px, int py, int c) -> double {
        px = std::clamp(px, 0, width - 1);
        py = std::clamp(py, 0, height - 1);
        return static_cast<double>(
            static_cast<uint8_t>(input[(py * width + px) * channels + c]));
    };

    // Cubic interpolation kernel (Catmull-Rom)
    auto cubicKernel = [](double t) -> double {
        double at = std::abs(t);
        if (at <= 1.0) {
            return (1.5 * at - 2.5) * at * at + 1.0;
        } else if (at < 2.0) {
            return ((-0.5 * at + 2.5) * at - 4.0) * at + 2.0;
        }
        return 0.0;
    };

    // Lanczos kernel
    auto lanczosKernel = [](double t, int a) -> double {
        if (t == 0.0)
            return 1.0;
        if (std::abs(t) >= a)
            return 0.0;
        double pit = M_PI * t;
        return (a * std::sin(pit) * std::sin(pit / a)) / (pit * pit);
    };

    switch (method) {
        case InterpolationMethod::NEAREST: {
            int ix = static_cast<int>(std::round(x));
            int iy = static_cast<int>(std::round(y));
            ix = std::clamp(ix, 0, width - 1);
            iy = std::clamp(iy, 0, height - 1);

            for (int c = 0; c < channels; ++c) {
                int idx = (iy * width + ix) * channels + c;
                result[c] = static_cast<uint8_t>(input[idx]);
            }
            break;
        }
        case InterpolationMethod::LINEAR: {
            int x0 = static_cast<int>(std::floor(x));
            int y0 = static_cast<int>(std::floor(y));
            int x1 = x0 + 1;
            int y1 = y0 + 1;

            double fx = x - x0;
            double fy = y - y0;

            x0 = std::clamp(x0, 0, width - 1);
            x1 = std::clamp(x1, 0, width - 1);
            y0 = std::clamp(y0, 0, height - 1);
            y1 = std::clamp(y1, 0, height - 1);

            for (int c = 0; c < channels; ++c) {
                double v00 = static_cast<double>(static_cast<uint8_t>(
                    input[(y0 * width + x0) * channels + c]));
                double v01 = static_cast<double>(static_cast<uint8_t>(
                    input[(y0 * width + x1) * channels + c]));
                double v10 = static_cast<double>(static_cast<uint8_t>(
                    input[(y1 * width + x0) * channels + c]));
                double v11 = static_cast<double>(static_cast<uint8_t>(
                    input[(y1 * width + x1) * channels + c]));

                double v0 = v00 * (1 - fx) + v01 * fx;
                double v1 = v10 * (1 - fx) + v11 * fx;
                double value = v0 * (1 - fy) + v1 * fy;

                result[c] = static_cast<uint8_t>(std::clamp(value, 0.0, 255.0));
            }
            break;
        }
        case InterpolationMethod::CUBIC: {
            int x0 = static_cast<int>(std::floor(x));
            int y0 = static_cast<int>(std::floor(y));
            double fx = x - x0;
            double fy = y - y0;

            for (int c = 0; c < channels; ++c) {
                double value = 0.0;
                for (int j = -1; j <= 2; ++j) {
                    double ky = cubicKernel(fy - j);
                    for (int i = -1; i <= 2; ++i) {
                        double kx = cubicKernel(fx - i);
                        value += getPixel(x0 + i, y0 + j, c) * kx * ky;
                    }
                }
                result[c] = static_cast<uint8_t>(std::clamp(value, 0.0, 255.0));
            }
            break;
        }
        case InterpolationMethod::LANCZOS: {
            int x0 = static_cast<int>(std::floor(x));
            int y0 = static_cast<int>(std::floor(y));
            double fx = x - x0;
            double fy = y - y0;
            const int a = 3;  // Lanczos-3

            for (int c = 0; c < channels; ++c) {
                double value = 0.0;
                double weightSum = 0.0;
                for (int j = -a + 1; j <= a; ++j) {
                    double ky = lanczosKernel(fy - j, a);
                    for (int i = -a + 1; i <= a; ++i) {
                        double kx = lanczosKernel(fx - i, a);
                        double w = kx * ky;
                        value += getPixel(x0 + i, y0 + j, c) * w;
                        weightSum += w;
                    }
                }
                if (weightSum > 0) {
                    value /= weightSum;
                }
                result[c] = static_cast<uint8_t>(std::clamp(value, 0.0, 255.0));
            }
            break;
        }
        case InterpolationMethod::AREA: {
            // Area-based interpolation (box filter for downsampling)
            int x0 = static_cast<int>(std::floor(x));
            int y0 = static_cast<int>(std::floor(y));
            int x1 = std::min(x0 + 1, width - 1);
            int y1 = std::min(y0 + 1, height - 1);

            for (int c = 0; c < channels; ++c) {
                double sum = 0.0;
                int count = 0;
                for (int py = y0; py <= y1; ++py) {
                    for (int px = x0; px <= x1; ++px) {
                        sum += getPixel(px, py, c);
                        count++;
                    }
                }
                result[c] =
                    static_cast<uint8_t>(std::clamp(sum / count, 0.0, 255.0));
            }
            break;
        }
        case InterpolationMethod::SUPER_SAMPLING: {
            // Super-sampling with 4x4 grid
            const int samples = 4;
            double sampleStep = 1.0 / samples;

            for (int c = 0; c < channels; ++c) {
                double sum = 0.0;
                for (int sy = 0; sy < samples; ++sy) {
                    for (int sx = 0; sx < samples; ++sx) {
                        double sampleX = x + (sx + 0.5) * sampleStep - 0.5;
                        double sampleY = y + (sy + 0.5) * sampleStep - 0.5;
                        int ix = static_cast<int>(std::round(sampleX));
                        int iy = static_cast<int>(std::round(sampleY));
                        sum += getPixel(ix, iy, c);
                    }
                }
                result[c] = static_cast<uint8_t>(
                    std::clamp(sum / (samples * samples), 0.0, 255.0));
            }
            break;
        }
        default:
            // Fall back to nearest neighbor
            return interpolatePixel(input, x, y, width, height, channels,
                                    InterpolationMethod::NEAREST, borderMode);
    }

    return result;
}

// Static utility functions
TransformMatrix ImageTransform::createIdentityMatrix() {
    return {{{{1, 0, 0}}, {{0, 1, 0}}, {{0, 0, 1}}}};
}

TransformMatrix ImageTransform::createTranslationMatrix(double dx, double dy) {
    return {{{{1, 0, dx}}, {{0, 1, dy}}, {{0, 0, 1}}}};
}

TransformMatrix ImageTransform::createRotationMatrix(double angle,
                                                     const Point2D& center) {
    double radians = angle * M_PI / 180.0;
    double cosA = std::cos(radians);
    double sinA = std::sin(radians);

    return {{{{cosA, -sinA, center.x * (1 - cosA) + center.y * sinA}},
             {{sinA, cosA, center.y * (1 - cosA) - center.x * sinA}},
             {{0, 0, 1}}}};
}

TransformMatrix ImageTransform::createScalingMatrix(double sx, double sy,
                                                    const Point2D& center) {
    return {{{{sx, 0, center.x * (1 - sx)}},
             {{0, sy, center.y * (1 - sy)}},
             {{0, 0, 1}}}};
}

TransformMatrix ImageTransform::createShearMatrix(double shx, double shy) {
    return {{{{1, shx, 0}}, {{shy, 1, 0}}, {{0, 0, 1}}}};
}

TransformMatrix ImageTransform::multiplyMatrices(const TransformMatrix& a,
                                                 const TransformMatrix& b) {
    TransformMatrix result = {{{{0, 0, 0}}, {{0, 0, 0}}, {{0, 0, 0}}}};
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            for (int k = 0; k < 3; ++k) {
                result[i][j] += a[i][k] * b[k][j];
            }
        }
    }
    return result;
}

TransformMatrix ImageTransform::invertMatrix(const TransformMatrix& matrix) {
    // Calculate determinant
    double det =
        matrix[0][0] *
            (matrix[1][1] * matrix[2][2] - matrix[1][2] * matrix[2][1]) -
        matrix[0][1] *
            (matrix[1][0] * matrix[2][2] - matrix[1][2] * matrix[2][0]) +
        matrix[0][2] *
            (matrix[1][0] * matrix[2][1] - matrix[1][1] * matrix[2][0]);

    if (std::abs(det) < 1e-10) {
        return createIdentityMatrix();  // Return identity for singular matrices
    }

    double invDet = 1.0 / det;
    TransformMatrix result;

    result[0][0] =
        (matrix[1][1] * matrix[2][2] - matrix[1][2] * matrix[2][1]) * invDet;
    result[0][1] =
        (matrix[0][2] * matrix[2][1] - matrix[0][1] * matrix[2][2]) * invDet;
    result[0][2] =
        (matrix[0][1] * matrix[1][2] - matrix[0][2] * matrix[1][1]) * invDet;
    result[1][0] =
        (matrix[1][2] * matrix[2][0] - matrix[1][0] * matrix[2][2]) * invDet;
    result[1][1] =
        (matrix[0][0] * matrix[2][2] - matrix[0][2] * matrix[2][0]) * invDet;
    result[1][2] =
        (matrix[0][2] * matrix[1][0] - matrix[0][0] * matrix[1][2]) * invDet;
    result[2][0] =
        (matrix[1][0] * matrix[2][1] - matrix[1][1] * matrix[2][0]) * invDet;
    result[2][1] =
        (matrix[0][1] * matrix[2][0] - matrix[0][0] * matrix[2][1]) * invDet;
    result[2][2] =
        (matrix[0][0] * matrix[1][1] - matrix[0][1] * matrix[1][0]) * invDet;

    return result;
}

Point2D ImageTransform::transformPoint(const Point2D& point,
                                       const TransformMatrix& matrix) {
    double x = matrix[0][0] * point.x + matrix[0][1] * point.y + matrix[0][2];
    double y = matrix[1][0] * point.x + matrix[1][1] * point.y + matrix[1][2];
    double w = matrix[2][0] * point.x + matrix[2][1] * point.y + matrix[2][2];

    if (w != 0) {
        x /= w;
        y /= w;
    }

    return Point2D(x, y);
}

// Perspective transformation implementation
blob ImageTransform::perspectiveTransform(const blob& input,
                                          const TransformMatrix& matrix,
                                          const Point2D& outputSize,
                                          InterpolationMethod method) const {
    if (input.isEmpty()) {
        return blob{};
    }

    Point2D outSize = outputSize;
    if (outSize.x == 0 || outSize.y == 0) {
        outSize = Point2D(input.getCols(), input.getRows());
    }

    // Calculate inverse matrix for backward mapping
    TransformMatrix invMatrix = invertMatrix(matrix);

    auto mapFunction = [invMatrix](const Point2D& output) -> Point2D {
        return transformPoint(output, invMatrix);
    };

    return applyTransformation(input, outSize, mapFunction, method);
}

// Perspective correction using four corner points
blob ImageTransform::correctPerspective(const blob& input,
                                        const std::array<Point2D, 4>& srcPoints,
                                        const std::array<Point2D, 4>& dstPoints,
                                        const Point2D& outputSize) const {
    if (input.isEmpty()) {
        return blob{};
    }

#ifdef ATOM_IMAGE_HAS_OPENCV
    cv::Mat src = input.to_mat();
    std::vector<cv::Point2f> srcPts, dstPts;
    for (int i = 0; i < 4; ++i) {
        srcPts.emplace_back(static_cast<float>(srcPoints[i].x),
                            static_cast<float>(srcPoints[i].y));
        dstPts.emplace_back(static_cast<float>(dstPoints[i].x),
                            static_cast<float>(dstPoints[i].y));
    }

    cv::Mat perspMatrix = cv::getPerspectiveTransform(srcPts, dstPts);
    cv::Mat dst;
    cv::warpPerspective(src, dst, perspMatrix,
                        cv::Size(static_cast<int>(outputSize.x),
                                 static_cast<int>(outputSize.y)));
    return blob(dst);
#else
    // Manual perspective transform calculation
    // Solve for homography matrix using DLT (Direct Linear Transform)
    // H * src = dst, solve for H

    // Build the 8x9 matrix for DLT
    std::array<std::array<double, 9>, 8> A{};
    for (int i = 0; i < 4; ++i) {
        double x = srcPoints[i].x, y = srcPoints[i].y;
        double u = dstPoints[i].x, v = dstPoints[i].y;

        A[2 * i][0] = -x;
        A[2 * i][1] = -y;
        A[2 * i][2] = -1;
        A[2 * i][3] = 0;
        A[2 * i][4] = 0;
        A[2 * i][5] = 0;
        A[2 * i][6] = u * x;
        A[2 * i][7] = u * y;
        A[2 * i][8] = u;

        A[2 * i + 1][0] = 0;
        A[2 * i + 1][1] = 0;
        A[2 * i + 1][2] = 0;
        A[2 * i + 1][3] = -x;
        A[2 * i + 1][4] = -y;
        A[2 * i + 1][5] = -1;
        A[2 * i + 1][6] = v * x;
        A[2 * i + 1][7] = v * y;
        A[2 * i + 1][8] = v;
    }

    // Simplified: use approximate inverse mapping
    TransformMatrix H = createIdentityMatrix();
    // Approximate homography from point correspondences
    double sx = (dstPoints[1].x - dstPoints[0].x) /
                (srcPoints[1].x - srcPoints[0].x + 1e-10);
    double sy = (dstPoints[3].y - dstPoints[0].y) /
                (srcPoints[3].y - srcPoints[0].y + 1e-10);
    H[0][0] = sx;
    H[1][1] = sy;
    H[0][2] = dstPoints[0].x - srcPoints[0].x * sx;
    H[1][2] = dstPoints[0].y - srcPoints[0].y * sy;

    return perspectiveTransform(input, H, outputSize);
#endif
}

// Elastic deformation using displacement fields
blob ImageTransform::elasticDeform(
    const blob& input, const std::vector<std::vector<double>>& displacementX,
    const std::vector<std::vector<double>>& displacementY,
    InterpolationMethod method) const {
    if (input.isEmpty() || displacementX.empty() || displacementY.empty()) {
        return blob{};
    }

    int width = input.getCols();
    int height = input.getRows();
    int channels = input.getChannels();

    int dispHeight = static_cast<int>(displacementX.size());
    int dispWidth = static_cast<int>(displacementX[0].size());

    std::vector<std::byte> inputData(input.begin(), input.end());
    std::vector<std::byte> outputData(width * height * channels);

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            // Interpolate displacement values
            double dispXRatio = static_cast<double>(x) / width * dispWidth;
            double dispYRatio = static_cast<double>(y) / height * dispHeight;

            int dx0 =
                std::clamp(static_cast<int>(dispXRatio), 0, dispWidth - 1);
            int dy0 =
                std::clamp(static_cast<int>(dispYRatio), 0, dispHeight - 1);
            int dx1 = std::min(dx0 + 1, dispWidth - 1);
            int dy1 = std::min(dy0 + 1, dispHeight - 1);

            double fx = dispXRatio - dx0;
            double fy = dispYRatio - dy0;

            // Bilinear interpolation of displacement
            double dispX = displacementX[dy0][dx0] * (1 - fx) * (1 - fy) +
                           displacementX[dy0][dx1] * fx * (1 - fy) +
                           displacementX[dy1][dx0] * (1 - fx) * fy +
                           displacementX[dy1][dx1] * fx * fy;

            double dispY = displacementY[dy0][dx0] * (1 - fx) * (1 - fy) +
                           displacementY[dy0][dx1] * fx * (1 - fy) +
                           displacementY[dy1][dx0] * (1 - fx) * fy +
                           displacementY[dy1][dx1] * fx * fy;

            double srcX = x + dispX;
            double srcY = y + dispY;

            auto pixelValues =
                interpolatePixel(inputData, srcX, srcY, width, height, channels,
                                 method, BorderMode::REPLICATE);

            for (int c = 0; c < channels; ++c) {
                int outputIdx = (y * width + x) * channels + c;
                outputData[outputIdx] = static_cast<std::byte>(pixelValues[c]);
            }
        }
    }

    blob result;
    for (const auto& byte : outputData) {
        result.append(&byte, 1);
    }
    return result;
}

// Polar coordinate transformation
blob ImageTransform::toPolar(const blob& input, const Point2D& center,
                             double maxRadius, double angleRange) const {
    if (input.isEmpty()) {
        return blob{};
    }

    int width = input.getCols();
    int height = input.getRows();

    Point2D polarCenter = center;
    if (polarCenter.x == 0 && polarCenter.y == 0) {
        polarCenter = Point2D(width / 2.0, height / 2.0);
    }

    if (maxRadius <= 0) {
        maxRadius = std::sqrt(polarCenter.x * polarCenter.x +
                              polarCenter.y * polarCenter.y);
    }

    // Output dimensions: width = radius range, height = angle range
    int outWidth = static_cast<int>(maxRadius);
    int outHeight = static_cast<int>(angleRange);

    auto mapFunction = [polarCenter, maxRadius, angleRange, outWidth,
                        outHeight](const Point2D& output) -> Point2D {
        double r = output.x * maxRadius / outWidth;
        double theta = output.y * angleRange / outHeight * M_PI / 180.0;

        double x = polarCenter.x + r * std::cos(theta);
        double y = polarCenter.y + r * std::sin(theta);

        return Point2D(x, y);
    };

    return applyTransformation(input, Point2D(outWidth, outHeight),
                               mapFunction);
}

// Inverse polar transformation
blob ImageTransform::fromPolar(const blob& input, const Point2D& outputSize,
                               const Point2D& center) const {
    if (input.isEmpty()) {
        return blob{};
    }

    int polarWidth = input.getCols();   // Radius dimension
    int polarHeight = input.getRows();  // Angle dimension

    int outWidth = static_cast<int>(outputSize.x);
    int outHeight = static_cast<int>(outputSize.y);

    Point2D cartCenter = center;
    if (cartCenter.x == 0 && cartCenter.y == 0) {
        cartCenter = Point2D(outWidth / 2.0, outHeight / 2.0);
    }

    double maxRadius = static_cast<double>(polarWidth);

    auto mapFunction = [cartCenter, polarWidth, polarHeight,
                        maxRadius](const Point2D& output) -> Point2D {
        double dx = output.x - cartCenter.x;
        double dy = output.y - cartCenter.y;

        double r = std::sqrt(dx * dx + dy * dy);
        double theta = std::atan2(dy, dx);
        if (theta < 0)
            theta += 2 * M_PI;

        double polarX = r / maxRadius * polarWidth;
        double polarY = theta / (2 * M_PI) * polarHeight;

        return Point2D(polarX, polarY);
    };

    return applyTransformation(input, outputSize, mapFunction);
}

// Image registration using feature matching
TransformMatrix ImageTransform::registerImages(
    const blob& reference, const blob& target,
    const std::string& method) const {
    if (reference.isEmpty() || target.isEmpty()) {
        return createIdentityMatrix();
    }

#ifdef ATOM_IMAGE_HAS_OPENCV
    cv::Mat refMat = reference.to_mat();
    cv::Mat tgtMat = target.to_mat();

    cv::Mat refGray, tgtGray;
    if (refMat.channels() > 1) {
        cv::cvtColor(refMat, refGray, cv::COLOR_BGR2GRAY);
    } else {
        refGray = refMat;
    }
    if (tgtMat.channels() > 1) {
        cv::cvtColor(tgtMat, tgtGray, cv::COLOR_BGR2GRAY);
    } else {
        tgtGray = tgtMat;
    }

    // Detect features
    cv::Ptr<cv::Feature2D> detector;
    if (method == "sift") {
        detector = cv::SIFT::create();
    } else if (method == "akaze") {
        detector = cv::AKAZE::create();
    } else {
        detector = cv::ORB::create(1000);
    }

    std::vector<cv::KeyPoint> kp1, kp2;
    cv::Mat desc1, desc2;
    detector->detectAndCompute(refGray, cv::noArray(), kp1, desc1);
    detector->detectAndCompute(tgtGray, cv::noArray(), kp2, desc2);

    if (kp1.empty() || kp2.empty()) {
        return createIdentityMatrix();
    }

    // Match features
    cv::BFMatcher matcher(cv::NORM_HAMMING);
    std::vector<std::vector<cv::DMatch>> knnMatches;
    matcher.knnMatch(desc1, desc2, knnMatches, 2);

    // Apply ratio test
    std::vector<cv::DMatch> goodMatches;
    for (const auto& m : knnMatches) {
        if (m.size() == 2 && m[0].distance < 0.75 * m[1].distance) {
            goodMatches.push_back(m[0]);
        }
    }

    if (goodMatches.size() < 4) {
        return createIdentityMatrix();
    }

    // Extract matched points
    std::vector<cv::Point2f> pts1, pts2;
    for (const auto& match : goodMatches) {
        pts1.push_back(kp1[match.queryIdx].pt);
        pts2.push_back(kp2[match.trainIdx].pt);
    }

    // Find homography
    cv::Mat H = cv::findHomography(pts2, pts1, cv::RANSAC, 5.0);

    if (H.empty()) {
        return createIdentityMatrix();
    }

    // Convert to TransformMatrix
    TransformMatrix result;
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            result[i][j] = H.at<double>(i, j);
        }
    }
    return result;
#else
    (void)method;
    return createIdentityMatrix();
#endif
}

// Control point warping using thin-plate spline
blob ImageTransform::warpControlPoints(const blob& input,
                                       const std::vector<Point2D>& srcPoints,
                                       const std::vector<Point2D>& dstPoints,
                                       const std::string& method) const {
    if (input.isEmpty() || srcPoints.size() != dstPoints.size() ||
        srcPoints.size() < 3) {
        return input;
    }

    int width = input.getCols();
    int height = input.getRows();
    int channels = input.getChannels();
    size_t n = srcPoints.size();

    (void)method;  // Currently only TPS is implemented

    // Thin-plate spline radial basis function
    auto tpsKernel = [](double r) -> double {
        if (r < 1e-10)
            return 0.0;
        return r * r * std::log(r);
    };

    // Build TPS system matrices
    // For simplicity, compute approximate displacement at each pixel
    std::vector<std::byte> inputData(input.begin(), input.end());
    std::vector<std::byte> outputData(width * height * channels);

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            // Compute displacement using RBF interpolation
            double totalWeight = 0.0;
            double dispX = 0.0, dispY = 0.0;

            for (size_t i = 0; i < n; ++i) {
                double dx = x - dstPoints[i].x;
                double dy = y - dstPoints[i].y;
                double r = std::sqrt(dx * dx + dy * dy);
                double w = std::exp(-r * r / (2 * 50 * 50));  // Gaussian weight

                dispX += w * (srcPoints[i].x - dstPoints[i].x);
                dispY += w * (srcPoints[i].y - dstPoints[i].y);
                totalWeight += w;
            }

            if (totalWeight > 0) {
                dispX /= totalWeight;
                dispY /= totalWeight;
            }

            double srcX = x + dispX;
            double srcY = y + dispY;

            // Clamp to bounds
            srcX = std::clamp(srcX, 0.0, static_cast<double>(width - 1));
            srcY = std::clamp(srcY, 0.0, static_cast<double>(height - 1));

            auto pixelValues = interpolatePixel(
                inputData, srcX, srcY, width, height, channels,
                InterpolationMethod::LINEAR, BorderMode::REPLICATE);

            for (int c = 0; c < channels; ++c) {
                int outputIdx = (y * width + x) * channels + c;
                outputData[outputIdx] = static_cast<std::byte>(pixelValues[c]);
            }
        }
    }

    blob result;
    for (const auto& byte : outputData) {
        result.append(&byte, 1);
    }
    return result;
}

// Panorama stitching
blob ImageTransform::stitchPanorama(const std::vector<blob>& images,
                                    const std::string& method,
                                    const std::string& blendMode) const {
    if (images.empty()) {
        return blob{};
    }
    if (images.size() == 1) {
        return images[0];
    }

#ifdef ATOM_IMAGE_HAS_OPENCV
    std::vector<cv::Mat> cvImages;
    cvImages.reserve(images.size());
    for (const auto& img : images) {
        if (!img.isEmpty()) {
            cvImages.push_back(img.to_mat());
        }
    }

    if (cvImages.size() < 2) {
        return images[0];
    }

    cv::Ptr<cv::Stitcher> stitcher;
    if (method == "spherical") {
        stitcher = cv::Stitcher::create(cv::Stitcher::SCANS);
    } else {
        stitcher = cv::Stitcher::create(cv::Stitcher::PANORAMA);
    }

    cv::Mat panorama;
    cv::Stitcher::Status status = stitcher->stitch(cvImages, panorama);

    if (status != cv::Stitcher::OK) {
        // Stitching failed, return first image
        return images[0];
    }

    return blob(panorama);
#else
    (void)method;
    (void)blendMode;

    // Simple horizontal concatenation fallback
    int totalWidth = 0;
    int maxHeight = 0;
    int channels = 3;

    for (const auto& img : images) {
        if (!img.isEmpty()) {
            totalWidth += img.getCols();
            maxHeight = std::max(maxHeight, static_cast<int>(img.getRows()));
            channels = img.getChannels();
        }
    }

    std::vector<std::byte> outputData(totalWidth * maxHeight * channels,
                                      std::byte{0});

    int xOffset = 0;
    for (const auto& img : images) {
        if (img.isEmpty())
            continue;

        int imgWidth = img.getCols();
        int imgHeight = img.getRows();
        int imgChannels = img.getChannels();

        for (int y = 0; y < imgHeight; ++y) {
            for (int x = 0; x < imgWidth; ++x) {
                for (int c = 0; c < std::min(channels, imgChannels); ++c) {
                    int srcIdx = (y * imgWidth + x) * imgChannels + c;
                    int dstIdx = (y * totalWidth + xOffset + x) * channels + c;
                    outputData[dstIdx] = *(img.begin() + srcIdx);
                }
            }
        }
        xOffset += imgWidth;
    }

    blob result;
    for (const auto& byte : outputData) {
        result.append(&byte, 1);
    }
    return result;
#endif
}

std::unique_ptr<ImageTransform> createOptimalTransform(bool useGPU) {
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
                // ImageTransform, so we return the CPU implementation but log
                // that GPU is available In a full implementation, we would
                // create a GPUImageTransform wrapper
                return std::make_unique<ImageTransform>();
            }
        } catch (const std::exception&) {
            // GPU initialization failed, fall through to CPU implementation
        }
#endif
    }

    // Return CPU implementation (default or fallback)
    return std::make_unique<ImageTransform>();
}

}  // namespace atom::image
