#ifndef ATOM_EXTRA_INICPP_FORMAT_CONVERTER_HPP
#define ATOM_EXTRA_INICPP_FORMAT_CONVERTER_HPP

#include <sstream>
#include <string>

#include "common.hpp"
#include "file.hpp"

#if INICPP_CONFIG_FORMAT_CONVERSION

// Forward declaration
namespace inicpp {
template <typename Comparator>
class IniFileBase;
using IniFile = IniFileBase<std::less<>>;
}  // namespace inicpp

namespace inicpp {

/**
 * @brief Supported configuration file formats
 */
enum class FormatType {
    INI,   ///< INI format
    JSON,  ///< JSON format
    XML,   ///< XML format
    YAML   ///< YAML format
};

/**
 * @class FormatConverter
 * @brief Converts between different configuration file formats
 */
class FormatConverter {
public:
    /**
     * @brief Converts INI format to JSON
     * @param iniFile INI file object
     * @return JSON format string
     */
    static std::string toJson(const IniFile& iniFile);

    /**
     * @brief Converts INI format to XML
     * @param iniFile INI file object
     * @return XML format string
     */
    static std::string toXml(const IniFile& iniFile);

    /**
     * @brief Converts INI format to YAML
     * @param iniFile INI file object
     * @return YAML format string
     */
    static std::string toYaml(const IniFile& iniFile);

    /**
     * @brief Imports from JSON format to INI
     * @param jsonContent JSON format string
     * @return Populated INI file object
     */
    static IniFile fromJson(const std::string& jsonContent);

    /**
     * @brief Imports from XML format to INI
     * @param xmlContent XML format string
     * @return Populated INI file object
     */
    static IniFile fromXml(const std::string& xmlContent);

    /**
     * @brief Imports from YAML format to INI
     * @param yamlContent YAML format string
     * @return Populated INI file object
     */
    static IniFile fromYaml(const std::string& yamlContent);

    /**
     * @brief Exports INI file according to the specified format
     * @param iniFile INI file object
     * @param format Target format
     * @return Converted string
     */
    static std::string exportTo(const IniFile& iniFile, FormatType format);

    /**
     * @brief Imports INI file from the specified format
     * @param content Source format content
     * @param format Source format type
     * @return Populated INI file object
     */
    static IniFile importFrom(const std::string& content, FormatType format);

private:
    /**
     * @brief Recursively converts JSON object to INI file
     * @param json JSON object
     * @param iniFile INI file object reference
     * @param currentPath Current path
     */
    static void convertJsonObjectToIni(const void* json, IniFile& iniFile,
                                       const std::string& currentPath);

    /**
     * @brief Recursively converts XML node to INI file
     * @param xml XML node
     * @param iniFile INI file object reference
     * @param currentPath Current path
     */
    static void convertXmlNodeToIni(const void* xml, IniFile& iniFile,
                                    const std::string& currentPath);

