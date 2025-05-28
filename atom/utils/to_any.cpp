#include "to_any.hpp"

#include <algorithm>
#include <charconv>
#include <chrono>
#include <iomanip>
#include <map>
#include <mutex>
#include <set>
#include <sstream>
#include <string>
#include <thread>

#ifdef __SSE4_1__
#include <immintrin.h>
#endif

#include <spdlog/spdlog.h>
#include "atom/type/json.hpp"

using json = nlohmann::json;

namespace atom::utils {

class Parser::Impl {
public:
    auto parseLiteral(std::string_view input) -> std::optional<std::any>;
    auto parseLiteralWithDefault(std::string_view input,
                                 const std::any& defaultValue) -> std::any;
    void print(const std::any& value) const;
    void logParsing(std::string_view input, const std::any& result) const;

    template <std::ranges::input_range Range>
        requires std::convertible_to<std::ranges::range_value_t<Range>,
                                     std::string_view>
    auto convertToAnyVector(const Range& input) -> std::vector<std::any>;

    void registerCustomParser(std::string_view type, CustomParserFunc parser);
    void printCustomParsers() const;
    void parseJson(std::string_view jsonString) const;
    void parseCsv(std::string_view csvString, char delimiter = ',') const;
    auto parseParallel(const std::vector<std::string>& inputs)
        -> std::vector<std::any>;

private:
    std::unordered_map<std::string, CustomParserFunc> customParsers_;
    mutable std::mutex parserMutex_;

    static constexpr auto trim(std::string_view str) noexcept
        -> std::string_view;
    auto fromString(std::string_view str) -> std::optional<std::any>;

    template <Numeric T>
    auto parseSingleValue(std::string_view str) noexcept -> std::optional<T>;

    template <StringLike T>
    auto parseSingleValue(std::string_view str) noexcept
        -> std::optional<std::string>;

    template <typename T>
    auto parseVectorOf(std::string_view str) -> std::optional<std::vector<T>>;

    template <typename T>
    auto parseSetOf(std::string_view str) -> std::optional<std::set<T>>;

    template <typename K, typename V>
    auto parseMapOf(std::string_view str) -> std::optional<std::map<K, V>>;

    static auto split(std::string_view str, char delimiter)
        -> std::vector<std::string>;
    static auto parseDateTime(std::string_view str)
        -> std::optional<std::chrono::system_clock::time_point>;

