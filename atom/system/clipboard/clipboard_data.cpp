/**
 * @file clipboard_data.cpp
 * @brief Binary data clipboard operations with zero-copy support.
 */

#include "clipboard.hpp"
#include "clipboard_impl.hpp"

namespace clip {

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

}  // namespace clip
