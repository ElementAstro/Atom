#pragma once

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <thread>
#include <future>
#include <vector>
#include <chrono>
#include <cstring>
#include "iconv_cpp.hpp"

namespace fs = std::filesystem;
using namespace iconv_cpp;

class IconvCppTest : public ::testing::Test {
protected:
    void SetUp() override {
        temp_input = fs::temp_directory_path() / "iconv_test_input.txt";
        temp_output = fs::temp_directory_path() / "iconv_test_output.txt";
        temp_output2 = fs::temp_directory_path() / "iconv_test_output2.txt";

        // Create test file with UTF-8 content including multibyte characters
        std::ofstream ofs(temp_input, std::ios::binary);
        ofs << "Hello, 世界! 🌍\nTest file with UTF-8 content.\n";
        ofs.close();

        // Create ASCII test file
        temp_ascii = fs::temp_directory_path() / "iconv_test_ascii.txt";
        std::ofstream ascii_ofs(temp_ascii, std::ios::binary);
        ascii_ofs << "Pure ASCII content 123";
        ascii_ofs.close();
    }

    void TearDown() override {
        // Clean up temp files
        for (const auto& file : {temp_input, temp_output, temp_output2, temp_ascii}) {
            if (fs::exists(file)) {
                fs::remove(file);
            }
        }
    }

    fs::path temp_input;
    fs::path temp_output;
    fs::path temp_output2;
    fs::path temp_ascii;
};

// Basic Converter Tests
TEST_F(IconvCppTest, BasicStringConversion) {
    std::string input = "Basic test string";
    auto output = convert_string("UTF-8", "UTF-8", input);
    EXPECT_EQ(input, output);
}

TEST_F(IconvCppTest, ConverterMoveSemantics) {
    Converter conv1("UTF-8", "UTF-8");
    std::string test = "move test";
    auto result1 = conv1.convert_string(test);

    // Move constructor
    Converter conv2 = std::move(conv1);
    auto result2 = conv2.convert_string(test);
    EXPECT_EQ(result1, result2);

    // Move assignment
    Converter conv3("UTF-8", "UTF-16LE");
    conv3 = std::move(conv2);
    auto result3 = conv3.convert_string(test);
    EXPECT_EQ(result1, result3);
}

TEST_F(IconvCppTest, ConverterGetters) {
    Converter conv("UTF-8", "UTF-16LE");
    EXPECT_EQ(conv.from_encoding(), "UTF-8");
    EXPECT_EQ(conv.to_encoding(), "UTF-16LE");
}

// UTF Conversion Tests
TEST_F(IconvCppTest, UTF8ToUTF16RoundTrip) {
    std::string utf8 = "Hello, 世界! 🌍";
    UTF8ToUTF16Converter to16;
    UTF16ToUTF8Converter to8;

    auto utf16 = to16.convert_u16string(utf8);
    EXPECT_GT(utf16.size(), 0);

    std::string roundtrip = to8.convert_u16string(utf16);
    EXPECT_EQ(utf8, roundtrip);
}

TEST_F(IconvCppTest, UTF8ToUTF32RoundTrip) {
    std::string utf8 = "Test 🌍 emoji";
    UTF8ToUTF32Converter to32;
    UTF32ToUTF8Converter to8;

    auto utf32 = to32.convert_u32string(utf8);
    EXPECT_GT(utf32.size(), 0);

    std::string roundtrip = to8.convert_u32string(utf32);
    EXPECT_EQ(utf8, roundtrip);
}

// Error Handling Tests
TEST_F(IconvCppTest, ErrorHandlingStrict) {
    std::string invalid_utf8 = "abc\xFF\xFE";
    Converter conv("UTF-8", "UTF-16LE");
    EXPECT_THROW(conv.convert_string(invalid_utf8), IconvConversionError);
}

