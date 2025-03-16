/*!
 * \file signature.hpp
 * \brief Enhanced signature parsing with C++20/23 features
 * \author Max Qian <lightapt.com>, Enhanced by Claude
 * \date 2024-6-7, Updated 2025-3-13
 */

#ifndef ATOM_META_SIGNATURE_HPP
#define ATOM_META_SIGNATURE_HPP

#include <expected>  // For type::expected (C++23)
#include <mutex>
#include <optional>       // For std::optional
#include <span>           // For std::span (C++20)
#include <string>         // For std::string
#include <string_view>    // For std::string_view
#include <unordered_map>  // For std::unordered_map
#include <vector>         // For std::vector

#include "atom/type/expected.hpp"
#include "atom/utils/cstring.hpp"

namespace atom::meta {

// Error handling with rich information
enum class ParsingErrorCode {
    InvalidPrefix,
    MissingFunctionName,
    MissingOpenParenthesis,
    MissingCloseParenthesis,
    MalformedParameters,
    MalformedReturnType,
    UnbalancedBrackets,
    InternalError
};

struct ParsingError {
    ParsingErrorCode code;
    std::string message;
    size_t position{0};
};

// Modifiers for function signatures with type safety
enum class FunctionModifier {
    None,
    Const,
    Noexcept,
    ConstNoexcept,
    Virtual,
    Override,
    Final
};

// Documentation comment with structured information
struct DocComment {
    std::string_view raw;
    std::unordered_map<std::string_view, std::string_view> tags;

    [[nodiscard]] bool hasTag(std::string_view tag) const noexcept {
        return tags.contains(tag);
    }

    [[nodiscard]] std::optional<std::string_view> getTag(
        std::string_view tag) const noexcept {
        if (auto it = tags.find(tag); it != tags.end()) {
            return it->second;
        }
        return std::nullopt;
    }
};

// Parameter with rich type information
struct Parameter {
    std::string_view name;
    std::string_view type;
    bool hasDefaultValue{false};
    std::optional<std::string_view> defaultValue;

    // C++20 default comparison operators
    auto operator<=>(const Parameter&) const = default;
};

// Enhanced function signature with comprehensive metadata
class FunctionSignature {
public:
    // Constructor with designated initializers support (C++20)
    constexpr FunctionSignature(
        std::string_view name, std::span<const Parameter> parameters,
        std::optional<std::string_view> returnType,
        FunctionModifier modifiers = FunctionModifier::None,
        std::optional<DocComment> docComment = std::nullopt,
        bool isTemplated = false,
        std::optional<std::string_view> templateParams = std::nullopt,
        bool isInline = false, bool isStatic = false,
        bool isExplicit = false) noexcept
        : name_(name),
          parameters_(parameters.begin(), parameters.end()),
          returnType_(returnType),
          modifiers_(modifiers),
          docComment_(docComment),
          isTemplated_(isTemplated),
          templateParams_(templateParams),
          isInline_(isInline),
          isStatic_(isStatic),
          isExplicit_(isExplicit) {}

    // Rule of five for proper resource management
    FunctionSignature(const FunctionSignature&) = default;
    FunctionSignature& operator=(const FunctionSignature&) = default;
    FunctionSignature(FunctionSignature&&) noexcept = default;
    FunctionSignature& operator=(FunctionSignature&&) noexcept = default;
    ~FunctionSignature() = default;

    // Enhanced getters with nodiscard attribute
    [[nodiscard]] constexpr auto getName() const noexcept -> std::string_view {
        return name_;
    }
    [[nodiscard]] constexpr auto getParameters() const noexcept
        -> std::span<const Parameter> {
        return parameters_;
    }
    [[nodiscard]] constexpr auto getReturnType() const noexcept
        -> std::optional<std::string_view> {
        return returnType_;
    }
    [[nodiscard]] constexpr auto getModifiers() const noexcept
        -> FunctionModifier {
        return modifiers_;
    }
    [[nodiscard]] constexpr auto getDocComment() const noexcept
        -> const std::optional<DocComment>& {
        return docComment_;
    }

