/*
 * ml_processing.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/**
 * @file ml_processing.cpp
 * @brief Machine learning-based image processing example
 * 
 * This example demonstrates:
 * - Image classification
 * - Feature extraction for ML
 * - Data preprocessing
 * - Model inference simulation
 * - Performance metrics
 */

#include <iostream>
#include <vector>
#include <string>
#include <random>
#include <algorithm>
#include <numeric>
#include <cmath>

using namespace std;

/**
 * @brief Image feature vector
 */
struct ImageFeatures {
    vector<float> histogram;
    vector<float> texture_features;
    vector<float> shape_features;
    string label;
    
    ImageFeatures() {
        histogram.resize(256, 0.0f);
        texture_features.resize(8, 0.0f);
        shape_features.resize(4, 0.0f);
    }
};

/**
 * @brief Feature extractor class
 */
class FeatureExtractor {
public:
    /**
     * @brief Extract histogram features
     */
    static vector<float> extractHistogram(int width, int height, const string& image_type) {
        cout << "Extracting histogram features from " << image_type << " image (" 
             << width << "x" << height << ")..." << endl;
        
        vector<float> histogram(256, 0.0f);
        
        // Simulate histogram extraction
        random_device rd;
        mt19937 gen(rd());
        
        if (image_type == "bright") {
            // Bright images have higher values
            normal_distribution<float> dist(180.0f, 30.0f);
            for (int i = 0; i < 256; ++i) {
                histogram[i] = max(0.0f, dist(gen));
            }
        } else if (image_type == "dark") {
            // Dark images have lower values
            normal_distribution<float> dist(80.0f, 25.0f);
            for (int i = 0; i < 256; ++i) {
                histogram[i] = max(0.0f, dist(gen));
            }
        } else {
            // Normal distribution
            normal_distribution<float> dist(128.0f, 40.0f);
            for (int i = 0; i < 256; ++i) {
                histogram[i] = max(0.0f, dist(gen));
            }
        }
        
        // Normalize histogram
        float sum = accumulate(histogram.begin(), histogram.end(), 0.0f);
        if (sum > 0) {
            for (auto& val : histogram) {
                val /= sum;
            }
        }
        
        cout << "  Extracted 256-bin histogram" << endl;
        return histogram;
    }
    
    /**
     * @brief Extract texture features (simulated GLCM features)
     */
    static vector<float> extractTextureFeatures(int width, int height, const string& texture_type) {
        cout << "Extracting texture features (" << texture_type << ")..." << endl;
        
        vector<float> features(8);
        random_device rd;
        mt19937 gen(rd());
        
        if (texture_type == "smooth") {
            // Smooth textures have low contrast, high homogeneity
            features[0] = 0.1f + (gen() % 100) / 1000.0f; // Contrast
            features[1] = 0.8f + (gen() % 100) / 1000.0f; // Homogeneity
            features[2] = 0.9f + (gen() % 100) / 1000.0f; // Energy
            features[3] = 0.2f + (gen() % 100) / 1000.0f; // Entropy
        } else if (texture_type == "rough") {
            // Rough textures have high contrast, low homogeneity
            features[0] = 0.7f + (gen() % 100) / 1000.0f; // Contrast
            features[1] = 0.3f + (gen() % 100) / 1000.0f; // Homogeneity
            features[2] = 0.4f + (gen() % 100) / 1000.0f; // Energy
            features[3] = 0.8f + (gen() % 100) / 1000.0f; // Entropy
        } else {
            // Medium texture
            uniform_real_distribution<float> dist(0.3f, 0.7f);
            for (auto& feature : features) {
                feature = dist(gen);
            }
        }
        
        // Add more texture features
        features[4] = features[0] * features[1]; // Correlation
        features[5] = sqrt(features[2]);         // Variance
        features[6] = features[3] / 2.0f;       // Sum average
        features[7] = 1.0f - features[1];       // Dissimilarity
        
        cout << "  Extracted 8 texture features" << endl;
        return features;
    }
    