TEST_F(IconvCppTest, ErrorHandlingReplace) {
    std::string invalid_utf8 = "abc\xFF\xFE";
    ConversionOptions opts;
    opts.error_policy = ErrorHandlingPolicy::Replace;
    opts.replacement_char = '?';

    Converter conv("UTF-8", "UTF-8", opts);
    std::string result = conv.convert_string(invalid_utf8);
    EXPECT_TRUE(result.find('?') != std::string::npos);
    EXPECT_TRUE(result.find("abc") != std::string::npos);
}

TEST_F(IconvCppTest, ErrorHandlingSkip) {
    std::string invalid_utf8 = "abc\xFF\\xFEdef";
    ConversionOptions opts;
    opts.error_policy = ErrorHandlingPolicy::Skip;

    Converter conv("UTF-8", "UTF-8", opts);
    std::string result = conv.convert_string(invalid_utf8);
    EXPECT_TRUE(result.find("abc") != std::string::npos);
    EXPECT_TRUE(result.find("def") != std::string::npos);
    EXPECT_EQ(result.find('\xFF'), std::string::npos);
}

TEST_F(IconvCppTest, ErrorHandlingIgnore) {
    std::string invalid_utf8 = "abc\xFF\xFE";
    ConversionOptions opts;
    opts.error_policy = ErrorHandlingPolicy::Ignore;

    Converter conv("UTF-8", "UTF-8", opts);
    std::string result = conv.convert_string(invalid_utf8);
    EXPECT_TRUE(result.find("abc") != std::string::npos);
}

TEST_F(IconvCppTest, ConversionOptionsTranslit) {
    ConversionOptions opts;
    opts.translit = true;
    auto encoding_str = opts.create_encoding_string("UTF-8");
    EXPECT_TRUE(encoding_str.find("//TRANSLIT") != std::string::npos);
}

TEST_F(IconvCppTest, IconvConversionErrorDetails) {
    try {
        std::string invalid = "abc\xFF";
        Converter conv("UTF-8", "UTF-16LE");
        conv.convert_string(invalid);
        FAIL() << "Expected IconvConversionError";
    } catch (const IconvConversionError& e) {
        EXPECT_GT(e.processed_bytes(), 0);
        EXPECT_TRUE(std::string(e.what()).find("Invalid") != std::string::npos ||
                   std::string(e.what()).find("Incomplete") != std::string::npos);
    }
}

// File Conversion Tests
TEST_F(IconvCppTest, FileConversion) {
    EXPECT_TRUE(convert_file("UTF-8", "UTF-8", temp_input, temp_output));
    EXPECT_TRUE(fs::exists(temp_output));
    EXPECT_GT(fs::file_size(temp_output), 0);
}

TEST_F(IconvCppTest, FileConversionWithProgress) {
    bool progress_called = false;
    size_t last_processed = 0;

    auto progress_cb = [&](size_t processed, size_t total) {
        progress_called = true;
        EXPECT_LE(processed, total);
        EXPECT_GE(processed, last_processed);
        last_processed = processed;
    };

    EXPECT_TRUE(convert_file("UTF-8", "UTF-8", temp_input, temp_output,
                           ConversionOptions(), progress_cb));
    EXPECT_TRUE(progress_called);
}

TEST_F(IconvCppTest, AsyncFileConversion) {
    auto future = convert_file_async("UTF-8", "UTF-8", temp_input, temp_output);
    EXPECT_TRUE(future.get());
    EXPECT_TRUE(fs::exists(temp_output));
}

TEST_F(IconvCppTest, FileConversionErrors) {
    fs::path nonexistent = "/nonexistent/path/file.txt";
    EXPECT_THROW(convert_file("UTF-8", "UTF-8", nonexistent, temp_output), IconvError);
}

// BOM Handling Tests
TEST_F(IconvCppTest, BomDetectionUTF8) {
    std::vector<char> utf8_bom = {'\xEF', '\xBB', '\xBF', 'H', 'e', 'l', 'l', 'o'};
    auto [encoding, size] = BomHandler::detect_bom(utf8_bom);
    EXPECT_EQ(encoding, "UTF-8");
    EXPECT_EQ(size, 3);
}

