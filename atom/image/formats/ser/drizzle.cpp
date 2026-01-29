// drizzle.cpp
#include "drizzle.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <opencv2/imgproc.hpp>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace serastro {

DrizzleProcessor::DrizzleProcessor()
    : registrar_(std::make_shared<FrameRegistrar>()) {}

DrizzleProcessor::DrizzleProcessor(const DrizzleParameters& params)
    : params_(params), registrar_(std::make_shared<FrameRegistrar>()) {}

void DrizzleProcessor::initialize(int inputWidth, int inputHeight) {
    inputWidth_ = inputWidth;
    inputHeight_ = inputHeight;
    outputWidth_ = static_cast<int>(inputWidth * params_.scaleFactor);
    outputHeight_ = static_cast<int>(inputHeight * params_.scaleFactor);

    // Initialize accumulation buffers
    outputSum_ = cv::Mat::zeros(outputHeight_, outputWidth_, CV_64FC3);
    weightSum_ = cv::Mat::zeros(outputHeight_, outputWidth_, CV_64F);
    varianceSum_ = cv::Mat::zeros(outputHeight_, outputWidth_, CV_64F);

    framesAdded_ = 0;
    initialized_ = true;
}

void DrizzleProcessor::reset() {
    outputSum_ = cv::Mat();
    weightSum_ = cv::Mat();
    varianceSum_ = cv::Mat();
    framesAdded_ = 0;
    initialized_ = false;
}

DrizzleResult DrizzleProcessor::drizzle(
    const std::vector<cv::Mat>& frames,
    const std::vector<FrameTransformation>& transforms,
    ProgressCallback progressCallback) {
    DrizzleResult result;

    if (frames.empty()) {
        return result;
    }

    auto startTime = std::chrono::high_resolution_clock::now();

    auto reportProgress = [&progressCallback](float p, const std::string& msg) {
        if (progressCallback)
            progressCallback(p, msg);
    };

    reportProgress(0.0f, "Initializing drizzle");

    // Initialize buffers based on first frame
    initialize(frames[0].cols, frames[0].rows);

    // Process each frame
    for (size_t i = 0; i < frames.size(); ++i) {
        float progress = static_cast<float>(i + 1) / frames.size();
        reportProgress(progress * 0.9f,
                       "Drizzling frame " + std::to_string(i + 1));

        double weight = 1.0;

        // Use provided transform or identity
        FrameTransformation transform;
        if (i < transforms.size()) {
            transform = transforms[i];
            if (params_.qualityWeighting) {
                weight = transform.confidence;
            }
        }

        addFrame(frames[i], transform, weight);
    }

    reportProgress(0.95f, "Finalizing output");
    result = finalize();

    auto endTime = std::chrono::high_resolution_clock::now();
    result.processingTimeMs =
        std::chrono::duration<double, std::milli>(endTime - startTime).count();

    reportProgress(1.0f, "Drizzle complete");

    return result;
}

DrizzleResult DrizzleProcessor::drizzleWithRegistration(
    const std::vector<cv::Mat>& frames, ProgressCallback progressCallback) {
    if (frames.empty()) {
        return DrizzleResult();
    }

    auto reportProgress = [&progressCallback](float p, const std::string& msg) {
        if (progressCallback)
            progressCallback(p, msg);
    };

    reportProgress(0.0f, "Registering frames");

    // Set reference frame
    registrar_->setReferenceFrame(frames[0]);

    // Calculate transformations
    std::vector<FrameTransformation> transforms;
    transforms.reserve(frames.size());

    // First frame is identity
    FrameTransformation identity;
    identity.type = FrameTransformation::Type::Translation;
    identity.transform = cv::Mat::eye(2, 3, CV_64F);
    identity.confidence = 1.0;
    transforms.push_back(identity);

    for (size_t i = 1; i < frames.size(); ++i) {
        float progress = static_cast<float>(i) / frames.size() * 0.3f;
        reportProgress(progress, "Registering frame " + std::to_string(i + 1));

        auto transform = registrar_->calculateTransformation(frames[i]);
        transforms.push_back(transform);
    }

    // Now drizzle with transforms
    auto adjustedCallback = [&progressCallback](float p,
                                                const std::string& msg) {
        if (progressCallback)
            progressCallback(0.3f + p * 0.7f, msg);
    };

    return drizzle(frames, transforms, adjustedCallback);
}

