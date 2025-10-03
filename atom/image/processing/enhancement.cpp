#include "enhancement.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <execution>
#include <numeric>
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
    (void)method;
    (void)params;

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
    (void)preserveDetails;

#ifdef ATOM_IMAGE_HAS_OPENCV
    cv::Mat src = input.to_mat();
    cv::Mat dst;
    
    // Convert brightness from [-100, 100] to additive value
    double addValue = brightness * 2.55; // Scale to [0, 255] range
    src.convertTo(dst, -1, contrast, addValue);
    
    return blob(dst);
#else
    // Manual implementation for brightness adjustment
    blob result = input.clone();
    int brightnessValue = static_cast<int>(brightness * 2.55);
    
    for (size_t i = 0; i < result.size(); ++i) {
        int pixel = std::min(255, std::max(0, static_cast<int>(result[i]) * static_cast<int>(contrast) + brightnessValue));
        result[i] = static_cast<std::byte>(pixel);
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
    (void)colorSpace;

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
    (void)vibrance;

#ifdef ATOM_IMAGE_HAS_OPENCV
    cv::Mat src = input.to_mat();
    cv::Mat dst;
    
    if (src.channels() >= 3) {
        cv::Mat hsv;
        cv::cvtColor(src, hsv, cv::COLOR_BGR2HSV);
        
        std::vector<cv::Mat> channels;
        cv::split(hsv, channels);
        
        // Adjust saturation channel
        double factorS = (saturation + vibrance) / 200.0 + 1.0; // Combined
        channels[1].convertTo(channels[1], -1, factorS, 0);
        
        cv::merge(channels, hsv);
        cv::cvtColor(hsv, dst, cv::COLOR_HSV2BGR);
    } else {
        dst = src.clone(); // No saturation adjustment for grayscale
    }
    
    return blob(dst);
#else
    // Manual vibrance and saturation adjustment
    blob result = input.clone();
    (void)vibrance;
    (void)saturation; // For manual, simple multiply average
    double avgFactor = (vibrance + saturation) / 200.0 + 1.0;
    for (size_t i = 0; i < result.size(); ++i) {
        int val = static_cast<int>(result[i]) * avgFactor;
        result[i] = static_cast<std::byte>(std::min(255, std::max(0, val)));
    }
    return result;
#endif
}



blob ImageEnhancement::toneMapping(const blob& input,
                                  ToneMappingOperator op,
                                  const EnhancementParams& params) const {
    if (input.isEmpty()) {
        return blob{};
    }
    (void)op;

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
    cv::Mat dst = src.clone();

    switch (method) {
        case ColorCorrectionMethod::WHITE_BALANCE: {
            // Simple white balance using gray world assumption
            cv::Scalar mean = cv::mean(src);
            double avgR = mean[2], avgG = mean[1], avgB = mean[0];
            double scaleR = 128.0 / avgR, scaleG = 128.0 / avgG, scaleB = 128.0 / avgB;
            
            std::vector<cv::Mat> channels;
            cv::split(src, channels);
            channels[0].convertTo(channels[0], -1, scaleB);
            channels[1].convertTo(channels[1], -1, scaleG);
            channels[2].convertTo(channels[2], -1, scaleR);
            cv::merge(channels, dst);
            break;
        }
        case ColorCorrectionMethod::COLOR_CAST: {
            // Color cast removal using histogram matching or simple normalization
            dst = src.clone();
            // Placeholder: normalize channels
            std::vector<cv::Mat> channels;
            cv::split(dst, channels);
            for (auto& ch : channels) {
                cv::normalize(ch, ch, 0, 255, cv::NORM_MINMAX);
            }
            cv::merge(channels, dst);
            break;
        }
        case ColorCorrectionMethod::GAMMA_CORRECTION: {
            dst = gammaCorrection(input, params.gamma).to_mat();
            break;
        }
        case ColorCorrectionMethod::CURVES: {
            // Apply simple S-curve
            dst = applyCurve(input, {{0,0}, {64, 64*0.8}, {128, 128*1.2}, {192, 192*1.1}, {255,255}}).to_mat();
            break;
        }
        case ColorCorrectionMethod::LEVELS: {
            dst = adjustLevels(input, params.blackPoint[0], params.whitePoint[0], params.gamma).to_mat();
            break;
        }
        case ColorCorrectionMethod::COLOR_GRADING: {
            // Advanced grading: adjust shadows, midtones, highlights
            dst = shadowHighlight(input, params.shadows, params.highlights).to_mat();
            break;
        }
        case ColorCorrectionMethod::AUTO_LEVELS: {
            // Auto levels: stretch histogram
            cv::Mat hist;
            int histSize = 256;
            float range[] = {0, 256};
            const float* histRange = {range};
            cv::calcHist(&src, 1, 0, cv::Mat(), hist, 1, &histSize, &histRange);
            double minVal, maxVal;
            cv::minMaxLoc(hist, &minVal, &maxVal);
            // Simple stretch
            dst = src.clone();
            dst.convertTo(dst, -1, 255.0 / (maxVal - minVal), -minVal * 255.0 / (maxVal - minVal));
            break;
        }
        case ColorCorrectionMethod::AUTO_COLOR: {
            // Auto color: white balance + levels
            auto wb = colorCorrection(input, ColorCorrectionMethod::WHITE_BALANCE, params);
            dst = adjustLevels(wb, params.blackPoint[0], params.whitePoint[0], params.gamma).to_mat(); // Use [0] for luminance
            break;
        }
        default:
            THROW_RUNTIME_ERROR("Unsupported color correction method");
    }

    return blob(dst);
#else
    // Manual fallback for simple corrections
    blob result = input.clone();
    switch (method) {
        case ColorCorrectionMethod::GAMMA_CORRECTION:
            result = gammaCorrection(input, params.gamma);
            break;
        case ColorCorrectionMethod::LEVELS:
            result = adjustLevels(input, params.blackPoint, params.whitePoint, params.gamma);
            break;
        default:
            // Basic brightness/contrast as fallback
            double add = params.brightness;
            double mul = params.contrast;
            for (size_t i = 0; i < result.size(); ++i) {
                int val = static_cast<int>(result[i]) * mul + add;
                result[i] = static_cast<std::byte>(std::clamp(val, 0, 255));
            }
            break;
    }
    return result;
#endif
}