    bool containsDigitsOnly(std::string_view str) const noexcept;
    bool containsFloatingPoint(std::string_view str) const noexcept;
};

Parser::Parser() : pImpl_(std::make_unique<Impl>()) {}
Parser::~Parser() = default;

auto Parser::parseLiteral(std::string_view input) -> std::optional<std::any> {
    if (input.empty()) {
        THROW_PARSER_ERROR("Cannot parse empty input");
    }

    if (isProcessing_.exchange(true)) {
        THROW_PARSER_ERROR("Parser is currently processing another input");
    }

    try {
        auto result = pImpl_->parseLiteral(input);
        isProcessing_ = false;
        return result;
    } catch (...) {
        isProcessing_ = false;
        THROW_PARSER_ERROR("Failed to parse literal");
    }
}

auto Parser::parseLiteralWithDefault(std::string_view input,
                                     const std::any& defaultValue) -> std::any {
    try {
        auto result = parseLiteral(input);
        return result ? *result : defaultValue;
    } catch (const ParserException& e) {
        spdlog::warn("Parser exception: {}", e.what());
        return defaultValue;
    } catch (const std::exception& e) {
        spdlog::warn("Standard exception: {}", e.what());
        return defaultValue;
    } catch (...) {
        spdlog::warn("Unknown exception during parsing");
        return defaultValue;
    }
}

void Parser::print(const std::any& value) const {
    try {
        pImpl_->print(value);
    } catch (const std::exception& e) {
        spdlog::error("Error printing value: {}", e.what());
    }
}

void Parser::logParsing(std::string_view input, const std::any& result) const {
    try {
        pImpl_->logParsing(input, result);
    } catch (const std::exception& e) {
        spdlog::error("Error logging parsing: {}", e.what());
    }
}

template <std::ranges::input_range Range>
    requires std::convertible_to<std::ranges::range_value_t<Range>,
                                 std::string_view>
auto Parser::convertToAnyVector(const Range& input) -> std::vector<std::any> {
    try {
        return pImpl_->convertToAnyVector(input);
    } catch (const std::exception& e) {
        spdlog::error("Error converting to any vector: {}", e.what());
        return {};
    }
}

void Parser::registerCustomParser(std::string_view type,
                                  CustomParserFunc parser) {
    if (type.empty()) {
        THROW_PARSER_ERROR("Type cannot be empty");
    }
    if (!parser) {
        THROW_PARSER_ERROR("Parser function cannot be null");
    }

    try {
        pImpl_->registerCustomParser(type, std::move(parser));
    } catch (const std::exception& e) {
        THROW_PARSER_ERROR(std::string("Failed to register custom parser: ") +
                           e.what());
    }
}

void Parser::parseJson(std::string_view jsonString) const {
    try {
        pImpl_->parseJson(jsonString);
    } catch (const ParserException& e) {
        throw;
    } catch (const std::exception& e) {
        THROW_PARSER_ERROR(std::string("JSON parsing error: ") + e.what());
    }
}

void Parser::parseCsv(std::string_view csvString, char delimiter) const {
    if (csvString.empty()) {
        THROW_PARSER_ERROR("CSV string cannot be empty");
    }

    try {
        pImpl_->parseCsv(csvString, delimiter);
    } catch (const std::exception& e) {
        THROW_PARSER_ERROR(std::string("CSV parsing error: ") + e.what());
    }
}

void Parser::printCustomParsers() const {
    try {
        pImpl_->printCustomParsers();
    } catch (const std::exception& e) {
        spdlog::error("Error printing custom parsers: {}", e.what());
    }
}

auto Parser::parseParallel(const std::vector<std::string>& inputs)
    -> std::vector<std::any> {
    try {
        return pImpl_->parseParallel(inputs);
    } catch (const std::exception& e) {
        THROW_PARSER_ERROR(std::string("Parallel parsing error: ") + e.what());
    }
}

constexpr auto Parser::Impl::trim(std::string_view str) noexcept
    -> std::string_view {
    const auto start = str.find_first_not_of(" \t\n\r");
    if (start == std::string_view::npos) {
        return {};
    }
    const auto end = str.find_last_not_of(" \t\n\r");
    return str.substr(start, end - start + 1);
}

template <Numeric T>
auto Parser::Impl::parseSingleValue(std::string_view str) noexcept
    -> std::optional<T> {
    if (str.empty()) {
        return std::nullopt;
    }

    T value{};
    auto [ptr, ec] =
        std::from_chars(str.data(), str.data() + str.size(), value);

    if (ec == std::errc{} && ptr == str.data() + str.size()) {
        return value;
    }
    return std::nullopt;
}

template <StringLike T>
auto Parser::Impl::parseSingleValue(std::string_view str) noexcept
    -> std::optional<std::string> {
    return std::string(str);
}

bool Parser::Impl::containsDigitsOnly(std::string_view str) const noexcept {
    if (str.empty()) {
        return false;
    }

    size_t startPos = 0;
    if (str[0] == '-' || str[0] == '+') {
        if (str.size() == 1) {
            return false;
        }
        startPos = 1;
    }

#ifdef __SSE4_1__
    const char* data = str.data() + startPos;
    const size_t len = str.size() - startPos;

    if (len >= 16) {
        size_t i = 0;
        for (; i + 16 <= len; i += 16) {
            __m128i chunk =
                _mm_loadu_si128(reinterpret_cast<const __m128i*>(data + i));
            __m128i lt_zero = _mm_cmplt_epi8(chunk, _mm_set1_epi8('0'));
            __m128i gt_nine = _mm_cmpgt_epi8(chunk, _mm_set1_epi8('9'));
            int mask = _mm_movemask_epi8(_mm_or_si128(lt_zero, gt_nine));
            if (mask != 0) {
                return false;
            }
        }

        for (; i < len; ++i) {
            if (data[i] < '0' || data[i] > '9') {
                return false;
            }
        }
        return true;
    }
#endif

    return std::ranges::all_of(str.substr(startPos), [](char c) {
        return std::isdigit(static_cast<unsigned char>(c));
    });
}

bool Parser::Impl::containsFloatingPoint(std::string_view str) const noexcept {
    if (str.empty()) {
        return false;
    }

    bool hasDecimal = false;
    bool hasExponent = false;
    bool hasDigit = false;
    char prev = '\0';

    for (size_t i = 0; i < str.size(); ++i) {
        char c = str[i];

        if (c >= '0' && c <= '9') {
            hasDigit = true;
        } else if (c == '.') {
            if (hasDecimal) {
                return false;
            }
            hasDecimal = true;
        } else if (c == 'e' || c == 'E') {
            if (hasExponent || !hasDigit) {
                return false;
            }
            hasExponent = true;
        } else if ((c == '+' || c == '-')) {
            if (i > 0 && prev != 'e' && prev != 'E') {
                return false;
            }
        } else {
            return false;
        }
        prev = c;
    }

    return hasDigit && (hasDecimal || hasExponent);
}

auto Parser::Impl::fromString(std::string_view str) -> std::optional<std::any> {
    auto trimmed = trim(str);
    if (trimmed.empty()) {
        return std::string{};
    }

    if (trimmed == "true") {
        return true;
    }
    if (trimmed == "false") {
        return false;
    }

    if (containsDigitsOnly(trimmed)) {
        if (auto intValue = parseSingleValue<int>(trimmed)) {
            return *intValue;
        }
        if (auto longValue = parseSingleValue<long>(trimmed)) {
            return *longValue;
        }
        if (auto longLongValue = parseSingleValue<long long>(trimmed)) {
            return *longLongValue;
        }
    }

    if (containsFloatingPoint(trimmed)) {
        if (auto doubleValue = parseSingleValue<double>(trimmed)) {
            return *doubleValue;
        }
    }

    if (trimmed.size() == 1 && !std::isspace(trimmed[0])) {
        return trimmed.front();
    }

    if (auto dateTimeValue = parseDateTime(trimmed)) {
        return *dateTimeValue;
    }

    return std::string(trimmed);
}

template <typename T>
auto Parser::Impl::parseVectorOf(std::string_view str)
    -> std::optional<std::vector<T>> {
    if (str.empty()) {
        return std::vector<T>{};
    }

    auto tokens = split(str, ',');
    std::vector<T> result;
    result.reserve(tokens.size());

    for (const auto& token : tokens) {
        auto optValue = parseSingleValue<T>(trim(token));
        if (!optValue) {
            return std::nullopt;
        }
        result.push_back(*optValue);
    }

    return result;
}

template <typename T>
auto Parser::Impl::parseSetOf(std::string_view str)
    -> std::optional<std::set<T>> {
    if (str.empty()) {
        return std::set<T>{};
    }

    auto tokens = split(str, ',');
    std::set<T> result;

    for (const auto& token : tokens) {
        auto optValue = parseSingleValue<T>(trim(token));
        if (!optValue) {
            return std::nullopt;
        }
        result.insert(*optValue);
    }

    return result;
}

template <typename K, typename V>
auto Parser::Impl::parseMapOf(std::string_view str)
    -> std::optional<std::map<K, V>> {
    if (str.empty()) {
        return std::map<K, V>{};
    }

    auto pairs = split(str, ',');
    std::map<K, V> result;

    for (const auto& pair : pairs) {
        auto keyValue = split(pair, ':');
        if (keyValue.size() != 2) {
            return std::nullopt;
        }

        auto key = trim(keyValue[0]);
        auto value = trim(keyValue[1]);

        auto keyOpt = parseSingleValue<K>(key);
        auto valueOpt = parseSingleValue<V>(value);

        if (!keyOpt || !valueOpt) {
            return std::nullopt;
        }

        result[*keyOpt] = *valueOpt;
    }

    return result;
}

auto Parser::Impl::split(std::string_view str, char delimiter)
    -> std::vector<std::string> {
    std::vector<std::string> result;
    if (str.empty()) {
        return result;
    }

    result.reserve(std::count(str.begin(), str.end(), delimiter) + 1);

    size_t start = 0;
    size_t end = str.find(delimiter);

    while (end != std::string_view::npos) {
        result.emplace_back(str.substr(start, end - start));
        start = end + 1;
        end = str.find(delimiter, start);
    }

    result.emplace_back(str.substr(start));
    return result;
}

auto Parser::Impl::parseLiteral(std::string_view input)
    -> std::optional<std::any> {
    try {
        std::lock_guard<std::mutex> lock(parserMutex_);
        for (const auto& [type, parserFunc] : customParsers_) {
            if (input.find(type) != std::string_view::npos) {
                spdlog::info("Using custom parser for type: {}", type);
                if (auto customValue = parserFunc(input)) {
                    spdlog::info("Custom parser succeeded for input: '{}'",
                                 input);
                    return customValue;
                }
            }
        }
    } catch (const std::exception& e) {
        spdlog::warn("Exception in custom parser: {}", e.what());
    }

    try {
        if (auto result = fromString(input)) {
            return result;
        }

        if (auto vectorResult = parseVectorOf<int>(input)) {
            return *vectorResult;
        }
        if (auto setResult = parseSetOf<float>(input)) {
            return *setResult;
        }
        if (auto mapResult = parseMapOf<std::string, int>(input)) {
            return *mapResult;
        }
    } catch (const std::exception& e) {
        spdlog::warn("Exception during parsing: {}", e.what());
    }

    return std::nullopt;
}

void Parser::Impl::print(const std::any& value) const {
    if (!value.has_value()) {
        spdlog::info("Parsed value: <empty>");
        return;
    }

    spdlog::info("Parsed value type: {}", value.type().name());

    try {
        if (value.type() == typeid(int)) {
            spdlog::info("Value: {}", std::any_cast<int>(value));
        } else if (value.type() == typeid(long)) {
            spdlog::info("Value: {}", std::any_cast<long>(value));
        } else if (value.type() == typeid(long long)) {
            spdlog::info("Value: {}", std::any_cast<long long>(value));
        } else if (value.type() == typeid(unsigned int)) {
            spdlog::info("Value: {}", std::any_cast<unsigned int>(value));
        } else if (value.type() == typeid(float)) {
            spdlog::info("Value: {}", std::any_cast<float>(value));
        } else if (value.type() == typeid(double)) {
            spdlog::info("Value: {:.15g}", std::any_cast<double>(value));
        } else if (value.type() == typeid(bool)) {
            spdlog::info("Value: {}",
                         std::any_cast<bool>(value) ? "true" : "false");
        } else if (value.type() == typeid(char)) {
            spdlog::info("Value: '{}'", std::any_cast<char>(value));
        } else if (value.type() == typeid(std::string)) {
            spdlog::info("Value: \"{}\"", std::any_cast<std::string>(value));
        } else if (value.type() ==
                   typeid(std::chrono::system_clock::time_point)) {
            auto time =
                std::any_cast<std::chrono::system_clock::time_point>(value);
            std::time_t timeT = std::chrono::system_clock::to_time_t(time);
            char buffer[64];
            std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S",
                          std::localtime(&timeT));
            spdlog::info("Value: {}", buffer);
        } else {
            spdlog::info("Value: <unsupported type>");
        }
    } catch (const std::bad_any_cast& e) {
        spdlog::warn("Bad any cast during printing: {}", e.what());
    } catch (const std::exception& e) {
        spdlog::warn("Exception during printing: {}", e.what());
    }
}

