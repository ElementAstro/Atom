/*!
 * \file signature.hpp
 * \brief Enhanced signature parsing with C++20/23 features
 * \author Max Qian <lightapt.com>, Enhanced by Claude
 * \date 2024-6-7, Updated 2025-3-13
 */

#ifndef ATOM_META_SIGNATURE_HPP
#define ATOM_META_SIGNATURE_HPP

#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "atom/type/expected.hpp"
#include "atom/utils/cstring.hpp"

namespace atom::meta {

/**
 * @brief Error codes for signature parsing operations
 */
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

/**
 * @brief Detailed parsing error information
 */
struct ParsingError {
    ParsingErrorCode code;
    std::string message;
    size_t position{0};
};

/**
 * @brief Function modifiers with type safety
 */
enum class FunctionModifier {
    None,
    Const,
    Noexcept,
    ConstNoexcept,
    Virtual,
    Override,
    Final
};

/**
 * @brief Documentation comment with structured information
 */
struct DocComment {
    std::string_view raw;
    std::unordered_map<std::string_view, std::string_view> tags;

    /**
     * @brief Check if a tag exists
     * @param tag Tag name to check
     * @return True if tag exists
     */
    [[nodiscard]] bool hasTag(std::string_view tag) const noexcept {
        return tags.contains(tag);
    }

    /**
     * @brief Get tag value
     * @param tag Tag name
     * @return Tag value if exists
     */
    [[nodiscard]] std::optional<std::string_view> getTag(
        std::string_view tag) const noexcept {
        if (auto it = tags.find(tag); it != tags.end()) {
            return it->second;
        }
        return std::nullopt;
    }
};

/**
 * @brief Parameter with type information
 */
struct Parameter {
    std::string_view name;
    std::string_view type;
    bool hasDefaultValue{false};
    std::optional<std::string_view> defaultValue;

    auto operator<=>(const Parameter&) const = default;
};

/**
 * @brief Enhanced function signature with comprehensive metadata
 */
class FunctionSignature {
public:
    /**
     * @brief Construct function signature
     * @param name Function name
     * @param parameters Function parameters
     * @param returnType Return type (optional)
     * @param modifiers Function modifiers
     * @param docComment Documentation comment
     * @param isTemplated Whether function is templated
     * @param templateParams Template parameters
     * @param isInline Whether function is inline
     * @param isStatic Whether function is static
     * @param isExplicit Whether constructor is explicit
     */
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

    FunctionSignature(const FunctionSignature&) = default;
    FunctionSignature& operator=(const FunctionSignature&) = default;
    FunctionSignature(FunctionSignature&&) noexcept = default;
    FunctionSignature& operator=(FunctionSignature&&) noexcept = default;
    ~FunctionSignature() = default;

    /**
     * @brief Get function name
     * @return Function name
     */
    [[nodiscard]] constexpr auto getName() const noexcept -> std::string_view {
        return name_;
    }

    /**
     * @brief Get function parameters
     * @return Span of parameters
     */
    [[nodiscard]] constexpr auto getParameters() const noexcept
        -> std::span<const Parameter> {
        return parameters_;
    }

    /**
     * @brief Get return type
     * @return Return type if specified
     */
    [[nodiscard]] constexpr auto getReturnType() const noexcept
        -> std::optional<std::string_view> {
        return returnType_;
    }

    /**
     * @brief Get function modifiers
     * @return Function modifiers
     */
    [[nodiscard]] constexpr auto getModifiers() const noexcept
        -> FunctionModifier {
        return modifiers_;
    }

    /**
     * @brief Get documentation comment
     * @return Documentation comment if available
     */
    [[nodiscard]] constexpr auto getDocComment() const noexcept
        -> const std::optional<DocComment>& {
        return docComment_;
    }

    /**
     * @brief Check if function is templated
     * @return True if templated
     */
    [[nodiscard]] constexpr bool isTemplated() const noexcept {
        return isTemplated_;
    }

    /**
     * @brief Get template parameters
     * @return Template parameters if available
     */
    [[nodiscard]] constexpr auto getTemplateParameters() const noexcept
        -> std::optional<std::string_view> {
        return templateParams_;
    }

    /**
     * @brief Check if function is inline
     * @return True if inline
     */
    [[nodiscard]] constexpr bool isInline() const noexcept { return isInline_; }

    /**
     * @brief Check if function is static
     * @return True if static
     */
    [[nodiscard]] constexpr bool isStatic() const noexcept { return isStatic_; }

    /**
     * @brief Check if constructor is explicit
     * @return True if explicit
     */
    [[nodiscard]] constexpr bool isExplicit() const noexcept {
        return isExplicit_;
    }

