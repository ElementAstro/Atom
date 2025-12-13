/**
 * @file test_data.hpp
 * @brief Test data factories and generators
 * @details Provides builders, factories, and random data generators
 *
 * @author Max Qian
 * @copyright GPL3 License
 */

#ifndef ATOM_TEST_UTILITIES_TEST_DATA_HPP
#define ATOM_TEST_UTILITIES_TEST_DATA_HPP

#include <any>
#include <functional>
#include <limits>
#include <map>
#include <random>
#include <string>
#include <type_traits>
#include <typeindex>
#include <vector>

namespace atom::test {

/**
 * @brief Test data factory for creating test objects
 * @details Provides a flexible way to create test data with builders and
 * factories
 */
template <typename T>
class TestDataBuilder {
public:
    using BuilderFunc = std::function<void(T&)>;

    TestDataBuilder() = default;

    /**
     * @brief Set a property using a setter function
     * @param setter Function to set the property
     * @return Reference for chaining
     */
    auto with(BuilderFunc setter) -> TestDataBuilder& {
        setters_.push_back(std::move(setter));
        return *this;
    }

    /**
     * @brief Set a member variable directly
     * @tparam MemberType Type of the member
     * @param member Pointer to member
     * @param value Value to set
     * @return Reference for chaining
     */
    template <typename MemberType>
    auto set(MemberType T::*member, MemberType value) -> TestDataBuilder& {
        setters_.push_back(
            [member, val = std::move(value)](T& obj) { obj.*member = val; });
        return *this;
    }

    /**
     * @brief Build the object with all configured setters
     * @return Constructed object
     */
    [[nodiscard]] auto build() const -> T {
        T obj{};
        for (const auto& setter : setters_) {
            setter(obj);
        }
        return obj;
    }

    /**
     * @brief Build multiple objects
     * @param count Number of objects to build
     * @return Vector of constructed objects
     */
    [[nodiscard]] auto buildMany(size_t count) const -> std::vector<T> {
        std::vector<T> result;
        result.reserve(count);
        for (size_t i = 0; i < count; ++i) {
            result.push_back(build());
        }
        return result;
    }

    /**
     * @brief Build with variations
     * @param variations Vector of additional setters for each object
     * @return Vector of constructed objects with variations
     */
    [[nodiscard]] auto buildWithVariations(
        const std::vector<BuilderFunc>& variations) const -> std::vector<T> {
        std::vector<T> result;
        result.reserve(variations.size());
        for (const auto& variation : variations) {
            T obj = build();
            variation(obj);
            result.push_back(std::move(obj));
        }
        return result;
    }

private:
    std::vector<BuilderFunc> setters_;
};

/**
 * @brief Create a test data builder
 * @tparam T Type to build
 * @return TestDataBuilder instance
 */
template <typename T>
auto builder() -> TestDataBuilder<T> {
    return TestDataBuilder<T>{};
}

/**
 * @brief Factory for creating test data with named presets
 * @tparam T Type to create
 */
template <typename T>
class TestDataFactory {
public:
    using FactoryFunc = std::function<T()>;

    /**
     * @brief Register a named preset
     * @param name Preset name
     * @param factory Factory function
     */
    void registerPreset(const std::string& name, FactoryFunc factory) {
        presets_[name] = std::move(factory);
    }

    /**
     * @brief Create an object using a preset
     * @param name Preset name
     * @return Created object or default if preset not found
     */
    [[nodiscard]] auto create(const std::string& name) const -> T {
        auto it = presets_.find(name);
        if (it != presets_.end()) {
            return it->second();
        }
        return T{};
    }

    /**
     * @brief Check if a preset exists
     * @param name Preset name
     * @return True if preset exists
     */
    [[nodiscard]] auto hasPreset(const std::string& name) const -> bool {
        return presets_.find(name) != presets_.end();
    }

    /**
     * @brief Get all preset names
     * @return Vector of preset names
     */
    [[nodiscard]] auto getPresetNames() const -> std::vector<std::string> {
        std::vector<std::string> names;
        names.reserve(presets_.size());
        for (const auto& [name, _] : presets_) {
            names.push_back(name);
        }
        return names;
    }