void Parser::Impl::logParsing(std::string_view input,
                              const std::any& result) const {
    spdlog::info("Parsed input: '{}'", input);

    if (!result.has_value()) {
        spdlog::info("Result type: <empty>");
        return;
    }

    try {
        spdlog::info("Result type: {}", result.type().name());

        if (result.type() == typeid(int)) {
            spdlog::info("Type: int");
        } else if (result.type() == typeid(long)) {
            spdlog::info("Type: long");
        } else if (result.type() == typeid(long long)) {
            spdlog::info("Type: long long");
        } else if (result.type() == typeid(unsigned int)) {
            spdlog::info("Type: unsigned int");
        } else if (result.type() == typeid(float)) {
            spdlog::info("Type: float");
        } else if (result.type() == typeid(double)) {
            spdlog::info("Type: double");
        } else if (result.type() == typeid(bool)) {
            spdlog::info("Type: bool");
        } else if (result.type() == typeid(char)) {
            spdlog::info("Type: char");
        } else if (result.type() == typeid(std::string)) {
            spdlog::info("Type: string");
        } else if (result.type() ==
                   typeid(std::chrono::system_clock::time_point)) {
            spdlog::info("Type: datetime");
        } else {
            spdlog::info("Type: unknown");
        }
    } catch (const std::exception& e) {
        spdlog::warn("Exception during log parsing: {}", e.what());
    }
}

