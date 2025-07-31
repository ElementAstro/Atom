#pragma once

#include "clipboard_error.hpp"

#include <atomic>
#include <chrono>
#include <concepts>
#include <cstddef>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>
#include <unordered_map>
#include <vector>

#ifdef CLIPBOARD_SUPPORT_OPENCV
#include <opencv2/opencv.hpp>
#endif

#ifdef CLIPBOARD_SUPPORT_CIMG
#include <CImg.h>
#endif

namespace clip {

/**
 * @brief Configuration options for clipboard operations
 */
struct ClipboardConfig {
    std::chrono::milliseconds timeout{5000};           // Operation timeout
    std::chrono::milliseconds cache_ttl{1000};         // Cache time-to-live
    std::chrono::milliseconds poll_interval{50};       // Polling interval for Linux
    size_t max_cache_size{10 * 1024 * 1024};          // Max cache size (10MB)
    size_t max_retry_count{10};                        // Max retry attempts
    bool enable_caching{true};                         // Enable intelligent caching
    bool enable_monitoring{true};                      // Enable change monitoring
    bool enable_metrics{false};                       // Enable performance metrics
};

/**
 * @brief Performance metrics for clipboard operations
 */
struct ClipboardMetrics {
    std::atomic<uint64_t> operations_count{0};
    std::atomic<uint64_t> cache_hits{0};
    std::atomic<uint64_t> cache_misses{0};
    std::atomic<uint64_t> errors_count{0};
    std::atomic<uint64_t> bytes_transferred{0};
    std::chrono::steady_clock::time_point start_time{std::chrono::steady_clock::now()};

    void reset() noexcept {
        operations_count = 0;
        cache_hits = 0;
        cache_misses = 0;
        errors_count = 0;
        bytes_transferred = 0;
        start_time = std::chrono::steady_clock::now();
    }

    double get_cache_hit_ratio() const noexcept {
        auto total = cache_hits.load() + cache_misses.load();
        return total > 0 ? static_cast<double>(cache_hits.load()) / total : 0.0;
    }
};

/**
 * @brief Strong type for clipboard format identifiers
 */
struct ClipboardFormat {
    unsigned int value;

    constexpr explicit ClipboardFormat(unsigned int v) noexcept : value(v) {}
    constexpr operator unsigned int() const noexcept { return value; }

    constexpr bool operator==(const ClipboardFormat& other) const noexcept =
        default;
    constexpr auto operator<=>(const ClipboardFormat& other) const noexcept =
        default;
};

}  // namespace clip

// Hash function for ClipboardFormat to enable use in unordered_map
namespace std {
template <>
struct hash<clip::ClipboardFormat> {
    std::size_t operator()(const clip::ClipboardFormat& format) const noexcept {
        return std::hash<unsigned int>{}(format.value);
    }
};
}  // namespace std

namespace clip {

/**
 * @brief Predefined clipboard formats
 */
namespace formats {
constexpr ClipboardFormat TEXT{1};
constexpr ClipboardFormat HTML{2};
constexpr ClipboardFormat IMAGE_TIFF{3};
constexpr ClipboardFormat IMAGE_PNG{4};
constexpr ClipboardFormat RTF{5};
}  // namespace formats

/**
 * @brief Concept for checking if an image type has required properties for
 * clipboard operations
 */
template <typename ImageT>
concept ClipboardImageType = requires(ImageT img) {
    { img.cols } -> std::convertible_to<int>;
    { img.rows } -> std::convertible_to<int>;
    { img.channels() } -> std::convertible_to<int>;
    { img.data } -> std::convertible_to<void*>;
};

/**
 * @brief Callback function type for clipboard change notifications
 */
using ClipboardChangeCallback = std::function<void()>;

/**
 * @brief Result type for operations that may fail
 */
template <typename T>
class ClipboardResult {
private:
    std::optional<T> m_value;
    std::error_code m_error;

public:
    ClipboardResult(T&& value) noexcept : m_value(std::move(value)) {}
    ClipboardResult(const T& value) : m_value(value) {}
    ClipboardResult(std::error_code error) noexcept : m_error(error) {}

    constexpr bool has_value() const noexcept { return m_value.has_value(); }
    constexpr explicit operator bool() const noexcept { return has_value(); }

    constexpr const T& value() const& {
        if (!has_value())
            throw std::runtime_error("ClipboardResult has no value");
        return *m_value;
    }

    constexpr T& value() & {
        if (!has_value())
            throw std::runtime_error("ClipboardResult has no value");
        return *m_value;
    }

