#include "server_config.hpp"
#include <spdlog/spdlog.h>
#include <fstream>
#include <nlohmann/json.hpp>


using json = nlohmann::json;

namespace atom::extra::asio::sse {

ServerConfig ServerConfig::from_file(const std::string& filename) {
    ServerConfig config;
    try {
        std::ifstream file(filename);
        if (file.is_open()) {
            json j;
            file >> j;

            auto apply = [&j](auto& field, const char* name) {
                if (j.contains(name)) {
                    field = j[name];
                }
            };

            apply(config.port, "port");
            apply(config.address, "address");
            apply(config.enable_ssl, "enable_ssl");
            apply(config.cert_file, "cert_file");
            apply(config.key_file, "key_file");
            apply(config.auth_file, "auth_file");
            apply(config.require_auth, "require_auth");
            apply(config.max_event_history, "max_event_history");
            apply(config.persist_events, "persist_events");
            apply(config.event_store_path, "event_store_path");
            apply(config.heartbeat_interval_seconds,
                  "heartbeat_interval_seconds");
            apply(config.max_connections, "max_connections");
            apply(config.enable_compression, "enable_compression");
            apply(config.connection_timeout_seconds,
                  "connection_timeout_seconds");
        }
    } catch (const std::exception& e) {
        spdlog::error("Error loading config file {}: {}", filename, e.what());
    }
    return config;
}

void ServerConfig::save_to_file(const std::string& filename) const {
    try {
        json j = {{"port", port},
                  {"address", address},
                  {"enable_ssl", enable_ssl},
                  {"cert_file", cert_file},
                  {"key_file", key_file},
                  {"auth_file", auth_file},
                  {"require_auth", require_auth},
                  {"max_event_history", max_event_history},
                  {"persist_events", persist_events},
                  {"event_store_path", event_store_path},
                  {"heartbeat_interval_seconds", heartbeat_interval_seconds},
                  {"max_connections", max_connections},
                  {"enable_compression", enable_compression},
                  {"connection_timeout_seconds", connection_timeout_seconds}};

        std::ofstream file(filename);
        file << j.dump(4);
    } catch (const std::exception& e) {
        spdlog::error("Error saving config file {}: {}", filename, e.what());
    }
}

}  // namespace atom::extra::asio::sse
