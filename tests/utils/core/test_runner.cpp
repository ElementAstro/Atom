// Simple compilation test - verify headers compile without GTest
#include <iostream>

// Test basic header includes to verify they compile
// We'll include the actual implementation headers, not the test headers
#include "atom/utils/container/span.hpp"
#include "atom/utils/conversion/convert.hpp"
#include "atom/utils/debug/color_print.hpp"
#include "atom/utils/memory/simd_wrapper.hpp"
#include "atom/utils/process/qprocess.hpp"
#include "atom/utils/text/utf.hpp"

int main() {
    std::cout << "=== ATOM UTILS COMPILATION CHECK ===" << std::endl;
    std::cout << "Testing compilation of utils implementation headers..."
              << std::endl;

    std::cout << "✓ UTF conversion utilities: Headers compiled successfully"
              << std::endl;
    std::cout << "✓ SIMD wrapper utilities: Headers compiled successfully"
              << std::endl;
    std::cout << "✓ Windows conversion utilities: Headers compiled successfully"
              << std::endl;
    std::cout << "✓ Color printing utilities: Headers compiled successfully"
              << std::endl;
    std::cout << "✓ Span utilities: Headers compiled successfully" << std::endl;
    std::cout << "✓ QProcess utilities: Headers compiled successfully"
              << std::endl;

    std::cout << std::endl;
    std::cout << "=== COMPILATION TEST SUCCESSFUL ===" << std::endl;
    std::cout << "All utils implementation headers compile without errors!"
              << std::endl;
    std::cout << "The implementation is ready for test integration."
              << std::endl;

    return 0;
}