// Implement sharpen
blob ImageEnhancement::sharpen(const blob& input,
                              double strength,
                              double radius,
                              double threshold,
                              const std::string& method) const {
    if (input.isEmpty()) {
        return blob{};
    }
    (void)radius;
    (void)threshold;
    (void)method;

#ifdef ATOM_IMAGE_HAS_OPENCV
    cv::Mat src = input.to_mat();
    cv::Mat dst;

    if (method == "unsharp_mask") {
        cv::Mat blurred;
        int ksize = static_cast<int>(radius * 6 + 1); // Approximate Gaussian kernel
        ksize = std::max(3, ksize | 1); // Make odd
        cv::GaussianBlur(src, blurred, cv::Size(ksize, ksize), radius);
        cv::addWeighted(src, 1.0 + strength, blurred, -strength, 0, dst);
    } else if (method == "high_pass") {
        cv::Mat blurred, highPass;
        cv::GaussianBlur(src, blurred, cv::Size(0, 0), radius);
        cv::subtract(src, blurred, highPass);
        cv::addWeighted(src, 1.0, highPass, strength, 0, dst);
    } else if (method == "clarity") {
        // Clarity: high-pass + blend
        cv::Mat blurred, highPass;
        cv::GaussianBlur(src, blurred, cv::Size(0, 0), radius);
        cv::subtract(src, blurred, highPass);
        // Apply threshold
        cv::compare(highPass, threshold, highPass, cv::CMP_GT);
        cv::multiply(highPass, highPass, highPass, 255.0);
        cv::addWeighted(src, 1.0, highPass, strength, 0, dst);
    } else {
        THROW_RUNTIME_ERROR("Unsupported sharpening method");
    }

    return blob(dst);
#else
    // Manual unsharp mask fallback
    blob result = input.clone();
    // Simple implementation: approximate with difference of gaussians or basic
    // For simplicity, apply a basic laplacian-like sharpen
    int kernel[9] = {0, -1, 0, -1, 5, -1, 0, -1, 0};
    // Convolve manually (simplified for 1 channel)
    if (input.getChannels() == 1) {
        for (int y = 1; y < input.getHeight() - 1; ++y) {
            for (int x = 1; x < input.getWidth() - 1; ++x) {
                int sum = 0;
                for (int dy = -1; dy <= 1; ++dy) {
                    for (int dx = -1; dx <= 1; ++dx) {
                        int idx = (y + dy) * input.getWidth() + (x + dx);
                        sum += static_cast<int>(result[idx]) * kernel[(dy + 1) * 3 + (dx + 1)];
                    }
                }
                int val = static_cast<int>(input[y * input.getWidth() + x]) + strength * (sum - 5 * static_cast<int>(input[y * input.getWidth() + x]));
                result[y * input.getWidth() + x] = static_cast<std::byte>(std::clamp(val, 0, 255));
            }
        }
    }
    return result;
#endif
}

