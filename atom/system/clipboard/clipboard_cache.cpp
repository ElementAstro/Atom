/**
 * @file clipboard_cache.cpp
 * @brief Clipboard cache management and configuration/metrics access.
 */

#include "clipboard.hpp"

namespace clip {

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

}  // namespace clip
