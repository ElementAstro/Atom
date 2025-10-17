#include "transforms.hpp"
#include <algorithm>
#include <cmath>
#include <execution>
#include "gpu_acceleration.hpp"

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

    // Handle out-of-bounds coordinates
    if (x < 0 || x >= width || y < 0 || y >= height) {
        if (borderMode == BorderMode::CONSTANT) {
            return result;  // Already initialized to 0
        }
        // TODO: Implement other border modes
        return result;
    }

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
