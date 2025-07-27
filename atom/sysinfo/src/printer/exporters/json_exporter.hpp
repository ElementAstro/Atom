/**
 * @file json_exporter.hpp
 * @brief JSON exporter for system information
 *
 * @copyright Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#ifndef ATOM_SYSINFO_PRINTER_EXPORTERS_JSON_EXPORTER_HPP
#define ATOM_SYSINFO_PRINTER_EXPORTERS_JSON_EXPORTER_HPP

#include "base_exporter.hpp"

namespace atom::system {

class JsonExporter : public BaseExporter {
public:
    JsonExporter() = default;
    explicit JsonExporter(const ExportOptions& options);
    
    bool exportToFile(const std::string& content, const std::string& filename) override;
    auto exportToString(const std::string& content) -> std::string override;
    [[nodiscard]] auto getFileExtension() const -> std::string override;
    [[nodiscard]] auto getMimeType() const -> std::string override;

private:
    [[nodiscard]] auto convertToJson(const std::string& content) const -> std::string;
    [[nodiscard]] auto escapeText(const std::string& text) const -> std::string override;
};

} // namespace atom::system

#endif // ATOM_SYSINFO_PRINTER_EXPORTERS_JSON_EXPORTER_HPP