// Implement denoise
blob ImageEnhancement::denoise(const blob& input,
                              double strength,
                              const std::string& method,
                              bool preserveEdges) const {
    if (input.isEmpty()) {
        return blob{};
    }
    (void)preserveEdges;

#ifdef ATOM_IMAGE_HAS_OPENCV
    cv::Mat src = input.to_mat();
    cv::Mat dst;

    if (method == "bilateral") {
        int d = static_cast<int>(9 * strength);
        double sigmaColor = 75 * strength * (preserveEdges ? 1.0 : 1.5);
        double sigmaSpace = 75 * strength;
        cv::bilateralFilter(src, dst, d, sigmaColor, sigmaSpace);
    } else if (method == "nlm") {
        cv::Mat denoised;
        cv::fastNlMeansDenoisingColored(src, denoised, static_cast<float>(strength * 10), static_cast<float>(strength * 10), 7, 21);
        dst = denoised;
    } else if (method == "bm3d") {
        // OpenCV has no built-in BM3D, fallback to bilateral
        cv::bilateralFilter(src, dst, 9, 75 * strength, 75 * strength);
    } else if (method == "dct") {
        // DCT denoising: threshold in frequency domain (simplified)
        cv::Mat dct;
        cv::dct(src, dct);
        // Threshold small coefficients
        double thresh = strength * 50;
        dct.setTo(0, cv::abs(dct) < thresh);
        cv::idct(dct, dst);
    } else {
        THROW_RUNTIME_ERROR("Unsupported denoising method");
    }

    return blob(dst);
#else
    // Manual bilateral-like filter
    blob result = input.clone();
    int kernelSize = static_cast<int>(5 + 2 * strength * 10);
    kernelSize |= 1; // Odd
    // Simplified averaging filter as fallback
    for (int y = kernelSize/2; y < input.getHeight() - kernelSize/2; ++y) {
        for (int x = kernelSize/2; x < input.getWidth() - kernelSize/2; ++x) {
            int sum = 0;
            for (int dy = -kernelSize/2; dy <= kernelSize/2; ++dy) {
                for (int dx = -kernelSize/2; dx <= kernelSize/2; ++dx) {
                    sum += static_cast<int>(input[(y + dy) * input.getWidth() + (x + dx)]);
                }
            }
            int avg = sum / (kernelSize * kernelSize);
            result[y * input.getWidth() + x] = static_cast<std::byte>(avg);
        }
    }
    return result;
#endif
}

// Implement shadowHighlight
blob ImageEnhancement::shadowHighlight(const blob& input,
                                      double shadows,
                                      double highlights,
                                      double radius) const {
    if (input.isEmpty()) {
        return blob{};
    }
    (void)radius;

#ifdef ATOM_IMAGE_HAS_OPENCV
    cv::Mat src = input.to_mat();
    cv::Mat lab, lChannel, enhancedL;
    cv::cvtColor(src, lab, cv::COLOR_BGR2Lab);
    std::vector<cv::Mat> channels;
    cv::split(lab, channels);
    lChannel = channels[0];

    // Shadows: lift low values
    cv::Mat shadowMask = lChannel < 50;
    cv::Mat shadowsL = lChannel.clone();
    shadowsL.setTo(lChannel + shadows * 2.55, shadowMask);

    // Highlights: compress high values
    cv::Mat highlightMask = lChannel > 200;
    cv::Mat highlightsL = lChannel.clone();
    highlightsL.setTo(lChannel - highlights * 2.55, highlightMask);

    // Blend
    cv::Mat blendedL = lChannel.clone();
    blendedL = shadowsL * 0.5 + highlightsL * 0.5; // Simple blend

    channels[0] = blendedL;
    cv::merge(channels, lab);
    cv::cvtColor(lab, enhancedL, cv::COLOR_Lab2BGR);

    return blob(enhancedL);
#else
    // Manual implementation in LAB-like space (simplified RGB)
    blob result = input.clone();
    double shadowLift = shadows / 100.0 * 50;
    double highlightCompress = highlights / 100.0 * 50;
    for (size_t i = 0; i < result.size(); i += 3) { // Assume RGB
        std::array<int, 3> pixel = {static_cast<int>(result[i]), static_cast<int>(result[i+1]), static_cast<int>(result[i+2])};
        int avg = (pixel[0] + pixel[1] + pixel[2]) / 3;
        if (avg < 50) {
            for (auto& p : pixel) p = std::min(255, p + static_cast<int>(shadowLift));
        } else if (avg > 200) {
            for (auto& p : pixel) p = std::max(0, p - static_cast<int>(highlightCompress));
        }
        result[i] = static_cast<std::byte>(pixel[0]);
        result[i+1] = static_cast<std::byte>(pixel[1]);
        result[i+2] = static_cast<std::byte>(pixel[2]);
    }
    return result;
#endif
}

