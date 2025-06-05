#ifndef ICONV_CPP_HPP
#define ICONV_CPP_HPP

#include <iconv.h>
#include <algorithm>
#include <array>
#include <concepts>
#include <filesystem>
#include <format>
#include <fstream>
#include <functional>
#include <future>
#include <mutex>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace iconv_cpp {

class Converter;

class IconvError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

class IconvInitError : public IconvError {
public:
    using IconvError::IconvError;
};

class IconvConversionError : public IconvError {
public:
    IconvConversionError(std::string_view message, size_t processed_bytes)
        : IconvError(std::string(message)),
          m_processed_bytes(processed_bytes) {}
    size_t processed_bytes() const noexcept { return m_processed_bytes; }

private:
    size_t m_processed_bytes;
};

enum class ErrorHandlingPolicy { Strict, Skip, Replace, Ignore };

struct ConversionOptions {
    ErrorHandlingPolicy error_policy = ErrorHandlingPolicy::Strict;
    std::optional<char> replacement_char = std::nullopt;
    bool enable_fallback = false;
    bool translit = false;
    bool ignore_bom = false;

    std::string create_encoding_string(std::string_view base_encoding) const {
        std::string result(base_encoding);
        if (translit)
            result += "//TRANSLIT";
        if (error_policy == ErrorHandlingPolicy::Ignore)
            result += "//IGNORE";
        return result;
    }
};

using ProgressCallback =
    std::function<void(size_t processed_bytes, size_t total_bytes)>;

struct ConversionState {
    size_t processed_input_bytes = 0;
    size_t processed_output_bytes = 0;
    bool is_complete = false;
    std::vector<char> state_data;
    void reset() {
        processed_input_bytes = 0;
        processed_output_bytes = 0;
        is_complete = false;
        state_data.clear();
    }
};

struct EncodingInfo {
    std::string name;
    std::string description;
    bool is_ascii_compatible = false;
    int min_char_size = 1;
    int max_char_size = 1;
    bool has_bom = false;
    static bool is_alias(std::string_view encoding,
                         std::string_view possible_alias);
};

struct EncodingDetectionResult {
    std::string encoding;
    float confidence;
    bool operator>(const EncodingDetectionResult& other) const {
        return confidence > other.confidence;
    }
};

class BomHandler {
public:
    static std::pair<std::string, size_t> detect_bom(
        std::span<const char> data) {
        if (data.size() >= 3 && static_cast<unsigned char>(data[0]) == 0xEF &&
            static_cast<unsigned char>(data[1]) == 0xBB &&
            static_cast<unsigned char>(data[2]) == 0xBF) {
            return {"UTF-8", 3};
        }
        if (data.size() >= 2) {
            if (static_cast<unsigned char>(data[0]) == 0xFE &&
                static_cast<unsigned char>(data[1]) == 0xFF) {
                return {"UTF-16BE", 2};
            }
            if (static_cast<unsigned char>(data[0]) == 0xFF &&
                static_cast<unsigned char>(data[1]) == 0xFE) {
                if (data.size() >= 4 &&
                    static_cast<unsigned char>(data[2]) == 0x00 &&
                    static_cast<unsigned char>(data[3]) == 0x00) {
                    return {"UTF-32LE", 4};
                }
                return {"UTF-16LE", 2};
            }
        }
        if (data.size() >= 4) {
            if (static_cast<unsigned char>(data[0]) == 0x00 &&
                static_cast<unsigned char>(data[1]) == 0x00 &&
                static_cast<unsigned char>(data[2]) == 0xFE &&
                static_cast<unsigned char>(data[3]) == 0xFF) {
                return {"UTF-32BE", 4};
            }
        }
        return {"", 0};
    }

    static std::vector<char> add_bom(std::string_view encoding,
                                     std::span<const char> data) {
        std::vector<char> result;
        if (encoding == "UTF-8") {
            result = {'\xEF', '\xBB', '\xBF'};
        } else if (encoding == "UTF-16LE") {
            result = {'\xFF', '\xFE'};
        } else if (encoding == "UTF-16BE") {
            result = {'\xFE', '\xFF'};
        } else if (encoding == "UTF-32LE") {
            result = {'\xFF', '\xFE', '\x00', '\x00'};
        } else if (encoding == "UTF-32BE") {
            result = {'\x00', '\x00', '\xFE', '\xFF'};
        }
        result.insert(result.end(), data.begin(), data.end());
        return result;
    }

    static std::span<const char> remove_bom(std::span<const char> data) {
        auto [encoding, bom_size] = detect_bom(data);
        if (bom_size > 0) {
            return data.subspan(bom_size);
        }
        return data;
    }
};

