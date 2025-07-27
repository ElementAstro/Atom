/**
 * @file markdown_exporter.hpp
 * @brief Markdown exporter for system information
 *
 * This file contains the MarkdownExporter class for exporting system information
 * to Markdown format with proper formatting and structure.
 *
 * @copyright Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#ifndef ATOM_SYSINFO_PRINTER_EXPORTERS_MARKDOWN_EXPORTER_HPP
#define ATOM_SYSINFO_PRINTER_EXPORTERS_MARKDOWN_EXPORTER_HPP

#include "base_exporter.hpp"

namespace atom::system {

/**
 * @class MarkdownExporter
 * @brief Exporter for Markdown format
 *
 * This class provides specialized export functionality for Markdown format
 * including proper headers, tables, and code blocks.
 */
class MarkdownExporter : public BaseExporter {
public:
    /**
     * @brief Default constructor
     */
    MarkdownExporter() = default;

    /**
     * @brief Constructor with options
     * @param options Initial export options
     */
    explicit MarkdownExporter(const ExportOptions& options);

    /**
     * @brief Export system information to Markdown file
     * @param content The content to export
     * @param filename The output filename
     * @return true if export was successful, false otherwise
     */
    bool exportToFile(const std::string& content, const std::string& filename) override;

    /**
     * @brief Export system information to Markdown string
     * @param content The content to export
     * @return Exported Markdown content as string
     */
    auto exportToString(const std::string& content) -> std::string override;

    /**
     * @brief Get the file extension for Markdown format
     * @return ".md"
     */
    [[nodiscard]] auto getFileExtension() const -> std::string override;

    /**
     * @brief Get the MIME type for Markdown format
     * @return "text/markdown"
     */
    [[nodiscard]] auto getMimeType() const -> std::string override;

private:
    /**
     * @brief Convert plain text content to Markdown
     * @param content The plain text content
     * @return Markdown formatted content
     */
    [[nodiscard]] auto convertToMarkdown(const std::string& content) const -> std::string;

    /**
     * @brief Generate Markdown document structure
     * @param bodyContent The body content
     * @return Complete Markdown document
     */
    [[nodiscard]] auto generateMarkdownDocument(const std::string& bodyContent) const -> std::string;

    /**
     * @brief Convert ASCII table to Markdown table
     * @param tableContent The ASCII table content
     * @return Markdown table
     */
    [[nodiscard]] auto convertTableToMarkdown(const std::string& tableContent) const -> std::string;

    /**
     * @brief Escape Markdown special characters
     * @param text The text to escape
     * @return Markdown escaped text
     */
    [[nodiscard]] auto escapeText(const std::string& text) const -> std::string override;

    /**
     * @brief Add metadata to Markdown document
     * @param content The content to add metadata to
     * @return Content with Markdown metadata
     */
    [[nodiscard]] auto addMetadata(const std::string& content) const -> std::string override;

    /**
     * @brief Generate Markdown header with title and metadata
     * @return Markdown header string
     */
    [[nodiscard]] auto generateHeader() const -> std::string;

    /**
     * @brief Generate Markdown footer
     * @return Markdown footer string
     */
    [[nodiscard]] auto generateFooter() const -> std::string;
};

} // namespace atom::system

#endif // ATOM_SYSINFO_PRINTER_EXPORTERS_MARKDOWN_EXPORTER_HPP