    /**
     * @brief Extract shape features
     */
    static vector<float> extractShapeFeatures(int width, int height, const string& shape_type) {
        cout << "Extracting shape features (" << shape_type << ")..." << endl;
        
        vector<float> features(4);
        
        if (shape_type == "circular") {
            features[0] = 0.9f; // Circularity
            features[1] = 1.0f; // Compactness
            features[2] = 0.8f; // Convexity
            features[3] = 0.1f; // Elongation
        } else if (shape_type == "rectangular") {
            features[0] = 0.2f; // Circularity
            features[1] = 0.6f; // Compactness
            features[2] = 1.0f; // Convexity
            features[3] = 0.7f; // Elongation
        } else {
            // Irregular shape
            features[0] = 0.4f; // Circularity
            features[1] = 0.5f; // Compactness
            features[2] = 0.6f; // Convexity
            features[3] = 0.5f; // Elongation
        }
        
        cout << "  Extracted 4 shape features" << endl;
        return features;
    }
};

/**
 * @brief Simple ML classifier
 */
class ImageClassifier {
private:
    vector<ImageFeatures> training_data_;
    vector<string> class_names_;
    
public:
    /**
     * @brief Train the classifier with sample data
     */
    void train() {
        cout << "Training image classifier..." << endl;
        
        class_names_ = {"bright_smooth", "dark_rough", "normal_mixed"};
        
        // Generate training data
        vector<tuple<string, string, string>> training_configs = {
            {"bright", "smooth", "circular"},
            {"bright", "smooth", "rectangular"},
            {"dark", "rough", "circular"},
            {"dark", "rough", "irregular"},
            {"normal", "medium", "rectangular"},
            {"normal", "medium", "irregular"}
        };
        
        for (const auto& config : training_configs) {
            for (int i = 0; i < 10; ++i) { // 10 samples per configuration
                ImageFeatures features;
                features.histogram = FeatureExtractor::extractHistogram(640, 480, get<0>(config));
                features.texture_features = FeatureExtractor::extractTextureFeatures(640, 480, get<1>(config));
                features.shape_features = FeatureExtractor::extractShapeFeatures(640, 480, get<2>(config));
                
                // Assign label based on brightness and texture
                if (get<0>(config) == "bright" && get<1>(config) == "smooth") {
                    features.label = "bright_smooth";
                } else if (get<0>(config) == "dark" && get<1>(config) == "rough") {
                    features.label = "dark_rough";
                } else {
                    features.label = "normal_mixed";
                }
                
                training_data_.push_back(features);
            }
        }
        
        cout << "  Trained with " << training_data_.size() << " samples" << endl;
        cout << "  Classes: ";
        for (const auto& class_name : class_names_) {
            cout << class_name << " ";
        }
        cout << endl;
    }
    
    /**
     * @brief Classify an image based on its features
     */
    string classify(const ImageFeatures& features) {
        if (training_data_.empty()) {
            return "unknown";
        }
        
        // Simple nearest neighbor classification
        float min_distance = numeric_limits<float>::max();
        string best_class = "unknown";
        
        for (const auto& training_sample : training_data_) {
            float distance = calculateDistance(features, training_sample);
            if (distance < min_distance) {
                min_distance = distance;
                best_class = training_sample.label;
            }
        }
        
        return best_class;
    }
    
    /**
     * @brief Get classification confidence
     */
    float getConfidence(const ImageFeatures& features, const string& predicted_class) {
        vector<float> distances;
        
        for (const auto& training_sample : training_data_) {
            if (training_sample.label == predicted_class) {
                distances.push_back(calculateDistance(features, training_sample));
            }
        }
        
        if (distances.empty()) return 0.0f;
        
        float avg_distance = accumulate(distances.begin(), distances.end(), 0.0f) / distances.size();
        return max(0.0f, 1.0f - avg_distance / 10.0f); // Normalize to [0,1]
    }
    
private:
    float calculateDistance(const ImageFeatures& a, const ImageFeatures& b) {
        float distance = 0.0f;
        
        // Histogram distance (Chi-square)
        for (size_t i = 0; i < a.histogram.size(); ++i) {
            float sum = a.histogram[i] + b.histogram[i];
            if (sum > 0) {
                float diff = a.histogram[i] - b.histogram[i];
                distance += (diff * diff) / sum;
            }
        }
        
        // Texture features distance (Euclidean)
        for (size_t i = 0; i < a.texture_features.size(); ++i) {
            float diff = a.texture_features[i] - b.texture_features[i];
            distance += diff * diff;
        }
        
        // Shape features distance (Euclidean)
        for (size_t i = 0; i < a.shape_features.size(); ++i) {
            float diff = a.shape_features[i] - b.shape_features[i];
            distance += diff * diff;
        }
        
        return sqrt(distance);
    }
};

