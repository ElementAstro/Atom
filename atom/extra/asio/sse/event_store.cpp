#include "event_store.hpp"

#include <spdlog/spdlog.h>
#include <filesystem>
#include <format>
#include <fstream>

#include "atom/type/json.hpp"

using json = nlohmann::json;

namespace atom::extra::asio::sse {

EventStore::EventStore(const std::string& store_path)
    : store_path_(store_path) {
    std::filesystem::create_directories(store_path_);
    load_existing_events();
}

void EventStore::store_event(const Event& event) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (event_ids_.contains(event.id())) {
        return;
    }

    event_ids_.insert(event.id());

    if (event.timestamp() > cached_latest_timestamp_) {
        cached_latest_timestamp_ = event.timestamp();
        cached_latest_id_ = event.id();
    }

    try {
        const json j = {{"id", event.id()},
                        {"event_type", event.event_type()},
                        {"data", event.data()},
                        {"timestamp", event.timestamp()}};

        const auto filename =
            std::format("{}/event_{}_{}_{}.json", store_path_,
                        event.timestamp(), event.event_type(), event.id());

        std::ofstream file(filename);
        if (!file.is_open()) {
            spdlog::error("Failed to open file for writing: {}", filename);
            return;
        }

        file << j.dump(2);
        if (!file.good()) {
            spdlog::error("Error writing to file: {}", filename);
        }
    } catch (const std::exception& e) {
        spdlog::error("Error storing event: {}", e.what());
    }
}

bool EventStore::has_seen_event(const std::string& event_id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return event_ids_.contains(event_id);
}

std::string EventStore::get_latest_event_id() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return cached_latest_id_;
}

void EventStore::load_existing_events() {
    try {
        if (!std::filesystem::exists(store_path_)) {
            return;
        }

        for (const auto& entry :
             std::filesystem::directory_iterator(store_path_)) {
            if (!entry.is_regular_file() ||
                entry.path().extension() != ".json") {
                continue;
            }

            try {
                std::ifstream file(entry.path());
                if (!file.is_open()) {
                    spdlog::warn("Failed to open file: {}",
                                 entry.path().string());
                    continue;
                }

                json j;
                file >> j;

                if (j.contains("id")) {
                    const std::string event_id = j["id"];
                    event_ids_.insert(event_id);

                    if (j.contains("timestamp")) {
                        const uint64_t timestamp = j["timestamp"];
                        if (timestamp > cached_latest_timestamp_) {
                            cached_latest_timestamp_ = timestamp;
                            cached_latest_id_ = event_id;
                        }
                    }
                }
            } catch (const std::exception& e) {
                spdlog::warn("Error parsing event file {}: {}",
                             entry.path().string(), e.what());
            }
        }

        spdlog::info("Loaded {} existing events from {}", event_ids_.size(),
                     store_path_);
    } catch (const std::exception& e) {
        spdlog::error("Error scanning event directory: {}", e.what());
    }
}

}  // namespace atom::extra::asio::sse
