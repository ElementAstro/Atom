#ifndef ATOM_IMAGE_ML_PROCESSING_HPP
#define ATOM_IMAGE_ML_PROCESSING_HPP

/**
 * @file ml_processing.hpp
 * @brief Machine Learning based image processing
 *
 * This module provides ML-based image processing capabilities including
 * super-resolution, denoising, style transfer, image generation,
 * and AI-powered enhancement operations.
 *
 * @author Atom Framework Team
 * @date 2025
 * @version 1.0.0
 */

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include "../core/image_blob.hpp"

namespace atom::image {

/**
 * @brief ML model types for image processing
 */
enum class MLModelType {
    // Super-resolution models
    ESRGAN,       // Enhanced Super-Resolution GAN
    REAL_ESRGAN,  // Real-ESRGAN
    SRCNN,        // Super-Resolution CNN
    VDSR,         // Very Deep Super-Resolution
    EDSR,         // Enhanced Deep Super-Resolution
    WAIFU2X,      // Waifu2x anime upscaler

    // Denoising models
    DNCNN,   // Denoising CNN
    FFDNet,  // Fast and Flexible Denoising
    RIDNET,  // Real Image Denoising
    CBDNet,  // Toward Convolutional Blind Denoising

    // Style transfer models
    NEURAL_STYLE,    // Neural Style Transfer
    FAST_STYLE,      // Fast Style Transfer
    ADAIN,           // Adaptive Instance Normalization
    PHOTOREALISTIC,  // Photorealistic Style Transfer

    // Image enhancement models
    DPED,         // DSLR-Quality Photos Enhancement
    WESPE,        // Weakly Supervised Photo Enhancer
    MIRNET,       // Learning Enriched Features
    RETINEX_NET,  // Deep Retinex Decomposition

    // Image restoration models
    NAFNET,     // Nonlinear Activation Free Network
    RESTORMER,  // Efficient Transformer for Image Restoration
    SWINIR,     // SwinIR Image Restoration
    UFORMER,    // U-shaped Transformer

    // Generative models
    STABLE_DIFFUSION,  // Stable Diffusion
    DALLE,             // DALL-E
    MIDJOURNEY,        // Midjourney-style generation
    GAN_PAINT,         // GAN-based inpainting

    // Specialized models
    COLORIZATION,        // Image colorization
    INPAINTING,          // Image inpainting
    OUTPAINTING,         // Image outpainting
    BACKGROUND_REMOVAL,  // Background removal
    FACE_RESTORATION,    // Face restoration

    CUSTOM  // Custom trained model
};

/**
 * @brief ML inference backends
 */
enum class MLBackend {
    ONNX,        // ONNX Runtime
    TENSORRT,    // NVIDIA TensorRT
    OPENVINO,    // Intel OpenVINO
    PYTORCH,     // PyTorch
    TENSORFLOW,  // TensorFlow
    NCNN,        // NCNN (mobile)
    MNN,         // MNN (mobile)
    PADDLE,      // PaddlePaddle
    AUTO         // Auto-select best backend
};

/**
 * @brief ML processing parameters
 */
struct MLParams {
    // Model parameters
    std::string modelPath;                // Path to model file
    MLBackend backend = MLBackend::AUTO;  // Inference backend
    bool useGPU = true;                   // Use GPU acceleration
    int gpuDeviceId = 0;                  // GPU device ID

    // Processing parameters
    int batchSize = 1;       // Batch size for processing
    int tileSize = 512;      // Tile size for large images
    int overlap = 32;        // Tile overlap
    bool enableTTA = false;  // Test-time augmentation

    // Super-resolution parameters
    int scaleFactor = 4;          // Upscaling factor
    bool preserveDetails = true;  // Preserve fine details

    // Denoising parameters
    double noiseLevel = 25.0;    // Noise level (0-100)
    bool blindDenoising = true;  // Blind denoising mode

    // Style transfer parameters
    double styleStrength = 1.0;  // Style transfer strength
    bool preserveColor = false;  // Preserve original colors

    // Enhancement parameters
    double enhancementStrength = 0.8;  // Enhancement strength
    bool autoAdjust = true;            // Auto-adjust parameters

    // Generation parameters
    std::string prompt;          // Text prompt for generation
    std::string negativePrompt;  // Negative prompt
    int steps = 50;              // Inference steps
    double guidanceScale = 7.5;  // Guidance scale
    int seed = -1;               // Random seed (-1 = random)

