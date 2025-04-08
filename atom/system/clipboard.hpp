#pragma once

#include <concepts>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

#ifdef CLIPBOARD_SUPPORT_OPENCV
#include <opencv2/opencv.hpp>
#endif

#ifdef CLIPBOARD_SUPPORT_CIMG
#include <CImg.h>
#endif

/**
 * @brief Concept for checking if an image type has required properties for
 * clipboard operations
 *
 * This concept checks if an image type has the following accessible properties:
 * - cols (width)
 * - rows (height)
 * - channels (color channels)
 * - data (pointer to raw pixel data)
 */
template <typename ImageT>
concept HasWidthHeightChannelsAccess = requires(ImageT img) {
    { img.cols } -> std::convertible_to<int>;
    { img.rows } -> std::convertible_to<int>;
    { img.channels() } -> std::convertible_to<int>;
    { img.data } -> std::convertible_to<void*>;
};

namespace clip {

/**
 * @class Clipboard
 * @brief Cross-platform clipboard operations wrapper class
 *
 * This class provides a unified interface for clipboard operations across
 * different platforms. It supports text, binary data, and image clipboard
 * operations with optional support for OpenCV and CImg libraries.
 *
 * The class is implemented as a singleton to ensure proper clipboard state
 * management.
 */
class Clipboard {
public:
    /**
     * @brief Get the singleton instance of the Clipboard
     * @return Reference to the singleton Clipboard instance
     */
    static Clipboard& instance();

    /**
     * @brief Destructor
     *
     * Ensures proper cleanup of clipboard resources
     */
    ~Clipboard();

    /**
     * @brief Open the clipboard for operations
     * @return true if clipboard was successfully opened, false otherwise
     *
     * @note Must be called before most clipboard operations
     */
    bool open();

    /**
     * @brief Close the clipboard
     *
     * Releases the clipboard after operations are complete
     */
    void close();

    /**
     * @brief Clear the clipboard contents
     * @return true if clipboard was successfully cleared, false otherwise
     */
    bool clear();

    /**
     * @brief Set text to the clipboard
     * @param text The text to place on the clipboard
     * @return true if operation succeeded, false otherwise
     */
    bool setText(std::string_view text);

    /**
     * @brief Get text from the clipboard
     * @return Optional containing the clipboard text if available, empty
     * optional otherwise
     */
    std::optional<std::string> getText();

    /**
     * @brief Set binary data to the clipboard in a specific format
     * @param format The clipboard format identifier
     * @param data The binary data to place on the clipboard
     * @return true if operation succeeded, false otherwise
     */
    bool setData(unsigned int format, std::span<const std::byte> data);

    /**
     * @brief Get binary data from the clipboard
     * @param format The clipboard format identifier to retrieve
     * @return Optional containing the binary data if available, empty optional
     * otherwise
     */
    std::optional<std::vector<std::byte>> getData(unsigned int format);

    /**
     * @brief Check if clipboard contains data in a specific format
     * @param format The clipboard format identifier to check
     * @return true if format is available, false otherwise
     */
    bool containsFormat(unsigned int format);

#ifdef CLIPBOARD_SUPPORT_OPENCV
    /**
     * @brief Set OpenCV Mat image to the clipboard
     * @param image The OpenCV Mat image to place on the clipboard
     * @return true if operation succeeded, false otherwise
     */
    bool setImage(const cv::Mat& image);

    /**
     * @brief Get image from clipboard as OpenCV Mat
     * @return Optional containing the image if available, empty optional
     * otherwise
     */
    std::optional<cv::Mat> getImageAsMat();
#endif

#ifdef CLIPBOARD_SUPPORT_CIMG
    /**
     * @brief Set CImg image to the clipboard
     * @param image The CImg image to place on the clipboard
     * @return true if operation succeeded, false otherwise
     */
    bool setImage(const cimg_library::CImg<unsigned char>& image);

    /**
     * @brief Get image from clipboard as CImg
     * @return Optional containing the image if available, empty optional
     * otherwise
     */
    std::optional<cimg_library::CImg<unsigned char>> getImageAsCImg();
#endif

    /**
     * @brief Generic image setting function using concepts
     * @tparam ImageT Image type that satisfies HasWidthHeightChannelsAccess
     * concept
     * @param image The image to place on the clipboard
     * @return true if operation succeeded, false otherwise
     */
    template <HasWidthHeightChannelsAccess ImageT>
    bool setGenericImage(const ImageT& image);

    /**
     * @brief Check if clipboard contains text data
     * @return true if text is available, false otherwise
     */
    bool hasText();

    /**
     * @brief Check if clipboard contains image data
     * @return true if image is available, false otherwise
     */
    bool hasImage();

    /**
     * @brief Get list of available clipboard formats
     * @return Vector of format identifiers currently available on the clipboard
     */
    std::vector<unsigned int> getAvailableFormats();

    /**
     * @brief Get human-readable name for a clipboard format
     * @param format The format identifier
     * @return Optional containing the format name if known, empty optional
     * otherwise
     */
    std::optional<std::string> getFormatName(unsigned int format);

    /**
     * @brief Register a custom clipboard format
     * @param formatName The name of the custom format
     * @return The registered format identifier
     */
    static unsigned int registerFormat(std::string_view formatName);

    /**
     * @class Impl
     * @brief Platform-specific implementation details (PIMPL pattern)
     */
     class Impl;

private:
    /**
     * @brief Private constructor (singleton pattern)
     */
    Clipboard();

    // Disable copy constructor and assignment
    Clipboard(const Clipboard&) = delete;
    Clipboard& operator=(const Clipboard&) = delete;
    
    std::unique_ptr<Impl> pImpl;
};

}  // namespace clip