void DrizzleProcessor::addFrame(const cv::Mat& frame,
                                const FrameTransformation& transform) {
    addFrame(frame, transform, 1.0);
}

void DrizzleProcessor::addFrame(const cv::Mat& frame,
                                const FrameTransformation& transform,
                                double weight) {
    if (!initialized_) {
        initialize(frame.cols, frame.rows);
    }

    if (weight < params_.minWeight) {
        return;  // Skip very low weight frames
    }

    drizzleFrame(frame, transform, weight);
    ++framesAdded_;
}

DrizzleResult DrizzleProcessor::finalize() {
    DrizzleResult result;

    result.outputWidth = outputWidth_;
    result.outputHeight = outputHeight_;
    result.framesUsed = framesAdded_;
    result.effectiveScale = params_.scaleFactor;
    result.weightMap = weightSum_.clone();

    if (framesAdded_ == 0 || outputSum_.empty()) {
        return result;
    }

    result.image = normalizeOutput();

    // Reset for next use
    reset();

    return result;
}

cv::Mat DrizzleProcessor::process(const cv::Mat& frame) {
    // Single frame processing - just upscale
    cv::Mat result;
    cv::resize(frame, result, cv::Size(), params_.scaleFactor,
               params_.scaleFactor, cv::INTER_LANCZOS4);
    return result;
}

void DrizzleProcessor::setParameter(const std::string& name, double value) {
    if (name == "scaleFactor") {
        params_.scaleFactor = value;
    } else if (name == "dropSize") {
        params_.dropSize = std::clamp(value, 0.1, 1.0);
    } else if (name == "minWeight") {
        params_.minWeight = value;
    } else if (name == "badPixelThreshold") {
        params_.badPixelThreshold = value;
    }
}

double DrizzleProcessor::getParameter(const std::string& name) const {
    if (name == "scaleFactor")
        return params_.scaleFactor;
    if (name == "dropSize")
        return params_.dropSize;
    if (name == "minWeight")
        return params_.minWeight;
    if (name == "badPixelThreshold")
        return params_.badPixelThreshold;
    return 0.0;
}

std::vector<std::string> DrizzleProcessor::getParameterNames() const {
    return {"scaleFactor", "dropSize", "minWeight", "badPixelThreshold"};
}

bool DrizzleProcessor::hasParameter(const std::string& name) const {
    auto names = getParameterNames();
    return std::find(names.begin(), names.end(), name) != names.end();
}

void DrizzleProcessor::setDrizzleParameters(const DrizzleParameters& params) {
    params_ = params;
}

void DrizzleProcessor::setRegistrar(std::shared_ptr<FrameRegistrar> registrar) {
    registrar_ = registrar;
}

cv::Mat DrizzleProcessor::getCoverageMap() const {
    if (weightSum_.empty()) {
        return cv::Mat();
    }

    cv::Mat coverage;
    double maxWeight;
    cv::minMaxLoc(weightSum_, nullptr, &maxWeight);

    if (maxWeight > 0) {
        coverage = weightSum_ / maxWeight;
    } else {
        coverage = cv::Mat::zeros(weightSum_.size(), CV_64F);
    }

    return coverage;
}