class EncodingDetector {
public:
    static std::vector<EncodingDetectionResult> detect_encoding(
        std::span<const char> data, int max_results = 3) {
        std::vector<EncodingDetectionResult> results;
        auto [bom_encoding, bom_size] = BomHandler::detect_bom(data);
        if (!bom_encoding.empty()) {
            results.push_back({bom_encoding, 1.0f});
            return results;
        }
        if (is_valid_utf8(data))
            results.push_back({"UTF-8", 0.9f});
        if (is_ascii(data))
            results.push_back({"ASCII", 0.8f});
        if (might_be_gb18030(data))
            results.push_back({"GB18030", 0.6f});
        if (might_be_shift_jis(data))
            results.push_back({"SHIFT-JIS", 0.5f});
        if (might_be_euc_jp(data))
            results.push_back({"EUC-JP", 0.5f});
        if (might_be_big5(data))
            results.push_back({"BIG5", 0.5f});
        results.push_back({"ISO-8859-1", 0.3f});
        std::sort(results.begin(), results.end(), std::greater<>());
        if (results.size() > static_cast<size_t>(max_results)) {
            results.resize(max_results);
        }
        return results;
    }

    static std::string detect_most_likely_encoding(std::span<const char> data) {
        auto results = detect_encoding(data, 1);
        return results.empty() ? "UTF-8" : results[0].encoding;
    }

private:
    static bool is_valid_utf8(std::span<const char> data) {
        const unsigned char* bytes =
            reinterpret_cast<const unsigned char*>(data.data());
        size_t i = 0;
        while (i < data.size()) {
            if (bytes[i] <= 0x7F) {
                i++;
            } else if (bytes[i] >= 0xC0 && bytes[i] <= 0xDF) {
                if (i + 1 >= data.size() || (bytes[i + 1] & 0xC0) != 0x80)
                    return false;
                i += 2;
            } else if (bytes[i] >= 0xE0 && bytes[i] <= 0xEF) {
                if (i + 2 >= data.size() || (bytes[i + 1] & 0xC0) != 0x80 ||
                    (bytes[i + 2] & 0xC0) != 0x80)
                    return false;
                i += 3;
            } else if (bytes[i] >= 0xF0 && bytes[i] <= 0xF7) {
                if (i + 3 >= data.size() || (bytes[i + 1] & 0xC0) != 0x80 ||
                    (bytes[i + 2] & 0xC0) != 0x80 ||
                    (bytes[i + 3] & 0xC0) != 0x80)
                    return false;
                i += 4;
            } else {
                return false;
            }
        }
        return true;
    }

    static bool is_ascii(std::span<const char> data) {
        return std::all_of(data.begin(), data.end(), [](char c) {
            return static_cast<unsigned char>(c) <= 0x7F;
        });
    }

    static bool might_be_gb18030(std::span<const char> data) {
        size_t chinese_byte_sequences = 0;
        for (size_t i = 0; i < data.size() - 1; ++i) {
            unsigned char b1 = static_cast<unsigned char>(data[i]);
            unsigned char b2 = static_cast<unsigned char>(data[i + 1]);
            if ((b1 >= 0x81 && b1 <= 0xFE) &&
                ((b2 >= 0x40 && b2 <= 0x7E) || (b2 >= 0x80 && b2 <= 0xFE))) {
                chinese_byte_sequences++;
                i++;
            }
        }
        return chinese_byte_sequences > 0 &&
               chinese_byte_sequences * 2 > data.size() * 0.1;
    }

    static bool might_be_shift_jis(std::span<const char> data) {
        size_t sjis_sequences = 0;
        for (size_t i = 0; i < data.size() - 1; ++i) {
            unsigned char b1 = static_cast<unsigned char>(data[i]);
            unsigned char b2 = static_cast<unsigned char>(data[i + 1]);
            if (((b1 >= 0x81 && b1 <= 0x9F) || (b1 >= 0xE0 && b1 <= 0xEF)) &&
                ((b2 >= 0x40 && b2 <= 0x7E) || (b2 >= 0x80 && b2 <= 0xFC))) {
                sjis_sequences++;
                i++;
            }
        }
        return sjis_sequences > 0 && sjis_sequences * 2 > data.size() * 0.1;
    }

    static bool might_be_euc_jp(std::span<const char> data) {
        size_t euc_jp_sequences = 0;
        for (size_t i = 0; i < data.size() - 1; ++i) {
            unsigned char b1 = static_cast<unsigned char>(data[i]);
            unsigned char b2 = static_cast<unsigned char>(data[i + 1]);
            if ((b1 >= 0xA1 && b1 <= 0xFE) && (b2 >= 0xA1 && b2 <= 0xFE)) {
                euc_jp_sequences++;
                i++;
            }
        }
        return euc_jp_sequences > 0 && euc_jp_sequences * 2 > data.size() * 0.1;
    }