/**
 * @brief Performance evaluator
 */
class PerformanceEvaluator {
public:
    /**
     * @brief Evaluate classifier performance
     */
    static void evaluateClassifier(ImageClassifier& classifier) {
        cout << "\nEvaluating classifier performance..." << endl;
        
        // Generate test data
        vector<tuple<string, string, string, string>> test_cases = {
            {"bright", "smooth", "circular", "bright_smooth"},
            {"dark", "rough", "irregular", "dark_rough"},
            {"normal", "medium", "rectangular", "normal_mixed"},
            {"bright", "rough", "rectangular", "normal_mixed"}, // Mixed case
            {"dark", "smooth", "circular", "normal_mixed"}      // Mixed case
        };
        
        int correct_predictions = 0;
        int total_predictions = 0;
        
        for (const auto& test_case : test_cases) {
            for (int i = 0; i < 5; ++i) { // 5 samples per test case
                ImageFeatures test_features;
                test_features.histogram = FeatureExtractor::extractHistogram(640, 480, get<0>(test_case));
                test_features.texture_features = FeatureExtractor::extractTextureFeatures(640, 480, get<1>(test_case));
                test_features.shape_features = FeatureExtractor::extractShapeFeatures(640, 480, get<2>(test_case));
                
                string predicted = classifier.classify(test_features);
                string actual = get<3>(test_case);
                float confidence = classifier.getConfidence(test_features, predicted);
                
                if (predicted == actual) {
                    correct_predictions++;
                }
                total_predictions++;
                
                cout << "  Test " << total_predictions << ": Predicted=" << predicted 
                     << ", Actual=" << actual << ", Confidence=" << confidence 
                     << (predicted == actual ? " ✓" : " ✗") << endl;
            }
        }
        
        float accuracy = static_cast<float>(correct_predictions) / total_predictions;
        cout << "\nClassification Results:" << endl;
        cout << "  Correct predictions: " << correct_predictions << "/" << total_predictions << endl;
        cout << "  Accuracy: " << (accuracy * 100) << "%" << endl;
    }
};

/**
 * @brief Demonstrate ML-based image processing
 */
void demonstrateMLProcessing() {
    cout << "=== Machine Learning Image Processing Demo ===" << endl;
    
    // 1. Create and train classifier
    cout << "\n1. Training Phase:" << endl;
    ImageClassifier classifier;
    classifier.train();
    
    // 2. Feature extraction demonstration
    cout << "\n2. Feature Extraction:" << endl;
    ImageFeatures sample_features;
    sample_features.histogram = FeatureExtractor::extractHistogram(640, 480, "bright");
    sample_features.texture_features = FeatureExtractor::extractTextureFeatures(640, 480, "smooth");
    sample_features.shape_features = FeatureExtractor::extractShapeFeatures(640, 480, "circular");
    
    cout << "  Total feature vector size: " << 
            (sample_features.histogram.size() + 
             sample_features.texture_features.size() + 
             sample_features.shape_features.size()) << endl;
    
    // 3. Classification demonstration
    cout << "\n3. Classification:" << endl;
    string predicted_class = classifier.classify(sample_features);
    float confidence = classifier.getConfidence(sample_features, predicted_class);
    
    cout << "  Predicted class: " << predicted_class << endl;
    cout << "  Confidence: " << confidence << endl;
    
    // 4. Performance evaluation
    cout << "\n4. Performance Evaluation:" << endl;
    PerformanceEvaluator::evaluateClassifier(classifier);
}

/**
 * @brief Main function
 */
int main() {
    try {
        cout << "Machine Learning Image Processing Example" << endl;
        cout << "========================================" << endl;
        
        demonstrateMLProcessing();
        
        cout << "\nML processing demonstration completed!" << endl;
        return 0;
        
    } catch (const exception& e) {
        cerr << "Error: " << e.what() << endl;
        return 1;
    }
}
