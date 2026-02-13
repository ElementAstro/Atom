/**
 * @file clipboard_monitor.cpp
 * @brief Clipboard change monitoring and callback management.
 */

#include "clipboard.hpp"
#include "clipboard_impl.hpp"

namespace clip {

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

}  // namespace clip
