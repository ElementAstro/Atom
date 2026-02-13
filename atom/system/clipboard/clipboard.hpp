/**
 * @file clipboard.hpp
 * @brief Cross-platform clipboard operations wrapper class.
 *
 * This is the main public header for the clipboard component. It provides
 * the Clipboard singleton class with support for text, binary data, and
 * image clipboard operations across Windows, Linux, and macOS.
 *
 * Sub-headers are included automatically:
 * - clipboard_types.hpp    : Format types, concepts, callback aliases
 * - clipboard_result.hpp   : ClipboardResult<T> error-handling type
 * - clipboard_error.hpp    : Error codes and exception classes
 * - clipboard_config.hpp   : Configuration, metrics, cache entry types
 */

#pragma once

#include "clipboard_config.hpp"
#include "clipboard_error.hpp"
#include "clipboard_result.hpp"
#include "clipboard_types.hpp"

#include <atomic>
#include <future>
#include <memory>
#include <mutex>
#include <span>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <vector>

namespace clip {

/**
 * @class Clipboard
 * @brief Cross-platform clipboard operations wrapper class
 *
 * This class provides a unified interface for clipboard operations across
 * different platforms using modern C++17+ features including:
 * - constexpr and noexcept for performance optimization
 * - std::span for zero-copy data transfers
 * - Strong typing with ClipboardFormat
 * - Exception-safe RAII resource management
 * - Callback mechanism for clipboard change monitoring
 * - Asynchronous operations with std::future
 * - Configurable caching and metrics collection
 */
class Clipboard {
public:
    /**
     * @brief Get the singleton instance of the Clipboard
     * @return Reference to the singleton Clipboard instance
     */
    static Clipboard& instance() noexcept;

    /**
     * @brief Destructor - ensures proper cleanup of clipboard resources
     */
    ~Clipboard();

    // ============================================================================
    // Core Operations with Exception Safety
    // ============================================================================

    /**
     * @brief Open the clipboard for operations
     * @throws ClipboardException on failure
     */
    void open();

    /**
     * @brief Close the clipboard
     */
    void close() noexcept;

    /**
     * @brief Clear the clipboard contents
     * @throws ClipboardException on failure
     */
    void clear();

    // ============================================================================
    // Text Operations with Unicode Support
    // ============================================================================

    /**
     * @brief Set text to the clipboard with exception safety
     * @param text UTF-8 encoded text to place on the clipboard
     * @throws ClipboardException on failure
     */
    void setText(std::string_view text);

    /**
     * @brief Set text to the clipboard (non-throwing version)
     * @param text UTF-8 encoded text to place on the clipboard
     * @return ClipboardResult indicating success or error
     */
    [[nodiscard]] ClipboardResult<void> setTextSafe(
        std::string_view text) noexcept;

    /**
     * @brief Get text from the clipboard with exception safety
     * @return UTF-8 encoded clipboard text
     * @throws ClipboardException on failure or if no text is available
     */
    [[nodiscard]] std::string getText();

    /**
     * @brief Get text from the clipboard (non-throwing version)
     * @return ClipboardResult containing the text if available, error otherwise
     */
    [[nodiscard]] ClipboardResult<std::string> getTextSafe() noexcept;

    // ============================================================================
    // Binary Data Operations with Zero-Copy Support
    // ============================================================================

    /**
     * @brief Set binary data to the clipboard in a specific format
     * @param format The clipboard format identifier
     * @param data Binary data to place on the clipboard (zero-copy with span)
     * @throws ClipboardException on failure
     */
    void setData(ClipboardFormat format, std::span<const std::byte> data);

    /**
     * @brief Set binary data to the clipboard (non-throwing version)
     * @param format The clipboard format identifier
     * @param data Binary data to place on the clipboard
     * @return ClipboardResult indicating success or error
     */
    [[nodiscard]] ClipboardResult<void> setDataSafe(
        ClipboardFormat format, std::span<const std::byte> data) noexcept;

