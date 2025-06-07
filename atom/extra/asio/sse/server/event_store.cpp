#include "event_store.hpp"
#include <spdlog/spdlog.h>
#include <algorithm>
#include <filesystem>
#include <format>
#include <fstream>
#include "atom/type/json.hpp"

using json = nlohmann::json;

namespace atom::extra::asio::sse {

EventStore::EventStore(const std::string& store_path, size_t max_events)
    : store_path_(store_path), max_events_(max_events) {
    std::filesystem::create_directories(store_path_);
    load_events();
}

void EventStore::store_event(const Event& event) {
    std::unique_lock lock(mutex_);

    events_.push_back(event);

    if (events_.size() > max_events_) {
        events_.pop_front();
    }

    persist_event(event);
}

std::vector<Event> EventStore::get_events(size_t limit,
                                          const std::string& event_type) const {
    std::shared_lock lock(mutex_);

    std::vector<Event> result;
    size_t count = 0;

    for (auto it = events_.rbegin(); it != events_.rend() && count < limit;
         ++it) {
        if (event_type.empty() || it->event_type() == event_type) {
            result.push_back(*it);
            ++count;
        }
    }

    return result;
}

std::vector<Event> EventStore::get_events_since(
    uint64_t timestamp, const std::string& event_type) const {
    std::shared_lock lock(mutex_);

    std::vector<Event> result;

    for (const auto& event : events_) {
        if (event.timestamp() > timestamp &&
            (event_type.empty() || event.event_type() == event_type)) {
            result.push_back(event);
        }
    }

    return result;
}

void EventStore::clear() {
    std::unique_lock lock(mutex_);
    events_.clear();

    try {
        for (const auto& entry :
             std::filesystem::directory_iterator(store_path_)) {
            std::filesystem::remove(entry.path());
        }
    } catch (const std::exception& e) {
        SPDLOG_ERROR("Error clearing event store: {}", e.what());
    }
}

void EventStore::load_events() {
    try {
        std::vector<std::filesystem::path> event_files;
        for (const auto& entry :
             std::filesystem::directory_iterator(store_path_)) {
            if (entry.is_regular_file() &&
                entry.path().extension() == ".json") {
                event_files.push_back(entry.path());
            }
        }

        std::sort(event_files.begin(), event_files.end());

        size_t count = 0;
        for (auto it = event_files.rbegin();
             it != event_files.rend() && count < max_events_; ++it, ++count) {
            try {
                std::ifstream file(*it);
                json j;
                file >> j;

                std::string id = j["id"];
                std::string event_type = j["event_type"];
                std::string data = j["data"];

                Event event(id, event_type, data);

                if (j.contains("metadata") && j["metadata"].is_object()) {
                    for (auto meta_it = j["metadata"].begin();
                         meta_it != j["metadata"].end(); ++meta_it) {
                        event.add_metadata(meta_it.key(), meta_it.value());
                    }
                }

                events_.push_front(event);
            } catch (const std::exception& e) {
                SPDLOG_ERROR("Error loading event from {}: {}", it->string(),
                             e.what());
            }
        }

        SPDLOG_INFO("Loaded {} events from storage", events_.size());

    } catch (const std::exception& e) {
        SPDLOG_ERROR("Error loading events: {}", e.what());
    }
}

void EventStore::persist_event(const Event& event) {
    try {
        json j = {{"id", event.id()},
                  {"event_type", event.event_type()},
                  {"data", event.data()},
                  {"timestamp", event.timestamp()}};

        j["metadata"] = json::object();
        // Note: In a real implementation, we'd need a method to get all
        // metadata

        std::string filename =
            std::format("{}/event_{}_{}_{}.json", store_path_,
                        event.timestamp(), event.event_type(), event.id());

        std::ofstream file(filename);
        file << j.dump();

    } catch (const std::exception& e) {
        SPDLOG_ERROR("Error persisting event: {}", e.what());
    }
}

}  // namespace atom::extra::asio::sse