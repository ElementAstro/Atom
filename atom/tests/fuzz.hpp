#ifndef ATOM_TESTS_FUZZ_HPP
#define ATOM_TESTS_FUZZ_HPP

#include <algorithm>
#include <chrono>
#include <concepts>
#include <filesystem>
#include <format>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <random>
#include <set>
#include <shared_mutex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <variant>
#include <vector>

#undef CHAR_MIN
#undef CHAR_MAX

namespace atom::tests {

/**
 * @brief Exception thrown when random data generation fails due to invalid
 * parameters
 */
class RandomGenerationError : public std::runtime_error {
public:
    explicit RandomGenerationError(const std::string& message)
        : std::runtime_error(message) {}
};

/**
 * @brief A configuration class for the RandomDataGenerator
 */
struct RandomConfig {
    int defaultIntMax = 100;        ///< Default maximum integer value
    int charMin = 32;               ///< Minimum ASCII character value
    int charMax = 126;              ///< Maximum ASCII character value
    int ipv4SegmentMax = 256;       ///< Maximum value for IPv4 address segments
    int macSegments = 6;            ///< Number of segments in MAC address
    int macSegmentMax = 256;        ///< Maximum value for MAC address segments
    int urlDomainLength = 8;        ///< Default URL domain length
    int filePathSegmentLength = 5;  ///< Default file path segment length
    int filePathExtensionLength = 3;  ///< Default file path extension length
    int jsonPrecision = 6;    ///< Precision for JSON floating point numbers
    bool threadSafe = false;  ///< Thread safety flag

    // Builder pattern for fluent interface
    RandomConfig& setDefaultIntMax(int value) {
        if (value <= 0)
            throw RandomGenerationError("Default int max must be positive");
        defaultIntMax = value;
        return *this;
    }
    RandomConfig& setCharRange(int min, int max) {
        if (min >= max)
            throw RandomGenerationError("Char min must be less than char max");
        charMin = min;
        charMax = max;
        return *this;
    }
    RandomConfig& setIPv4SegmentMax(int value) {
        if (value <= 0 || value > 256)
            throw RandomGenerationError(
                "IPv4 segment max must be between 1 and 256");
        ipv4SegmentMax = value;
        return *this;
    }
    RandomConfig& setMacConfig(int segments, int segmentMax) {
        if (segments <= 0)
            throw RandomGenerationError("MAC segments must be positive");
        if (segmentMax <= 0)
            throw RandomGenerationError("MAC segment max must be positive");
        macSegments = segments;
        macSegmentMax = segmentMax;
        return *this;
    }
    RandomConfig& setUrlDomainLength(int value) {
        if (value <= 0)
            throw RandomGenerationError("URL domain length must be positive");
        urlDomainLength = value;
        return *this;
    }
    RandomConfig& setFilePathConfig(int segmentLength, int extensionLength) {
        if (segmentLength <= 0)
            throw RandomGenerationError(
                "File path segment length must be positive");
        if (extensionLength < 0)
            throw RandomGenerationError(
                "File path extension length can't be negative");
        filePathSegmentLength = segmentLength;
        filePathExtensionLength = extensionLength;
        return *this;
    }
    RandomConfig& setJsonPrecision(int value) {
        if (value < 0)
            throw RandomGenerationError("JSON precision can't be negative");
        jsonPrecision = value;
        return *this;
    }
    RandomConfig& enableThreadSafety(bool value = true) {
        threadSafe = value;
        return *this;
    }
};

/**
 * @concept Serializable
 * @brief Concept for types that can be serialized to JSON
 */
template <typename T>
concept Serializable = requires(std::ostringstream& oss, const T& value) {
    { serializeToJSON(oss, value) } -> std::same_as<void>;
};

/**
 * @class RandomDataGenerator
 * @brief A modern, thread-safe class for generating random data for testing
 * purposes.
 *
 * This class provides a wide range of methods for generating random data
 * including primitive types, strings, collections, and complex data structures.
 * It's designed to be used in testing, fuzzing, and simulation scenarios.
 *
 * Thread safety is optional and can be enabled via configuration.
 *
 * Usage example:
 * @code
 * // Create a generator with default configuration
 * RandomDataGenerator generator;
 *
 * // Generate 10 random integers between 1 and 100
 * auto integers = generator.generateIntegers(10, 1, 100);
 *
 * // Generate a random alphanumeric string of length 16
 * auto str = generator.generateString(16, true);
 *
 * // Create a generator with custom configuration
 * RandomConfig config;
 * config.setDefaultIntMax(1000).setCharRange(0, 255).enableThreadSafety();
 * RandomDataGenerator customGenerator(config);
 * @endcode
 */
class RandomDataGenerator {
private:
    // Configuration
    RandomConfig config_;