    constexpr T&& value() && {
        if (!has_value())
            throw std::runtime_error("ClipboardResult has no value");
        return std::move(*m_value);
    }

    constexpr const T& operator*() const& noexcept { return *m_value; }
    constexpr T& operator*() & noexcept { return *m_value; }
    constexpr T&& operator*() && noexcept { return std::move(*m_value); }

    constexpr const T* operator->() const noexcept { return &*m_value; }
    constexpr T* operator->() noexcept { return &*m_value; }

    std::error_code error() const noexcept { return m_error; }

    template <typename U>
    constexpr T value_or(U&& default_value) const& {
        return has_value() ? *m_value
                           : static_cast<T>(std::forward<U>(default_value));
    }

    template <typename U>
    constexpr T value_or(U&& default_value) && {
        return has_value() ? std::move(*m_value)
                           : static_cast<T>(std::forward<U>(default_value));
    }
};

/**
 * @brief Result type specialization for void operations
 */
template <>
class ClipboardResult<void> {
private:
    std::error_code m_error;

public:
    ClipboardResult() noexcept = default;
    ClipboardResult(std::error_code error) noexcept : m_error(error) {}

    constexpr bool has_value() const noexcept { return !m_error; }
    constexpr explicit operator bool() const noexcept { return has_value(); }

    constexpr void value() const {
        if (!has_value())
            throw std::runtime_error("ClipboardResult has error");
    }

    std::error_code error() const noexcept { return m_error; }
};

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
     * @brief Update clipboard configuration
     * @param config New configuration settings
     */
    void setConfig(const ClipboardConfig& config) noexcept;

    /**
     * @brief Get current clipboard configuration
     * @return Current configuration settings
     */
    [[nodiscard]] const ClipboardConfig& getConfig() const noexcept;

    /**
     * @brief Get performance metrics
     * @return Current performance metrics
     */
    [[nodiscard]] const ClipboardMetrics& getMetrics() const noexcept;

    /**
     * @brief Reset performance metrics
     */
    void resetMetrics() noexcept;

    /**
     * @brief Clear clipboard cache
     */
    void clearCache() noexcept;

    // ============================================================================
    // Asynchronous Operations
    // ============================================================================

    /**
     * @brief Asynchronously set text to clipboard
     * @param text Text to set
     * @return Future that resolves when operation completes
     */
    [[nodiscard]] std::future<ClipboardResult<void>> setTextAsync(
        std::string text);

    /**
     * @brief Asynchronously get text from clipboard
     * @return Future that resolves with clipboard text
     */
    [[nodiscard]] std::future<ClipboardResult<std::string>> getTextAsync();

    /**
     * @brief Asynchronously set binary data to clipboard
     * @param format Data format
     * @param data Data to set
     * @return Future that resolves when operation completes
     */
    [[nodiscard]] std::future<ClipboardResult<void>> setDataAsync(
        ClipboardFormat format, std::vector<std::byte> data);

    /**
     * @brief Asynchronously get binary data from clipboard
     * @param format Data format to retrieve
     * @return Future that resolves with clipboard data
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

    std::unique_ptr<Impl> pImpl;

    // Configuration and metrics
    ClipboardConfig m_config;
    mutable ClipboardMetrics m_metrics;

    // Callback management
    mutable std::mutex m_callbackMutex;
    std::unordered_map<std::size_t, ClipboardChangeCallback> m_callbacks;
    std::atomic<std::size_t> m_nextCallbackId{1};

    // Change monitoring
    mutable std::atomic<bool> m_hasChanged{false};

    // Caching system
    struct CacheEntry {
        std::vector<std::byte> data;
        std::chrono::steady_clock::time_point timestamp{std::chrono::steady_clock::now()};
        ClipboardFormat format{0};

        CacheEntry() = default;
        CacheEntry(std::vector<std::byte> d, ClipboardFormat f)
            : data(std::move(d)), timestamp(std::chrono::steady_clock::now()), format(f) {}
    };
    mutable std::mutex m_cacheMutex;
    mutable std::unordered_map<ClipboardFormat, CacheEntry> m_cache;

    // Thread pool for async operations
    mutable std::mutex m_threadMutex;
    mutable std::vector<std::thread> m_workerThreads;
    mutable std::atomic<bool> m_shutdown{false};

    // Helper methods
    void notifyCallbacks() const noexcept;
    bool isCacheValid(const CacheEntry& entry) const noexcept;
    void cleanupCache() const noexcept;
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