    static bool might_be_big5(std::span<const char> data) {
        size_t big5_sequences = 0;
        for (size_t i = 0; i < data.size() - 1; ++i) {
            unsigned char b1 = static_cast<unsigned char>(data[i]);
            unsigned char b2 = static_cast<unsigned char>(data[i + 1]);
            if ((b1 >= 0xA1 && b1 <= 0xF9) &&
                ((b2 >= 0x40 && b2 <= 0x7E) || (b2 >= 0xA1 && b2 <= 0xFE))) {
                big5_sequences++;
                i++;
            }
        }
        return big5_sequences > 0 && big5_sequences * 2 > data.size() * 0.1;
    }
};

class EncodingRegistry {
public:
    static EncodingRegistry& instance() {
        static EncodingRegistry registry;
        return registry;
    }

    std::vector<EncodingInfo> list_all_encodings() {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_encodings.empty())
            return m_encodings;
        m_encodings = {
            {"UTF-8", "Unicode UTF-8", true, 1, 4, true},
            {"UTF-16LE", "Unicode UTF-16 Little Endian", false, 2, 4, true},
            {"UTF-16BE", "Unicode UTF-16 Big Endian", false, 2, 4, true},
            {"UTF-32LE", "Unicode UTF-32 Little Endian", false, 4, 4, true},
            {"UTF-32BE", "Unicode UTF-32 Big Endian", false, 4, 4, true},
            {"ASCII", "US ASCII", true, 1, 1, false},
            {"ISO-8859-1", "Western European", true, 1, 1, false},
            {"ISO-8859-2", "Central European", true, 1, 1, false},
            {"ISO-8859-3", "South European", true, 1, 1, false},
            {"ISO-8859-4", "North European", true, 1, 1, false},
            {"ISO-8859-5", "Cyrillic", true, 1, 1, false},
            {"ISO-8859-6", "Arabic", true, 1, 1, false},
            {"ISO-8859-7", "Greek", true, 1, 1, false},
            {"ISO-8859-8", "Hebrew", true, 1, 1, false},
            {"ISO-8859-9", "Turkish", true, 1, 1, false},
            {"ISO-8859-10", "Nordic", true, 1, 1, false},
            {"ISO-8859-13", "Baltic", true, 1, 1, false},
            {"ISO-8859-14", "Celtic", true, 1, 1, false},
            {"ISO-8859-15", "Western European with Euro", true, 1, 1, false},
            {"ISO-8859-16", "South-Eastern European", true, 1, 1, false},
            {"CP1250", "Central European (Windows)", true, 1, 1, false},
            {"CP1251", "Cyrillic (Windows)", true, 1, 1, false},
            {"CP1252", "Western European (Windows)", true, 1, 1, false},
            {"CP1253", "Greek (Windows)", true, 1, 1, false},
            {"CP1254", "Turkish (Windows)", true, 1, 1, false},
            {"CP1255", "Hebrew (Windows)", true, 1, 1, false},
            {"CP1256", "Arabic (Windows)", true, 1, 1, false},
            {"CP1257", "Baltic (Windows)", true, 1, 1, false},
            {"CP1258", "Vietnamese (Windows)", true, 1, 1, false},
            {"GB18030", "Chinese National Standard", false, 1, 4, false},
            {"GBK", "Chinese Simplified", false, 1, 2, false},
            {"BIG5", "Chinese Traditional", false, 1, 2, false},
            {"EUC-JP", "Japanese EUC", false, 1, 3, false},
            {"SHIFT-JIS", "Japanese Shift-JIS", false, 1, 2, false},
            {"EUC-KR", "Korean EUC", false, 1, 2, false},
            {"KOI8-R", "Russian", true, 1, 1, false},
            {"KOI8-U", "Ukrainian", true, 1, 1, false},
            {"TIS-620", "Thai", true, 1, 1, false}};
        try {
            probe_supported_encodings();
        } catch (...) {
        }
        return m_encodings;
    }

    bool is_encoding_supported(std::string_view encoding) {
        auto encodings = list_all_encodings();
        auto it = std::find_if(encodings.begin(), encodings.end(),
                               [&encoding](const EncodingInfo& info) {
                                   return info.name == encoding;
                               });
        if (it != encodings.end())
            return true;
        try {
            iconv_t cd = iconv_open("UTF-8", encoding.data());
            if (cd != (iconv_t)-1) {
                iconv_close(cd);
                return true;
            }
            return false;
        } catch (...) {
            return false;
        }
    }

    std::optional<EncodingInfo> get_encoding_info(std::string_view encoding) {
        auto encodings = list_all_encodings();
        for (const auto& info : encodings) {
            if (info.name == encoding)
                return info;
        }
        return std::nullopt;
    }

private:
    EncodingRegistry() = default;
    void probe_supported_encodings() {}
    std::mutex m_mutex;
    std::vector<EncodingInfo> m_encodings;
};

