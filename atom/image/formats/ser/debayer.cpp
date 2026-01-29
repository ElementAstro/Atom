// debayer.cpp
#include "debayer.h"

#include <algorithm>
#include <cmath>
#include <opencv2/imgproc.hpp>

namespace serastro {

DebayerProcessor::DebayerProcessor() = default;

DebayerProcessor::DebayerProcessor(const DebayerParameters& params)
    : params_(params) {}

DebayerProcessor::DebayerProcessor(SERColorID pattern,
                                   DebayerAlgorithm algorithm) {
    params_.bayerPattern = pattern;
    params_.algorithm = algorithm;
}

cv::Mat DebayerProcessor::process(const cv::Mat& frame) {
    if (frame.empty()) {
        return frame;
    }

    // Check if input is actually a Bayer pattern
    if (!isBayerPattern(params_.bayerPattern)) {
        // Not a Bayer pattern, return as-is or convert
        if (params_.bayerPattern == SERColorID::RGB && params_.outputBGR) {
            cv::Mat bgr;
            cv::cvtColor(frame, bgr, cv::COLOR_RGB2BGR);
            return bgr;
        }
        if (params_.bayerPattern == SERColorID::BGR && !params_.outputBGR) {
            cv::Mat rgb;
            cv::cvtColor(frame, rgb, cv::COLOR_BGR2RGB);
            return rgb;
        }
        return frame.clone();
    }

    cv::Mat debayered;

    // Select debayering algorithm
    switch (params_.algorithm) {
        case DebayerAlgorithm::Nearest:
        case DebayerAlgorithm::Bilinear:
        case DebayerAlgorithm::EdgeAware:
            debayered = debayerOpenCV(frame);
            break;

        case DebayerAlgorithm::VNG:
            debayered = debayerVNG(frame);
            break;

        case DebayerAlgorithm::AHD:
            debayered = debayerAHD(frame);
            break;

        case DebayerAlgorithm::SuperPixel:
            debayered = debayerSuperPixel(frame);
            break;

        case DebayerAlgorithm::HalfSize:
            debayered = debayerHalfSize(frame);
            break;

        default:
            debayered = debayerOpenCV(frame);
            break;
    }

    // Apply noise reduction if requested
    if (params_.applyNoiseReduction) {
        debayered = applyNoiseReduction(debayered);
    }

    // Apply white balance
    if (params_.autoWhiteBalance) {
        auto coeffs = calculateAutoWhiteBalance(debayered);
        debayered = applyWhiteBalance(debayered, coeffs);
    } else if (params_.applyColorCorrection) {
        WhiteBalanceCoeffs coeffs(params_.redMultiplier,
                                  params_.greenMultiplier,
                                  params_.blueMultiplier);
        debayered = applyWhiteBalance(debayered, coeffs);
    }

    // Apply color matrix if requested
    if (params_.applyColorCorrection) {
        debayered = applyColorMatrix(debayered);
    }

    // Convert bit depth if needed
    if (params_.output16bit && debayered.depth() == CV_8U) {
        cv::Mat output16;
        debayered.convertTo(output16, CV_16U, 256.0);
        return output16;
    } else if (!params_.output16bit && debayered.depth() == CV_16U) {
        cv::Mat output8;
        debayered.convertTo(output8, CV_8U, 1.0 / 256.0);
        return output8;
    }

    return debayered;
}

void DebayerProcessor::setParameter(const std::string& name, double value) {
    if (name == "algorithm") {
        params_.algorithm =
            static_cast<DebayerAlgorithm>(static_cast<int>(value));
    } else if (name == "redMultiplier") {
        params_.redMultiplier = value;
    } else if (name == "greenMultiplier") {
        params_.greenMultiplier = value;
    } else if (name == "blueMultiplier") {
        params_.blueMultiplier = value;
    } else if (name == "colorTemperature") {
        params_.colorTemperature = value;
    } else if (name == "noiseThreshold") {
        params_.noiseThreshold = value;
    } else if (name == "autoWhiteBalance") {
        params_.autoWhiteBalance = value != 0.0;
    } else if (name == "applyNoiseReduction") {
        params_.applyNoiseReduction = value != 0.0;
    }
}

double DebayerProcessor::getParameter(const std::string& name) const {
    if (name == "algorithm") {
        return static_cast<double>(params_.algorithm);
    } else if (name == "redMultiplier") {
        return params_.redMultiplier;
    } else if (name == "greenMultiplier") {
        return params_.greenMultiplier;
    } else if (name == "blueMultiplier") {
        return params_.blueMultiplier;
    } else if (name == "colorTemperature") {
        return params_.colorTemperature;
    } else if (name == "noiseThreshold") {
        return params_.noiseThreshold;
    } else if (name == "autoWhiteBalance") {
        return params_.autoWhiteBalance ? 1.0 : 0.0;
    } else if (name == "applyNoiseReduction") {
        return params_.applyNoiseReduction ? 1.0 : 0.0;
    }
    return 0.0;
}

std::vector<std::string> DebayerProcessor::getParameterNames() const {
    return {"algorithm",        "redMultiplier",      "greenMultiplier",
            "blueMultiplier",   "colorTemperature",   "noiseThreshold",
            "autoWhiteBalance", "applyNoiseReduction"};
}

bool DebayerProcessor::hasParameter(const std::string& name) const {
    auto names = getParameterNames();
    return std::find(names.begin(), names.end(), name) != names.end();
}

void DebayerProcessor::setDebayerParameters(const DebayerParameters& params) {
    params_ = params;
}

void DebayerProcessor::setBayerPattern(SERColorID pattern) {
    params_.bayerPattern = pattern;
}

void DebayerProcessor::setAlgorithm(DebayerAlgorithm algorithm) {
    params_.algorithm = algorithm;
}

WhiteBalanceCoeffs DebayerProcessor::calculateAutoWhiteBalance(
    const cv::Mat& frame) const {
    if (frame.channels() != 3) {
        return WhiteBalanceCoeffs();
    }

    // Gray World assumption
    cv::Scalar mean = cv::mean(frame);

    double avgGray = (mean[0] + mean[1] + mean[2]) / 3.0;

    WhiteBalanceCoeffs coeffs;
    coeffs.b = avgGray / (mean[0] + 1e-10);
    coeffs.g = avgGray / (mean[1] + 1e-10);
    coeffs.r = avgGray / (mean[2] + 1e-10);

    // Normalize
    double maxCoeff = std::max({coeffs.r, coeffs.g, coeffs.b});
    coeffs.r /= maxCoeff;
    coeffs.g /= maxCoeff;
    coeffs.b /= maxCoeff;

    return coeffs;
}

cv::Mat DebayerProcessor::applyWhiteBalance(
    const cv::Mat& frame, const WhiteBalanceCoeffs& coeffs) const {
    if (frame.channels() != 3) {
        return frame.clone();
    }

    cv::Mat balanced;
    frame.copyTo(balanced);

    std::vector<cv::Mat> channels;
    cv::split(balanced, channels);

    // Apply multipliers (BGR order)
    channels[0] *= coeffs.b;
    channels[1] *= coeffs.g;
    channels[2] *= coeffs.r;

    cv::merge(channels, balanced);

    return balanced;
}

WhiteBalanceCoeffs DebayerProcessor::getWhiteBalanceForTemperature(
    double kelvin) {
    // Approximate white balance from color temperature
    // Based on Planckian locus approximation

    WhiteBalanceCoeffs coeffs;

    if (kelvin < 4000) {
        // Warm (tungsten-like)
        coeffs.r = 1.0;
        coeffs.g = 0.8 + 0.2 * (kelvin - 2000) / 2000;
        coeffs.b = 0.5 + 0.3 * (kelvin - 2000) / 2000;
    } else if (kelvin < 6500) {
        // Neutral to daylight
        double t = (kelvin - 4000) / 2500;
        coeffs.r = 1.0 - 0.1 * t;
        coeffs.g = 1.0;
        coeffs.b = 0.8 + 0.2 * t;
    } else {
        // Cool (shade/overcast)
        double t = std::min((kelvin - 6500) / 3500, 1.0);
        coeffs.r = 0.9 - 0.1 * t;
        coeffs.g = 0.95;
        coeffs.b = 1.0;
    }

    // Normalize
    double maxCoeff = std::max({coeffs.r, coeffs.g, coeffs.b});
    coeffs.r /= maxCoeff;
    coeffs.g /= maxCoeff;
    coeffs.b /= maxCoeff;

    return coeffs;
}

bool DebayerProcessor::isBayerPattern(SERColorID colorID) {
    return colorID >= SERColorID::BayerRGGB && colorID <= SERColorID::BayerBGGR;
}

int DebayerProcessor::getOpenCVBayerCode(SERColorID pattern) {
    switch (pattern) {
        case SERColorID::BayerRGGB:
            return cv::COLOR_BayerRG2BGR;
        case SERColorID::BayerGRBG:
            return cv::COLOR_BayerGR2BGR;
        case SERColorID::BayerGBRG:
            return cv::COLOR_BayerGB2BGR;
        case SERColorID::BayerBGGR:
            return cv::COLOR_BayerBG2BGR;
        default:
            return cv::COLOR_BayerRG2BGR;
    }
}

cv::Mat DebayerProcessor::debayerOpenCV(const cv::Mat& frame) const {
    cv::Mat result;

    int code = getOpenCVBayerCode(params_.bayerPattern);

    // Adjust for algorithm
    switch (params_.algorithm) {
        case DebayerAlgorithm::Nearest:
            // OpenCV doesn't have nearest, use bilinear
            cv::cvtColor(frame, result, code);
            break;

        case DebayerAlgorithm::EdgeAware:
            // Use VNG for edge-aware in OpenCV
            code = code + (cv::COLOR_BayerRG2BGR_VNG - cv::COLOR_BayerRG2BGR);
            cv::cvtColor(frame, result, code);
            break;

        case DebayerAlgorithm::Bilinear:
        default:
            cv::cvtColor(frame, result, code);
            break;
    }

    // Convert RGB/BGR if needed
    if (!params_.outputBGR) {
        cv::cvtColor(result, result, cv::COLOR_BGR2RGB);
    }

    return result;
}

cv::Mat DebayerProcessor::debayerVNG(const cv::Mat& frame) const {
    cv::Mat result;

    int baseCode = getOpenCVBayerCode(params_.bayerPattern);
    int vngCode =
        baseCode + (cv::COLOR_BayerRG2BGR_VNG - cv::COLOR_BayerRG2BGR);

    cv::cvtColor(frame, result, vngCode);

    if (!params_.outputBGR) {
        cv::cvtColor(result, result, cv::COLOR_BGR2RGB);
    }

    return result;
}

cv::Mat DebayerProcessor::debayerAHD(const cv::Mat& frame) const {
    // AHD is not directly available in OpenCV, use EA as approximation
    cv::Mat result;

    int baseCode = getOpenCVBayerCode(params_.bayerPattern);
    int eaCode = baseCode + (cv::COLOR_BayerRG2BGR_EA - cv::COLOR_BayerRG2BGR);

    cv::cvtColor(frame, result, eaCode);

    if (!params_.outputBGR) {
        cv::cvtColor(result, result, cv::COLOR_BGR2RGB);
    }

    return result;
}

cv::Mat DebayerProcessor::debayerSuperPixel(const cv::Mat& frame) const {
    // SuperPixel: 2x2 pixel binning for each color
    // Results in half-resolution color image

    int rows = frame.rows / 2;
    int cols = frame.cols / 2;

    cv::Mat result(rows, cols, CV_MAKETYPE(frame.depth(), 3));

    // Pattern determines which pixel is which color
    int rRow = 0, rCol = 0;
    int gRow1 = 0, gCol1 = 1;
    int gRow2 = 1, gCol2 = 0;
    int bRow = 1, bCol = 1;

    switch (params_.bayerPattern) {
        case SERColorID::BayerRGGB:
            rRow = 0;
            rCol = 0;
            bRow = 1;
            bCol = 1;
            break;
        case SERColorID::BayerGRBG:
            rRow = 0;
            rCol = 1;
            bRow = 1;
            bCol = 0;
            gRow1 = 0;
            gCol1 = 0;
            gRow2 = 1;
            gCol2 = 1;
            break;
        case SERColorID::BayerGBRG:
            rRow = 1;
            rCol = 0;
            bRow = 0;
            bCol = 1;
            gRow1 = 0;
            gCol1 = 0;
            gRow2 = 1;
            gCol2 = 1;
            break;
        case SERColorID::BayerBGGR:
            rRow = 1;
            rCol = 1;
            bRow = 0;
            bCol = 0;
            break;
        default:
            break;
    }

    for (int y = 0; y < rows; ++y) {
        for (int x = 0; x < cols; ++x) {
            int srcY = y * 2;
            int srcX = x * 2;

            double r, g, b;

            if (frame.depth() == CV_8U) {
                r = frame.at<uint8_t>(srcY + rRow, srcX + rCol);
                g = (frame.at<uint8_t>(srcY + gRow1, srcX + gCol1) +
                     frame.at<uint8_t>(srcY + gRow2, srcX + gCol2)) /
                    2.0;
                b = frame.at<uint8_t>(srcY + bRow, srcX + bCol);

                if (params_.outputBGR) {
                    result.at<cv::Vec3b>(y, x) = cv::Vec3b(
                        static_cast<uint8_t>(b), static_cast<uint8_t>(g),
                        static_cast<uint8_t>(r));
                } else {
                    result.at<cv::Vec3b>(y, x) = cv::Vec3b(
                        static_cast<uint8_t>(r), static_cast<uint8_t>(g),
                        static_cast<uint8_t>(b));
                }
            } else if (frame.depth() == CV_16U) {
                r = frame.at<uint16_t>(srcY + rRow, srcX + rCol);
                g = (frame.at<uint16_t>(srcY + gRow1, srcX + gCol1) +
                     frame.at<uint16_t>(srcY + gRow2, srcX + gCol2)) /
                    2.0;
                b = frame.at<uint16_t>(srcY + bRow, srcX + bCol);

                if (params_.outputBGR) {
                    result.at<cv::Vec3w>(y, x) = cv::Vec3w(
                        static_cast<uint16_t>(b), static_cast<uint16_t>(g),
                        static_cast<uint16_t>(r));
                } else {
                    result.at<cv::Vec3w>(y, x) = cv::Vec3w(
                        static_cast<uint16_t>(r), static_cast<uint16_t>(g),
                        static_cast<uint16_t>(b));
                }
            }
        }
    }

    return result;
}

cv::Mat DebayerProcessor::debayerHalfSize(const cv::Mat& frame) const {
    // Same as SuperPixel but simpler implementation
    return debayerSuperPixel(frame);
}

cv::Mat DebayerProcessor::applyColorMatrix(const cv::Mat& frame) const {
    // Simple color correction matrix
    // Identity matrix with user multipliers applied
    cv::Mat result;
    frame.copyTo(result);

    // Color matrix is already applied via white balance
    // This could be extended with a full 3x3 color correction matrix

    return result;
}

cv::Mat DebayerProcessor::applyNoiseReduction(const cv::Mat& frame) const {
    cv::Mat result;

    // Use bilateral filter for edge-preserving noise reduction
    int d = 5;
    double sigmaColor = params_.noiseThreshold * 255;
    double sigmaSpace = params_.noiseThreshold * 50;

    cv::bilateralFilter(frame, result, d, sigmaColor, sigmaSpace);

    return result;
}

// Utility functions

std::string debayerAlgorithmToString(DebayerAlgorithm algorithm) {
    switch (algorithm) {
        case DebayerAlgorithm::Nearest:
            return "Nearest";
        case DebayerAlgorithm::Bilinear:
            return "Bilinear";
        case DebayerAlgorithm::VNG:
            return "VNG";
        case DebayerAlgorithm::EdgeAware:
            return "EdgeAware";
        case DebayerAlgorithm::AHD:
            return "AHD";
        case DebayerAlgorithm::DCB:
            return "DCB";
        case DebayerAlgorithm::AMAZE:
            return "AMaZE";
        case DebayerAlgorithm::SuperPixel:
            return "SuperPixel";
        case DebayerAlgorithm::HalfSize:
            return "HalfSize";
        case DebayerAlgorithm::IGV:
            return "IGV";
        case DebayerAlgorithm::LMMSE:
            return "LMMSE";
    }
    return "Unknown";
}

DebayerAlgorithm debayerAlgorithmFromString(const std::string& name) {
    if (name == "Nearest")
        return DebayerAlgorithm::Nearest;
    if (name == "Bilinear")
        return DebayerAlgorithm::Bilinear;
    if (name == "VNG")
        return DebayerAlgorithm::VNG;
    if (name == "EdgeAware" || name == "EA")
        return DebayerAlgorithm::EdgeAware;
    if (name == "AHD")
        return DebayerAlgorithm::AHD;
    if (name == "DCB")
        return DebayerAlgorithm::DCB;
    if (name == "AMaZE" || name == "AMAZE")
        return DebayerAlgorithm::AMAZE;
    if (name == "SuperPixel")
        return DebayerAlgorithm::SuperPixel;
    if (name == "HalfSize")
        return DebayerAlgorithm::HalfSize;
    if (name == "IGV")
        return DebayerAlgorithm::IGV;
    if (name == "LMMSE")
        return DebayerAlgorithm::LMMSE;
    return DebayerAlgorithm::Bilinear;
}

std::string bayerPatternToString(SERColorID pattern) {
    switch (pattern) {
        case SERColorID::BayerRGGB:
            return "RGGB";
        case SERColorID::BayerGRBG:
            return "GRBG";
        case SERColorID::BayerGBRG:
            return "GBRG";
        case SERColorID::BayerBGGR:
            return "BGGR";
        case SERColorID::Mono:
            return "Mono";
        case SERColorID::RGB:
            return "RGB";
        case SERColorID::BGR:
            return "BGR";
    }
    return "Unknown";
}

SERColorID detectBayerPattern(const cv::Mat& image) {
    // Simple heuristic based on corner pixel analysis
    // Real implementation would use more sophisticated detection

    if (image.channels() != 1) {
        return SERColorID::Mono;  // Already color
    }

    // Check variance of different assumed patterns
    // This is a placeholder - real detection is complex

    return SERColorID::BayerRGGB;  // Default assumption
}

}  // namespace serastro
