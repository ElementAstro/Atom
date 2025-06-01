#if defined(__linux__) && !defined(__APPLE__)

#include "clipboard.ipp"

#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <chrono>
#include <cstring>
#include <thread>
#include <unordered_map>

namespace clip {

// X11/Xlib实现
class LinuxClipboard : public Clipboard::Impl {
public:
    LinuxClipboard() {
        // 打开X display
        m_display = XOpenDisplay(nullptr);
        if (m_display) {
            // 获取原子
            m_atomClipboard = XInternAtom(m_display, "CLIPBOARD", False);
            m_atomUTF8String = XInternAtom(m_display, "UTF8_STRING", False);
            m_atomTargets = XInternAtom(m_display, "TARGETS", False);
            m_atomBitmap = XInternAtom(m_display, "image/bmp", False);
            m_atomPNG = XInternAtom(m_display, "image/png", False);
            m_atomJPEG = XInternAtom(m_display, "image/jpeg", False);

            // 选择窗口
            int screen = DefaultScreen(m_display);
            m_window = XCreateSimpleWindow(
                m_display, RootWindow(m_display, screen), 0, 0, 1, 1, 0, 0, 0);
        }
    }

    ~LinuxClipboard() override {
        if (m_display) {
            if (m_window != None) {
                XDestroyWindow(m_display, m_window);
            }
            XCloseDisplay(m_display);
        }
    }

    bool open() override { return m_display != nullptr; }    void close() noexcept override {
        // 在X11实现中，不需要显式关闭剪贴板
    }

    bool clear() override {
        if (!m_display)
            return false;

        XSetSelectionOwner(m_display, m_atomClipboard, None, CurrentTime);
        XFlush(m_display);

        return true;
    }

    bool setText(std::string_view text) override {
        if (!m_display)
            return false;

        // 分配X服务器内存并复制文本
        XLockDisplay(m_display);
        XChangeProperty(
            m_display, m_window, XA_PRIMARY, XA_STRING, 8, PropModeReplace,
            reinterpret_cast<const unsigned char *>(text.data()), text.size());

        // 声明我们的窗口拥有选择权
        XSetSelectionOwner(m_display, m_atomClipboard, m_window, CurrentTime);
        XFlush(m_display);
        XUnlockDisplay(m_display);

        // 保存文本以备后续请求
        m_text = std::string(text);

        return true;
    }