    // Additional getters for enhanced metadata
    [[nodiscard]] constexpr bool isTemplated() const noexcept {
        return isTemplated_;
    }
    [[nodiscard]] constexpr auto getTemplateParameters() const noexcept
        -> std::optional<std::string_view> {
        return templateParams_;
    }
    [[nodiscard]] constexpr bool isInline() const noexcept { return isInline_; }
    [[nodiscard]] constexpr bool isStatic() const noexcept { return isStatic_; }
    [[nodiscard]] constexpr bool isExplicit() const noexcept {
        return isExplicit_;
    }

    // String representation
    [[nodiscard]] std::string toString() const {
        std::string result;

        // Add modifiers
        if (isStatic_)
            result += "static ";
        if (isInline_)
            result += "inline ";
        if (isExplicit_)
            result += "explicit ";

        // Add return type if available
        if (returnType_) {
            result += std::string(*returnType_);
            result += " ";
        }

        // Add name
        result += std::string(name_);

        // Add parameters
        result += "(";
        bool first = true;
        for (const auto& param : parameters_) {
            if (!first)
                result += ", ";
            first = false;

            result += std::string(param.name);
            if (!param.type.empty()) {
                result += ": ";
                result += std::string(param.type);
            }

            if (param.hasDefaultValue && param.defaultValue) {
                result += " = ";
                result += std::string(*param.defaultValue);
            }
        }
        result += ")";

        // Add function modifiers
        switch (modifiers_) {
            case FunctionModifier::Const:
                result += " const";
                break;
            case FunctionModifier::Noexcept:
                result += " noexcept";
                break;
            case FunctionModifier::ConstNoexcept:
                result += " const noexcept";
                break;
            case FunctionModifier::Virtual:
                result += " virtual";
                break;
            case FunctionModifier::Override:
                result += " override";
                break;
            case FunctionModifier::Final:
                result += " final";
                break;
            default:
                break;
        }

        return result;
    }

private:
    std::string_view name_;
    std::vector<Parameter> parameters_;
    std::optional<std::string_view> returnType_;
    FunctionModifier modifiers_{FunctionModifier::None};
    std::optional<DocComment> docComment_;
    bool isTemplated_{false};
    std::optional<std::string_view> templateParams_;
    bool isInline_{false};
    bool isStatic_{false};
    bool isExplicit_{false};
};

// Parse doc comments into structured format
[[nodiscard]] inline auto parseDocComment(std::string_view comment)
    -> DocComment {
    DocComment result{comment, {}};

    // Skip the opening /**
    size_t pos = comment.find("/**");
    if (pos != std::string_view::npos) {
        pos += 3;
    } else {
        return result;
    }

    // Find tags (e.g., @param, @return, @brief)
    while (pos < comment.size()) {
        pos = comment.find('@', pos);
        if (pos == std::string_view::npos || pos + 1 >= comment.size()) {
            break;
        }

        // Extract tag name
        size_t tagStart = pos + 1;
        size_t tagEnd = comment.find_first_of(" \t\n\r", tagStart);
        if (tagEnd == std::string_view::npos)
            break;

        std::string_view tagName = comment.substr(tagStart, tagEnd - tagStart);

        // Extract tag value
        size_t valueStart = comment.find_first_not_of(" \t\n\r", tagEnd);
        if (valueStart == std::string_view::npos)
            break;

        size_t valueEnd = comment.find('@', valueStart);
        if (valueEnd == std::string_view::npos) {
            valueEnd = comment.find("*/", valueStart);
            if (valueEnd == std::string_view::npos) {
                valueEnd = comment.size();
            }
        }

        std::string_view tagValue = atom::utils::trim(
            comment.substr(valueStart, valueEnd - valueStart));
        result.tags[tagName] = tagValue;

        pos = valueEnd;
    }

    return result;
}

// Parse function definition with robust error handling
[[nodiscard]] inline constexpr auto parseFunctionDefinition(
    const std::string_view definition) noexcept
    -> type::expected<FunctionSignature, ParsingError> {
    using enum ParsingErrorCode;

    // Constants for parsing
    constexpr std::string_view DEF_PREFIX = "def ";
    constexpr std::string_view ARROW = " -> ";
    constexpr std::string_view CONST_MODIFIER = " const";
    constexpr std::string_view NOEXCEPT_MODIFIER = " noexcept";
    constexpr std::string_view VIRTUAL_MODIFIER = "virtual ";
    constexpr std::string_view OVERRIDE_MODIFIER = " override";
    constexpr std::string_view FINAL_MODIFIER = " final";
    constexpr std::string_view INLINE_MODIFIER = "inline ";
    constexpr std::string_view STATIC_MODIFIER = "static ";
    constexpr std::string_view EXPLICIT_MODIFIER = "explicit ";
    constexpr std::string_view TEMPLATE_PREFIX = "template<";

    // Check for valid prefix
    if (!definition.starts_with(DEF_PREFIX)) {
        return type::unexpected(ParsingError{
            InvalidPrefix, "Function definition must start with 'def '", 0});
    }

    // Check for template
    bool isTemplated = false;
    std::optional<std::string_view> templateParams;
    size_t startPos = 0;

    if (definition.find(TEMPLATE_PREFIX) == 0) {
        isTemplated = true;
        size_t templateEnd = definition.find(">", TEMPLATE_PREFIX.size());
        if (templateEnd != std::string_view::npos) {
            templateParams = definition.substr(
                TEMPLATE_PREFIX.size(), templateEnd - TEMPLATE_PREFIX.size());
            startPos = templateEnd + 1;

            // Find the "def" after template declaration
            startPos = definition.find(DEF_PREFIX, startPos);
            if (startPos == std::string_view::npos) {
                return type::unexpected(ParsingError{
                    InvalidPrefix,
                    "Cannot find 'def' after template declaration", 0});
            }
        }
    }

    // Parse function modifiers
    bool isInline = definition.find(INLINE_MODIFIER) != std::string_view::npos;
    bool isStatic = definition.find(STATIC_MODIFIER) != std::string_view::npos;
    bool isExplicit =
        definition.find(EXPLICIT_MODIFIER) != std::string_view::npos;

    // Find function name
    size_t nameStart = DEF_PREFIX.size();
    size_t nameEnd = definition.find('(', nameStart);
    if (nameEnd == std::string_view::npos) {
        return type::unexpected(ParsingError{
            MissingOpenParenthesis,
            "Cannot find opening parenthesis in function definition",
            nameStart});
    }

    if (nameEnd == nameStart) {
        return type::unexpected(ParsingError{
            MissingFunctionName, "Function name is missing", nameStart});
    }

    std::string_view name =
        atom::utils::trim(definition.substr(nameStart, nameEnd - nameStart));

    // Parse parameters
    size_t paramsStart = nameEnd + 1;
    size_t paramsEnd = definition.find(')', paramsStart);
    if (paramsEnd == std::string_view::npos) {
        return type::unexpected(ParsingError{
            MissingCloseParenthesis,
            "Cannot find closing parenthesis in function definition",
            paramsStart});
    }

    std::string_view paramsStr =
        definition.substr(paramsStart, paramsEnd - paramsStart);

    // Parse return type
    size_t arrowPos = definition.find(ARROW, paramsEnd + 1);
    std::optional<std::string_view> returnType;
    if (arrowPos != std::string_view::npos) {
        returnType =
            atom::utils::trim(definition.substr(arrowPos + ARROW.size()));
    }

    // Parse function modifiers
    FunctionModifier modifiers = FunctionModifier::None;
    if (definition.find(CONST_MODIFIER) != std::string_view::npos &&
        definition.find(NOEXCEPT_MODIFIER) != std::string_view::npos) {
        modifiers = FunctionModifier::ConstNoexcept;
    } else if (definition.find(CONST_MODIFIER) != std::string_view::npos) {
        modifiers = FunctionModifier::Const;
    } else if (definition.find(NOEXCEPT_MODIFIER) != std::string_view::npos) {
        modifiers = FunctionModifier::Noexcept;
    } else if (definition.find(VIRTUAL_MODIFIER) != std::string_view::npos) {
        modifiers = FunctionModifier::Virtual;
    } else if (definition.find(OVERRIDE_MODIFIER) != std::string_view::npos) {
        modifiers = FunctionModifier::Override;
    } else if (definition.find(FINAL_MODIFIER) != std::string_view::npos) {
        modifiers = FunctionModifier::Final;
    }

    // Parse parameters with enhanced error handling
    std::vector<Parameter> parameters;
    size_t paramStart = 0;

    while (paramStart < paramsStr.size()) {
        size_t paramEnd = paramsStr.size();
        int bracketCount = 0;
        int angleCount = 0;  // For handling template parameters like vector<T>

        for (size_t i = paramStart; i < paramsStr.size(); ++i) {
            char c = paramsStr[i];

            // Update bracket counts
            if (c == '[')
                ++bracketCount;
            else if (c == ']') {
                if (bracketCount == 0) {
                    return type::unexpected(
                        ParsingError{UnbalancedBrackets,
                                     "Unbalanced square brackets in parameters",
                                     paramsStart + i});
                }
                --bracketCount;
            } else if (c == '<')
                ++angleCount;
            else if (c == '>') {
                if (angleCount == 0) {
                    return type::unexpected(
                        ParsingError{UnbalancedBrackets,
                                     "Unbalanced angle brackets in parameters",
                                     paramsStart + i});
                }
                --angleCount;
            }

            // Find parameter separator when not inside a bracket/template
            if (c == ',' && bracketCount == 0 && angleCount == 0) {
                paramEnd = i;
                break;
            }
        }

        // Check for unbalanced brackets
        if (bracketCount != 0 || angleCount != 0) {
            return type::unexpected(
                ParsingError{UnbalancedBrackets,
                             "Unbalanced brackets in parameters", paramsStart});
        }

        std::string_view param = atom::utils::trim(
            paramsStr.substr(paramStart, paramEnd - paramStart));
        if (param.empty()) {
            // Skip empty parameters
            paramStart = paramEnd + 1;
            continue;
        }

        // Parse parameter with name, type, and default value
        Parameter parameter;

        // Check for default value
        size_t equalsPos = param.find('=');
        if (equalsPos != std::string_view::npos) {
            parameter.hasDefaultValue = true;
            parameter.defaultValue =
                atom::utils::trim(param.substr(equalsPos + 1));
            param = atom::utils::trim(param.substr(0, equalsPos));
        }

        // Parse name and type
        size_t colonPos = param.find(':');
        if (colonPos != std::string_view::npos) {
            parameter.name = atom::utils::trim(param.substr(0, colonPos));
            parameter.type = atom::utils::trim(param.substr(colonPos + 1));
        } else {
            parameter.name = param;
            parameter.type = "any";  // Default type
        }

        parameters.push_back(parameter);
        paramStart = paramEnd + 1;
    }

    // Parse doc comment if available
    std::optional<DocComment> docComment;
    size_t docStart = definition.find("/**");
    if (docStart != std::string_view::npos) {
        size_t docEnd = definition.find("*/", docStart);
        if (docEnd != std::string_view::npos) {
            docComment = parseDocComment(
                definition.substr(docStart, docEnd - docStart + 2));
        }
    }

    // Create and return the function signature
    return FunctionSignature(name, parameters, returnType, modifiers,
                             docComment, isTemplated, templateParams, isInline,
                             isStatic, isExplicit);
}

// Signature registry for caching and managing function signatures
class SignatureRegistry {
public:
    // Thread-safe singleton pattern
    static SignatureRegistry& instance() {
        static SignatureRegistry instance;
        return instance;
    }

