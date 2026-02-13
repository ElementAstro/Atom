/**
 * @file clipboard_async.cpp
 * @brief Asynchronous clipboard operations and thread management.
 */

#include "clipboard.hpp"
#include "clipboard_impl.hpp"

#include <cstring>
#include <future>

namespace clip {

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
            std::memcpy(entry.data.data(), result.value().data(),
                        result.value().size());
            entry.timestamp = std::chrono::steady_clock::now();
            entry.format = formats::TEXT;
            m_cache[formats::TEXT] = std::move(entry);
        }

        return result;
    });
}

std::future<ClipboardResult<void>> Clipboard::setDataAsync(
    ClipboardFormat format, std::vector<std::byte> data) {
    return std::async(
        std::launch::async, [this, format, data = std::move(data)]() {
            if (m_config.enable_metrics) {
                m_metrics.operations_count++;
                m_metrics.bytes_transferred += data.size();
            }
            return setDataSafe(format, std::span<const std::byte>(data));
        });
}

std::future<ClipboardResult<std::vector<std::byte>>> Clipboard::getDataAsync(
    ClipboardFormat format) {
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
