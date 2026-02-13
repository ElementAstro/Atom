#include "audit_log.hpp"

#include <algorithm>
#include <fstream>
#include <iomanip>
#include <mutex>
#include <sstream>

namespace atom::secret {

std::string AuditEntry::getActionName() const {
    return auditActionToString(action);
}

AuditLog::AuditLog(const std::filesystem::path& logPath, size_t maxEntries)
    : logPath_(logPath), maxEntries_(maxEntries), enabled_(true), nextId_(1) {
    loadFromFile();
}

AuditLog::~AuditLog() {
    try {
        saveToFile();
    } catch (...) {
        // Ignore errors during destruction
    }
}

Result<void> AuditLog::log(AuditAction action, const std::string& entryId,
                           const std::string& entryTitle,
                           const std::string& details) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!enabled_) {
        return Result<void>::success();
    }

    AuditEntry entry;
    entry.id = nextId_++;
    entry.timestamp = std::chrono::system_clock::now();
    entry.action = action;
    entry.entryId = entryId;
    entry.entryTitle = entryTitle;
    entry.details = details;

    entries_.push_back(std::move(entry));
    trimEntries();

    try {
        saveToFile();
    } catch (const std::exception& e) {
        return Result<void>::error(
            ErrorCode::StorageWriteFailed,
            std::string("Failed to save audit log: ") + e.what());
    }

    return Result<void>::success();
}

Result<std::vector<AuditEntry>> AuditLog::query(
    const AuditQueryOptions& options) {
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<AuditEntry> results;

    for (const auto& entry : entries_) {
        // Filter by time range
        if (options.startTime != std::chrono::system_clock::time_point{} &&
            entry.timestamp < options.startTime) {
            continue;
        }
        if (options.endTime != std::chrono::system_clock::time_point{} &&
            entry.timestamp > options.endTime) {
            continue;
        }

        // Filter by actions
        if (!options.actions.empty()) {
            bool found = false;
            for (const auto& action : options.actions) {
                if (entry.action == action) {
                    found = true;
                    break;
                }
            }
            if (!found)
                continue;
        }

        // Filter by entry ID
        if (!options.entryId.empty() && entry.entryId != options.entryId) {
            continue;
        }

        results.push_back(entry);
    }

    // Sort by timestamp descending
    std::sort(results.begin(), results.end(),
              [](const AuditEntry& a, const AuditEntry& b) {
                  return a.timestamp > b.timestamp;
              });

    // Apply offset and limit
    if (options.offset > 0 &&
        static_cast<size_t>(options.offset) < results.size()) {
        results.erase(results.begin(), results.begin() + options.offset);
    } else if (options.offset > 0) {
        results.clear();
    }

    if (options.limit > 0 &&
        results.size() > static_cast<size_t>(options.limit)) {
        results.resize(options.limit);
    }

    return Result<std::vector<AuditEntry>>::success(std::move(results));
}

Result<std::vector<AuditEntry>> AuditLog::getRecent(int count) {
    AuditQueryOptions options;
    options.limit = count;
    return query(options);
}

Result<std::vector<AuditEntry>> AuditLog::getForEntry(
    const std::string& entryId, int limit) {
    AuditQueryOptions options;
    options.entryId = entryId;
    options.limit = limit;
    return query(options);
}

Result<int> AuditLog::clearOldEntries(
    std::chrono::system_clock::time_point olderThan) {
    std::lock_guard<std::mutex> lock(mutex_);

    size_t originalSize = entries_.size();

    entries_.erase(std::remove_if(entries_.begin(), entries_.end(),
                                  [&olderThan](const AuditEntry& entry) {
                                      return entry.timestamp < olderThan;
                                  }),
                   entries_.end());

    int deleted = static_cast<int>(originalSize - entries_.size());

    try {
        saveToFile();
    } catch (const std::exception& e) {
        return Result<int>::error(
            ErrorCode::StorageWriteFailed,
            std::string("Failed to save after clearing: ") + e.what());
    }

    return Result<int>::success(deleted);
}