TEST_F(IconvCppTest, BomDetectionUTF16LE) {
    std::vector<char> utf16le_bom = {'\xFF', '\xFE', 'H', '\x00'};
    auto [encoding, size] = BomHandler::detect_bom(utf16le_bom);
    EXPECT_EQ(encoding, "UTF-16LE");
    EXPECT_EQ(size, 2);
}

TEST_F(IconvCppTest, BomDetectionUTF16BE) {
    std::vector<char> utf16be_bom = {'\xFE', '\xFF', '\x00', 'H'};
    auto [encoding, size] = BomHandler::detect_bom(utf16be_bom);
    EXPECT_EQ(encoding, "UTF-16BE");
    EXPECT_EQ(size, 2);
}

TEST_F(IconvCppTest, BomDetectionUTF32LE) {
    std::vector<char> utf32le_bom = {'\xFF', '\xFE', '\x00', '\x00', 'H', '\x00', '\x00', '\x00'};
    auto [encoding, size] = BomHandler::detect_bom(utf32le_bom);
    EXPECT_EQ(encoding, "UTF-32LE");
    EXPECT_EQ(size, 4);
}

TEST_F(IconvCppTest, BomDetectionUTF32BE) {
    std::vector<char> utf32be_bom = {'\x00', '\x00', '\xFE', '\xFF', '\x00', '\x00', '\x00', 'H'};
    auto [encoding, size] = BomHandler::detect_bom(utf32be_bom);
    EXPECT_EQ(encoding, "UTF-32BE");
    EXPECT_EQ(size, 4);
}

TEST_F(IconvCppTest, BomDetectionNoBom) {
    std::vector<char> no_bom = {'H', 'e', 'l', 'l', 'o'};
    auto [encoding, size] = BomHandler::detect_bom(no_bom);
    EXPECT_TRUE(encoding.empty());
    EXPECT_EQ(size, 0);
}

TEST_F(IconvCppTest, BomAddition) {
    std::vector<char> data = {'H', 'e', 'l', 'l', 'o'};
    auto with_bom = BomHandler::add_bom("UTF-8", data);
    EXPECT_GT(with_bom.size(), data.size());

    auto [detected_enc, bom_size] = BomHandler::detect_bom(with_bom);
    EXPECT_EQ(detected_enc, "UTF-8");
    EXPECT_EQ(bom_size, 3);
}

TEST_F(IconvCppTest, BomRemoval) {
    std::vector<char> utf8_with_bom = {'\xEF', '\xBB', '\xBF', 'H', 'e', 'l', 'l', 'o'};
    auto without_bom = BomHandler::remove_bom(utf8_with_bom);
    EXPECT_EQ(without_bom.size(), 5);
    EXPECT_EQ(without_bom[0], 'H');
}

// Encoding Detection Tests
TEST_F(IconvCppTest, EncodingDetectionASCII) {
    std::string ascii_text = "Pure ASCII text 123";
    auto results = EncodingDetector::detect_encoding({ascii_text.data(), ascii_text.size()});
    EXPECT_FALSE(results.empty());
    EXPECT_EQ(results[0].encoding, "ASCII");
    EXPECT_GT(results[0].confidence, 0.7f);
}

TEST_F(IconvCppTest, EncodingDetectionUTF8) {
    std::string utf8_text = "UTF-8 text with 中文 characters";
    auto results = EncodingDetector::detect_encoding({utf8_text.data(), utf8_text.size()});
    EXPECT_FALSE(results.empty());
    EXPECT_EQ(results[0].encoding, "UTF-8");
    EXPECT_GT(results[0].confidence, 0.8f);
}

TEST_F(IconvCppTest, EncodingDetectionWithBom) {
    std::vector<char> utf8_with_bom = {'\xEF', '\xBB', '\xBF', 'H', 'e', 'l', 'l', 'o'};
    auto results = EncodingDetector::detect_encoding(utf8_with_bom);
    EXPECT_FALSE(results.empty());
    EXPECT_EQ(results[0].encoding, "UTF-8");
    EXPECT_EQ(results[0].confidence, 1.0f);
}