// Implement clarity
blob ImageEnhancement::clarity(const blob& input,
                              double clarity,
                              double radius,
                              bool preserveSkin) const {
    if (input.isEmpty()) {
        return blob{};
    }
    (void)preserveSkin;

#ifdef ATOM_IMAGE_HAS_OPENCV
    cv::Mat src = input.to_mat();
    cv::Mat blurred, highPass, dst;
    cv::GaussianBlur(src, blurred, cv::Size(0, 0), radius);
    cv::subtract(src, blurred, highPass);
    
    // Apply clarity amount
    cv::addWeighted(src, 1.0, highPass, clarity / 100.0, 0, dst);
    
    if (preserveSkin) {
        // Simple skin tone preservation: reduce clarity on warm tones
        cv::Mat hsv;
        cv::cvtColor(dst, hsv, cv::COLOR_BGR2HSV);
        std::vector<cv::Mat> channels;
        cv::split(hsv, channels);
        // Reduce clarity where hue is skin-like (20-40)
        cv::Mat skinMask = (channels[0] > 20) & (channels[0] < 40);
        dst.setTo(cv::Scalar(0,0,0), skinMask); // Placeholder: blend back original
    }
    
    return blob(dst);
#else
    // Manual high-pass clarity
    blob result = input.clone();
    // Simplified: add edge enhancement
    double amount = clarity / 100.0;
    // Basic laplacian kernel application (simplified)
    for (int y = 1; y < input.getHeight() - 1; ++y) {
        for (int x = 1; x < input.getWidth() - 1; ++x) {
            // Average neighbors
            int avg = 0;
            for (int dy = -1; dy <= 1; ++dy) {
                for (int dx = -1; dx <= 1; ++dx) {
                    if (dy == 0 && dx == 0) continue;
                    avg += static_cast<int>(input[(y + dy) * input.getWidth() + (x + dx)]);
                }
            }
            avg /= 8;
            int enhanced = static_cast<int>(input[y * input.getWidth() + x]) + amount * (static_cast<int>(input[y * input.getWidth() + x]) - avg);
            result[y * input.getWidth() + x] = static_cast<std::byte>(std::clamp(enhanced, 0, 255));
        }
    }
    return result;
#endif
}

// Implement dehaze
blob ImageEnhancement::dehaze(const blob& input,
                             double strength,
                             bool preserveColors) const {
    if (input.isEmpty()) {
        return blob{};
    }
    (void)preserveColors;

#ifdef ATOM_IMAGE_HAS_OPENCV
    cv::Mat src = input.to_mat();
    cv::Mat dst;
    
    // Simple dark channel prior approximation
    cv::Mat darkChannel = cv::Mat::ones(src.size(), src.type()) * 255;
    std::vector<cv::Mat> channels;
    cv::split(src, channels);
    for (int i = 0; i < 3; ++i) {
        cv::min(darkChannel, channels[i], darkChannel);
    }
    
    // Estimate airlight (simplified: top 0.1% brightest)
    cv::Mat sortedDark;
    cv::sort(darkChannel, sortedDark, cv::SORT_EVERY_ROW + cv::SORT_DESCENDING);
    cv::Scalar airlight = cv::mean(sortedDark(cv::Rect(0, 0, 1, static_cast<int>(sortedDark.rows * 0.001))));
    
    // Transmission map (simplified)
    double t = 1.0 - strength * 0.95;
    cv::Mat transmission = darkChannel / std::max(airlight[0], 1.0);
    transmission = transmission * t + (1 - t);
    
    // Dehaze formula
    cv::Mat normalized = src / 255.0;
    dst = (normalized - airlight[0] / 255.0) / transmission + airlight[0] / 255.0;
    dst *= 255.0;
    dst.convertTo(dst, CV_8U);
    
    if (preserveColors) {
        // Adjust saturation
        cv::Mat hsv;
        cv::cvtColor(dst, hsv, cv::COLOR_BGR2HSV);
        std::vector<cv::Mat> hsvChannels;
        cv::split(hsv, hsvChannels);
        hsvChannels[1] *= preserveColors ? 0.8 : 1.0; // Reduce saturation
        cv::merge(hsvChannels, hsv);
        cv::cvtColor(hsv, dst, cv::COLOR_HSV2BGR);
    }
    
    return blob(dst);
#else
    // Manual simple dehaze: increase contrast and brightness
    return adjustBrightnessContrast(input, 20 * strength, 1.2 + 0.3 * strength);
#endif
}

