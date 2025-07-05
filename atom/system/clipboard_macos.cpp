#if defined(__APPLE__)

#include "clipboard.ipp"
#include "clipboard_error.hpp"

#include <AppKit/AppKit.h>
#include <ApplicationServices/ApplicationServices.h>
#include <CoreFoundation/CoreFoundation.h>
#include <Foundation/Foundation.h>

#include <chrono>
#include <thread>
#include <unordered_map>

namespace clip {

namespace {
// Helper class for managing CFString resources
class CFStringWrapper {
public:
    explicit CFStringWrapper(CFStringRef str) : m_string(str) {}

    ~CFStringWrapper() {
        if (m_string) {
            CFRelease(m_string);
        }
    }

    CFStringWrapper(const CFStringWrapper&) = delete;
    CFStringWrapper& operator=(const CFStringWrapper&) = delete;

    CFStringWrapper(CFStringWrapper&& other) noexcept
        : m_string(other.m_string) {
        other.m_string = nullptr;
    }

    CFStringWrapper& operator=(CFStringWrapper&& other) noexcept {
        if (this != &other) {
            if (m_string) {
                CFRelease(m_string);
            }
            m_string = other.m_string;
            other.m_string = nullptr;
        }
        return *this;
    }

    CFStringRef get() const noexcept { return m_string; }
    operator bool() const noexcept { return m_string != nullptr; }

private:
    CFStringRef m_string;
};

// Helper function to create CFString from std::string
CFStringWrapper createCFString(std::string_view text) {
    CFStringRef str = CFStringCreateWithBytes(
        kCFAllocatorDefault, reinterpret_cast<const UInt8*>(text.data()),
        text.size(), kCFStringEncodingUTF8, false);
    return CFStringWrapper(str);
}

// Helper function to convert CFString to std::string
std::string cfStringToString(CFStringRef cfStr) {
    if (!cfStr) {
        return {};
    }

    CFIndex length = CFStringGetLength(cfStr);
    CFIndex maxSize =
        CFStringGetMaximumSizeForEncoding(length, kCFStringEncodingUTF8);

    std::string result(maxSize, '\0');
    CFIndex actualSize = 0;

    Boolean success = CFStringGetBytes(
        cfStr, CFRangeMake(0, length), kCFStringEncodingUTF8, 0, false,
        reinterpret_cast<UInt8*>(result.data()), maxSize, &actualSize);

    if (success) {
        result.resize(actualSize);
        return result;
    }

    return {};
}

// Convert error to ClipboardErrorCode
ClipboardErrorCode osStatusToErrorCode(OSStatus status) {
    switch (status) {
        case noErr:
            return ClipboardErrorCode::SUCCESS;
        case paramErr:
            return ClipboardErrorCode::INVALID_DATA;
        case memFullErr:
            return ClipboardErrorCode::OUT_OF_MEMORY;
        case fnfErr:
            return ClipboardErrorCode::FORMAT_NOT_SUPPORTED;
        default:
            return ClipboardErrorCode::SYSTEM_ERROR;
    }
}
}  // namespace

// macOS Pasteboard implementation
class MacOSClipboard : public Clipboard::Impl {
public:
    MacOSClipboard() {
        @autoreleasepool {
            m_pasteboard = [NSPasteboard generalPasteboard];
            if (!m_pasteboard) {
                throw ClipboardSystemException(
                    "Failed to access general pasteboard");
            }
            m_changeCount = [m_pasteboard changeCount];
        }
    }

    ~MacOSClipboard() override = default;

    bool open() override {
        // macOS pasteboard doesn't require explicit opening
        return m_pasteboard != nullptr;
    }

    void close() override {
        // macOS pasteboard doesn't require explicit closing
    }

    bool clear() override {
        @autoreleasepool {
            if (!m_pasteboard) {
                return false;
            }

            [m_pasteboard clearContents];
            m_changeCount = [m_pasteboard changeCount];
            return true;
        }
    }

