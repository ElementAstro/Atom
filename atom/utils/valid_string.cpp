#include "valid_string.hpp"

#include <stack>
#include <string>
#include <unordered_map>
#include <vector>

#ifdef ATOM_USE_BOOST
#include <boost/algorithm/string.hpp>
#include <boost/format.hpp>
#include <boost/unordered_map.hpp>
#endif

namespace atom::utils {

auto isValidBracket(const std::string& str) -> ValidationResult {
    std::stack<BracketInfo> stack;

#ifdef ATOM_USE_BOOST
    boost::unordered_map<char, char> brackets = {
        {')', '('}, {']', '['}, {'}', '{'}, {'>', '<'}};
#else
    std::unordered_map<char, char> brackets = {
        {')', '('}, {']', '['}, {'}', '{'}, {'>', '<'}};
#endif

    ValidationResult result;
    result.isValid = true;

    bool singleQuoteOpen = false;
    bool doubleQuoteOpen = false;

    for (std::string::size_type i = 0; i < str.size(); ++i) {
        char current = str[i];

        if (current == '\'' && !doubleQuoteOpen) {
            singleQuoteOpen = !singleQuoteOpen;
            continue;
        }

        if (current == '\"' && !singleQuoteOpen) {
            doubleQuoteOpen = !doubleQuoteOpen;
            continue;
        }

        if (singleQuoteOpen || doubleQuoteOpen) {
            continue;
        }

#ifdef ATOM_USE_BOOST
        if (!boost::contains(brackets, current)) {
#else
        if (brackets.find(current) == brackets.end()) {
#endif
            // Push opening brackets onto the stack
            stack.push({current, static_cast<int>(i)});
        } else {
            // Check for matching closing brackets
            if (stack.empty() || stack.top().character != brackets[current]) {
#ifdef ATOM_USE_BOOST
                result.invalidBrackets.emplace_back(
                    BracketInfo{current, static_cast<int>(i)});
                result.errorMessages.emplace_back(boost::str(
                    boost::format("Error: Closing bracket '%c' at position %d "
                                  "has no matching opening bracket.") %
                    current % i));
#else
                result.invalidBrackets.push_back(
                    BracketInfo{current, static_cast<int>(i)});
                result.errorMessages.push_back(
                    "Error: Closing bracket '" + std::string(1, current) +
                    "' at position " + std::to_string(i) +
                    " has no matching opening bracket.");
#endif
                result.isValid = false;
            } else {
                stack.pop();
            }
        }
    }

    // Handle unmatched opening brackets
    while (!stack.empty()) {
        auto top = stack.top();
        stack.pop();
        result.invalidBrackets.push_back(top);
#ifdef ATOM_USE_BOOST
        result.errorMessages.emplace_back(
            boost::str(boost::format("Error: Opening bracket '%c' at position "
                                     "%d needs a closing bracket.") %
                       top.character % top.position));
#else
        result.errorMessages.push_back(
            "Error: Opening bracket '" + std::string(1, top.character) +
            "' at position " + std::to_string(top.position) +
            " needs a closing bracket.");
#endif
        result.isValid = false;
    }

    // Handle unmatched quotes
    if (singleQuoteOpen) {
#ifdef ATOM_USE_BOOST
        result.errorMessages.emplace_back("Error: Single quote is not closed.");
#else
        result.errorMessages.emplace_back("Error: Single quote is not closed.");
#endif
        result.isValid = false;
    }

    if (doubleQuoteOpen) {
#ifdef ATOM_USE_BOOST
        result.errorMessages.emplace_back("Error: Double quote is not closed.");
#else
        result.errorMessages.emplace_back("Error: Double quote is not closed.");
#endif
        result.isValid = false;
    }

    return result;
}

}  // namespace atom::utils