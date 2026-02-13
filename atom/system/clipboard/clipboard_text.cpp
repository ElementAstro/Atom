/**
 * @file clipboard_text.cpp
 * @brief Text clipboard operations with Unicode support and caching.
 */

#include "clipboard.hpp"
#include "clipboard_impl.hpp"

#include <cstring>

namespace clip {

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
                reinterpret_cast<const std::byte*>(result->data() +
                                                   result->size())),
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

}  // namespace clip
