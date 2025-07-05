#include "cron_job.hpp"

#include <chrono>
#include <iomanip>
#include <sstream>

#include "atom/type/json.hpp"

using json = nlohmann::json;

namespace {
auto timePointToString(const std::chrono::system_clock::time_point& timePoint)
    -> std::string {
    auto time = std::chrono::system_clock::to_time_t(timePoint);
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time), "%Y-%m-%d %H:%M:%S");
    return ss.str();
}

auto stringToTimePoint(const std::string& timeStr)
    -> std::chrono::system_clock::time_point {
    std::tm tm = {};
    std::stringstream ss(timeStr);
    ss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
    auto time = std::mktime(&tm);
    return std::chrono::system_clock::from_time_t(time);
}
}  // namespace

auto CronJob::getId() const -> std::string { return time_ + "_" + command_; }

auto CronJob::toJson() const -> json {
    json historyJson = json::array();
    for (const auto& entry : execution_history_) {
        historyJson.push_back({{"timestamp", timePointToString(entry.first)},
                               {"success", entry.second}});
    }

    return json{
        {"time", time_},
        {"command", command_},
        {"enabled", enabled_},
        {"category", category_},
        {"description", description_},
        {"created_at", timePointToString(created_at_)},
        {"last_run", last_run_ != std::chrono::system_clock::time_point()
                         ? timePointToString(last_run_)
                         : ""},
        {"run_count", run_count_},
        {"priority", priority_},
        {"max_retries", max_retries_},
        {"current_retries", current_retries_},
        {"one_time", one_time_},
        {"execution_history", std::move(historyJson)}};
}

auto CronJob::fromJson(const json& jsonObj) -> CronJob {
    CronJob job;
    job.time_ = jsonObj.at("time").get<std::string>();
    job.command_ = jsonObj.at("command").get<std::string>();
    job.enabled_ = jsonObj.at("enabled").get<bool>();
    job.category_ = jsonObj.value("category", "default");
    job.description_ = jsonObj.value("description", "");

    const auto createdAtStr = jsonObj.value("created_at", "");
    job.created_at_ = createdAtStr.empty() ? std::chrono::system_clock::now()
                                           : stringToTimePoint(createdAtStr);

    const auto lastRunStr = jsonObj.value("last_run", "");
    if (!lastRunStr.empty()) {
        job.last_run_ = stringToTimePoint(lastRunStr);
    }

    job.run_count_ = jsonObj.value("run_count", 0);
    job.priority_ = jsonObj.value("priority", 5);
    job.max_retries_ = jsonObj.value("max_retries", 0);
    job.current_retries_ = jsonObj.value("current_retries", 0);
    job.one_time_ = jsonObj.value("one_time", false);

    if (jsonObj.contains("execution_history") &&
        jsonObj["execution_history"].is_array()) {
        const auto& history = jsonObj["execution_history"];
        job.execution_history_.reserve(history.size());
        for (const auto& entry : history) {
            if (entry.contains("timestamp") && entry.contains("success")) {
                auto timestamp =
                    stringToTimePoint(entry["timestamp"].get<std::string>());
                bool success = entry["success"].get<bool>();
                job.execution_history_.emplace_back(timestamp, success);
            }
        }
    }

    return job;
}

void CronJob::recordExecution(bool success) {
    last_run_ = std::chrono::system_clock::now();
    ++run_count_;
    execution_history_.emplace_back(last_run_, success);

    constexpr size_t MAX_HISTORY = 100;
    if (execution_history_.size() > MAX_HISTORY) {
        execution_history_.erase(execution_history_.begin(),
                                 execution_history_.begin() +
                                     (execution_history_.size() - MAX_HISTORY));
    }
}