    // Random number generation
    std::mt19937 generator_;
    std::uniform_int_distribution<> intDistribution_;
    std::uniform_real_distribution<> realDistribution_;
    std::uniform_int_distribution<> charDistribution_;

    // Thread safety
    mutable std::shared_mutex mutex_;

    // Caches for frequently used distributions
    std::unordered_map<std::string,
                       std::unique_ptr<std::uniform_int_distribution<>>>
        intDistCache_;
    std::unordered_map<std::string,
                       std::unique_ptr<std::uniform_real_distribution<>>>
        realDistCache_;

    // Distribution factory methods
    template <typename T>
    auto getIntDistribution(int min, int max)
        -> std::uniform_int_distribution<T>&;

    template <typename T>
    auto getRealDistribution(T min, T max)
        -> std::uniform_real_distribution<T>&;

    // Thread safety helpers
    template <typename Func>
    auto withSharedLock(Func&& func) const;

    template <typename Func>
    auto withExclusiveLock(Func&& func);

public:
    /**
     * @struct TreeNode
     * @brief A structure representing a node in a tree.
     */
    struct TreeNode {
        int value;                            ///< The value of the node.
        std::vector<TreeNode> children = {};  ///< The children of the node.

        // Comparison operators for testing
        bool operator==(const TreeNode& other) const {
            return value == other.value && children == other.children;
        }

        bool operator!=(const TreeNode& other) const {
            return !(*this == other);
        }
    };

    /**
     * @brief Constructs a RandomDataGenerator with optional configuration and
     * seed.
     * @param configOrSeed Configuration object or seed value
     *
     * @throws RandomGenerationError if configuration validation fails
     */
    explicit RandomDataGenerator(
        std::variant<RandomConfig, int> configOrSeed = RandomConfig{});

    /**
     * @brief Constructs a RandomDataGenerator with a specific configuration and
     * seed.
     * @param config Configuration object
     * @param seed Seed value for the random number generator
     *
     * @throws RandomGenerationError if configuration validation fails
     */
    RandomDataGenerator(const RandomConfig& config, int seed);

    /**
     * @brief Reset the generator with a new seed
     * @param seed New seed value
     * @return Reference to this generator for method chaining
     */
    auto reseed(int seed) -> RandomDataGenerator&;

    /**
     * @brief Get the current configuration
     * @return Copy of the current configuration
     */
    auto getConfig() const -> RandomConfig;

    /**
     * @brief Update the configuration
     * @param config New configuration
     * @return Reference to this generator for method chaining
     *
     * @throws RandomGenerationError if configuration validation fails
     */
    auto updateConfig(const RandomConfig& config) -> RandomDataGenerator&;

    /**
     * @brief Generates a vector of random integers.
     * @param count The number of integers to generate.
     * @param min The minimum value of the integers.
     * @param max The maximum value of the integers.
     * @return A vector of random integers.
     *
     * @throws RandomGenerationError if count is negative or min > max
     */
    auto generateIntegers(int count, int min = 0, int max = -1)
        -> std::vector<int>;

    /**
     * @brief Generates a single random integer.
     * @param min The minimum value of the integer.
     * @param max The maximum value of the integer.
     * @return A random integer.
     *
     * @throws RandomGenerationError if min > max
     */
    auto generateInteger(int min = 0, int max = -1) -> int;

