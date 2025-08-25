#include "atom/extra/iconv/iconv_cpp.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

using namespace iconv_cpp;

// Helper function to create test files with different encodings
void create_test_file(const std::string& filename, const std::string& content,
                      const std::string& encoding = "UTF-8") {
    std::ofstream file(filename, std::ios::binary);
    if (file.is_open()) {
        if (encoding == "UTF-8") {
            file << content;
        } else if (encoding == "UTF-16LE") {
            // Simple UTF-16LE conversion for demo (in real code, use proper
            // conversion)
            file.write("\xFF\xFE", 2);  // BOM for UTF-16LE
            for (char c : content) {
                file.write(&c, 1);
                file.write("\0", 1);
            }
        } else {
            file << content;  // Fallback
        }
        file.close();
        std::cout << "Created test file: " << filename << " (" << encoding
                  << ")" << std::endl;
    }
}

// Helper function to display bytes in hex
void display_bytes(const std::vector<char>& data, const std::string& label) {
    std::cout << label << " (hex): ";
    for (size_t i = 0; i < std::min(data.size(), size_t(32)); ++i) {
        printf("%02X ", static_cast<unsigned char>(data[i]));
    }
    if (data.size() > 32) {
        std::cout << "... (" << data.size() << " bytes total)";
    }
    std::cout << std::endl;
}