// Implement calculateHistogram
std::vector<std::vector<double>> ImageEnhancement::calculateHistogram(const blob& input,
                                                                    int channel,
                                                                    int bins) const {
    std::vector<std::vector<double>> histograms;
    
    if (input.isEmpty()) {
        return histograms;
    }

#ifdef ATOM_IMAGE_HAS_OPENCV
    cv::Mat src = input.to_mat();
    if (channel == -1) {
        // All channels
        std::vector<cv::Mat> channels;
        cv::split(src, channels);
        histograms.resize(channels.size());
        for (size_t i = 0; i < channels.size(); ++i) {
            cv::Mat hist;
            int histSize = bins;
            float range[] = {0, 256};
            const float* histRange = {range};
            cv::calcHist(&channels[i], 1, 0, cv::Mat(), hist, 1, &histSize, &histRange);
            histograms[i].resize(bins);
            for (int j = 0; j < bins; ++j) {
                histograms[i][j] = hist.at<float>(j);
            }
        }
    } else {
        // Single channel
        histograms.resize(1);
        cv::Mat hist;
        int histSize = bins;
        float range[] = {0, 256};
        const float* histRange = {range};
        cv::calcHist(&src, 1, &channel, cv::Mat(), hist, 1, &histSize, &histRange);
        histograms[0].resize(bins);
        for (int j = 0; j < bins; ++j) {
            histograms[0][j] = hist.at<float>(j);
        }
    }
#else
    // Manual histogram calculation
    std::vector<int> counts(bins, 0);
    auto data = input.begin();
    auto end = input.end();
    while (data != end) {
        int val = static_cast<int>(*data);
        int bin = std::min(val * bins / 256, bins - 1);
        counts[bin]++;
        ++data;
    }
    std::vector<double> hist(bins);
    for (int i = 0; i < bins; ++i) {
        hist[i] = static_cast<double>(counts[i]);
    }
    histograms.push_back(hist);
#endif
    
    return histograms;
}

// Implement applyCurve
blob ImageEnhancement::applyCurve(const blob& input,
                                 const std::vector<std::pair<double, double>>& curve,
                                 int channel) const {
    if (input.isEmpty() || curve.empty()) {
        return input;
    }

#ifdef ATOM_IMAGE_HAS_OPENCV
    cv::Mat src = input.to_mat();
    cv::Mat dst = src.clone();
    
    // Create lookup table from curve points (linear interpolation)
    std::vector<cv::Point2f> points;
    for (const auto& p : curve) {
        points.emplace_back(static_cast<float>(p.first), static_cast<float>(p.second));
    }
    cv::Mat lut(1, 256, CV_8U);
    
    // Interpolate curve
    for (int i = 0; i < 256; ++i) {
        // Find segment
        auto it = std::lower_bound(points.begin(), points.end(), cv::Point2f(static_cast<float>(i), 0),
                                   [](const cv::Point2f& a, const cv::Point2f& b) { return a.x < b.x; });
        if (it == points.end()) {
            lut.at<uint8_t>(0, i) = static_cast<uint8_t>(points.back().y);
        } else if (it == points.begin()) {
            lut.at<uint8_t>(0, i) = static_cast<uint8_t>(points.front().y);
        } else {
            auto prev = std::prev(it);
            float t = (i - prev->x) / (it->x - prev->x);
            float val = prev->y * (1 - t) + it->y * t;
            lut.at<uint8_t>(0, i) = static_cast<uint8_t>(std::clamp(val, 0.0f, 255.0f));
        }
    }
    
    if (channel == -1) {
        // Apply to all channels
        std::vector<cv::Mat> channels;
        cv::split(dst, channels);
        for (auto& ch : channels) {
            cv::LUT(ch, lut, ch);
        }
        cv::merge(channels, dst);
    } else {
        // Single channel
        std::vector<cv::Mat> channels;
        cv::split(dst, channels);
        if (channel >= 0 && channel < static_cast<int>(channels.size())) {
            cv::LUT(channels[channel], lut, channels[channel]);
        }
        cv::merge(channels, dst);
    }
    
    return blob(dst);
#else
    // Manual curve application
    blob result = input.clone();
    // Create LUT
    std::array<uint8_t, 256> lut{};
    for (int i = 0; i < 256; ++i) {
        // Simple linear interpolation between points
        auto it = std::lower_bound(curve.begin(), curve.end(), std::make_pair(static_cast<double>(i), -1.0),
                                   [](const auto& a, const auto& b) { return a.first < b.first; });
        if (it == curve.end()) {
            lut[i] = static_cast<uint8_t>(curve.back().second);
        } else if (it == curve.begin()) {
            lut[i] = static_cast<uint8_t>(curve.front().second);
        } else {
            auto prev = std::prev(it);
            double t = (i - prev->first) / (it->first - prev->first);
            double val = prev->second * (1 - t) + it->second * t;
            lut[i] = static_cast<uint8_t>(std::clamp(val, 0.0, 255.0));
        }
    }
    
    // Apply LUT (assume single channel for simplicity)
    for (size_t i = 0; i < result.size(); ++i) {
        uint8_t val = static_cast<uint8_t>(result[i]);
        result[i] = static_cast<std::byte>(lut[val]);
    }
    return result;
#endif
}

