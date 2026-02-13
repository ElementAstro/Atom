#include "markdown_exporter.hpp"
#include <spdlog/spdlog.h>
#include <format>
#include <sstream>
#include <regex>

namespace atom::system {

MarkdownExporter::MarkdownExporter(const ExportOptions& options) {
    setOptions(options);
}

bool MarkdownExporter::exportToFile(const std::string& content, const std::string& filename) {
    auto markdownContent = exportToString(content);
    return writeToFile(markdownContent, filename);
}

auto MarkdownExporter::exportToString(const std::string& content) -> std::string {
    auto markdownContent = convertToMarkdown(content);
    auto document = generateMarkdownDocument(markdownContent);
    return addMetadata(document);
}

auto MarkdownExporter::getFileExtension() const -> std::string {
    return ".md";
}

auto MarkdownExporter::getMimeType() const -> std::string {
    return "text/markdown";
}

auto MarkdownExporter::convertToMarkdown(const std::string& content) const -> std::string {
    std::string markdownContent = content;

    // Convert ASCII tables to Markdown tables
    markdownContent = convertTableToMarkdown(markdownContent);

    // Convert headers (lines with === or ---)
    std::regex headerRegex("=== (.+) ===");
    markdownContent = std::regex_replace(markdownContent, headerRegex, "## $1");

    // Convert table footers (lines with --- only)
    std::regex footerRegex("^-{3,}$", std::regex_constants::multiline);
    markdownContent = std::regex_replace(markdownContent, footerRegex, "");

    return markdownContent;
}

auto MarkdownExporter::generateMarkdownDocument(const std::string& bodyContent) const -> std::string {
    std::stringstream ss;

    ss << generateHeader();
    ss << bodyContent;
    ss << generateFooter();

    return ss.str();
}

auto MarkdownExporter::convertTableToMarkdown(const std::string& tableContent) const -> std::string {
    std::stringstream ss;
    std::istringstream iss(tableContent);
    std::string line;
    bool inTable = false;
    bool headerWritten = false;

    while (std::getline(iss, line)) {
        // Check if this is a table row (contains |)
        if (line.find('|') != std::string::npos && line.find("===") == std::string::npos) {
            if (!inTable) {
                inTable = true;
                headerWritten = false;
            }

            // Clean up the line and convert to Markdown table format
            std::string cleanLine = line;

            // Remove leading/trailing whitespace
            cleanLine = std::regex_replace(cleanLine, std::regex("^\\s+|\\s+$"), "");

            // Ensure proper Markdown table format
            if (!cleanLine.empty() && cleanLine.front() != '|') {
                cleanLine = "| " + cleanLine;
            }
            if (!cleanLine.empty() && cleanLine.back() != '|') {
                cleanLine += " |";
            }

            ss << cleanLine << "\n";

            // Add header separator after first row
            if (!headerWritten) {
                // Count columns
                int columns = std::count(cleanLine.begin(), cleanLine.end(), '|') - 1;
                ss << "|";
                for (int i = 0; i < columns; ++i) {
                    ss << " --- |";
                }
                ss << "\n";
                headerWritten = true;
            }
        } else {
            if (inTable) {
                ss << "\n"; // Add spacing after table
                inTable = false;
            }

            // Skip table borders and footers
            if (line.find("===") == std::string::npos &&
                line.find("---") == std::string::npos &&
                !line.empty()) {
                ss << line << "\n";
            }
        }
    }

    return ss.str();
}

auto MarkdownExporter::escapeText(const std::string& text) const -> std::string {
    std::string result = text;

    // Escape Markdown special characters
    std::regex backslashRegex("\\\\");
    result = std::regex_replace(result, backslashRegex, "\\\\");

    std::regex asteriskRegex("\\*");
    result = std::regex_replace(result, asteriskRegex, "\\*");

    std::regex underscoreRegex("_");
    result = std::regex_replace(result, underscoreRegex, "\\_");

    std::regex backtickRegex("`");
    result = std::regex_replace(result, backtickRegex, "\\`");

    std::regex hashRegex("^#", std::regex_constants::multiline);
    result = std::regex_replace(result, hashRegex, "\\#");

    return result;
}

auto MarkdownExporter::addMetadata(const std::string& content) const -> std::string {
    if (!options_.includeMetadata) {
        return content;
    }

    // Markdown metadata is added in the generateHeader method
    return content;
}

auto MarkdownExporter::generateHeader() const -> std::string {
    std::stringstream ss;

    // Main title
    ss << "# " << options_.title << "\n\n";

    // Description
    if (!options_.description.empty()) {
        ss << "> " << options_.description << "\n\n";
    }

    // Metadata
    if (options_.includeMetadata) {
        if (options_.includeTimestamp) {
            ss << "**Generated at:** " << getCurrentTimestamp() << "\n\n";
        }

        if (!options_.author.empty()) {
            ss << "**Author:** " << options_.author << "\n\n";
        }

        ss << "**Generator:** Atom System Information Printer\n\n";
        ss << "---\n\n";
    }

    return ss.str();
}

auto MarkdownExporter::generateFooter() const -> std::string {
    std::stringstream ss;

    ss << "\n---\n\n";
    ss << "*Generated by Atom System Information Printer*\n";

    return ss.str();
}

} // namespace atom::system
