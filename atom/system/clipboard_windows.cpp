#ifdef _WIN32

#include "clipboard.ipp"

#include <windows.h>

#include <cstring>
#include <format>
#include <stdexcept>

namespace clip {

namespace {
/**
 * @brief Creates a bitmap handle from DIB (Device Independent Bitmap) data
 * @param bmi Bitmap info structure
 * @param bits Raw bitmap pixel data
 * @return HBITMAP handle if successful, nullptr on failure
 */
HBITMAP CreateBitmapFromDIB(const BITMAPINFO* bmi, const void* bits) {
    return CreateDIBitmap(GetDC(nullptr), &bmi->bmiHeader, CBM_INIT, bits, bmi,
                          DIB_RGB_COLORS);
}

/**
 * @brief Extracts DIB (Device Independent Bitmap) data from a clipboard bitmap
 * @param hBitmap Bitmap handle to extract data from
 * @return Pair containing bitmap info structure and raw pixel data
 * @throws std::runtime_error If bitmap data cannot be accessed
 */
std::pair<std::unique_ptr<BITMAPINFO>, std::unique_ptr<std::byte[]>>
GetDIBFromClipboard(HBITMAP hBitmap) {
    if (!hBitmap) {
        throw std::runtime_error("Invalid bitmap handle");
    }

    BITMAP bm;
    if (!GetObject(hBitmap, sizeof(BITMAP), &bm)) {
        throw std::runtime_error("Failed to get bitmap information");
    }

    // Create bitmap info header
    BITMAPINFOHEADER bi{};
    bi.biSize = sizeof(BITMAPINFOHEADER);
    bi.biWidth = bm.bmWidth;
    bi.biHeight = bm.bmHeight;
    bi.biPlanes = 1;
    bi.biBitCount = 24;  // Use 24-bit bitmap
    bi.biCompression = BI_RGB;

    // Calculate stride (bytes per row, 4-byte aligned)
    int stride = ((bi.biWidth * bi.biBitCount + 31) / 32) * 4;
    int imageSize = stride * bi.biHeight;

    // Allocate memory
    auto bmi = std::make_unique<BITMAPINFO>();
    auto bits = std::make_unique<std::byte[]>(imageSize);

    if (!bmi || !bits) {
        throw std::runtime_error("Failed to allocate memory for bitmap data");
    }

    // Set bitmap info header
    bmi->bmiHeader = bi;

    // Get bitmap data
    HDC hdc = GetDC(nullptr);
    if (!hdc) {
        throw std::runtime_error("Failed to get device context");
    }

    int result = GetDIBits(hdc, hBitmap, 0, bi.biHeight, bits.get(), bmi.get(),
                           DIB_RGB_COLORS);
    ReleaseDC(nullptr, hdc);

    if (!result) {
        throw std::runtime_error("Failed to get bitmap bits");
    }

    return {std::move(bmi), std::move(bits)};
}
}  // namespace

// Windows implementation
class WindowsClipboard : public Clipboard::Impl {
public:
    WindowsClipboard() = default;

    ~WindowsClipboard() override {
        try {
            if (m_isOpen) {
                CloseClipboard();
            }
        } catch (...) {
            // Suppress exceptions in destructor
        }
    }

    bool open() override {
        if (m_isOpen)
            return true;

        m_isOpen = OpenClipboard(nullptr);
        if (!m_isOpen) {
            // Get the last error message for debugging
            DWORD errorCode = GetLastError();
            // We don't throw here because failure to open is a normal condition
            // that calling code should handle
        }

        return m_isOpen;
    }

    void close() noexcept override {
        if (m_isOpen) {
            if (!CloseClipboard()) {
                // Get the last error message for debugging
                DWORD errorCode = GetLastError();
                // Log but don't throw exception here as this is a cleanup
                // operation
            }
            m_isOpen = false;
        }
    }

    bool clear() override {
        try {
            if (!open())
                return false;

            bool result = EmptyClipboard();
            DWORD errorCode = result ? 0 : GetLastError();

            close();
            return result;
        } catch (const std::exception& e) {
            // Ensure clipboard is closed even if an exception occurs
            try {
                close();
            } catch (...) {
            }
            return false;
        }
    }