int main() {
    try {
        std::cout << "=== iconv Character Encoding Conversion Example ==="
                  << std::endl;

        // 1. Basic string conversion
        std::cout << "\n1. Basic String Conversion:" << std::endl;
        {
            std::string utf8_text = "Hello, 世界! Здравствуй мир! ¡Hola mundo!";
            std::cout << "Original UTF-8 text: " << utf8_text << std::endl;

            try {
                // Convert UTF-8 to ISO-8859-1 (Latin-1)
                Converter utf8_to_latin1("UTF-8", "ISO-8859-1");
                auto latin1_result = utf8_to_latin1.convert_string(utf8_text);
                std::cout << "Converted to ISO-8859-1: " << latin1_result
                          << std::endl;
            } catch (const IconvError& e) {
                std::cout << "Expected error (non-Latin characters): "
                          << e.what() << std::endl;
            }

            // Convert UTF-8 to ASCII with transliteration
            try {
                ConversionOptions options;
                options.translit = true;
                Converter utf8_to_ascii("UTF-8", "ASCII", options);
                auto ascii_result = utf8_to_ascii.convert_string(utf8_text);
                std::cout << "Converted to ASCII (translit): " << ascii_result
                          << std::endl;
            } catch (const IconvError& e) {
                std::cout << "ASCII conversion error: " << e.what()
                          << std::endl;
            }
        }

        // 2. Different encoding conversions
        std::cout << "\n2. Different Encoding Conversions:" << std::endl;
        {
            std::string test_text = "Testing: àáâãäå çñü ßæø";
            std::cout << "Test text: " << test_text << std::endl;

            std::vector<std::pair<std::string, std::string>> conversions = {
                {"UTF-8", "UTF-16LE"},
                {"UTF-8", "UTF-32BE"},
                {"UTF-8", "ISO-8859-1"},
                {"UTF-8", "Windows-1252"},
                {"UTF-8", "KOI8-R"}};

            for (const auto& [from, to] : conversions) {
                try {
                    Converter converter(from, to);
                    auto result = converter.convert_string(test_text);
                    std::cout << from << " -> " << to << ": ";

                    if (to.find("UTF-16") != std::string::npos ||
                        to.find("UTF-32") != std::string::npos) {
                        std::cout << "(" << result.size() << " bytes)";
                        display_bytes(
                            std::vector<char>(result.begin(), result.end()),
                            "");
                    } else {
                        std::cout << result << std::endl;
                    }
                } catch (const IconvError& e) {
                    std::cout << from << " -> " << to << ": Error - "
                              << e.what() << std::endl;
                }
            }
        }

        // 3. Error handling policies
        std::cout << "\n3. Error Handling Policies:" << std::endl;
        {
            std::string problematic_text =
                "Valid text with invalid byte: \xFF\xFE";
            std::cout << "Problematic text with invalid UTF-8 sequence"
                      << std::endl;

            std::vector<ErrorHandlingPolicy> policies = {
                ErrorHandlingPolicy::Strict, ErrorHandlingPolicy::Skip,
                ErrorHandlingPolicy::Ignore};

            for (auto policy : policies) {
                try {
                    ConversionOptions options;
                    options.error_policy = policy;

                    Converter converter("UTF-8", "ASCII", options);
                    auto result = converter.convert_string(problematic_text);

                    std::string policy_name;
                    switch (policy) {
                        case ErrorHandlingPolicy::Strict:
                            policy_name = "Strict";
                            break;
                        case ErrorHandlingPolicy::Skip:
                            policy_name = "Skip";
                            break;
                        case ErrorHandlingPolicy::Ignore:
                            policy_name = "Ignore";
                            break;
                        case ErrorHandlingPolicy::Replace:
                            policy_name = "Replace";
                            break;
                    }

                    std::cout << policy_name << " policy result: " << result
                              << std::endl;
                } catch (const IconvError& e) {
                    std::cout << "Error with policy: " << e.what() << std::endl;
                }
            }
        }

        // 4. File conversion
        std::cout << "\n4. File Conversion:" << std::endl;
        {
            // Create test files
            create_test_file(
                "utf8_test.txt",
                "Hello, 世界!\nThis is a test file.\nWith multiple lines.",
                "UTF-8");

            try {
                // Convert file from UTF-8 to UTF-16LE
                bool success = convert_file("UTF-8", "UTF-16LE",
                                            "utf8_test.txt", "utf16_test.txt");

                if (success) {
                    std::cout << "Successfully converted utf8_test.txt to "
                                 "utf16_test.txt"
                              << std::endl;

                    // Check file sizes
                    auto utf8_size =
                        std::filesystem::file_size("utf8_test.txt");
                    auto utf16_size =
                        std::filesystem::file_size("utf16_test.txt");
                    std::cout << "UTF-8 file size: " << utf8_size << " bytes"
                              << std::endl;
                    std::cout << "UTF-16LE file size: " << utf16_size
                              << " bytes" << std::endl;
                } else {
                    std::cout << "File conversion failed" << std::endl;
                }
            } catch (const IconvError& e) {
                std::cout << "File conversion error: " << e.what() << std::endl;
            }
        }

        // 5. Batch conversion
        std::cout << "\n5. Batch Conversion:" << std::endl;
        {
            std::vector<std::string> test_strings = {
                "English text", "Français: àáâãäå", "Deutsch: äöüß",
                "Español: ñáéíóú", "Русский: привет мир"};

            try {
                auto results =
                    convert_batch("UTF-8", "ISO-8859-1", test_strings);

                std::cout << "Batch conversion results (UTF-8 to ISO-8859-1):"
                          << std::endl;
                for (size_t i = 0; i < test_strings.size(); ++i) {
                    std::cout << "  Input:  " << test_strings[i] << std::endl;
                    if (i < results.size()) {
                        std::cout << "  Output: " << results[i] << std::endl;
                    } else {
                        std::cout << "  Output: (conversion failed)"
                                  << std::endl;
                    }
                    std::cout << std::endl;
                }
            } catch (const IconvError& e) {
                std::cout << "Batch conversion error: " << e.what()
                          << std::endl;
            }
        }

        // 6. Encoding detection (if available)
        std::cout << "\n6. Encoding Detection:" << std::endl;
        {
            std::vector<std::string> test_samples = {
                "Plain ASCII text",
                "UTF-8 with special chars: àáâã",
                "Windows-1252 specific: "
                "''",
            };

            for (const auto& sample : test_samples) {
                try {
                    auto detection_result = detect_encoding(sample);
                    std::cout << "Sample: " << sample << std::endl;
                    std::cout
                        << "  Detected encoding: " << detection_result.encoding
                        << std::endl;
                    std::cout << "  Confidence: " << detection_result.confidence
                              << std::endl;
                } catch (const IconvError& e) {
                    std::cout << "Detection error for sample: " << e.what()
                              << std::endl;
                }
            }
        }

        // 7. Conversion with progress callback
        std::cout << "\n7. Conversion with Progress Callback:" << std::endl;
        {
            // Create a larger test string
            std::string large_text;
            for (int i = 0; i < 1000; ++i) {
                large_text += "Line " + std::to_string(i) + ": Hello, 世界! ";
            }

            std::cout << "Converting large text (" << large_text.size()
                      << " bytes)..." << std::endl;

            try {
                ProgressCallback progress = [](size_t processed, size_t total) {
                    if (total > 0) {
                        int percent =
                            static_cast<int>((processed * 100) / total);
                        if (percent % 20 == 0) {  // Show progress every 20%
                            std::cout << "  Progress: " << percent << "% ("
                                      << processed << "/" << total << " bytes)"
                                      << std::endl;
                        }
                    }
                };

                bool success =
                    convert_file_with_progress("UTF-8", "UTF-16LE", large_text,
                                               "large_utf16.txt", progress);

                if (success) {
                    auto file_size =
                        std::filesystem::file_size("large_utf16.txt");
                    std::cout
                        << "Large file conversion completed: " << file_size
                        << " bytes" << std::endl;
                }
            } catch (const IconvError& e) {
                std::cout << "Large file conversion error: " << e.what()
                          << std::endl;
            }
        }

        // 8. Converter reuse and thread safety
        std::cout << "\n8. Converter Reuse:" << std::endl;
        {
            try {
                Converter converter("UTF-8", "ASCII");

                std::vector<std::string> inputs = {"First conversion",
                                                   "Second conversion",
                                                   "Third conversion"};

                for (const auto& input : inputs) {
                    converter.reset();  // Reset converter state
                    auto result = converter.convert_string(input);
                    std::cout << "Converted: " << input << " -> " << result
                              << std::endl;
                }

                std::cout << "Converter info:" << std::endl;
                std::cout << "  From: " << converter.from_encoding()
                          << std::endl;
                std::cout << "  To: " << converter.to_encoding() << std::endl;
            } catch (const IconvError& e) {
                std::cout << "Converter reuse error: " << e.what() << std::endl;
            }
        }

        // 9. Memory-efficient streaming conversion
        std::cout << "\n9. Streaming Conversion:" << std::endl;
        {
            std::string stream_data =
                "This is streaming data that will be converted in chunks. ";
            // Repeat to make it larger
            for (int i = 0; i < 10; ++i) {
                stream_data += stream_data;
            }

            std::cout << "Streaming conversion of " << stream_data.size()
                      << " bytes..." << std::endl;

            try {
                ConversionState state;
                Converter converter("UTF-8", "UTF-16LE");

                const size_t chunk_size = 1024;
                std::vector<char> output_buffer;

                for (size_t pos = 0; pos < stream_data.size();
                     pos += chunk_size) {
                    size_t current_chunk_size =
                        std::min(chunk_size, stream_data.size() - pos);
                    std::span<const char> chunk(stream_data.data() + pos,
                                                current_chunk_size);

                    auto chunk_result = converter.convert(chunk);
                    output_buffer.insert(output_buffer.end(),
                                         chunk_result.begin(),
                                         chunk_result.end());

                    state.processed_input_bytes += current_chunk_size;
                    state.processed_output_bytes += chunk_result.size();
                }

                std::cout << "Streaming conversion completed:" << std::endl;
                std::cout << "  Input bytes: " << state.processed_input_bytes
                          << std::endl;
                std::cout << "  Output bytes: " << state.processed_output_bytes
                          << std::endl;
                std::cout << "  Compression ratio: "
                          << (static_cast<double>(
                                  state.processed_output_bytes) /
                              state.processed_input_bytes)
                          << std::endl;
            } catch (const IconvError& e) {
                std::cout << "Streaming conversion error: " << e.what()
                          << std::endl;
            }
        }

        // Cleanup test files
        std::cout << "\nCleaning up test files..." << std::endl;
        std::filesystem::remove("utf8_test.txt");
        std::filesystem::remove("utf16_test.txt");
        std::filesystem::remove("large_utf16.txt");

        std::cout
            << "\n=== iconv Character Encoding Conversion Example Completed ==="
            << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "Unexpected error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
