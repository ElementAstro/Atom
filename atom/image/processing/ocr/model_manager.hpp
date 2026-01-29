/**
 * @file model_manager.hpp
 * @brief Cross-platform model and resource manager for OCR system
 *
 * Provides functionality for:
 * - Downloading models from remote URLs
 * - Verifying model integrity with checksums
 * - Managing model cache and updates
 * - Platform-specific download implementations
 */

#pragma once

#include <filesystem>
#include <functional>
#include <future>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace atom::image::ocr {

/**
 * @struct ModelInfo
 * @brief Information about an OCR model resource
 */
struct ModelInfo {
    std::string name;         ///< Model name/identifier
    std::string url;          ///< Download URL
    std::string localPath;    ///< Local file path
    std::string sha256;       ///< SHA256 checksum (optional)
    size_t expectedSize = 0;  ///< Expected file size in bytes
    bool required = true;     ///< Whether model is required for operation
    std::string description;  ///< Human-readable description
};

/**
 * @struct DownloadProgress
 * @brief Progress information for download operations
 */
struct DownloadProgress {
    size_t bytesDownloaded = 0;  ///< Bytes downloaded so far
    size_t totalBytes = 0;       ///< Total bytes to download (0 if unknown)
    float percentage = 0.0f;     ///< Download percentage (0-100)
    std::string status;          ///< Status message
    bool completed = false;      ///< Whether download is complete
    bool success = false;        ///< Whether download was successful
    std::string errorMessage;    ///< Error message if failed
};

/**
 * @typedef ProgressCallback
 * @brief Callback function for download progress updates
 */
using ProgressCallback = std::function<void(const DownloadProgress&)>;

/**
 * @class ModelManager
 * @brief Manages OCR model resources with cross-platform download support
 */
class ModelManager {
public:
    /**
     * @brief Construct a new ModelManager
     * @param modelsDir Directory to store downloaded models
     * @param dictDir Directory to store dictionary files
     */
    explicit ModelManager(const std::string& modelsDir = "./models",
                          const std::string& dictDir = "./dict");

    ~ModelManager() = default;

    // Disable copy and move (mutex is not moveable)
    ModelManager(const ModelManager&) = delete;
    ModelManager& operator=(const ModelManager&) = delete;
    ModelManager(ModelManager&&) = delete;
    ModelManager& operator=(ModelManager&&) = delete;

    /**
     * @brief Initialize the model manager and create directories
     * @return True if initialization successful
     */
    bool initialize();

    /**
     * @brief Register a model for management
     * @param info Model information
     */
    void registerModel(const ModelInfo& info);

    /**
     * @brief Check if a model is available locally
     * @param modelName Name of the model
     * @return True if model exists and is valid
     */
    bool isModelAvailable(const std::string& modelName) const;

    /**
     * @brief Get the local path for a model
     * @param modelName Name of the model
     * @return Local file path or empty string if not found
     */
    std::string getModelPath(const std::string& modelName) const;

    /**
     * @brief Download a specific model
     * @param modelName Name of the model to download
     * @param callback Progress callback
     * @return True if download successful
     */
    bool downloadModel(const std::string& modelName,
                       ProgressCallback callback = nullptr);

    /**
     * @brief Download all registered models
     * @param callback Progress callback
     * @return Number of models successfully downloaded
     */
    size_t downloadAllModels(ProgressCallback callback = nullptr);

    /**
     * @brief Download a model asynchronously
     * @param modelName Name of the model
     * @param callback Progress callback
     * @return Future for the download result
     */
    std::future<bool> downloadModelAsync(const std::string& modelName,
                                         ProgressCallback callback = nullptr);

    /**
     * @brief Verify model integrity using checksum
     * @param modelName Name of the model
     * @return True if checksum matches or no checksum specified
     */
    bool verifyModel(const std::string& modelName) const;

    /**
     * @brief Get list of all registered models
     * @return Vector of model names
     */
    std::vector<std::string> getRegisteredModels() const;

    /**
     * @brief Get information about a specific model
     * @param modelName Name of the model
     * @return Optional ModelInfo if found
     */
    std::optional<ModelInfo> getModelInfo(const std::string& modelName) const;

    /**
     * @brief Check if all required models are available
     * @return True if all required models are present
     */
    bool hasRequiredModels() const;

    /**
     * @brief Get list of missing required models
     * @return Vector of missing model names
     */
    std::vector<std::string> getMissingRequiredModels() const;

    /**
     * @brief Delete a downloaded model
     * @param modelName Name of the model
     * @return True if deletion successful
     */
    bool deleteModel(const std::string& modelName);

    /**
     * @brief Clear all downloaded models
     */
    void clearAllModels();

    /**
     * @brief Get total size of downloaded models
     * @return Size in bytes
     */
    size_t getTotalModelsSize() const;

    /**
     * @brief Register default OCR models
     */
    void registerDefaultModels();

    /**
     * @brief Download dictionary file for spell checking
     * @param language Language code (e.g., "eng", "fra")
     * @param callback Progress callback
     * @return True if download successful
     */
    bool downloadDictionary(const std::string& language = "eng",
                            ProgressCallback callback = nullptr);

    /**
     * @brief Check if dictionary is available
     * @param language Language code
     * @return True if dictionary exists
     */
    bool isDictionaryAvailable(const std::string& language = "eng") const;

    /**
     * @brief Get dictionary file path
     * @param language Language code
     * @return Path to dictionary file
     */
    std::string getDictionaryPath(const std::string& language = "eng") const;

private:
    std::string m_modelsDir;  ///< Directory for model files
    std::string m_dictDir;    ///< Directory for dictionary files
    std::unordered_map<std::string, ModelInfo> m_models;  ///< Registered models
    mutable std::mutex m_mutex;  ///< Thread safety mutex

    /**
     * @brief Download a file from URL to local path
     * @param url Source URL
     * @param localPath Destination path
     * @param callback Progress callback
     * @return True if download successful
     */
    bool downloadFile(const std::string& url, const std::string& localPath,
                      ProgressCallback callback = nullptr);

    /**
     * @brief Calculate SHA256 checksum of a file
     * @param filePath Path to file
     * @return Hex-encoded SHA256 string
     */
    std::string calculateSHA256(const std::string& filePath) const;

    /**
     * @brief Platform-specific HTTP download implementation
     * @param url Source URL
     * @param localPath Destination path
     * @param callback Progress callback
     * @return True if successful
     */
    bool httpDownload(const std::string& url, const std::string& localPath,
                      ProgressCallback callback);

#ifdef _WIN32
    /**
     * @brief Windows-specific download using WinHTTP
     */
    bool downloadWindows(const std::string& url, const std::string& localPath,
                         ProgressCallback callback);
#else
    /**
     * @brief Unix-specific download using curl
     */
    bool downloadUnix(const std::string& url, const std::string& localPath,
                      ProgressCallback callback);
#endif
};

/**
 * @brief Create a default ModelManager instance with standard OCR models
 * @return Unique pointer to configured ModelManager
 */
std::unique_ptr<ModelManager> createDefaultModelManager();

}  // namespace atom::image::ocr