auto Parser::Impl::parseDateTime(std::string_view str)
    -> std::optional<std::chrono::system_clock::time_point> {
    if (str.empty() || str.size() < 10) {
        return std::nullopt;
    }

    std::tm timeStruct{};
    std::istringstream ss{std::string(str)};

    ss >> std::get_time(&timeStruct, "%Y-%m-%d %H:%M:%S");

    if (ss.fail()) {
        ss.clear();
        ss.str(std::string(str));
        ss >> std::get_time(&timeStruct, "%Y/%m/%d %H:%M:%S");

        if (ss.fail()) {
            return std::nullopt;
        }
    }

    if (timeStruct.tm_year < 0 || timeStruct.tm_mon < 0 ||
        timeStruct.tm_mon > 11 || timeStruct.tm_mday < 1 ||
        timeStruct.tm_mday > 31 || timeStruct.tm_hour < 0 ||
        timeStruct.tm_hour > 23 || timeStruct.tm_min < 0 ||
        timeStruct.tm_min > 59 || timeStruct.tm_sec < 0 ||
        timeStruct.tm_sec > 60) {
        return std::nullopt;
    }

    timeStruct.tm_isdst = -1;
    std::time_t timeT = std::mktime(&timeStruct);

    if (timeT == -1) {
        return std::nullopt;
    }

    return std::chrono::system_clock::from_time_t(timeT);
}

