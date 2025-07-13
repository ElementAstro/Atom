#pragma once

#include <functional>
#include <regex>
#include <string>
#include <unordered_map>
#include <vector>

namespace dotenv {

/**
 * @brief Configuration options for the parser
 */
struct ParseOptions {
    bool ignore_comments = true;
    bool trim_whitespace = true;
    bool expand_variables = true;
    bool allow_empty_values = true;
    char comment_char = '#';
    std::string encoding = "utf-8";
};

/**
 * @brief Represents a parsed environment variable entry
 */
struct EnvEntry {
    std::string key;
    std::string value;
    std::string original_line;
    size_t line_number;
    bool is_quoted = false;
    char quote_type = '\0';  // '"' or '\''
};

/**
 * @brief Modern C++ parser for .env files with comprehensive feature support
 */
class Parser {
public:
    using EnvMap = std::unordered_map<std::string, std::string>;
    using EnvEntries = std::vector<EnvEntry>;
    using VariableExpander = std::function<std::string(const std::string&)>;

    /**
     * @brief Construct parser with options
     */
    explicit Parser(const ParseOptions& options = ParseOptions{});

    /**
     * @brief Parse content from string
     * @param content The .env file content
     * @return Parsed environment variables
     */
    EnvMap parse(const std::string& content);

    /**
     * @brief Parse content and return detailed entries
     * @param content The .env file content
     * @return Detailed parsing results
     */
    EnvEntries parseDetailed(const std::string& content);

    /**
     * @brief Set custom variable expander
     * @param expander Function to expand variables
     */
    void setVariableExpander(VariableExpander expander);

private:
    ParseOptions options_;
    VariableExpander variable_expander_;
    static const std::regex variable_pattern_;
    static const std::regex quoted_pattern_;

    std::string processLine(const std::string& line, size_t line_number);
    std::pair<std::string, std::string> parseLine(const std::string& line);
    std::string expandVariables(const std::string& value,
                                const EnvMap& existing_vars);
    std::string unquote(const std::string& value, char& quote_type);
    std::string trim(const std::string& str);
    bool isComment(const std::string& line);
    bool isEmpty(const std::string& line);
};

}  // namespace dotenv
