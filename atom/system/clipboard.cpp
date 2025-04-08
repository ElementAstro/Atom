#include "clipboard.hpp"
#include "clipboard.ipp"

namespace clip {

Clipboard& Clipboard::instance() {
    static Clipboard instance;
    return instance;
}

Clipboard::Clipboard() : pImpl(Impl::create()) {}

Clipboard::~Clipboard() = default;

bool Clipboard::open() { return pImpl->open(); }

void Clipboard::close() { pImpl->close(); }

bool Clipboard::clear() { return pImpl->clear(); }

bool Clipboard::setText(std::string_view text) { return pImpl->setText(text); }

std::optional<std::string> Clipboard::getText() { return pImpl->getText(); }

bool Clipboard::setData(unsigned int format, std::span<const std::byte> data) {
    return pImpl->setData(format, data);
}

std::optional<std::vector<std::byte>> Clipboard::getData(unsigned int format) {
    return pImpl->getData(format);
}

bool Clipboard::containsFormat(unsigned int format) {
    return pImpl->containsFormat(format);
}

#ifdef CLIPBOARD_SUPPORT_OPENCV
bool Clipboard::setImage(const cv::Mat& image) {
    return pImpl->setImage(image);
}

std::optional<cv::Mat> Clipboard::getImageAsMat() {
    return pImpl->getImageAsMat();
}
#endif

#ifdef CLIPBOARD_SUPPORT_CIMG
bool Clipboard::setImage(const cimg_library::CImg<unsigned char>& image) {
    return pImpl->setImage(image);
}

std::optional<cimg_library::CImg<unsigned char>> Clipboard::getImageAsCImg() {
    return pImpl->getImageAsCImg();
}
#endif

// 实现通用图像处理函数
template <HasWidthHeightChannelsAccess ImageT>
bool Clipboard::setGenericImage(const ImageT& image) {
#ifdef CLIPBOARD_SUPPORT_OPENCV
    // 转换为OpenCV Mat并使用它
    cv::Mat converted(image.rows, image.cols, CV_8UC(image.channels()),
                      image.data);
    return setImage(converted);
#else
    // 基本实现...
    return false;
#endif
}

bool Clipboard::hasText() { return pImpl->hasText(); }

bool Clipboard::hasImage() { return pImpl->hasImage(); }

std::vector<unsigned int> Clipboard::getAvailableFormats() {
    return pImpl->getAvailableFormats();
}

std::optional<std::string> Clipboard::getFormatName(unsigned int format) {
    return pImpl->getFormatName(format);
}

unsigned int Clipboard::registerFormat(std::string_view formatName) {
    return Impl::registerFormat(formatName);
}

}  // namespace clip