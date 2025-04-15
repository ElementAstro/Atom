#include "fuzz.hpp"

#include <chrono>
#include <ctime>
#include <filesystem>
#include <format>
#include <iomanip>
#include <mutex>
#include <random>
#include <ranges>
#include <sstream>

namespace atom::tests {

// 常量定义
namespace {
constexpr const char* ALPHA_NUMERIC_CHARS =
    "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
constexpr const char* PRINTABLE_CHARS =
    " !\"#$%&'()*+,-./"
    "0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`"
    "abcdefghijklmnopqrstuvwxyz{|}~";

thread_local std::random_device rd;
thread_local RandomDataGenerator* threadLocalGenerator = nullptr;
}  // namespace

void RandomDataGenerator::validateCount(int count,
                                        const std::string& paramName) const {
    if (count < 0) {
        throw RandomGenerationError(std::format(
            "Invalid {} value: {} (must be non-negative)", paramName, count));
    }
}

void RandomDataGenerator::validateProbability(
    double probability, const std::string& paramName) const {
    if (probability < 0.0 || probability > 1.0) {
        throw RandomGenerationError(
            std::format("Invalid {} value: {} (must be between 0.0 and 1.0)",
                        paramName, probability));
    }
}

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
}

RandomDataGenerator::RandomDataGenerator(const RandomConfig& config, int seed)
    : config_(config),
      generator_(seed),
      intDistribution_(0, config.defaultIntMax),
      realDistribution_(0.0, 1.0),
      charDistribution_(config.charMin, config.charMax) {}

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
    validateCount(count, "count");

    if (max == -1) {
        max = config_.defaultIntMax;
    }

    return withExclusiveLock([&]() {
        auto& dist = getIntDistribution<int>(min, max);

        // 使用 std::ranges 进行更简洁的实现(C++20)
        return std::views::iota(0, count) |
               std::views::transform([&](auto) { return dist(generator_); }) |
               std::ranges::to<std::vector>();
    });
}

auto RandomDataGenerator::generateInteger(int min, int max) -> int {
    if (max == -1) {
        max = config_.defaultIntMax;
    }

    return withExclusiveLock([&]() {
        auto& dist = getIntDistribution<int>(min, max);
        return dist(generator_);
    });
}

auto RandomDataGenerator::generateReals(int count, double min, double max)
    -> std::vector<double> {
    validateCount(count, "count");
    validateRange(min, max, "real range");

    return withExclusiveLock([&]() {
        auto& dist = getRealDistribution<double>(min, max);

        // 使用 std::ranges 进行更简洁的实现(C++20)
        return std::views::iota(0, count) |
               std::views::transform([&](auto) { return dist(generator_); }) |
               std::ranges::to<std::vector>();
    });
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
    validateCount(length, "string length");

    return withExclusiveLock([&]() {
        std::string chars;

        if (charset.has_value()) {
            chars = std::string{charset.value()};
            if (chars.empty()) {
                throw RandomGenerationError("Custom charset cannot be empty");
            }
        } else if (alphanumeric) {
            chars = ALPHA_NUMERIC_CHARS;
        } else {
            // 使用可打印字符
            chars = PRINTABLE_CHARS;
        }

        std::uniform_int_distribution<> dist(
            0, static_cast<int>(chars.size() - 1));

        // 使用 std::ranges 进行更简洁的实现(C++20)
        return std::views::iota(0, length) | std::views::transform([&](auto) {
                   return chars[dist(generator_)];
               }) |
               std::ranges::to<std::string>();
    });
}

auto RandomDataGenerator::generateBooleans(int count, double trueProbability)
    -> std::vector<bool> {
    validateCount(count, "count");
    validateProbability(trueProbability, "probability");

    return withExclusiveLock([&]() {
        std::bernoulli_distribution dist(trueProbability);

        // 使用 std::ranges 进行更简洁的实现(C++20)
        return std::views::iota(0, count) |
               std::views::transform([&](auto) { return dist(generator_); }) |
               std::ranges::to<std::vector<bool>>();
    });
}

auto RandomDataGenerator::generateBoolean(double trueProbability) -> bool {
    validateProbability(trueProbability, "probability");

    return withExclusiveLock([&]() {
        return std::bernoulli_distribution(trueProbability)(generator_);
    });
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
                    oss << "\"" << generateString(5, true) << "\"";
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
            config.enableThreadSafety(false);  // 线程局部不需要线程安全

            if (seed.has_value()) {
                threadLocalGenerator =
                    new RandomDataGenerator(config, seed.value());
            } else {
                // 使用随机种子
                std::random_device rd;
                threadLocalGenerator = new RandomDataGenerator(config, rd());
            }
        }
    }

    return *threadLocalGenerator;
}

}  // namespace atom::tests
