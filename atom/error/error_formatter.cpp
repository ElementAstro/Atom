/*
 * error_formatter.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-22

Description: Implementation of error formatting system

**************************************************/

#include "error_formatter.hpp"
#include <algorithm>
#include <iomanip>
#include <iostream>
#include <regex>
#include <fstream>

namespace atom::error {

// ErrorFormatter base implementation
std::string ErrorFormatter::formatMultiple(const std::vector<std::shared_ptr<ErrorContext>>& contexts) {
    std::stringstream ss;
    for (size_t i = 0; i < contexts.size(); ++i) {
        if (i > 0) ss << "\n";
        ss << format(contexts[i]);
    }
    return ss.str();
}

// PlainTextFormatter implementation
PlainTextFormatter::PlainTextFormatter()
    : includeStackTrace_(true)
    , includeSystemInfo_(true)
    , includeTimestamp_(true)
    , dateFormat_("%Y-%m-%d %H:%M:%S") {
    options_["include_stack_trace"] = "true";
    options_["include_system_info"] = "true";
    options_["include_timestamp"] = "true";
    options_["date_format"] = dateFormat_;
}

std::string PlainTextFormatter::format(std::shared_ptr<ErrorContext> context) {
    if (!context) return "";
    
    std::stringstream ss;
    
    // Header
    ss << "=== ERROR REPORT ===\n";
    
    // Basic information
    ss << "Error ID: " << context->getErrorId() << "\n";
    ss << "Error Code: " << context->getErrorCode() << "\n";
    ss << "Message: " << context->getMessage() << "\n";
    ss << "Severity: " << severityToString(context->getSeverity()) << "\n";
    ss << "Category: " << categoryToString(context->getCategory()) << "\n";
    
    // Timestamp
    if (includeTimestamp_) {
        auto time_t = std::chrono::system_clock::to_time_t(context->getTimestamp());
        ss << "Timestamp: " << std::put_time(std::localtime(&time_t), dateFormat_.c_str()) << "\n";
    }
    
    // Correlation information
    if (!context->getCorrelationId().empty()) {
        ss << "Correlation ID: " << context->getCorrelationId() << "\n";
    }
    
    // Retry information
    ss << "Retry Count: " << context->getRetryCount() << "/" << context->getMaxRetries() << "\n";
    
    // Tags
    const auto& tags = context->getTags();
    if (!tags.empty()) {
        ss << "Tags: ";
        for (size_t i = 0; i < tags.size(); ++i) {
            if (i > 0) ss << ", ";
            ss << tags[i];
        }
        ss << "\n";
    }
    
    // System information
    if (includeSystemInfo_) {
        ss << "\n--- System Information ---\n";
        auto sysInfo = context->getSystemInfo("pid");
        if (!sysInfo.empty()) ss << "Process ID: " << sysInfo << "\n";
        
        sysInfo = context->getSystemInfo("thread_id");
        if (!sysInfo.empty()) ss << "Thread ID: " << sysInfo << "\n";
        
        sysInfo = context->getSystemInfo("hostname");
        if (!sysInfo.empty()) ss << "Hostname: " << sysInfo << "\n";
    }
    
    // Stack trace
    if (includeStackTrace_ && !context->getStackTrace().empty()) {
        ss << "\n--- Stack Trace ---\n";
        ss << context->getStackTrace() << "\n";
    }
    
    ss << "==================\n";
    
    return ss.str();
}

void PlainTextFormatter::setOption(const std::string& key, const std::string& value) {
    options_[key] = value;
    
    if (key == "include_stack_trace") {
        includeStackTrace_ = (value == "true");
    } else if (key == "include_system_info") {
        includeSystemInfo_ = (value == "true");
    } else if (key == "include_timestamp") {
        includeTimestamp_ = (value == "true");
    } else if (key == "date_format") {
        dateFormat_ = value;
    }
}

std::string PlainTextFormatter::getOption(const std::string& key) const {
    auto it = options_.find(key);
    return it != options_.end() ? it->second : "";
}

// JsonFormatter implementation
JsonFormatter::JsonFormatter()
    : prettyPrint_(true)
    , indentSize_(2) {
    options_["pretty_print"] = "true";
    options_["indent_size"] = "2";
}

std::string JsonFormatter::format(std::shared_ptr<ErrorContext> context) {
    if (!context) return "{}";
    
    std::stringstream ss;
    std::string indent = prettyPrint_ ? std::string(indentSize_, ' ') : "";
    std::string newline = prettyPrint_ ? "\n" : "";
    
    ss << "{" << newline;
    
    // Basic information
    ss << indent << formatJsonValue("errorId", context->getErrorId()) << newline;
    ss << indent << formatJsonValue("errorCode", std::to_string(context->getErrorCode())) << newline;
    ss << indent << formatJsonValue("message", context->getMessage()) << newline;
    ss << indent << formatJsonValue("severity", std::string(severityToString(context->getSeverity()))) << newline;
    ss << indent << formatJsonValue("category", std::string(categoryToString(context->getCategory()))) << newline;
    
    // Timestamp
    auto time_t = std::chrono::system_clock::to_time_t(context->getTimestamp());
    ss << indent << formatJsonValue("timestamp", std::to_string(time_t)) << newline;
    
    // Correlation ID
    if (!context->getCorrelationId().empty()) {
        ss << indent << formatJsonValue("correlationId", context->getCorrelationId()) << newline;
    }
    
    // Retry information
    ss << indent << formatJsonValue("retryCount", std::to_string(context->getRetryCount())) << newline;
    ss << indent << formatJsonValue("maxRetries", std::to_string(context->getMaxRetries())) << newline;
    
    // Tags
    const auto& tags = context->getTags();
    if (!tags.empty()) {
        ss << indent << "\"tags\": [";
        for (size_t i = 0; i < tags.size(); ++i) {
            if (i > 0) ss << ", ";
            ss << "\"" << escapeJsonString(tags[i]) << "\"";
        }
        ss << "]," << newline;
    }
    
    // Stack trace
    if (!context->getStackTrace().empty()) {
        ss << indent << formatJsonValue("stackTrace", context->getStackTrace(), true) << newline;
    }
    
    ss << "}";
    
    return ss.str();
}

std::string JsonFormatter::formatMultiple(const std::vector<std::shared_ptr<ErrorContext>>& contexts) {
    std::stringstream ss;
    std::string newline = prettyPrint_ ? "\n" : "";
    std::string indent = prettyPrint_ ? std::string(indentSize_, ' ') : "";
    
    ss << "[" << newline;
    for (size_t i = 0; i < contexts.size(); ++i) {
        if (i > 0) ss << "," << newline;
        if (prettyPrint_) {
            std::string contextJson = format(contexts[i]);
            // Add indentation to each line
            std::regex lineRegex("^", std::regex_constants::multiline);
            contextJson = std::regex_replace(contextJson, lineRegex, indent);
            ss << contextJson;
        } else {
            ss << format(contexts[i]);
        }
    }
    ss << newline << "]";
    
    return ss.str();
}

void JsonFormatter::setOption(const std::string& key, const std::string& value) {
    options_[key] = value;
    
    if (key == "pretty_print") {
        prettyPrint_ = (value == "true");
    } else if (key == "indent_size") {
        indentSize_ = std::stoi(value);
    }
}

std::string JsonFormatter::getOption(const std::string& key) const {
    auto it = options_.find(key);
    return it != options_.end() ? it->second : "";
}

std::string JsonFormatter::escapeJsonString(const std::string& str) const {
    std::string escaped;
    escaped.reserve(str.length() * 2);
    
    for (char c : str) {
        switch (c) {
            case '"': escaped += "\\\""; break;
            case '\\': escaped += "\\\\"; break;
            case '\b': escaped += "\\b"; break;
            case '\f': escaped += "\\f"; break;
            case '\n': escaped += "\\n"; break;
            case '\r': escaped += "\\r"; break;
            case '\t': escaped += "\\t"; break;
            default:
                if (c < 0x20) {
                    escaped += "\\u";
                    escaped += std::to_string((c >> 12) & 0xF);
                    escaped += std::to_string((c >> 8) & 0xF);
                    escaped += std::to_string((c >> 4) & 0xF);
                    escaped += std::to_string(c & 0xF);
                } else {
                    escaped += c;
                }
                break;
        }
    }
    
    return escaped;
}

std::string JsonFormatter::formatJsonValue(const std::string& key, const std::string& value, bool isLast) const {
    std::stringstream ss;
    ss << "\"" << escapeJsonString(key) << "\": \"" << escapeJsonString(value) << "\"";
    if (!isLast) ss << ",";
    return ss.str();
}

// ColoredFormatter implementation
ColoredFormatter::ColoredFormatter()
    : enableColors_(true) {
    options_["enable_colors"] = "true";
    
    // Set default severity colors
    severityColors_[ErrorSeverity::Trace] = Color::White;
    severityColors_[ErrorSeverity::Debug] = Color::Cyan;
    severityColors_[ErrorSeverity::Info] = Color::Green;
    severityColors_[ErrorSeverity::Warning] = Color::Yellow;
    severityColors_[ErrorSeverity::Error] = Color::Red;
    severityColors_[ErrorSeverity::Critical] = Color::BrightRed;
    severityColors_[ErrorSeverity::Fatal] = Color::Magenta;
}

std::string ColoredFormatter::format(std::shared_ptr<ErrorContext> context) {
    if (!context) return "";
    
    std::stringstream ss;
    
    // Colored header based on severity
    Color headerColor = getSeverityColor(context->getSeverity());
    ss << colorize("=== ERROR REPORT ===", headerColor) << "\n";
    
    // Basic information with colors
    ss << "Error ID: " << colorize(context->getErrorId(), Color::Cyan) << "\n";
    ss << "Error Code: " << colorize(std::to_string(context->getErrorCode()), Color::Yellow) << "\n";
    ss << "Message: " << colorize(context->getMessage(), Color::White) << "\n";
    ss << "Severity: " << colorize(std::string(severityToString(context->getSeverity())), headerColor) << "\n";
    ss << "Category: " << colorize(std::string(categoryToString(context->getCategory())), Color::Blue) << "\n";
    
    // Timestamp
    auto time_t = std::chrono::system_clock::to_time_t(context->getTimestamp());
    ss << "Timestamp: " << colorize(std::to_string(time_t), Color::Green) << "\n";
    
    // Correlation information
    if (!context->getCorrelationId().empty()) {
        ss << "Correlation ID: " << colorize(context->getCorrelationId(), Color::Magenta) << "\n";
    }
    
    ss << colorize("==================", headerColor) << "\n";
    
    return ss.str();
}

void ColoredFormatter::setOption(const std::string& key, const std::string& value) {
    options_[key] = value;
    
    if (key == "enable_colors") {
        enableColors_ = (value == "true");
    }
}

std::string ColoredFormatter::getOption(const std::string& key) const {
    auto it = options_.find(key);
    return it != options_.end() ? it->second : "";
}

std::string ColoredFormatter::colorize(const std::string& text, Color color) const {
    if (!enableColors_) return text;
    
    std::stringstream ss;
    ss << "\033[" << static_cast<int>(color) << "m" << text << "\033[" << static_cast<int>(Color::Reset) << "m";
    return ss.str();
}

Color ColoredFormatter::getSeverityColor(ErrorSeverity severity) const {
    auto it = severityColors_.find(severity);
    return it != severityColors_.end() ? it->second : Color::White;
}

// ErrorFormatterFactory implementation
std::unordered_map<std::string, std::function<std::unique_ptr<ErrorFormatter>()>> ErrorFormatterFactory::customFormatters_;

std::unique_ptr<ErrorFormatter> ErrorFormatterFactory::createFormatter(OutputFormat format) {
    switch (format) {
        case OutputFormat::Plain:
            return std::make_unique<PlainTextFormatter>();
        case OutputFormat::Json:
            return std::make_unique<JsonFormatter>();
        case OutputFormat::Colored:
            return std::make_unique<ColoredFormatter>();
        case OutputFormat::Html:
            return std::make_unique<HtmlFormatter>();
        case OutputFormat::Structured:
            return std::make_unique<StructuredFormatter>();
        default:
            return std::make_unique<PlainTextFormatter>();
    }
}

void ErrorFormatterFactory::registerFormatter(const std::string& name, std::function<std::unique_ptr<ErrorFormatter>()> factory) {
    customFormatters_[name] = std::move(factory);
}

std::unique_ptr<ErrorFormatter> ErrorFormatterFactory::createCustomFormatter(const std::string& name) {
    auto it = customFormatters_.find(name);
    if (it != customFormatters_.end()) {
        return it->second();
    }
    return nullptr;
}

std::vector<std::string> ErrorFormatterFactory::getAvailableFormatters() {
    std::vector<std::string> formatters = {"plain", "json", "colored", "html", "structured"};

    for (const auto& [name, factory] : customFormatters_) {
        formatters.push_back(name);
    }

    return formatters;
}

// HtmlFormatter implementation
HtmlFormatter::HtmlFormatter()
    : includeCSS_(true) {
    options_["include_css"] = "true";

    cssStyle_ = R"(
        <style>
        .error-report { font-family: Arial, sans-serif; margin: 10px; padding: 15px; border-radius: 5px; }
        .error-trace { background-color: #f8f8f8; color: #333; }
        .error-debug { background-color: #e1f5fe; color: #0277bd; }
        .error-info { background-color: #e8f5e8; color: #2e7d32; }
        .error-warning { background-color: #fff3e0; color: #f57c00; }
        .error-error { background-color: #ffebee; color: #c62828; }
        .error-critical { background-color: #fce4ec; color: #ad1457; }
        .error-fatal { background-color: #f3e5f5; color: #6a1b9a; }
        .error-header { font-weight: bold; font-size: 1.2em; margin-bottom: 10px; }
        .error-field { margin: 5px 0; }
        .error-label { font-weight: bold; }
        .error-value { margin-left: 10px; }
        </style>
    )";
}

std::string HtmlFormatter::format(std::shared_ptr<ErrorContext> context) {
    if (!context) return "";

    std::stringstream ss;

    if (includeCSS_) {
        ss << cssStyle_ << "\n";
    }

    std::string severityClass = getSeverityClass(context->getSeverity());
    ss << "<div class=\"error-report " << severityClass << "\">\n";

    // Header
    ss << "  <div class=\"error-header\">Error Report</div>\n";

    // Basic information
    ss << "  <div class=\"error-field\">\n";
    ss << "    <span class=\"error-label\">Error ID:</span>\n";
    ss << "    <span class=\"error-value\">" << escapeHtml(context->getErrorId()) << "</span>\n";
    ss << "  </div>\n";

    ss << "  <div class=\"error-field\">\n";
    ss << "    <span class=\"error-label\">Error Code:</span>\n";
    ss << "    <span class=\"error-value\">" << context->getErrorCode() << "</span>\n";
    ss << "  </div>\n";

    ss << "  <div class=\"error-field\">\n";
    ss << "    <span class=\"error-label\">Message:</span>\n";
    ss << "    <span class=\"error-value\">" << escapeHtml(context->getMessage()) << "</span>\n";
    ss << "  </div>\n";

    ss << "  <div class=\"error-field\">\n";
    ss << "    <span class=\"error-label\">Severity:</span>\n";
    ss << "    <span class=\"error-value\">" << severityToString(context->getSeverity()) << "</span>\n";
    ss << "  </div>\n";

    ss << "  <div class=\"error-field\">\n";
    ss << "    <span class=\"error-label\">Category:</span>\n";
    ss << "    <span class=\"error-value\">" << categoryToString(context->getCategory()) << "</span>\n";
    ss << "  </div>\n";

    // Stack trace
    if (!context->getStackTrace().empty()) {
        ss << "  <div class=\"error-field\">\n";
        ss << "    <span class=\"error-label\">Stack Trace:</span>\n";
        ss << "    <pre class=\"error-value\">" << escapeHtml(context->getStackTrace()) << "</pre>\n";
        ss << "  </div>\n";
    }

    ss << "</div>\n";

    return ss.str();
}

std::string HtmlFormatter::formatMultiple(const std::vector<std::shared_ptr<ErrorContext>>& contexts) {
    std::stringstream ss;

    if (includeCSS_) {
        ss << cssStyle_ << "\n";
    }

    ss << "<div class=\"error-reports\">\n";
    for (const auto& context : contexts) {
        std::string contextHtml = format(context);
        // Remove CSS from individual contexts to avoid duplication
        size_t styleEnd = contextHtml.find("</style>");
        if (styleEnd != std::string::npos) {
            contextHtml = contextHtml.substr(styleEnd + 8);
        }
        ss << contextHtml << "\n";
    }
    ss << "</div>\n";

    return ss.str();
}

void HtmlFormatter::setOption(const std::string& key, const std::string& value) {
    options_[key] = value;

    if (key == "include_css") {
        includeCSS_ = (value == "true");
    } else if (key == "css_style") {
        cssStyle_ = value;
    }
}

std::string HtmlFormatter::getOption(const std::string& key) const {
    auto it = options_.find(key);
    return it != options_.end() ? it->second : "";
}

std::string HtmlFormatter::escapeHtml(const std::string& str) const {
    std::string escaped;
    escaped.reserve(str.length() * 2);

    for (char c : str) {
        switch (c) {
            case '<': escaped += "&lt;"; break;
            case '>': escaped += "&gt;"; break;
            case '&': escaped += "&amp;"; break;
            case '"': escaped += "&quot;"; break;
            case '\'': escaped += "&#39;"; break;
            default: escaped += c; break;
        }
    }

    return escaped;
}

std::string HtmlFormatter::getSeverityClass(ErrorSeverity severity) const {
    switch (severity) {
        case ErrorSeverity::Trace: return "error-trace";
        case ErrorSeverity::Debug: return "error-debug";
        case ErrorSeverity::Info: return "error-info";
        case ErrorSeverity::Warning: return "error-warning";
        case ErrorSeverity::Error: return "error-error";
        case ErrorSeverity::Critical: return "error-critical";
        case ErrorSeverity::Fatal: return "error-fatal";
        default: return "error-error";
    }
}

// StructuredFormatter implementation
StructuredFormatter::StructuredFormatter()
    : fieldSeparator_(" ")
    , keyValueSeparator_("=") {
    options_["field_separator"] = " ";
    options_["key_value_separator"] = "=";

    fieldOrder_ = {"timestamp", "severity", "category", "code", "message", "correlation_id", "error_id"};
}

std::string StructuredFormatter::format(std::shared_ptr<ErrorContext> context) {
    if (!context) return "";

    std::stringstream ss;

    for (size_t i = 0; i < fieldOrder_.size(); ++i) {
        if (i > 0) ss << fieldSeparator_;

        const std::string& field = fieldOrder_[i];
        if (field == "timestamp") {
            auto time_t = std::chrono::system_clock::to_time_t(context->getTimestamp());
            ss << formatField("timestamp", std::to_string(time_t));
        } else if (field == "severity") {
            ss << formatField("severity", std::string(severityToString(context->getSeverity())));
        } else if (field == "category") {
            ss << formatField("category", std::string(categoryToString(context->getCategory())));
        } else if (field == "code") {
            ss << formatField("code", std::to_string(context->getErrorCode()));
        } else if (field == "message") {
            ss << formatField("message", context->getMessage());
        } else if (field == "correlation_id" && !context->getCorrelationId().empty()) {
            ss << formatField("correlation_id", context->getCorrelationId());
        } else if (field == "error_id") {
            ss << formatField("error_id", context->getErrorId());
        }
    }

    return ss.str();
}

void StructuredFormatter::setOption(const std::string& key, const std::string& value) {
    options_[key] = value;

    if (key == "field_separator") {
        fieldSeparator_ = value;
    } else if (key == "key_value_separator") {
        keyValueSeparator_ = value;
    }
}

std::string StructuredFormatter::getOption(const std::string& key) const {
    auto it = options_.find(key);
    return it != options_.end() ? it->second : "";
}

std::string StructuredFormatter::formatField(const std::string& key, const std::string& value) const {
    return key + keyValueSeparator_ + value;
}

} // namespace atom::error
