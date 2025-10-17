#include "clipboard.hpp"
#include "clipboard.ipp"
#include "clipboard_error.hpp"

namespace clip {

// ============================================================================
// Singleton Instance Management
// ============================================================================

Clipboard& Clipboard::instance() noexcept {
    static Clipboard instance;
    return instance;
}

Clipboard::Clipboard() : pImpl(Impl::create()) {
    if (!pImpl) {
        throw ClipboardSystemException(
            "Failed to create clipboard implementation");
    }
}

Clipboard::~Clipboard() = default;

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
    if (!pImpl->setText(text)) {
        throw ClipboardSystemException("Failed to set clipboard text");
    }
    m_hasChanged = true;
    notifyCallbacks();
}

ClipboardResult<void> Clipboard::setTextSafe(std::string_view text) noexcept {
    try {
        setText(text);
        return ClipboardResult<void>{};
    } catch (const ClipboardException& e) {
        return ClipboardResult<void>{e.code()};
    } catch (...) {
        return ClipboardResult<void>{
            make_error_code(ClipboardErrorCode::SYSTEM_ERROR)};
    }
}

std::string Clipboard::getText() {
    auto result = pImpl->getText();
    if (!result) {
        throw ClipboardFormatException("No text available on clipboard");
    }
    return std::move(*result);
}

ClipboardResult<std::string> Clipboard::getTextSafe() noexcept {
    try {
        return ClipboardResult<std::string>{getText()};
    } catch (const ClipboardException& e) {
        return ClipboardResult<std::string>{e.code()};
    } catch (...) {
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

}  // namespace clip