    /**
     * @brief Create multiple objects using a preset
     * @param name Preset name
     * @param count Number of objects
     * @return Vector of created objects
     */
    [[nodiscard]] auto createMany(const std::string& name,
                                  size_t count) const -> std::vector<T> {
        std::vector<T> result;
        result.reserve(count);
        for (size_t i = 0; i < count; ++i) {
            result.push_back(create(name));
        }
        return result;
    }

private:
    std::map<std::string, FactoryFunc> presets_;
};

/**
 * @brief Global factory registry
 */
class FactoryRegistry {
public:
    static auto instance() -> FactoryRegistry& {
        static FactoryRegistry registry;
        return registry;
    }

    template <typename T>
    void registerFactory(const std::string& name,
                         std::function<T()> factory) {
        factories_[std::type_index(typeid(T))][name] =
            [f = std::move(factory)]() -> std::any { return f(); };
    }

    template <typename T>
    [[nodiscard]] auto create(const std::string& name) const -> T {
        auto typeIt = factories_.find(std::type_index(typeid(T)));
        if (typeIt != factories_.end()) {
            auto factoryIt = typeIt->second.find(name);
            if (factoryIt != typeIt->second.end()) {
                return std::any_cast<T>(factoryIt->second());
            }
        }
        return T{};
    }

    template <typename T>
    [[nodiscard]] auto hasFactory(const std::string& name) const -> bool {
        auto typeIt = factories_.find(std::type_index(typeid(T)));
        if (typeIt != factories_.end()) {
            return typeIt->second.find(name) != typeIt->second.end();
        }
        return false;
    }

private:
    FactoryRegistry() = default;
    std::map<std::type_index, std::map<std::string, std::function<std::any()>>>
        factories_;
};

/**
 * @brief Random data generator for tests
 */
class RandomTestData {
public:
    explicit RandomTestData(uint64_t seed = 0)
        : gen_(seed == 0 ? std::random_device{}() : seed) {}

    /**
     * @brief Generate random integer in range
     */
    template <typename T = int>
    auto randomInt(T min = std::numeric_limits<T>::min(),
                   T max = std::numeric_limits<T>::max()) -> T {
        std::uniform_int_distribution<T> dist(min, max);
        return dist(gen_);
    }

    /**
     * @brief Generate random floating point in range
     */
    template <typename T = double>
    auto randomReal(T min = 0.0, T max = 1.0) -> T {
        std::uniform_real_distribution<T> dist(min, max);
        return dist(gen_);
    }

    /**
     * @brief Generate random boolean
     */
    auto randomBool(double trueProbability = 0.5) -> bool {
        std::bernoulli_distribution dist(trueProbability);
        return dist(gen_);
    }

    /**
     * @brief Generate random string
     */
    auto randomString(size_t length,
                      const std::string& charset =
                          "abcdefghijklmnopqrstuvwxyz"
                          "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                          "0123456789") -> std::string {
        std::string result;
        result.reserve(length);
        std::uniform_int_distribution<size_t> dist(0, charset.size() - 1);
        for (size_t i = 0; i < length; ++i) {
            result += charset[dist(gen_)];
        }
        return result;
    }

