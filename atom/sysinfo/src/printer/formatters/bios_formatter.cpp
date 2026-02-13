#include "bios_formatter.hpp"
#include <spdlog/spdlog.h>
#include <format>
#include <sstream>

namespace atom::system {

BiosFormatter::BiosFormatter(const FormatterOptions& options) {
    setOptions(options);
}

auto BiosFormatter::format(const BiosInfoData& info) const -> std::string {
    return format(info, "BIOS Information");
}

auto BiosFormatter::format(const BiosInfoData& info, const std::string& title) const -> std::string {
    std::stringstream ss;

    ss << createTableHeader(title);
    ss << createTableRow("Vendor", info.manufacturer);
    ss << createTableRow("Version", info.version);
    ss << createTableRow("Release Date", info.releaseDate);

    if (!info.serialNumber.empty()) {
        ss << createTableRow("Serial Number", info.serialNumber);
    }

    if (!info.characteristics.empty()) {
        ss << createTableRow("Characteristics", info.characteristics);
    }

    ss << createTableRow("Upgradeable", info.isUpgradeable ? "Yes" : "No");

    ss << createTableFooter();

    return addTimestamp(ss.str());
}

auto BiosFormatter::formatBasic(const BiosInfoData& info) const -> std::string {
    return std::format("{} {} ({})", info.manufacturer, info.version, info.releaseDate);
}

} // namespace atom::system
