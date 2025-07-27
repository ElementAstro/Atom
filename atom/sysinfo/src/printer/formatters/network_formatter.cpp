#include "network_formatter.hpp"
#include <spdlog/spdlog.h>
#include <format>
#include <sstream>

namespace atom::system {

NetworkFormatter::NetworkFormatter(const FormatterOptions& options) {
    setOptions(options);
}

auto NetworkFormatter::format(const NetworkStats& stats) const -> std::string {
    return format(stats, "Network Information");
}

auto NetworkFormatter::format(const NetworkStats& stats, const std::string& title) const -> std::string {
    std::stringstream ss;

    ss << createTableHeader(title);

    // Current WiFi connection
    auto currentWifi = getCurrentWifi();
    if (!currentWifi.empty()) {
        ss << createTableRow("Current WiFi", currentWifi);
    }

    // Wired network
    auto wiredNetwork = getCurrentWiredNetwork();
    if (!wiredNetwork.empty()) {
        ss << createTableRow("Wired Network", wiredNetwork);
    }

    // Network performance
    ss << createTableRow("Download Speed", formatWithUnits(stats.downloadSpeed, "MB/s"));
    ss << createTableRow("Upload Speed", formatWithUnits(stats.uploadSpeed, "MB/s"));
    ss << createTableRow("Latency", formatWithUnits(stats.latency, "ms"));
    ss << createTableRow("Packet Loss", formatPercentage(stats.packetLoss));

    if (stats.signalStrength != 0) {
        ss << createTableRow("Signal Strength", formatWithUnits(stats.signalStrength, "dBm"));
    }

    // Connected devices
    if (!stats.connectedDevices.empty()) {
        ss << createTableRow("Connected Devices", std::to_string(stats.connectedDevices.size()));

        if (options_.style == FormatterStyle::DETAILED || options_.style == FormatterStyle::VERBOSE) {
            for (size_t i = 0; i < stats.connectedDevices.size() && i < 5; ++i) {
                ss << createTableRow("  Device " + std::to_string(i + 1), stats.connectedDevices[i]);
            }
            if (stats.connectedDevices.size() > 5) {
                ss << createTableRow("  ...", "(" + std::to_string(stats.connectedDevices.size() - 5) + " more)");
            }
        }
    }

    // Hotspot status
    bool hotspotConnected = isHotspotConnected();
    ss << createTableRow("Hotspot", hotspotConnected ? "Connected" : "Disconnected");

    ss << createTableFooter();

    return addTimestamp(ss.str());
}

auto NetworkFormatter::formatBasic(const NetworkStats& stats) const -> std::string {
    auto currentWifi = getCurrentWifi();
    if (!currentWifi.empty()) {
        return std::format("WiFi: {} (↓{:.1f} MB/s, ↑{:.1f} MB/s)",
                          currentWifi, stats.downloadSpeed, stats.uploadSpeed);
    }

    auto wiredNetwork = getCurrentWiredNetwork();
    if (!wiredNetwork.empty()) {
        return std::format("Wired: {} (↓{:.1f} MB/s, ↑{:.1f} MB/s)",
                          wiredNetwork, stats.downloadSpeed, stats.uploadSpeed);
    }

    return "No network connection";
}

auto NetworkFormatter::formatInterfaces() const -> std::string {
    std::stringstream ss;

    ss << createTableHeader("Network Interfaces");

    auto interfaces = getInterfaceNames();
    auto ipv4Addresses = getIPv4Addresses();
    auto ipv6Addresses = getIPv6Addresses();

    for (size_t i = 0; i < interfaces.size(); ++i) {
        ss << createTableRow("Interface " + std::to_string(i + 1), interfaces[i]);
    }

    if (!ipv4Addresses.empty()) {
        ss << createTableRow("IPv4 Addresses", std::to_string(ipv4Addresses.size()) + " addresses");
        if (options_.style == FormatterStyle::DETAILED || options_.style == FormatterStyle::VERBOSE) {
            for (size_t i = 0; i < ipv4Addresses.size() && i < 3; ++i) {
                ss << createTableRow("  IPv4 " + std::to_string(i + 1), ipv4Addresses[i]);
            }
        }
    }

    if (!ipv6Addresses.empty()) {
        ss << createTableRow("IPv6 Addresses", std::to_string(ipv6Addresses.size()) + " addresses");
        if (options_.style == FormatterStyle::DETAILED || options_.style == FormatterStyle::VERBOSE) {
            for (size_t i = 0; i < ipv6Addresses.size() && i < 3; ++i) {
                ss << createTableRow("  IPv6 " + std::to_string(i + 1), ipv6Addresses[i]);
            }
        }
    }

    ss << createTableFooter();

    return ss.str();
}

auto NetworkFormatter::formatConnections() const -> std::string {
    std::stringstream ss;

    ss << createTableHeader("Network Connections");

    // Current connections
    auto currentWifi = getCurrentWifi();
    if (!currentWifi.empty()) {
        ss << createTableRow("WiFi Connection", currentWifi);
    }

    auto wiredNetwork = getCurrentWiredNetwork();
    if (!wiredNetwork.empty()) {
        ss << createTableRow("Wired Connection", wiredNetwork);
    }

    // Available networks (if in verbose mode)
    if (options_.style == FormatterStyle::VERBOSE) {
        try {
            auto availableNetworks = scanAvailableNetworks();
            if (!availableNetworks.empty()) {
                ss << createTableRow("Available Networks", std::to_string(availableNetworks.size()) + " networks");
                for (size_t i = 0; i < availableNetworks.size() && i < 5; ++i) {
                    ss << createTableRow("  Network " + std::to_string(i + 1), availableNetworks[i]);
                }
            }
        } catch (const std::exception& e) {
            ss << createTableRow("Available Networks", "Scan failed");
        }
    }

    ss << createTableFooter();

    return ss.str();
}

} // namespace atom::system