    /**
     * @brief Get binary data from the clipboard with move semantics
     * @param format The clipboard format identifier to retrieve
     * @return Binary data if available
     * @throws ClipboardException on failure or if format is not available
     */
    [[nodiscard]] std::vector<std::byte> getData(ClipboardFormat format);

    /**
     * @brief Get binary data from the clipboard (non-throwing version)
     * @param format The clipboard format identifier to retrieve
     * @return ClipboardResult containing the binary data if available, error
     * otherwise
     */
    [[nodiscard]] ClipboardResult<std::vector<std::byte>> getDataSafe(
        ClipboardFormat format) noexcept;

    /**
     * @brief Check if clipboard contains data in a specific format
     * @param format The clipboard format identifier to check
     * @return true if format is available, false otherwise
     */
    [[nodiscard]] bool containsFormat(ClipboardFormat format) const noexcept;

    // ============================================================================
    // Image Operations with Modern C++ Features
    // ============================================================================

#ifdef CLIPBOARD_SUPPORT_OPENCV
    /**
     * @brief Set OpenCV Mat image to the clipboard with exception safety
     * @param image OpenCV Mat image to place on the clipboard
     * @throws ClipboardException on failure
     */
    void setImage(const cv::Mat& image);

    /**
     * @brief Set OpenCV Mat image to the clipboard (non-throwing version)
     * @param image OpenCV Mat image to place on the clipboard
     * @return ClipboardResult indicating success or error
     */
    [[nodiscard]] ClipboardResult<void> setImageSafe(
        const cv::Mat& image) noexcept;

    /**
     * @brief Get image from clipboard as OpenCV Mat with move semantics
     * @return Image if available
     * @throws ClipboardException on failure or if no image is available
     */
    [[nodiscard]] cv::Mat getImageAsMat();

    /**
     * @brief Get image from clipboard as OpenCV Mat (non-throwing version)
     * @return ClipboardResult containing the image if available, error
     * otherwise
     */
    [[nodiscard]] ClipboardResult<cv::Mat> getImageAsMatSafe() noexcept;
#endif

#ifdef CLIPBOARD_SUPPORT_CIMG
    /**
     * @brief Set CImg image to the clipboard with exception safety
     * @param image CImg image to place on the clipboard
     * @throws ClipboardException on failure
     */
    void setImage(const cimg_library::CImg<unsigned char>& image);

    /**
     * @brief Set CImg image to the clipboard (non-throwing version)
     * @param image CImg image to place on the clipboard
     * @return ClipboardResult indicating success or error
     */
    [[nodiscard]] ClipboardResult<void> setImageSafe(
        const cimg_library::CImg<unsigned char>& image) noexcept;

    /**
     * @brief Get image from clipboard as CImg with move semantics
     * @return Image if available
     * @throws ClipboardException on failure or if no image is available
     */
    [[nodiscard]] cimg_library::CImg<unsigned char> getImageAsCImg();

    /**
     * @brief Get image from clipboard as CImg (non-throwing version)
     * @return ClipboardResult containing the image if available, error
     * otherwise
     */
    [[nodiscard]] ClipboardResult<cimg_library::CImg<unsigned char>>
    getImageAsCImgSafe() noexcept;
#endif

    /**
     * @brief Generic image setting function using concepts and perfect
     * forwarding
     * @tparam ImageT Image type that satisfies ClipboardImageType concept
     * @param image Image to place on the clipboard
     * @throws ClipboardException on failure
     */
    template <ClipboardImageType ImageT>
    void setGenericImage(ImageT&& image);

    /**
     * @brief Generic image setting function (non-throwing version)
     * @tparam ImageT Image type that satisfies ClipboardImageType concept
     * @param image Image to place on the clipboard
     * @return ClipboardResult indicating success or error
     */
    template <ClipboardImageType ImageT>
    [[nodiscard]] ClipboardResult<void> setGenericImageSafe(
        ImageT&& image) noexcept;