    bool setText(std::string_view text) override {
        try {
            if (!open())
                return false;

            if (!EmptyClipboard()) {
                close();
                return false;
            }

            // Calculate required memory size (including null terminator)
            size_t memSize = text.size() + 1;

            // Allocate global memory
            HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, memSize);
            if (!hMem) {
                close();
                return false;
            }

            // Lock memory and get pointer
            char* pMem = static_cast<char*>(GlobalLock(hMem));
            if (!pMem) {
                GlobalFree(hMem);
                close();
                return false;
            }

            // Copy text to memory
            std::memcpy(pMem, text.data(), text.size());
            pMem[text.size()] = '\0';  // Add null terminator

            // Unlock memory
            GlobalUnlock(hMem);

            // Set clipboard data
            HANDLE result = SetClipboardData(CF_TEXT, hMem);

            close();

            // If setting failed, free allocated memory
            if (!result) {
                GlobalFree(hMem);
                return false;
            }

            return true;
        } catch (const std::exception& e) {
            // Ensure clipboard is closed and memory is freed if an exception
            // occurs
            try {
                close();
            } catch (...) {
            }
            return false;
        }
    }

    std::optional<std::string> getText() override {
        try {
            if (!open())
                return std::nullopt;

            HANDLE hData = GetClipboardData(CF_TEXT);
            if (!hData) {
                close();
                return std::nullopt;
            }

            // Lock memory and get pointer
            char* pText = static_cast<char*>(GlobalLock(hData));
            if (!pText) {
                close();
                return std::nullopt;
            }

            // Copy text (safely handle potential null pointer)
            std::string text = pText ? pText : "";

            // Unlock memory
            GlobalUnlock(hData);
            close();

            return text;
        } catch (const std::exception& e) {
            // Ensure clipboard is closed if an exception occurs
            try {
                close();
            } catch (...) {
            }
            return std::nullopt;
        }
    }
    bool setData(ClipboardFormat format,
                 std::span<const std::byte> data) override {
        try {
            if (!open())
                return false;

            if (!EmptyClipboard()) {
                close();
                return false;
            }

            // Skip if data is empty
            if (data.empty()) {
                close();
                return false;
            }

            // Allocate global memory
            HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, data.size());
            if (!hMem) {
                close();
                return false;
            }

            // Lock memory and get pointer
            void* pMem = GlobalLock(hMem);
            if (!pMem) {
                GlobalFree(hMem);
                close();
                return false;
            }

            // Copy data to memory
            std::memcpy(pMem, data.data(), data.size());

            // Unlock memory
            GlobalUnlock(hMem);

            // Set clipboard data
            HANDLE result = SetClipboardData(format, hMem);

            close();

            // If setting failed, free allocated memory
            if (!result) {
                GlobalFree(hMem);
                return false;
            }

            return true;
        } catch (const std::exception& e) {
            // Ensure clipboard is closed and memory is freed if an exception
            // occurs
            try {
                close();
            } catch (...) {
            }
            return false;
        }
    }
    std::optional<std::vector<std::byte>> getData(
        ClipboardFormat format) override {
        try {
            if (!open())
                return std::nullopt;

            HANDLE hData = GetClipboardData(format);
            if (!hData) {
                close();
                return std::nullopt;
            }

            // Lock memory and get pointer
            void* pData = GlobalLock(hData);
            if (!pData) {
                close();
                return std::nullopt;
            }

            // Get data size
            SIZE_T size = GlobalSize(hData);
            if (size == 0) {
                GlobalUnlock(hData);
                close();
                return std::nullopt;
            }

            // Create data buffer
            std::vector<std::byte> buffer(size);

            // Copy data
            std::memcpy(buffer.data(), pData, size);

            // Unlock memory
            GlobalUnlock(hData);
            close();

            return buffer;
        } catch (const std::exception& e) {
            // Ensure clipboard is closed if an exception occurs
            try {
                close();
            } catch (...) {
            }
            return std::nullopt;
        }
    }

    bool containsFormat(ClipboardFormat format) override {
        try {
            if (!open())
                return false;

            bool result = IsClipboardFormatAvailable(format);

            close();

            return result;
        } catch (const std::exception& e) {
            // Ensure clipboard is closed if an exception occurs
            try {
                close();
            } catch (...) {
            }
            return false;
        }
    }

