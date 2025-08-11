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
    Black = 30,
    Red = 31,
    Green = 32,
    Yellow = 33,
    Blue = 34,
    Magenta = 35,
    Cyan = 36,
    White = 37,
    BrightBlack = 90,
    BrightRed = 91,
    BrightGreen = 92,
    BrightYellow = 93,
    BrightBlue = 94,
    BrightMagenta = 95,
    BrightCyan = 96,
    BrightWhite = 97
};

/**
 * @brief Text style code enumeration
 * @details ANSI text formatting attributes for terminal text styling
 */
enum class TextStyle {
    Normal = 0,
    Bold = 1,
    Dim = 2,
    Italic = 3,
    Underline = 4,
    Blinking = 5,
    Reverse = 7,
    Hidden = 8,
    Strikethrough = 9
};

/**
 * @brief Color printing utility class with modern formatting support
 * @details Provides functionality for colored text output in terminals with
 * std::format compatibility and integration with atom::utils::print facilities
 */
class ColorPrinter {
private:
    static constexpr const char* RESET_CODE = "\033[0m";

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
                  << static_cast<int>(color) << "m" << text << RESET_CODE;
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
        std::cout << '\n';
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
        std::cout << "\033[" << static_cast<int>(style) << ";"
                  << static_cast<int>(color) << "m"
                  << std::vformat(fmt, std::make_format_args(args...))
                  << RESET_CODE;
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
        printColored(color, style, fmt, args...);
        std::cout << '\n';
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
        printColoredLine(ColorCode::Red, TextStyle::Bold, fmt, args...);
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
        printColoredLine(ColorCode::Yellow, TextStyle::Normal, fmt, args...);
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
        printColoredLine(ColorCode::Green, TextStyle::Normal, fmt, args...);
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
        printColoredLine(ColorCode::Cyan, TextStyle::Normal, fmt, args...);
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
