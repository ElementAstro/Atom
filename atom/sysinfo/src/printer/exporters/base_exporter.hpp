/**
 * @file base_exporter.hpp
 * @brief Base exporter interface for system information
 *
 * This file defines the base interface and common functionality for all
 * system information exporters. It provides a consistent API for exporting
 * system information to various file formats.
 *
 * @copyright Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#ifndef ATOM_SYSINFO_PRINTER_EXPORTERS_BASE_EXPORTER_HPP
#define ATOM_SYSINFO_PRINTER_EXPORTERS_BASE_EXPORTER_HPP

#include <string>
#include <unordered_map>
#include <memory>
#include <fstream>
#include "atom/macro.hpp"

namespace atom::system {

/**
 * @enum ExportFormat
 * @brief Defines different export formats
 */
enum class ExportFormat {
    HTML,       ///< HTML format with CSS styling
    JSON,       ///< JSON format for structured data
    MARKDOWN,   ///< Markdown format for documentation
    XML,        ///< XML format for data exchange
    CSV,        ///< CSV format for spreadsheet applications
    PDF,        ///< PDF format for professional reports (future)
    PLAIN_TEXT  ///< Plain text format
};

/**
 * @struct ExportOptions
 * @brief Configuration options for exporters
 */
struct ExportOptions {
    ExportFormat format = ExportFormat::HTML;
    bool includeTimestamp = true;
    bool includeMetadata = true;
    bool prettyPrint = true;
    bool compressOutput = false;
    std::string templatePath;
    std::string cssPath;
    std::string title = "System Information Report";
    std::string author;
    std::string description;
    std::unordered_map<std::string, std::string> customOptions;
} ATOM_ALIGNAS(64);

/**
 * @class BaseExporter
 * @brief Abstract base class for all system information exporters
 *
 * This class provides the common interface and functionality that all
 * specific exporters must implement. It handles configuration, file I/O,
 * and common export operations.
 */
class BaseExporter {
public:
    /**
     * @brief Virtual destructor
     */
    virtual ~BaseExporter() = default;

    /**
     * @brief Export system information to a file
     * @param content The content to export
     * @param filename The output filename
     * @return true if export was successful, false otherwise
     */
    virtual bool exportToFile(const std::string& content, const std::string& filename) = 0;

    /**
     * @brief Export system information to a string
     * @param content The content to export
     * @return Exported content as string
     */
    virtual auto exportToString(const std::string& content) -> std::string = 0;

    /**
     * @brief Set export options
     * @param options The export options to use
     */
    virtual void setOptions(const ExportOptions& options);

    /**
     * @brief Get current export options
     * @return Current export options
     */
    [[nodiscard]] virtual auto getOptions() const -> const ExportOptions&;

    /**
     * @brief Set a custom option
     * @param key The option key
     * @param value The option value
     */
    virtual void setCustomOption(const std::string& key, const std::string& value);

    /**
     * @brief Get a custom option value
     * @param key The option key
     * @param defaultValue Default value if key not found
     * @return The option value or default
     */
    [[nodiscard]] virtual auto getCustomOption(const std::string& key, 
                                              const std::string& defaultValue = "") const -> std::string;

    /**
     * @brief Get the file extension for this export format
     * @return File extension (e.g., ".html", ".json")
     */
    [[nodiscard]] virtual auto getFileExtension() const -> std::string = 0;

    /**
     * @brief Get the MIME type for this export format
     * @return MIME type (e.g., "text/html", "application/json")
     */
    [[nodiscard]] virtual auto getMimeType() const -> std::string = 0;

protected:
    /**
     * @brief Add metadata to the exported content
     * @param content The content to add metadata to
     * @return Content with metadata
     */
    [[nodiscard]] virtual auto addMetadata(const std::string& content) const -> std::string;

    /**
     * @brief Add timestamp to the exported content
     * @param content The content to add timestamp to
     * @return Content with timestamp
     */
    [[nodiscard]] virtual auto addTimestamp(const std::string& content) const -> std::string;

    /**
     * @brief Escape special characters for the target format
     * @param text The text to escape
     * @return Escaped text
     */
    [[nodiscard]] virtual auto escapeText(const std::string& text) const -> std::string;

    /**
     * @brief Load template from file
     * @param templatePath Path to the template file
     * @return Template content or empty string if failed
     */
    [[nodiscard]] virtual auto loadTemplate(const std::string& templatePath) const -> std::string;

    /**
     * @brief Replace placeholders in template
     * @param templateContent The template content
     * @param placeholders Map of placeholder -> value
     * @return Template with placeholders replaced
     */
    [[nodiscard]] virtual auto replacePlaceholders(const std::string& templateContent,
                                                   const std::unordered_map<std::string, std::string>& placeholders) const -> std::string;

    /**
     * @brief Write content to file with error handling
     * @param content The content to write
     * @param filename The output filename
     * @return true if successful, false otherwise
     */
    [[nodiscard]] virtual bool writeToFile(const std::string& content, const std::string& filename) const;

    /**
     * @brief Validate filename and create directories if needed
     * @param filename The filename to validate
     * @return true if valid and directories created, false otherwise
     */
    [[nodiscard]] virtual bool validateAndPrepareFile(const std::string& filename) const;

    /**
     * @brief Get current timestamp as string
     * @return Formatted timestamp string
     */
    [[nodiscard]] virtual auto getCurrentTimestamp() const -> std::string;

    /**
     * @brief Format file size in human-readable format
     * @param bytes File size in bytes
     * @return Formatted size string
     */
    [[nodiscard]] virtual auto formatFileSize(size_t bytes) const -> std::string;

    ExportOptions options_; ///< Current export options

private:
    /**
     * @brief Create directory structure for file path
     * @param filepath The file path
     * @return true if successful or already exists
     */
    [[nodiscard]] bool createDirectories(const std::string& filepath) const;
};

/**
 * @brief Factory function to create exporters
 * @tparam T The exporter type to create
 * @param options Initial export options
 * @return Unique pointer to the created exporter
 */
template<typename T>
[[nodiscard]] auto createExporter(const ExportOptions& options = {}) -> std::unique_ptr<T> {
    static_assert(std::is_base_of_v<BaseExporter, T>, "T must derive from BaseExporter");
    auto exporter = std::make_unique<T>();
    exporter->setOptions(options);
    return exporter;
}

} // namespace atom::system

#endif // ATOM_SYSINFO_PRINTER_EXPORTERS_BASE_EXPORTER_HPP