#ifdef CLIPBOARD_SUPPORT_OPENCV
    bool setImage(const cv::Mat& image) override {
        try {
            if (image.empty()) {
                return false;
            }

            // Ensure image is continuous, create a copy if not
            cv::Mat continuous = image.isContinuous() ? image : image.clone();

            // Create bitmap info header
            BITMAPINFOHEADER bi{};
            bi.biSize = sizeof(BITMAPINFOHEADER);
            bi.biWidth = continuous.cols;
            bi.biHeight =
                -continuous.rows;  // Negative height indicates top-down DIB
            bi.biPlanes = 1;
            bi.biBitCount = continuous.channels() * 8;  // 8 bits per channel
            bi.biCompression = BI_RGB;

            // Create bitmap info
            BITMAPINFO info{};
            info.bmiHeader = bi;

            // Create DIB bitmap
            void* bits = continuous.data;
            HBITMAP hBitmap =
                CreateDIBitmap(GetDC(nullptr), &info.bmiHeader, CBM_INIT, bits,
                               &info, DIB_RGB_COLORS);

            if (!hBitmap) {
                return false;
            }

            // Open clipboard and set bitmap
            if (!open()) {
                DeleteObject(hBitmap);
                return false;
            }

            if (!EmptyClipboard()) {
                DeleteObject(hBitmap);
                close();
                return false;
            }

            HANDLE result = SetClipboardData(CF_BITMAP, hBitmap);
            close();

            if (!result) {
                DeleteObject(hBitmap);
                return false;
            }

            return true;
        } catch (const std::exception& e) {
            // Ensure clipboard is closed if an exception occurs
            try {
                close();
            } catch (...) {
            }
            return false;
        }
    }

    std::optional<cv::Mat> getImageAsMat() override {
        try {
            if (!containsFormat(CF_BITMAP)) {
                return std::nullopt;
            }

            if (!open()) {
                return std::nullopt;
            }

            HBITMAP hBitmap = static_cast<HBITMAP>(GetClipboardData(CF_BITMAP));
            if (!hBitmap) {
                close();
                return std::nullopt;
            }

            // Use scope guard to ensure clipboard is closed
            auto closeGuard = [this]() { this->close(); };
            std::unique_ptr<void, decltype(closeGuard)> guard(nullptr,
                                                              closeGuard);

            auto [bmi, bits] = GetDIBFromClipboard(hBitmap);

            // Create OpenCV Mat
            int height = bmi->bmiHeader.biHeight;
            int width = bmi->bmiHeader.biWidth;
            int channels = bmi->bmiHeader.biBitCount / 8;

            // OpenCV expects BGR order, Windows DIB is also BGR, so no
            // conversion needed
            cv::Mat image(std::abs(height), width, CV_8UC3);

            // Calculate bytes per row including padding
            int stride = ((width * bmi->bmiHeader.biBitCount + 31) / 32) * 4;

            // Copy data (handle vertical flip)
            if (height > 0) {  // Bottom-up DIB
                for (int y = 0; y < height; ++y) {
                    std::memcpy(
                        image.data + (height - 1 - y) * image.step,
                        static_cast<const std::byte*>(bits.get()) + y * stride,
                        width * channels);
                }
            } else {  // Top-down DIB
                height = -height;
                for (int y = 0; y < height; ++y) {
                    std::memcpy(
                        image.data + y * image.step,
                        static_cast<const std::byte*>(bits.get()) + y * stride,
                        width * channels);
                }
            }

            // Cleanup is handled by the guard
            return image;
        } catch (const std::exception& e) {
            // Ensure clipboard is closed if an exception occurs
            try {
                close();
            } catch (...) {
            }
            return std::nullopt;
        }
    }
#endif