    bool setText(std::string_view text) override {
        @autoreleasepool {
            if (!m_pasteboard) {
                return false;
            }

            try {
                NSString* nsString =
                    [[NSString alloc] initWithBytes:text.data()
                                             length:text.size()
                                           encoding:NSUTF8StringEncoding];

                if (!nsString) {
                    throw ClipboardFormatException(
                        "Failed to create NSString from text");
                }

                auto guard =
                    make_scope_guard([nsString] { [nsString release]; });

                [m_pasteboard clearContents];
                BOOL success = [m_pasteboard setString:nsString
                                               forType:NSPasteboardTypeString];

                if (success) {
                    m_changeCount = [m_pasteboard changeCount];
                    return true;
                }

                return false;
            } catch (const std::exception&) {
                return false;
            }
        }
    }

    std::optional<std::string> getText() override {
        @autoreleasepool {
            if (!m_pasteboard) {
                return std::nullopt;
            }

            try {
                NSString* string =
                    [m_pasteboard stringForType:NSPasteboardTypeString];
                if (!string) {
                    return std::nullopt;
                }

                const char* cString = [string UTF8String];
                if (!cString) {
                    return std::nullopt;
                }

                return std::string(cString);
            } catch (const std::exception&) {
                return std::nullopt;
            }
        }
    }

    bool setData(unsigned int format,
                 std::span<const std::byte> data) override {
        @autoreleasepool {
            if (!m_pasteboard) {
                return false;
            }

            try {
                // Convert format to pasteboard type
                NSString* pasteboardType = formatToPasteboardType(format);
                if (!pasteboardType) {
                    return false;
                }

                NSData* nsData =
                    [NSData dataWithBytes:data.data() length:data.size()];
                if (!nsData) {
                    return false;
                }

                [m_pasteboard clearContents];
                BOOL success =
                    [m_pasteboard setData:nsData forType:pasteboardType];

                if (success) {
                    m_changeCount = [m_pasteboard changeCount];
                    return true;
                }

                return false;
            } catch (const std::exception&) {
                return false;
            }
        }
    }

    std::optional<std::vector<std::byte>> getData(
        unsigned int format) override {
        @autoreleasepool {
            if (!m_pasteboard) {
                return std::nullopt;
            }

            try {
                NSString* pasteboardType = formatToPasteboardType(format);
                if (!pasteboardType) {
                    return std::nullopt;
                }

                NSData* data = [m_pasteboard dataForType:pasteboardType];
                if (!data) {
                    return std::nullopt;
                }

                std::vector<std::byte> result(data.length);
                std::memcpy(result.data(), data.bytes, data.length);

                return result;
            } catch (const std::exception&) {
                return std::nullopt;
            }
        }
    }

    bool containsFormat(unsigned int format) override {
        @autoreleasepool {
            if (!m_pasteboard) {
                return false;
            }

            NSString* pasteboardType = formatToPasteboardType(format);
            if (!pasteboardType) {
                return false;
            }

            NSArray* types = [m_pasteboard types];
            return [types containsObject:pasteboardType];
        }
    }

#ifdef CLIPBOARD_SUPPORT_OPENCV
    bool setImage(const cv::Mat& image) override {
        @autoreleasepool {
            if (!m_pasteboard || image.empty()) {
                return false;
            }

            try {
                // Convert cv::Mat to NSImage
                cv::Mat rgbImage;
                if (image.channels() == 4) {
                    cv::cvtColor(image, rgbImage, cv::COLOR_BGRA2RGB);
                } else if (image.channels() == 3) {
                    cv::cvtColor(image, rgbImage, cv::COLOR_BGR2RGB);
                } else {
                    rgbImage = image.clone();
                }

                NSBitmapImageRep* imageRep = [[NSBitmapImageRep alloc]
                    initWithBitmapDataPlanes:nil
                                  pixelsWide:rgbImage.cols
                                  pixelsHigh:rgbImage.rows
                               bitsPerSample:8
                             samplesPerPixel:rgbImage.channels()
                                    hasAlpha:(rgbImage.channels() == 4)
                                    isPlanar:NO
                              colorSpaceName:NSCalibratedRGBColorSpace
                                 bytesPerRow:rgbImage.step
                                bitsPerPixel:rgbImage.channels() * 8];

                if (!imageRep) {
                    return false;
                }

                auto guard =
                    make_scope_guard([imageRep] { [imageRep release]; });

                std::memcpy([imageRep bitmapData], rgbImage.data,
                            rgbImage.total() * rgbImage.elemSize());

                NSImage* nsImage = [[NSImage alloc] init];
                [nsImage addRepresentation:imageRep];

                auto imageGuard =
                    make_scope_guard([nsImage] { [nsImage release]; });

                [m_pasteboard clearContents];
                BOOL success = [m_pasteboard writeObjects:@[ nsImage ]];

                if (success) {
                    m_changeCount = [m_pasteboard changeCount];
                    return true;
                }

                return false;
            } catch (const std::exception&) {
                return false;
            }
        }
    }

