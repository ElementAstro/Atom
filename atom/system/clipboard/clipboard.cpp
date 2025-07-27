#include "clipboard.hpp"
#include "clipboard.ipp"
#include "clipboard_error.hpp"

#include <cstring>
#include <future>

namespace clip {

// ============================================================================
// Singleton Instance Management
// ============================================================================

Clipboard& Clipboard::instance() noexcept {
    static Clipboard instance;
    return instance;
}

Clipboard::Clipboard() : pImpl(Impl::create()), m_config{}, m_metrics{} {
    if (!pImpl) {
        throw ClipboardSystemException(
            "Failed to create clipboard implementation");
    }
}

Clipboard::~Clipboard() {
    try {
        shutdownThreads();
        if (pImpl) {
            pImpl->close();
        }
    } catch (...) {
        // Suppress exceptions in destructor
    }
}

// ============================================================================
// Core Operations with Exception Safety
// ============================================================================

void Clipboard::open() {
    if (!pImpl->open()) {
        throw ClipboardAccessDeniedException("Failed to open clipboard");
    }
}

void Clipboard::close() noexcept {
    try {
        pImpl->close();
    } catch (...) {
        // Ensure noexcept guarantee
    }
}

void Clipboard::clear() {
    if (!pImpl->clear()) {
        throw ClipboardSystemException("Failed to clear clipboard");
    }
}

// ============================================================================
// Text Operations with Unicode Support
// ============================================================================

void Clipboard::setText(std::string_view text) {
    if (m_config.enable_metrics) {
        m_metrics.operations_count++;
        m_metrics.bytes_transferred += text.size();
    }

    if (!pImpl->setText(text)) {
        if (m_config.enable_metrics) {
            m_metrics.errors_count++;
        }
        throw ClipboardSystemException("Failed to set clipboard text");
    }

    // Invalidate cache since clipboard content changed
    if (m_config.enable_caching) {
        std::lock_guard<std::mutex> lock(m_cacheMutex);
        m_cache.clear();
    }

    m_hasChanged = true;
    notifyCallbacks();
}

ClipboardResult<void> Clipboard::setTextSafe(std::string_view text) noexcept {
    try {
        setText(text);
        return ClipboardResult<void>{};
    } catch (const ClipboardException& e) {
        if (m_config.enable_metrics) {
            m_metrics.errors_count++;
        }
        return ClipboardResult<void>{e.code()};
    } catch (...) {
        if (m_config.enable_metrics) {
            m_metrics.errors_count++;
        }
        return ClipboardResult<void>{
            make_error_code(ClipboardErrorCode::SYSTEM_ERROR)};
    }
}

std::string Clipboard::getText() {
    if (m_config.enable_metrics) {
        m_metrics.operations_count++;
    }

    // Check cache first
    if (m_config.enable_caching) {
        std::lock_guard<std::mutex> lock(m_cacheMutex);
        auto it = m_cache.find(formats::TEXT);
        if (it != m_cache.end() && isCacheValid(it->second)) {
            if (m_config.enable_metrics) {
                m_metrics.cache_hits++;
            }
            // Convert cached binary data back to string
            return std::string(
                reinterpret_cast<const char*>(it->second.data.data()),
                it->second.data.size());
        }
        if (m_config.enable_metrics) {
            m_metrics.cache_misses++;
        }
    }

    auto result = pImpl->getText();
    if (!result) {
        if (m_config.enable_metrics) {
            m_metrics.errors_count++;
        }
        throw ClipboardFormatException("No text available on clipboard");
    }

    // Cache the result
    if (m_config.enable_caching) {
        std::lock_guard<std::mutex> lock(m_cacheMutex);
        CacheEntry entry(
            std::vector<std::byte>(
                reinterpret_cast<const std::byte*>(result->data()),
                reinterpret_cast<const std::byte*>(result->data() + result->size())),
            formats::TEXT);
        m_cache[formats::TEXT] = std::move(entry);
    }

    return std::move(*result);
}

ClipboardResult<std::string> Clipboard::getTextSafe() noexcept {
    try {
        return ClipboardResult<std::string>{getText()};
    } catch (const ClipboardException& e) {
        if (m_config.enable_metrics) {
            m_metrics.errors_count++;
        }
        return ClipboardResult<std::string>{e.code()};
    } catch (...) {
        if (m_config.enable_metrics) {
            m_metrics.errors_count++;
        }
        return ClipboardResult<std::string>{
            make_error_code(ClipboardErrorCode::SYSTEM_ERROR)};
    }
}

// ============================================================================
// Binary Data Operations with Zero-Copy Support
// ============================================================================

void Clipboard::setData(ClipboardFormat format,
                        std::span<const std::byte> data) {
    if (!pImpl->setData(format, data)) {
        throw ClipboardSystemException("Failed to set clipboard data");
    }
    m_hasChanged = true;
    notifyCallbacks();
}

ClipboardResult<void> Clipboard::setDataSafe(
    ClipboardFormat format, std::span<const std::byte> data) noexcept {
    try {
        setData(format, data);
        return ClipboardResult<void>{};
    } catch (const ClipboardException& e) {
        return ClipboardResult<void>{e.code()};
    } catch (...) {
        return ClipboardResult<void>{
            make_error_code(ClipboardErrorCode::SYSTEM_ERROR)};
    }
}

std::vector<std::byte> Clipboard::getData(ClipboardFormat format) {
    auto result = pImpl->getData(format);
    if (!result) {
        throw ClipboardFormatException(
            "Requested format not available on clipboard");
    }
    return std::move(*result);
}

ClipboardResult<std::vector<std::byte>> Clipboard::getDataSafe(
    ClipboardFormat format) noexcept {
    try {
        return ClipboardResult<std::vector<std::byte>>{getData(format)};
    } catch (const ClipboardException& e) {
        return ClipboardResult<std::vector<std::byte>>{e.code()};
    } catch (...) {
        return ClipboardResult<std::vector<std::byte>>{
            make_error_code(ClipboardErrorCode::SYSTEM_ERROR)};
    }
}

bool Clipboard::containsFormat(ClipboardFormat format) const noexcept {
    try {
        return pImpl->containsFormat(format);
    } catch (...) {
        return false;
    }
}

// ============================================================================
// Image Operations with Modern C++ Features
// ============================================================================

#ifdef CLIPBOARD_SUPPORT_OPENCV
void Clipboard::setImage(const cv::Mat& image) {
    if (!pImpl->setImage(image)) {
        throw ClipboardSystemException("Failed to set clipboard image");
    }
    m_hasChanged = true;
    notifyCallbacks();
}

ClipboardResult<void> Clipboard::setImageSafe(const cv::Mat& image) noexcept {
    try {
        setImage(image);
        return ClipboardResult<void>{};
    } catch (const ClipboardException& e) {
        return ClipboardResult<void>{e.code()};
    } catch (...) {
        return ClipboardResult<void>{
            make_error_code(ClipboardErrorCode::SYSTEM_ERROR)};
    }
}

cv::Mat Clipboard::getImageAsMat() {
    auto result = pImpl->getImageAsMat();
    if (!result) {
        throw ClipboardFormatException("No image available on clipboard");
    }
    return std::move(*result);
}

ClipboardResult<cv::Mat> Clipboard::getImageAsMatSafe() noexcept {
    try {
        return ClipboardResult<cv::Mat>{getImageAsMat()};
    } catch (const ClipboardException& e) {
        return ClipboardResult<cv::Mat>{e.code()};
    } catch (...) {
        return ClipboardResult<cv::Mat>{
            make_error_code(ClipboardErrorCode::SYSTEM_ERROR)};
    }
}
#endif

#ifdef CLIPBOARD_SUPPORT_CIMG
void Clipboard::setImage(const cimg_library::CImg<unsigned char>& image) {
    if (!pImpl->setImage(image)) {
        throw ClipboardSystemException("Failed to set clipboard image");
    }
    m_hasChanged = true;
    notifyCallbacks();
}

ClipboardResult<void> Clipboard::setImageSafe(
    const cimg_library::CImg<unsigned char>& image) noexcept {
    try {
        setImage(image);
        return ClipboardResult<void>{};
    } catch (const ClipboardException& e) {
        return ClipboardResult<void>{e.code()};
    } catch (...) {
        return ClipboardResult<void>{
            make_error_code(ClipboardErrorCode::SYSTEM_ERROR)};
    }
}

cimg_library::CImg<unsigned char> Clipboard::getImageAsCImg() {
    auto result = pImpl->getImageAsCImg();
    if (!result) {
        throw ClipboardFormatException("No image available on clipboard");
    }
    return std::move(*result);
}

ClipboardResult<cimg_library::CImg<unsigned char>>
Clipboard::getImageAsCImgSafe() noexcept {
    try {
        return ClipboardResult<cimg_library::CImg<unsigned char>>{
            getImageAsCImg()};
    } catch (const ClipboardException& e) {
        return ClipboardResult<cimg_library::CImg<unsigned char>>{e.code()};
    } catch (...) {
        return ClipboardResult<cimg_library::CImg<unsigned char>>{
            make_error_code(ClipboardErrorCode::SYSTEM_ERROR)};
    }
}
#endif

// ============================================================================
// Query Operations with Performance Optimization
// ============================================================================

bool Clipboard::hasText() const noexcept {
    try {
        return pImpl->hasText();
    } catch (...) {
        return false;
    }
}

bool Clipboard::hasImage() const noexcept {
    try {
        return pImpl->hasImage();
    } catch (...) {
        return false;
    }
}

std::vector<ClipboardFormat> Clipboard::getAvailableFormats() const {
    auto result = pImpl->getAvailableFormats();
    return result;
}

ClipboardResult<std::vector<ClipboardFormat>>
Clipboard::getAvailableFormatsSafe() const noexcept {
    try {
        return ClipboardResult<std::vector<ClipboardFormat>>{
            getAvailableFormats()};
    } catch (const ClipboardException& e) {
        return ClipboardResult<std::vector<ClipboardFormat>>{e.code()};
    } catch (...) {
        return ClipboardResult<std::vector<ClipboardFormat>>{
            make_error_code(ClipboardErrorCode::SYSTEM_ERROR)};
    }
}

std::string Clipboard::getFormatName(ClipboardFormat format) const {
    auto result = pImpl->getFormatName(format);
    if (!result) {
        throw ClipboardFormatException("Unknown clipboard format");
    }
    return *result;
}

ClipboardResult<std::string> Clipboard::getFormatNameSafe(
    ClipboardFormat format) const noexcept {
    try {
        return ClipboardResult<std::string>{getFormatName(format)};
    } catch (const ClipboardException& e) {
        return ClipboardResult<std::string>{e.code()};
    } catch (...) {
        return ClipboardResult<std::string>{
            make_error_code(ClipboardErrorCode::SYSTEM_ERROR)};
    }
}

// ============================================================================
// Clipboard Change Monitoring with Callback Mechanism
// ============================================================================

std::size_t Clipboard::registerChangeCallback(
    ClipboardChangeCallback callback) {
    std::lock_guard<std::mutex> lock(m_callbackMutex);

    if (!callback) {
        return 0;
    }

    std::size_t id = m_nextCallbackId++;
    m_callbacks[id] = std::move(callback);
    return id;
}

bool Clipboard::unregisterChangeCallback(std::size_t callbackId) noexcept {
    std::lock_guard<std::mutex> lock(m_callbackMutex);
    return m_callbacks.erase(callbackId) > 0;
}

bool Clipboard::hasChanged() const noexcept {
    // Check both internal flag and platform-specific change detection
    bool internalChanged = m_hasChanged.load();
    bool platformChanged = false;

    try {
        platformChanged = pImpl->hasChanged();
    } catch (...) {
        // Ignore errors in change detection
    }

    return internalChanged || platformChanged;
}

void Clipboard::markChangeProcessed() noexcept {
    m_hasChanged = false;
    try {
        pImpl->updateChangeCount();
    } catch (...) {
        // Ignore errors in change count update
    }
}

void Clipboard::notifyCallbacks() const noexcept {
    std::lock_guard<std::mutex> lock(m_callbackMutex);

    for (const auto& [id, callback] : m_callbacks) {
        try {
            if (callback) {
                callback();
            }
        } catch (...) {
            // Callbacks should not throw, but protect against it
        }
    }
}

// ============================================================================
// Static Format Registration
// ============================================================================

ClipboardFormat Clipboard::registerFormat(std::string_view formatName) {
    auto result = Impl::registerFormat(formatName);
    if (result.value == 0) {
        throw ClipboardSystemException("Failed to register clipboard format");
    }
    return result;
}

ClipboardResult<ClipboardFormat> Clipboard::registerFormatSafe(
    std::string_view formatName) noexcept {
    try {
        return ClipboardResult<ClipboardFormat>{registerFormat(formatName)};
    } catch (const ClipboardException& e) {
        return ClipboardResult<ClipboardFormat>{e.code()};
    } catch (...) {
        return ClipboardResult<ClipboardFormat>{
            make_error_code(ClipboardErrorCode::SYSTEM_ERROR)};
    }
}

// ============================================================================
// Configuration and Performance Management
// ============================================================================

void Clipboard::setConfig(const ClipboardConfig& config) noexcept {
    m_config = config;
}

const ClipboardConfig& Clipboard::getConfig() const noexcept {
    return m_config;
}

const ClipboardMetrics& Clipboard::getMetrics() const noexcept {
    return m_metrics;
}

void Clipboard::resetMetrics() noexcept {
    m_metrics.reset();
}

void Clipboard::clearCache() noexcept {
    std::lock_guard<std::mutex> lock(m_cacheMutex);
    m_cache.clear();
}

// ============================================================================
// Cache Management Helper Methods
// ============================================================================

bool Clipboard::isCacheValid(const CacheEntry& entry) const noexcept {
    if (!m_config.enable_caching) {
        return false;
    }

    auto now = std::chrono::steady_clock::now();
    auto age = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - entry.timestamp);

