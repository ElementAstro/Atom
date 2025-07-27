/**
 * @file html_exporter.hpp
 * @brief HTML exporter for system information
 *
 * This file contains the HtmlExporter class for exporting system information
 * to HTML format with CSS styling and responsive design.
 *
 * @copyright Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#ifndef ATOM_SYSINFO_PRINTER_EXPORTERS_HTML_EXPORTER_HPP
#define ATOM_SYSINFO_PRINTER_EXPORTERS_HTML_EXPORTER_HPP

#include "base_exporter.hpp"

namespace atom::system {

/**
 * @class HtmlExporter
 * @brief Exporter for HTML format
 *
 * This class provides specialized export functionality for HTML format including
 * CSS styling, responsive design, and interactive elements.
 */
class HtmlExporter : public BaseExporter {
public:
    /**
     * @brief Default constructor
     */
    HtmlExporter() = default;

    /**
     * @brief Constructor with options
     * @param options Initial export options
     */
    explicit HtmlExporter(const ExportOptions& options);

    /**
     * @brief Export system information to HTML file
     * @param content The content to export
     * @param filename The output filename
     * @return true if export was successful, false otherwise
     */
    bool exportToFile(const std::string& content, const std::string& filename) override;

    /**
     * @brief Export system information to HTML string
     * @param content The content to export
     * @return Exported HTML content as string
     */
    auto exportToString(const std::string& content) -> std::string override;

    /**
     * @brief Get the file extension for HTML format
     * @return ".html"
     */
    [[nodiscard]] auto getFileExtension() const -> std::string override;

    /**
     * @brief Get the MIME type for HTML format
     * @return "text/html"
     */
    [[nodiscard]] auto getMimeType() const -> std::string override;

private:
    /**
     * @brief Convert plain text content to HTML
     * @param content The plain text content
     * @return HTML formatted content
     */
    [[nodiscard]] auto convertToHtml(const std::string& content) const -> std::string;

    /**
     * @brief Generate HTML document structure
     * @param bodyContent The body content
     * @return Complete HTML document
     */
    [[nodiscard]] auto generateHtmlDocument(const std::string& bodyContent) const -> std::string;

    /**
     * @brief Generate CSS styles
     * @return CSS style string
     */
    [[nodiscard]] auto generateCss() const -> std::string;

    /**
     * @brief Generate HTML header
     * @return HTML header string
     */
    [[nodiscard]] auto generateHeader() const -> std::string;

    /**
     * @brief Generate HTML footer
     * @return HTML footer string
     */
    [[nodiscard]] auto generateFooter() const -> std::string;

    /**
     * @brief Convert ASCII table to HTML table
     * @param tableContent The ASCII table content
     * @return HTML table
     */
    [[nodiscard]] auto convertTableToHtml(const std::string& tableContent) const -> std::string;

    /**
     * @brief Escape HTML special characters
     * @param text The text to escape
     * @return HTML escaped text
     */
    [[nodiscard]] auto escapeText(const std::string& text) const -> std::string override;

    /**
     * @brief Add metadata to HTML document
     * @param content The content to add metadata to
     * @return Content with HTML metadata
     */
    [[nodiscard]] auto addMetadata(const std::string& content) const -> std::string override;

    /**
     * @brief Load external CSS file
     * @param cssPath Path to CSS file
     * @return CSS content or empty string if failed
     */
    [[nodiscard]] auto loadExternalCss(const std::string& cssPath) const -> std::string;

    /**
     * @brief Generate responsive meta tags
     * @return HTML meta tags for responsive design
     */
    [[nodiscard]] auto generateMetaTags() const -> std::string;

    /**
     * @brief Generate JavaScript for interactive features
     * @return JavaScript code
     */
    [[nodiscard]] auto generateJavaScript() const -> std::string;
};

} // namespace atom::system

#endif // ATOM_SYSINFO_PRINTER_EXPORTERS_HTML_EXPORTER_HPP