    std::optional<cv::Mat> getImageAsMat() override {
        @autoreleasepool {
            if (!m_pasteboard) {
                return std::nullopt;
            }

            try {
                NSArray* images =
                    [m_pasteboard readObjectsForClasses:@[ [NSImage class] ]
                                                options:nil];
                if (!images || images.count == 0) {
                    return std::nullopt;
                }

                NSImage* image = [images firstObject];
                if (!image) {
                    return std::nullopt;
                }

                NSBitmapImageRep* imageRep = nil;
                for (NSImageRep* rep in [image representations]) {
                    if ([rep isKindOfClass:[NSBitmapImageRep class]]) {
                        imageRep = (NSBitmapImageRep*)rep;
                        break;
                    }
                }

                if (!imageRep) {
                    // Convert to bitmap representation
                    NSSize imageSize = [image size];
                    [image lockFocus];
                    imageRep = [[NSBitmapImageRep alloc]
                        initWithFocusedViewRect:NSMakeRect(0, 0,
                                                           imageSize.width,
                                                           imageSize.height)];
                    [image unlockFocus];

                    if (!imageRep) {
                        return std::nullopt;
                    }
                }

                auto guard =
                    make_scope_guard([imageRep] { [imageRep release]; });

                int width = (int)[imageRep pixelsWide];
                int height = (int)[imageRep pixelsHigh];
                int channels = (int)[imageRep samplesPerPixel];

                cv::Mat result(height, width, CV_8UC(channels));

                unsigned char* imageData = [imageRep bitmapData];
                if (imageData) {
                    std::memcpy(result.data, imageData,
                                result.total() * result.elemSize());

                    // Convert from RGB to BGR for OpenCV
                    if (channels == 3) {
                        cv::cvtColor(result, result, cv::COLOR_RGB2BGR);
                    } else if (channels == 4) {
                        cv::cvtColor(result, result, cv::COLOR_RGBA2BGRA);
                    }
                }

                return result;
            } catch (const std::exception&) {
                return std::nullopt;
            }
        }
    }
#endif

#ifdef CLIPBOARD_SUPPORT_CIMG
    bool setImage(const cimg_library::CImg<unsigned char>& image) override {
        // Convert CImg to cv::Mat and use OpenCV implementation
#ifdef CLIPBOARD_SUPPORT_OPENCV
        cv::Mat mat(image.height(), image.width(), CV_8UC(image.spectrum()));

        if (image.spectrum() == 1) {
            // Grayscale
            std::memcpy(mat.data, image.data(), image.size());
        } else if (image.spectrum() == 3) {
            // RGB to BGR conversion
            for (int y = 0; y < image.height(); ++y) {
                for (int x = 0; x < image.width(); ++x) {
                    mat.at<cv::Vec3b>(y, x)[0] = image(x, y, 0, 2);  // B
                    mat.at<cv::Vec3b>(y, x)[1] = image(x, y, 0, 1);  // G
                    mat.at<cv::Vec3b>(y, x)[2] = image(x, y, 0, 0);  // R
                }
            }
        }

        return setImage(mat);
#else
        return false;
#endif
    }

