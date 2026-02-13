#include "cron_job.hpp"

#include <chrono>
#include <iomanip>
#include <sstream>
#include <algorithm>

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
    {
        std::lock_guard<std::mutex> lock(history_mutex_);
        for (const auto& entry : execution_history_) {
            json entryJson = {
                {"timestamp", timePointToString(entry.timestamp)},
                {"success", entry.success}
            };
            if (!entry.error_message.empty()) {
                entryJson["error_message"] = entry.error_message;
            }
            historyJson.push_back(std::move(entryJson));
        }
    }

    auto last_run_time = last_run_.load();
    return json{
        {"time", time_},
        {"command", command_},
        {"enabled", isEnabled()},
        {"status", static_cast<int>(status_)},
        {"category", category_},
        {"description", description_},
        {"created_at", timePointToString(created_at_)},
        {"last_run", last_run_time != std::chrono::system_clock::time_point()
                         ? timePointToString(last_run_time)
                         : ""},
        {"run_count", run_count_.load()},
        {"priority", static_cast<int>(priority_)},
        {"max_retries", max_retries_},
        {"current_retries", current_retries_},
        {"one_time", one_time_},
        {"execution_history", std::move(historyJson)}};
}

auto CronJob::fromJson(const json& jsonObj) -> CronJob {
    CronJob job;
    job.time_ = jsonObj.at("time").get<std::string>();
    job.command_ = jsonObj.at("command").get<std::string>();

    // Handle both old 'enabled' field and new 'status' field
    if (jsonObj.contains("status")) {
        job.status_ = static_cast<JobStatus>(jsonObj["status"].get<int>());
    } else {
        bool enabled = jsonObj.value("enabled", true);
        job.status_ = enabled ? JobStatus::ENABLED : JobStatus::DISABLED;
    }

    job.category_ = jsonObj.value("category", "default");
    job.description_ = jsonObj.value("description", "");

    const auto createdAtStr = jsonObj.value("created_at", "");
    job.created_at_ = createdAtStr.empty() ? std::chrono::system_clock::now()
                                           : stringToTimePoint(createdAtStr);

    const auto lastRunStr = jsonObj.value("last_run", "");
    if (!lastRunStr.empty()) {
        job.last_run_.store(stringToTimePoint(lastRunStr));
    }

    job.run_count_.store(jsonObj.value("run_count", 0));

    // Handle priority conversion
    int priority_val = jsonObj.value("priority", 5);
    job.priority_ = static_cast<JobPriority>(std::clamp(priority_val, 1, 10));

    job.max_retries_ = jsonObj.value("max_retries", 0);
    job.current_retries_ = jsonObj.value("current_retries", 0);
    job.one_time_ = jsonObj.value("one_time", false);

    if (jsonObj.contains("execution_history") &&
        jsonObj["execution_history"].is_array()) {
        const auto& history = jsonObj["execution_history"];
        // deque doesn't have reserve, but we can pre-size if needed
        for (const auto& entry : history) {
            if (entry.contains("timestamp") && entry.contains("success")) {
                auto timestamp =
                    stringToTimePoint(entry["timestamp"].get<std::string>());
                bool success = entry["success"].get<bool>();
                std::string error_msg = entry.value("error_message", "");
                job.execution_history_.emplace_back(timestamp, success, std::move(error_msg));
            }
        }
    }

    return job;
}

void CronJob::recordExecution(bool success, const std::string& error_message) {
    auto now = std::chrono::system_clock::now();
    last_run_.store(now);
    run_count_.fetch_add(1);

    {
        std::lock_guard<std::mutex> lock(history_mutex_);
        execution_history_.emplace_back(now, success, error_message);

        // Maintain maximum history size using efficient deque operations
        if (execution_history_.size() > MAX_HISTORY_SIZE) {
            execution_history_.pop_front();
        }
    }

    // Reset retries on success, increment on failure
    if (success) {
        resetRetries();
    } else {
        incrementRetries();
    }
}

auto CronJob::getExecutionHistory() const -> std::vector<ExecutionEntry> {
    std::lock_guard<std::mutex> lock(history_mutex_);
    return std::vector<ExecutionEntry>(execution_history_.begin(), execution_history_.end());
}

auto CronJob::getRecentStats(size_t count) const -> std::pair<size_t, size_t> {
    std::lock_guard<std::mutex> lock(history_mutex_);

    size_t total = std::min(count, execution_history_.size());
    if (total == 0) {
        return {0, 0};
    }

    auto start_it = execution_history_.end() - total;
    size_t success_count = std::count_if(start_it, execution_history_.end(),
                                        [](const ExecutionEntry& entry) {
                                            return entry.success;
                                        });

    return {success_count, total};
}

void CronJob::clearHistory() {
    std::lock_guard<std::mutex> lock(history_mutex_);
    execution_history_.clear();
}