    return age < m_config.cache_ttl;
}

void Clipboard::cleanupCache() const noexcept {
    std::lock_guard<std::mutex> lock(m_cacheMutex);

    auto now = std::chrono::steady_clock::now();
    auto it = m_cache.begin();

    while (it != m_cache.end()) {
        auto age = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - it->second.timestamp);

        if (age >= m_config.cache_ttl) {
            it = m_cache.erase(it);
        } else {
            ++it;
        }
    }
}

// ============================================================================
// Asynchronous Operations
// ============================================================================

std::future<ClipboardResult<void>> Clipboard::setTextAsync(std::string text) {
    return std::async(std::launch::async, [this, text = std::move(text)]() {
        if (m_config.enable_metrics) {
            m_metrics.operations_count++;
        }
        return setTextSafe(text);
    });
}

std::future<ClipboardResult<std::string>> Clipboard::getTextAsync() {
    return std::async(std::launch::async, [this]() {
        if (m_config.enable_metrics) {
            m_metrics.operations_count++;
        }

        // Check cache first
        if (m_config.enable_caching) {
            std::lock_guard<std::mutex> lock(m_cacheMutex);
            auto it = m_cache.find(formats::TEXT);
            if (it != m_cache.end() && isCacheValid(it->second)) {
                if (m_config.enable_metrics) {
                    m_metrics.cache_hits++;
                }
                // Convert cached binary data back to string
                std::string result(
                    reinterpret_cast<const char*>(it->second.data.data()),
                    it->second.data.size());
                return ClipboardResult<std::string>{std::move(result)};
            }
            if (m_config.enable_metrics) {
                m_metrics.cache_misses++;
            }
        }

        auto result = getTextSafe();

        // Cache the result if successful
        if (result && m_config.enable_caching) {
            std::lock_guard<std::mutex> lock(m_cacheMutex);
            CacheEntry entry;
            entry.data.resize(result.value().size());
            std::memcpy(entry.data.data(), result.value().data(), result.value().size());
            entry.timestamp = std::chrono::steady_clock::now();
            entry.format = formats::TEXT;
            m_cache[formats::TEXT] = std::move(entry);
        }

        return result;
    });
}