class BufferManager {
public:
    static std::vector<char> create_resizable_buffer(
        size_t initial_size = 4096) {
        return std::vector<char>(initial_size);
    }
    static void ensure_buffer_capacity(std::vector<char>& buffer,
                                       size_t required_size) {
        if (buffer.size() < required_size) {
            buffer.resize(std::max(buffer.size() * 2, required_size));
        }
    }
    static size_t estimate_output_size(size_t input_size,
                                       const std::string& from_encoding,
                                       const std::string& to_encoding) {
        auto& registry = EncodingRegistry::instance();
        auto from_info = registry.get_encoding_info(from_encoding);
        auto to_info = registry.get_encoding_info(to_encoding);
        if (!from_info || !to_info)
            return input_size * 4;
        float ratio = static_cast<float>(to_info->max_char_size) /
                      static_cast<float>(from_info->min_char_size);
        return static_cast<size_t>(input_size * ratio) + 16;
    }
};

class Converter {
public:
    Converter(std::string_view from_encoding, std::string_view to_encoding,
              const ConversionOptions& options = ConversionOptions())
        : m_from_encoding(from_encoding),
          m_to_encoding(to_encoding),
          m_options(options) {
        std::string from_enc_str(from_encoding);
        std::string to_enc_str = options.create_encoding_string(to_encoding);
        m_cd = iconv_open(to_enc_str.c_str(), from_enc_str.c_str());
        if (m_cd == (iconv_t)-1) {
            switch (errno) {
                case EINVAL:
                    throw IconvInitError(
                        std::format("Conversion from {} to {} is not supported",
                                    from_encoding, to_encoding));
                default:
                    throw IconvInitError(
                        "Failed to initialize iconv conversion descriptor");
            }
        }
    }
    Converter(const Converter&) = delete;
    Converter& operator=(const Converter&) = delete;
    Converter(Converter&& other) noexcept
        : m_cd(other.m_cd),
          m_from_encoding(std::move(other.m_from_encoding)),
          m_to_encoding(std::move(other.m_to_encoding)),
          m_options(other.m_options) {
        other.m_cd = (iconv_t)-1;
    }
    Converter& operator=(Converter&& other) noexcept {
        if (this != &other) {
            close();
            m_cd = other.m_cd;
            m_from_encoding = std::move(other.m_from_encoding);
            m_to_encoding = std::move(other.m_to_encoding);
            m_options = other.m_options;
            other.m_cd = (iconv_t)-1;
        }
        return *this;
    }
    ~Converter() { close(); }

    void reset() {
        std::lock_guard<std::mutex> lock(m_mutex);
        iconv(m_cd, nullptr, nullptr, nullptr, nullptr);
    }
    std::string_view from_encoding() const { return m_from_encoding; }
    std::string_view to_encoding() const { return m_to_encoding; }

    std::vector<char> convert(std::span<const char> input) {
        std::lock_guard<std::mutex> lock(m_mutex);
        reset();
        size_t estimated_size = BufferManager::estimate_output_size(
            input.size(), m_from_encoding, m_to_encoding);
        std::vector<char> output(estimated_size);
        char* inbuf = const_cast<char*>(input.data());
        size_t inbytesleft = input.size();
        char* outbuf = output.data();
        size_t outbytesleft = output.size();
        while (inbytesleft > 0) {
            size_t result =
                iconv(m_cd, &inbuf, &inbytesleft, &outbuf, &outbytesleft);
            if (result == (size_t)-1) {
                if (errno == E2BIG) {
                    size_t used = output.size() - outbytesleft;
                    output.resize(output.size() * 2);
                    outbuf = output.data() + used;
                    outbytesleft = output.size() - used;
                } else if (errno == EILSEQ || errno == EINVAL) {
                    handle_conversion_error(errno, inbuf, inbytesleft, outbuf,
                                            outbytesleft, output);
                } else {
                    size_t processed = input.size() - inbytesleft;
                    throw IconvConversionError(
                        "Unknown error during conversion", processed);
                }
            }
        }
        output.resize(output.size() - outbytesleft);
        return output;
    }

    std::string convert_string(std::string_view input) {
        auto result = convert({input.data(), input.size()});
        return std::string(result.begin(), result.end());
    }