void DrizzleProcessor::drizzleFrame(const cv::Mat& frame,
                                    const FrameTransformation& transform,
                                    double weight) {
    // Convert frame to floating point
    cv::Mat floatFrame;
    if (frame.channels() == 1) {
        cv::cvtColor(frame, floatFrame, cv::COLOR_GRAY2BGR);
    } else {
        frame.copyTo(floatFrame);
    }
    floatFrame.convertTo(floatFrame, CV_64FC3);

    // Create bad pixel mask if needed
    cv::Mat badMask;
    if (params_.maskBadPixels) {
        badMask = createBadPixelMask(frame);
    }

    double scale = params_.scaleFactor;
    double dropSize = params_.dropSize;
    double dropRadius = dropSize / 2.0;

    // For each input pixel
    for (int y = 0; y < inputHeight_; ++y) {
        for (int x = 0; x < inputWidth_; ++x) {
            // Skip bad pixels
            if (!badMask.empty() && badMask.at<uint8_t>(y, x) > 0) {
                continue;
            }

            // Get pixel value
            cv::Vec3d pixelValue = floatFrame.at<cv::Vec3d>(y, x);

            // Calculate transformed position
            double srcX = x + 0.5;  // Center of pixel
            double srcY = y + 0.5;

            // Apply transformation
            double dstX, dstY;
            if (!transform.transform.empty()) {
                cv::Mat pt = (cv::Mat_<double>(3, 1) << srcX, srcY, 1.0);
                cv::Mat transformed;
                if (transform.transform.rows == 2) {
                    transformed = transform.transform * pt;
                } else if (transform.transform.rows == 3) {
                    transformed = transform.transform * pt;
                    transformed /= transformed.at<double>(2, 0);
                }
                dstX = transformed.at<double>(0, 0);
                dstY = transformed.at<double>(1, 0);
            } else {
                dstX = srcX;
                dstY = srcY;
            }

            // Scale to output coordinates
            dstX *= scale;
            dstY *= scale;

            // Calculate drop footprint in output grid
            int outX0 = static_cast<int>(std::floor(dstX - dropRadius * scale));
            int outX1 = static_cast<int>(std::ceil(dstX + dropRadius * scale));
            int outY0 = static_cast<int>(std::floor(dstY - dropRadius * scale));
            int outY1 = static_cast<int>(std::ceil(dstY + dropRadius * scale));

            // Clamp to output bounds
            outX0 = std::max(0, outX0);
            outX1 = std::min(outputWidth_ - 1, outX1);
            outY0 = std::max(0, outY0);
            outY1 = std::min(outputHeight_ - 1, outY1);

            // Distribute pixel value to output grid
            for (int oy = outY0; oy <= outY1; ++oy) {
                for (int ox = outX0; ox <= outX1; ++ox) {
                    // Calculate distance from drop center
                    double dx = (ox + 0.5 - dstX) / scale;
                    double dy = (oy + 0.5 - dstY) / scale;

                    // Calculate kernel weight
                    double kernelWeight = calculateKernelWeight(dx, dy);

                    if (kernelWeight > 0) {
                        double totalWeight = weight * kernelWeight;

                        outputSum_.at<cv::Vec3d>(oy, ox) +=
                            pixelValue * totalWeight;
                        weightSum_.at<double>(oy, ox) += totalWeight;
                    }
                }
            }
        }
    }
}

double DrizzleProcessor::calculateKernelWeight(double dx, double dy) const {
    double distance = std::sqrt(dx * dx + dy * dy);
    double dropRadius = params_.dropSize / 2.0;

    switch (params_.kernel) {
        case DrizzleKernel::Point:
            return (distance < 0.5) ? 1.0 : 0.0;

        case DrizzleKernel::Square:
            return (std::abs(dx) <= dropRadius && std::abs(dy) <= dropRadius)
                       ? 1.0
                       : 0.0;

        case DrizzleKernel::Gaussian: {
            double sigma = dropRadius / 2.0;
            return std::exp(-(dx * dx + dy * dy) / (2 * sigma * sigma));
        }

        case DrizzleKernel::Lanczos: {
            if (distance >= dropRadius)
                return 0.0;
            if (distance < 1e-10)
                return 1.0;

            double x = distance / dropRadius * M_PI;
            double sinc = std::sin(x) / x;
            double lanczos = std::sin(x / 3) / (x / 3);
            return sinc * lanczos;
        }

        case DrizzleKernel::Turbo:
        default:
            // Linear falloff
            return std::max(0.0, 1.0 - distance / dropRadius);
    }
}

