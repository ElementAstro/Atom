#include "cron_storage.hpp"

#include <fstream>
#include "atom/type/json.hpp"
#include "spdlog/spdlog.h"

using json = nlohmann::json;

auto CronStorage::exportToJSON(const std::vector<CronJob>& jobs,
                              const std::string& filename) -> bool {
    spdlog::info("Exporting Cron jobs to JSON file: {}", filename);

    json jsonObj = json::array();

    for (const auto& job : jobs) {
        jsonObj.push_back(job.toJson());
    }

    std::ofstream file(filename);
    if (file.is_open()) {
        file << jsonObj.dump(4);
        spdlog::info("Exported Cron jobs to {} successfully", filename);
        return true;
    }

    spdlog::error("Failed to open file: {}", filename);
    return false;
}

auto CronStorage::importFromJSON(const std::string& filename) -> std::vector<CronJob> {
    spdlog::info("Importing Cron jobs from JSON file: {}", filename);

    std::ifstream file(filename);
    if (!file.is_open()) {
        spdlog::error("Failed to open file: {}", filename);
        return {};
    }

    try {
        json jsonObj;
        file >> jsonObj;

        std::vector<CronJob> jobs;
        jobs.reserve(jsonObj.size());

        for (const auto& jobJson : jsonObj) {
            try {
                CronJob job = CronJob::fromJson(jobJson);
                jobs.push_back(std::move(job));
            } catch (const std::exception& e) {
                spdlog::error("Error parsing job from JSON: {}", e.what());
            }
        }

        spdlog::info("Successfully imported {} jobs from {}", jobs.size(), filename);
        return jobs;
    } catch (const std::exception& e) {
        spdlog::error("Error parsing JSON file: {}", e.what());
        return {};
    }
}
