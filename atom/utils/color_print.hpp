#ifndef ATOM_UTILS_COLOR_PRINT_HPP
#define ATOM_UTILS_COLOR_PRINT_HPP

#include <format>
#include <iostream>
#include <string_view>


namespace atom::utils {

/**
 * @brief Console color code enumeration
 * @details ANSI color escape sequences for terminal text coloring
 */
enum class ColorCode {
    Black = 30,          ///< Black color
    Red = 31,            ///< Red color
    Green = 32,          ///< Green color
    Yellow = 33,         ///< Yellow color
    Blue = 34,           ///< Blue color
    Magenta = 35,        ///< Magenta color
    Cyan = 36,           ///< Cyan color
    White = 37,          ///< White color
    BrightBlack = 90,    ///< Bright black color (gray)
    BrightRed = 91,      ///< Bright red color
    BrightGreen = 92,    ///< Bright green color
    BrightYellow = 93,   ///< Bright yellow color
    BrightBlue = 94,     ///< Bright blue color
    BrightMagenta = 95,  ///< Bright magenta color
    BrightCyan = 96,     ///< Bright cyan color
    BrightWhite = 97     ///< Bright white color
};

/**
 * @brief Text style code enumeration
 * @details ANSI text formatting attributes for terminal text styling
 */
enum class TextStyle {
    Normal = 0,        ///< Normal text style
    Bold = 1,          ///< Bold text style
    Dim = 2,           ///< Dimmed text style
    Italic = 3,        ///< Italic text style
    Underline = 4,     ///< Underlined text style
    Blinking = 5,      ///< Blinking text style
    Reverse = 7,       ///< Reversed colors
    Hidden = 8,        ///< Hidden text
    Strikethrough = 9  ///< Strikethrough text
};

/**
 * @brief Color printing utility class with modern formatting support
 * @details Provides functionality for colored text output in terminals with
 * std::format compatibility and integration with atom::utils::print facilities
 */
class ColorPrinter {
public:
    /**
     * @brief Print text with specified color
     * @param text The text to print
     * @param color The text color
     * @param style The text style, defaults to normal style
     */
    static void printColored(std::string_view text, ColorCode color,
                             TextStyle style = TextStyle::Normal) {
        std::cout << "\033[" << static_cast<int>(style) << ";"
                  << static_cast<int>(color) << "m" << text << "\033[0m";
    }

    /**
     * @brief Print text with specified color, followed by a newline
     * @param text The text to print
     * @param color The text color
     * @param style The text style, defaults to normal style
     */
    static void printColoredLine(std::string_view text, ColorCode color,
                                 TextStyle style = TextStyle::Normal) {
        printColored(text, color, style);
        std::cout << std::endl;
    }

    /**
     * @brief Print formatted text with specified color using std::format
     * @param color The text color
     * @param style The text style, defaults to normal style
     * @param fmt Format string
     * @param args Format arguments
     */
    template <typename... Args>
    static void printColored(ColorCode color, TextStyle style,
                             std::string_view fmt, Args&&... args) {
        try {
            std::cout << "\033[" << static_cast<int>(style) << ";"
                      << static_cast<int>(color) << "m"
                      << std::vformat(fmt, std::make_format_args(
                                               std::forward<Args>(args)...))
                      << "\033[0m";
        } catch (const std::format_error& e) {
            std::cout << "\033[" << static_cast<int>(TextStyle::Bold) << ";"
                      << static_cast<int>(ColorCode::Red) << "m"
                      << "Format error: " << e.what() << "\033[0m";
        }
    }

    /**
     * @brief Print formatted text with specified color using std::format,
     * followed by a newline
     * @param color The text color
     * @param style The text style, defaults to normal style
     * @param fmt Format string
     * @param args Format arguments
     */
    template <typename... Args>
    static void printColoredLine(ColorCode color, TextStyle style,
                                 std::string_view fmt, Args&&... args) {
        printColored(color, style, fmt, std::forward<Args>(args)...);
        std::cout << std::endl;
    }

    /**
     * @brief Print error message in red color
     * @param text The error message text
     */
    static void error(std::string_view text) {
        printColoredLine(text, ColorCode::Red, TextStyle::Bold);
    }

    /**
     * @brief Print formatted error message in red color
     * @param fmt Format string
     * @param args Format arguments
     */
    template <typename... Args>
    static void error(std::string_view fmt, Args&&... args) {
        printColoredLine(ColorCode::Red, TextStyle::Bold, fmt,
                         std::forward<Args>(args)...);
    }

    /**
     * @brief Print warning message in yellow color
     * @param text The warning message text
     */
    static void warning(std::string_view text) {
        printColoredLine(text, ColorCode::Yellow);
    }

    /**
     * @brief Print formatted warning message in yellow color
     * @param fmt Format string
     * @param args Format arguments
     */
    template <typename... Args>
    static void warning(std::string_view fmt, Args&&... args) {
        printColoredLine(ColorCode::Yellow, TextStyle::Normal, fmt,
                         std::forward<Args>(args)...);
    }

    /**
     * @brief Print success message in green color
     * @param text The success message text
     */
    static void success(std::string_view text) {
        printColoredLine(text, ColorCode::Green);
    }

    /**
     * @brief Print formatted success message in green color
     * @param fmt Format string
     * @param args Format arguments
     */
    template <typename... Args>
    static void success(std::string_view fmt, Args&&... args) {
        printColoredLine(ColorCode::Green, TextStyle::Normal, fmt,
                         std::forward<Args>(args)...);
    }

    /**
     * @brief Print information message in cyan color
     * @param text The information message text
     */
    static void info(std::string_view text) {
        printColoredLine(text, ColorCode::Cyan);
    }

    /**
     * @brief Print formatted information message in cyan color
     * @param fmt Format string
     * @param args Format arguments
     */
    template <typename... Args>
    static void info(std::string_view fmt, Args&&... args) {
        printColoredLine(ColorCode::Cyan, TextStyle::Normal, fmt,
                         std::forward<Args>(args)...);
    }
};

}  // namespace atom::utils

/**
 * @brief Convenient aliases for testing utilities
 */
namespace atom::test {
using ColorCode = atom::utils::ColorCode;
using TextStyle = atom::utils::TextStyle;
using ColorPrinter = atom::utils::ColorPrinter;
}  // namespace atom::test

#endif  // ATOM_UTILS_COLOR_PRINT_HPP