    // ============================================================================
    // Query Operations with Performance Optimization
    // ============================================================================

    /**
     * @brief Check if clipboard contains text data
     * @return true if text is available, false otherwise
     */
    [[nodiscard]] bool hasText() const noexcept;

    /**
     * @brief Check if clipboard contains image data
     * @return true if image is available, false otherwise
     */
    [[nodiscard]] bool hasImage() const noexcept;

    /**
     * @brief Get list of available clipboard formats with move semantics
     * @return Vector of format identifiers currently available on the clipboard
     */
    [[nodiscard]] std::vector<ClipboardFormat> getAvailableFormats() const;

    /**
     * @brief Get list of available clipboard formats (non-throwing version)
     * @return ClipboardResult containing vector of format identifiers or error
     */
    [[nodiscard]] ClipboardResult<std::vector<ClipboardFormat>>
    getAvailableFormatsSafe() const noexcept;

    /**
     * @brief Get human-readable name for a clipboard format
     * @param format The format identifier
     * @return Format name if known
     * @throws ClipboardException if format is unknown
     */
    [[nodiscard]] std::string getFormatName(ClipboardFormat format) const;

    /**
     * @brief Get human-readable name for a clipboard format (non-throwing
     * version)
     * @param format The format identifier
     * @return ClipboardResult containing the format name if known, error
     * otherwise
     */
    [[nodiscard]] ClipboardResult<std::string> getFormatNameSafe(
        ClipboardFormat format) const noexcept;

    // ============================================================================
    // Clipboard Change Monitoring with Callback Mechanism
    // ============================================================================

    /**
     * @brief Register a callback for clipboard change notifications
     * @param callback Function to call when clipboard content changes
     * @return Callback ID for unregistering, or 0 on failure
     */
    [[nodiscard]] std::size_t registerChangeCallback(
        ClipboardChangeCallback callback);

    /**
     * @brief Unregister a clipboard change callback
     * @param callbackId The callback ID returned by registerChangeCallback
     * @return true if callback was successfully unregistered, false otherwise
     */
    bool unregisterChangeCallback(std::size_t callbackId) noexcept;

    /**
     * @brief Check if clipboard content has changed since last check
     * @return true if content has changed, false otherwise
     */
    [[nodiscard]] bool hasChanged() const noexcept;

    /**
     * @brief Update internal change tracking state
     * Call this after processing clipboard changes to reset the changed flag
     */
    void markChangeProcessed() noexcept;

    // ============================================================================
    // Static Format Registration
    // ============================================================================

    /**
     * @brief Register a custom clipboard format
     * @param formatName Name of the custom format
     * @return Registered format identifier
     * @throws ClipboardException on registration failure
     */
    [[nodiscard]] static ClipboardFormat registerFormat(
        std::string_view formatName);

    /**
     * @brief Register a custom clipboard format (non-throwing version)
     * @param formatName Name of the custom format
     * @return ClipboardResult containing the registered format identifier or
     * error
     */
    [[nodiscard]] static ClipboardResult<ClipboardFormat> registerFormatSafe(
        std::string_view formatName) noexcept;

    // ============================================================================
    // Configuration and Performance Management
    // ============================================================================

    /**
     * @brief Set clipboard configuration
     * @param config The configuration to apply
     */
    void setConfig(const ClipboardConfig& config) noexcept;

    /**
     * @brief Get current clipboard configuration
     * @return Reference to the current configuration
     */
    [[nodiscard]] const ClipboardConfig& getConfig() const noexcept;

    /**
     * @brief Get current performance metrics
     * @return Reference to the current metrics
     */
    [[nodiscard]] const ClipboardMetrics& getMetrics() const noexcept;

    /**
     * @brief Reset all performance metrics to zero
     */
    void resetMetrics() noexcept;

    /**
     * @brief Clear the clipboard data cache
     */
    void clearCache() noexcept;

    // ============================================================================
    // Asynchronous Operations
    // ============================================================================