void Parser::Impl::parseJson(std::string_view jsonString) const {
    if (jsonString.empty()) {
        THROW_PARSER_ERROR("JSON string cannot be empty");
    }

    try {
        auto jsonObj = json::parse(jsonString);
        spdlog::info("Parsed JSON successfully");
        spdlog::info("JSON structure: {}", jsonObj.dump(2));

        if (jsonObj.is_object()) {
            spdlog::info("JSON contains the following keys:");
            for (const auto& [key, value] : jsonObj.items()) {
                spdlog::info("Key: {}, Type: {}", key, value.type_name());
            }
        }
    } catch (const json::parse_error& e) {
        spdlog::error("JSON parse error at byte {}: {}", e.byte, e.what());
        THROW_PARSER_ERROR(std::string("Failed to parse JSON: ") + e.what());
    } catch (const std::exception& e) {
        THROW_PARSER_ERROR(std::string("JSON processing error: ") + e.what());
    }
}

void Parser::Impl::parseCsv(std::string_view csvString, char delimiter) const {
    if (csvString.empty()) {
        THROW_PARSER_ERROR("CSV string cannot be empty");
    }

    try {
        std::istringstream stream{std::string(csvString)};
        std::string line;
        int lineCount = 0;
        std::vector<std::vector<std::string>> parsedData;

        if (std::getline(stream, line)) {
            auto headers = split(line, delimiter);
            spdlog::info("CSV Headers ({}): ", headers.size());
            for (const auto& header : headers) {
                spdlog::info("  {}", header);
            }

            while (std::getline(stream, line)) {
                lineCount++;
                auto values = split(line, delimiter);

                if (values.size() != headers.size()) {
                    spdlog::warn("Row {} has {} fields, expected {}", lineCount,
                                 values.size(), headers.size());
                }

                parsedData.push_back(std::move(values));
                if (lineCount <= 5) {
                    spdlog::info("Row {}: {}", lineCount, line);
                    for (size_t i = 0; i < values.size() && i < headers.size();
                         ++i) {
                        spdlog::info("  {} = {}", headers[i], values[i]);
                    }
                }
            }
        }

        spdlog::info("CSV parsed successfully. Total rows: {}", lineCount);
    } catch (const std::exception& e) {
        THROW_PARSER_ERROR(std::string("CSV parsing error: ") + e.what());
    }
}