    std::vector<char> convert_with_progress(
        std::span<const char> input, ProgressCallback progress_callback) {
        std::lock_guard<std::mutex> lock(m_mutex);
        reset();
        size_t estimated_size = BufferManager::estimate_output_size(
            input.size(), m_from_encoding, m_to_encoding);
        std::vector<char> output(estimated_size);
        char* inbuf = const_cast<char*>(input.data());
        size_t inbytesleft = input.size();
        size_t total_size = input.size();
        char* outbuf = output.data();
        size_t outbytesleft = output.size();
        size_t last_reported = 0;
        while (inbytesleft > 0) {
            size_t result =
                iconv(m_cd, &inbuf, &inbytesleft, &outbuf, &outbytesleft);
            size_t processed = total_size - inbytesleft;
            if (processed >= last_reported + (total_size / 100) ||
                processed == total_size) {
                progress_callback(processed, total_size);
                last_reported = processed;
            }
            if (result == (size_t)-1) {
                if (errno == E2BIG) {
                    size_t used = output.size() - outbytesleft;
                    output.resize(output.size() * 2);
                    outbuf = output.data() + used;
                    outbytesleft = output.size() - used;
                } else if (errno == EILSEQ || errno == EINVAL) {
                    handle_conversion_error(errno, inbuf, inbytesleft, outbuf,
                                            outbytesleft, output);
                } else {
                    size_t processed = total_size - inbytesleft;
                    throw IconvConversionError(
                        "Unknown error during conversion", processed);
                }
            }
        }
        progress_callback(total_size, total_size);
        output.resize(output.size() - outbytesleft);
        return output;
    }

    std::vector<char> convert_with_state(std::span<const char> input,
                                         ConversionState& state) {
        std::lock_guard<std::mutex> lock(m_mutex);
        size_t estimated_size = BufferManager::estimate_output_size(
            input.size(), m_from_encoding, m_to_encoding);
        std::vector<char> output(estimated_size);
        char* inbuf = const_cast<char*>(input.data());
        size_t inbytesleft = input.size();
        char* outbuf = output.data();
        size_t outbytesleft = output.size();
        while (inbytesleft > 0) {
            size_t result =
                iconv(m_cd, &inbuf, &inbytesleft, &outbuf, &outbytesleft);
            if (result == (size_t)-1) {
                if (errno == E2BIG) {
                    size_t used = output.size() - outbytesleft;
                    output.resize(output.size() * 2);
                    outbuf = output.data() + used;
                    outbytesleft = output.size() - used;
                } else if (errno == EILSEQ || errno == EINVAL) {
                    handle_conversion_error(errno, inbuf, inbytesleft, outbuf,
                                            outbytesleft, output);
                } else {
                    throw IconvConversionError(
                        "Unknown error during conversion",
                        input.size() - inbytesleft);
                }
            }
        }
        state.processed_input_bytes += input.size();
        state.processed_output_bytes += (output.size() - outbytesleft);
        state.is_complete = (inbytesleft == 0);
        output.resize(output.size() - outbytesleft);
        return output;
    }

    bool convert_file(const std::filesystem::path& input_path,
                      const std::filesystem::path& output_path,
                      ProgressCallback progress_callback = nullptr) {
        std::ifstream input_file(input_path, std::ios::binary);
        if (!input_file)
            throw IconvError("Cannot open input file: " + input_path.string());
        input_file.seekg(0, std::ios::end);
        size_t file_size = input_file.tellg();
        input_file.seekg(0, std::ios::beg);
        std::ofstream output_file(output_path, std::ios::binary);
        if (!output_file)
            throw IconvError("Cannot create output file: " +
                             output_path.string());
        constexpr size_t CHUNK_SIZE = 1024 * 1024;
        std::vector<char> input_buffer(CHUNK_SIZE);
        ConversionState state;
        size_t total_processed = 0;
        while (input_file && total_processed < file_size) {
            input_file.read(input_buffer.data(), CHUNK_SIZE);
            size_t bytes_read = input_file.gcount();
            if (bytes_read == 0)
                break;
            std::span<const char> input_span(input_buffer.data(), bytes_read);
            std::vector<char> output;
            if (progress_callback) {
                output = convert_with_progress(
                    input_span, [&](size_t processed, size_t total) {
                        double chunk_progress =
                            static_cast<double>(processed) / total;
                        size_t overall_processed =
                            total_processed +
                            static_cast<size_t>(chunk_progress * bytes_read);
                        progress_callback(overall_processed, file_size);
                    });
            } else {
                output = convert_with_state(input_span, state);
            }
            output_file.write(output.data(), output.size());
            total_processed += bytes_read;
        }
        return true;
    }

    std::future<bool> convert_file_async(
        const std::filesystem::path& input_path,
        const std::filesystem::path& output_path,
        ProgressCallback progress_callback = nullptr) {
        return std::async(std::launch::async,
                          [this, input_path, output_path, progress_callback]() {
                              return this->convert_file(input_path, output_path,
                                                        progress_callback);
                          });
    }

private:
    iconv_t m_cd;
    std::string m_from_encoding;
    std::string m_to_encoding;
    ConversionOptions m_options;
    std::mutex m_mutex;

