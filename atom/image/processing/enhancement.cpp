#include "enhancement.hpp"
#include <algorithm>
#include <cmath>
#include <numeric>
#include <execution>
#include <stdexcept>

// Define error macros to avoid atom error system namespace pollution
#define THROW_RUNTIME_ERROR(msg) throw std::runtime_error(msg)
#define THROW_INVALID_ARGUMENT(msg) throw std::invalid_argument(msg)

#ifdef ATOM_IMAGE_HAS_OPENCV
#include <opencv2/opencv.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/photo.hpp>
#endif

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace atom::image {

blob ImageEnhancement::equalizeHistogram(const blob& input,
                                        HistogramMethod method,
                                        const EnhancementParams& params) const {
    if (input.isEmpty()) {
        return blob{};
    }

#ifdef ATOM_IMAGE_HAS_OPENCV
    cv::Mat src = input.to_mat();
    cv::Mat dst;

    switch (method) {
        case HistogramMethod::GLOBAL: {
            if (src.channels() == 1) {
                cv::equalizeHist(src, dst);
            } else {
                // Convert to YUV and equalize Y channel
                cv::Mat yuv;
                cv::cvtColor(src, yuv, cv::COLOR_BGR2YUV);
                std::vector<cv::Mat> channels;
                cv::split(yuv, channels);
                cv::equalizeHist(channels[0], channels[0]);
                cv::merge(channels, yuv);
                cv::cvtColor(yuv, dst, cv::COLOR_YUV2BGR);
            }
            break;
        }
        case HistogramMethod::CLAHE: {
            auto clahe = cv::createCLAHE(params.clipLimit, cv::Size(params.tileGridSize, params.tileGridSize));
            if (src.channels() == 1) {
                clahe->apply(src, dst);
            } else {
                cv::Mat lab;
                cv::cvtColor(src, lab, cv::COLOR_BGR2Lab);
                std::vector<cv::Mat> channels;
                cv::split(lab, channels);
                clahe->apply(channels[0], channels[0]);
                cv::merge(channels, lab);
                cv::cvtColor(lab, dst, cv::COLOR_Lab2BGR);
            }
            break;
        }
        case HistogramMethod::ADAPTIVE: {
            // Adaptive histogram equalization using local windows
            dst = src.clone();
            int windowSize = params.tileGridSize * 8;
            for (int y = 0; y < src.rows; y += windowSize) {
                for (int x = 0; x < src.cols; x += windowSize) {
                    int endY = std::min(y + windowSize, src.rows);
                    int endX = std::min(x + windowSize, src.cols);
                    cv::Rect roi(x, y, endX - x, endY - y);
                    cv::Mat window = dst(roi);
                    if (window.channels() == 1) {
                        cv::equalizeHist(window, window);
                    }
                }
            }
            break;
        }
        default:
            THROW_RUNTIME_ERROR("Unsupported histogram equalization method");
    }

    return blob(dst);
#else
    THROW_RUNTIME_ERROR("OpenCV required for histogram equalization");
#endif
}

blob ImageEnhancement::adjustBrightnessContrast(const blob& input,
                                               double brightness,
                                               double contrast,
                                               bool preserveDetails) const {
    if (input.isEmpty()) {
        return blob{};
    }

#ifdef ATOM_IMAGE_HAS_OPENCV
    cv::Mat src = input.to_mat();
    cv::Mat dst;
    
    // Convert brightness from [-100, 100] to additive value
    double addValue = brightness * 2.55; // Scale to [0, 255] range
    src.convertTo(dst, -1, 1.0, addValue);
    
    return blob(dst);
#else
    // Manual implementation for brightness adjustment
    blob result = input.clone();
    int brightnessValue = static_cast<int>(brightness * 2.55);
    
    for (size_t i = 0; i < result.size(); ++i) {
        int pixel = static_cast<int>(result[i]) + brightnessValue;
        result[i] = static_cast<std::byte>(std::clamp(pixel, 0, 255));
    }
    
    return result;
#endif
}



blob ImageEnhancement::gammaCorrection(const blob& input,
                                      double gamma,
                                      ColorSpace colorSpace) const {
    if (input.isEmpty()) {
        return blob{};
    }

    // Create gamma correction lookup table
    std::array<uint8_t, 256> lookupTable;
    double invGamma = 1.0 / gamma;
    
    for (int i = 0; i < 256; ++i) {
        lookupTable[i] = static_cast<uint8_t>(std::pow(i / 255.0, invGamma) * 255.0);
    }

#ifdef ATOM_IMAGE_HAS_OPENCV
    cv::Mat src = input.to_mat();
    cv::Mat dst;
    cv::Mat lut(1, 256, CV_8U, lookupTable.data());
    cv::LUT(src, lut, dst);
    
    return blob(dst);
#else
    // Manual gamma correction
    blob result = input.clone();
    
    for (size_t i = 0; i < result.size(); ++i) {
        uint8_t pixel = static_cast<uint8_t>(result[i]);
        result[i] = static_cast<std::byte>(lookupTable[pixel]);
    }
    
    return result;
#endif
}

blob ImageEnhancement::vibranceSaturation(const blob& input,
                                         double vibrance,
                                         double saturation) const {
    if (input.isEmpty()) {
        return blob{};
    }

#ifdef ATOM_IMAGE_HAS_OPENCV
    cv::Mat src = input.to_mat();
    cv::Mat dst;
    
    if (src.channels() >= 3) {
        cv::Mat hsv;
        cv::cvtColor(src, hsv, cv::COLOR_BGR2HSV);
        
        std::vector<cv::Mat> channels;
        cv::split(hsv, channels);
        
        // Adjust saturation channel
        double factor = (saturation + 100.0) / 100.0;
        channels[1].convertTo(channels[1], -1, factor, 0);
        
        cv::merge(channels, hsv);
        cv::cvtColor(hsv, dst, cv::COLOR_HSV2BGR);
    } else {
        dst = src.clone(); // No saturation adjustment for grayscale
    }
    
    return blob(dst);
#else
    THROW_RUNTIME_ERROR("OpenCV required for saturation adjustment");
#endif
}



blob ImageEnhancement::toneMapping(const blob& input,
                                  ToneMappingOperator op,
                                  const EnhancementParams& params) const {
    if (input.isEmpty()) {
        return blob{};
    }

#ifdef ATOM_IMAGE_HAS_OPENCV
    cv::Mat src = input.to_mat();
    cv::Mat dst;
    
    // Convert to float for tone mapping
    cv::Mat floatSrc;
    src.convertTo(floatSrc, CV_32F, 1.0/255.0);
    
    switch (op) {
        case ToneMappingOperator::REINHARD: {
            auto tonemap = cv::createTonemapReinhard(params.gamma, params.intensity, 
                                                    params.lightAdaptation, params.colorAdaptation);
            tonemap->process(floatSrc, dst);
            break;
        }
        case ToneMappingOperator::DRAGO: {
            auto tonemap = cv::createTonemapDrago(params.gamma, params.saturation);
            tonemap->process(floatSrc, dst);
            break;
        }
        case ToneMappingOperator::MANTIUK: {
            auto tonemap = cv::createTonemapMantiuk(params.gamma, params.saturation);
            tonemap->process(floatSrc, dst);
            break;
        }
        default:
            THROW_RUNTIME_ERROR("Unsupported tone mapping operator");
    }
    
    // Convert back to 8-bit
    dst.convertTo(dst, CV_8U, 255.0);
    
    return blob(dst);
#else
    THROW_RUNTIME_ERROR("OpenCV required for tone mapping");
#endif
}



blob ImageEnhancement::colorCorrection(const blob& input,
                                      ColorCorrectionMethod method,
                                      const EnhancementParams& params) const {
    if (input.isEmpty()) {
        return blob{};
    }

#ifdef ATOM_IMAGE_HAS_OPENCV
    cv::Mat src = input.to_mat();
    cv::Mat dst;
    
    // Create color correction matrix
    cv::Mat matrix(3, 3, CV_64F, const_cast<double*>(colorMatrix.data()));
    
    // Apply color correction
    cv::transform(src, dst, matrix);
    
    return blob(dst);
#else
    THROW_RUNTIME_ERROR("OpenCV required for color correction");
#endif
}

blob ImageEnhancement::autoEnhance(const blob& input,
                                  const std::string& mode,
                                  double strength) const {
    if (input.isEmpty()) {
        return blob{};
    }

    blob result = input;
    
    // Apply different enhancement strategies based on mode
    if (mode == "auto" || mode == "general") {
        // General purpose enhancement
        result = adjustBrightnessContrast(result, 5.0 * strength, 1.0 + 0.1 * strength);
        result = vibranceSaturation(result, 10.0 * strength, 15.0 * strength);
    } else if (mode == "portrait") {
        // Portrait-specific enhancements
        result = adjustBrightnessContrast(result, 8.0 * strength, 1.0 + 0.05 * strength);
        result = vibranceSaturation(result, 5.0 * strength, 10.0 * strength);
    } else if (mode == "landscape") {
        // Landscape-specific enhancements
        result = adjustBrightnessContrast(result, 3.0 * strength, 1.0 + 0.15 * strength);
        result = vibranceSaturation(result, 15.0 * strength, 20.0 * strength);
    } else if (mode == "night") {
        // Night photography enhancements
        result = adjustBrightnessContrast(result, 20.0 * strength, 1.0 + 0.25 * strength);
        result = gammaCorrection(result, 0.8);
    }
    
    return result;
}

blob ImageEnhancement::convertColorSpace(const blob& input,
                                        ColorSpace fromSpace,
                                        ColorSpace toSpace) const {
    if (input.isEmpty() || fromSpace == toSpace) {
        return input;
    }

#ifdef ATOM_IMAGE_HAS_OPENCV
    cv::Mat src = input.to_mat();
    cv::Mat dst;
    
    // Determine OpenCV color conversion code
    int conversionCode = -1;
    
    if (fromSpace == ColorSpace::RGB && toSpace == ColorSpace::HSV) {
        conversionCode = cv::COLOR_RGB2HSV;
    } else if (fromSpace == ColorSpace::HSV && toSpace == ColorSpace::RGB) {
        conversionCode = cv::COLOR_HSV2RGB;
    } else if (fromSpace == ColorSpace::RGB && toSpace == ColorSpace::LAB) {
        conversionCode = cv::COLOR_RGB2Lab;
    } else if (fromSpace == ColorSpace::LAB && toSpace == ColorSpace::RGB) {
        conversionCode = cv::COLOR_Lab2RGB;
    } else if (fromSpace == ColorSpace::RGB && toSpace == ColorSpace::GRAY) {
        conversionCode = cv::COLOR_RGB2GRAY;
    } else if (fromSpace == ColorSpace::GRAY && toSpace == ColorSpace::RGB) {
        conversionCode = cv::COLOR_GRAY2RGB;
    }
    
    if (conversionCode != -1) {
        cv::cvtColor(src, dst, conversionCode);
        return blob(dst);
    } else {
        THROW_RUNTIME_ERROR("Unsupported color space conversion");
    }
#else
    THROW_RUNTIME_ERROR("OpenCV required for color space conversion");
#endif
}

}  // namespace atom::image
