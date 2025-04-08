#pragma once

#include "clipboard.hpp"

namespace clip {
class Clipboard::Impl {
public:
    virtual ~Impl() = default;

    virtual bool open() = 0;
    virtual void close() = 0;
    virtual bool clear() = 0;

    virtual bool setText(std::string_view text) = 0;
    virtual std::optional<std::string> getText() = 0;

    virtual bool setData(unsigned int format,
                         std::span<const std::byte> data) = 0;
    virtual std::optional<std::vector<std::byte>> getData(
        unsigned int format) = 0;

    virtual bool containsFormat(unsigned int format) = 0;

#ifdef CLIPBOARD_SUPPORT_OPENCV
    virtual bool setImage(const cv::Mat& image) = 0;
    virtual std::optional<cv::Mat> getImageAsMat() = 0;
#endif

#ifdef CLIPBOARD_SUPPORT_CIMG
    virtual bool setImage(const cimg_library::CImg<unsigned char>& image) = 0;
    virtual std::optional<cimg_library::CImg<unsigned char>>
    getImageAsCImg() = 0;
#endif

    virtual bool hasText() = 0;
    virtual bool hasImage() = 0;

    virtual std::vector<unsigned int> getAvailableFormats() = 0;
    virtual std::optional<std::string> getFormatName(unsigned int format) = 0;

    static std::unique_ptr<Impl> create();

    static unsigned int registerFormat(std::string_view formatName);
};

}  // namespace clip