    /**
     * @brief Recursively converts YAML node to INI file
     * @param yaml YAML node
     * @param iniFile INI file object reference
     * @param currentPath Current path
     */
    static void convertYamlNodeToIni(const void* yaml, IniFile& iniFile,
                                     const std::string& currentPath);
};

// Implement toJson method
inline std::string FormatConverter::toJson(const IniFile& iniFile) {
    std::ostringstream json;
    json << "{\n";

    bool firstSection = true;
    for (const auto& [sectionName, section] : iniFile) {
        if (!firstSection) {
            json << ",\n";
        }
        firstSection = false;

        json << "  \"" << sectionName << "\": {\n";

        bool firstField = true;
        for (const auto& [fieldName, field] : section) {
            if (!firstField) {
                json << ",\n";
            }
            firstField = false;

            // JSON escape field value
            std::string value = std::string(field.raw_value());
            std::string escapedValue;
            escapedValue.reserve(value.size());

            for (char c : value) {
                switch (c) {
                    case '\"':
                        escapedValue += "\\\"";
                        break;
                    case '\\':
                        escapedValue += "\\\\";
                        break;
                    case '\b':
                        escapedValue += "\\b";
                        break;
                    case '\f':
                        escapedValue += "\\f";
                        break;
                    case '\n':
                        escapedValue += "\\n";
                        break;
                    case '\r':
                        escapedValue += "\\r";
                        break;
                    case '\t':
                        escapedValue += "\\t";
                        break;
                    default:
                        if (static_cast<unsigned char>(c) < 0x20) {
                            char buf[7];
                            snprintf(buf, sizeof(buf), "\\u%04x", c);
                            escapedValue += buf;
                        } else {
                            escapedValue += c;
                        }
                }
            }

            json << "    \"" << fieldName << "\": \"" << escapedValue << "\"";
        }

        if (!section.empty()) {
            json << "\n  }";
        } else {
            json << "  }";
        }
    }

    json << "\n}";
    return json.str();
}

// Implement toXml method
inline std::string FormatConverter::toXml(const IniFile& iniFile) {
    std::ostringstream xml;
    xml << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
    xml << "<ini>\n";

    for (const auto& [sectionName, section] : iniFile) {
        xml << "  <section name=\"" << sectionName << "\">\n";

        for (const auto& [fieldName, field] : section) {
            // XML escape field value
            std::string value = std::string(field.raw_value());
            std::string escapedValue;
            escapedValue.reserve(value.size());

            for (char c : value) {
                switch (c) {
                    case '&':
                        escapedValue += "&amp;";
                        break;
                    case '<':
                        escapedValue += "&lt;";
                        break;
                    case '>':
                        escapedValue += "&gt;";
                        break;
                    case '\"':
                        escapedValue += "&quot;";
                        break;
                    case '\'':
                        escapedValue += "&apos;";
                        break;
                    default:
                        escapedValue += c;
                }
            }

            xml << "    <field name=\"" << fieldName << "\">" << escapedValue
                << "</field>\n";
        }

        xml << "  </section>\n";
    }

    xml << "</ini>";
    return xml.str();
}

// Implement toYaml method
inline std::string FormatConverter::toYaml(const IniFile& iniFile) {
    std::ostringstream yaml;

    for (const auto& [sectionName, section] : iniFile) {
        yaml << sectionName << ":\n";

        for (const auto& [fieldName, field] : section) {
            std::string value = std::string(field.raw_value());

            // Check if multiline representation is needed
            bool isMultiline = value.find('\n') != std::string::npos;

            if (isMultiline) {
                yaml << "  " << fieldName << ": |-\n";
                std::istringstream iss(value);
                std::string line;
                while (std::getline(iss, line)) {
                    yaml << "    " << line << "\n";
                }
            } else {
                // Check if quotes are needed
                bool needQuotes = value.empty() || value.front() == ' ' ||
                                  value.back() == ' ' ||
                                  value.find(':') != std::string::npos ||
                                  value.find('#') != std::string::npos;

                if (needQuotes) {
                    // Escape quotes
                    std::string escapedValue;
                    escapedValue.reserve(value.size());
                    for (char c : value) {
                        if (c == '\"') {
                            escapedValue += "\\\"";
                        } else if (c == '\\') {
                            escapedValue += "\\\\";
                        } else {
                            escapedValue += c;
                        }
                    }
                    yaml << "  " << fieldName << ": \"" << escapedValue
                         << "\"\n";
                } else {
                    yaml << "  " << fieldName << ": " << value << "\n";
                }
            }
        }

        yaml << "\n";
    }

    return yaml.str();
}

// Export method implementation
inline std::string FormatConverter::exportTo(const IniFile& iniFile,
                                             FormatType format) {
    switch (format) {
        case FormatType::JSON:
            return toJson(iniFile);
        case FormatType::XML:
            return toXml(iniFile);
        case FormatType::YAML:
            return toYaml(iniFile);
        case FormatType::INI:
        default:
            return iniFile.encode();
    }
}

// Import method implementation
inline IniFile FormatConverter::importFrom(const std::string& content,
                                           FormatType format) {
    switch (format) {
        case FormatType::JSON:
            return fromJson(content);
        case FormatType::XML:
            return fromXml(content);
        case FormatType::YAML:
            return fromYaml(content);
        case FormatType::INI:
        default: {
            IniFile iniFile;
            iniFile.decode(content);
            // return iniFile;
        }
    }
}

// Note: The complete implementation of fromJson, fromXml, and fromYaml requires
// external JSON, XML, and YAML parsing libraries. Here is a framework for a
// simple implementation, which should be modified appropriately based on the
// library used in the actual project.

}  // namespace inicpp

#endif  // INICPP_CONFIG_FORMAT_CONVERSION

#endif  // ATOM_EXTRA_INICPP_FORMAT_CONVERTER_HPP
