#include "client_config.hpp"

#include <spdlog/spdlog.h>
#include <fstream>
#include "atom/type/json.hpp"

using json = nlohmann::json;

namespace atom::extra::asio::sse {

ClientConfig ClientConfig::from_file(const std::string& filename) {
    ClientConfig config;

    std::ifstream file(filename);
    if (!file.is_open()) {
        spdlog::warn("Configuration file {} not found, using defaults",
                     filename);
        return config;
    }

    try {
        json j;
        file >> j;

        const auto& root = j;

        if (auto it = root.find("host"); it != root.end())
            config.host = *it;
        if (auto it = root.find("port"); it != root.end())
            config.port = *it;
        if (auto it = root.find("path"); it != root.end())
            config.path = *it;
        if (auto it = root.find("use_ssl"); it != root.end())
            config.use_ssl = *it;
        if (auto it = root.find("verify_ssl"); it != root.end())
            config.verify_ssl = *it;
        if (auto it = root.find("ca_cert_file"); it != root.end())
            config.ca_cert_file = *it;
        if (auto it = root.find("api_key"); it != root.end())
            config.api_key = *it;
        if (auto it = root.find("username"); it != root.end())
            config.username = *it;
        if (auto it = root.find("password"); it != root.end())
            config.password = *it;
        if (auto it = root.find("reconnect"); it != root.end())
            config.reconnect = *it;
        if (auto it = root.find("max_reconnect_attempts"); it != root.end())
            config.max_reconnect_attempts = *it;
        if (auto it = root.find("reconnect_base_delay_ms"); it != root.end())
            config.reconnect_base_delay_ms = *it;
        if (auto it = root.find("store_events"); it != root.end())
            config.store_events = *it;
        if (auto it = root.find("event_store_path"); it != root.end())
            config.event_store_path = *it;
        if (auto it = root.find("last_event_id"); it != root.end())
            config.last_event_id = *it;

        if (auto it = root.find("event_types_filter");
            it != root.end() && it->is_array()) {
            config.event_types_filter.clear();
            config.event_types_filter.reserve(it->size());
            for (const auto& type : *it) {
                config.event_types_filter.emplace_back(type);
            }
        }
    } catch (const std::exception& e) {
        spdlog::error("Error loading config file {}: {}", filename, e.what());
    }

    return config;
}

void ClientConfig::save_to_file(const std::string& filename) const {
    try {
        json j = {{"host", host},
                  {"port", port},
                  {"path", path},
                  {"use_ssl", use_ssl},
                  {"verify_ssl", verify_ssl},
                  {"ca_cert_file", ca_cert_file},
                  {"api_key", api_key},
                  {"username", username},
                  {"password", password},
                  {"reconnect", reconnect},
                  {"max_reconnect_attempts", max_reconnect_attempts},
                  {"reconnect_base_delay_ms", reconnect_base_delay_ms},
                  {"store_events", store_events},
                  {"event_store_path", event_store_path},
                  {"last_event_id", last_event_id},
                  {"event_types_filter", event_types_filter}};

        std::ofstream file(filename);
        if (!file.is_open()) {
            spdlog::error("Failed to open config file {} for writing",
                          filename);
            return;
        }

        file << j.dump(4);
    } catch (const std::exception& e) {
        spdlog::error("Error saving config file {}: {}", filename, e.what());
    }
}

}  // namespace sse