// Implement adjustLevels
blob ImageEnhancement::adjustLevels(const blob& input,
                                   double blackPoint,
                                   double whitePoint,
                                   double gamma,
                                   double outputBlack,
                                   double outputWhite) const {
    if (input.isEmpty()) {
        return input;
    }

#ifdef ATOM_IMAGE_HAS_OPENCV
    cv::Mat src = input.to_mat();
    cv::Mat dst;
    
    // Normalize to 0-1
    cv::Mat norm;
    src.convertTo(norm, CV_32F, 1.0/255.0);
    
    // Stretch to [blackPoint, whitePoint]
    double inputRange = whitePoint - blackPoint;
    norm = (norm - blackPoint / 255.0) / (inputRange / 255.0);
    norm = cv::max(norm, 0.0);
    norm = cv::min(norm, 1.0);
    
    // Apply gamma
    cv::pow(norm, gamma, norm);
    
    // Output range
    double outputRange = outputWhite - outputBlack;
    norm = norm * (outputRange / 255.0) + outputBlack / 255.0;
    
    // Back to 8-bit
    norm.convertTo(dst, CV_8U, 255.0);
    
    return blob(dst);
#else
    // Manual levels adjustment
    blob result = input.clone();
    double bp = blackPoint / 255.0;
    double wp = whitePoint / 255.0;
    double invGamma = 1.0 / gamma;
    double ob = outputBlack / 255.0;
    double ow = outputWhite / 255.0;
    double scale = ow - ob;
    
    for (size_t i = 0; i < result.size(); ++i) {
        double val = static_cast<double>(result[i]) / 255.0;
        val = std::max(0.0, (val - bp) / (wp - bp));
        val = std::pow(val, invGamma);
        val = val * scale + ob;
        val = std::clamp(val, 0.0, 1.0);
        result[i] = static_cast<std::byte>(val * 255.0);
    }
    return result;
#endif
}

// Implement protected enhanceInColorSpace
std::vector<std::byte> ImageEnhancement::enhanceInColorSpace(
    const std::vector<std::byte>& input,
    int width, int height, int channels,
    ColorSpace colorSpace,
    std::function<std::vector<std::byte>(const std::vector<std::byte>&, int, int, int)> enhanceFunction) const {
    if (input.empty() || width <= 0 || height <= 0 || channels <= 0) {
        return input;
    }

#ifdef ATOM_IMAGE_HAS_OPENCV
    // Use OpenCV for color space conversion
    cv::Mat rgb(height, width, CV_8UC(channels), const_cast<std::byte*>(input.data()));
    cv::Mat converted;
    
    int code = -1;
    if (colorSpace == ColorSpace::HSV) code = cv::COLOR_RGB2HSV;
    else if (colorSpace == ColorSpace::HSL) code = cv::COLOR_RGB2HLS; // Approximate
    else if (colorSpace == ColorSpace::LAB) code = cv::COLOR_RGB2Lab;
    else if (colorSpace == ColorSpace::YUV) code = cv::COLOR_RGB2YUV;
    else if (colorSpace == ColorSpace::XYZ) code = cv::COLOR_RGB2XYZ;
    else if (colorSpace == ColorSpace::GRAY) {
        cv::cvtColor(rgb, converted, cv::COLOR_RGB2GRAY);
        channels = 1;
    }
    
    if (code != -1) {
        cv::cvtColor(rgb, converted, code);
    } else {
        converted = rgb;
    }
    
    // Convert cv::Mat to std::vector<std::byte> for the enhancement function
    const auto* byte_ptr = reinterpret_cast<const std::byte*>(converted.data);
    std::vector<std::byte> input_data(byte_ptr, byte_ptr + converted.total() * converted.elemSize());

    // Apply enhancement
    std::vector<std::byte> enhancedData(enhanceFunction(input_data, converted.rows, converted.cols, converted.channels()));
    
    // Convert back
    cv::Mat enhanced(height, width, CV_8UC(channels), enhancedData.data());
    cv::Mat result;
    int backCode = -1;
    if (colorSpace == ColorSpace::HSV) backCode = cv::COLOR_HSV2RGB;
    else if (colorSpace == ColorSpace::HSL) backCode = cv::COLOR_HLS2RGB;
    else if (colorSpace == ColorSpace::LAB) backCode = cv::COLOR_Lab2RGB;
    else if (colorSpace == ColorSpace::YUV) backCode = cv::COLOR_YUV2RGB;
    else if (colorSpace == ColorSpace::XYZ) backCode = cv::COLOR_XYZ2RGB;
    else if (colorSpace == ColorSpace::GRAY) backCode = cv::COLOR_GRAY2RGB;
    
    if (backCode != -1) {
        cv::cvtColor(enhanced, result, backCode);
    } else {
        result = enhanced;
    }
    
    const auto* result_byte_ptr = reinterpret_cast<const std::byte*>(result.data);
    return std::vector<std::byte>(result_byte_ptr, result_byte_ptr + result.total() * result.elemSize());
#else
    // Manual conversion and enhancement (simplified for RGB only)
    return enhanceFunction(input, width, height, channels);
#endif
}