    void handle_conversion_error(int error_code, char*& inbuf,
                                 size_t& inbytesleft, char*& outbuf,
                                 size_t& outbytesleft,
                                 std::vector<char>& output) {
        switch (m_options.error_policy) {
            case ErrorHandlingPolicy::Strict:
                if (error_code == EILSEQ) {
                    throw IconvConversionError(
                        "Invalid multibyte sequence in input",
                        output.size() - outbytesleft);
                } else if (error_code == EINVAL) {
                    throw IconvConversionError(
                        "Incomplete multibyte sequence in input",
                        output.size() - outbytesleft);
                }
                break;
            case ErrorHandlingPolicy::Skip:
            case ErrorHandlingPolicy::Ignore:
                if (inbytesleft > 0) {
                    inbuf++;
                    inbytesleft--;
                }
                break;
            case ErrorHandlingPolicy::Replace:
                if (m_options.replacement_char.has_value() &&
                    outbytesleft >= 1) {
                    *outbuf = m_options.replacement_char.value();
                    outbuf++;
                    outbytesleft--;
                }
                if (inbytesleft > 0) {
                    inbuf++;
                    inbytesleft--;
                }
                break;
        }
    }

    void close() {
        if (m_cd != (iconv_t)-1) {
            iconv_close(m_cd);
            m_cd = (iconv_t)-1;
        }
    }
};

class StreamConverter {
public:
    StreamConverter(std::string_view from_encoding,
                    std::string_view to_encoding,
                    const ConversionOptions& options = ConversionOptions())
        : m_converter(from_encoding, to_encoding, options) {}

    void convert(std::istream& input, std::ostream& output,
                 ProgressCallback progress_callback = nullptr) {
        auto current_pos = input.tellg();
        input.seekg(0, std::ios::end);
        size_t stream_size = input.tellg();
        input.seekg(current_pos);
        constexpr size_t CHUNK_SIZE = 4096;
        std::vector<char> buffer(CHUNK_SIZE);
        size_t total_read = 0;
        ConversionState state;
        while (input) {
            input.read(buffer.data(), CHUNK_SIZE);
            size_t bytes_read = input.gcount();
            if (bytes_read == 0)
                break;
            std::span<const char> input_span(buffer.data(), bytes_read);
            std::vector<char> converted;
            if (progress_callback) {
                converted = m_converter.convert_with_progress(
                    input_span, [&](size_t processed, size_t total) {
                        size_t overall_processed =
                            total_read + (processed * bytes_read / total);
                        progress_callback(overall_processed, stream_size);
                    });
            } else {
                converted = m_converter.convert_with_state(input_span, state);
            }
            output.write(converted.data(), converted.size());
            total_read += bytes_read;
        }
    }

    std::string convert_to_string(
        std::istream& input, ProgressCallback progress_callback = nullptr) {
        std::ostringstream output;
        convert(input, output, progress_callback);
        return output.str();
    }

    void convert_from_string(std::string_view input, std::ostream& output,
                             ProgressCallback progress_callback = nullptr) {
        std::string input_copy(input);
        std::istringstream input_stream(input_copy);
        convert(input_stream, output, progress_callback);
    }

private:
    Converter m_converter;
};

inline std::vector<char> convert(
    std::string_view from_encoding, std::string_view to_encoding,
    std::span<const char> input,
    const ConversionOptions& options = ConversionOptions()) {
    Converter converter(from_encoding, to_encoding, options);
    return converter.convert(input);
}

inline std::string convert_string(
    std::string_view from_encoding, std::string_view to_encoding,
    std::string_view input,
    const ConversionOptions& options = ConversionOptions()) {
    Converter converter(from_encoding, to_encoding, options);
    return converter.convert_string(input);
}

inline bool convert_file(std::string_view from_encoding,
                         std::string_view to_encoding,
                         const std::filesystem::path& input_path,
                         const std::filesystem::path& output_path,
                         const ConversionOptions& options = ConversionOptions(),
                         ProgressCallback progress_callback = nullptr) {
    Converter converter(from_encoding, to_encoding, options);
    return converter.convert_file(input_path, output_path, progress_callback);
}

inline std::future<bool> convert_file_async(
    std::string_view from_encoding, std::string_view to_encoding,
    const std::filesystem::path& input_path,
    const std::filesystem::path& output_path,
    const ConversionOptions& options = ConversionOptions(),
    ProgressCallback progress_callback = nullptr) {
    return std::async(std::launch::async, [=]() {
        Converter converter(from_encoding, to_encoding, options);
        return converter.convert_file(input_path, output_path,
                                      progress_callback);
    });
}

inline std::string detect_file_encoding(const std::filesystem::path& file_path,
                                        int max_check_size = 16384) {
    std::ifstream file(file_path, std::ios::binary);
    if (!file)
        throw IconvError("Cannot open file: " + file_path.string());
    std::vector<char> buffer(max_check_size);
    file.read(buffer.data(), max_check_size);
    size_t bytes_read = file.gcount();
    if (bytes_read == 0)
        return "UTF-8";
    return EncodingDetector::detect_most_likely_encoding(
        {buffer.data(), bytes_read});
}