TEST_F(IconvCppTest, EncodingDetectionMostLikely) {
    std::string text = "Simple text";
    auto encoding = EncodingDetector::detect_most_likely_encoding({text.data(), text.size()});
    EXPECT_FALSE(encoding.empty());
}

TEST_F(IconvCppTest, EncodingDetectionMaxResults) {
    std::string text = "Test text";
    auto results = EncodingDetector::detect_encoding({text.data(), text.size()}, 2);
    EXPECT_LE(results.size(), 2);
}

TEST_F(IconvCppTest, FileEncodingDetection) {
    auto encoding = detect_file_encoding(temp_ascii);
    EXPECT_TRUE(encoding == "ASCII" || encoding == "UTF-8");

    encoding = detect_file_encoding(temp_input);
    EXPECT_TRUE(encoding == "UTF-8" || encoding == "ASCII");
}

TEST_F(IconvCppTest, FileEncodingDetectionNonexistent) {
    EXPECT_THROW(detect_file_encoding("/nonexistent/file.txt"), IconvError);
}

// Encoding Registry Tests
TEST_F(IconvCppTest, EncodingRegistryInstance) {
    auto& reg1 = EncodingRegistry::instance();
    auto& reg2 = EncodingRegistry::instance();
    EXPECT_EQ(&reg1, &reg2);
}

TEST_F(IconvCppTest, EncodingRegistryListEncodings) {
    auto& registry = EncodingRegistry::instance();
    auto encodings = registry.list_all_encodings();
    EXPECT_FALSE(encodings.empty());
    EXPECT_GT(encodings.size(), 10);

    // Check for common encodings
    bool found_utf8 = false, found_ascii = false;
    for (const auto& enc : encodings) {
        if (enc.name == "UTF-8") found_utf8 = true;
        if (enc.name == "ASCII") found_ascii = true;
    }
    EXPECT_TRUE(found_utf8);
    EXPECT_TRUE(found_ascii);
}

TEST_F(IconvCppTest, EncodingRegistrySupport) {
    auto& registry = EncodingRegistry::instance();
    EXPECT_TRUE(registry.is_encoding_supported("UTF-8"));
    EXPECT_TRUE(registry.is_encoding_supported("ASCII"));
    EXPECT_FALSE(registry.is_encoding_supported("INVALID-ENCODING-12345"));
}

TEST_F(IconvCppTest, EncodingRegistryInfo) {
    auto& registry = EncodingRegistry::instance();
    auto info = registry.get_encoding_info("UTF-8");
    ASSERT_TRUE(info.has_value());
    EXPECT_EQ(info->name, "UTF-8");
    EXPECT_TRUE(info->is_ascii_compatible);
    EXPECT_EQ(info->min_char_size, 1);
    EXPECT_EQ(info->max_char_size, 4);

    auto invalid_info = registry.get_encoding_info("INVALID-ENCODING");
    EXPECT_FALSE(invalid_info.has_value());
}

// Buffer Manager Tests
TEST_F(IconvCppTest, BufferManagerCreate) {
    auto buffer = BufferManager::create_resizable_buffer(1024);
    EXPECT_EQ(buffer.size(), 1024);

    auto default_buffer = BufferManager::create_resizable_buffer();
    EXPECT_EQ(default_buffer.size(), 4096);
}

TEST_F(IconvCppTest, BufferManagerEnsureCapacity) {
    auto buffer = BufferManager::create_resizable_buffer(10);
    EXPECT_EQ(buffer.size(), 10);

    BufferManager::ensure_buffer_capacity(buffer, 50);
    EXPECT_GE(buffer.size(), 50);
}

