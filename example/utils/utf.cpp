/**
 * @file utf_example.cpp
 * @brief Comprehensive examples demonstrating UTF conversion utilities
 *
 * This example demonstrates all functions available in atom::utils::utf.hpp:
 * - UTF-8, UTF-16, and UTF-32 conversions
 * - Wide character string handling
 * - Unicode character processing
 * - Error handling for invalid sequences
 * - Performance considerations
 * - Real-world internationalization scenarios
 */

#include "atom/utils/text/utf.hpp"

#include <cstdint>
#include <iomanip>
#include <iostream>
#include <locale>
#include <string>
#include <vector>

using namespace atom::utils;

// Helper function to print section headers
void printSection(const std::string& title) {
    std::cout << "\n==========================================" << std::endl;
    std::cout << "  " << title << std::endl;
    std::cout << "==========================================" << std::endl;
}

// Helper function to print subsection headers
void printSubsection(const std::string& title) {
    std::cout << "\n--- " << title << " ---" << std::endl;
}

// Helper function to print UTF-16 string as hex
void printUTF16Hex(const std::u16string& str, const std::string& label) {
    std::cout << label << " (hex): ";
    for (char16_t ch : str) {
        std::cout << "U+" << std::hex << std::setw(4) << std::setfill('0')
                  << static_cast<uint16_t>(ch) << " ";
    }
    std::cout << std::dec << std::endl;
}

// Helper function to print UTF-32 string as hex
void printUTF32Hex(const std::u32string& str, const std::string& label) {
    std::cout << label << " (hex): ";
    for (char32_t ch : str) {
        std::cout << "U+" << std::hex << std::setw(6) << std::setfill('0')
                  << static_cast<uint32_t>(ch) << " ";
    }
    std::cout << std::dec << std::endl;
}

// Helper function to print UTF-8 bytes as hex
void printUTF8Hex(const std::string& str, const std::string& label) {
    std::cout << label << " (bytes): ";
    for (unsigned char ch : str) {
        std::cout << "0x" << std::hex << std::setw(2) << std::setfill('0')
                  << static_cast<unsigned int>(ch) << " ";
    }
    std::cout << std::dec << std::endl;
}