    /**
     * @brief Generates a vector of random real numbers.
     * @param count The number of real numbers to generate.
     * @param min The minimum value of the real numbers.
     * @param max The maximum value of the real numbers.
     * @return A vector of random real numbers.
     *
     * @throws RandomGenerationError if count is negative or min > max
     */
    auto generateReals(int count, double min = 0.0, double max = 1.0)
        -> std::vector<double>;

    /**
     * @brief Generates a single random real number.
     * @param min The minimum value of the real number.
     * @param max The maximum value of the real number.
     * @return A random real number.
     *
     * @throws RandomGenerationError if min > max
     */
    auto generateReal(double min = 0.0, double max = 1.0) -> double;

    /**
     * @brief Generates a random string.
     * @param length The length of the string.
     * @param alphanumeric If true, the string will contain only alphanumeric
     * characters.
     * @param charset Optional custom character set to use
     * @return A random string.
     *
     * @throws RandomGenerationError if length is negative or charset is empty
     */
    auto generateString(int length, bool alphanumeric = false,
                        std::optional<std::string_view> charset = std::nullopt)
        -> std::string;

    /**
     * @brief Generates a vector of random boolean values.
     * @param count The number of boolean values to generate.
     * @param trueProbability The probability of generating true (default: 0.5)
     * @return A vector of random boolean values.
     *
     * @throws RandomGenerationError if count is negative or probability is out
     * of range
     */
    auto generateBooleans(int count, double trueProbability = 0.5)
        -> std::vector<bool>;

    /**
     * @brief Generates a single random boolean.
     * @param trueProbability The probability of generating true (default: 0.5)
     * @return A random boolean.
     *
     * @throws RandomGenerationError if probability is out of range
     */
    auto generateBoolean(double trueProbability = 0.5) -> bool;

    /**
     * @brief Generates and throws a random exception.
     * @return Never returns, always throws an exception.
     *
     * @throws std::exception or a derived class
     */
    auto generateException() -> std::string;

    /**
     * @brief Generates a random date and time within a specified range.
     * @param start The start of the date and time range.
     * @param end The end of the date and time range.
     * @return A random date and time within the specified range.
     *
     * @throws RandomGenerationError if end is before start
     */
    auto generateDateTime(const std::chrono::system_clock::time_point& start,
                          const std::chrono::system_clock::time_point& end)
        -> std::chrono::system_clock::time_point;

    /**
     * @brief Generates a string that matches a given regular expression-like
     * pattern. Currently supports: '.', 'd' (digit), 'w' (word character).
     * @param pattern The pattern to match.
     * @return A string that matches the pattern.
     */
    auto generateRegexMatch(std::string_view pattern) -> std::string;

    /**
     * @brief Generates a random file path.
     * @param baseDir The base directory for the file path.
     * @param depth The depth of the file path (number of subdirectories).
     * @param withExtension Whether to include a file extension.
     * @return A random file path.
     *
     * @throws RandomGenerationError if depth is negative
     */
    auto generateFilePath(std::string_view baseDir, int depth = 3,
                          bool withExtension = true) -> std::filesystem::path;

    /**
     * @brief Generates a random JSON string.
     * @param depth The depth of the JSON structure.
     * @param maxElementsPerLevel Maximum number of elements per nesting level.
     * @return A random JSON string.
     *
     * @throws RandomGenerationError if depth or maxElementsPerLevel is negative
     */
    auto generateRandomJSON(int depth = 2, int maxElementsPerLevel = 4)
        -> std::string;

    /**
     * @brief Generates a random XML string.
     * @param depth The depth of the XML structure.
     * @param maxElementsPerLevel Maximum number of elements per nesting level.
     * @return A random XML string.
     *
     * @throws RandomGenerationError if depth or maxElementsPerLevel is negative
     */
    auto generateRandomXML(int depth = 2, int maxElementsPerLevel = 3)
        -> std::string;

