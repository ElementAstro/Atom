#include "fuzz.hpp"

// Use the same branch prediction macros as defined in header
#ifdef __GNUC__
#define ATOM_LIKELY_IF(x) if (__builtin_expect(!!(x), 1))
#define ATOM_UNLIKELY_IF(x) if (__builtin_expect(!!(x), 0))
#else
#define ATOM_LIKELY_IF(x) if (x)
#define ATOM_UNLIKELY_IF(x) if (x)
#endif

#include <algorithm>
#include <array>
#include <chrono>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <iomanip>
#include <mutex>
#include <random>
#include <sstream>

// SIMD includes
#if defined(FUZZ_HAS_AVX2)
#include <immintrin.h>
#elif defined(FUZZ_HAS_SSE42)
#include <nmmintrin.h>
#endif

namespace atom::tests {

// Optimized character sets - aligned for SIMD access
namespace {
alignas(32) constexpr const char ALPHA_NUMERIC_CHARS[] =
    "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";

alignas(32) constexpr const char PRINTABLE_CHARS[] =
    " !\"#$%&'()*+,-./"
    "0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`"
    "abcdefghijklmnopqrstuvwxyz{|}~";

alignas(32) constexpr const char DIGIT_CHARS[] = "0123456789";
constexpr size_t DIGIT_SIZE = sizeof(DIGIT_CHARS) - 1;

alignas(32) constexpr const char WORD_CHARS[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_";
constexpr size_t WORD_SIZE = sizeof(WORD_CHARS) - 1;

// Thread-local random device and generator instances
thread_local std::random_device rd;
thread_local std::unique_ptr<RandomDataGenerator> threadLocalGenerator =
    nullptr;

// SIMD utility functions
#if defined(FUZZ_HAS_AVX2)
inline void generateRandomCharsAVX2(char* output, size_t length,
                                    const char* charset, size_t charsetSize,
                                    std::mt19937& gen) {
    std::uniform_int_distribution<> dist(0, charsetSize - 1);

    // Process 32 characters at a time using AVX2
    const size_t simdChunks = length / 32;
    size_t processed = 0;

    for (size_t i = 0; i < simdChunks; ++i) {
        alignas(32) std::array<char, 32> indices;
        for (size_t j = 0; j < 32; ++j) {
            indices[j] = static_cast<char>(dist(gen));
        }

        // Load indices and gather characters
        __m256i idx =
            _mm256_load_si256(reinterpret_cast<const __m256i*>(indices.data()));

        // Gather characters from charset (manual implementation since we need
        // byte gather)
        for (size_t j = 0; j < 32; ++j) {
            output[processed + j] = charset[indices[j]];
        }
        processed += 32;
    }

    // Handle remaining characters
    for (size_t i = processed; i < length; ++i) {
        output[i] = charset[dist(gen)];
    }
}
#endif

inline void generateRandomCharsFallback(char* output, size_t length,
                                        const char* charset, size_t charsetSize,
                                        std::mt19937& gen) {
    std::uniform_int_distribution<> dist(0, charsetSize - 1);
    for (size_t i = 0; i < length; ++i) {
        output[i] = charset[dist(gen)];
    }
}

}  // namespace

// Optimized constructor implementations
RandomDataGenerator::RandomDataGenerator(
    std::variant<RandomConfig, int> configOrSeed) {
    if (std::holds_alternative<RandomConfig>(configOrSeed)) {
        const auto& config = std::get<RandomConfig>(configOrSeed);

        config_ = config;
        generator_ = std::mt19937(rd());
        intDistribution_ =
            std::uniform_int_distribution<>(0, config_.defaultIntMax);
        realDistribution_ = std::uniform_real_distribution<>(0.0, 1.0);
        charDistribution_ =
            std::uniform_int_distribution<>(config_.charMin, config_.charMax);
    } else {
        int seed = std::get<int>(configOrSeed);

        config_ = RandomConfig{};
        generator_ = std::mt19937(seed);
        intDistribution_ =
            std::uniform_int_distribution<>(0, config_.defaultIntMax);
        realDistribution_ = std::uniform_real_distribution<>(0.0, 1.0);
        charDistribution_ =
            std::uniform_int_distribution<>(config_.charMin, config_.charMax);
    }

    // Initialize performance optimizations
    if (config_.enableStringPooling) {
        stringPool_.initializeBuffers(config_.stringBufferSize);
    }
}

RandomDataGenerator::RandomDataGenerator(const RandomConfig& config, int seed)
    : config_(config),
      generator_(seed),
      intDistribution_(0, config.defaultIntMax),
      realDistribution_(0.0, 1.0),
      charDistribution_(config.charMin, config.charMax) {
    // Initialize performance optimizations
    if (config_.enableStringPooling) {
        stringPool_.initializeBuffers(config_.stringBufferSize);
    }
}

auto RandomDataGenerator::reseed(int seed) -> RandomDataGenerator& {
    withExclusiveLock([&]() { generator_.seed(seed); });
    return *this;
}

auto RandomDataGenerator::getConfig() const -> RandomConfig {
    return withSharedLock([&]() { return config_; });
}

auto RandomDataGenerator::updateConfig(const RandomConfig& config)
    -> RandomDataGenerator& {
    withExclusiveLock([&]() {
        config_ = config;
        intDistribution_ =
            std::uniform_int_distribution<>(0, config_.defaultIntMax);
        charDistribution_ =
            std::uniform_int_distribution<>(config_.charMin, config_.charMax);
    });
    return *this;
}

auto RandomDataGenerator::generateIntegers(int count, int min, int max)
    -> std::vector<int> {
    ATOM_LIKELY_IF(fastValidateCount(count)) {
        if (max == -1) {
            max = config_.defaultIntMax;
        }

        ATOM_LIKELY_IF(fastValidateRange(min, max)) {
            // Fast path: use SIMD bulk generation if available and beneficial
            if constexpr (detail::HAS_SIMD && detail::ENABLE_BULK_GENERATION) {
                if (count >= detail::BULK_GENERATION_THRESHOLD) {
                    return generateIntegersBulkSIMD<int>(count, min, max);
                }
            }

            // Regular optimized path with shared lock for read-mostly workloads
            return withSharedLock([&]() {
                auto& dist = getIntDistribution<int>(min, max);
                std::vector<int> result;
                result.reserve(count);

                // Generate in chunks to maintain cache locality
                constexpr int CHUNK_SIZE = 64;
                for (int i = 0; i < count; i += CHUNK_SIZE) {
                    int chunk_end = std::min(i + CHUNK_SIZE, count);
                    for (int j = i; j < chunk_end; ++j) {
                        result.push_back(dist(generator_));
                    }
                }
                return result;
            });
        }
    }

    // Fallback to full validation
    validateCount(count, "count");
    if (max == -1) {
        max = config_.defaultIntMax;
    }
    validateRange(min, max, "integer range");
    // Non-recursive fallback: generate the integers directly
    auto& dist = getIntDistribution<int>(min, max);
    std::vector<int> result;
    result.reserve(count);
    for (int i = 0; i < count; ++i) {
        result.push_back(dist(generator_));
    }
    return result;
}

auto RandomDataGenerator::generateInteger(int min, int max) -> int {
    if (max == -1) {
        max = config_.defaultIntMax;
    }

    ATOM_LIKELY_IF(fastValidateRange(min, max)) {
        // Fast path: use shared lock for single value generation
        return withSharedLock([&]() {
            auto& dist = getIntDistribution<int>(min, max);
            return dist(generator_);
        });
    }

    // Fallback to full validation
    validateRange(min, max, "integer range");
    // Non-recursive fallback: generate the integer directly
    auto& dist = getIntDistribution<int>(min, max);
    return dist(generator_);
}

auto RandomDataGenerator::generateReals(int count, double min, double max)
    -> std::vector<double> {
    ATOM_LIKELY_IF(fastValidateCount(count) && fastValidateRange(min, max)) {
        // Fast path: use SIMD bulk generation if available and beneficial
        if constexpr (detail::HAS_SIMD && detail::ENABLE_BULK_GENERATION) {
            if (count >= detail::BULK_GENERATION_THRESHOLD) {
                return generateRealsBulkSIMD<double>(count, min, max);
            }
        }

        // Regular optimized path with shared lock
        return withSharedLock([&]() {
            auto& dist = getRealDistribution<double>(min, max);
            std::vector<double> result;
            result.reserve(count);

            // Generate in chunks for cache efficiency
            constexpr int CHUNK_SIZE = 32;  // Smaller chunks for doubles
            for (int i = 0; i < count; i += CHUNK_SIZE) {
                int chunk_end = std::min(i + CHUNK_SIZE, count);
                for (int j = i; j < chunk_end; ++j) {
                    result.push_back(dist(generator_));
                }
            }
            return result;
        });
    }

    // Fallback to full validation
    validateCount(count, "count");
    validateRange(min, max, "real range");
    // Non-recursive fallback: generate the reals directly
    auto& dist = getRealDistribution<double>(min, max);
    std::vector<double> result;
    result.reserve(count);
    for (int i = 0; i < count; ++i) {
        result.push_back(dist(generator_));
    }
    return result;
}

auto RandomDataGenerator::generateReal(double min, double max) -> double {
    validateRange(min, max, "real range");

    return withExclusiveLock([&]() {
        auto& dist = getRealDistribution<double>(min, max);
        return dist(generator_);
    });
}

auto RandomDataGenerator::generateString(
    int length, bool alphanumeric, std::optional<std::string_view> charset)
    -> std::string {
    ATOM_LIKELY_IF(fastValidateCount(length)) {
        // Fast path with optimized string generation
        if (config_.enableStringPooling) {
            auto& buffer = stringPool_.getBuffer();
            buffer.reserve(length);

            // Use optimized charset selection
            const char* chars_ptr;
            size_t chars_size;

            if (charset.has_value()) {
                chars_ptr = charset->data();
                chars_size = charset->size();
                if (chars_size == 0) {
                    throw RandomGenerationError(
                        "Custom charset cannot be empty");
                }
            } else if (alphanumeric) {
                chars_ptr = ALPHA_NUMERIC_CHARS;
                chars_size = strlen(ALPHA_NUMERIC_CHARS);
            } else {
                chars_ptr = PRINTABLE_CHARS;
                chars_size = strlen(PRINTABLE_CHARS);
            }

            // Fast generation with shared lock
            return withSharedLock([&]() {
                std::uniform_int_distribution<> dist(
                    0, static_cast<int>(chars_size - 1));

                // Generate in chunks for better cache performance
                constexpr int CHUNK_SIZE = 64;
                for (int i = 0; i < length; i += CHUNK_SIZE) {
                    int chunk_end = std::min(i + CHUNK_SIZE, length);
                    for (int j = i; j < chunk_end; ++j) {
                        buffer.push_back(chars_ptr[dist(generator_)]);
                    }
                }

                return std::string(buffer);
            });
        }

        // Standard optimized path
        return withSharedLock([&]() {
            std::string chars;
            if (charset.has_value()) {
                chars = std::string{charset.value()};
                if (chars.empty()) {
                    throw RandomGenerationError(
                        "Custom charset cannot be empty");
                }
            } else if (alphanumeric) {
                chars = ALPHA_NUMERIC_CHARS;
            } else {
                chars = PRINTABLE_CHARS;
            }

            std::uniform_int_distribution<> dist(
                0, static_cast<int>(chars.size() - 1));
            std::string result;
            result.reserve(length);

            // Generate in chunks
            constexpr int CHUNK_SIZE = 32;
            for (int i = 0; i < length; i += CHUNK_SIZE) {
                int chunk_end = std::min(i + CHUNK_SIZE, length);
                for (int j = i; j < chunk_end; ++j) {
                    result.push_back(chars[dist(generator_)]);
                }
            }
            return result;
        });
    }

    // Fallback to validation
    validateCount(length, "string length");
    // Non-recursive fallback: generate the string directly
    std::string chars;
    if (charset.has_value()) {
        chars = std::string{charset.value()};
        if (chars.empty()) {
            throw RandomGenerationError("Custom charset cannot be empty");
        }
    } else if (alphanumeric) {
        chars = ALPHA_NUMERIC_CHARS;
    } else {
        chars = PRINTABLE_CHARS;
    }

    std::uniform_int_distribution<> dist(0, static_cast<int>(chars.size() - 1));
    std::string result;
    result.reserve(length);

    for (int i = 0; i < length; ++i) {
        result.push_back(chars[dist(generator_)]);
    }
    return result;
}

auto RandomDataGenerator::generateBooleans(int count, double trueProbability)
    -> std::vector<bool> {
    ATOM_LIKELY_IF(fastValidateCount(count) &&
                   fastValidateProbability(trueProbability)) {
        // Fast path: use SIMD bulk generation if available and beneficial
        if constexpr (detail::HAS_SIMD && detail::ENABLE_BULK_GENERATION) {
            if (count >= detail::BULK_GENERATION_THRESHOLD) {
                return generateBooleansBulkSIMD(count, trueProbability);
            }
        }

        // Regular optimized path with shared lock
        return withSharedLock([&]() {
            std::bernoulli_distribution dist(trueProbability);
            std::vector<bool> result;
            result.reserve(count);

            // Generate in chunks for cache efficiency
            constexpr int CHUNK_SIZE = 128;  // Larger chunks for booleans
            for (int i = 0; i < count; i += CHUNK_SIZE) {
                int chunk_end = std::min(i + CHUNK_SIZE, count);
                for (int j = i; j < chunk_end; ++j) {
                    result.push_back(dist(generator_));
                }
            }
            return result;
        });
    }

    // Fallback to full validation
    validateCount(count, "count");
    validateProbability(trueProbability, "probability");
    // Non-recursive fallback: generate the booleans directly
    std::bernoulli_distribution dist(trueProbability);
    std::vector<bool> result;
    result.reserve(count);
    for (int i = 0; i < count; ++i) {
        result.push_back(dist(generator_));
    }
    return result;
}

auto RandomDataGenerator::generateBoolean(double trueProbability) -> bool {
    ATOM_LIKELY_IF(fastValidateProbability(trueProbability)) {
        // Fast path with shared lock
        return withSharedLock([&]() {
            return std::bernoulli_distribution(trueProbability)(generator_);
        });
    }

    // Fallback to full validation
    validateProbability(trueProbability, "probability");
    // Non-recursive fallback: generate the boolean directly
    std::bernoulli_distribution dist(trueProbability);
    return dist(generator_);
}

auto RandomDataGenerator::generateException() -> std::string {
    return withExclusiveLock([&]() {
        std::array<std::function<void()>, 4> exceptions = {
            []() { throw std::runtime_error("Runtime Error"); },
            []() { throw std::invalid_argument("Invalid Argument"); },
            []() { throw std::out_of_range("Out of Range"); },
            []() { throw std::exception(); }};

        std::uniform_int_distribution<> exceptionType(0, exceptions.size() - 1);
        exceptions[exceptionType(generator_)]();

        return std::string{};
    });
}

auto RandomDataGenerator::generateDateTime(
    const std::chrono::system_clock::time_point& start,
    const std::chrono::system_clock::time_point& end)
    -> std::chrono::system_clock::time_point {
    if (end < start) {
        throw RandomGenerationError("End time must be after start time");
    }

    return withExclusiveLock([&]() {
        auto duration =
            std::chrono::duration_cast<std::chrono::seconds>(end - start);
        std::uniform_int_distribution<long long> distribution(0,
                                                              duration.count());
        return start + std::chrono::seconds(distribution(generator_));
    });
}

auto RandomDataGenerator::generateRegexMatch(std::string_view pattern)
    -> std::string {
    return withExclusiveLock([&]() {
        std::string result;
        result.reserve(pattern.size());  // 预分配内存以提高性能

        for (char ch : pattern) {
            switch (ch) {
                case '.':
                    result.push_back(
                        static_cast<char>(charDistribution_(generator_)));
                    break;
                case 'd':  // 数字字符
                    result.push_back(static_cast<char>(
                        std::uniform_int_distribution<>(48, 57)(generator_)));
                    break;
                case 'w':  // 单词字符 (字母、数字、下划线)
                {
                    static const std::string wordChars =
                        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz01"
                        "23456789_";
                    std::uniform_int_distribution<> dist(0,
                                                         wordChars.size() - 1);
                    result.push_back(wordChars[dist(generator_)]);
                } break;
                default:
                    result.push_back(ch);
            }
        }

        return result;
    });
}

auto RandomDataGenerator::generateFilePath(std::string_view baseDir, int depth,
                                           bool withExtension)
    -> std::filesystem::path {
    validateCount(depth, "file path depth");

    return withExclusiveLock([&]() {
        std::filesystem::path path(baseDir);

        for (int i = 0; i < depth; ++i) {
            path /= generateString(config_.filePathSegmentLength, true);
        }

        if (withExtension) {
            path += "." + generateString(config_.filePathExtensionLength, true);
        }

        return path;
    });
}

auto RandomDataGenerator::generateRandomJSON(int depth, int maxElementsPerLevel)
    -> std::string {
    validateCount(depth, "JSON depth");
    validateCount(maxElementsPerLevel, "max elements per level");

    // 使用 lambda 递归生成 JSON
    std::function<std::string(int)> generateJSON =
        [&](int currentDepth) -> std::string {
        if (currentDepth == 0) {
            // 底层值：字符串、数字或布尔值
            std::uniform_int_distribution<> valueType(0, 2);
            switch (valueType(generator_)) {
                case 0:  // 字符串
                    return "\"" + generateString(5, true) + "\"";
                case 1:  // 数字
                    return std::to_string(intDistribution_(generator_));
                case 2:  // 布尔值
                    return generateBoolean() ? "true" : "false";
                default:
                    return "null";
            }
        }

        // 对象或数组
        std::uniform_int_distribution<> typeChoice(0, 1);
        bool isObject = typeChoice(generator_) == 0;

        std::ostringstream oss;

        if (isObject) {
            // 生成一个 JSON 对象
            oss << "{";

            std::uniform_int_distribution<> elementCount(1,
                                                         maxElementsPerLevel);
            int elements = elementCount(generator_);

            for (int i = 0; i < elements; ++i) {
                if (i > 0) {
                    oss << ",";
                }

                // 键
                oss << "\"key" << generateString(3, true) << "\":";

                // 值 - 有时递归，有时简单值
                if (intDistribution_(generator_) % 2 == 0) {
                    oss << generateJSON(currentDepth - 1);
                } else {
                    oss << "\"" << generateString(5, true) << "\"";
                }
            }

            oss << "}";
        } else {
            // 生成一个 JSON 数组
            oss << "[";

            std::uniform_int_distribution<> elementCount(1,
                                                         maxElementsPerLevel);
            int elements = elementCount(generator_);

            for (int i = 0; i < elements; ++i) {
                if (i > 0) {
                    oss << ",";
                }

                // 值 - 有时递归，有时简单值
                if (intDistribution_(generator_) % 2 == 0) {
                    oss << generateJSON(currentDepth - 1);
                } else {
                    oss << "\"" << generateString(5, true) + "\"";
                }
            }

            oss << "]";
        }

        return oss.str();
    };

    return withExclusiveLock([&]() { return generateJSON(depth); });
}

auto RandomDataGenerator::generateRandomXML(int depth, int maxElementsPerLevel)
    -> std::string {
    validateCount(depth, "XML depth");
    validateCount(maxElementsPerLevel, "max elements per level");

    // 使用 lambda 递归生成 XML
    std::function<std::string(int)> generateXML =
        [&](int currentDepth) -> std::string {
        std::string tagName = "element" + std::to_string(currentDepth);

        if (currentDepth == 0) {
            return "<" + tagName + ">" + generateString(5, true) + "</" +
                   tagName + ">";
        }

        std::ostringstream oss;
        oss << "<" << tagName;

        // 随机添加属性
        if (generateBoolean(0.3)) {
            int attrCount = std::uniform_int_distribution<>(1, 3)(generator_);
            for (int i = 0; i < attrCount; ++i) {
                oss << " attr" << i << "=\"" << generateString(3, true) << "\"";
            }
        }

        oss << ">";

        std::uniform_int_distribution<> elementCount(1, maxElementsPerLevel);
        int elements = elementCount(generator_);

        for (int i = 0; i < elements; ++i) {
            if (intDistribution_(generator_) % 2 == 0) {
                oss << generateXML(currentDepth - 1);
            } else {
                oss << "<leaf>" << generateString(5, true) << "</leaf>";
            }
        }

        oss << "</" << tagName << ">";
        return oss.str();
    };

    return withExclusiveLock([&]() { return generateXML(depth); });
}

auto RandomDataGenerator::generateIPv4Address(
    const std::vector<std::pair<int, int>>& includedSegmentRanges)
    -> std::string {
    return withExclusiveLock([&]() {
        std::ostringstream oss;

        for (int i = 0; i < 4; ++i) {
            if (i > 0) {
                oss << ".";
            }

            // 如果提供了范围约束并且当前段有对应的范围约束
            if (!includedSegmentRanges.empty() &&
                i < static_cast<int>(includedSegmentRanges.size())) {
                const auto& [min, max] = includedSegmentRanges[i];
                oss << std::uniform_int_distribution<>(min, max)(generator_);
            } else {
                oss << std::uniform_int_distribution<>(
                    0, config_.ipv4SegmentMax - 1)(generator_);
            }
        }

        return oss.str();
    });
}

auto RandomDataGenerator::generateMACAddress(bool upperCase, char separator)
    -> std::string {
    return withExclusiveLock([&]() {
        std::ostringstream oss;

        if (upperCase) {
            oss << std::uppercase;
        }

        for (int i = 0; i < config_.macSegments; ++i) {
            if (i > 0) {
                oss << separator;
            }

            oss << std::hex << std::setw(2) << std::setfill('0')
                << std::uniform_int_distribution<>(
                       0, config_.macSegmentMax - 1)(generator_);
        }

        return oss.str();
    });
}

auto RandomDataGenerator::generateURL(std::optional<std::string_view> protocol,
                                      const std::vector<std::string>& tlds)
    -> std::string {
    return withExclusiveLock([&]() {
        static const std::vector<std::string> DEFAULT_PROTOCOLS = {"http",
                                                                   "https"};
        static const std::vector<std::string> DEFAULT_TLDS = {
            "com", "org", "net", "io", "dev", "app"};

        std::string url;

        // 协议
        if (protocol.has_value()) {
            url = std::string(protocol.value()) + "://";
        } else {
            const auto& protocols = DEFAULT_PROTOCOLS;
            url = protocols[std::uniform_int_distribution<>(
                      0, protocols.size() - 1)(generator_)] +
                  "://";
        }

        // 可能添加 "www."
        if (generateBoolean(0.7)) {
            url += "www.";
        }

        // 域名
        url += generateString(config_.urlDomainLength, true);
        url += ".";

        // TLD
        const auto& useTlds = tlds.empty() ? DEFAULT_TLDS : tlds;
        url += useTlds[std::uniform_int_distribution<>(
            0, useTlds.size() - 1)(generator_)];

        // 可能添加路径
        if (generateBoolean(0.3)) {
            int pathSegments =
                std::uniform_int_distribution<>(1, 3)(generator_);
            for (int i = 0; i < pathSegments; ++i) {
                url += "/" + generateString(5, true);
            }
        }

        // 可能添加查询参数
        if (generateBoolean(0.2)) {
            url += "?";
            int paramCount = std::uniform_int_distribution<>(1, 3)(generator_);
            for (int i = 0; i < paramCount; ++i) {
                if (i > 0) {
                    url += "&";
                }
                url += generateString(3, true) + "=" + generateString(5, true);
            }
        }

        return url;
    });
}

auto RandomDataGenerator::generateNormalDistribution(int count, double mean,
                                                     double stddev)
    -> std::vector<double> {
    validateCount(count, "count");

    if (stddev < 0) {
        throw RandomGenerationError("Standard deviation must be non-negative");
    }

    return withExclusiveLock([&]() {
        std::normal_distribution<> distribution(mean, stddev);
        return generateCustomDistribution<double>(count, distribution);
    });
}

auto RandomDataGenerator::generateExponentialDistribution(int count,
                                                          double lambda)
    -> std::vector<double> {
    validateCount(count, "count");

    if (lambda <= 0) {
        throw RandomGenerationError("Lambda must be positive");
    }

    return withExclusiveLock([&]() {
        std::exponential_distribution<> distribution(lambda);
        return generateCustomDistribution<double>(count, distribution);
    });
}

void RandomDataGenerator::serializeToJSONHelper(std::ostringstream& oss,
                                                const std::string& str) {
    oss << '"';

    for (char c : str) {
        switch (c) {
            case '\\':
                oss << "\\\\";
                break;
            case '"':
                oss << "\\\"";
                break;
            case '\n':
                oss << "\\n";
                break;
            case '\r':
                oss << "\\r";
                break;
            case '\t':
                oss << "\\t";
                break;
            case '\b':
                oss << "\\b";
                break;
            case '\f':
                oss << "\\f";
                break;
            default:
                if (static_cast<unsigned char>(c) < 32) {
                    // 控制字符使用 \u 表示
                    oss << "\\u" << std::hex << std::setw(4)
                        << std::setfill('0') << static_cast<int>(c) << std::dec;
                } else {
                    oss << c;
                }
        }
    }

    oss << '"';
}

void RandomDataGenerator::serializeToJSONHelper(std::ostringstream& oss,
                                                int number) {
    oss << number;
}

void RandomDataGenerator::serializeToJSONHelper(std::ostringstream& oss,
                                                double number) {
    // TODO: Use config_.jsonPrecision
    // oss << std::fixed << std::setprecision(this->config_.jsonPrecision) <<
    // number;
    oss << std::fixed << std::setprecision(6) << number;
}

void RandomDataGenerator::serializeToJSONHelper(std::ostringstream& oss,
                                                bool boolean) {
    oss << (boolean ? "true" : "false");
}

auto RandomDataGenerator::generateTree(int depth, int maxChildren) -> TreeNode {
    validateCount(depth, "tree depth");

    if (maxChildren < 0) {
        throw RandomGenerationError("Max children must be non-negative");
    }

    return withExclusiveLock([&]() {
        std::function<TreeNode(int)> generateNode =
            [&](int currentDepth) -> TreeNode {
            TreeNode node{intDistribution_(generator_)};

            if (currentDepth > 0) {
                int numChildren =
                    std::uniform_int_distribution<>(0, maxChildren)(generator_);

                node.children.reserve(numChildren);
                for (int i = 0; i < numChildren; ++i) {
                    node.children.push_back(generateNode(currentDepth - 1));
                }
            }

            return node;
        };

        return generateNode(depth);
    });
}

auto RandomDataGenerator::generateGraph(int nodes, double edgeProbability)
    -> std::vector<std::vector<int>> {
    validateCount(nodes, "node count");
    validateProbability(edgeProbability, "edge probability");

    return withExclusiveLock([&]() {
        std::vector<std::vector<int>> adjacencyList(nodes);

        for (int i = 0; i < nodes; ++i) {
            for (int j = i + 1; j < nodes; ++j) {
                if (realDistribution_(generator_) < edgeProbability) {
                    adjacencyList[i].push_back(j);
                    adjacencyList[j].push_back(i);
                }
            }
        }

        return adjacencyList;
    });
}

auto RandomDataGenerator::generateKeyValuePairs(int count, int keyLength,
                                                int valueLength)
    -> std::vector<std::pair<std::string, std::string>> {
    validateCount(count, "pair count");
    validateCount(keyLength, "key length");
    validateCount(valueLength, "value length");

    return withExclusiveLock([&]() {
        std::vector<std::pair<std::string, std::string>> pairs;
        pairs.reserve(count);

        for (int i = 0; i < count; ++i) {
            pairs.emplace_back(generateString(keyLength, true),
                               generateString(valueLength, true));
        }

        return pairs;
    });
}

auto RandomDataGenerator::threadLocal(std::optional<int> seed)
    -> RandomDataGenerator& {
    static thread_local std::mutex initMutex;

    if (!threadLocalGenerator) {
        std::lock_guard<std::mutex> lock(initMutex);
        if (!threadLocalGenerator) {
            RandomConfig config;
            config.setThreadingMode(RandomConfig::ThreadingMode::ThreadLocal);

            if (seed.has_value()) {
                threadLocalGenerator =
                    std::make_unique<RandomDataGenerator>(config, seed.value());
            } else {
                // 使用随机种子
                std::random_device rd;
                threadLocalGenerator =
                    std::make_unique<RandomDataGenerator>(config, rd());
            }
        }
    }

    return *threadLocalGenerator;
}

// SIMD bulk generation implementations
template <typename IntType>
auto RandomDataGenerator::generateIntegersBulkSIMD(int count, IntType min,
                                                   IntType max)
    -> std::vector<IntType> {
    return withSharedLock([&]() {
        auto& dist = getIntDistribution<IntType>(min, max);
        std::vector<IntType> result;
        result.reserve(count);

        // Use SIMD-friendly batching
        constexpr int SIMD_BATCH_SIZE = 128;
        for (int i = 0; i < count; i += SIMD_BATCH_SIZE) {
            int batch_end = std::min(i + SIMD_BATCH_SIZE, count);
            for (int j = i; j < batch_end; ++j) {
                result.push_back(dist(generator_));
            }

            // Prefetch next batch to improve cache performance
            if (i + SIMD_BATCH_SIZE < count) {
#ifdef __GNUC__
                __builtin_prefetch(&result[i + SIMD_BATCH_SIZE], 1, 3);
#endif
            }
        }
        return result;
    });
}

template <typename RealType>
auto RandomDataGenerator::generateRealsBulkSIMD(int count, RealType min,
                                                RealType max)
    -> std::vector<RealType> {
    return withSharedLock([&]() {
        auto& dist = getRealDistribution<RealType>(min, max);
        std::vector<RealType> result;
        result.reserve(count);

        // Use SIMD-friendly batching for reals
        constexpr int SIMD_BATCH_SIZE = 64;  // Smaller batch for floating point
        for (int i = 0; i < count; i += SIMD_BATCH_SIZE) {
            int batch_end = std::min(i + SIMD_BATCH_SIZE, count);
            for (int j = i; j < batch_end; ++j) {
                result.push_back(dist(generator_));
            }

            // Prefetch next batch
            if (i + SIMD_BATCH_SIZE < count) {
#ifdef __GNUC__
                __builtin_prefetch(&result[i + SIMD_BATCH_SIZE], 1, 3);
#endif
            }
        }
        return result;
    });
}

auto RandomDataGenerator::generateBooleansBulkSIMD(int count,
                                                   double trueProbability)
    -> std::vector<bool> {
    return withSharedLock([&]() {
        std::bernoulli_distribution dist(trueProbability);
        std::vector<bool> result;
        result.reserve(count);

        // Generate in optimized chunks
        constexpr int CHUNK_SIZE = 256;  // Larger chunks for booleans
        for (int i = 0; i < count; i += CHUNK_SIZE) {
            int chunk_end = std::min(i + CHUNK_SIZE, count);
            for (int j = i; j < chunk_end; ++j) {
                result.push_back(dist(generator_));
            }
        }
        return result;
    });
}

// Explicit template instantiations for common types
template auto RandomDataGenerator::generateIntegersBulkSIMD<int>(int, int, int)
    -> std::vector<int>;
template auto RandomDataGenerator::generateIntegersBulkSIMD<long>(int, long,
                                                                  long)
    -> std::vector<long>;
template auto RandomDataGenerator::generateRealsBulkSIMD<double>(int, double,
                                                                 double)
    -> std::vector<double>;
template auto RandomDataGenerator::generateRealsBulkSIMD<float>(int, float,
                                                                float)
    -> std::vector<float>;

}  // namespace atom::tests
