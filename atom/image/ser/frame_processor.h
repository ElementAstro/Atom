// frame_processor.h
#pragma once

#include "exception.h"

#include <vector>
#include <memory>
#include <functional>
#include <string>
#include <concepts>
#include <unordered_map>
#include <opencv2/core.hpp>

namespace serastro {

// Progress callback type
using ProgressCallback = std::function<void(double progress, const std::string& message)>;

// Base class for all frame processors
class FrameProcessor {
public:
    virtual ~FrameProcessor() = default;

    // Process a single frame
    virtual cv::Mat process(const cv::Mat& frame) = 0;

    // Process multiple frames with optional progress callback
    virtual std::vector<cv::Mat> process(const std::vector<cv::Mat>& frames,
                                       const ProgressCallback& progress = nullptr);

    // Get processor name
    virtual std::string getName() const = 0;

    // Allow cancellation of multi-frame processing
    void requestCancel() { cancelRequested = true; }
    bool isCancelled() const { return cancelRequested; }
    void resetCancel() { cancelRequested = false; }

protected:
    bool cancelRequested = false;
};

// Interface for customizable processors
class CustomizableProcessor : public FrameProcessor {
public:
    // Set parameter by name
    virtual void setParameter(const std::string& name, double value) = 0;

    // Get parameter value
    virtual double getParameter(const std::string& name) const = 0;

    // Get all parameter names
    virtual std::vector<std::string> getParameterNames() const = 0;

    // Check if parameter exists
    virtual bool hasParameter(const std::string& name) const = 0;

    // Set multiple parameters
    virtual void setParameters(const std::unordered_map<std::string, double>& params);

    // Get all parameters as a map
    virtual std::unordered_map<std::string, double> getParameters() const;
};

// Helper base class for implementing customizable processors
class BaseCustomizableProcessor : public CustomizableProcessor {
public:
    void setParameter(const std::string& name, double value) override;
    double getParameter(const std::string& name) const override;
    std::vector<std::string> getParameterNames() const override;
    bool hasParameter(const std::string& name) const override;

protected:
    std::unordered_map<std::string, double> parameters;

    // Register a parameter with initial value
    void registerParameter(const std::string& name, double initialValue);
};

// Processing pipeline that combines multiple processors
class ProcessingPipeline : public FrameProcessor {
public:
    ProcessingPipeline();

    cv::Mat process(const cv::Mat& frame) override;
    std::vector<cv::Mat> process(const std::vector<cv::Mat>& frames,
                               const ProgressCallback& progress = nullptr) override;
    std::string getName() const override;

    // Add processor to the pipeline
    void addProcessor(std::shared_ptr<FrameProcessor> processor);

    // Remove processor by index
    void removeProcessor(size_t index);

    // Get all processors
    std::vector<std::shared_ptr<FrameProcessor>> getProcessors() const;

    // Clear all processors
    void clear();

private:
    std::vector<std::shared_ptr<FrameProcessor>> processors;
};

} // namespace serastro
