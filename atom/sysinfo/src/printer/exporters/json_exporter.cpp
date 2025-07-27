#include "json_exporter.hpp"
#include <spdlog/spdlog.h>
#include <format>
#include <sstream>
#include <regex>

namespace atom::system {

JsonExporter::JsonExporter(const ExportOptions& options) {
    setOptions(options);
}

bool JsonExporter::exportToFile(const std::string& content, const std::string& filename) {
    auto jsonContent = exportToString(content);
    return writeToFile(jsonContent, filename);
}

auto JsonExporter::exportToString(const std::string& content) -> std::string {
    std::stringstream ss;
    
    ss << "{\n";
    
    if (options_.includeMetadata) {
        ss << "  \"metadata\": {\n";
        ss << "    \"title\": \"" << escapeText(options_.title) << "\",\n";
        ss << "    \"generator\": \"Atom System Information Printer\",\n";
        if (!options_.author.empty()) {
            ss << "    \"author\": \"" << escapeText(options_.author) << "\",\n";
        }
        if (!options_.description.empty()) {
            ss << "    \"description\": \"" << escapeText(options_.description) << "\",\n";
        }
        if (options_.includeTimestamp) {
            ss << "    \"timestamp\": \"" << getCurrentTimestamp() << "\",\n";
        }
        ss << "    \"format\": \"json\"\n";
        ss << "  },\n";
    }
    
    ss << "  \"system_information\": {\n";
    ss << convertToJson(content);
    ss << "  }\n";
    ss << "}\n";
    
    return ss.str();
}

auto JsonExporter::getFileExtension() const -> std::string {
    return ".json";
}

auto JsonExporter::getMimeType() const -> std::string {
    return "application/json";
}

auto JsonExporter::convertToJson(const std::string& content) const -> std::string {
    // Simplified JSON conversion
    // In a real implementation, you would parse the structured content properly
    std::stringstream ss;
    ss << "    \"raw_content\": \"" << escapeText(content) << "\"\n";
    return ss.str();
}

auto JsonExporter::escapeText(const std::string& text) const -> std::string {
    std::string result = text;
    
    // Escape JSON special characters
    std::regex backslashRegex("\\\\");
    result = std::regex_replace(result, backslashRegex, "\\\\");
    
    std::regex quotRegex("\"");
    result = std::regex_replace(result, quotRegex, "\\\"");
    
    std::regex newlineRegex("\n");
    result = std::regex_replace(result, newlineRegex, "\\n");
    
    std::regex tabRegex("\t");
    result = std::regex_replace(result, tabRegex, "\\t");
    
    std::regex crRegex("\r");
    result = std::regex_replace(result, crRegex, "\\r");
    
    return result;
}

} // namespace atom::system