Result<void> AuditLog::exportLog(const std::filesystem::path& outputPath,
                                 const AuditQueryOptions& options) {
    auto queryResult = query(options);
    if (queryResult.isError()) {
        return Result<void>::error(queryResult.errorCode(),
                                   queryResult.errorMessage());
    }

    try {
        std::ofstream outFile(outputPath);
        if (!outFile) {
            return Result<void>::error(ErrorCode::StorageWriteFailed,
                                       "Failed to create export file");
        }

        // Write CSV header
        outFile << "ID,Timestamp,Action,EntryID,EntryTitle,Details\n";

        for (const auto& entry : queryResult.value()) {
            auto time = std::chrono::system_clock::to_time_t(entry.timestamp);

            outFile << entry.id << ",";
            outFile << std::put_time(std::localtime(&time), "%Y-%m-%d %H:%M:%S")
                    << ",";
            outFile << "\"" << entry.getActionName() << "\",";
            outFile << "\"" << entry.entryId << "\",";
            outFile << "\"" << entry.entryTitle << "\",";
            outFile << "\"" << entry.details << "\"\n";
        }

        return Result<void>::success();

    } catch (const std::exception& e) {
        return Result<void>::error(ErrorCode::StorageWriteFailed,
                                   std::string("Export failed: ") + e.what());
    }
}

size_t AuditLog::getEntryCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return entries_.size();
}

void AuditLog::setEnabled(bool enabled) {
    std::lock_guard<std::mutex> lock(mutex_);
    enabled_ = enabled;
}

bool AuditLog::isEnabled() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return enabled_;
}

void AuditLog::loadFromFile() {
    if (!std::filesystem::exists(logPath_)) {
        return;
    }

    try {
        std::ifstream inFile(logPath_);
        if (!inFile)
            return;

        std::string line;
        while (std::getline(inFile, line)) {
            if (line.empty())
                continue;

            // Simple format: id|timestamp|action|entryId|entryTitle|details
            std::istringstream ss(line);
            std::string token;

            AuditEntry entry;

            if (!std::getline(ss, token, '|'))
                continue;
            entry.id = std::stoll(token);

            if (!std::getline(ss, token, '|'))
                continue;
            entry.timestamp = std::chrono::system_clock::time_point(
                std::chrono::seconds(std::stoll(token)));

            if (!std::getline(ss, token, '|'))
                continue;
            entry.action = static_cast<AuditAction>(std::stoi(token));

            if (!std::getline(ss, token, '|'))
                continue;
            entry.entryId = token;

            if (!std::getline(ss, token, '|'))
                continue;
            entry.entryTitle = token;

            std::getline(ss, entry.details);

            entries_.push_back(std::move(entry));

            if (entry.id >= nextId_) {
                nextId_ = entry.id + 1;
            }
        }
    } catch (...) {
        // Ignore load errors
    }
}

void AuditLog::saveToFile() {
    std::filesystem::create_directories(logPath_.parent_path());

    std::ofstream outFile(logPath_);
    if (!outFile)
        return;

    for (const auto& entry : entries_) {
        auto timestamp = std::chrono::duration_cast<std::chrono::seconds>(
                             entry.timestamp.time_since_epoch())
                             .count();

        outFile << entry.id << "|" << timestamp << "|"
                << static_cast<int>(entry.action) << "|" << entry.entryId << "|"
                << entry.entryTitle << "|" << entry.details << "\n";
    }
}

void AuditLog::trimEntries() {
    if (maxEntries_ > 0 && entries_.size() > maxEntries_) {
        // Remove oldest entries
        size_t toRemove = entries_.size() - maxEntries_;
        entries_.erase(entries_.begin(), entries_.begin() + toRemove);
    }
}

}  // namespace atom::secret