TEST_F(IconvCppTest, BufferManagerEstimateSize) {
    size_t estimate = BufferManager::estimate_output_size(100, "UTF-8", "UTF-16LE");
    EXPECT_GT(estimate, 100);

    size_t unknown_estimate = BufferManager::estimate_output_size(100, "UNKNOWN", "UNKNOWN");
    EXPECT_EQ(unknown_estimate, 400); // 4x fallback
}

// Progress Callback Tests
TEST_F(IconvCppTest, ProgressCallbackCalled) {
    std::string large_input(10000, 'a');
    bool callback_called = false;
    size_t max_processed = 0;

    auto progress_cb = [&](size_t processed, size_t total) {
        callback_called = true;
        EXPECT_LE(processed, total);
        max_processed = std::max(max_processed, processed);
    };

    Converter conv("UTF-8", "UTF-8");
    auto result = conv.convert_with_progress({large_input.data(), large_input.size()}, progress_cb);

    EXPECT_TRUE(callback_called);
    EXPECT_EQ(max_processed, large_input.size());
    EXPECT_EQ(result.size(), large_input.size());
}

// Stateful Conversion Tests
TEST_F(IconvCppTest, StatefulConversion) {
    ConversionState state;
    Converter conv("UTF-8", "UTF-8");

    std::string part1 = "First part ";
    std::string part2 = "Second part";

    auto out1 = conv.convert_with_state({part1.data(), part1.size()}, state);
    EXPECT_GT(state.processed_input_bytes, 0);
    EXPECT_GT(state.processed_output_bytes, 0);

    auto out2 = conv.convert_with_state({part2.data(), part2.size()}, state);
    EXPECT_EQ(state.processed_input_bytes, part1.size() + part2.size());

    std::string combined(out1.begin(), out1.end());
    combined.append(out2.begin(), out2.end());
    EXPECT_EQ(combined, part1 + part2);
}

TEST_F(IconvCppTest, ConversionStateReset) {
    ConversionState state;
    state.processed_input_bytes = 100;
    state.processed_output_bytes = 50;
    state.is_complete = true;
    state.state_data = {'a', 'b', 'c'};

    state.reset();
    EXPECT_EQ(state.processed_input_bytes, 0);
    EXPECT_EQ(state.processed_output_bytes, 0);
    EXPECT_FALSE(state.is_complete);
    EXPECT_TRUE(state.state_data.empty());
}

// Stream Converter Tests
TEST_F(IconvCppTest, StreamConverter) {
    std::string input = "Stream conversion test with 中文";
    std::istringstream iss(input);
    std::ostringstream oss;

    StreamConverter sc("UTF-8", "UTF-8");
    sc.convert(iss, oss);

    EXPECT_EQ(oss.str(), input);
}

TEST_F(IconvCppTest, StreamConverterToString) {
    std::string input = "Convert to string test";
    std::istringstream iss(input);

    StreamConverter sc("UTF-8", "UTF-8");
    std::string result = sc.convert_to_string(iss);

    EXPECT_EQ(result, input);
}

TEST_F(IconvCppTest, StreamConverterFromString) {
    std::string input = "Convert from string test";
    std::ostringstream oss;

    StreamConverter sc("UTF-8", "UTF-8");
    sc.convert_from_string(input, oss);

    EXPECT_EQ(oss.str(), input);
}

TEST_F(IconvCppTest, StreamConverterWithProgress) {
    std::string input = "Stream with progress test";
    std::istringstream iss(input);
    std::ostringstream oss;
    bool progress_called = false;

    auto progress_cb = [&](size_t processed, size_t total) {
        progress_called = true;
        EXPECT_LE(processed, total);
    };

    StreamConverter sc("UTF-8", "UTF-8");
    sc.convert(iss, oss, progress_cb);

    EXPECT_EQ(oss.str(), input);
    // Note: Progress may not be called for small inputs
}

// Batch Converter Tests
TEST_F(IconvCppTest, BatchConverterStrings) {
    BatchConverter batch("UTF-8", "UTF-8");
    std::vector<std::string> inputs = {"first", "second", "third 中文"};

    auto outputs = batch.convert_strings(inputs);
    EXPECT_EQ(outputs.size(), inputs.size());
    EXPECT_EQ(outputs, inputs);
}

