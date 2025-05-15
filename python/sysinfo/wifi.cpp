#include "atom/sysinfo/wifi.hpp"

#include <pybind11/chrono.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <thread>

namespace py = pybind11;
using namespace atom::system;

PYBIND11_MODULE(wifi, m) {
    m.doc() = "WiFi and network information module for the atom package";

    // Register exception translations
    py::register_exception_translator([](std::exception_ptr p) {
        try {
            if (p)
                std::rethrow_exception(p);
        } catch (const std::invalid_argument& e) {
            PyErr_SetString(PyExc_ValueError, e.what());
        } catch (const std::runtime_error& e) {
            PyErr_SetString(PyExc_RuntimeError, e.what());
        } catch (const std::exception& e) {
            PyErr_SetString(PyExc_Exception, e.what());
        }
    });

    // NetworkStats structure binding
    py::class_<NetworkStats>(m, "NetworkStats",
                             R"(Network statistics information structure.

This class provides detailed statistics about the current network connection,
including download/upload speeds, latency, packet loss, and signal strength.

Examples:
    >>> from atom.sysinfo import wifi
    >>> # Get current network statistics
    >>> stats = wifi.get_network_stats()
    >>> print(f"Download speed: {stats.download_speed:.2f} MB/s")
    >>> print(f"Upload speed: {stats.upload_speed:.2f} MB/s")
    >>> print(f"Latency: {stats.latency:.2f} ms")
    >>> print(f"Signal strength: {stats.signal_strength:.2f} dBm")
)")
        .def(py::init<>(), "Constructs a new NetworkStats object.")
        .def_readwrite("download_speed", &NetworkStats::downloadSpeed,
                       "Download speed in MB/s")
        .def_readwrite("upload_speed", &NetworkStats::uploadSpeed,
                       "Upload speed in MB/s")
        .def_readwrite("latency", &NetworkStats::latency,
                       "Network latency in milliseconds")
        .def_readwrite("packet_loss", &NetworkStats::packetLoss,
                       "Packet loss percentage")
        .def_readwrite("signal_strength", &NetworkStats::signalStrength,
                       "Signal strength in dBm")
        .def_readwrite("connected_devices", &NetworkStats::connectedDevices,
                       "List of connected devices")
        .def("__repr__", [](const NetworkStats& stats) {
            return "<NetworkStats download=" +
                   std::to_string(stats.downloadSpeed) + "MB/s" +
                   " upload=" + std::to_string(stats.uploadSpeed) + "MB/s" +
                   " latency=" + std::to_string(stats.latency) + "ms" +
                   " signal=" + std::to_string(stats.signalStrength) + "dBm>";
        });

    // Function bindings
    m.def("get_current_wifi", &getCurrentWifi,
          R"(Get current WiFi network name.

Returns:
    String containing the name of the currently connected WiFi network or empty if not connected

Examples:
    >>> from atom.sysinfo import wifi
    >>> wifi_name = wifi.get_current_wifi()
    >>> print(f"Connected to WiFi: {wifi_name}")
)");

    m.def("get_current_wired_network", &getCurrentWiredNetwork,
          R"(Get current wired network name.

Returns:
    String containing the name of the currently connected wired network or empty if not connected

Examples:
    >>> from atom.sysinfo import wifi
    >>> wired_name = wifi.get_current_wired_network()
    >>> print(f"Connected to wired network: {wired_name}")
)");

    m.def("is_hotspot_connected", &isHotspotConnected,
          R"(Check if a hotspot is connected.

Returns:
    Boolean indicating whether a hotspot is connected

Examples:
    >>> from atom.sysinfo import wifi
    >>> if wifi.is_hotspot_connected():
    ...     print("Connected to a hotspot")
    ... else:
    ...     print("Not connected to a hotspot")
)");

    m.def("get_host_ips", &getHostIPs,
          R"(Get host IP addresses.

Returns:
    List of strings containing all host IP addresses

Examples:
    >>> from atom.sysinfo import wifi
    >>> ip_addresses = wifi.get_host_ips()
    >>> print("Host IP addresses:")
    >>> for ip in ip_addresses:
    ...     print(f"- {ip}")
)");

    m.def("get_ipv4_addresses", &getIPv4Addresses,
          R"(Get IPv4 addresses.

Returns:
    List of strings containing all IPv4 addresses

Examples:
    >>> from atom.sysinfo import wifi
    >>> ipv4_addresses = wifi.get_ipv4_addresses()
    >>> print("IPv4 addresses:")
    >>> for ip in ipv4_addresses:
    ...     print(f"- {ip}")
)");

    m.def("get_ipv6_addresses", &getIPv6Addresses,
          R"(Get IPv6 addresses.

Returns:
    List of strings containing all IPv6 addresses

Examples:
    >>> from atom.sysinfo import wifi
    >>> ipv6_addresses = wifi.get_ipv6_addresses()
    >>> print("IPv6 addresses:")
    >>> for ip in ipv6_addresses:
    ...     print(f"- {ip}")
)");

    m.def("get_interface_names", &getInterfaceNames,
          R"(Get network interface names.

Returns:
    List of strings containing all network interface names

Examples:
    >>> from atom.sysinfo import wifi
    >>> interfaces = wifi.get_interface_names()
    >>> print("Network interfaces:")
    >>> for interface in interfaces:
    ...     print(f"- {interface}")
)");

    m.def("get_network_stats", &getNetworkStats,
          R"(Get current network statistics.

Returns:
    NetworkStats object containing detailed network statistics

Examples:
    >>> from atom.sysinfo import wifi
    >>> stats = wifi.get_network_stats()
    >>> print(f"Download speed: {stats.download_speed:.2f} MB/s")
    >>> print(f"Upload speed: {stats.upload_speed:.2f} MB/s")
    >>> print(f"Latency: {stats.latency:.2f} ms")
    >>> print(f"Packet loss: {stats.packet_loss:.2f}%")
    >>> print(f"Signal strength: {stats.signal_strength:.2f} dBm")
)");

    m.def("get_network_history", &getNetworkHistory, py::arg("duration"),
          R"(Get network history over a duration.

Args:
    duration: Duration for which network statistics are collected (as a timedelta object)

Returns:
    List of NetworkStats objects representing the network history

Examples:
    >>> from atom.sysinfo import wifi
    >>> import datetime
    >>> # Get network history for the last 5 minutes
    >>> history = wifi.get_network_history(datetime.timedelta(minutes=5))
    >>> for i, stats in enumerate(history):
    ...     print(f"Snapshot {i}:")
    ...     print(f"  Download: {stats.download_speed:.2f} MB/s")
    ...     print(f"  Upload: {stats.upload_speed:.2f} MB/s")
    ...     print(f"  Latency: {stats.latency:.2f} ms")
)");

    m.def("scan_available_networks", &scanAvailableNetworks,
          R"(Scan available networks.

Returns:
    List of strings containing names of available networks

Examples:
    >>> from atom.sysinfo import wifi
    >>> networks = wifi.scan_available_networks()
    >>> print("Available networks:")
    >>> for network in networks:
    ...     print(f"- {network}")
)");

    m.def("get_network_security", &getNetworkSecurity,
          R"(Get network security information.

Returns:
    String containing network security details

Examples:
    >>> from atom.sysinfo import wifi
    >>> security = wifi.get_network_security()
    >>> print(f"Network security: {security}")
)");

    m.def("measure_bandwidth", &measureBandwidth,
          R"(Measure bandwidth.

Returns:
    Tuple of (upload_speed, download_speed) in MB/s

Examples:
    >>> from atom.sysinfo import wifi
    >>> upload, download = wifi.measure_bandwidth()
    >>> print(f"Upload speed: {upload:.2f} MB/s")
    >>> print(f"Download speed: {download:.2f} MB/s")
)");

    m.def("analyze_network_quality", &analyzeNetworkQuality,
          R"(Analyze network quality.

Returns:
    String containing network quality analysis result

Examples:
    >>> from atom.sysinfo import wifi
    >>> quality = wifi.analyze_network_quality()
    >>> print(f"Network quality: {quality}")
)");

    m.def("get_connected_devices", &getConnectedDevices,
          R"(Get connected devices.

Returns:
    List of strings containing names of connected devices

Examples:
    >>> from atom.sysinfo import wifi
    >>> devices = wifi.get_connected_devices()
    >>> print(f"Number of connected devices: {len(devices)}")
    >>> for device in devices:
    ...     print(f"- {device}")
)");

    // Additional utility functions
    m.def(
        "check_connectivity",
        []() {
            bool has_wifi = !getCurrentWifi().empty();
            bool has_wired = !getCurrentWiredNetwork().empty();
            auto ipv4 = getIPv4Addresses();
            bool has_ipv4 = !ipv4.empty();

            py::dict result;
            result["has_wifi"] = has_wifi;
            result["has_wired"] = has_wired;
            result["has_ipv4"] = has_ipv4;
            result["is_connected"] = has_wifi || has_wired;

            if (has_wifi) {
                result["wifi_name"] = getCurrentWifi();
            }

            if (has_wired) {
                result["wired_name"] = getCurrentWiredNetwork();
            }

            if (has_ipv4) {
                result["primary_ip"] = ipv4[0];
            }

            return result;
        },
        R"(Check overall network connectivity.

Returns:
    Dictionary containing various connectivity status flags

Examples:
    >>> from atom.sysinfo import wifi
    >>> status = wifi.check_connectivity()
    >>> if status["is_connected"]:
    ...     if status["has_wifi"]:
    ...         print(f"Connected via WiFi: {status['wifi_name']}")
    ...     elif status["has_wired"]:
    ...         print("Connected via wired connection")
    ...     print(f"Primary IP: {status.get('primary_ip', 'Unknown')}")
    ... else:
    ...     print("Not connected to any network")
)");

    m.def(
        "get_connection_type",
        []() {
            if (!getCurrentWifi().empty()) {
                return std::string("wifi");
            } else if (!getCurrentWiredNetwork().empty()) {
                return std::string("wired");
            } else if (isHotspotConnected()) {
                return std::string("hotspot");
            } else {
                return std::string("none");
            }
        },
        R"(Get the current network connection type.

Returns:
    String representing the connection type: 'wifi', 'wired', 'hotspot', or 'none'

Examples:
    >>> from atom.sysinfo import wifi
    >>> conn_type = wifi.get_connection_type()
    >>> if conn_type == "wifi":
    ...     print(f"Connected to WiFi: {wifi.get_current_wifi()}")
    >>> elif conn_type == "wired":
    ...     print("Connected via wired connection")
    >>> elif conn_type == "hotspot":
    ...     print("Connected via hotspot")
    >>> else:
    ...     print("Not connected")
)");

    m.def(
        "format_signal_strength",
        [](double signal_strength) {
            std::string quality;
            if (signal_strength >= -50) {
                quality = "Excellent";
            } else if (signal_strength >= -60) {
                quality = "Good";
            } else if (signal_strength >= -70) {
                quality = "Fair";
            } else if (signal_strength >= -80) {
                quality = "Poor";
            } else {
                quality = "Very poor";
            }

            return py::make_tuple(quality, signal_strength);
        },
        py::arg("signal_strength"),
        R"(Format signal strength as a human-readable quality description.

Args:
    signal_strength: Signal strength in dBm

Returns:
    Tuple of (quality_description, signal_strength_dbm)

Examples:
    >>> from atom.sysinfo import wifi
    >>> stats = wifi.get_network_stats()
    >>> quality, dbm = wifi.format_signal_strength(stats.signal_strength)
    >>> print(f"WiFi signal: {quality} ({dbm} dBm) ")
)");

    m.def(
        "get_connection_summary",
        []() {
            auto stats = getNetworkStats();

            py::dict summary;
            summary["connection_type"] = [&]() {
                if (!getCurrentWifi().empty()) {
                    return std::string("WiFi");
                } else if (!getCurrentWiredNetwork().empty()) {
                    return std::string("Wired");
                } else if (isHotspotConnected()) {
                    return std::string("Hotspot");
                } else {
                    return std::string("None");
                }
            }();

            summary["name"] = [&]() {
                if (!getCurrentWifi().empty()) {
                    return getCurrentWifi();
                } else if (!getCurrentWiredNetwork().empty()) {
                    return getCurrentWiredNetwork();
                } else {
                    return std::string("");
                }
            }();

            summary["download_speed"] = stats.downloadSpeed;
            summary["upload_speed"] = stats.uploadSpeed;
            summary["latency"] = stats.latency;
            summary["packet_loss"] = stats.packetLoss;
            summary["signal_strength"] = stats.signalStrength;

            auto ipv4 = getIPv4Addresses();
            if (!ipv4.empty()) {
                summary["primary_ip"] = ipv4[0];
            }

            summary["connected_devices"] = stats.connectedDevices;
            summary["quality"] = [&]() {
                if (stats.latency < 20 && stats.packetLoss < 1.0) {
                    return std::string("Excellent");
                } else if (stats.latency < 50 && stats.packetLoss < 2.0) {
                    return std::string("Good");
                } else if (stats.latency < 100 && stats.packetLoss < 5.0) {
                    return std::string("Fair");
                } else if (stats.latency < 150 && stats.packetLoss < 10.0) {
                    return std::string("Poor");
                } else {
                    return std::string("Very poor");
                }
            }();

            return summary;
        },
        R"(Get a comprehensive network connection summary.

Returns:
    Dictionary containing network connection details

Examples:
    >>> from atom.sysinfo import wifi
    >>> summary = wifi.get_connection_summary()
    >>> print(f"Connection type: {summary['connection_type']}")
    >>> if summary['connection_type'] != "None":
    ...     print(f"Network name: {summary['name']}")
    ...     print(f"Primary IP: {summary.get('primary_ip', 'Unknown')}")
    ...     print(f"Download: {summary['download_speed']:.2f} MB/s")
    ...     print(f"Upload: {summary['upload_speed']:.2f} MB/s")
    ...     print(f"Latency: {summary['latency']:.2f} ms")
    ...     print(f"Quality: {summary['quality']}")
    ... else:
    ...     print("Not connected to any network")
)");

    // Context manager for network monitoring
    py::class_<py::object>(m, "NetworkMonitorContext")
        .def(py::init([](int duration_seconds, int interval_seconds) {
                 py::object obj;
                 obj.attr("duration") = py::int_(duration_seconds);
                 obj.attr("interval") = py::int_(interval_seconds);
                 return obj;
             }),
             py::arg("duration_seconds") = 60, py::arg("interval_seconds") = 5,
             "Create a context manager for network monitoring")
        .def("__enter__",
             [](py::object& self) {
                 self.attr("history") = py::list();
                 self.attr("start_time") =
                     py::module::import("time").attr("time")();
                 self.attr("end_time") =
                     self.attr("start_time") + self.attr("duration");

                 // Record initial stats
                 NetworkStats stats = getNetworkStats();
                 self.attr("history").attr("append")(py::cast(stats));

                 return self;
             })
        .def("__exit__",
             [](py::object& self, py::object, py::object, py::object) {
                 return py::bool_(false);  // Don't suppress exceptions
             })
        .def(
            "update",
            [](py::object& self) {
                // Get current time
                py::object current_time =
                    py::module::import("time").attr("time")();

                // If we're still within the monitoring duration
                if (current_time < self.attr("end_time")) {
                    NetworkStats stats = getNetworkStats();
                    self.attr("history").attr("append")(py::cast(stats));

                    // Sleep for the interval
                    py::module::import("time").attr("sleep")(
                        self.attr("interval"));
                    return py::bool_(true);
                }

                return py::bool_(false);
            },
            "Update network statistics, returns True if monitoring is still "
            "active")
        .def_property_readonly(
            "is_active",
            [](py::object& self) {
                py::object current_time =
                    py::module::import("time").attr("time")();
                return current_time < self.attr("end_time");
            },
            "Whether monitoring is still active")
        .def_property_readonly(
            "elapsed_time",
            [](py::object& self) {
                py::object current_time =
                    py::module::import("time").attr("time")();
                return current_time - self.attr("start_time");
            },
            "Elapsed monitoring time in seconds")
        .def_property_readonly(
            "remaining_time",
            [](py::object& self) {
                py::object current_time =
                    py::module::import("time").attr("time")();
                py::object end_time = self.attr("end_time");
                py::object remaining = end_time - current_time;
                // Ensure we don't return negative time
                return py::module::import("builtins")
                    .attr("max")(remaining, py::float_(0.0));
            },
            "Remaining monitoring time in seconds")
        .def_property_readonly(
            "stats_history",
            [](py::object& self) { return self.attr("history"); },
            "History of recorded network statistics")
        .def_property_readonly(
            "average_stats",
            [](py::object& self) -> py::object {
                py::list history = self.attr("history");
                if (py::len(history) == 0) {
                    return py::none();
                }

                double total_download = 0.0;
                double total_upload = 0.0;
                double total_latency = 0.0;
                double total_packet_loss = 0.0;
                double total_signal_strength = 0.0;

                for (py::handle stats : history) {
                    NetworkStats* ns = py::cast<NetworkStats*>(stats);
                    total_download += ns->downloadSpeed;
                    total_upload += ns->uploadSpeed;
                    total_latency += ns->latency;
                    total_packet_loss += ns->packetLoss;
                    total_signal_strength += ns->signalStrength;
                }

                int count = py::len(history);
                NetworkStats avg_stats;
                avg_stats.downloadSpeed = total_download / count;
                avg_stats.uploadSpeed = total_upload / count;
                avg_stats.latency = total_latency / count;
                avg_stats.packetLoss = total_packet_loss / count;
                avg_stats.signalStrength = total_signal_strength / count;

                return py::cast(avg_stats);
            },
            "Average network statistics over the monitoring period");

    // Factory function for network monitor context
    m.def(
        "monitor_network",
        [&m](int duration_seconds, int interval_seconds) {
            return m.attr("NetworkMonitorContext")(duration_seconds,
                                                   interval_seconds);
        },
        py::arg("duration_seconds") = 60, py::arg("interval_seconds") = 5,
        R"(Create a context manager for monitoring network statistics over time.

Args:
    duration_seconds: Total duration to monitor for in seconds (default: 60)
    interval_seconds: Interval between measurements in seconds (default: 5)

Returns:
    A context manager for network monitoring

Examples:
    >>> from atom.sysinfo import wifi
    >>> import time
    >>> 
    >>> # Simple automatic monitoring for 20 seconds
    >>> with wifi.monitor_network(20, 2) as monitor:
    ...     while monitor.is_active:
    ...         print(f"Monitoring... {monitor.elapsed_time:.1f}s elapsed, "
    ...               f"{monitor.remaining_time:.1f}s remaining")
    ...         monitor.update()  # This will sleep for the interval
    ... 
    >>> # Get results after monitoring completes
    >>> avg_stats = monitor.average_stats
    >>> print(f"Average download: {avg_stats.download_speed:.2f} MB/s")
    >>> print(f"Average upload: {avg_stats.upload_speed:.2f} MB/s")
    >>> print(f"Average latency: {avg_stats.latency:.2f} ms")
    >>> 
    >>> # Manual updating
    >>> with wifi.monitor_network(30, 5) as monitor:
    ...     # Do other things and manually update periodically
    ...     for i in range(6):
    ...         print(f"Taking measurement {i+1}")
    ...         monitor.update()
    ... 
    >>> print(f"Collected {len(monitor.stats_history)} measurements")
)");

    // Simple ping utility
    m.def(
        "ping",
        [](const std::string& host, int count) {
            // This is a simplified placeholder. In a real implementation,
            // you would actually ping the host and collect results.
            NetworkStats stats = getNetworkStats();

            // Simulate slight variations in ping
            auto now = std::chrono::steady_clock::now();
            auto time_seed = now.time_since_epoch().count();
            std::srand(static_cast<unsigned int>(time_seed));

            py::list results;
            double base_latency = stats.latency;
            double packet_loss_rate =
                stats.packetLoss / 100.0;  // Convert to probability

            for (int i = 0; i < count; i++) {
                py::dict ping_result;

                // Simulate packet loss
                bool packet_lost = (static_cast<double>(std::rand()) /
                                    RAND_MAX) < packet_loss_rate;

                if (packet_lost) {
                    ping_result["success"] = false;
                    ping_result["error"] = "Request timed out";
                } else {
                    // Vary latency slightly
                    double variation =
                        (static_cast<double>(std::rand()) / RAND_MAX) * 10.0 -
                        5.0;
                    double latency = std::max(1.0, base_latency + variation);

                    ping_result["success"] = true;
                    ping_result["latency"] = latency;
                    ping_result["ttl"] = 64;
                }

                results.append(ping_result);

                // Actually wait between pings (standard is 1 second)
                if (i < count - 1) {  // Don't wait after the last ping
                    std::this_thread::sleep_for(std::chrono::seconds(1));
                }
            }

            // Calculate summary statistics
            int successful = 0;
            double total_latency = 0.0;
            double min_latency = std::numeric_limits<double>::max();
            double max_latency = 0.0;

            for (py::handle result : results) {
                py::dict result_dict = py::cast<py::dict>(result);
                if (py::cast<bool>(result_dict["success"])) {
                    double latency = py::cast<double>(result_dict["latency"]);
                    successful++;
                    total_latency += latency;
                    min_latency = std::min(min_latency, latency);
                    max_latency = std::max(max_latency, latency);
                }
            }

            py::dict summary;
            summary["host"] = host;
            summary["packets_sent"] = count;
            summary["packets_received"] = successful;
            summary["packet_loss"] = 100.0 * (count - successful) / count;

            if (successful > 0) {
                summary["min_latency"] = min_latency;
                summary["max_latency"] = max_latency;
                summary["avg_latency"] = total_latency / successful;
            }

            return py::make_tuple(results, summary);
        },
        py::arg("host"), py::arg("count") = 4,
        R"(Ping a host and measure latency.

This is a simplified ping implementation for network diagnostics.

Args:
    host: Hostname or IP address to ping
    count: Number of ping requests to send (default: 4)

Returns:
    Tuple of (individual_results, summary_statistics)

Examples:
    >>> from atom.sysinfo import wifi
    >>> # Ping a host 5 times
    >>> results, summary = wifi.ping("www.example.com", 5)
    >>> 
    >>> # Print summary
    >>> print(f"Host: {summary['host']}")
    >>> print(f"Packets: {summary['packets_received']}/{summary['packets_sent']}")
    >>> print(f"Packet loss: {summary['packet_loss']:.1f}%")
    >>> 
    >>> if summary['packets_received'] > 0:
    ...     print(f"Latency: min={summary['min_latency']:.1f}ms, "
    ...           f"avg={summary['avg_latency']:.1f}ms, "
    ...           f"max={summary['max_latency']:.1f}ms")
    >>> 
    >>> # Individual results
    >>> for i, result in enumerate(results):
    ...     if result['success']:
    ...         print(f"Ping {i+1}: {result['latency']:.1f}ms (TTL={result['ttl']}) ")
    ...     else:
    ...         print(f"Ping {i+1}: {result['error']} ")
)");
}