    // Custom parameters
    std::unordered_map<std::string, double> customParams;
};

/**
 * @brief ML processing result
 */
struct MLResult {
    blob outputImage;             // Processed image
    double processingTime = 0.0;  // Processing time in seconds
    double confidence = 0.0;      // Result confidence
    std::string modelUsed;        // Model that was used
    std::unordered_map<std::string, double> metrics;  // Quality metrics
    std::string errorMessage;                         // Error message if failed
    bool success = true;                              // Success status
};

/**
 * @brief Machine Learning image processor
 */
class MLImageProcessor {
public:
    MLImageProcessor() = default;
    virtual ~MLImageProcessor() = default;

    /**
     * @brief Initialize ML processor with models
     * @param modelDir Directory containing model files
     * @param backend Preferred inference backend
     * @param useGPU Whether to use GPU acceleration
     * @return Success status
     */
    virtual bool initialize(const std::string& modelDir = "",
                            MLBackend backend = MLBackend::AUTO,
                            bool useGPU = true);

    /**
     * @brief Apply super-resolution to image
     * @param input Input low-resolution image
     * @param model Super-resolution model
     * @param params Processing parameters
     * @return Super-resolved image result
     */
    virtual MLResult superResolution(
        const blob& input, MLModelType model = MLModelType::REAL_ESRGAN,
        const MLParams& params = {}) const;

    /**
     * @brief Apply AI-based denoising
     * @param input Input noisy image
     * @param model Denoising model
     * @param params Processing parameters
     * @return Denoised image result
     */
    virtual MLResult denoise(const blob& input,
                             MLModelType model = MLModelType::DNCNN,
                             const MLParams& params = {}) const;

    /**
     * @brief Apply neural style transfer
     * @param contentImage Content image
     * @param styleImage Style reference image
     * @param model Style transfer model
     * @param params Processing parameters
     * @return Stylized image result
     */
    virtual MLResult styleTransfer(const blob& contentImage,
                                   const blob& styleImage,
                                   MLModelType model = MLModelType::FAST_STYLE,
                                   const MLParams& params = {}) const;

    /**
     * @brief Apply AI-based image enhancement
     * @param input Input image
     * @param model Enhancement model
     * @param params Processing parameters
     * @return Enhanced image result
     */
    virtual MLResult enhance(const blob& input,
                             MLModelType model = MLModelType::MIRNET,
                             const MLParams& params = {}) const;

    /**
     * @brief Apply image restoration
     * @param input Input degraded image
     * @param model Restoration model
     * @param params Processing parameters
     * @return Restored image result
     */
    virtual MLResult restore(const blob& input,
                             MLModelType model = MLModelType::SWINIR,
                             const MLParams& params = {}) const;

    /**
     * @brief Generate image from text prompt
     * @param prompt Text description
     * @param model Generation model
     * @param params Generation parameters
     * @return Generated image result
     */
    virtual MLResult generateFromText(
        const std::string& prompt,
        MLModelType model = MLModelType::STABLE_DIFFUSION,
        const MLParams& params = {}) const;

    /**
     * @brief Apply image inpainting
     * @param input Input image with missing regions
     * @param mask Mask indicating regions to inpaint
     * @param model Inpainting model
     * @param params Processing parameters
     * @return Inpainted image result
     */
    virtual MLResult inpaint(const blob& input, const blob& mask,
                             MLModelType model = MLModelType::GAN_PAINT,
                             const MLParams& params = {}) const;

    /**
     * @brief Apply image colorization
     * @param input Input grayscale image
     * @param model Colorization model
     * @param params Processing parameters
     * @return Colorized image result
     */
    virtual MLResult colorize(const blob& input,
                              MLModelType model = MLModelType::COLORIZATION,
                              const MLParams& params = {}) const;

    /**
     * @brief Remove background from image
     * @param input Input image
     * @param model Background removal model
     * @param params Processing parameters
     * @return Image with background removed
     */
    virtual MLResult removeBackground(
        const blob& input, MLModelType model = MLModelType::BACKGROUND_REMOVAL,
        const MLParams& params = {}) const;

    /**
     * @brief Apply face restoration
     * @param input Input image with degraded faces
     * @param model Face restoration model
     * @param params Processing parameters
     * @return Image with restored faces
     */
    virtual MLResult restoreFaces(
        const blob& input, MLModelType model = MLModelType::FACE_RESTORATION,
        const MLParams& params = {}) const;