TEST_F(IconvCppTest, BatchConverterFiles) {
    BatchConverter batch("UTF-8", "UTF-8");
    std::vector<fs::path> input_paths = {temp_input};
    std::vector<fs::path> output_paths = {temp_output};

    auto results = batch.convert_files(input_paths, output_paths);
    EXPECT_EQ(results.size(), 1);
    EXPECT_TRUE(results[0]);
    EXPECT_TRUE(fs::exists(temp_output));
}

TEST_F(IconvCppTest, BatchConverterFilesMismatch) {
    BatchConverter batch("UTF-8", "UTF-8");
    std::vector<fs::path> input_paths = {temp_input, temp_ascii};
    std::vector<fs::path> output_paths = {temp_output}; // Size mismatch

    EXPECT_THROW(batch.convert_files(input_paths, output_paths), IconvError);
}

TEST_F(IconvCppTest, BatchConverterParallel) {
    BatchConverter batch("UTF-8", "UTF-8");
    std::vector<fs::path> input_paths = {temp_input, temp_ascii};
    std::vector<fs::path> output_paths = {temp_output, temp_output2};

    auto results = batch.convert_files_parallel(input_paths, output_paths, 2);
    EXPECT_EQ(results.size(), 2);
    EXPECT_TRUE(results[0]);
    EXPECT_TRUE(results[1]);
    EXPECT_TRUE(fs::exists(temp_output));
    EXPECT_TRUE(fs::exists(temp_output2));
}

// Specialized Converter Tests
TEST_F(IconvCppTest, ChineseEncodingConverter) {
    ChineseEncodingConverter conv;
    std::string utf8 = "你好世界";

    // Test GB18030 conversion
    std::string gb18030 = conv.utf8_to_gb18030_string(utf8);
    EXPECT_NE(gb18030, utf8);
    std::string utf8_back = conv.gb18030_to_utf8_string(gb18030);
    EXPECT_EQ(utf8_back, utf8);

    // Test GBK conversion
    std::string gbk = conv.utf8_to_gbk_string(utf8);
    EXPECT_NE(gbk, utf8);
    utf8_back = conv.gbk_to_utf8_string(gbk);
    EXPECT_EQ(utf8_back, utf8);

    // Test Big5 conversion
    std::string big5 = conv.utf8_to_big5_string(utf8);
    EXPECT_NE(big5, utf8);
    utf8_back = conv.big5_to_utf8_string(big5);
    EXPECT_EQ(utf8_back, utf8);
}

TEST_F(IconvCppTest, JapaneseEncodingConverter) {
    JapaneseEncodingConverter conv;
    std::string utf8 = "こんにちは";

    // Test Shift-JIS conversion
    std::string sjis = conv.utf8_to_shift_jis_string(utf8);
    EXPECT_NE(sjis, utf8);
    std::string utf8_back = conv.shift_jis_to_utf8_string(sjis);
    EXPECT_EQ(utf8_back, utf8);

    // Test EUC-JP conversion
    std::string euc_jp = conv.utf8_to_euc_jp_string(utf8);
    EXPECT_NE(euc_jp, utf8);
    utf8_back = conv.euc_jp_to_utf8_string(euc_jp);
    EXPECT_EQ(utf8_back, utf8);
}

TEST_F(IconvCppTest, KoreanEncodingConverter) {
    KoreanEncodingConverter conv;
    std::string utf8 = "안녕하세요";

    // Test EUC-KR conversion
    std::string euc_kr = conv.utf8_to_euc_kr_string(utf8);
    EXPECT_NE(euc_kr, utf8);
    std::string utf8_back = conv.euc_kr_to_utf8_string(euc_kr);
    EXPECT_EQ(utf8_back, utf8);
}