    /**
     * @brief Convert signature to string representation
     * @return String representation of the signature
     */
    [[nodiscard]] std::string toString() const {
        std::string result;

        if (isStatic_)
            result += "static ";
        if (isInline_)
            result += "inline ";
        if (isExplicit_)
            result += "explicit ";

        if (returnType_) {
            result += std::string(*returnType_);
            result += " ";
        }

        result += std::string(name_);

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

/**
 * @brief Parse documentation comment into structured format
 * @param comment Raw comment string
 * @return Parsed documentation comment
 */
[[nodiscard]] inline auto parseDocComment(std::string_view comment)
    -> DocComment {
    DocComment result{comment, {}};

    size_t pos = comment.find("/**");
    if (pos == std::string_view::npos) {
        return result;
    }
    pos += 3;

    while (pos < comment.size()) {
        pos = comment.find('@', pos);
        if (pos == std::string_view::npos || pos + 1 >= comment.size()) {
            break;
        }

        size_t tagStart = pos + 1;
        size_t tagEnd = comment.find_first_of(" \t\n\r", tagStart);
        if (tagEnd == std::string_view::npos)
            break;

        std::string_view tagName = comment.substr(tagStart, tagEnd - tagStart);

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

/**
 * @brief Parse function definition with error handling
 * @param definition Function definition string
 * @return Parsed function signature or error
 */
[[nodiscard]] inline constexpr auto parseFunctionDefinition(
    const std::string_view definition) noexcept
    -> type::expected<FunctionSignature, ParsingError> {
    using enum ParsingErrorCode;

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

    if (!definition.starts_with(DEF_PREFIX)) {
        return type::unexpected(ParsingError{
            InvalidPrefix, "Function definition must start with 'def '", 0});
    }

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

            startPos = definition.find(DEF_PREFIX, startPos);
            if (startPos == std::string_view::npos) {
                return type::unexpected(ParsingError{
                    InvalidPrefix,
                    "Cannot find 'def' after template declaration", 0});
            }
        }
    }

    bool isInline = definition.find(INLINE_MODIFIER) != std::string_view::npos;
    bool isStatic = definition.find(STATIC_MODIFIER) != std::string_view::npos;
    bool isExplicit =
        definition.find(EXPLICIT_MODIFIER) != std::string_view::npos;

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

    size_t arrowPos = definition.find(ARROW, paramsEnd + 1);
    std::optional<std::string_view> returnType;
    if (arrowPos != std::string_view::npos) {
        returnType =
            atom::utils::trim(definition.substr(arrowPos + ARROW.size()));
    }

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

    std::vector<Parameter> parameters;
    size_t paramStart = 0;

    while (paramStart < paramsStr.size()) {
        size_t paramEnd = paramsStr.size();
        int bracketCount = 0;
        int angleCount = 0;

        for (size_t i = paramStart; i < paramsStr.size(); ++i) {
            char c = paramsStr[i];

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

            if (c == ',' && bracketCount == 0 && angleCount == 0) {
                paramEnd = i;
                break;
            }
        }

        if (bracketCount != 0 || angleCount != 0) {
            return type::unexpected(
                ParsingError{UnbalancedBrackets,
                             "Unbalanced brackets in parameters", paramsStart});
        }

        std::string_view param = atom::utils::trim(
            paramsStr.substr(paramStart, paramEnd - paramStart));
        if (param.empty()) {
            paramStart = paramEnd + 1;
            continue;
        }

        Parameter parameter;

        size_t equalsPos = param.find('=');
        if (equalsPos != std::string_view::npos) {
            parameter.hasDefaultValue = true;
            parameter.defaultValue =
                atom::utils::trim(param.substr(equalsPos + 1));
            param = atom::utils::trim(param.substr(0, equalsPos));
        }

        size_t colonPos = param.find(':');
        if (colonPos != std::string_view::npos) {
            parameter.name = atom::utils::trim(param.substr(0, colonPos));
            parameter.type = atom::utils::trim(param.substr(colonPos + 1));
        } else {
            parameter.name = param;
            parameter.type = "any";
        }

        parameters.push_back(parameter);
        paramStart = paramEnd + 1;
    }

    std::optional<DocComment> docComment;
    size_t docStart = definition.find("/**");
    if (docStart != std::string_view::npos) {
        size_t docEnd = definition.find("*/", docStart);
        if (docEnd != std::string_view::npos) {
            docComment = parseDocComment(
                definition.substr(docStart, docEnd - docStart + 2));
        }
    }

    return FunctionSignature(name, parameters, returnType, modifiers,
                             docComment, isTemplated, templateParams, isInline,
                             isStatic, isExplicit);
}

/**
 * @brief Signature registry for caching and managing function signatures
 */
class SignatureRegistry {
public:
    /**
     * @brief Get singleton instance
     * @return Reference to singleton instance
     */
    static SignatureRegistry& instance() {
        static SignatureRegistry instance;
        return instance;
    }

    /**
     * @brief Register a function signature with caching
     * @param signature Function signature string
     * @return Parsed function signature or error
     */
    template <typename Signature>
        requires std::is_convertible_v<Signature, std::string_view>
    auto registerSignature(Signature&& signature)
        -> type::expected<FunctionSignature, ParsingError> {
        std::string_view sigView = signature;
        std::lock_guard lock(mutex_);

        if (auto it = cache_.find(std::string(sigView)); it != cache_.end()) {
            return it->second;
        }

        auto result = parseFunctionDefinition(sigView);
        if (result) {
            cache_[std::string(sigView)] = result.value();
        }
        return result;
    }

    /**
     * @brief Clear signature cache
     */
    void clearCache() {
        std::lock_guard lock(mutex_);
        cache_.clear();
    }

    /**
     * @brief Get cache size
     * @return Number of cached signatures
     */
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
