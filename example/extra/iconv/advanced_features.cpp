#include "atom/extra/iconv/iconv_cpp.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <future>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

using namespace iconv_cpp;
using namespace std::chrono_literals;

// Helper function to create test files with specific encodings
void create_encoded_file(const std::string& filename,
                         const std::string& content,
                         const std::string& encoding) {
    try {
        if (encoding == "UTF-8") {
            std::ofstream file(filename, std::ios::binary);
            file << content;
        } else {
            // Convert from UTF-8 to target encoding
            Converter converter("UTF-8", encoding);
            auto converted = converter.convert_string(content);

            std::ofstream file(filename, std::ios::binary);
            file.write(converted.data(), converted.size());
        }
        std::cout << "Created " << filename << " in " << encoding << " encoding"
                  << std::endl;
    } catch (const IconvError& e) {
        std::cerr << "Failed to create " << filename << ": " << e.what()
                  << std::endl;
    }
}

// Helper function to display encoding information
void display_encoding_info(const std::string& encoding) {
    try {
        auto& registry = EncodingRegistry::instance();
        auto info = registry.get_encoding_info(encoding);
        if (info) {
            std::cout << "Encoding: " << info->name << std::endl;
            std::cout << "  Description: " << info->description << std::endl;
            std::cout << "  ASCII compatible: "
                      << (info->is_ascii_compatible ? "Yes" : "No")
                      << std::endl;
            std::cout << "  Min char size: " << info->min_char_size << " bytes"
                      << std::endl;
            std::cout << "  Max char size: " << info->max_char_size << " bytes"
                      << std::endl;
            std::cout << "  Has BOM: " << (info->has_bom ? "Yes" : "No")
                      << std::endl;
        } else {
            std::cout << "No information available for encoding: " << encoding
                      << std::endl;
        }
    } catch (const std::exception& e) {
        std::cout << "Error getting encoding info: " << e.what() << std::endl;
    }
}