    /**
     * @brief Performs a fuzz test on a given function with random inputs.
     * @tparam Func The type of the function to test.
     * @tparam Args The types of the arguments to the function.
     * @param testFunc The function to test.
     * @param iterations The number of iterations to run the test.
     * @param argGenerators The generators for the function arguments.
     * @param exceptionHandler Optional handler for exceptions thrown by
     * testFunc.
     *
     * @return Number of successful test runs
     *
     * @throws RandomGenerationError if iterations is negative
     */
    template <typename Func, typename... Args>
    auto fuzzTest(
        Func testFunc, int iterations, std::function<Args()>... argGenerators,
        std::function<void(const std::exception&)> exceptionHandler = nullptr)
        -> int;

    /**
     * @brief Generates a random IPv4 address.
     * @param includedSegmentRanges Optional vector of pairs representing
     * allowed ranges for each segment
     * @return A random IPv4 address.
     */
    auto generateIPv4Address(
        const std::vector<std::pair<int, int>>& includedSegmentRanges = {})
        -> std::string;

    /**
     * @brief Generates a random MAC address.
     * @param upperCase Whether to use uppercase hexadecimal digits
     * @param separator Separator character between segments (default: ':')
     * @return A random MAC address.
     */
    auto generateMACAddress(bool upperCase = false, char separator = ':')
        -> std::string;

    /**
     * @brief Generates a random URL.
     * @param protocol Optional protocol (default: randomly chosen from
     * http/https)
     * @param tlds Optional vector of top-level domains (default: common TLDs)
     * @return A random URL.
     */
    auto generateURL(std::optional<std::string_view> protocol = std::nullopt,
                     const std::vector<std::string>& tlds = {}) -> std::string;

    /**
     * @brief Generates a vector of random numbers following a normal
     * distribution.
     * @param count The number of numbers to generate.
     * @param mean The mean of the distribution.
     * @param stddev The standard deviation of the distribution.
     * @return A vector of random numbers following a normal distribution.
     *
     * @throws RandomGenerationError if count is negative or stddev is negative
     */
    auto generateNormalDistribution(int count, double mean, double stddev)
        -> std::vector<double>;

    /**
     * @brief Generates a vector of random numbers following an exponential
     * distribution.
     * @param count The number of numbers to generate.
     * @param lambda The rate parameter of the distribution.
     * @return A vector of random numbers following an exponential distribution.
     *
     * @throws RandomGenerationError if count is negative or lambda is negative
     */
    auto generateExponentialDistribution(int count, double lambda)
        -> std::vector<double>;

    /**
     * @brief Serializes data to a JSON string.
     * @tparam T The type of the data (must satisfy Serializable concept).
     * @param data The data to serialize.
     * @return A JSON string representing the data.
     */
    template <Serializable T>
    auto serializeToJSON(const T& data) -> std::string;

    /**
     * @brief Generates a vector of random elements.
     * @tparam T The type of the elements.
     * @param count The number of elements to generate.
     * @param generator The generator function for the elements.
     * @return A vector of random elements.
     *
     * @throws RandomGenerationError if count is negative
     */
    template <typename T>
    auto generateVector(int count, std::function<T()> generator)
        -> std::vector<T>;

    /**
     * @brief Generates a map of random key-value pairs.
     * @tparam K The type of the keys.
     * @tparam V The type of the values.
     * @param count The number of key-value pairs to generate.
     * @param keyGenerator The generator function for the keys.
     * @param valueGenerator The generator function for the values.
     * @param allowDuplicateKeys Whether to allow duplicate keys (default:
     * false)
     * @return A map of random key-value pairs.
     *
     * @throws RandomGenerationError if count is negative
     */
    template <typename K, typename V>
    auto generateMap(int count, std::function<K()> keyGenerator,
                     std::function<V()> valueGenerator,
                     bool allowDuplicateKeys = false) -> std::map<K, V>;

    /**
     * @brief Generates a set of random elements.
     * @tparam T The type of the elements.
     * @param count The target number of elements to generate.
     * @param generator The generator function for the elements.
     * @param maxAttempts Maximum number of generation attempts to reach count
     * @return A set of random elements (may contain fewer than count if
     * duplicates occur).
     *
     * @throws RandomGenerationError if count or maxAttempts is negative
     */
    template <typename T>
    auto generateSet(int count, std::function<T()> generator,
                     int maxAttempts = -1) -> std::set<T>;

