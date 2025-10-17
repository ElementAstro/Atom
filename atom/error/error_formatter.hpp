/*
 * error_formatter.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-22

Description: Comprehensive error formatting system with customizable output
formats

**************************************************/

#ifndef ATOM_ERROR_FORMATTER_HPP
#define ATOM_ERROR_FORMATTER_HPP

#include <functional>
#include <locale>
#include <memory>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#include "error_context.hpp"

namespace atom::error {

/**
 * @brief Output format types
 */
enum class OutputFormat {
    Plain,      ///< Plain text format
    Json,       ///< JSON format
    Xml,        ///< XML format
    Html,       ///< HTML format
    Markdown,   ///< Markdown format
    Colored,    ///< Colored terminal output
    Structured  ///< Structured logging format
};

/**
 * @brief Color codes for terminal output
 */
enum class Color {
    Reset = 0,
    Black = 30,
    Red = 31,
    Green = 32,
    Yellow = 33,
    Blue = 34,
    Magenta = 35,
    Cyan = 36,
    White = 37,
    BrightBlack = 90,
    BrightRed = 91,
    BrightGreen = 92,
    BrightYellow = 93,
    BrightBlue = 94,
    BrightMagenta = 95,
    BrightCyan = 96,
    BrightWhite = 97
};

/**
 * @brief Error formatter interface
 */
class ErrorFormatter {
public:
    virtual ~ErrorFormatter() = default;

    /**
     * @brief Format error context to string
     */
    virtual std::string format(std::shared_ptr<ErrorContext> context) = 0;

    /**
     * @brief Format multiple error contexts
     */
    virtual std::string formatMultiple(
        const std::vector<std::shared_ptr<ErrorContext>>& contexts);

    /**
     * @brief Set formatting options
     */
    virtual void setOption(const std::string& key,
                           const std::string& value) = 0;

    /**
     * @brief Get formatting options
     */
    virtual std::string getOption(const std::string& key) const = 0;
};

/**
 * @brief Plain text error formatter
 */
class PlainTextFormatter : public ErrorFormatter {
public:
    PlainTextFormatter();

    std::string format(std::shared_ptr<ErrorContext> context) override;
    void setOption(const std::string& key, const std::string& value) override;
    std::string getOption(const std::string& key) const override;

private:
    std::unordered_map<std::string, std::string> options_;
    bool includeStackTrace_;
    bool includeSystemInfo_;
    bool includeTimestamp_;
    std::string dateFormat_;
};

/**
 * @brief JSON error formatter
 */
class JsonFormatter : public ErrorFormatter {
public:
    JsonFormatter();

    std::string format(std::shared_ptr<ErrorContext> context) override;
    std::string formatMultiple(
        const std::vector<std::shared_ptr<ErrorContext>>& contexts) override;
    void setOption(const std::string& key, const std::string& value) override;
    std::string getOption(const std::string& key) const override;

private:
    std::unordered_map<std::string, std::string> options_;
    bool prettyPrint_;
    int indentSize_;

    std::string escapeJsonString(const std::string& str) const;
    std::string formatJsonValue(const std::string& key,
                                const std::string& value,
                                bool isLast = false) const;
};

/**
 * @brief Colored terminal formatter
 */
class ColoredFormatter : public ErrorFormatter {
public:
    ColoredFormatter();

    std::string format(std::shared_ptr<ErrorContext> context) override;
    void setOption(const std::string& key, const std::string& value) override;
    std::string getOption(const std::string& key) const override;

private:
    std::unordered_map<std::string, std::string> options_;
    std::unordered_map<ErrorSeverity, Color> severityColors_;
    bool enableColors_;

    std::string colorize(const std::string& text, Color color) const;
    Color getSeverityColor(ErrorSeverity severity) const;
};

/**
 * @brief HTML error formatter
 */
class HtmlFormatter : public ErrorFormatter {
public:
    HtmlFormatter();

    std::string format(std::shared_ptr<ErrorContext> context) override;
    std::string formatMultiple(
        const std::vector<std::shared_ptr<ErrorContext>>& contexts) override;
    void setOption(const std::string& key, const std::string& value) override;
    std::string getOption(const std::string& key) const override;

private:
    std::unordered_map<std::string, std::string> options_;
    bool includeCSS_;
    std::string cssStyle_;

    std::string escapeHtml(const std::string& str) const;
    std::string getSeverityClass(ErrorSeverity severity) const;
};

/**
 * @brief Structured logging formatter
 */
class StructuredFormatter : public ErrorFormatter {
public:
    StructuredFormatter();

    std::string format(std::shared_ptr<ErrorContext> context) override;
    void setOption(const std::string& key, const std::string& value) override;
    std::string getOption(const std::string& key) const override;

private:
    std::unordered_map<std::string, std::string> options_;
    std::string fieldSeparator_;
    std::string keyValueSeparator_;
    std::vector<std::string> fieldOrder_;

    std::string formatField(const std::string& key,
                            const std::string& value) const;
};

/**
 * @brief Localization support for error messages
 */
class ErrorLocalizer {
public:
    ErrorLocalizer();

    /**
     * @brief Set current locale
     */
    void setLocale(const std::string& locale);

