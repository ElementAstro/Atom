/**
 * @file clipboard.ipp
 * @brief Backwards compatibility header for clipboard implementation.
 *
 * @deprecated This header location is deprecated. Please use
 * "atom/system/clipboard/clipboard.ipp" instead.
 */

<<<<<<<< HEAD:atom/system/clipboard/clipboard.ipp
    // ============================================================================
    // Core Operations
    // ============================================================================

    virtual bool open() = 0;
    virtual void close() noexcept = 0;
    virtual bool clear() = 0;

    // ============================================================================
    // Text Operations
    // ============================================================================

    virtual bool setText(std::string_view text) = 0;
    virtual std::optional<std::string> getText() = 0;

    // ============================================================================
    // Binary Data Operations
    // ============================================================================

    virtual bool setData(ClipboardFormat format,
                         std::span<const std::byte> data) = 0;
    virtual std::optional<std::vector<std::byte>> getData(
        ClipboardFormat format) = 0;
    virtual bool containsFormat(ClipboardFormat format) = 0;

    // ============================================================================
    // Image Operations
    // ============================================================================

#ifdef CLIPBOARD_SUPPORT_OPENCV
    virtual bool setImage(const cv::Mat& image) = 0;
    virtual std::optional<cv::Mat> getImageAsMat() = 0;
#endif

#ifdef CLIPBOARD_SUPPORT_CIMG
    virtual bool setImage(const cimg_library::CImg<unsigned char>& image) = 0;
    virtual std::optional<cimg_library::CImg<unsigned char>>
    getImageAsCImg() = 0;
#endif

    // ============================================================================
    // Query Operations
    // ============================================================================

    virtual bool hasText() = 0;
    virtual bool hasImage() = 0;
    virtual std::vector<ClipboardFormat> getAvailableFormats() = 0;
    virtual std::optional<std::string> getFormatName(
        ClipboardFormat format) = 0;

    // ============================================================================
    // Change Monitoring
    // ============================================================================

    virtual bool hasChanged() const { return false; }
    virtual void updateChangeCount() {}

    // ============================================================================
    // Static Factory Methods
    // ============================================================================

    static std::unique_ptr<Impl> create();
    static ClipboardFormat registerFormat(std::string_view formatName);
};

}  // namespace clip
========
// Forward to the new location
#include "clipboard/clipboard.ipp"
>>>>>>>> test-fixes/systematic-testing:atom/system/clipboard.ipp