    /**
     * @brief Asynchronously set text to the clipboard
     * @param text UTF-8 encoded text to place on the clipboard
     * @return Future containing the operation result
     */
    [[nodiscard]] std::future<ClipboardResult<void>> setTextAsync(
        std::string text);

    /**
     * @brief Asynchronously get text from the clipboard
     * @return Future containing the text result
     */
    [[nodiscard]] std::future<ClipboardResult<std::string>> getTextAsync();

    /**
     * @brief Asynchronously set binary data to the clipboard
     * @param format The clipboard format identifier
     * @param data Binary data to place on the clipboard
     * @return Future containing the operation result
     */
    [[nodiscard]] std::future<ClipboardResult<void>> setDataAsync(
        ClipboardFormat format, std::vector<std::byte> data);

    /**
     * @brief Asynchronously get binary data from the clipboard
     * @param format The clipboard format identifier to retrieve
     * @return Future containing the binary data result
     */
    [[nodiscard]] std::future<ClipboardResult<std::vector<std::byte>>>
    getDataAsync(ClipboardFormat format);

    /**
     * @class Impl
     * @brief Platform-specific implementation details (PIMPL pattern)
     */
    class Impl;

private:
    /**
     * @brief Private constructor (singleton pattern)
     */
    Clipboard();

    // Disable copy and move operations for singleton
    Clipboard(const Clipboard&) = delete;
    Clipboard& operator=(const Clipboard&) = delete;
    Clipboard(Clipboard&&) = delete;
    Clipboard& operator=(Clipboard&&) = delete;

    // Platform implementation (PIMPL)
    std::unique_ptr<Impl> pImpl;

    // Configuration and metrics
    ClipboardConfig m_config;
    ClipboardMetrics m_metrics;

    // Callback management
    mutable std::mutex m_callbackMutex;
    std::unordered_map<std::size_t, ClipboardChangeCallback> m_callbacks;
    std::atomic<std::size_t> m_nextCallbackId{1};

    // Change monitoring
    mutable std::atomic<bool> m_hasChanged{false};

    // Cache management
    mutable std::mutex m_cacheMutex;
    mutable std::unordered_map<unsigned int, CacheEntry> m_cache;

    // Thread management
    std::atomic<bool> m_shutdown{false};
    std::mutex m_threadMutex;
    std::vector<std::thread> m_workerThreads;

    // ============================================================================
    // Private Helper Methods
    // ============================================================================

    /** @brief Notify all registered callbacks of clipboard changes */
    void notifyCallbacks() const noexcept;

    /** @brief Check if a cache entry is still valid */
    [[nodiscard]] bool isCacheValid(const CacheEntry& entry) const noexcept;

    /** @brief Remove expired entries from the cache */
    void cleanupCache() const noexcept;

    /** @brief Shutdown worker threads */
    void shutdownThreads() noexcept;
};

// ============================================================================
// Template Implementations
// ============================================================================

template <ClipboardImageType ImageT>
void Clipboard::setGenericImage(ImageT&& image) {
    static_assert(ClipboardImageType<std::decay_t<ImageT>>,
                  "Type must satisfy ClipboardImageType concept");

#ifdef CLIPBOARD_SUPPORT_OPENCV
    // Convert to cv::Mat and use existing implementation
    cv::Mat mat(image.rows, image.cols, CV_8UC(image.channels()), image.data);
    setImage(mat);
#else
    throw ClipboardFormatException("Generic image support requires OpenCV");
#endif
}

template <ClipboardImageType ImageT>
ClipboardResult<void> Clipboard::setGenericImageSafe(ImageT&& image) noexcept {
    try {
        setGenericImage(std::forward<ImageT>(image));
        return ClipboardResult<void>{};
    } catch (const std::exception&) {
        return ClipboardResult<void>{
            make_error_code(ClipboardErrorCode::INVALID_DATA)};
    }
}

}  // namespace clip