template <typename T>
concept StringLike = requires(T t) {
    { t.data() } -> std::convertible_to<const char*>;
    { t.size() } -> std::convertible_to<std::size_t>;
};

template <StringLike InputT, StringLike OutputT = std::string>
OutputT convert_string_to(
    std::string_view from_encoding, std::string_view to_encoding,
    const InputT& input,
    const ConversionOptions& options = ConversionOptions()) {
    auto result = convert(from_encoding, to_encoding,
                          {input.data(), input.size()}, options);
    return OutputT(result.begin(), result.end());
}

namespace encodings {
inline constexpr std::string_view UTF8 = "UTF-8";
inline constexpr std::string_view UTF16 = "UTF-16";
inline constexpr std::string_view UTF16LE = "UTF-16LE";
inline constexpr std::string_view UTF16BE = "UTF-16BE";
inline constexpr std::string_view UTF32 = "UTF-32";
inline constexpr std::string_view UTF32LE = "UTF-32LE";
inline constexpr std::string_view UTF32BE = "UTF-32BE";
inline constexpr std::string_view ASCII = "ASCII";
inline constexpr std::string_view ISO8859_1 = "ISO-8859-1";
inline constexpr std::string_view GB18030 = "GB18030";
inline constexpr std::string_view GBK = "GBK";
inline constexpr std::string_view BIG5 = "BIG5";
inline constexpr std::string_view SHIFT_JIS = "SHIFT-JIS";
inline constexpr std::string_view EUC_JP = "EUC-JP";
inline constexpr std::string_view EUC_KR = "EUC-KR";
}  // namespace encodings

class UTF8ToUTF16Converter : public Converter {
public:
    UTF8ToUTF16Converter(const ConversionOptions& options = ConversionOptions())
        : Converter(encodings::UTF8, encodings::UTF16LE, options) {}
    std::u16string convert_u16string(std::string_view utf8_str) {
        auto result = convert({utf8_str.data(), utf8_str.size()});
        auto* u16_data = reinterpret_cast<const char16_t*>(result.data());
        size_t u16_length = result.size() / sizeof(char16_t);
        return std::u16string(u16_data, u16_length);
    }
};

class UTF16ToUTF8Converter : public Converter {
public:
    UTF16ToUTF8Converter(const ConversionOptions& options = ConversionOptions())
        : Converter(encodings::UTF16LE, encodings::UTF8, options) {}
    std::string convert_u16string(std::u16string_view utf16_str) {
        auto* char_data = reinterpret_cast<const char*>(utf16_str.data());
        size_t byte_length = utf16_str.size() * sizeof(char16_t);
        auto result = convert({char_data, byte_length});
        return std::string(result.begin(), result.end());
    }
};

class UTF8ToUTF32Converter : public Converter {
public:
    UTF8ToUTF32Converter(const ConversionOptions& options = ConversionOptions())
        : Converter(encodings::UTF8, encodings::UTF32LE, options) {}
    std::u32string convert_u32string(std::string_view utf8_str) {
        auto result = convert({utf8_str.data(), utf8_str.size()});
        auto* u32_data = reinterpret_cast<const char32_t*>(result.data());
        size_t u32_length = result.size() / sizeof(char32_t);
        return std::u32string(u32_data, u32_length);
    }
};

class UTF32ToUTF8Converter : public Converter {
public:
    UTF32ToUTF8Converter(const ConversionOptions& options = ConversionOptions())
        : Converter(encodings::UTF32LE, encodings::UTF8, options) {}
    std::string convert_u32string(std::u32string_view utf32_str) {
        auto* char_data = reinterpret_cast<const char*>(utf32_str.data());
        size_t byte_length = utf32_str.size() * sizeof(char32_t);
        auto result = convert({char_data, byte_length});
        return std::string(result.begin(), result.end());
    }
};

class ChineseEncodingConverter {
public:
    ChineseEncodingConverter()
        : utf8_to_gb18030(encodings::UTF8, encodings::GB18030),
          gb18030_to_utf8(encodings::GB18030, encodings::UTF8),
          utf8_to_gbk(encodings::UTF8, encodings::GBK),
          gbk_to_utf8(encodings::GBK, encodings::UTF8),
          utf8_to_big5(encodings::UTF8, encodings::BIG5),
          big5_to_utf8(encodings::BIG5, encodings::UTF8) {}

    std::string utf8_to_gb18030_string(std::string_view utf8_str) {
        return utf8_to_gb18030.convert_string(utf8_str);
    }
    std::string gb18030_to_utf8_string(std::string_view gb18030_str) {
        return gb18030_to_utf8.convert_string(gb18030_str);
    }
    std::string utf8_to_gbk_string(std::string_view utf8_str) {
        return utf8_to_gbk.convert_string(utf8_str);
    }
    std::string gbk_to_utf8_string(std::string_view gbk_str) {
        return gbk_to_utf8.convert_string(gbk_str);
    }
    std::string utf8_to_big5_string(std::string_view utf8_str) {
        return utf8_to_big5.convert_string(utf8_str);
    }
    std::string big5_to_utf8_string(std::string_view big5_str) {
        return big5_to_utf8.convert_string(big5_str);
    }

private:
    Converter utf8_to_gb18030;
    Converter gb18030_to_utf8;
    Converter utf8_to_gbk;
    Converter gbk_to_utf8;
    Converter utf8_to_big5;
    Converter big5_to_utf8;
};