    /**
     * @brief Generates a vector of random elements following a custom
     * distribution.
     * @tparam T The type of the elements.
     * @tparam Distribution The type of the distribution.
     * @param count The number of elements to generate.
     * @param distribution The distribution to use.
     * @return A vector of random elements following the custom distribution.
     *
     * @throws RandomGenerationError if count is negative
     */
    template <typename T, typename Distribution>
    auto generateCustomDistribution(int count, Distribution& distribution)
        -> std::vector<T>;

    /**
     * @brief Generates a sorted vector of random elements.
     * @tparam T The type of the elements.
     * @param count The number of elements to generate.
     * @param generator The generator function for the elements.
     * @param comparator Optional custom comparison function
     * @return A sorted vector of random elements.
     *
     * @throws RandomGenerationError if count is negative
     */
    template <typename T>
    auto generateSortedVector(
        int count, std::function<T()> generator,
        std::function<bool(const T&, const T&)> comparator = std::less<T>{})
        -> std::vector<T>;

    /**
     * @brief Generates a vector of unique random elements.
     * @tparam T The type of the elements.
     * @param count The number of elements to generate.
     * @param generator The generator function for the elements.
     * @param maxAttempts Maximum number of generation attempts to reach count
     * @return A vector of unique random elements.
     *
     * @throws RandomGenerationError if count is negative or maxAttempts is
     * reached
     */
    template <typename T>
    auto generateUniqueVector(int count, std::function<T()> generator,
                              int maxAttempts = -1) -> std::vector<T>;

    /**
     * @brief Generates a random tree.
     * @param depth The depth of the tree.
     * @param maxChildren The maximum number of children per node.
     * @return A random tree.
     *
     * @throws RandomGenerationError if depth is negative or maxChildren is
     * negative
     */
    auto generateTree(int depth, int maxChildren) -> TreeNode;

    /**
     * @brief Generates a random graph.
     * @param nodes The number of nodes in the graph.
     * @param edgeProbability The probability of an edge between any two nodes.
     * @return A random graph represented as an adjacency list.
     *
     * @throws RandomGenerationError if nodes is negative or edgeProbability is
     * out of range
     */
    auto generateGraph(int nodes, double edgeProbability)
        -> std::vector<std::vector<int>>;

    /**
     * @brief Generates a vector of random key-value pairs.
     * @param count The number of key-value pairs to generate.
     * @param keyLength Optional key length (default: 5)
     * @param valueLength Optional value length (default: 8)
     * @return A vector of random key-value pairs.
     *
     * @throws RandomGenerationError if count, keyLength, or valueLength is
     * negative
     */
    auto generateKeyValuePairs(int count, int keyLength = 5,
                               int valueLength = 8)
        -> std::vector<std::pair<std::string, std::string>>;

    /**
     * @brief Creates a thread-local instance of the generator
     * @param seed Optional seed value (default: random)
     * @return Reference to thread-local generator
     */
    static auto threadLocal(std::optional<int> seed = std::nullopt)
        -> RandomDataGenerator&;

private:
    // Helper functions for serialization
    static void serializeToJSONHelper(std::ostringstream& oss,
                                      const std::string& str);
    static void serializeToJSONHelper(std::ostringstream& oss, int number);
    static void serializeToJSONHelper(std::ostringstream& oss, double number);
    static void serializeToJSONHelper(std::ostringstream& oss, bool boolean);

    template <typename T>
    static void serializeToJSONHelper(std::ostringstream& oss,
                                      const std::vector<T>& vec);

    template <typename K, typename V>
    static void serializeToJSONHelper(std::ostringstream& oss,
                                      const std::map<K, V>& map);

