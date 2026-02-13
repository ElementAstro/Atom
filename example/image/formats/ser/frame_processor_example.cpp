/**
 * @file frame_processor_example.cpp
 * @brief Example demonstrating SER frame processing capabilities
 *
 * This example covers:
 * - Creating custom frame processors
 * - Processing single frames
 * - Batch processing multiple frames
 * - Building processing pipelines
 * - Progress tracking and cancellation
 * - Customizable processor parameters
 *
 * @author Atom Image Module
 * @date 2025
 */

#include <iostream>
#include <memory>
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>

#include "atom/image/formats/ser/ser.hpp"

using namespace serastro;
using namespace std;

/**
 * @brief Simple brightness adjustment processor
 */
class BrightnessProcessor : public BaseCustomizableProcessor {
public:
    BrightnessProcessor() { registerParameter("brightness", 1.0); }

    cv::Mat process(const cv::Mat& frame) override {
        double brightness = getParameter("brightness");
        cv::Mat result;
        frame.convertTo(result, -1, brightness, 0);
        return result;
    }

    string getName() const override { return "BrightnessProcessor"; }
};

/**
 * @brief Demonstrate basic frame processing
 */
void demonstrateBasicProcessing() {
    cout << "\n=== Basic Frame Processing ===\n";

    try {
        // Create test frame
        cv::Mat frame(480, 640, CV_8UC1, cv::Scalar(100));

        // Create processor
        auto processor = make_shared<BrightnessProcessor>();
        processor->setParameter("brightness", 1.5);

        // Process frame
        cv::Mat result = processor->process(frame);

        cout << "Original mean: " << cv::mean(frame)[0] << "\n";
        cout << "Processed mean: " << cv::mean(result)[0] << "\n";
        cout << "Processor: " << processor->getName() << "\n";

    } catch (const exception& e) {
        cerr << "Error: " << e.what() << "\n";
    }
}

/**
 * @brief Demonstrate batch processing with progress
 */
void demonstrateBatchProcessing() {
    cout << "\n=== Batch Processing with Progress ===\n";

    try {
        // Create test frames
        vector<cv::Mat> frames;
        for (int i = 0; i < 10; ++i) {
            frames.push_back(
                cv::Mat(480, 640, CV_8UC1, cv::Scalar(100 + i * 10)));
        }

        // Create processor
        auto processor = make_shared<BrightnessProcessor>();
        processor->setParameter("brightness", 1.2);

        // Process with progress callback
        auto progressCallback = [](double progress, const string& message) {
            cout << "Progress: " << int(progress * 100) << "% - " << message
                 << "\n";
        };

        auto results = processor->process(frames, progressCallback);

        cout << "Processed " << results.size() << " frames\n";

    } catch (const exception& e) {
        cerr << "Error: " << e.what() << "\n";
    }
}

/**
 * @brief Demonstrate processing pipeline
 */
void demonstrateProcessingPipeline() {
    cout << "\n=== Processing Pipeline ===\n";

    try {
        // Create test frame
        cv::Mat frame(480, 640, CV_8UC1, cv::Scalar(100));

        // Create pipeline
        ProcessingPipeline pipeline;

        // Add processors
        auto brightness = make_shared<BrightnessProcessor>();
        brightness->setParameter("brightness", 1.5);
        pipeline.addProcessor(brightness);

        // Process through pipeline
        cv::Mat result = pipeline.process(frame);

        cout << "Pipeline: " << pipeline.getName() << "\n";
        cout << "Processors in pipeline: " << pipeline.getProcessors().size()
             << "\n";
        cout << "Original mean: " << cv::mean(frame)[0] << "\n";
        cout << "Final mean: " << cv::mean(result)[0] << "\n";

    } catch (const exception& e) {
        cerr << "Error: " << e.what() << "\n";
    }
}

/**
 * @brief Demonstrate parameter management
 */
void demonstrateParameterManagement() {
    cout << "\n=== Parameter Management ===\n";

    try {
        auto processor = make_shared<BrightnessProcessor>();

        // Get parameter names
        auto paramNames = processor->getParameterNames();
        cout << "Available parameters:\n";
        for (const auto& name : paramNames) {
            cout << "  " << name << " = " << processor->getParameter(name)
                 << "\n";
        }

        // Set multiple parameters
        unordered_map<string, double> params = {{"brightness", 2.0}};
        processor->setParameters(params);

        cout << "\nAfter setting parameters:\n";
        auto allParams = processor->getParameters();
        for (const auto& [name, value] : allParams) {
            cout << "  " << name << " = " << value << "\n";
        }

    } catch (const exception& e) {
        cerr << "Error: " << e.what() << "\n";
    }
}

int main() {
    demonstrateBasicProcessing();
    demonstrateBatchProcessing();
    demonstrateProcessingPipeline();
    demonstrateParameterManagement();

    return 0;
}