cv::Mat DrizzleProcessor::createBadPixelMask(const cv::Mat& frame) const {
    cv::Mat gray;
    if (frame.channels() > 1) {
        cv::cvtColor(frame, gray, cv::COLOR_BGR2GRAY);
    } else {
        gray = frame;
    }

    cv::Mat floatGray;
    gray.convertTo(floatGray, CV_64F);

    // Calculate local statistics
    cv::Mat mean, stdDev;
    cv::blur(floatGray, mean, cv::Size(5, 5));

    cv::Mat diff;
    cv::absdiff(floatGray, mean, diff);
    cv::blur(diff.mul(diff), stdDev, cv::Size(5, 5));
    cv::sqrt(stdDev, stdDev);

    // Create mask
    cv::Mat mask = cv::Mat::zeros(frame.size(), CV_8U);
    double threshold = params_.badPixelThreshold;

    for (int y = 0; y < frame.rows; ++y) {
        for (int x = 0; x < frame.cols; ++x) {
            double deviation = diff.at<double>(y, x);
            double localStd = stdDev.at<double>(y, x);

            if (localStd > 0 && deviation > threshold * localStd) {
                mask.at<uint8_t>(y, x) = 255;
            }
        }
    }

    return mask;
}

cv::Mat DrizzleProcessor::normalizeOutput() const {
    cv::Mat result = cv::Mat::zeros(outputHeight_, outputWidth_, CV_64FC3);

    for (int y = 0; y < outputHeight_; ++y) {
        for (int x = 0; x < outputWidth_; ++x) {
            double w = weightSum_.at<double>(y, x);

            if (w > params_.minWeight) {
                result.at<cv::Vec3d>(y, x) = outputSum_.at<cv::Vec3d>(y, x) / w;
            }
        }
    }

    // Convert to appropriate output format
    cv::Mat output;
    if (params_.preserveBitDepth) {
        result.convertTo(output, CV_16UC3);
    } else {
        if (params_.normalizeOutput) {
            cv::normalize(result, result, 0, 255, cv::NORM_MINMAX);
        }
        result.convertTo(output, CV_8UC3);
    }

    return output;
}

// Utility functions

cv::Mat quickDrizzle(const std::vector<cv::Mat>& frames, double scale,
                     double dropSize) {
    DrizzleParameters params;
    params.scaleFactor = scale;
    params.dropSize = dropSize;

    DrizzleProcessor processor(params);
    auto result = processor.drizzleWithRegistration(frames, nullptr);

    return result.image;
}

std::string drizzleKernelToString(DrizzleKernel kernel) {
    switch (kernel) {
        case DrizzleKernel::Point:
            return "Point";
        case DrizzleKernel::Square:
            return "Square";
        case DrizzleKernel::Gaussian:
            return "Gaussian";
        case DrizzleKernel::Lanczos:
            return "Lanczos";
        case DrizzleKernel::Turbo:
            return "Turbo";
    }
    return "Unknown";
}

DrizzleKernel drizzleKernelFromString(const std::string& name) {
    if (name == "Point")
        return DrizzleKernel::Point;
    if (name == "Square")
        return DrizzleKernel::Square;
    if (name == "Gaussian")
        return DrizzleKernel::Gaussian;
    if (name == "Lanczos")
        return DrizzleKernel::Lanczos;
    if (name == "Turbo")
        return DrizzleKernel::Turbo;
    return DrizzleKernel::Square;
}

}  // namespace serastro
