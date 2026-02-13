/**
 * @file clipboard_query.cpp
 * @brief Clipboard query operations for format inspection.
 */

#include "clipboard.hpp"
#include "clipboard_impl.hpp"

namespace clip {

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

}  // namespace clip