// Template Function Tests
TEST_F(IconvCppTest, ConvertStringToTemplate) {
    std::string input = "Template test";
    auto output = convert_string_to<std::string>("UTF-8", "UTF-8", input);
    EXPECT_EQ(output, input);
}

TEST_F(IconvCppTest, ConvertFunction) {
    std::string input = "Convert function test";
    auto output = convert("UTF-8", "UTF-8", {input.data(), input.size()});
    std::string result(output.begin(), output.end());
    EXPECT_EQ(result, input);
}

// Thread Safety Tests
TEST_F(IconvCppTest, ThreadSafety) {
    std::string input = "Thread safety test 线程安全测试";
    Converter conv("UTF-8", "UTF-8");

    const int num_threads = 4;
    const int iterations = 100;
    std::vector<std::thread> threads;
    std::vector<bool> results(num_threads, true);

    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&conv, &input, &results, t, iterations]() {
            try {
                for (int i = 0; i < iterations; ++i) {
                    auto result = conv.convert_string(input);
                    if (result != input) {
                        results[t] = false;
                        break;
                    }
                }
            } catch (...) {
                results[t] = false;
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    for (bool result : results) {
        EXPECT_TRUE(result);
    }
}

// Error Condition Tests
TEST_F(IconvCppTest, InvalidEncoding) {
    EXPECT_THROW(Converter("INVALID-FROM", "UTF-8"), IconvInitError);
    EXPECT_THROW(Converter("UTF-8", "INVALID-TO"), IconvInitError);
}

TEST_F(IconvCppTest, ConverterReset) {
    Converter conv("UTF-8", "UTF-8");
    std::string test = "Reset test";
    auto result1 = conv.convert_string(test);

    conv.reset(); // Should not affect subsequent conversions
    auto result2 = conv.convert_string(test);
    EXPECT_EQ(result1, result2);
}

// Performance Tests
TEST_F(IconvCppTest, LargeInputPerformance) {
    const size_t large_size = 1024 * 1024; // 1MB
    std::string large_input(large_size, 'A');

    auto start = std::chrono::high_resolution_clock::now();

    Converter conv("UTF-8", "UTF-8");
    auto result = conv.convert_string(large_input);

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    EXPECT_EQ(result.size(), large_size);
    // Performance assertion - should complete within reasonable time
    EXPECT_LT(duration.count(), 1000); // Less than 1 second
}

// Encoding Constants Tests
TEST_F(IconvCppTest, EncodingConstants) {
    EXPECT_EQ(encodings::UTF8, "UTF-8");
    EXPECT_EQ(encodings::UTF16LE, "UTF-16LE");
    EXPECT_EQ(encodings::UTF16BE, "UTF-16BE");
    EXPECT_EQ(encodings::UTF32LE, "UTF-32LE");
    EXPECT_EQ(encodings::UTF32BE, "UTF-32BE");
    EXPECT_EQ(encodings::ASCII, "ASCII");
    EXPECT_EQ(encodings::GB18030, "GB18030");
    EXPECT_EQ(encodings::SHIFT_JIS, "SHIFT-JIS");
}

// Edge Cases
TEST_F(IconvCppTest, EmptyStringConversion) {
    std::string empty = "";
    auto result = convert_string("UTF-8", "UTF-8", empty);
    EXPECT_TRUE(result.empty());
}

TEST_F(IconvCppTest, SingleCharacterConversion) {
    std::string single = "A";
    auto result = convert_string("UTF-8", "UTF-8", single);
    EXPECT_EQ(result, single);
}

TEST_F(IconvCppTest, OnlyMultibyteCharacters) {
    std::string multibyte = "中文日本語한국어";
    auto result = convert_string("UTF-8", "UTF-8", multibyte);
    EXPECT_EQ(result, multibyte);
}

TEST_F(IconvCppTest, MixedContentConversion) {
    std::string mixed = "ASCII 中文 123 🌍 test";
    auto result = convert_string("UTF-8", "UTF-8", mixed);
    EXPECT_EQ(result, mixed);
}
