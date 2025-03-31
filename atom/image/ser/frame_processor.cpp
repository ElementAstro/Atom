// frame_processor.cpp
#include "frame_processor.h"
#include <algorithm>

namespace serastro {

// Process multiple frames with progress reporting
std::vector<cv::Mat> FrameProcessor::process(const std::vector<cv::Mat>& frames,
                                          const ProgressCallback& progress) {
    std::vector<cv::Mat> results;
    results.reserve(frames.size());
    
    cancelRequested = false;
    
    for (size_t i = 0; i < frames.size(); ++i) {
        if (cancelRequested) {
            break;
        }
        
        results.push_back(process(frames[i]));
        
        if (progress) {
            double progressValue = static_cast<double>(i + 1) / frames.size();
            progress(progressValue, std::format("{}: Processing frame {}/{}", 
                                              getName(), i + 1, frames.size()));
        }
    }
    
    return results;
}

// Set multiple parameters at once
void CustomizableProcessor::setParameters(const std::unordered_map<std::string, double>& params) {
    for (const auto& [name, value] : params) {
        if (hasParameter(name)) {
            setParameter(name, value);
        }
    }
}

// Get all parameters as a map
std::unordered_map<std::string, double> CustomizableProcessor::getParameters() const {
    std::unordered_map<std::string, double> result;
    for (const auto& name : getParameterNames()) {
        result[name] = getParameter(name);
    }
    return result;
}

// BaseCustomizableProcessor implementation
void BaseCustomizableProcessor::setParameter(const std::string& name, double value) {
    if (!hasParameter(name)) {
        throw InvalidParameterException(std::format("Unknown parameter: {}", name));
    }
    parameters[name] = value;
}

double BaseCustomizableProcessor::getParameter(const std::string& name) const {
    if (!hasParameter(name)) {
        throw InvalidParameterException(std::format("Unknown parameter: {}", name));
    }
    return parameters.at(name);
}

std::vector<std::string> BaseCustomizableProcessor::getParameterNames() const {
    std::vector<std::string> names;
    names.reserve(parameters.size());
    for (const auto& [name, _] : parameters) {
        names.push_back(name);
    }
    return names;
}

bool BaseCustomizableProcessor::hasParameter(const std::string& name) const {
    return parameters.find(name) != parameters.end();
}

void BaseCustomizableProcessor::registerParameter(const std::string& name, double initialValue) {
    parameters[name] = initialValue;
}

// ProcessingPipeline implementation
ProcessingPipeline::ProcessingPipeline() = default;

cv::Mat ProcessingPipeline::process(const cv::Mat& frame) {
    cv::Mat result = frame.clone();
    
    for (auto& processor : processors) {
        if (cancelRequested) {
            break;
        }
        
        result = processor->process(result);
    }
    
    return result;
}

std::vector<cv::Mat> ProcessingPipeline::process(const std::vector<cv::Mat>& frames,
                                              const ProgressCallback& progress) {
    std::vector<cv::Mat> results = frames;
    
    cancelRequested = false;
    
    for (size_t i = 0; i < processors.size(); ++i) {
        if (cancelRequested) {
            break;
        }
        
        auto& processor = processors[i];
        
        if (progress) {
            progress(static_cast<double>(i) / processors.size(),
                   std::format("Running processor {}/{}: {}", 
                              i + 1, processors.size(), processor->getName()));
        }
        
        // Create a wrapper progress function that scales appropriately
        ProgressCallback processorProgress = nullptr;
        if (progress) {
            processorProgress = [&progress, i, total = processors.size()]
                              (double subProgress, const std::string& message) {
                double overallProgress = (i + subProgress) / total;
                progress(overallProgress, message);
            };
        }
        
        results = processor->process(results, processorProgress);
        
        if (processor->isCancelled()) {
            cancelRequested = true;
        }
    }
    
    return results;
}

std::string ProcessingPipeline::getName() const {
    return "Processing Pipeline";
}

void ProcessingPipeline::addProcessor(std::shared_ptr<FrameProcessor> processor) {
    if (!processor) {
        throw InvalidParameterException("Cannot add null processor to pipeline");
    }
    processors.push_back(std::move(processor));
}

void ProcessingPipeline::removeProcessor(size_t index) {
    if (index >= processors.size()) {
        throw InvalidParameterException::outOfRange("index", index, 0, processors.size() - 1);
    }
    processors.erase(processors.begin() + index);
}

std::vector<std::shared_ptr<FrameProcessor>> ProcessingPipeline::getProcessors() const {
    return processors;
}

void ProcessingPipeline::clear() {
    processors.clear();
}

} // namespace serastro