    /**
     * @brief Generate random alphanumeric string
     */
    auto randomAlphanumeric(size_t length) -> std::string {
        return randomString(length,
                            "abcdefghijklmnopqrstuvwxyz"
                            "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                            "0123456789");
    }

    /**
     * @brief Generate random alphabetic string
     */
    auto randomAlpha(size_t length) -> std::string {
        return randomString(length,
                            "abcdefghijklmnopqrstuvwxyz"
                            "ABCDEFGHIJKLMNOPQRSTUVWXYZ");
    }

    /**
     * @brief Generate random numeric string
     */
    auto randomNumeric(size_t length) -> std::string {
        return randomString(length, "0123456789");
    }

    /**
     * @brief Generate random lowercase string
     */
    auto randomLower(size_t length) -> std::string {
        return randomString(length, "abcdefghijklmnopqrstuvwxyz");
    }

    /**
     * @brief Generate random uppercase string
     */
    auto randomUpper(size_t length) -> std::string {
        return randomString(length, "ABCDEFGHIJKLMNOPQRSTUVWXYZ");
    }

    /**
     * @brief Generate random email
     */
    auto randomEmail() -> std::string {
        return randomAlpha(8) + "@" + randomAlpha(5) + ".com";
    }

    /**
     * @brief Generate random UUID-like string
     */
    auto randomUuid() -> std::string {
        auto hex = [this]() { return randomString(4, "0123456789abcdef"); };
        return hex() + hex() + "-" + hex() + "-" + hex() + "-" + hex() + "-" +
               hex() + hex() + hex();
    }

    /**
     * @brief Generate random IPv4 address
     */
    auto randomIPv4() -> std::string {
        return std::to_string(randomInt<int>(0, 255)) + "." +
               std::to_string(randomInt<int>(0, 255)) + "." +
               std::to_string(randomInt<int>(0, 255)) + "." +
               std::to_string(randomInt<int>(0, 255));
    }

    /**
     * @brief Pick random element from vector
     */
    template <typename T>
    auto randomElement(const std::vector<T>& vec) -> const T& {
        std::uniform_int_distribution<size_t> dist(0, vec.size() - 1);
        return vec[dist(gen_)];
    }

    /**
     * @brief Pick random element from initializer list
     */
    template <typename T>
    auto randomChoice(std::initializer_list<T> choices) -> T {
        std::vector<T> vec(choices);
        return randomElement(vec);
    }

    /**
     * @brief Generate vector of random values
     */
    template <typename T, typename Generator>
    auto randomVector(size_t count, Generator gen) -> std::vector<T> {
        std::vector<T> result;
        result.reserve(count);
        for (size_t i = 0; i < count; ++i) {
            result.push_back(gen());
        }
        return result;
    }

    /**
     * @brief Shuffle a vector
     */
    template <typename T>
    void shuffle(std::vector<T>& vec) {
        std::shuffle(vec.begin(), vec.end(), gen_);
    }

    /**
     * @brief Get the random generator
     */
    auto generator() -> std::mt19937_64& { return gen_; }

    /**
     * @brief Reset with new seed
     */
    void seed(uint64_t newSeed) { gen_.seed(newSeed); }

private:
    std::mt19937_64 gen_;
};

/**
 * @brief Test data sequence generator
 */
class SequenceGenerator {
public:
    /**
     * @brief Generate integer sequence
     */
    static auto integers(int start, int end, int step = 1) -> std::vector<int> {
        std::vector<int> result;
        for (int i = start; i < end; i += step) {
            result.push_back(i);
        }
        return result;
    }

    /**
     * @brief Generate string sequence with pattern
     */
    static auto strings(const std::string& pattern,
                        size_t count) -> std::vector<std::string> {
        std::vector<std::string> result;
        result.reserve(count);
        for (size_t i = 0; i < count; ++i) {
            std::string str = pattern;
            size_t pos = str.find("{}");
            if (pos != std::string::npos) {
                str.replace(pos, 2, std::to_string(i));
            }
            result.push_back(str);
        }
        return result;
    }

    /**
     * @brief Generate fibonacci sequence
     */
    static auto fibonacci(size_t count) -> std::vector<uint64_t> {
        std::vector<uint64_t> result;
        result.reserve(count);
        uint64_t a = 0, b = 1;
        for (size_t i = 0; i < count; ++i) {
            result.push_back(a);
            uint64_t next = a + b;
            a = b;
            b = next;
        }
        return result;
    }

    /**
     * @brief Generate powers of 2
     */
    static auto powersOf2(size_t count) -> std::vector<uint64_t> {
        std::vector<uint64_t> result;
        result.reserve(count);
        uint64_t val = 1;
        for (size_t i = 0; i < count; ++i) {
            result.push_back(val);
            val *= 2;
        }
        return result;
    }

    /**
     * @brief Generate repeated value
     */
    template <typename T>
    static auto repeat(const T& value, size_t count) -> std::vector<T> {
        return std::vector<T>(count, value);
    }

    /**
     * @brief Generate cycle of values
     */
    template <typename T>
    static auto cycle(const std::vector<T>& values,
                      size_t count) -> std::vector<T> {
        std::vector<T> result;
        result.reserve(count);
        for (size_t i = 0; i < count; ++i) {
            result.push_back(values[i % values.size()]);
        }
        return result;
    }

    /**
     * @brief Generate linear sequence
     */
    template <typename T>
    static auto linear(T start, T step, size_t count) -> std::vector<T> {
        std::vector<T> result;
        result.reserve(count);
        T val = start;
        for (size_t i = 0; i < count; ++i) {
            result.push_back(val);
            val += step;
        }
        return result;
    }