int main() {
    try {
        std::cout << "=== iconv Advanced Features Example ===" << std::endl;

        // 1. Encoding registry and information
        std::cout << "\n1. Encoding Registry and Information:" << std::endl;
        {
            std::vector<std::string> encodings = {
                "UTF-8",        "UTF-16LE", "UTF-32BE", "ISO-8859-1",
                "Windows-1252", "ASCII",    "KOI8-R",   "GB2312"};

            for (const auto& encoding : encodings) {
                display_encoding_info(encoding);
                std::cout << std::endl;
            }
        }

        // 2. BOM (Byte Order Mark) handling
        std::cout << "\n2. BOM (Byte Order Mark) Handling:" << std::endl;
        {
            std::string test_text = "Hello, World! 你好世界!";

            // Create files with and without BOM
            try {
                ConversionOptions with_bom;
                with_bom.ignore_bom = false;

                ConversionOptions without_bom;
                without_bom.ignore_bom = true;

                Converter utf8_to_utf16_bom("UTF-8", "UTF-16LE", with_bom);
                Converter utf8_to_utf16_no_bom("UTF-8", "UTF-16LE",
                                               without_bom);

                auto result_with_bom =
                    utf8_to_utf16_bom.convert_string(test_text);
                auto result_without_bom =
                    utf8_to_utf16_no_bom.convert_string(test_text);

                std::cout << "UTF-16LE with BOM: " << result_with_bom.size()
                          << " bytes" << std::endl;
                std::cout << "UTF-16LE without BOM: "
                          << result_without_bom.size() << " bytes" << std::endl;

                // Check for BOM presence
                if (result_with_bom.size() >= 2 &&
                    static_cast<unsigned char>(result_with_bom[0]) == 0xFF &&
                    static_cast<unsigned char>(result_with_bom[1]) == 0xFE) {
                    std::cout << "BOM detected in result" << std::endl;
                } else {
                    std::cout << "No BOM in result" << std::endl;
                }
            } catch (const IconvError& e) {
                std::cout << "BOM handling error: " << e.what() << std::endl;
            }
        }

        // 3. Transliteration and fallback mechanisms
        std::cout << "\n3. Transliteration and Fallback:" << std::endl;
        {
            std::string unicode_text =
                "Café, naïve, résumé, Москва, 北京, العربية";
            std::cout << "Original text: " << unicode_text << std::endl;

            // Try different conversion strategies
            std::vector<std::pair<std::string, ConversionOptions>> strategies =
                {{"Strict", ConversionOptions{}},
                 {"Transliteration",
                  []() {
                      ConversionOptions opts;
                      opts.translit = true;
                      return opts;
                  }()},
                 {"Ignore errors",
                  []() {
                      ConversionOptions opts;
                      opts.error_policy = ErrorHandlingPolicy::Ignore;
                      return opts;
                  }()},
                 {"Replace with ?", []() {
                      ConversionOptions opts;
                      opts.error_policy = ErrorHandlingPolicy::Replace;
                      opts.replacement_char = '?';
                      return opts;
                  }()}};

            for (const auto& [strategy_name, options] : strategies) {
                try {
                    Converter converter("UTF-8", "ASCII", options);
                    auto result = converter.convert_string(unicode_text);
                    std::cout << strategy_name << ": " << result << std::endl;
                } catch (const IconvError& e) {
                    std::cout << strategy_name << ": Error - " << e.what()
                              << std::endl;
                }
            }
        }

        // 4. Parallel conversion
        std::cout << "\n4. Parallel Conversion:" << std::endl;
        {
            std::vector<std::string> texts = {
                "Text 1: Hello, World!",  "Text 2: Bonjour le monde!",
                "Text 3: Hola mundo!",    "Text 4: Привет мир!",
                "Text 5: 你好世界!",      "Text 6: こんにちは世界!",
                "Text 7: مرحبا بالعالم!", "Text 8: Hej världen!"};

            auto start_time = std::chrono::high_resolution_clock::now();

            // Sequential conversion
            std::vector<std::string> sequential_results;
            for (const auto& text : texts) {
                try {
                    Converter converter("UTF-8", "UTF-16LE");
                    auto result = converter.convert_string(text);
                    sequential_results.push_back(result);
                } catch (const IconvError& e) {
                    sequential_results.push_back("ERROR: " +
                                                 std::string(e.what()));
                }
            }

            auto sequential_time = std::chrono::high_resolution_clock::now();

            // Parallel conversion
            std::vector<std::future<std::string>> futures;
            for (const auto& text : texts) {
                futures.push_back(
                    std::async(std::launch::async, [text]() -> std::string {
                        try {
                            Converter converter("UTF-8", "UTF-16LE");
                            return converter.convert_string(text);
                        } catch (const IconvError& e) {
                            return "ERROR: " + std::string(e.what());
                        }
                    }));
            }

            std::vector<std::string> parallel_results;
            for (auto& future : futures) {
                parallel_results.push_back(future.get());
            }

            auto parallel_time = std::chrono::high_resolution_clock::now();

            auto seq_duration =
                std::chrono::duration_cast<std::chrono::microseconds>(
                    sequential_time - start_time);
            auto par_duration =
                std::chrono::duration_cast<std::chrono::microseconds>(
                    parallel_time - sequential_time);

            std::cout << "Sequential conversion: " << seq_duration.count()
                      << " μs" << std::endl;
            std::cout << "Parallel conversion: " << par_duration.count()
                      << " μs" << std::endl;
            std::cout << "Speedup: "
                      << (static_cast<double>(seq_duration.count()) /
                          par_duration.count())
                      << "x" << std::endl;

            // Verify results are the same
            bool results_match =
                (sequential_results.size() == parallel_results.size());
            for (size_t i = 0; i < sequential_results.size() && results_match;
                 ++i) {
                if (sequential_results[i] != parallel_results[i]) {
                    results_match = false;
                }
            }
            std::cout << "Results match: " << (results_match ? "Yes" : "No")
                      << std::endl;
        }

        // 5. Conversion state management
        std::cout << "\n5. Conversion State Management:" << std::endl;
        {
            std::string large_text;
            for (int i = 0; i < 100; ++i) {
                large_text +=
                    "Chunk " + std::to_string(i) +
                    ": Unicode text with special chars: àáâãäå çñü ßæø\n";
            }

            std::cout << "Processing large text in chunks ("
                      << large_text.size() << " bytes)" << std::endl;

            try {
                Converter converter("UTF-8", "UTF-16LE");
                ConversionState state;

                const size_t chunk_size = 256;
                std::string output_text;

                for (size_t pos = 0; pos < large_text.size();
                     pos += chunk_size) {
                    size_t current_chunk_size =
                        std::min(chunk_size, large_text.size() - pos);
                    std::string chunk =
                        large_text.substr(pos, current_chunk_size);

                    auto chunk_result = converter.convert_string(chunk);
                    output_text += chunk_result;

                    state.processed_input_bytes += chunk.size();
                    state.processed_output_bytes += chunk_result.size();

                    if ((pos / chunk_size) % 10 == 0) {
                        std::cout << "  Processed "
                                  << state.processed_input_bytes << "/"
                                  << large_text.size() << " bytes" << std::endl;
                    }
                }

                state.is_complete = true;

                std::cout << "Conversion state:" << std::endl;
                std::cout << "  Input bytes: " << state.processed_input_bytes
                          << std::endl;
                std::cout << "  Output bytes: " << state.processed_output_bytes
                          << std::endl;
                std::cout << "  Complete: "
                          << (state.is_complete ? "Yes" : "No") << std::endl;
                std::cout << "  Expansion ratio: "
                          << (static_cast<double>(
                                  state.processed_output_bytes) /
                              state.processed_input_bytes)
                          << std::endl;
            } catch (const IconvError& e) {
                std::cout << "State management error: " << e.what()
                          << std::endl;
            }
        }

        // 6. File encoding detection and conversion
        std::cout << "\n6. File Encoding Detection and Conversion:"
                  << std::endl;
        {
            // Create test files in different encodings
            std::string test_content =
                "Test file content with special characters: àáâãäå çñü ßæø";

            std::vector<std::string> encodings = {"UTF-8", "ISO-8859-1",
                                                  "Windows-1252"};

            for (const auto& encoding : encodings) {
                std::string filename = "test_" + encoding + ".txt";
                // Replace special chars in filename
                std::replace(filename.begin(), filename.end(), '-', '_');

                create_encoded_file(filename, test_content, encoding);
            }

            // Try to detect and convert each file
            for (const auto& encoding : encodings) {
                std::string filename = "test_" + encoding + ".txt";
                std::replace(filename.begin(), filename.end(), '-', '_');

                if (std::filesystem::exists(filename)) {
                    try {
                        // Read file content
                        std::ifstream file(filename, std::ios::binary);
                        std::string content(
                            (std::istreambuf_iterator<char>(file)),
                            std::istreambuf_iterator<char>());

                        // Try to detect encoding
                        auto detection = detect_encoding(content);
                        std::cout << "File " << filename << ":" << std::endl;
                        std::cout
                            << "  Detected encoding: " << detection.encoding
                            << std::endl;
                        std::cout << "  Confidence: " << detection.confidence
                                  << std::endl;

                        // Convert to UTF-8 if not already
                        if (detection.encoding != "UTF-8") {
                            Converter converter(detection.encoding, "UTF-8");
                            auto utf8_content =
                                converter.convert_string(content);
                            std::cout << "  Converted content: " << utf8_content
                                      << std::endl;
                        }
                    } catch (const IconvError& e) {
                        std::cout << "Error processing " << filename << ": "
                                  << e.what() << std::endl;
                    }
                }
            }
        }

        // 7. Performance benchmarking
        std::cout << "\n7. Performance Benchmarking:" << std::endl;
        {
            std::string benchmark_text;
            for (int i = 0; i < 10000; ++i) {
                benchmark_text += "Performance test line " + std::to_string(i) +
                                  " with Unicode: àáâãäå\n";
            }

            std::cout << "Benchmarking conversion of " << benchmark_text.size()
                      << " bytes" << std::endl;

            std::vector<std::pair<std::string, std::string>> conversions = {
                {"UTF-8", "UTF-16LE"},
                {"UTF-8", "UTF-32BE"},
                {"UTF-8", "ISO-8859-1"},
                {"UTF-8", "ASCII"}};

            for (const auto& [from, to] : conversions) {
                try {
                    auto start = std::chrono::high_resolution_clock::now();

                    Converter converter(from, to);
                    auto result = converter.convert_string(benchmark_text);

                    auto end = std::chrono::high_resolution_clock::now();
                    auto duration =
                        std::chrono::duration_cast<std::chrono::microseconds>(
                            end - start);

                    double throughput =
                        (static_cast<double>(benchmark_text.size()) /
                         duration.count()) *
                        1000000.0 / (1024 * 1024);

                    std::cout << from << " -> " << to << ":" << std::endl;
                    std::cout << "  Time: " << duration.count() << " μs"
                              << std::endl;
                    std::cout << "  Throughput: " << throughput << " MB/s"
                              << std::endl;
                    std::cout << "  Output size: " << result.size() << " bytes"
                              << std::endl;
                } catch (const IconvError& e) {
                    std::cout << from << " -> " << to << ": Error - "
                              << e.what() << std::endl;
                }
            }
        }

        // 8. Memory usage optimization
        std::cout << "\n8. Memory Usage Optimization:" << std::endl;
        {
            std::cout << "Testing memory-efficient streaming conversion..."
                      << std::endl;

            // Simulate a very large file by processing in small chunks
            const size_t total_size = 1024 * 1024;  // 1MB
            const size_t chunk_size = 4096;         // 4KB chunks

            try {
                Converter converter("UTF-8", "UTF-16LE");
                size_t total_output = 0;

                auto start = std::chrono::high_resolution_clock::now();

                for (size_t processed = 0; processed < total_size;
                     processed += chunk_size) {
                    // Generate chunk data
                    std::string chunk_data;
                    for (size_t i = 0;
                         i < chunk_size && (processed + i) < total_size; ++i) {
                        chunk_data +=
                            static_cast<char>('A' + ((processed + i) % 26));
                    }

                    // Convert chunk
                    auto result = converter.convert_string(chunk_data);
                    total_output += result.size();

                    // Don't store the result to save memory
                }

                auto end = std::chrono::high_resolution_clock::now();
                auto duration =
                    std::chrono::duration_cast<std::chrono::milliseconds>(
                        end - start);

                std::cout << "Streaming conversion completed:" << std::endl;
                std::cout << "  Input size: " << total_size << " bytes"
                          << std::endl;
                std::cout << "  Output size: " << total_output << " bytes"
                          << std::endl;
                std::cout << "  Time: " << duration.count() << " ms"
                          << std::endl;
                std::cout << "  Throughput: "
                          << (static_cast<double>(total_size) /
                              duration.count())
                          << " KB/s" << std::endl;
            } catch (const IconvError& e) {
                std::cout << "Memory optimization test error: " << e.what()
                          << std::endl;
            }
        }

        // Cleanup test files
        std::cout << "\nCleaning up test files..." << std::endl;
        for (const std::string& encoding :
             {"UTF_8", "ISO_8859_1", "Windows_1252"}) {
            std::string filename = "test_" + encoding + ".txt";
            std::filesystem::remove(filename);
        }

        std::cout << "\n=== iconv Advanced Features Example Completed ==="
                  << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "Unexpected error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
