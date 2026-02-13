/**
 * @file clipboard_example.cpp
 * @brief Example demonstrating clipboard operations
 *
 * This example shows how to use the Clipboard class for:
 * - Text operations (set/get)
 * - Binary data operations
 * - Clipboard monitoring
 * - Error handling with safe operations
 */

#include "atom/system/clipboard.hpp"

#include <chrono>
#include <iostream>
#include <thread>

using namespace clip;

void demonstrateTextOperations() {
    std::cout << "\n=== Text Operations ===" << std::endl;

    auto& clipboard = Clipboard::instance();

    // Set text to clipboard
    try {
        clipboard.setText("Hello from Atom Clipboard!");
        std::cout << "✓ Text set to clipboard" << std::endl;
    } catch (const ClipboardException& e) {
        std::cerr << "✗ Failed to set text: " << e.what() << std::endl;
        return;
    }

    // Get text from clipboard
    try {
        std::string text = clipboard.getText();
        std::cout << "✓ Retrieved text: " << text << std::endl;
    } catch (const ClipboardException& e) {
        std::cerr << "✗ Failed to get text: " << e.what() << std::endl;
    }

    // Check if clipboard has text
    if (clipboard.hasText()) {
        std::cout << "✓ Clipboard contains text" << std::endl;
    }
}

void demonstrateSafeOperations() {
    std::cout << "\n=== Safe Operations (No Exceptions) ===" << std::endl;

    auto& clipboard = Clipboard::instance();

    // Safe text set operation
    auto setResult = clipboard.setTextSafe("Safe text operation");
    if (setResult) {
        std::cout << "✓ Safe text set succeeded" << std::endl;
    } else {
        std::cerr << "✗ Safe text set failed: " << setResult.error().message()
                  << std::endl;
    }

    // Safe text get operation
    auto getResult = clipboard.getTextSafe();
    if (getResult) {
        std::cout << "✓ Safe text get succeeded: " << getResult.value()
                  << std::endl;
    } else {
        std::cerr << "✗ Safe text get failed: " << getResult.error().message()
                  << std::endl;
    }
}

void demonstrateBinaryData() {
    std::cout << "\n=== Binary Data Operations ===" << std::endl;

    auto& clipboard = Clipboard::instance();

    // Create some binary data
    std::vector<std::byte> data = {std::byte{0x48}, std::byte{0x65},
                                   std::byte{0x6C}, std::byte{0x6C},
                                   std::byte{0x6F}};

    try {
        clipboard.setData(formats::TEXT, data);
        std::cout << "✓ Binary data set to clipboard" << std::endl;

        auto retrievedData = clipboard.getData(formats::TEXT);
        std::cout << "✓ Retrieved " << retrievedData.size() << " bytes"
                  << std::endl;
    } catch (const ClipboardException& e) {
        std::cerr << "✗ Binary data operation failed: " << e.what()
                  << std::endl;
    }
}

void demonstrateClipboardMonitoring() {
    std::cout << "\n=== Clipboard Monitoring ===" << std::endl;

    auto& clipboard = Clipboard::instance();

    // Set up clipboard change callback
    clipboard.setChangeCallback(
        []() { std::cout << "📋 Clipboard content changed!" << std::endl; });

    std::cout << "Monitoring clipboard for 5 seconds..." << std::endl;
    std::cout << "Try copying something to the clipboard!" << std::endl;

    // Monitor for 5 seconds
    std::this_thread::sleep_for(std::chrono::seconds(5));

    // Clear callback
    clipboard.clearChangeCallback();
    std::cout << "✓ Monitoring stopped" << std::endl;
}

void demonstrateClearOperation() {
    std::cout << "\n=== Clear Operation ===" << std::endl;

    auto& clipboard = Clipboard::instance();

    try {
        clipboard.clear();
        std::cout << "✓ Clipboard cleared" << std::endl;

        if (!clipboard.hasText()) {
            std::cout << "✓ Verified clipboard is empty" << std::endl;
        }
    } catch (const ClipboardException& e) {
        std::cerr << "✗ Failed to clear clipboard: " << e.what() << std::endl;
    }
}

int main() {
    std::cout << "Clipboard Operations Example" << std::endl;
    std::cout << "=============================" << std::endl;

    try {
        demonstrateTextOperations();
        demonstrateSafeOperations();
        demonstrateBinaryData();
        demonstrateClipboardMonitoring();
        demonstrateClearOperation();

        std::cout << "\n✓ All clipboard operations completed successfully!"
                  << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "\n✗ Unexpected error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