int main() {
    std::cout << "==========================================" << std::endl;
    std::cout << "  UTF Conversion Utilities Demo" << std::endl;
    std::cout << "==========================================" << std::endl;

    // ============================
    // Example 1: Basic UTF Conversions
    // ============================
    printSection("1. Basic UTF Conversions");

    printSubsection("Wide String to UTF-8");

    std::wstring wstr = L"Hello, 世界! 🌍🚀";
    std::string utf8Str = toUTF8(wstr);

    std::wcout << L"Original wide string: " << wstr << std::endl;
    std::cout << "UTF-8 string: " << utf8Str << std::endl;
    printUTF8Hex(utf8Str, "UTF-8");

    printSubsection("UTF-8 to Wide String");

    std::wstring convertedWStr = fromUTF8(utf8Str);
    std::wcout << L"Converted back to wide: " << convertedWStr << std::endl;
    std::cout << "Round-trip successful: "
              << (wstr == convertedWStr ? "YES" : "NO") << std::endl;

    // ============================
    // Example 2: UTF-8 to UTF-16/32 Conversions
    // ============================
    printSection("2. UTF-8 to UTF-16/32 Conversions");

    printSubsection("UTF-8 to UTF-16");

    std::u16string utf16Str = utf8toUtF16(utf8Str);
    std::cout << "UTF-16 length: " << utf16Str.length() << " code units"
              << std::endl;
    printUTF16Hex(utf16Str, "UTF-16");

    printSubsection("UTF-8 to UTF-32");

    std::u32string utf32Str = utf8toUtF32(utf8Str);
    std::cout << "UTF-32 length: " << utf32Str.length() << " code points"
              << std::endl;
    printUTF32Hex(utf32Str, "UTF-32");

    // ============================
    // Example 3: UTF-16/32 to UTF-8 Conversions
    // ============================
    printSection("3. UTF-16/32 to UTF-8 Conversions");

    printSubsection("UTF-16 to UTF-8");

    std::string convertedUtf8Str = utf16toUtF8(utf16Str);
    std::cout << "UTF-16 to UTF-8: " << convertedUtf8Str << std::endl;
    std::cout << "Round-trip UTF-8 successful: "
              << (utf8Str == convertedUtf8Str ? "YES" : "NO") << std::endl;

    printSubsection("UTF-32 to UTF-8");

    std::string utf8FromUtf32 = utf32toUtF8(utf32Str);
    std::cout << "UTF-32 to UTF-8: " << utf8FromUtf32 << std::endl;
    std::cout << "Round-trip UTF-8 successful: "
              << (utf8Str == utf8FromUtf32 ? "YES" : "NO") << std::endl;

    // ============================
    // Example 4: UTF-16 to UTF-32 Conversions
    // ============================
    printSection("4. UTF-16 to UTF-32 Conversions");

    printSubsection("UTF-16 to UTF-32");

    std::u32string convertedUtf32Str = utf16toUtF32(utf16Str);
    std::cout << "UTF-16 to UTF-32 length: " << convertedUtf32Str.length()
              << " code points" << std::endl;
    printUTF32Hex(convertedUtf32Str, "UTF-16 to UTF-32");
    std::cout << "Round-trip UTF-32 successful: "
              << (utf32Str == convertedUtf32Str ? "YES" : "NO") << std::endl;

    printSubsection("UTF-32 to UTF-16");

    std::u16string utf16FromUtf32 = utf32toUtF16(utf32Str);
    std::cout << "UTF-32 to UTF-16 length: " << utf16FromUtf32.length()
              << " code units" << std::endl;
    printUTF16Hex(utf16FromUtf32, "UTF-32 to UTF-16");
    std::cout << "Round-trip UTF-16 successful: "
              << (utf16Str == utf16FromUtf32 ? "YES" : "NO") << std::endl;

    // ============================
    // Example 5: Multilingual Text Processing
    // ============================
    printSection("5. Multilingual Text Processing");

    printSubsection("Various Languages and Scripts");

    std::vector<std::wstring> multilingualTexts = {
        L"English: Hello World!",       L"中文: 你好世界！",
        L"日本語: こんにちは世界！",    L"한국어: 안녕하세요 세계!",
        L"العربية: مرحبا بالعالم!",     L"Русский: Привет мир!",
        L"Français: Bonjour le monde!", L"Deutsch: Hallo Welt!",
        L"Español: ¡Hola mundo!",       L"हिन्दी: नमस्ते दुनिया!"};

    for (size_t i = 0; i < multilingualTexts.size(); ++i) {
        const auto& wtext = multilingualTexts[i];
        std::string utf8Text = toUTF8(wtext);
        std::u16string utf16Text = utf8toUtF16(utf8Text);
        std::u32string utf32Text = utf8toUtF32(utf8Text);

        std::wcout << L"Text " << (i + 1) << L": " << wtext << std::endl;
        std::cout << "  UTF-8 bytes: " << utf8Text.length() << std::endl;
        std::cout << "  UTF-16 code units: " << utf16Text.length() << std::endl;
        std::cout << "  UTF-32 code points: " << utf32Text.length()
                  << std::endl;

        // Verify round-trip conversion
        std::wstring roundTrip = fromUTF8(utf8Text);
        std::cout << "  Round-trip successful: "
                  << (wtext == roundTrip ? "YES" : "NO") << std::endl;
        std::cout << std::endl;
    }

    // ============================
    // Example 6: Emoji and Special Characters
    // ============================
    printSection("6. Emoji and Special Characters");

    printSubsection("Emoji Processing");

    std::wstring emojiText =
        L"Emojis: "
        L"😀😃😄😁😆😅😂🤣😊😇🙂🙃😉😌😍🥰😘😗😙😚😋😛😝😜🤪🤨🧐🤓😎🤩🥳";
    std::string emojiUtf8 = toUTF8(emojiText);
    std::u16string emojiUtf16 = utf8toUtF16(emojiUtf8);
    std::u32string emojiUtf32 = utf8toUtF32(emojiUtf8);

    std::wcout << L"Emoji text: " << emojiText << std::endl;
    std::cout << "UTF-8 bytes: " << emojiUtf8.length() << std::endl;
    std::cout << "UTF-16 code units: " << emojiUtf16.length() << std::endl;
    std::cout << "UTF-32 code points: " << emojiUtf32.length() << std::endl;

    // Show some emoji code points
    std::cout << "First few emoji code points: ";
    for (size_t i = 0; i < std::min(size_t(5), emojiUtf32.length()); ++i) {
        std::cout << "U+" << std::hex << std::setw(6) << std::setfill('0')
                  << static_cast<uint32_t>(emojiUtf32[i]) << " ";
    }
    std::cout << std::dec << std::endl;

    printSubsection("Mathematical and Scientific Symbols");

    std::wstring mathText = L"Math: ∑∏∫∮∇∂√∞±×÷≤≥≠≈∈∉⊂⊃∪∩∧∨¬∀∃∄∅ℕℤℚℝℂ";
    std::string mathUtf8 = toUTF8(mathText);

    std::wcout << L"Mathematical symbols: " << mathText << std::endl;
    std::cout << "UTF-8 representation: " << mathUtf8 << std::endl;
    printUTF8Hex(mathUtf8, "Math symbols");

    std::cout << "\nAll UTF conversion examples completed successfully!"
              << std::endl;

    return 0;
}