#ifdef CLIPBOARD_SUPPORT_CIMG
    bool setImage(const cimg_library::CImg<unsigned char>& image) override {
        try {
            if (image.is_empty()) {
                return false;
            }

            // Ensure image has appropriate channel count
            cimg_library::CImg<unsigned char> continuous;
            if (image.spectrum() == 1) {
                // Grayscale image, convert to RGB
                continuous =
                    image.get_resize(image.width(), image.height(), 1, 3);
                cimg_forXY(continuous, x, y) {
                    continuous(x, y, 0, 0) = continuous(x, y, 0, 1) =
                        continuous(x, y, 0, 2) = image(x, y);
                }
            } else if (image.spectrum() == 3 || image.spectrum() == 4) {
                continuous =
                    image.get_resize(image.width(), image.height(), 1, 3);
            } else {
                return false;
            }

            // CImg uses planar storage (all R channels, then all G channels,
            // then all B channels) Windows DIB requires interleaved format
            // (BGRBGRBGR...), so we need to rearrange
            std::vector<unsigned char> interleavedData(continuous.width() *
                                                       continuous.height() * 3);

            for (int y = 0; y < continuous.height(); ++y) {
                for (int x = 0; x < continuous.width(); ++x) {
                    interleavedData[(y * continuous.width() + x) * 3 + 0] =
                        continuous(x, y, 0, 2);  // B
                    interleavedData[(y * continuous.width() + x) * 3 + 1] =
                        continuous(x, y, 0, 1);  // G
                    interleavedData[(y * continuous.width() + x) * 3 + 2] =
                        continuous(x, y, 0, 0);  // R
                }
            }

            // Create bitmap info header
            BITMAPINFOHEADER bi{};
            bi.biSize = sizeof(BITMAPINFOHEADER);
            bi.biWidth = continuous.width();
            bi.biHeight =
                -continuous.height();  // Negative height indicates top-down DIB
            bi.biPlanes = 1;
            bi.biBitCount = 24;  // 24-bit BGR
            bi.biCompression = BI_RGB;

            // Create bitmap info
            BITMAPINFO info{};
            info.bmiHeader = bi;

            // Create DIB bitmap
            HBITMAP hBitmap =
                CreateDIBitmap(GetDC(nullptr), &info.bmiHeader, CBM_INIT,
                               interleavedData.data(), &info, DIB_RGB_COLORS);

            if (!hBitmap) {
                return false;
            }

            // Open clipboard and set bitmap
            if (!open()) {
                DeleteObject(hBitmap);
                return false;
            }

            if (!EmptyClipboard()) {
                DeleteObject(hBitmap);
                close();
                return false;
            }

            HANDLE result = SetClipboardData(CF_BITMAP, hBitmap);
            close();

            if (!result) {
                DeleteObject(hBitmap);
                return false;
            }

            return true;
        } catch (const std::exception& e) {
            // Ensure clipboard is closed if an exception occurs
            try {
                close();
            } catch (...) {
            }
            return false;
        }
    }

    std::optional<cimg_library::CImg<unsigned char>> getImageAsCImg() override {
        try {
            if (!containsFormat(CF_BITMAP)) {
                return std::nullopt;
            }

            if (!open()) {
                return std::nullopt;
            }

            HBITMAP hBitmap = static_cast<HBITMAP>(GetClipboardData(CF_BITMAP));
            if (!hBitmap) {
                close();
                return std::nullopt;
            }

            // Use scope guard to ensure clipboard is closed
            auto closeGuard = [this]() { this->close(); };
            std::unique_ptr<void, decltype(closeGuard)> guard(nullptr,
                                                              closeGuard);

            auto [bmi, bits] = GetDIBFromClipboard(hBitmap);

            // Create CImg
            int height = std::abs(bmi->bmiHeader.biHeight);
            int width = bmi->bmiHeader.biWidth;
            int channels = bmi->bmiHeader.biBitCount / 8;

            cimg_library::CImg<unsigned char> image(width, height, 1, 3);

            // Calculate bytes per row including padding
            int stride = ((width * bmi->bmiHeader.biBitCount + 31) / 32) * 4;

            // DIB stores as BGR, CImg stores as RGB and uses planar format, so
            // conversion is needed
            for (int y = 0; y < height; ++y) {
                for (int x = 0; x < width; ++x) {
                    int srcIdx;
                    if (bmi->bmiHeader.biHeight > 0) {  // Bottom-up DIB
                        srcIdx = (height - 1 - y) * stride + x * channels;
                    } else {  // Top-down DIB
                        srcIdx = y * stride + x * channels;
                    }

                    const unsigned char* pBits =
                        static_cast<const unsigned char*>(
                            static_cast<const void*>(bits.get()));
                    image(x, y, 0, 0) = pBits[srcIdx + 2];  // R
                    image(x, y, 0, 1) = pBits[srcIdx + 1];  // G
                    image(x, y, 0, 2) = pBits[srcIdx + 0];  // B
                }
            }

            // Cleanup is handled by the guard
            return image;
        } catch (const std::exception& e) {
            // Ensure clipboard is closed if an exception occurs
            try {
                close();
            } catch (...) {
            }
            return std::nullopt;
        }
    }
