#include "stacking.h"
#include "quality.h"

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>

#include <algorithm>
#include <numeric>
#include <stdexcept>

namespace serastro {

using namespace cv;

// =============================
// FrameWeightCalculator
// =============================
std::vector<double> FrameWeightCalculator::calculateWeights(
    const std::vector<cv::Mat>& frames) {
    std::vector<double> weights;
    weights.reserve(frames.size());
    for (const auto& f : frames) {
        weights.push_back(calculateWeight(f));
    }
    return weights;
}

// =============================
// QualityWeightCalculator
// =============================
QualityWeightCalculator::QualityWeightCalculator(
    std::shared_ptr<QualityAssessor> assessor)
    : qualityAssessor(std::move(assessor)) {}

double QualityWeightCalculator::calculateWeight(const cv::Mat& frame) {
    if (!qualityAssessor) {
        // Fallback: use variance as a crude quality measure
        cv::Scalar mean, stddev;
        cv::meanStdDev(frame, mean, stddev);
        return stddev[0] + 1e-6;  // avoid zero
    }
    double score = qualityAssessor->assessQuality(frame);
    return std::max(1e-6, score);
}

std::vector<double> QualityWeightCalculator::calculateWeights(
    const std::vector<cv::Mat>& frames) {
    std::vector<double> weights;
    weights.reserve(frames.size());
    for (const auto& f : frames) {
        weights.push_back(calculateWeight(f));
    }
    return weights;
}

void QualityWeightCalculator::setQualityAssessor(
    std::shared_ptr<QualityAssessor> assessor) {
    qualityAssessor = std::move(assessor);
}

std::shared_ptr<QualityAssessor> QualityWeightCalculator::getQualityAssessor()
    const {
    return qualityAssessor;
}

// =============================
// FrameStacker
// =============================

FrameStacker::FrameStacker() = default;
FrameStacker::FrameStacker(const StackingParameters& params)
    : parameters(params) {}

static cv::Mat toFloatGray(const cv::Mat& in) {
    cv::Mat gray;
    if (in.channels() == 3 || in.channels() == 4) {
        cv::cvtColor(in, gray, cv::COLOR_BGR2GRAY);
    } else {
        gray = in;
    }
    cv::Mat f;
    gray.convertTo(f, CV_32F, 1.0 / 255.0);
    return f;
}

std::vector<cv::Mat> FrameStacker::prepareFrames(
    const std::vector<cv::Mat>& frames) const {
    std::vector<cv::Mat> out;
    out.reserve(frames.size());
    for (const auto& f : frames)
        out.push_back(toFloatGray(f));

    if (parameters.normalizeBeforeStacking) {
        for (auto& f : out) {
            double minV, maxV;
            cv::minMaxLoc(f, &minV, &maxV);
            if (maxV > minV) {
                f = (f - minV) / (maxV - minV);
            }
        }
    }
    return out;
}

cv::Mat FrameStacker::normalizeResult(const cv::Mat& stacked) const {
    if (!parameters.normalizeResult)
        return stacked;
    cv::Mat out;
    double minV, maxV;
    cv::minMaxLoc(stacked, &minV, &maxV);
    if (maxV > minV) {
        out = (stacked - minV) / (maxV - minV);
    } else {
        out = stacked.clone();
    }
    return out;
}

cv::Mat FrameStacker::stackMean(const std::vector<cv::Mat>& frames) const {
    if (frames.empty())
        return cv::Mat();
    cv::Mat acc = cv::Mat::zeros(frames[0].size(), CV_32F);
    for (const auto& f : frames)
        acc += f;
    acc /= static_cast<float>(frames.size());
    return normalizeResult(acc);
}

cv::Mat FrameStacker::stackMedian(const std::vector<cv::Mat>& frames) const {
    if (frames.empty())
        return cv::Mat();
    int rows = frames[0].rows, cols = frames[0].cols;
    cv::Mat out(rows, cols, CV_32F);

    // Simple per-pixel median (could be optimized)
    std::vector<float> buf;
    buf.reserve(frames.size());
    for (int y = 0; y < rows; ++y) {
        for (int x = 0; x < cols; ++x) {
            buf.clear();
            for (const auto& f : frames)
                buf.push_back(f.at<float>(y, x));
            std::nth_element(buf.begin(), buf.begin() + buf.size() / 2,
                             buf.end());
            out.at<float>(y, x) = buf[buf.size() / 2];
        }
    }
    return normalizeResult(out);
}

cv::Mat FrameStacker::stackMaximum(const std::vector<cv::Mat>& frames) const {
    if (frames.empty())
        return cv::Mat();
    cv::Mat out = frames[0].clone();
    for (size_t i = 1; i < frames.size(); ++i)
        cv::max(out, frames[i], out);
    return normalizeResult(out);
}

cv::Mat FrameStacker::stackMinimum(const std::vector<cv::Mat>& frames) const {
    if (frames.empty())
        return cv::Mat();
    cv::Mat out = frames[0].clone();
    for (size_t i = 1; i < frames.size(); ++i)
        cv::min(out, frames[i], out);
    return normalizeResult(out);
}

cv::Mat FrameStacker::stackSigmaClipping(
    const std::vector<cv::Mat>& frames) const {
    if (frames.empty())
        return cv::Mat();
    const int rows = frames[0].rows, cols = frames[0].cols;
    const int N = static_cast<int>(frames.size());

    cv::Mat out(rows, cols, CV_32F, cv::Scalar(0));

    std::vector<float> buf(N);
    for (int y = 0; y < rows; ++y) {
        for (int x = 0; x < cols; ++x) {
            for (int i = 0; i < N; ++i)
                buf[i] = frames[i].at<float>(y, x);

            // Iterative sigma clipping
            std::vector<float> cur = buf;
            for (int it = 0; it < parameters.iterations && cur.size() > 2;
                 ++it) {
                double mean =
                    std::accumulate(cur.begin(), cur.end(), 0.0) / cur.size();
                double var = 0.0;
                for (float v : cur)
                    var += (v - mean) * (v - mean);
                double stddev = std::sqrt(var / cur.size());

                double low = mean - parameters.sigmaLow * stddev;
                double high = mean + parameters.sigmaHigh * stddev;

                std::vector<float> next;
                next.reserve(cur.size());
                for (float v : cur)
                    if (v >= low && v <= high)
                        next.push_back(v);
                if (next.size() == cur.size())
                    break;  // converged
                cur.swap(next);
            }

            if (cur.empty()) {
                out.at<float>(y, x) = 0.0f;
            } else {
                float sum = std::accumulate(cur.begin(), cur.end(), 0.0f);
                out.at<float>(y, x) = sum / static_cast<float>(cur.size());
            }
        }
    }

    return normalizeResult(out);
}

cv::Mat FrameStacker::stackWeightedAverage(
    const std::vector<cv::Mat>& frames,
    const std::vector<double>& weights) const {
    if (frames.empty())
        return cv::Mat();
    double wsum = std::accumulate(weights.begin(), weights.end(), 0.0);
    if (wsum <= 0.0)
        return stackMean(frames);

    cv::Mat acc = cv::Mat::zeros(frames[0].size(), CV_32F);
    for (size_t i = 0; i < frames.size(); ++i) {
        acc += frames[i] * static_cast<float>(weights[i]);
    }
    acc /= static_cast<float>(wsum);
    return normalizeResult(acc);
}

cv::Mat FrameStacker::stackFrames(const std::vector<cv::Mat>& frames) {
    if (frames.empty())
        return cv::Mat();

    auto prepared = prepareFrames(frames);

    switch (parameters.method) {
        case StackingMethod::Mean:
            return stackMean(prepared);
        case StackingMethod::Median:
            return stackMedian(prepared);
        case StackingMethod::MaximumValue:
            return stackMaximum(prepared);
        case StackingMethod::MinimumValue:
            return stackMinimum(prepared);
        case StackingMethod::SigmaClipping:
            return stackSigmaClipping(prepared);
        case StackingMethod::WeightedAverage: {
            if (!parameters.weightCalculator) {
                parameters.weightCalculator =
                    std::make_shared<QualityWeightCalculator>();
            }
            auto weights =
                parameters.weightCalculator->calculateWeights(frames);
            return stackWeightedAverage(prepared, weights);
        }
        default:
            return stackMean(prepared);
    }
}

cv::Mat FrameStacker::stackFramesWithWeights(
    const std::vector<cv::Mat>& frames, const std::vector<double>& weights) {
    if (frames.empty())
        return cv::Mat();
    auto prepared = prepareFrames(frames);
    return stackWeightedAverage(prepared, weights);
}

cv::Mat FrameStacker::process(const cv::Mat& frame) {
    addFrameToBuffer(frame);
    if (frameBuffer.size() >= std::min<size_t>(5, maxBufferSize)) {
        auto result = stackFrames(frameBuffer);
        frameBuffer.clear();
        return result;
    }
    return frame.clone();
}

std::string FrameStacker::getName() const { return "FrameStacker"; }

void FrameStacker::setParameter(const std::string& name, double value) {
    if (name == "sigmaLow")
        parameters.sigmaLow = value;
    else if (name == "sigmaHigh")
        parameters.sigmaHigh = value;
    else if (name == "iterations")
        parameters.iterations = static_cast<int>(value);
    else if (name == "normalizeBefore")
        parameters.normalizeBeforeStacking = (value != 0.0);
    else if (name == "normalizeResult")
        parameters.normalizeResult = (value != 0.0);
}

double FrameStacker::getParameter(const std::string& name) const {
    if (name == "sigmaLow")
        return parameters.sigmaLow;
    if (name == "sigmaHigh")
        return parameters.sigmaHigh;
    if (name == "iterations")
        return parameters.iterations;
    if (name == "normalizeBefore")
        return parameters.normalizeBeforeStacking ? 1.0 : 0.0;
    if (name == "normalizeResult")
        return parameters.normalizeResult ? 1.0 : 0.0;
    return 0.0;
}

std::vector<std::string> FrameStacker::getParameterNames() const {
    return {"sigmaLow", "sigmaHigh", "iterations", "normalizeBefore",
            "normalizeResult"};
}

bool FrameStacker::hasParameter(const std::string& name) const {
    auto names = getParameterNames();
    return std::find(names.begin(), names.end(), name) != names.end();
}

void FrameStacker::setStackingParameters(const StackingParameters& params) {
    parameters = params;
}

const StackingParameters& FrameStacker::getStackingParameters() const {
    return parameters;
}

void FrameStacker::setWeightCalculator(
    std::shared_ptr<FrameWeightCalculator> calculator) {
    parameters.weightCalculator = std::move(calculator);
}

std::shared_ptr<FrameWeightCalculator> FrameStacker::getWeightCalculator()
    const {
    return parameters.weightCalculator;
}

void FrameStacker::addFrameToBuffer(const cv::Mat& frame) {
    if (frameBuffer.size() >= maxBufferSize)
        frameBuffer.erase(frameBuffer.begin());
    frameBuffer.push_back(frame.clone());
}

void FrameStacker::clearBuffer() { frameBuffer.clear(); }

size_t FrameStacker::getBufferSize() const { return frameBuffer.size(); }

void FrameStacker::setMaxBufferSize(size_t size) { maxBufferSize = size; }

size_t FrameStacker::getMaxBufferSize() const { return maxBufferSize; }

}  // namespace serastro
