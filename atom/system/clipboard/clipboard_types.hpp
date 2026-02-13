/**
 * @file clipboard_types.hpp
 * @brief Core type definitions for clipboard operations.
 *
 * Contains ClipboardFormat strong type, predefined format constants,
 * ClipboardImageType concept, and callback type aliases.
 */

#ifndef ATOM_SYSTEM_CLIPBOARD_TYPES_HPP
#define ATOM_SYSTEM_CLIPBOARD_TYPES_HPP

#include <concepts>
#include <functional>

#ifdef CLIPBOARD_SUPPORT_OPENCV
#include <opencv2/opencv.hpp>
#endif

#ifdef CLIPBOARD_SUPPORT_CIMG
#include <CImg.h>
#endif

namespace clip {

/**
 * @brief Strong type for clipboard format identifiers
 */
struct ClipboardFormat {
    unsigned int value;

    constexpr explicit ClipboardFormat(unsigned int v) noexcept : value(v) {}
    constexpr operator unsigned int() const noexcept { return value; }

    constexpr bool operator==(const ClipboardFormat& other) const noexcept =
        default;
    constexpr auto operator<=>(const ClipboardFormat& other) const noexcept =
        default;
};

/**
 * @brief Predefined clipboard formats
 */
namespace formats {
constexpr ClipboardFormat TEXT{1};
constexpr ClipboardFormat HTML{2};
constexpr ClipboardFormat IMAGE_TIFF{3};
constexpr ClipboardFormat IMAGE_PNG{4};
constexpr ClipboardFormat RTF{5};
}  // namespace formats

/**
 * @brief Concept for checking if an image type has required properties for
 * clipboard operations
 */
template <typename ImageT>
concept ClipboardImageType = requires(ImageT img) {
    { img.cols } -> std::convertible_to<int>;
    { img.rows } -> std::convertible_to<int>;
    { img.channels() } -> std::convertible_to<int>;
    { img.data } -> std::convertible_to<void*>;
};

/**
 * @brief Callback function type for clipboard change notifications
 */
using ClipboardChangeCallback = std::function<void()>;

}  // namespace clip

#endif  // ATOM_SYSTEM_CLIPBOARD_TYPES_HPP
