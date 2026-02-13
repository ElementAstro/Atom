/**
 * @file clipboard.cpp
 * @brief Core clipboard operations: singleton, lifecycle, format registration.
 */

#include "clipboard.hpp"
#include "clipboard_impl.hpp"

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

}  // namespace clip
