/**
 * @file clipboard_image.cpp
 * @brief Image clipboard operations (OpenCV and CImg support).
 */

#include "clipboard.hpp"
#include "clipboard_impl.hpp"

namespace clip {

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

}  // namespace clip