    // Validate parameters
    void validateCount(int count, const std::string& paramName) const;
    template <typename T>
    void validateRange(T min, T max, const std::string& paramName) const {
        if (min > max) {
            throw RandomGenerationError(std::format(
                "Invalid {} range: min ({}) > max ({})", paramName, min, max));
        }
    }
    void validateProbability(double probability,
                             const std::string& paramName) const;
};

// Template implementation

template <typename T>
auto RandomDataGenerator::getIntDistribution(int min, int max)
    -> std::uniform_int_distribution<T>& {
    validateRange(min, max, "integer range");

    std::string key = std::to_string(min) + ":" + std::to_string(max);
    if (config_.threadSafe) {
        std::unique_lock lock(mutex_);
        if (!intDistCache_.contains(key)) {
            intDistCache_[key] =
                std::make_unique<std::uniform_int_distribution<>>(min, max);
        }
        return *static_cast<std::uniform_int_distribution<T>*>(
            intDistCache_[key].get());
    } else {
        if (!intDistCache_.contains(key)) {
            intDistCache_[key] =
                std::make_unique<std::uniform_int_distribution<>>(min, max);
        }
        return *static_cast<std::uniform_int_distribution<T>*>(
            intDistCache_[key].get());
    }
}

template <typename T>
auto RandomDataGenerator::getRealDistribution(T min, T max)
    -> std::uniform_real_distribution<T>& {
    validateRange(min, max, "real range");

    std::string key = std::to_string(min) + ":" + std::to_string(max);
    if (config_.threadSafe) {
        std::unique_lock lock(mutex_);
        if (!realDistCache_.contains(key)) {
            realDistCache_[key] =
                std::make_unique<std::uniform_real_distribution<>>(min, max);
        }
        return *static_cast<std::uniform_real_distribution<T>*>(
            realDistCache_[key].get());
    } else {
        if (!realDistCache_.contains(key)) {
            realDistCache_[key] =
                std::make_unique<std::uniform_real_distribution<>>(min, max);
        }
        return *static_cast<std::uniform_real_distribution<T>*>(
            realDistCache_[key].get());
    }
}

template <typename Func>
auto RandomDataGenerator::withSharedLock(Func&& func) const {
    if (config_.threadSafe) {
        std::shared_lock lock(mutex_);
        return std::forward<Func>(func)();
    } else {
        return std::forward<Func>(func)();
    }
}

template <typename Func>
auto RandomDataGenerator::withExclusiveLock(Func&& func) {
    if (config_.threadSafe) {
        std::unique_lock lock(mutex_);
        return std::forward<Func>(func)();
    } else {
        return std::forward<Func>(func)();
    }
}

template <typename Func, typename... Args>
auto RandomDataGenerator::fuzzTest(
    Func testFunc, int iterations, std::function<Args()>... argGenerators,
    std::function<void(const std::exception&)> exceptionHandler) -> int {
    validateCount(iterations, "iterations");

    int successCount = 0;
    for (int i = 0; i < iterations; ++i) {
        try {
            testFunc(argGenerators()...);
            successCount++;
        } catch (const std::exception& e) {
            if (exceptionHandler) {
                exceptionHandler(e);
            }
        }
    }
    return successCount;
}

template <Serializable T>
auto RandomDataGenerator::serializeToJSON(const T& data) -> std::string {
    std::ostringstream oss;
    serializeToJSONHelper(oss, data);
    return oss.str();
}

template <typename T>
auto RandomDataGenerator::generateVector(int count,
                                         std::function<T()> generator)
    -> std::vector<T> {
    validateCount(count, "count");

    return withExclusiveLock([&]() {
        std::vector<T> result;
        result.reserve(count);
        for (int i = 0; i < count; ++i) {
            result.push_back(generator());
        }
        return result;
    });
}

template <typename T, typename Distribution>
auto RandomDataGenerator::generateCustomDistribution(int count,
                                                     Distribution& distribution)
    -> std::vector<T> {
    validateCount(count, "count");

    return withExclusiveLock([&]() {
        std::vector<T> result;
        result.reserve(count);
        for (int i = 0; i < count; ++i) {
            result.push_back(static_cast<T>(distribution(generator_)));
        }
        return result;
    });
}

template <typename K, typename V>
auto RandomDataGenerator::generateMap(int count,
                                      std::function<K()> keyGenerator,
                                      std::function<V()> valueGenerator,
                                      bool allowDuplicateKeys)
    -> std::map<K, V> {
    validateCount(count, "count");

    return withExclusiveLock([&]() {
        std::map<K, V> result;
        if (allowDuplicateKeys) {
            // Simple case: just add count pairs
            for (int i = 0; i < count; ++i) {
                result.emplace(keyGenerator(), valueGenerator());
            }
        } else {
            // We need to ensure unique keys
            const int maxAttempts = count * 10;  // Avoid infinite loops
            int attempts = 0;
            while (result.size() < static_cast<size_t>(count) &&
                   attempts < maxAttempts) {
                result.emplace(keyGenerator(), valueGenerator());
                attempts++;
            }

            if (result.size() < static_cast<size_t>(count)) {
                throw RandomGenerationError(
                    "Could not generate enough unique keys");
            }
        }
        return result;
    });
}

template <typename T>
auto RandomDataGenerator::generateSet(int count, std::function<T()> generator,
                                      int maxAttempts) -> std::set<T> {
    validateCount(count, "count");
    if (maxAttempts < 0) {
        maxAttempts = count * 10;  // Default to 10x count
    } else if (maxAttempts == 0) {
        throw RandomGenerationError("maxAttempts must be positive");
    }

    return withExclusiveLock([&]() {
        std::set<T> result;
        int attempts = 0;
        while (result.size() < static_cast<size_t>(count) &&
               attempts < maxAttempts) {
            result.insert(generator());
            attempts++;
        }
        return result;
    });
}

template <typename T>
auto RandomDataGenerator::generateSortedVector(
    int count, std::function<T()> generator,
    std::function<bool(const T&, const T&)> comparator) -> std::vector<T> {
    auto result = generateVector<T>(count, generator);
    std::sort(result.begin(), result.end(), comparator);
    return result;
}

template <typename T>
auto RandomDataGenerator::generateUniqueVector(int count,
                                               std::function<T()> generator,
                                               int maxAttempts)
    -> std::vector<T> {
    validateCount(count, "count");
    if (maxAttempts < 0) {
        maxAttempts = count * 10;  // Default to 10x count
    } else if (maxAttempts == 0) {
        throw RandomGenerationError("maxAttempts must be positive");
    }

    return withExclusiveLock([&]() {
        std::set<T> uniqueItems;
        int attempts = 0;
        while (uniqueItems.size() < static_cast<size_t>(count) &&
               attempts < maxAttempts) {
            uniqueItems.insert(generator());
            attempts++;
        }

        if (uniqueItems.size() < static_cast<size_t>(count)) {
            throw RandomGenerationError(
                "Could not generate enough unique items");
        }

        return std::vector<T>(uniqueItems.begin(), uniqueItems.end());
    });
}

template <typename T>
void RandomDataGenerator::serializeToJSONHelper(std::ostringstream& oss,
                                                const std::vector<T>& vec) {
    oss << "[";
    for (size_t i = 0; i < vec.size(); ++i) {
        if (i > 0) {
            oss << ",";
        }
        serializeToJSONHelper(oss, vec[i]);
    }
    oss << "]";
}

template <typename K, typename V>
void RandomDataGenerator::serializeToJSONHelper(std::ostringstream& oss,
                                                const std::map<K, V>& map) {
    oss << "{";
    bool first = true;
    for (const auto& [key, value] : map) {
        if (!first) {
            oss << ",";
        }
        first = false;
        oss << "\"";
        oss << key;
        oss << "\":";
        serializeToJSONHelper(oss, value);
    }
    oss << "}";
}

// Function to serialize any Serializable type (template specialization point
// for custom types)
template <Serializable T>
void serializeToJSON(std::ostringstream& oss, const T& value) {
    // Default implementation for common types
    if constexpr (std::is_same_v<T, int> || std::is_same_v<T, double> ||
                  std::is_same_v<T, bool> || std::is_same_v<T, std::string>) {
        RandomDataGenerator::serializeToJSONHelper(oss, value);
    } else {
        // Customization point - specialize this for your types
        oss << "null";
    }
}

}  // namespace atom::tests

#endif  // ATOM_TESTS_FUZZ_HPP
