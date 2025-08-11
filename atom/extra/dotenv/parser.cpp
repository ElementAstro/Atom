#include "parser.hpp"

#include <cctype>
#include <sstream>
#include "exceptions.hpp"

namespace dotenv {

const std::regex Parser::variable_pattern_{
    R"(\$\{([^}]+)\}|\$([A-Za-z_][A-Za-z0-9_]*))"};
const std::regex Parser::quoted_pattern_{R"(^(["'])(.*)\1$)"};

Parser::Parser(const ParseOptions& options) : options_(options) {
    // Default variable expander using system environment
    variable_expander_ = [](const std::string& var_name) -> std::string {
        const char* value = std::getenv(var_name.c_str());
        return value ? std::string(value) : std::string{};
    };
}

Parser::EnvMap Parser::parse(const std::string& content) {
    EnvMap result;
    std::istringstream stream(content);
    std::string line;
    size_t line_number = 0;

    while (std::getline(stream, line)) {
        ++line_number;

        try {
            std::string processed_line = processLine(line, line_number);
            if (processed_line.empty())
                continue;

            auto [key, value] = parseLine(processed_line);
            if (!key.empty()) {
                if (options_.expand_variables) {
                    value = expandVariables(value, result);
                }
                result[key] = value;
            }
        } catch (const std::exception& e) {
            throw ParseException(e.what(), line_number);
        }
    }

    return result;
}

Parser::EnvEntries Parser::parseDetailed(const std::string& content) {
    EnvEntries result;
    std::istringstream stream(content);
    std::string line;
    size_t line_number = 0;

    while (std::getline(stream, line)) {
        ++line_number;

        if (isComment(line) || isEmpty(line))
            continue;

        try {
            auto [key, value] = parseLine(line);
            if (!key.empty()) {
                EnvEntry entry;
                entry.key = key;
                entry.value = value;
                entry.original_line = line;
                entry.line_number = line_number;

                // Detect if value was quoted
                std::smatch match;
                if (std::regex_match(value, match, quoted_pattern_)) {
                    entry.is_quoted = true;
                    entry.quote_type = match[1].str()[0];
                }

                result.push_back(std::move(entry));
            }
        } catch (const std::exception& e) {
            throw ParseException(e.what(), line_number);
        }
    }

    return result;
}

std::string Parser::processLine(const std::string& line, size_t line_number) {
    if (isComment(line) || isEmpty(line)) {
        return "";
    }

    std::string processed = line;
    if (options_.trim_whitespace) {
        processed = trim(processed);
    }

    return processed;
}

std::pair<std::string, std::string> Parser::parseLine(const std::string& line) {
    size_t equals_pos = line.find('=');
    if (equals_pos == std::string::npos) {
        if (!options_.allow_empty_values) {
            throw ParseException("Missing '=' in line: " + line);
        }
        return {trim(line), ""};
    }

    std::string key = line.substr(0, equals_pos);
    std::string value = line.substr(equals_pos + 1);

    if (options_.trim_whitespace) {
        key = trim(key);
        value = trim(value);
    }

    // Validate key format
    if (key.empty() || !std::isalpha(key[0]) && key[0] != '_') {
        throw ParseException("Invalid variable name: " + key);
    }

    char quote_type;
    value = unquote(value, quote_type);

    return {key, value};
}

std::string Parser::expandVariables(const std::string& value,
                                    const EnvMap& existing_vars) {
    if (!options_.expand_variables) {
        return value;
    }

    std::string result = value;
    std::smatch match;

    while (std::regex_search(result, match, variable_pattern_)) {
        std::string var_name =
            match[1].matched ? match[1].str() : match[2].str();
        std::string replacement;

        // First check existing parsed variables
        auto it = existing_vars.find(var_name);
        if (it != existing_vars.end()) {
            replacement = it->second;
        } else if (variable_expander_) {
            replacement = variable_expander_(var_name);
        }

        result.replace(match.position(), match.length(), replacement);
    }

    return result;
}

std::string Parser::unquote(const std::string& value, char& quote_type) {
    quote_type = '\0';

    if (value.length() < 2) {
        return value;
    }

    char first = value.front();
    char last = value.back();

    if ((first == '"' || first == '\'') && first == last) {
        quote_type = first;
        std::string unquoted = value.substr(1, value.length() - 2);

        // Handle escape sequences for double quotes
        if (first == '"') {
            std::string result;
            for (size_t i = 0; i < unquoted.length(); ++i) {
                if (unquoted[i] == '\\' && i + 1 < unquoted.length()) {
                    char next = unquoted[i + 1];
                    switch (next) {
                        case 'n':
                            result += '\n';
                            ++i;
                            break;
                        case 't':
                            result += '\t';
                            ++i;
                            break;
                        case 'r':
                            result += '\r';
                            ++i;
                            break;
                        case '\\':
                            result += '\\';
                            ++i;
                            break;
                        case '"':
                            result += '"';
                            ++i;
                            break;
                        default:
                            result += unquoted[i];
                            break;
                    }
                } else {
                    result += unquoted[i];
                }
            }
            return result;
        }

        return unquoted;
    }

    return value;
}

std::string Parser::trim(const std::string& str) {
    auto start = str.find_first_not_of(" \t\r\n");
    if (start == std::string::npos)
        return "";

    auto end = str.find_last_not_of(" \t\r\n");
    return str.substr(start, end - start + 1);
}

bool Parser::isComment(const std::string& line) {
    if (!options_.ignore_comments)
        return false;

    std::string trimmed = trim(line);
    return !trimmed.empty() && trimmed[0] == options_.comment_char;
}

bool Parser::isEmpty(const std::string& line) { return trim(line).empty(); }

void Parser::setVariableExpander(VariableExpander expander) {
    variable_expander_ = std::move(expander);
}

}  // namespace dotenv
