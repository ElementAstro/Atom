#include "atom/utils/utf.hpp"

#include <cstdint>
#include <iostream>

using namespace atom::utils;

int main() {
    // Convert a wide-character string to a UTF-8 encoded string
    std::wstring wstr = L"Hello, 世界";
    std::string utf8Str = toUTF8(wstr);
    std::cout << "UTF-8 string: " << utf8Str << std::endl;

    // Convert a UTF-8 encoded string to a wide-character string
    std::wstring convertedWStr = fromUTF8(utf8Str);
    std::wcout << L"Wide-character string: " << convertedWStr << std::endl;

    // Convert a UTF-8 encoded string to a UTF-16 encoded string
    std::u16string utf16Str = utf8toUtF16(utf8Str);
    std::cout << "UTF-16 string: ";
    for (char16_t ch : utf16Str) {
        std::cout << std::hex << static_cast<int>(ch) << " ";
    }
    std::cout << std::endl;

    // Convert a UTF-8 encoded string to a UTF-32 encoded string
    std::u32string utf32Str = utf8toUtF32(utf8Str);
    std::cout << "UTF-32 string: ";
    for (char32_t ch : utf32Str) {
        std::cout << std::hex << static_cast<uint32_t>(ch) << " ";
    }
    std::cout << std::endl;

    // Convert a UTF-16 encoded string to a UTF-8 encoded string
    std::string convertedUtf8Str = utf16toUtF8(utf16Str);
    std::cout << "Converted UTF-8 string: " << convertedUtf8Str << std::endl;

    // Convert a UTF-16 encoded string to a UTF-32 encoded string
    std::u32string convertedUtf32Str = utf16toUtF32(utf16Str);
    std::cout << "Converted UTF-32 string: ";
    for (char32_t ch : convertedUtf32Str) {
        std::cout << std::hex << static_cast<int>(ch) << " ";
    }
    std::cout << std::endl;

    // Convert a UTF-32 encoded string to a UTF-8 encoded string
    std::string utf8FromUtf32 = utf32toUtF8(utf32Str);
    std::cout << "UTF-8 from UTF-32 string: " << utf8FromUtf32 << std::endl;

    // Convert a UTF-32 encoded string to a UTF-16 encoded string
    std::u16string utf16FromUtf32 = utf32toUtF16(utf32Str);
    std::cout << "UTF-16 from UTF-32 string: ";
    for (char16_t ch : utf16FromUtf32) {
        std::cout << std::hex << static_cast<int>(ch) << " ";
    }
    std::cout << std::endl;

    // Validate if a UTF-8 encoded string is well-formed
    bool isValid = isValidUTF8(utf8Str);
    std::cout << "Is valid UTF-8: " << std::boolalpha << isValid << std::endl;

    return 0;
}