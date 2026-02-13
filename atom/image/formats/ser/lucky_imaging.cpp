// lucky_imaging.cpp
#include "lucky_imaging.h"
#include "ser_reader.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <numeric>
#include <opencv2/imgproc.hpp>

namespace serastro {

LuckyImaging::LuckyImaging()
    : qualityAssessor_(std::make_shared<QualityAssessor>()),
      registrar_(std::make_shared<FrameRegistrar>()),
      stacker_(std::make_shared<FrameStacker>()) {}

LuckyImaging::LuckyImaging(const LuckyImagingParams& params)
    : params_(params),
      qualityAssessor_(std::make_shared<QualityAssessor>(params.qualityParams)),
      registrar_(std::make_shared<FrameRegistrar>(params.registrationParams)),
      stacker_(std::make_shared<FrameStacker>(params.stackingParams)) {}

LuckyImagingResult LuckyImaging::process(const std::vector<cv::Mat>& frames,
                                         ProgressCallback progressCallback) {
    LuckyImagingResult result;
    result.totalFrames = frames.size();

    if (frames.empty()) {
        return result;
    }

    auto totalStart = std::chrono::high_resolution_clock::now();

    auto reportProgress = [&progressCallback](float p, const std::string& msg) {
        if (progressCallback)
            progressCallback(p, msg);
    };

    // Phase 1: Quality assessment
    reportProgress(0.0f, "Assessing frame quality");
    auto qualityStart = std::chrono::high_resolution_clock::now();

    result.qualityScores = assessFrameQuality(
        frames, [&reportProgress](float p, const std::string& msg) {
            reportProgress(p * 0.3f, msg);
        });

    auto qualityEnd = std::chrono::high_resolution_clock::now();
    result.qualityTimeMs =
        std::chrono::duration<double, std::milli>(qualityEnd - qualityStart)
            .count();

    // Phase 2: Frame selection
    reportProgress(0.3f, "Selecting best frames");
    result.selectedIndices = selectFrames(result.qualityScores);
    result.selectedFrames = result.selectedIndices.size();
    result.rejectedFrames = result.totalFrames - result.selectedFrames;

    if (result.selectedIndices.empty()) {
        return result;
    }

    // Calculate quality statistics
    result.bestQuality = *std::max_element(result.qualityScores.begin(),
                                           result.qualityScores.end());

    double selectedSum = 0.0;
    double worstSelected = std::numeric_limits<double>::max();
    for (size_t idx : result.selectedIndices) {
        selectedSum += result.qualityScores[idx];
        worstSelected = std::min(worstSelected, result.qualityScores[idx]);
    }
    result.averageQuality = selectedSum / result.selectedIndices.size();
    result.worstSelectedQuality = worstSelected;

    // Extract selected frames
    std::vector<cv::Mat> selectedFrames;
    selectedFrames.reserve(result.selectedIndices.size());
    for (size_t idx : result.selectedIndices) {
        selectedFrames.push_back(frames[idx].clone());
    }

    // Set reference frame (best quality)
    size_t bestIdx =
        std::distance(result.qualityScores.begin(),
                      std::max_element(result.qualityScores.begin(),
                                       result.qualityScores.end()));
    result.referenceFrame = frames[bestIdx].clone();

    // Phase 3: Registration
    std::vector<cv::Mat> alignedFrames;
    if (params_.enableRegistration && selectedFrames.size() > 1) {
        reportProgress(0.35f, "Aligning frames");
        auto regStart = std::chrono::high_resolution_clock::now();

        alignedFrames =
            alignFrames(selectedFrames, result.selectedIndices,
                        [&reportProgress](float p, const std::string& msg) {
                            reportProgress(0.35f + p * 0.35f, msg);
                        });

        auto regEnd = std::chrono::high_resolution_clock::now();
        result.registrationTimeMs =
            std::chrono::duration<double, std::milli>(regEnd - regStart)
                .count();
    } else {
        alignedFrames = std::move(selectedFrames);
    }

    // Phase 4: Stacking
    reportProgress(0.7f, "Stacking frames");
    auto stackStart = std::chrono::high_resolution_clock::now();

    result.stackedImage = stackFrames(
        alignedFrames, [&reportProgress](float p, const std::string& msg) {
            reportProgress(0.7f + p * 0.2f, msg);
        });

    auto stackEnd = std::chrono::high_resolution_clock::now();
    result.stackingTimeMs =
        std::chrono::duration<double, std::milli>(stackEnd - stackStart)
            .count();

    // Phase 5: Post-processing
    if (params_.applyWaveletSharpening || params_.applyDeconvolution) {
        reportProgress(0.9f, "Applying post-processing");
        result.stackedImage = applyPostProcessing(result.stackedImage);
    }

    auto totalEnd = std::chrono::high_resolution_clock::now();
    result.processingTimeMs =
        std::chrono::duration<double, std::milli>(totalEnd - totalStart)
            .count();

    reportProgress(1.0f, "Processing complete");

    return result;
}

LuckyImagingResult LuckyImaging::processFile(
    const std::filesystem::path& serFile, ProgressCallback progressCallback) {
    auto reportProgress = [&progressCallback](float p, const std::string& msg) {
        if (progressCallback)
            progressCallback(p, msg);
    };

    reportProgress(0.0f, "Loading SER file");

    SERReader reader(serFile);
    size_t frameCount = reader.getFrameCount();

    std::vector<cv::Mat> frames;
    frames.reserve(frameCount);

    for (size_t i = 0; i < frameCount; ++i) {
        frames.push_back(reader.readFrame(i));

        float loadProgress = static_cast<float>(i + 1) / frameCount * 0.2f;
        reportProgress(loadProgress, "Loading frame " + std::to_string(i + 1));
    }

    // Adjust progress callback for remaining processing
    auto adjustedCallback = [&progressCallback](float p,
                                                const std::string& msg) {
        if (progressCallback)
            progressCallback(0.2f + p * 0.8f, msg);
    };

    return process(frames, adjustedCallback);
}

cv::Mat LuckyImaging::quickProcess(const std::vector<cv::Mat>& frames,
                                   double topPercent) {
    LuckyImagingParams params;
    params.selectionMethod = SelectionMethod::Percentage;
    params.selectionPercentage = topPercent;
    params.enableRegistration = true;
    params.stackingParams.method = StackingMethod::Mean;

    setParameters(params);

    auto result = process(frames, nullptr);
    return result.stackedImage;
}

void LuckyImaging::setParameters(const LuckyImagingParams& params) {
    params_ = params;

    qualityAssessor_ = std::make_shared<QualityAssessor>(params.qualityParams);
    registrar_ = std::make_shared<FrameRegistrar>(params.registrationParams);
    stacker_ = std::make_shared<FrameStacker>(params.stackingParams);
}

void LuckyImaging::setQualityAssessor(
    std::shared_ptr<QualityAssessor> assessor) {
    qualityAssessor_ = assessor;
}

void LuckyImaging::setRegistrar(std::shared_ptr<FrameRegistrar> registrar) {
    registrar_ = registrar;
}

void LuckyImaging::setStacker(std::shared_ptr<FrameStacker> stacker) {
    stacker_ = stacker;
}

cv::Rect LuckyImaging::autoDetectPlanetROI(const cv::Mat& frame) const {
    cv::Mat gray;
    if (frame.channels() > 1) {
        cv::cvtColor(frame, gray, cv::COLOR_BGR2GRAY);
    } else {
        gray = frame;
    }

    // Threshold to find bright object
    cv::Mat binary;
    double maxVal;
    cv::minMaxLoc(gray, nullptr, &maxVal);
    cv::threshold(gray, binary, maxVal * 0.1, 255, cv::THRESH_BINARY);

    // Find contours
    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(binary, contours, cv::RETR_EXTERNAL,
                     cv::CHAIN_APPROX_SIMPLE);

    if (contours.empty()) {
        return cv::Rect(0, 0, frame.cols, frame.rows);
    }

    // Find largest contour (the planet)
    size_t largestIdx = 0;
    double largestArea = 0;
    for (size_t i = 0; i < contours.size(); ++i) {
        double area = cv::contourArea(contours[i]);
        if (area > largestArea) {
            largestArea = area;
            largestIdx = i;
        }
    }

    // Get bounding rect with margin
    cv::Rect boundingRect = cv::boundingRect(contours[largestIdx]);

    int margin = static_cast<int>(
        std::max(boundingRect.width, boundingRect.height) * 0.2);
    boundingRect.x = std::max(0, boundingRect.x - margin);
    boundingRect.y = std::max(0, boundingRect.y - margin);
    boundingRect.width =
        std::min(frame.cols - boundingRect.x, boundingRect.width + 2 * margin);
    boundingRect.height =
        std::min(frame.rows - boundingRect.y, boundingRect.height + 2 * margin);

    return boundingRect;
}

cv::Mat LuckyImaging::applyWaveletSharpening(const cv::Mat& image,
                                             double strength,
                                             int layers) const {
    cv::Mat result;
    image.convertTo(result, CV_32F);

    std::vector<cv::Mat> waveletLayers;
    cv::Mat current = result.clone();

    // Decompose into wavelet layers (using Laplacian pyramid)
    for (int i = 0; i < layers; ++i) {
        cv::Mat blurred;
        cv::GaussianBlur(current, blurred, cv::Size(5, 5), 0);

        cv::Mat detail = current - blurred;
        waveletLayers.push_back(detail);
        current = blurred;
    }
    waveletLayers.push_back(current);  // Residual

    // Reconstruct with enhanced details
    result = waveletLayers.back().clone();
    for (int i = layers - 1; i >= 0; --i) {
        // Apply layer-specific sharpening
        double layerStrength = strength * (1.0 + 0.5 * (layers - 1 - i));
        result = result + waveletLayers[i] * layerStrength;
    }

    // Convert back to original type
    cv::Mat output;
    if (image.depth() == CV_8U) {
        result.convertTo(output, CV_8U);
    } else if (image.depth() == CV_16U) {
        result.convertTo(output, CV_16U);
    } else {
        output = result;
    }

    return output;
}

cv::Mat LuckyImaging::applyDeconvolution(const cv::Mat& image, double psfSigma,
                                         int iterations) const {
    // Richardson-Lucy deconvolution
    cv::Mat floatImage;
    image.convertTo(floatImage, CV_32F);

    if (floatImage.channels() > 1) {
        cv::cvtColor(floatImage, floatImage, cv::COLOR_BGR2GRAY);
    }

    // Create PSF
    int psfSize = static_cast<int>(psfSigma * 6) | 1;  // Ensure odd
    cv::Mat psf = createGaussianPSF(psfSigma, psfSize);
    cv::Mat psfFlipped;
    cv::flip(psf, psfFlipped, -1);

    // Initialize estimate
    cv::Mat estimate = floatImage.clone();
    cv::Mat ones = cv::Mat::ones(floatImage.size(), CV_32F);

    for (int iter = 0; iter < iterations; ++iter) {
        // Convolve estimate with PSF
        cv::Mat blurred;
        cv::filter2D(estimate, blurred, -1, psf);

        // Avoid division by zero
        blurred = cv::max(blurred, 1e-10f);

        // Calculate ratio
        cv::Mat ratio = floatImage / blurred;

        // Correlate with PSF (convolve with flipped PSF)
        cv::Mat correction;
        cv::filter2D(ratio, correction, -1, psfFlipped);

        // Update estimate
        estimate = estimate.mul(correction);
    }

    // Convert back to original format
    cv::Mat output;
    if (image.depth() == CV_8U) {
        estimate.convertTo(output, CV_8U);
    } else if (image.depth() == CV_16U) {
        estimate.convertTo(output, CV_16U);
    } else {
        output = estimate;
    }

    // Convert back to color if input was color
    if (image.channels() > 1) {
        cv::Mat colorOutput;
        cv::cvtColor(output, colorOutput, cv::COLOR_GRAY2BGR);
        return colorOutput;
    }

    return output;
}

std::vector<double> LuckyImaging::assessFrameQuality(
    const std::vector<cv::Mat>& frames,
    ProgressCallback progressCallback) const {
    std::vector<double> qualities;
    qualities.reserve(frames.size());

    for (size_t i = 0; i < frames.size(); ++i) {
        cv::Mat frameToAssess = frames[i];

        // Use ROI if specified
        if (params_.useROI && !params_.roi.empty()) {
            frameToAssess = frames[i](params_.roi);
        } else if (params_.autoDetectROI && i == 0) {
            // Auto-detect ROI on first frame
            // (would need to make params_ non-const or cache ROI)
        }

        double quality = qualityAssessor_->assessQuality(frameToAssess);
        qualities.push_back(quality);

        if (progressCallback) {
            float progress = static_cast<float>(i + 1) / frames.size();
            progressCallback(progress,
                             "Assessing frame " + std::to_string(i + 1));
        }
    }

    return qualities;
}

std::vector<size_t> LuckyImaging::selectFrames(
    const std::vector<double>& qualities) const {
    if (qualities.empty()) {
        return {};
    }

    // Create indexed quality pairs
    std::vector<std::pair<size_t, double>> indexed;
    indexed.reserve(qualities.size());
    for (size_t i = 0; i < qualities.size(); ++i) {
        indexed.emplace_back(i, qualities[i]);
    }

    // Sort by quality descending
    std::sort(indexed.begin(), indexed.end(),
              [](const auto& a, const auto& b) { return a.second > b.second; });

    std::vector<size_t> selected;

    switch (params_.selectionMethod) {
        case SelectionMethod::Percentage: {
            size_t count = static_cast<size_t>(
                qualities.size() * params_.selectionPercentage / 100.0);
            count = std::max<size_t>(count, 1);
            selected.reserve(count);
            for (size_t i = 0; i < count && i < indexed.size(); ++i) {
                selected.push_back(indexed[i].first);
            }
            break;
        }

        case SelectionMethod::Count: {
            size_t count = std::min(params_.selectionCount, indexed.size());
            selected.reserve(count);
            for (size_t i = 0; i < count; ++i) {
                selected.push_back(indexed[i].first);
            }
            break;
        }

        case SelectionMethod::Threshold: {
            for (const auto& [idx, quality] : indexed) {
                if (quality >= params_.qualityThreshold) {
                    selected.push_back(idx);
                }
            }
            break;
        }

        case SelectionMethod::Adaptive: {
            // Use mean + stddev to determine threshold
            double sum =
                std::accumulate(qualities.begin(), qualities.end(), 0.0);
            double mean = sum / qualities.size();

            double sqSum = 0.0;
            for (double q : qualities) {
                sqSum += (q - mean) * (q - mean);
            }
            double stdDev = std::sqrt(sqSum / qualities.size());

            double threshold = mean + 0.5 * stdDev;

            for (const auto& [idx, quality] : indexed) {
                if (quality >= threshold) {
                    selected.push_back(idx);
                }
            }

            // Ensure at least 10% selected
            if (selected.size() < qualities.size() / 10) {
                selected.clear();
                size_t minCount = std::max<size_t>(qualities.size() / 10, 1);
                for (size_t i = 0; i < minCount && i < indexed.size(); ++i) {
                    selected.push_back(indexed[i].first);
                }
            }
            break;
        }
    }

    // Sort selected indices for sequential access
    std::sort(selected.begin(), selected.end());

    return selected;
}

std::vector<cv::Mat> LuckyImaging::alignFrames(
    const std::vector<cv::Mat>& frames, const std::vector<size_t>& indices,
    ProgressCallback progressCallback) {
    if (frames.empty()) {
        return {};
    }

    // Use first (best quality) frame as reference
    registrar_->setReferenceFrame(frames[0]);

    return registrar_->registerFrames(
        frames, [&progressCallback](double p, const std::string& msg) {
            if (progressCallback) {
                progressCallback(static_cast<float>(p), msg);
            }
        });
}

cv::Mat LuckyImaging::stackFrames(const std::vector<cv::Mat>& alignedFrames,
                                  ProgressCallback progressCallback) {
    if (alignedFrames.empty()) {
        return cv::Mat();
    }

    return stacker_->stackFrames(alignedFrames);
}

cv::Mat LuckyImaging::applyPostProcessing(const cv::Mat& stacked) const {
    cv::Mat result = stacked.clone();

    if (params_.applyWaveletSharpening) {
        result = applyWaveletSharpening(result, params_.waveletStrength,
                                        params_.waveletLayers);
    }

    if (params_.applyDeconvolution) {
        result = applyDeconvolution(result, params_.psfSigma,
                                    params_.deconvIterations);
    }

    return result;
}

cv::Mat LuckyImaging::createGaussianPSF(double sigma, int size) const {
    cv::Mat psf = cv::Mat::zeros(size, size, CV_32F);

    int center = size / 2;
    double sum = 0.0;

    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            double dx = x - center;
            double dy = y - center;
            double value = std::exp(-(dx * dx + dy * dy) / (2 * sigma * sigma));
            psf.at<float>(y, x) = static_cast<float>(value);
            sum += value;
        }
    }

    // Normalize
    psf /= static_cast<float>(sum);

    return psf;
}

// Utility functions

cv::Mat luckyStack(const std::vector<cv::Mat>& frames, double topPercent) {
    LuckyImaging processor;
    return processor.quickProcess(frames, topPercent);
}

std::string selectionMethodToString(SelectionMethod method) {
    switch (method) {
        case SelectionMethod::Percentage:
            return "Percentage";
        case SelectionMethod::Count:
            return "Count";
        case SelectionMethod::Threshold:
            return "Threshold";
        case SelectionMethod::Adaptive:
            return "Adaptive";
    }
    return "Unknown";
}

SelectionMethod selectionMethodFromString(const std::string& name) {
    if (name == "Percentage")
        return SelectionMethod::Percentage;
    if (name == "Count")
        return SelectionMethod::Count;
    if (name == "Threshold")
        return SelectionMethod::Threshold;
    if (name == "Adaptive")
        return SelectionMethod::Adaptive;
    return SelectionMethod::Percentage;
}

}  // namespace serastro