    /**
     * @brief Get current locale
     */
    std::string getCurrentLocale() const;

    /**
     * @brief Add translation for error code
     */
    void addTranslation(const std::string& locale, int errorCode,
                        const std::string& message);

    /**
     * @brief Add translation for severity level
     */
    void addSeverityTranslation(const std::string& locale,
                                ErrorSeverity severity,
                                const std::string& translation);

    /**
     * @brief Add translation for category
     */
    void addCategoryTranslation(const std::string& locale,
                                ErrorCategory category,
                                const std::string& translation);

    /**
     * @brief Get localized error message
     */
    std::string getLocalizedMessage(int errorCode) const;

    /**
     * @brief Get localized severity string
     */
    std::string getLocalizedSeverity(ErrorSeverity severity) const;

    /**
     * @brief Get localized category string
     */
    std::string getLocalizedCategory(ErrorCategory category) const;

    /**
     * @brief Load translations from file
     */
    bool loadTranslations(const std::string& filePath);

private:
    std::string currentLocale_;
    std::unordered_map<std::string, std::unordered_map<int, std::string>>
        errorTranslations_;
    std::unordered_map<std::string,
                       std::unordered_map<ErrorSeverity, std::string>>
        severityTranslations_;
    std::unordered_map<std::string,
                       std::unordered_map<ErrorCategory, std::string>>
        categoryTranslations_;
};

/**
 * @brief Error formatter factory
 */
class ErrorFormatterFactory {
public:
    /**
     * @brief Create formatter by output format
     */
    static std::unique_ptr<ErrorFormatter> createFormatter(OutputFormat format);

    /**
     * @brief Register custom formatter
     */
    static void registerFormatter(
        const std::string& name,
        std::function<std::unique_ptr<ErrorFormatter>()> factory);

    /**
     * @brief Create custom formatter by name
     */
    static std::unique_ptr<ErrorFormatter> createCustomFormatter(
        const std::string& name);

    /**
     * @brief Get available formatter names
     */
    static std::vector<std::string> getAvailableFormatters();

private:
    static std::unordered_map<std::string,
                              std::function<std::unique_ptr<ErrorFormatter>()>>
        customFormatters_;
};

/**
 * @brief Error display manager for handling formatted output
 */
class ErrorDisplayManager {
public:
    ErrorDisplayManager();

    /**
     * @brief Set default formatter
     */
    void setDefaultFormatter(std::unique_ptr<ErrorFormatter> formatter);

    /**
     * @brief Add formatter for specific output format
     */
    void addFormatter(OutputFormat format,
                      std::unique_ptr<ErrorFormatter> formatter);

    /**
     * @brief Display error using specified format
     */
    void displayError(std::shared_ptr<ErrorContext> context,
                      OutputFormat format = OutputFormat::Plain);

    /**
     * @brief Display multiple errors
     */
    void displayErrors(
        const std::vector<std::shared_ptr<ErrorContext>>& contexts,
        OutputFormat format = OutputFormat::Plain);

    /**
     * @brief Set output stream
     */
    void setOutputStream(std::ostream& stream);

    /**
     * @brief Set error localizer
     */
    void setLocalizer(std::shared_ptr<ErrorLocalizer> localizer);

    /**
     * @brief Enable/disable automatic display
     */
    void setAutoDisplay(bool enabled);

private:
    std::unordered_map<OutputFormat, std::unique_ptr<ErrorFormatter>>
        formatters_;
    std::unique_ptr<ErrorFormatter> defaultFormatter_;
    std::ostream* outputStream_;
    std::shared_ptr<ErrorLocalizer> localizer_;
    bool autoDisplay_;

    ErrorFormatter* getFormatter(OutputFormat format);
};

/**
 * @brief Template formatter for custom formatting
 */
class TemplateFormatter : public ErrorFormatter {
public:
    explicit TemplateFormatter(const std::string& templateStr);

    std::string format(std::shared_ptr<ErrorContext> context) override;
    void setOption(const std::string& key, const std::string& value) override;
    std::string getOption(const std::string& key) const override;

    /**
     * @brief Set template string
     */
    void setTemplate(const std::string& templateStr);

private:
    std::string templateStr_;
    std::unordered_map<std::string, std::string> options_;

    std::string processTemplate(std::shared_ptr<ErrorContext> context) const;
    std::string replaceVariables(
        const std::string& str,
        const std::unordered_map<std::string, std::string>& variables) const;
};

/**
 * @brief Convenience macros for error formatting
 */
#define FORMAT_ERROR_PLAIN(context)                      \
    atom::error::ErrorFormatterFactory::createFormatter( \
        atom::error::OutputFormat::Plain)                \
        ->format(context)

#define FORMAT_ERROR_JSON(context)                       \
    atom::error::ErrorFormatterFactory::createFormatter( \
        atom::error::OutputFormat::Json)                 \
        ->format(context)

#define FORMAT_ERROR_COLORED(context)                    \
    atom::error::ErrorFormatterFactory::createFormatter( \
        atom::error::OutputFormat::Colored)              \
        ->format(context)

}  // namespace atom::error

#endif  // ATOM_ERROR_FORMATTER_HPP