class JapaneseEncodingConverter {
public:
    JapaneseEncodingConverter()
        : utf8_to_sjis(encodings::UTF8, encodings::SHIFT_JIS),
          sjis_to_utf8(encodings::SHIFT_JIS, encodings::UTF8),
          utf8_to_euc_jp(encodings::UTF8, encodings::EUC_JP),
          euc_jp_to_utf8(encodings::EUC_JP, encodings::UTF8) {}

    std::string utf8_to_shift_jis_string(std::string_view utf8_str) {
        return utf8_to_sjis.convert_string(utf8_str);
    }
    std::string shift_jis_to_utf8_string(std::string_view sjis_str) {
        return sjis_to_utf8.convert_string(sjis_str);
    }
    std::string utf8_to_euc_jp_string(std::string_view utf8_str) {
        return utf8_to_euc_jp.convert_string(utf8_str);
    }
    std::string euc_jp_to_utf8_string(std::string_view euc_jp_str) {
        return euc_jp_to_utf8.convert_string(euc_jp_str);
    }

private:
    Converter utf8_to_sjis;
    Converter sjis_to_utf8;
    Converter utf8_to_euc_jp;
    Converter euc_jp_to_utf8;
};

class KoreanEncodingConverter {
public:
    KoreanEncodingConverter()
        : utf8_to_euc_kr(encodings::UTF8, encodings::EUC_KR),
          euc_kr_to_utf8(encodings::EUC_KR, encodings::UTF8) {}

    std::string utf8_to_euc_kr_string(std::string_view utf8_str) {
        return utf8_to_euc_kr.convert_string(utf8_str);
    }
    std::string euc_kr_to_utf8_string(std::string_view euc_kr_str) {
        return euc_kr_to_utf8.convert_string(euc_kr_str);
    }

private:
    Converter utf8_to_euc_kr;
    Converter euc_kr_to_utf8;
};

class BatchConverter {
public:
    BatchConverter(std::string_view from_encoding, std::string_view to_encoding,
                   const ConversionOptions& options = ConversionOptions())
        : m_converter(from_encoding, to_encoding, options) {}

    std::vector<std::string> convert_strings(
        const std::vector<std::string>& inputs) {
        std::vector<std::string> results;
        results.reserve(inputs.size());
        for (const auto& input : inputs) {
            results.push_back(m_converter.convert_string(input));
        }
        return results;
    }

    std::vector<bool> convert_files(
        const std::vector<std::filesystem::path>& input_paths,
        const std::vector<std::filesystem::path>& output_paths) {
        if (input_paths.size() != output_paths.size()) {
            throw IconvError("Input and output file path counts do not match");
        }
        std::vector<bool> results(input_paths.size());
        for (size_t i = 0; i < input_paths.size(); ++i) {
            try {
                results[i] =
                    m_converter.convert_file(input_paths[i], output_paths[i]);
            } catch (...) {
                results[i] = false;
            }
        }
        return results;
    }

    std::vector<bool> convert_files_parallel(
        const std::vector<std::filesystem::path>& input_paths,
        const std::vector<std::filesystem::path>& output_paths,
        int num_threads = 0) {
        if (input_paths.size() != output_paths.size()) {
            throw IconvError("Input and output file path counts do not match");
        }
        if (num_threads <= 0)
            num_threads = std::thread::hardware_concurrency();
        num_threads = std::max(1, num_threads);
        std::vector<bool> results(input_paths.size(), false);
        std::vector<std::future<bool>> futures;
        for (size_t i = 0; i < input_paths.size(); ++i) {
            futures.push_back(std::async(
                std::launch::async, [this, i, &input_paths, &output_paths]() {
                    try {
                        return m_converter.convert_file(input_paths[i],
                                                        output_paths[i]);
                    } catch (...) {
                        return false;
                    }
                }));
            if (futures.size() >= static_cast<size_t>(num_threads)) {
                for (size_t j = 0; j < futures.size(); ++j) {
                    results[i - futures.size() + j + 1] = futures[j].get();
                }
                futures.clear();
            }
        }
        for (size_t i = 0; i < futures.size(); ++i) {
            results[input_paths.size() - futures.size() + i] = futures[i].get();
        }
        return results;
    }

private:
    Converter m_converter;
};

}  // namespace iconv_cpp

#endif  // ICONV_CPP_HPP