#endif

    bool hasText() override { return containsFormat(ClipboardFormat{CF_TEXT}); }

    bool hasImage() override {
        return containsFormat(ClipboardFormat{CF_BITMAP});
    }

    // ============================================================================
    // Change Monitoring Implementation
    // ============================================================================

    bool hasChanged() const override {
        // Windows doesn't provide built-in change detection,
        // so we'll use a simple sequence number approach
        DWORD currentSequence = GetClipboardSequenceNumber();
        if (currentSequence != m_lastSequenceNumber) {
            m_lastSequenceNumber = currentSequence;
            return true;
        }
        return false;
    }

    void updateChangeCount() override {
        m_lastSequenceNumber = GetClipboardSequenceNumber();
    }
    std::vector<ClipboardFormat> getAvailableFormats() override {
        try {
            if (!open())
                return {};

            std::vector<ClipboardFormat> formats;
            UINT format = 0;

            while ((format = EnumClipboardFormats(format)) != 0) {
                formats.push_back(ClipboardFormat{format});
            }

            close();
            return formats;
        } catch (const std::exception& e) {
            // Ensure clipboard is closed if an exception occurs
            try {
                close();
            } catch (...) {
            }
            return {};
        }
    }
    std::optional<std::string> getFormatName(ClipboardFormat format) override {
        try {
            char name[256] = {0};
            int result =
                GetClipboardFormatNameA(format.value, name, sizeof(name));

            if (result == 0) {
                // Handle standard formats
                switch (format.value) {
                    case CF_TEXT:
                        return "CF_TEXT";
                    case CF_BITMAP:
                        return "CF_BITMAP";
                    case CF_METAFILEPICT:
                        return "CF_METAFILEPICT";
                    case CF_SYLK:
                        return "CF_SYLK";
                    case CF_DIF:
                        return "CF_DIF";
                    case CF_TIFF:
                        return "CF_TIFF";
                    case CF_OEMTEXT:
                        return "CF_OEMTEXT";
                    case CF_DIB:
                        return "CF_DIB";
                    case CF_PALETTE:
                        return "CF_PALETTE";
                    case CF_PENDATA:
                        return "CF_PENDATA";
                    case CF_RIFF:
                        return "CF_RIFF";
                    case CF_WAVE:
                        return "CF_WAVE";
                    case CF_UNICODETEXT:
                        return "CF_UNICODETEXT";
                    case CF_ENHMETAFILE:
                        return "CF_ENHMETAFILE";
                    case CF_HDROP:
                        return "CF_HDROP";
                    case CF_LOCALE:
                        return "CF_LOCALE";
                    case CF_DIBV5:
                        return "CF_DIBV5";
                    default:
                        return std::format("Unknown Format ({})", format.value);
                }
            }

            return std::string(name);
        } catch (const std::exception& e) {
            return std::format("Error Format ({})", format.value);
        }
    }

private:
    bool m_isOpen = false;
    mutable DWORD m_lastSequenceNumber = 0;
};

// Static factory method
std::unique_ptr<Clipboard::Impl> Clipboard::Impl::create() {
    return std::make_unique<WindowsClipboard>();
}

// Static format registration method
ClipboardFormat Clipboard::Impl::registerFormat(std::string_view formatName) {
    return ClipboardFormat{RegisterClipboardFormatA(formatName.data())};
}

}  // namespace clip

#endif  // _WIN32