    /**
     * @brief Process image with custom model
     * @param input Input image
     * @param modelPath Path to custom model file
     * @param params Processing parameters
     * @return Processed image result
     */
    virtual MLResult processWithCustomModel(const blob& input,
                                            const std::string& modelPath,
                                            const MLParams& params = {}) const;

    /**
     * @brief Batch process multiple images
     * @param inputs Vector of input images
     * @param model Model to use
     * @param params Processing parameters
     * @param progressCallback Progress callback function
     * @return Vector of processing results
     */
    virtual std::vector<MLResult> batchProcess(
        const std::vector<blob>& inputs, MLModelType model,
        const MLParams& params = {},
        std::function<void(int, int)> progressCallback = nullptr) const;

    /**
     * @brief Get available models
     * @param modelType Type of models to list
     * @return Vector of available model names
     */
    virtual std::vector<std::string> getAvailableModels(
        MLModelType modelType) const;

    /**
     * @brief Download model from repository
     * @param model Model to download
     * @param modelDir Directory to save model
     * @param progressCallback Download progress callback
     * @return Success status
     */
    virtual bool downloadModel(
        MLModelType model, const std::string& modelDir = "",
        std::function<void(int)> progressCallback = nullptr) const;

    /**
     * @brief Check if model is available
     * @param model Model to check
     * @return True if model is available
     */
    virtual bool isModelAvailable(MLModelType model) const;

    /**
     * @brief Get model information
     * @param model Model to get info for
     * @return Model information map
     */
    virtual std::unordered_map<std::string, std::string> getModelInfo(
        MLModelType model) const;

    /**
     * @brief Benchmark model performance
     * @param model Model to benchmark
     * @param testImage Test image
     * @param iterations Number of iterations
     * @return Benchmark results
     */
    virtual std::unordered_map<std::string, double> benchmarkModel(
        MLModelType model, const blob& testImage, int iterations = 10) const;

    /**
     * @brief Set inference backend
     * @param backend Backend to use
     * @param deviceId Device ID (for GPU backends)
     * @return Success status
     */
    virtual bool setBackend(MLBackend backend, int deviceId = 0);

    /**
     * @brief Get current backend information
     * @return Backend information
     */
    virtual std::unordered_map<std::string, std::string> getBackendInfo() const;

protected:
    /**
     * @brief Load ML model
     * @param model Model type to load
     * @param params Model parameters
     * @return Success status
     */
    virtual bool loadModel(MLModelType model,
                           const MLParams& params = {}) const;

    /**
     * @brief Preprocess image for ML model
     * @param input Input image
     * @param model Target model
     * @param params Processing parameters
     * @return Preprocessed image data
     */
    virtual std::vector<float> preprocessImage(
        const blob& input, MLModelType model,
        const MLParams& params = {}) const;

    /**
     * @brief Postprocess ML model output
     * @param output Model output data
     * @param model Source model
     * @param originalSize Original image size
     * @param params Processing parameters
     * @return Postprocessed image blob
     */
    virtual blob postprocessOutput(const std::vector<float>& output,
                                   MLModelType model,
                                   const std::pair<int, int>& originalSize,
                                   const MLParams& params = {}) const;

    /**
     * @brief Run inference on preprocessed data
     * @param input Preprocessed input data
     * @param model Model to use
     * @param params Inference parameters
     * @return Model output data
     */
    virtual std::vector<float> runInference(const std::vector<float>& input,
                                            MLModelType model,
                                            const MLParams& params = {}) const;

    /**
     * @brief Get model file path for a given model type
     * @param model Model type
     * @return Full path to the model file
     */
    std::string getModelPath(MLModelType model) const;

private:
    std::string modelDir_;
    MLBackend currentBackend_ = MLBackend::AUTO;
    bool useGPU_ = true;
};

/**
 * @brief Factory function to create optimal ML processor
 * @param modelDir Directory containing model files
 * @param useGPU Whether to use GPU acceleration
 * @param backend Preferred inference backend
 * @return Unique pointer to ML processor
 */
std::unique_ptr<MLImageProcessor> createOptimalMLProcessor(
    const std::string& modelDir = "", bool useGPU = true,
    MLBackend backend = MLBackend::AUTO);

}  // namespace atom::image

#endif  // ATOM_IMAGE_ML_PROCESSING_HPP