    std::optional<std::string> getText() override {
        if (!m_display)
            return std::nullopt;

        Window owner = XGetSelectionOwner(m_display, m_atomClipboard);
        if (owner == None) {
            return std::nullopt;
        }

        // 如果我们是剪贴板的拥有者，直接返回保存的文本
        if (owner == m_window && !m_text.empty()) {
            return m_text;
        }

        // 通过选择转换请求数据
        XConvertSelection(m_display, m_atomClipboard, m_atomUTF8String,
                          XA_PRIMARY, m_window, CurrentTime);
        XFlush(m_display);

        // 等待数据到达
        XEvent event;
        for (int tries = 0; tries < 10; ++tries) {
            if (XCheckTypedWindowEvent(m_display, m_window, SelectionNotify,
                                       &event)) {
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }

        // 检查属性
        Atom type;
        int format;
        unsigned long nitems, bytes_after;
        unsigned char *data;

        XGetWindowProperty(m_display, m_window, XA_PRIMARY, 0, 65536, False,
                           AnyPropertyType, &type, &format, &nitems,
                           &bytes_after, &data);

        if (type != m_atomUTF8String && type != XA_STRING) {
            if (data)
                XFree(data);
            return std::nullopt;
        }

        std::string result(reinterpret_cast<char *>(data), nitems);
        XFree(data);

        return result;
    }    bool setData(ClipboardFormat format,
                 std::span<const std::byte> data) override {
        if (!m_display)
            return false;

        // 保存数据以备后续请求
        m_customData[format.value] = std::vector<std::byte>(data.begin(), data.end());

        // 在X11中处理自定义数据格式需要更复杂的实现
        // 暂时只针对一些常见格式实现
        return true;
    }    std::optional<std::vector<std::byte>> getData(
        ClipboardFormat format) override {
        if (!m_display)
            return std::nullopt;

        // 如果我们拥有格式，直接返回
        auto it = m_customData.find(format.value);
        if (it != m_customData.end()) {
            return it->second;
        }

        // 在X11中检索自定义格式的数据需要复杂的选择处理
        // 这里简化实现
        return std::nullopt;
    }

    bool containsFormat(unsigned int format) override {
        if (!m_display)
            return false;

        // 检查本地存储
        if (m_customData.find(format) != m_customData.end()) {
            return true;
        }

        // 查询X服务器以确定是否有特定格式可用
        // 简化实现
        if (format == 1) {  // 假设1是文本格式
            return hasText();
        } else if (format == 2) {  // 假设2是图像格式
            return hasImage();
        }

        return false;
    }

#ifdef CLIPBOARD_SUPPORT_OPENCV
    bool setImage(const cv::Mat &image) override {
        if (!m_display)
            return false;

        // 将OpenCV Mat转换为PNG格式
        std::vector<uchar> buffer;
        cv::imencode(".png", image, buffer);

        // 将PNG数据设置到剪贴板
        std::span<const std::byte> data(
            reinterpret_cast<const std::byte *>(buffer.data()), buffer.size());

        XLockDisplay(m_display);

        // 设置PNG数据
        XChangeProperty(
            m_display, m_window, m_atomPNG, XA_STRING, 8, PropModeReplace,
            reinterpret_cast<const unsigned char *>(data.data()), data.size());

        // 声明我们拥有剪贴板
        XSetSelectionOwner(m_display, m_atomClipboard, m_window, CurrentTime);
        XFlush(m_display);
        XUnlockDisplay(m_display);

        // 保存数据以备后续请求
        m_imageData = std::vector<std::byte>(data.begin(), data.end());
        m_imageFormat = m_atomPNG;

        return true;
    }

    // ClipboardWrapper_linux.cpp (继续)

    std::optional<cv::Mat> getImageAsMat() override {
        if (!m_display)
            return std::nullopt;

        // 如果我们是剪贴板拥有者，直接使用保存的数据
        if (XGetSelectionOwner(m_display, m_atomClipboard) == m_window &&
            !m_imageData.empty()) {
            // 从保存的数据加载图像
            std::vector<uchar> buffer(m_imageData.size());
            std::memcpy(buffer.data(), m_imageData.data(), m_imageData.size());
            return cv::imdecode(buffer, cv::IMREAD_COLOR);
        }

        // 否则从剪贴板请求图像数据
        // 首先尝试PNG，然后是JPEG，然后是BMP
        for (Atom format : {m_atomPNG, m_atomJPEG, m_atomBitmap}) {
            XConvertSelection(m_display, m_atomClipboard, format, format,
                              m_window, CurrentTime);
            XFlush(m_display);

            // 等待数据到达
            XEvent event;
            for (int tries = 0; tries < 10; ++tries) {
                if (XCheckTypedWindowEvent(m_display, m_window, SelectionNotify,
                                           &event)) {
                    break;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }

            // 检查属性
            Atom type;
            int fmt;
            unsigned long nitems, bytes_after;
            unsigned char *data;

            XGetWindowProperty(m_display, m_window, format, 0, 0, False,
                               AnyPropertyType, &type, &fmt, &nitems,
                               &bytes_after, &data);

            if (bytes_after > 0) {
                // 释放任何已经分配的数据
                if (data)
                    XFree(data);

                // 获取完整数据
                XGetWindowProperty(m_display, m_window, format, 0, bytes_after,
                                   False, AnyPropertyType, &type, &fmt, &nitems,
                                   &bytes_after, &data);

                if (data) {
                    // 创建数据副本
                    std::vector<uchar> buffer(data, data + nitems);
                    XFree(data);

                    // 从格式解码图像
                    return cv::imdecode(buffer, cv::IMREAD_COLOR);
                }
            } else if (data) {
                XFree(data);
            }
        }

        return std::nullopt;
    }
#endif

#ifdef CLIPBOARD_SUPPORT_CIMG
    bool setImage(const cimg_library::CImg<unsigned char> &image) override {
        if (!m_display)
            return false;

        // 将CImg转换为内存中的PNG格式
        // 这需要使用临时文件或OpenCV辅助，这里简化处理

        // 创建临时内存缓冲区存储PNG数据
        std::vector<std::byte> buffer;

        // 如果CImg是灰度图，转换为RGB
        cimg_library::CImg<unsigned char> rgb_image = image;
        if (image.spectrum() == 1) {
            rgb_image = image.get_resize(image.width(), image.height(), 1, 3);
            cimg_forXY(rgb_image, x, y) {
                rgb_image(x, y, 0, 0) = rgb_image(x, y, 0, 1) =
                    rgb_image(x, y, 0, 2) = image(x, y);
            }
        }

        // 将CImg保存为临时文件然后加载(简化版本)
        char temp_filename[] = "/tmp/cimg_clipboard_XXXXXX";
        int fd = mkstemp(temp_filename);
        if (fd != -1) {
            close(fd);
            rgb_image.save_png(temp_filename);

            // 读取文件到内存
            FILE *f = fopen(temp_filename, "rb");
            if (f) {
                fseek(f, 0, SEEK_END);
                long size = ftell(f);
                fseek(f, 0, SEEK_SET);

                buffer.resize(size);
                fread(buffer.data(), 1, size, f);
                fclose(f);
            }

            // 删除临时文件
            unlink(temp_filename);
        }

        if (buffer.empty())
            return false;

        // 设置到X剪贴板
        XLockDisplay(m_display);
        XChangeProperty(m_display, m_window, m_atomPNG, XA_STRING, 8,
                        PropModeReplace,
                        reinterpret_cast<const unsigned char *>(buffer.data()),
                        buffer.size());

        XSetSelectionOwner(m_display, m_atomClipboard, m_window, CurrentTime);
        XFlush(m_display);
        XUnlockDisplay(m_display);

        // 保存数据以备后续请求
        m_imageData = buffer;
        m_imageFormat = m_atomPNG;

        return true;
    }

    std::optional<cimg_library::CImg<unsigned char>> getImageAsCImg() override {
        if (!m_display)
            return std::nullopt;

        // 尝试获取图像数据，类似于getImageAsMat的过程
        std::vector<std::byte> imageData;

        // 如果我们是剪贴板拥有者，直接使用保存的数据
        if (XGetSelectionOwner(m_display, m_atomClipboard) == m_window &&
            !m_imageData.empty()) {
            imageData = m_imageData;
        } else {
            // 否则从剪贴板请求图像数据
            for (Atom format : {m_atomPNG, m_atomJPEG, m_atomBitmap}) {
                XConvertSelection(m_display, m_atomClipboard, format, format,
                                  m_window, CurrentTime);
                XFlush(m_display);

                // 等待数据并处理，与getImageAsMat相同
                XEvent event;
                for (int tries = 0; tries < 10; ++tries) {
                    if (XCheckTypedWindowEvent(m_display, m_window,
                                               SelectionNotify, &event)) {
                        break;
                    }
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                }

                Atom type;
                int fmt;
                unsigned long nitems, bytes_after;
                unsigned char *data;

                XGetWindowProperty(m_display, m_window, format, 0, 0, False,
                                   AnyPropertyType, &type, &fmt, &nitems,
                                   &bytes_after, &data);

                if (bytes_after > 0) {
                    if (data)
                        XFree(data);

                    XGetWindowProperty(m_display, m_window, format, 0,
                                       bytes_after, False, AnyPropertyType,
                                       &type, &fmt, &nitems, &bytes_after,
                                       &data);

                    if (data) {
                        imageData.resize(nitems);
                        std::memcpy(imageData.data(), data, nitems);
                        XFree(data);
                        break;
                    }
                } else if (data) {
                    XFree(data);
                }
            }
        }

        if (imageData.empty())
            return std::nullopt;

        // 使用临时文件将数据转换为CImg
        char temp_filename[] = "/tmp/clipboard_img_XXXXXX";
        int fd = mkstemp(temp_filename);
        if (fd == -1)
            return std::nullopt;

        close(fd);

        // 写入临时文件
        FILE *f = fopen(temp_filename, "wb");
        if (!f) {
            unlink(temp_filename);
            return std::nullopt;
        }

        fwrite(imageData.data(), 1, imageData.size(), f);
        fclose(f);

        // 加载CImg
        cimg_library::CImg<unsigned char> result;
        try {
            result.load(temp_filename);
        } catch (...) {
            unlink(temp_filename);
            return std::nullopt;
        }

        // 删除临时文件
        unlink(temp_filename);

        return result;
    }
#endif

    bool hasText() override {
        if (!m_display)
            return false;

        // 检查剪贴板是否包含UTF8_STRING或XA_STRING格式的数据
        Window owner = XGetSelectionOwner(m_display, m_atomClipboard);
        if (owner == None)
            return false;

        // 如果我们拥有剪贴板，检查我们是否有文本
        if (owner == m_window) {
            return !m_text.empty();
        }

        // 请求目标列表
        XConvertSelection(m_display, m_atomClipboard, m_atomTargets, XA_PRIMARY,
                          m_window, CurrentTime);
        XFlush(m_display);

        // 等待响应
        XEvent event;
        for (int tries = 0; tries < 10; ++tries) {
            if (XCheckTypedWindowEvent(m_display, m_window, SelectionNotify,
                                       &event)) {
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }

        // 检查返回的目标
        Atom type;
        int format;
        unsigned long nitems, bytes_after;
        unsigned char *data;

        XGetWindowProperty(m_display, m_window, XA_PRIMARY, 0, 65536, False,
                           AnyPropertyType, &type, &format, &nitems,
                           &bytes_after, &data);

        if (!data)
            return false;

        // 确认返回的是原子列表
        if (type != XA_ATOM) {
            XFree(data);
            return false;
        }

        // 检查列表中是否包含UTF8_STRING或XA_STRING
        Atom *atoms = reinterpret_cast<Atom *>(data);
        bool hasTextFormat = false;

        for (unsigned long i = 0; i < nitems; ++i) {
            if (atoms[i] == m_atomUTF8String || atoms[i] == XA_STRING) {
                hasTextFormat = true;
                break;
            }
        }

        XFree(data);
        return hasTextFormat;
    }

    bool hasImage() override {
        if (!m_display)
            return false;

        // 检查剪贴板是否包含图像格式
        Window owner = XGetSelectionOwner(m_display, m_atomClipboard);
        if (owner == None)
            return false;

        // 如果我们拥有剪贴板，检查我们是否有图像
        if (owner == m_window) {
            return !m_imageData.empty();
        }

        // 请求目标列表
        XConvertSelection(m_display, m_atomClipboard, m_atomTargets, XA_PRIMARY,
                          m_window, CurrentTime);
        XFlush(m_display);

        // 等待响应
        XEvent event;
        for (int tries = 0; tries < 10; ++tries) {
            if (XCheckTypedWindowEvent(m_display, m_window, SelectionNotify,
                                       &event)) {
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }

        // 检查返回的目标
        Atom type;
        int format;
        unsigned long nitems, bytes_after;
        unsigned char *data;

        XGetWindowProperty(m_display, m_window, XA_PRIMARY, 0, 65536, False,
                           AnyPropertyType, &type, &format, &nitems,
                           &bytes_after, &data);

        if (!data)
            return false;

        // 确认返回的是原子列表
        if (type != XA_ATOM) {
            XFree(data);
            return false;
        }

        // 检查列表中是否包含图像格式
        Atom *atoms = reinterpret_cast<Atom *>(data);
        bool hasImageFormat = false;

        for (unsigned long i = 0; i < nitems; ++i) {
            if (atoms[i] == m_atomBitmap || atoms[i] == m_atomPNG ||
                atoms[i] == m_atomJPEG) {
                hasImageFormat = true;
                break;
            }
        }

        XFree(data);
        return hasImageFormat;
    }    std::vector<ClipboardFormat> getAvailableFormats() override {
        std::vector<ClipboardFormat> formats;

        if (!m_display)
            return formats;

        // 请求目标列表
        XConvertSelection(m_display, m_atomClipboard, m_atomTargets, XA_PRIMARY,
                          m_window, CurrentTime);
        XFlush(m_display);

        // 等待响应
        XEvent event;
        for (int tries = 0; tries < 10; ++tries) {
            if (XCheckTypedWindowEvent(m_display, m_window, SelectionNotify,
                                       &event)) {
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }

        // 检查返回的目标
        Atom type;
        int format;
        unsigned long nitems, bytes_after;
        unsigned char *data;

        XGetWindowProperty(m_display, m_window, XA_PRIMARY, 0, 65536, False,
                           AnyPropertyType, &type, &format, &nitems,
                           &bytes_after, &data);

        if (!data)
            return formats;

        // 确认返回的是原子列表
        if (type != XA_ATOM) {
            XFree(data);
            return formats;
        }

        // 获取原子列表
        Atom *atoms = reinterpret_cast<Atom *>(data);        for (unsigned long i = 0; i < nitems; ++i) {
            // 使用Atom值作为format ID
            formats.push_back(ClipboardFormat{static_cast<unsigned int>(atoms[i])});
        }

        XFree(data);
        return formats;
    }    std::optional<std::string> getFormatName(ClipboardFormat format) override {
        if (!m_display)
            return std::nullopt;

        // 转换为X11 Atom
        Atom atom = static_cast<Atom>(format.value);

        // 获取Atom名称
        char *name = XGetAtomName(m_display, atom);
        if (!name)
            return std::nullopt;

        std::string result(name);
        XFree(name);

        return result;
    }

private:
    Display *m_display = nullptr;
    Window m_window = None;

    // 常用原子
    Atom m_atomClipboard = None;
    Atom m_atomUTF8String = None;
    Atom m_atomTargets = None;
    Atom m_atomBitmap = None;
    Atom m_atomPNG = None;
    Atom m_atomJPEG = None;

    // 缓存的剪贴板数据
    std::string m_text;
    std::vector<std::byte> m_imageData;
    Atom m_imageFormat = None;

    // 自定义数据格式缓存
    std::unordered_map<unsigned int, std::vector<std::byte>> m_customData;
};

// 静态工厂方法
std::unique_ptr<Clipboard::Impl> Clipboard::Impl::create() {
    return std::make_unique<LinuxClipboard>();
}

// 静态格式注册方法
ClipboardFormat Clipboard::Impl::registerFormat(std::string_view formatName) {
    // 获取X display
    Display *display = XOpenDisplay(nullptr);
    if (!display)
        return ClipboardFormat{0};

    // 创建一个Atom
    Atom atom = XInternAtom(display, formatName.data(), False);

    // 关闭display
    XCloseDisplay(display);

    return ClipboardFormat{static_cast<unsigned int>(atom)};
}

}  // namespace clip

#endif  // defined(__linux__) && !defined(__APPLE__)
