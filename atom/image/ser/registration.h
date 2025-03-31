// registration.h
#pragma once

#include "exception.h"
#include "frame_processor.h"

#include <vector>
#include <memory>
#include <string>
#include <opencv2/core.hpp>

namespace serastro {

// Forward declarations
class QualityAssessor;

// Registration method enum
enum class RegistrationMethod {
    PhaseCorrelation,   // FFT-based phase correlation
    FeatureMatching,    // Feature detection and matching
    OpticalFlow,        // Dense optical flow
    ECC,                // Enhanced correlation coefficient
    Template            // Template matching
};

// Registration parameters
struct RegistrationParameters {
    RegistrationMethod method = RegistrationMethod::PhaseCorrelation;
    bool subpixelAlignment = true;
    double pyramidLevel = 3;        // For ECC and optical flow
    int maxIterations = 50;         // For iterative methods
    double terminationEpsilon = 0.001; // Termination criteria
    bool useGPU = false;            // Use GPU acceleration if available
    std::string featureDetector = "AKAZE"; // For feature matching
    int templateSize = 100;         // For template matching
    bool autoSelectReference = true; // Auto-select reference frame
    bool cropToCommonArea = true;   // Crop result to common area
    bool usePolynomialTransform = false; // Use higher-order transforms for distortion
    int polynomialDegree = 2;       // Degree of polynomial transform
};

// Transformation model
struct FrameTransformation {
    enum class Type {
        Translation,    // Translation only
        Rigid,          // Translation + Rotation
        Similarity,     // Translation + Rotation + Scale
        Affine,         // Full affine transform (includes shear)
        Perspective,    // Perspective transform
        Polynomial      // Higher-order polynomial transform
    };
    
    Type type = Type::Translation;
    cv::Mat transform;  // Transformation matrix
    double confidence = 0.0; // Confidence score (0-1)
    
    // Apply transformation to a point
    cv::Point2f apply(const cv::Point2f& pt) const;
    
    // Apply transformation to a frame
    cv::Mat applyToFrame(const cv::Mat& frame, const cv::Size& outputSize = cv::Size()) const;
};

// Frame registrar class
class FrameRegistrar : public CustomizableProcessor {
public:
    FrameRegistrar();
    explicit FrameRegistrar(const RegistrationParameters& params);
    
    // Calculate transformation between frames
    FrameTransformation calculateTransformation(const cv::Mat& frame) const;
    
    // Register frame and return transformation
    std::pair<cv::Mat, FrameTransformation> registerFrame(const cv::Mat& frame) const;
    
    // Register and apply in one step
    cv::Mat registerAndApply(const cv::Mat& frame);
    
    // Set reference frame
    void setReferenceFrame(const cv::Mat& referenceFrame);
    
    // Auto-select reference frame from a set of frames
    void autoSelectReferenceFrame(const std::vector<cv::Mat>& frames);
    
    // Get reference frame
    cv::Mat getReferenceFrame() const;
    
    // Check if reference frame is set
    bool hasReferenceFrame() const;
    
    // Register multiple frames
    std::vector<cv::Mat> registerFrames(const std::vector<cv::Mat>& frames,
                                      const ProgressCallback& progress = nullptr);
    
    // CustomizableProcessor interface implementation
    cv::Mat process(const cv::Mat& frame) override;
    std::string getName() const override;
    void setParameter(const std::string& name, double value) override;
    double getParameter(const std::string& name) const override;
    std::vector<std::string> getParameterNames() const override;
    bool hasParameter(const std::string& name) const override;
    
    // Set/get registration parameters
    void setRegistrationParameters(const RegistrationParameters& params);
    const RegistrationParameters& getRegistrationParameters() const;
    
    // Set quality assessor for reference frame selection
    void setQualityAssessor(std::shared_ptr<QualityAssessor> assessor);
    std::shared_ptr<QualityAssessor> getQualityAssessor() const;

private:
    RegistrationParameters parameters;
    cv::Mat referenceFrame;
    bool hasReference = false;
    std::shared_ptr<QualityAssessor> qualityAssessor;
    
    // Transformation methods
    FrameTransformation calculatePhaseCorrelation(const cv::Mat& frame) const;
    FrameTransformation calculateFeatureMatching(const cv::Mat& frame) const;
    FrameTransformation calculateOpticalFlow(const cv::Mat& frame) const;
    FrameTransformation calculateECC(const cv::Mat& frame) const;
    FrameTransformation calculateTemplateMatching(const cv::Mat& frame) const;
    
    // Helper methods
    cv::Mat prepareFrameForRegistration(const cv::Mat& frame) const;
    cv::Rect calculateCommonArea(const std::vector<FrameTransformation>& transforms,
                               const cv::Size& frameSize) const;
};

} // namespace serastro