// Implement rgbToColorSpace
std::array<double, 3> ImageEnhancement::rgbToColorSpace(const std::array<uint8_t, 3>& rgb,
                                                       ColorSpace colorSpace) const {
    double r = rgb[0] / 255.0, g = rgb[1] / 255.0, b = rgb[2] / 255.0;
    
    switch (colorSpace) {
        case ColorSpace::HSV: {
            double maxC = std::max({r, g, b});
            double minC = std::min({r, g, b});
            double delta = maxC - minC;
            double h = 0;
            if (delta > 0) {
                if (maxC == r) h = 60 * fmod((g - b) / delta, 6);
                else if (maxC == g) h = 60 * ((b - r) / delta + 2);
                else h = 60 * ((r - g) / delta + 4);
            }
            double s = maxC == 0 ? 0 : delta / maxC;
            return {h, s, maxC};
        }
        case ColorSpace::HSL: {
            double maxC = std::max({r, g, b});
            double minC = std::min({r, g, b});
            double delta = maxC - minC;
            double h = 0, s = 0, l = (maxC + minC) / 2;
            if (delta > 0) {
                if (l <= 0.5) s = delta / (maxC + minC);
                else s = delta / (2 - maxC - minC);
                if (maxC == r) h = fmod((g - b) / delta + 6, 6) * 60;
                else if (maxC == g) h = ((b - r) / delta + 2) * 60;
                else h = ((r - g) / delta + 4) * 60;
            }
            return {h, s, l};
        }
        case ColorSpace::LAB: {
            // Simplified LAB conversion (full requires XYZ)
            double x = 0.4124 * r + 0.3576 * g + 0.1805 * b;
            double y = 0.2126 * r + 0.7152 * g + 0.0722 * b;
            double z = 0.0193 * r + 0.1192 * g + 0.9505 * b;
            double fx = x > 0.008856 ? pow(x, 1/3.0) : 7.787 * x + 16.0/116.0;
            double fy = y > 0.008856 ? pow(y, 1/3.0) : 7.787 * y + 16.0/116.0;
            double fz = z > 0.008856 ? pow(z, 1/3.0) : 7.787 * z + 16.0/116.0;
            double l = 116 * fy - 16;
            double a = 500 * (fx - fy);
            double b_ = 200 * (fy - fz);
            return {l, a, b_};
        }
        case ColorSpace::YUV: {
            double y = 0.299 * r + 0.587 * g + 0.114 * b;
            double u = -0.147 * r - 0.289 * g + 0.436 * b;
            double v = 0.615 * r - 0.515 * g - 0.100 * b;
            return {y, u + 0.5, v + 0.5};
        }
        case ColorSpace::XYZ: {
            double x = 0.4124 * r + 0.3576 * g + 0.1805 * b;
            double y = 0.2126 * r + 0.7152 * g + 0.0722 * b;
            double z = 0.0193 * r + 0.1192 * g + 0.9505 * b;
            return {x, y, z};
        }
        default:
            return {r, g, b}; // RGB
    }
}