    std::optional<cimg_library::CImg<unsigned char>> getImageAsCImg() override {
#ifdef CLIPBOARD_SUPPORT_OPENCV
        auto mat = getImageAsMat();
        if (!mat) {
            return std::nullopt;
        }

        cimg_library::CImg<unsigned char> result(mat->cols, mat->rows, 1,
                                                 mat->channels());

        if (mat->channels() == 1) {
            std::memcpy(result.data(), mat->data, mat->total());
        } else if (mat->channels() == 3) {
            // BGR to RGB conversion
            for (int y = 0; y < mat->rows; ++y) {
                for (int x = 0; x < mat->cols; ++x) {
                    result(x, y, 0, 0) = mat->at<cv::Vec3b>(y, x)[2];  // R
                    result(x, y, 0, 1) = mat->at<cv::Vec3b>(y, x)[1];  // G
                    result(x, y, 0, 2) = mat->at<cv::Vec3b>(y, x)[0];  // B
                }
            }
        }

        return result;
#else
        return std::nullopt;
#endif
    }
#endif

    bool hasText() override {
        @autoreleasepool {
            if (!m_pasteboard) {
                return false;
            }

            NSArray* types = [m_pasteboard types];
            return [types containsObject:NSPasteboardTypeString];
        }
    }

    bool hasImage() override {
        @autoreleasepool {
            if (!m_pasteboard) {
                return false;
            }

            NSArray* types = [m_pasteboard types];
            return [types containsObject:NSPasteboardTypeTIFF] ||
                   [types containsObject:NSPasteboardTypePNG];
        }
    }

    std::vector<unsigned int> getAvailableFormats() override {
        @autoreleasepool {
            std::vector<unsigned int> formats;

            if (!m_pasteboard) {
                return formats;
            }

            NSArray* types = [m_pasteboard types];
            for (NSString* type in types) {
                unsigned int format = pasteboardTypeToFormat(type);
                if (format != 0) {
                    formats.push_back(format);
                }
            }

            return formats;
        }
    }

    std::optional<std::string> getFormatName(unsigned int format) override {
        NSString* pasteboardType = formatToPasteboardType(format);
        if (pasteboardType) {
            return std::string([pasteboardType UTF8String]);
        }
        return std::nullopt;
    }

    // Clipboard change monitoring
    bool hasChanged() const {
        @autoreleasepool {
            if (!m_pasteboard) {
                return false;
            }

            NSInteger currentChangeCount = [m_pasteboard changeCount];
            return currentChangeCount != m_changeCount;
        }
    }

    void updateChangeCount() {
        @autoreleasepool {
            if (m_pasteboard) {
                m_changeCount = [m_pasteboard changeCount];
            }
        }
    }

private:
    NSPasteboard* m_pasteboard = nullptr;
    NSInteger m_changeCount = 0;

    // Format conversion mappings
    std::unordered_map<unsigned int, NSString*> m_formatToType = {
        {1, NSPasteboardTypeString},
        {2, NSPasteboardTypeHTML},
        {3, NSPasteboardTypeTIFF},
        {4, NSPasteboardTypePNG},
        {5, NSPasteboardTypeRTF}};

    std::unordered_map<NSString*, unsigned int> m_typeToFormat;

    NSString* formatToPasteboardType(unsigned int format) {
        auto it = m_formatToType.find(format);
        return (it != m_formatToType.end()) ? it->second : nil;
    }

    unsigned int pasteboardTypeToFormat(NSString* type) {
        if (m_typeToFormat.empty()) {
            // Initialize reverse mapping
            for (const auto& pair : m_formatToType) {
                m_typeToFormat[pair.second] = pair.first;
            }
        }

        auto it = m_typeToFormat.find(type);
        return (it != m_typeToFormat.end()) ? it->second : 0;
    }
};

// Static factory method
std::unique_ptr<Clipboard::Impl> Clipboard::Impl::create() {
    return std::make_unique<MacOSClipboard>();
}

// Static format registration method
unsigned int Clipboard::Impl::registerFormat(std::string_view formatName) {
    @autoreleasepool {
        NSString* nsFormatName =
            [[NSString alloc] initWithBytes:formatName.data()
                                     length:formatName.size()
                                   encoding:NSUTF8StringEncoding];

        if (!nsFormatName) {
            return 0;
        }

        auto guard =
            make_scope_guard([nsFormatName] { [nsFormatName release]; });

        // In macOS, we use the string directly as the pasteboard type
        // Return a hash of the format name as the format ID
        std::hash<std::string> hasher;
        return static_cast<unsigned int>(hasher(std::string(formatName)));
    }
}

}  // namespace clip

#endif  // defined(__APPLE__)