std::future<ClipboardResult<void>> Clipboard::setDataAsync(
    ClipboardFormat format, std::vector<std::byte> data) {
    return std::async(std::launch::async,
        [this, format, data = std::move(data)]() {
            if (m_config.enable_metrics) {
                m_metrics.operations_count++;
                m_metrics.bytes_transferred += data.size();
            }
            return setDataSafe(format, std::span<const std::byte>(data));
        });
}

std::future<ClipboardResult<std::vector<std::byte>>>
Clipboard::getDataAsync(ClipboardFormat format) {
    return std::async(std::launch::async, [this, format]() {
        if (m_config.enable_metrics) {
            m_metrics.operations_count++;
        }

        // Check cache first
        if (m_config.enable_caching) {
            std::lock_guard<std::mutex> lock(m_cacheMutex);
            auto it = m_cache.find(format);
            if (it != m_cache.end() && isCacheValid(it->second)) {
                if (m_config.enable_metrics) {
                    m_metrics.cache_hits++;
                }
                return ClipboardResult<std::vector<std::byte>>{it->second.data};
            }
            if (m_config.enable_metrics) {
                m_metrics.cache_misses++;
            }
        }

        auto result = getDataSafe(format);

        // Cache the result if successful
        if (result && m_config.enable_caching) {
            std::lock_guard<std::mutex> lock(m_cacheMutex);
            CacheEntry entry;
            entry.data = result.value();
            entry.timestamp = std::chrono::steady_clock::now();
            entry.format = format;
            m_cache[format] = std::move(entry);

            if (m_config.enable_metrics) {
                m_metrics.bytes_transferred += result.value().size();
            }
        }

        return result;
    });
}

// ============================================================================
// Thread Management
// ============================================================================

void Clipboard::shutdownThreads() noexcept {
    m_shutdown = true;

    std::lock_guard<std::mutex> lock(m_threadMutex);
    for (auto& thread : m_workerThreads) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    m_workerThreads.clear();
}

}  // namespace clip