void Parser::Impl::registerCustomParser(std::string_view type,
                                        CustomParserFunc parser) {
    std::lock_guard<std::mutex> lock(parserMutex_);
    customParsers_[std::string(type)] = std::move(parser);
}

void Parser::Impl::printCustomParsers() const {
    std::lock_guard<std::mutex> lock(parserMutex_);
    for (const auto& [type, parserFunc] : customParsers_) {
        spdlog::info("Custom parser for type: {}", type);
    }
}

auto Parser::Impl::parseParallel(const std::vector<std::string>& inputs)
    -> std::vector<std::any> {
    if (inputs.empty()) {
        return {};
    }

    const size_t inputSize = inputs.size();
    std::vector<std::any> results(inputSize);

    const auto hardwareConcurrency = std::thread::hardware_concurrency();
    const auto numThreads = std::min(
        std::max(1u, hardwareConcurrency),
        static_cast<unsigned>(inputSize > 1000 ? 32 : inputSize / 32 + 1));

    spdlog::info("Starting parallel parsing with {} threads for {} inputs",
                 numThreads, inputSize);

    std::vector<std::thread> threads;
    threads.reserve(numThreads);
    std::atomic<size_t> nextIndex = 0;
    std::mutex resultsMutex;

    auto worker = [&]() {
        while (true) {
            size_t index = nextIndex.fetch_add(1, std::memory_order_relaxed);
            if (index >= inputSize) {
                break;
            }

            try {
                auto result = parseLiteral(inputs[index]);

                std::lock_guard<std::mutex> lock(resultsMutex);
                results[index] = result
                                     ? *result
                                     : std::any(std::string("Error parsing: ") +
                                                inputs[index]);
            } catch (const std::exception& e) {
                std::lock_guard<std::mutex> lock(resultsMutex);
                results[index] =
                    std::any(std::string("Exception: ") + e.what());
            }
        }
    };

    for (unsigned i = 0; i < numThreads; ++i) {
        threads.emplace_back(worker);
    }

    for (auto& thread : threads) {
        thread.join();
    }

    spdlog::info("Parallel parsing completed for {} inputs", inputSize);
    return results;
}

template <std::ranges::input_range Range>
    requires std::convertible_to<std::ranges::range_value_t<Range>,
                                 std::string_view>
auto Parser::Impl::convertToAnyVector(const Range& input)
    -> std::vector<std::any> {
    const size_t estimatedSize = std::distance(input.begin(), input.end());
    std::vector<std::any> result;
    result.reserve(estimatedSize);

    if (estimatedSize > 100) {
        std::vector<std::string> inputStrings;
        inputStrings.reserve(estimatedSize);

        for (const auto& item : input) {
            inputStrings.emplace_back(item);
        }

        return parseParallel(inputStrings);
    }

    for (const auto& str : input) {
        try {
            auto parsedValue = parseLiteral(str);
            if (parsedValue) {
                result.push_back(*parsedValue);
            } else {
                result.emplace_back(std::string("Invalid input: ") +
                                    std::string(str));
            }
        } catch (const std::exception& e) {
            spdlog::warn("Error parsing '{}': {}", str, e.what());
            result.emplace_back(std::string("Error: ") + e.what());
        }
    }

    return result;
}

}  // namespace atom::utils
