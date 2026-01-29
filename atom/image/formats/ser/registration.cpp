#include "registration.h"
#include "frame_processor.h"
#include "quality.h"

#include <opencv2/calib3d.hpp>
#include <opencv2/features2d.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/video/tracking.hpp>

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace serastro {

using Mat = cv::Mat;
using namespace cv;

// =============================
// FrameTransformation methods
// =============================
cv::Point2f FrameTransformation::apply(const cv::Point2f& pt) const {
    if (transform.empty())
        return pt;

    if (type == Type::Perspective && transform.rows == 3 &&
        transform.cols == 3) {
        Mat src(3, 1, CV_64F);
        src.at<double>(0, 0) = pt.x;
        src.at<double>(1, 0) = pt.y;
        src.at<double>(2, 0) = 1.0;
        Mat dst = transform * src;
        double w = dst.at<double>(2, 0);
        if (w == 0.0)
            w = 1.0;
        return Point2f(static_cast<float>(dst.at<double>(0, 0) / w),
                       static_cast<float>(dst.at<double>(1, 0) / w));
    }

    if (transform.rows == 2 && transform.cols == 3) {
        double x = transform.at<double>(0, 0) * pt.x +
                   transform.at<double>(0, 1) * pt.y +
                   transform.at<double>(0, 2);
        double y = transform.at<double>(1, 0) * pt.x +
                   transform.at<double>(1, 1) * pt.y +
                   transform.at<double>(1, 2);
        return Point2f(static_cast<float>(x), static_cast<float>(y));
    }

    // Fallback
    return pt;
}

Mat FrameTransformation::applyToFrame(const Mat& frame,
                                      const Size& outputSize) const {
    if (transform.empty())
        return frame.clone();

    Size out = outputSize.area() > 0 ? outputSize : frame.size();

    if (type == Type::Perspective && transform.rows == 3 &&
        transform.cols == 3) {
        Mat result;
        Mat H;
        transform.convertTo(H, CV_64F);
        warpPerspective(frame, result, H, out, INTER_LINEAR, BORDER_REPLICATE);
        return result;
    }

    if (transform.rows == 2 && transform.cols == 3) {
        Mat result;
        Mat A;
        transform.convertTo(A, CV_64F);
        warpAffine(frame, result, A, out, INTER_LINEAR, BORDER_REPLICATE);
        return result;
    }

    return frame.clone();
}

// =============================
// Utilities
// =============================
namespace {

Mat toGrayFloat(const Mat& in) {
    Mat gray;
    if (in.channels() == 3 || in.channels() == 4) {
        cvtColor(in, gray, COLOR_BGR2GRAY);
    } else {
        gray = in;
    }
    gray.convertTo(gray, CV_32F, 1.0 / 255.0);
    return gray;
}

Mat hannWindowLike(const Size& size) {
    Mat hannX = Mat::zeros(1, size.width, CV_32F);
    Mat hannY = Mat::zeros(size.height, 1, CV_32F);
    createHanningWindow(hannX, hannX.size(), CV_32F);
    createHanningWindow(hannY, hannY.size(), CV_32F);
    Mat hann = hannY * hannX;
    return hann;
}

Mat ensureGray8(const Mat& in) {
    Mat gray;
    if (in.channels() == 3 || in.channels() == 4) {
        cvtColor(in, gray, COLOR_BGR2GRAY);
    } else {
        gray = in;
    }
    if (gray.depth() != CV_8U) {
        gray.convertTo(gray, CV_8U, 255.0);
    }
    return gray;
}

}  // namespace

// =============================
// FrameRegistrar methods
// =============================

FrameRegistrar::FrameRegistrar() = default;
FrameRegistrar::FrameRegistrar(const RegistrationParameters& params)
    : parameters(params) {}

FrameTransformation FrameRegistrar::calculateTransformation(
    const Mat& frame) const {
    if (!hasReference) {
        // No reference; identity transformation
        FrameTransformation tf;
        tf.type = FrameTransformation::Type::Translation;
        tf.transform = (Mat_<double>(2, 3) << 1, 0, 0, 0, 1, 0);
        tf.confidence = 1.0;
        return tf;
    }

    switch (parameters.method) {
        case RegistrationMethod::PhaseCorrelation:
            return calculatePhaseCorrelation(frame);
        case RegistrationMethod::FeatureMatching:
            return calculateFeatureMatching(frame);
        case RegistrationMethod::OpticalFlow:
            return calculateOpticalFlow(frame);
        case RegistrationMethod::ECC:
            return calculateECC(frame);
        case RegistrationMethod::Template:
            return calculateTemplateMatching(frame);
        default: {
            FrameTransformation tf;
            tf.type = FrameTransformation::Type::Translation;
            tf.transform = (Mat_<double>(2, 3) << 1, 0, 0, 0, 1, 0);
            tf.confidence = 0.0;
            return tf;
        }
    }
}

std::pair<Mat, FrameTransformation> FrameRegistrar::registerFrame(
    const Mat& frame) const {
    FrameTransformation tf = calculateTransformation(frame);
    Mat aligned = tf.applyToFrame(frame, referenceFrame.size());
    return {aligned, tf};
}

Mat FrameRegistrar::registerAndApply(const Mat& frame) {
    return registerFrame(frame).first;
}

void FrameRegistrar::setReferenceFrame(const Mat& referenceFrame_) {
    referenceFrame = ensureGray8(referenceFrame_);
    hasReference = !referenceFrame.empty();
}

void FrameRegistrar::autoSelectReferenceFrame(const std::vector<Mat>& frames) {
    if (frames.empty()) {
        hasReference = false;
        referenceFrame.release();
        return;
    }

    if (parameters.autoSelectReference && qualityAssessor) {
        // Pick frame with best quality
        double bestScore = -1.0;
        size_t bestIdx = 0;
        for (size_t i = 0; i < frames.size(); ++i) {
            double score = qualityAssessor->assessQuality(frames[i]);
            if (score > bestScore) {
                bestScore = score;
                bestIdx = i;
            }
        }
        setReferenceFrame(frames[bestIdx]);
    } else {
        setReferenceFrame(frames[0]);
    }
}

Mat FrameRegistrar::getReferenceFrame() const { return referenceFrame; }

bool FrameRegistrar::hasReferenceFrame() const { return hasReference; }

std::vector<Mat> FrameRegistrar::registerFrames(
    const std::vector<Mat>& frames, const ProgressCallback& progress) {
    std::vector<Mat> out;
    out.reserve(frames.size());

    if (!hasReference) {
        if (!frames.empty())
            setReferenceFrame(frames.front());
    }

    for (size_t i = 0; i < frames.size(); ++i) {
        auto [aligned, tf] = registerFrame(frames[i]);
        out.push_back(aligned);
        if (progress) {
            progress((i + 1.0) / frames.size(), "Registered frame");
        }
        if (cancelRequested)
            break;
    }
    return out;
}

Mat FrameRegistrar::process(const Mat& frame) {
    return registerAndApply(frame);
}

std::string FrameRegistrar::getName() const { return "FrameRegistrar"; }

void FrameRegistrar::setParameter(const std::string& name, double value) {
    if (name == "pyramidLevel")
        parameters.pyramidLevel = value;
    else if (name == "maxIterations")
        parameters.maxIterations = static_cast<int>(value);
    else if (name == "terminationEpsilon")
        parameters.terminationEpsilon = value;
    else if (name == "subpixelAlignment")
        parameters.subpixelAlignment = (value != 0.0);
    else if (name == "useGPU")
        parameters.useGPU = (value != 0.0);
    else if (name == "polynomialDegree")
        parameters.polynomialDegree = static_cast<int>(value);
}

double FrameRegistrar::getParameter(const std::string& name) const {
    if (name == "pyramidLevel")
        return parameters.pyramidLevel;
    if (name == "maxIterations")
        return parameters.maxIterations;
    if (name == "terminationEpsilon")
        return parameters.terminationEpsilon;
    if (name == "subpixelAlignment")
        return parameters.subpixelAlignment ? 1.0 : 0.0;
    if (name == "useGPU")
        return parameters.useGPU ? 1.0 : 0.0;
    if (name == "polynomialDegree")
        return parameters.polynomialDegree;
    return 0.0;
}

std::vector<std::string> FrameRegistrar::getParameterNames() const {
    return {"pyramidLevel",      "maxIterations", "terminationEpsilon",
            "subpixelAlignment", "useGPU",        "polynomialDegree"};
}

bool FrameRegistrar::hasParameter(const std::string& name) const {
    auto names = getParameterNames();
    return std::find(names.begin(), names.end(), name) != names.end();
}

void FrameRegistrar::setRegistrationParameters(
    const RegistrationParameters& params) {
    parameters = params;
}

const RegistrationParameters& FrameRegistrar::getRegistrationParameters()
    const {
    return parameters;
}

void FrameRegistrar::setQualityAssessor(
    std::shared_ptr<QualityAssessor> assessor) {
    qualityAssessor = std::move(assessor);
}

std::shared_ptr<QualityAssessor> FrameRegistrar::getQualityAssessor() const {
    return qualityAssessor;
}

// =============================
// Private methods (algorithms)
// =============================

FrameTransformation FrameRegistrar::calculatePhaseCorrelation(
    const Mat& frame) const {
    Mat refF = toGrayFloat(referenceFrame);
    Mat curF = toGrayFloat(frame);

    Mat window = hannWindowLike(refF.size());
    Mat refWin, curWin;
    multiply(refF, window, refWin);
    multiply(curF, window, curWin);

    double response = 0.0;
    Point2d shift = phaseCorrelate(curWin, refWin, noArray(), &response);
    // phaseCorrelate returns shift to align src with template; we'll create
    // affine with that shift
    Mat A = (Mat_<double>(2, 3) << 1, 0, shift.x, 0, 1, shift.y);

    FrameTransformation tf;
    tf.type = FrameTransformation::Type::Translation;
    tf.transform = A;
    tf.confidence = std::max(0.0, std::min(1.0, response));
    return tf;
}

FrameTransformation FrameRegistrar::calculateTemplateMatching(
    const Mat& frame) const {
    Mat ref = ensureGray8(referenceFrame);
    Mat cur = ensureGray8(frame);

    // Use the entire current frame to match within a padded reference if sizes
    // differ
    Mat result;
    int matchMethod = TM_CCORR_NORMED;

    // For robustness, use a central template from current frame
    int tw = std::min(cur.cols / 2, 200);
    int th = std::min(cur.rows / 2, 200);
    Rect templR((cur.cols - tw) / 2, (cur.rows - th) / 2, tw, th);
    Mat templ = cur(templR);

    matchTemplate(ref, templ, result, matchMethod);
    double minVal, maxVal;
    Point minLoc, maxLoc;
    minMaxLoc(result, &minVal, &maxVal, &minLoc, &maxLoc);
    Point best = maxLoc;

    // Compute translation from template center to best location
    Point2f templCenter(static_cast<float>(templR.x + templR.width / 2.0f),
                        static_cast<float>(templR.y + templR.height / 2.0f));
    Point2f bestCenter(static_cast<float>(best.x + templR.width / 2.0f),
                       static_cast<float>(best.y + templR.height / 2.0f));
    Point2f shift(bestCenter.x - templCenter.x, bestCenter.y - templCenter.y);

    Mat A = (Mat_<double>(2, 3) << 1, 0, shift.x, 0, 1, shift.y);

    FrameTransformation tf;
    tf.type = FrameTransformation::Type::Translation;
    tf.transform = A;
    tf.confidence = std::max(0.0, std::min(1.0, maxVal));
    return tf;
}

FrameTransformation FrameRegistrar::calculateECC(const Mat& frame) const {
    Mat refGray = toGrayFloat(referenceFrame);
    Mat curGray = toGrayFloat(frame);

    int warpMode = MOTION_TRANSLATION;  // Keep simple and robust
    Mat warpMatrix = Mat::eye(2, 3, CV_32F);

    TermCriteria criteria(
        TermCriteria::COUNT + TermCriteria::EPS,
        parameters.maxIterations > 0 ? parameters.maxIterations : 50,
        parameters.terminationEpsilon > 0 ? parameters.terminationEpsilon
                                          : 1e-4);

    double cc = 0.0;
    try {
        cc = findTransformECC(refGray, curGray, warpMatrix, warpMode, criteria);
    } catch (...) {
        // Fallback if ECC fails
        return calculatePhaseCorrelation(frame);
    }

    Mat A;
    warpMatrix.convertTo(A, CV_64F);

    FrameTransformation tf;
    tf.type = FrameTransformation::Type::Translation;
    tf.transform = Mat(2, 3, CV_64F);
    A.copyTo(tf.transform);
    tf.confidence = std::max(0.0, std::min(1.0, cc));
    return tf;
}

FrameTransformation FrameRegistrar::calculateFeatureMatching(
    const Mat& frame) const {
    Mat ref = ensureGray8(referenceFrame);
    Mat cur = ensureGray8(frame);

    // Use AKAZE if available, otherwise ORB
    Ptr<Feature2D> detector;
    try {
        detector = AKAZE::create();
    } catch (...) {
        detector = ORB::create();
    }

    std::vector<KeyPoint> k1, k2;
    Mat d1, d2;
    detector->detectAndCompute(ref, noArray(), k1, d1);
    detector->detectAndCompute(cur, noArray(), k2, d2);

    if (d1.empty() || d2.empty()) {
        return calculatePhaseCorrelation(frame);
    }

    BFMatcher matcher(NORM_HAMMING, true);
    std::vector<DMatch> matches;
    matcher.match(d1, d2, matches);

    if (matches.size() < 8) {
        return calculatePhaseCorrelation(frame);
    }

    std::vector<Point2f> p1, p2;
    p1.reserve(matches.size());
    p2.reserve(matches.size());
    for (const auto& m : matches) {
        p1.push_back(k1[m.queryIdx].pt);
        p2.push_back(k2[m.trainIdx].pt);
    }

    Mat H = estimateAffinePartial2D(p2, p1);  // map current->reference
    if (H.empty()) {
        return calculatePhaseCorrelation(frame);
    }

    Mat A;
    H.convertTo(A, CV_64F);

    FrameTransformation tf;
    tf.type = FrameTransformation::Type::Similarity;
    tf.transform = A;
    tf.confidence = 0.8;  // heuristic
    return tf;
}

FrameTransformation FrameRegistrar::calculateOpticalFlow(
    const Mat& frame) const {
    // Simplified: fall back to phase correlation for robustness
    return calculatePhaseCorrelation(frame);
}

Mat FrameRegistrar::prepareFrameForRegistration(const Mat& frame) const {
    return ensureGray8(frame);
}

cv::Rect FrameRegistrar::calculateCommonArea(
    const std::vector<FrameTransformation>&, const cv::Size& frameSize) const {
    // Simplified: return full frame
    return cv::Rect(0, 0, frameSize.width, frameSize.height);
}

}  // namespace serastro
