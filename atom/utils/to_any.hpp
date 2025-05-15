#ifndef ATOM_UTILS_TO_ANY_HPP
#define ATOM_UTILS_TO_ANY_HPP

#include <any>
#include <atomic>
#include <concepts>
#include <functional>
#include <memory>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <vector>

#include "atom/error/exception.hpp"

namespace atom::utils {
class ParserException : public atom::error::RuntimeError {
public:
    using atom::error::RuntimeError::RuntimeError;
};

#define THROW_PARSER_ERROR(...)                                           \
    throw ParserException(ATOM_FILE_NAME, ATOM_FILE_LINE, ATOM_FUNC_NAME, \
                          __VA_ARGS__)

// Numeric concept for type constraints
template <typename T>
concept Numeric = std::integral<T> || std::floating_point<T>;

// String-like concept
template <typename T>
concept StringLike = std::convertible_to<T, std::string_view>;

/**
 * @class Parser
 * @brief A class that provides various parsing functionalities with C++20
 * features.
 *
 * The Parser class offers methods to parse literals, JSON, CSV, and custom
 * types with enhanced type safety using concepts. It also allows registering
 * custom parsers and provides utility functions for printing and logging parsed
 * values.
 */
class Parser {
public:
    /**
     * @brief Type alias for a custom parser function.
     *
     * A custom parser function takes a string_view as input and returns an
     * optional std::any for efficiency.
     */
    using CustomParserFunc =
        std::function<std::optional<std::any>(std::string_view)>;

    /**
     * @brief Constructs a new Parser object.
     */
    Parser();

    /**
     * @brief Destroys the Parser object.
     */
    ~Parser();

    /**
     * @brief Parses a literal string into an std::any type.
     *
     * @param input The input string to parse.
     * @return An optional std::any containing the parsed value.
     * @throws ParserException if the parser is already processing or input is
     * invalid.
     */
    auto parseLiteral(std::string_view input) -> std::optional<std::any>;

    /**
     * @brief Parses a literal string into an std::any type with a default
     * value.
     *
     * @param input The input string to parse.
     * @param defaultValue The default value to return if parsing fails.
     * @return The parsed value or the default value if parsing fails.
     * @throws ParserException if the parser is already processing.
     */
    auto parseLiteralWithDefault(std::string_view input,
                                 const std::any& defaultValue) -> std::any;

    /**
     * @brief Prints the given std::any value.
     *
     * @param value The value to print.
     */
    void print(const std::any& value) const;

    /**
     * @brief Logs the parsing result.
     *
     * @param input The input string that was parsed.
     * @param result The result of the parsing.
     */
    void logParsing(std::string_view input, const std::any& result) const;

    /**
     * @brief Converts a range of strings to a vector of std::any types.
     *
     * @tparam Range Type that satisfies std::ranges::input_range
     * @param input The range of strings to convert.
     * @return A vector of std::any containing the converted values.
     */
    template <std::ranges::input_range Range>
        requires std::convertible_to<std::ranges::range_value_t<Range>,
                                     std::string_view>
    auto convertToAnyVector(const Range& input) -> std::vector<std::any>;

    /**
     * @brief Registers a custom parser for a specific type.
     *
     * @param type The type for which the custom parser is registered.
     * @param parser The custom parser function.
     * @throws ParserException if the type is empty or parser is null.
     */
    void registerCustomParser(std::string_view type, CustomParserFunc parser);

    /**
     * @brief Prints the registered custom parsers.
     */
    void printCustomParsers() const;

    /**
     * @brief Parses a JSON string.
     *
     * @param jsonString The JSON string to parse.
     * @throws ParserException if JSON parsing fails.
     */
    void parseJson(std::string_view jsonString) const;

    /**
     * @brief Parses a CSV string.
     *
     * @param csvString The CSV string to parse.
     * @param delimiter The delimiter used in the CSV string. Default is ','.
     * @throws ParserException if CSV parsing fails.
     */
    void parseCsv(std::string_view csvString, char delimiter = ',') const;

    /**
     * @brief Performs parallel parsing of multiple inputs.
     *
     * @param inputs Vector of string inputs to parse in parallel.
     * @return Vector of parsed values.
     */
    auto parseParallel(const std::vector<std::string>& inputs)
        -> std::vector<std::any>;

private:
    class Impl;  ///< Forward declaration of the implementation class.
    std::unique_ptr<Impl> pImpl_;  ///< Pointer to the implementation.
    std::atomic<bool> isProcessing_{
        false};  ///< Atomic flag indicating if processing is ongoing.
};
}  // namespace atom::utils

#endif