    // Register a function signature
    template <typename Signature>
        requires std::is_convertible_v<Signature, std::string_view>
    auto registerSignature(Signature&& signature)
        -> type::expected<FunctionSignature, ParsingError> {
        std::string_view sigView = signature;
        std::lock_guard lock(mutex_);

        // Check cache first
        if (auto it = cache_.find(std::string(sigView)); it != cache_.end()) {
            return it->second;
        }

        // Parse and cache the result
        auto result = parseFunctionDefinition(sigView);
        if (result) {
            cache_[std::string(sigView)] = result.value();
        }
        return result;
    }

    // Clear the cache
    void clearCache() {
        std::lock_guard lock(mutex_);
        cache_.clear();
    }

    // Get statistics
    [[nodiscard]] size_t getCacheSize() const {
        std::lock_guard lock(mutex_);
        return cache_.size();
    }

private:
    SignatureRegistry() = default;
    ~SignatureRegistry() = default;
    SignatureRegistry(const SignatureRegistry&) = delete;
    SignatureRegistry& operator=(const SignatureRegistry&) = delete;

    mutable std::mutex mutex_;
    std::unordered_map<std::string, FunctionSignature> cache_;
};

}  // namespace atom::meta

#endif  // ATOM_META_SIGNATURE_HPP