    /**
     * @brief Generate geometric sequence
     */
    template <typename T>
    static auto geometric(T start, T ratio, size_t count) -> std::vector<T> {
        std::vector<T> result;
        result.reserve(count);
        T val = start;
        for (size_t i = 0; i < count; ++i) {
            result.push_back(val);
            val *= ratio;
        }
        return result;
    }
};

/**
 * @brief Boundary value generator for testing edge cases
 */
class BoundaryValues {
public:
    /**
     * @brief Get boundary values for integer type
     */
    template <typename T>
    static auto forInteger() -> std::vector<T> {
        if constexpr (std::is_signed_v<T>) {
            return {std::numeric_limits<T>::min(),
                    std::numeric_limits<T>::min() + 1,
                    static_cast<T>(-1),
                    static_cast<T>(0),
                    static_cast<T>(1),
                    std::numeric_limits<T>::max() - 1,
                    std::numeric_limits<T>::max()};
        } else {
            return {static_cast<T>(0),
                    static_cast<T>(1),
                    std::numeric_limits<T>::max() - 1,
                    std::numeric_limits<T>::max()};
        }
    }

    /**
     * @brief Get boundary values for unsigned integer type
     */
    template <typename T>
    static auto forUnsigned() -> std::vector<T> {
        return {static_cast<T>(0), static_cast<T>(1),
                std::numeric_limits<T>::max() - 1,
                std::numeric_limits<T>::max()};
    }

    /**
     * @brief Get boundary values for floating point type
     */
    template <typename T>
    static auto forFloat() -> std::vector<T> {
        return {-std::numeric_limits<T>::infinity(),
                std::numeric_limits<T>::lowest(),
                static_cast<T>(-1.0),
                -std::numeric_limits<T>::min(),
                -std::numeric_limits<T>::denorm_min(),
                static_cast<T>(-0.0),
                static_cast<T>(0.0),
                std::numeric_limits<T>::denorm_min(),
                std::numeric_limits<T>::min(),
                static_cast<T>(1.0),
                std::numeric_limits<T>::max(),
                std::numeric_limits<T>::infinity(),
                std::numeric_limits<T>::quiet_NaN()};
    }

    /**
     * @brief Get boundary values for string length
     */
    static auto forStringLength(size_t maxLen) -> std::vector<size_t> {
        return {0, 1, maxLen / 2, maxLen - 1, maxLen};
    }

    /**
     * @brief Get boundary values for array index
     */
    static auto forArrayIndex(size_t arraySize) -> std::vector<size_t> {
        if (arraySize == 0) return {0};
        return {0, 1, arraySize / 2, arraySize - 1};
    }

    /**
     * @brief Get common special string values
     */
    static auto specialStrings() -> std::vector<std::string> {
        return {
            "",                    // Empty string
            " ",                   // Single space
            "  ",                  // Multiple spaces
            "\t",                  // Tab
            "\n",                  // Newline
            "\r\n",                // Windows newline
            "null",                // Null keyword
            "undefined",           // Undefined keyword
            "true",                // Boolean
            "false",               // Boolean
            "0",                   // Zero
            "-1",                  // Negative
            "1.5",                 // Float
            "NaN",                 // Not a number
            "Infinity",            // Infinity
            "<script>alert(1)</script>",  // XSS
            "'; DROP TABLE users; --",    // SQL injection
            "../../../etc/passwd",        // Path traversal
        };
    }
};

/**
 * @brief Test fixture with pre-built data
 */
template <typename T>
class TestDataFixture {
public:
    using DataType = T;

    TestDataFixture() = default;

    /**
     * @brief Add test data
     */
    void addData(const std::string& name, T data) {
        data_[name] = std::move(data);
    }

    /**
     * @brief Get test data by name
     */
    [[nodiscard]] auto getData(const std::string& name) const -> const T& {
        return data_.at(name);
    }

    /**
     * @brief Check if data exists
     */
    [[nodiscard]] auto hasData(const std::string& name) const -> bool {
        return data_.find(name) != data_.end();
    }

    /**
     * @brief Get all data
     */
    [[nodiscard]] auto getAllData() const
        -> const std::map<std::string, T>& {
        return data_;
    }

private:
    std::map<std::string, T> data_;
};

}  // namespace atom::test

/**
 * @brief Macro to define a test data factory
 */
#define DEFINE_TEST_FACTORY(Type, Name, ...)                                  \
    static struct Name##_Factory_Registrar {                                  \
        Name##_Factory_Registrar() {                                          \
            atom::test::FactoryRegistry::instance().registerFactory<Type>(    \
                #Name, []() -> Type { return __VA_ARGS__; });                 \
        }                                                                     \
    } Name##_factory_registrar_instance

/**
 * @brief Macro to create test data using factory
 */
#define CREATE_TEST_DATA(Type, Name) \
    atom::test::FactoryRegistry::instance().create<Type>(#Name)

#endif  // ATOM_TEST_UTILITIES_TEST_DATA_HPP