// Implement colorSpaceToRgb
std::array<uint8_t, 3> ImageEnhancement::colorSpaceToRgb(const std::array<double, 3>& values,
                                                        ColorSpace colorSpace) const {
    double r = 0, g = 0, b = 0;
    
    switch (colorSpace) {
        case ColorSpace::HSV: {
            double h = values[0], s = values[1], v = values[2];
            h = fmod(h, 360) / 60;
            int i = static_cast<int>(h);
            double f = h - i;
            double p = v * (1 - s);
            double q = v * (1 - s * f);
            double t = v * (1 - s * (1 - f));
            switch (i) {
                case 0: r = v; g = t; b = p; break;
                case 1: r = q; g = v; b = p; break;
                case 2: r = p; g = v; b = t; break;
                case 3: r = p; g = q; b = v; break;
                case 4: r = t; g = p; b = v; break;
                case 5: r = v; g = p; b = q; break;
            }
            break;
        }
        case ColorSpace::HSL: {
            double h = values[0], s = values[1], l = values[2];
            if (s == 0) {
                r = g = b = l;
            } else {
                double q = l < 0.5 ? l * (1 + s) : l + s - l * s;
                double p = 2 * l - q;
                r = hueToRgb(p, q, h / 360 + 1/3.0);
                g = hueToRgb(p, q, h / 360);
                b = hueToRgb(p, q, h / 360 - 1/3.0);
            }
            break;
        }
        case ColorSpace::LAB: {
            // Simplified inverse LAB to RGB
            double l = values[0], a = values[1], bb = values[2];
            double fy = (l + 16) / 116;
            double fx = a / 500 + fy;
            double fz = fy - bb / 200;
            double x = fx > 0.2069 ? fx*fx*fx : (fx - 16.0/116.0) / 7.787;
            double y = fy > 0.2069 ? fy*fy*fy : (fy - 16.0/116.0) / 7.787;
            double z = fz > 0.2069 ? fz*fz*fz : (fz - 16.0/116.0) / 7.787;
            double rr = 3.2406 * x - 1.5372 * y - 0.4986 * z;
            double gg = -0.9689 * x + 1.8758 * y + 0.0415 * z;
            double bb_ = 0.0557 * x - 0.2040 * y + 1.0570 * z;
            r = rr > 0.0031308 ? 1.055 * pow(rr, 1/2.4) - 0.055 : 12.92 * rr;
            g = gg > 0.0031308 ? 1.055 * pow(gg, 1/2.4) - 0.055 : 12.92 * gg;
            b = bb_ > 0.0031308 ? 1.055 * pow(bb_, 1/2.4) - 0.055 : 12.92 * bb_;
            break;
        }
        case ColorSpace::YUV: {
            double y = values[0], u = values[1] - 0.5, v = values[2] - 0.5;
            r = y + 1.13983 * v;
            g = y - 0.39465 * u - 0.58060 * v;
            b = y + 2.03211 * u;
            break;
        }
        case ColorSpace::XYZ: {
            double x = values[0], y = values[1], z = values[2];
            r = 3.2406 * x - 1.5372 * y - 0.4986 * z;
            g = -0.9689 * x + 1.8758 * y + 0.0415 * z;
            b = 0.0557 * x - 0.2040 * y + 1.0570 * z;
            r = r > 0.0031308 ? 1.055 * pow(r, 1/2.4) - 0.055 : 12.92 * r;
            g = g > 0.0031308 ? 1.055 * pow(g, 1/2.4) - 0.055 : 12.92 * g;
            b = b > 0.0031308 ? 1.055 * pow(b, 1/2.4) - 0.055 : 12.92 * b;
            break;
        }
        default:
            r = values[0]; g = values[1]; b = values[2];
    }
    
    return {static_cast<uint8_t>(std::clamp(r * 255, 0.0, 255.0)),
            static_cast<uint8_t>(std::clamp(g * 255, 0.0, 255.0)),
            static_cast<uint8_t>(std::clamp(b * 255, 0.0, 255.0))};
}

// Helper for HSL to RGB
double ImageEnhancement::hueToRgb(double p, double q, double t) const {
    if (t < 0) t += 1;
    if (t > 1) t -= 1;
    if (t < 1.0/6) return p + (q - p) * 6 * t;
    if (t < 0.5) return q;
    if (t < 2.0/6) return p + (q - p) * (2.0/3 - t) * 6;
    return p;
}

// Implement factory
std::unique_ptr<ImageEnhancement> createOptimalEnhancement(bool useGPU) {
    (void)useGPU; // GPU not implemented yet
    return std::make_unique<ImageEnhancement>();
}

}  // namespace atom::image
