#include "cron_storage.hpp"

#include <fstream>
#include <filesystem>
#include <algorithm>
#include <iomanip>
#include <sstream>
#include <cstring>
#include "atom/type/json.hpp"
#include "spdlog/spdlog.h"

using json = nlohmann::json;
namespace fs = std::filesystem;

// Static member definitions
StorageConfig CronStorage::config_;
StorageStats CronStorage::stats_;

void CronStorage::setConfig(const StorageConfig& config) {
    config_ = config;
    spdlog::info("CronStorage configuration updated");
}

auto CronStorage::getConfig() -> const StorageConfig& {
    return config_;
}

auto CronStorage::getStats() -> const StorageStats& {
    return stats_;
}

void CronStorage::resetStats() {
    stats_ = StorageStats{};
    spdlog::info("CronStorage statistics reset");
}

void CronStorage::updateStats(bool is_save, bool success,
                             std::chrono::milliseconds duration, size_t bytes) {
    if (is_save) {
        stats_.total_saves++;
        if (!success) stats_.failed_saves++;
        stats_.total_bytes_written += bytes;
        // Update average save time
        auto total_saves = stats_.total_saves;
        stats_.avg_save_time = std::chrono::milliseconds(
            (stats_.avg_save_time.count() * (total_saves - 1) + duration.count()) / total_saves
        );
    } else {
        stats_.total_loads++;
        if (!success) stats_.failed_loads++;
        stats_.total_bytes_read += bytes;
        // Update average load time
        auto total_loads = stats_.total_loads;
        stats_.avg_load_time = std::chrono::milliseconds(
            (stats_.avg_load_time.count() * (total_loads - 1) + duration.count()) / total_loads
        );
    }
}

auto CronStorage::saveJobs(const std::vector<CronJob>& jobs,
                          const std::string& filename) -> bool {
    auto start_time = std::chrono::steady_clock::now();
    bool success = false;
    size_t bytes_written = 0;

    try {
        switch (config_.format) {
            case StorageFormat::JSON:
                success = exportToJSON(jobs, filename + ".json");
                break;
            case StorageFormat::COMPRESSED_JSON: {
                // First save to JSON, then compress
                std::string temp_file = filename + ".tmp.json";
                if (exportToJSON(jobs, temp_file)) {
                    // TODO: Implement compression
                    success = true; // For now, just rename
                    fs::rename(temp_file, filename + ".json.gz");
                }
                break;
            }
            case StorageFormat::BINARY: {
                auto binary_data = serializeToBinary(jobs);
                std::ofstream file(filename + ".bin", std::ios::binary);
                if (file.is_open()) {
                    file.write(reinterpret_cast<const char*>(binary_data.data()), binary_data.size());
                    bytes_written = binary_data.size();
                    success = true;
                }
                break;
            }
            default:
                success = exportToJSON(jobs, filename + ".json");
                break;
        }

        if (success && config_.enable_backup) {
            createBackup(jobs, fs::path(filename).parent_path().string() + "/backups");
        }

    } catch (const std::exception& e) {
        spdlog::error("Error saving jobs: {}", e.what());
        success = false;
    }

    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start_time);
    updateStats(true, success, duration, bytes_written);

    return success;
}

auto CronStorage::loadJobs(const std::string& filename) -> std::vector<CronJob> {
    auto start_time = std::chrono::steady_clock::now();
    std::vector<CronJob> jobs;
    bool success = false;
    size_t bytes_read = 0;

    try {
        auto format = detectStorageFormat(filename);

        switch (format) {
            case StorageFormat::JSON:
                jobs = importFromJSON(filename);
                success = !jobs.empty();
                break;
            case StorageFormat::BINARY: {
                std::ifstream file(filename, std::ios::binary);
                if (file.is_open()) {
                    file.seekg(0, std::ios::end);
                    size_t file_size = file.tellg();
                    file.seekg(0, std::ios::beg);

                    std::vector<uint8_t> binary_data(file_size);
                    file.read(reinterpret_cast<char*>(binary_data.data()), file_size);
                    bytes_read = file_size;

                    jobs = deserializeFromBinary(binary_data);
                    success = !jobs.empty();
                }
                break;
            }
            default:
                jobs = importFromJSON(filename);
                success = !jobs.empty();
                break;
        }

    } catch (const std::exception& e) {
        spdlog::error("Error loading jobs: {}", e.what());
        success = false;
    }

    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start_time);
    updateStats(false, success, duration, bytes_read);

    return jobs;
}

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

auto CronStorage::createBackup(const std::vector<CronJob>& jobs,
                              const std::string& backup_dir) -> std::optional<BackupInfo> {
    try {
        // Create backup directory if it doesn't exist
        fs::create_directories(backup_dir);

        std::string backup_filename = getBackupFilename(backup_dir);
        std::string full_path = backup_dir + "/" + backup_filename;

        // Save jobs to backup file
        if (!exportToJSON(jobs, full_path)) {
            return std::nullopt;
        }

        // Get file info
        auto file_size = fs::file_size(full_path);
        auto timestamp = std::chrono::system_clock::now();
        std::string checksum = generateChecksum(full_path);

        BackupInfo info(backup_filename, timestamp, file_size, jobs.size(), checksum);

        spdlog::info("Created backup: {} ({} jobs, {} bytes)",
                    backup_filename, jobs.size(), file_size);

        // Cleanup old backups if needed
        cleanupOldBackups(backup_dir);

        return info;

    } catch (const std::exception& e) {
        spdlog::error("Failed to create backup: {}", e.what());
        return std::nullopt;
    }
}

auto CronStorage::restoreFromBackup(const BackupInfo& backup_info) -> std::vector<CronJob> {
    try {
        auto jobs = importFromJSON(backup_info.filename);

        // Verify backup integrity
        std::string current_checksum = generateChecksum(backup_info.filename);
        if (current_checksum != backup_info.checksum) {
            spdlog::warn("Backup checksum mismatch for {}", backup_info.filename);
        }

        spdlog::info("Restored {} jobs from backup: {}",
                    jobs.size(), backup_info.filename);
        return jobs;

    } catch (const std::exception& e) {
        spdlog::error("Failed to restore from backup {}: {}",
                     backup_info.filename, e.what());
        return {};
    }
}

auto CronStorage::listBackups(const std::string& backup_dir) -> std::vector<BackupInfo> {
    std::vector<BackupInfo> backups;

    try {
        if (!fs::exists(backup_dir)) {
            return backups;
        }

        for (const auto& entry : fs::directory_iterator(backup_dir)) {
            if (entry.is_regular_file() && entry.path().extension() == ".json") {
                auto filename = entry.path().filename().string();
                auto file_size = entry.file_size();
                auto timestamp = fs::last_write_time(entry);

                // Convert file_time to system_clock time_point
                auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
                    timestamp - fs::file_time_type::clock::now() + std::chrono::system_clock::now());

                std::string checksum = generateChecksum(entry.path().string());

                // Count jobs in backup (simplified - just estimate)
                size_t job_count = 0; // TODO: Parse file to get actual count

                backups.emplace_back(filename, sctp, file_size, job_count, checksum);
            }
        }

        // Sort by timestamp (newest first)
        std::sort(backups.begin(), backups.end(),
                 [](const BackupInfo& a, const BackupInfo& b) {
                     return a.timestamp > b.timestamp;
                 });

    } catch (const std::exception& e) {
        spdlog::error("Failed to list backups: {}", e.what());
    }

    return backups;
}

auto CronStorage::cleanupOldBackups(const std::string& backup_dir) -> size_t {
    try {
        auto backups = listBackups(backup_dir);
        size_t removed_count = 0;

        if (backups.size() > config_.max_backup_files) {
            // Remove oldest backups
            for (size_t i = config_.max_backup_files; i < backups.size(); ++i) {
                std::string full_path = backup_dir + "/" + backups[i].filename;
                if (fs::remove(full_path)) {
                    removed_count++;
                    spdlog::info("Removed old backup: {}", backups[i].filename);
                }
            }
        }

        return removed_count;

    } catch (const std::exception& e) {
        spdlog::error("Failed to cleanup old backups: {}", e.what());
        return 0;
    }
}

auto CronStorage::detectStorageFormat(const std::string& filename) -> StorageFormat {
    fs::path file_path(filename);
    std::string extension = file_path.extension().string();

    if (extension == ".json") {
        return StorageFormat::JSON;
    } else if (extension == ".bin") {
        return StorageFormat::BINARY;
    } else if (extension == ".gz") {
        return StorageFormat::COMPRESSED_JSON;
    }

    // Default to JSON
    return StorageFormat::JSON;
}

auto CronStorage::generateChecksum(const std::string& filename) -> std::string {
    try {
        std::ifstream file(filename, std::ios::binary);
        if (!file.is_open()) {
            return "";
        }

        // Simple checksum implementation (in production, use proper hash like SHA-256)
        std::hash<std::string> hasher;
        std::string content((std::istreambuf_iterator<char>(file)),
                           std::istreambuf_iterator<char>());

        auto hash_value = hasher(content);

        std::stringstream ss;
        ss << std::hex << hash_value;
        return ss.str();

    } catch (const std::exception& e) {
        spdlog::error("Failed to generate checksum for {}: {}", filename, e.what());
        return "";
    }
}

auto CronStorage::getBackupFilename([[maybe_unused]] const std::string& backup_dir) -> std::string {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);

    std::stringstream ss;
    ss << "cron_backup_" << std::put_time(std::localtime(&time_t), "%Y%m%d_%H%M%S") << ".json";
    return ss.str();
}

auto CronStorage::serializeToBinary(const std::vector<CronJob>& jobs) -> std::vector<uint8_t> {
    // Simplified binary serialization
    // In production, use a proper serialization library like protobuf or msgpack
    std::vector<uint8_t> data;

    try {
        // Convert to JSON first, then to binary
        json jsonObj = json::array();
        for (const auto& job : jobs) {
            jsonObj.push_back(job.toJson());
        }

        std::string json_str = jsonObj.dump();
        data.resize(json_str.size());
        std::memcpy(data.data(), json_str.c_str(), json_str.size());

    } catch (const std::exception& e) {
        spdlog::error("Failed to serialize to binary: {}", e.what());
        data.clear();
    }

    return data;
}

auto CronStorage::deserializeFromBinary(const std::vector<uint8_t>& data) -> std::vector<CronJob> {
    std::vector<CronJob> jobs;

    try {
        // Convert binary back to JSON string
        std::string json_str(reinterpret_cast<const char*>(data.data()), data.size());
        json jsonObj = json::parse(json_str);

        jobs.reserve(jsonObj.size());
        for (const auto& jobJson : jsonObj) {
            try {
                CronJob job = CronJob::fromJson(jobJson);
                jobs.push_back(std::move(job));
            } catch (const std::exception& e) {
                spdlog::error("Error parsing job from binary data: {}", e.what());
            }
        }

    } catch (const std::exception& e) {
        spdlog::error("Failed to deserialize from binary: {}", e.what());
        jobs.clear();
    }

    return jobs;
}

// Placeholder implementations for database and other advanced features
auto CronStorage::initializeDatabase([[maybe_unused]] const std::string& connection_string) -> bool {
    // TODO: Implement database initialization
    spdlog::warn("Database storage not yet implemented");
    return false;
}

auto CronStorage::saveJobsToDatabase([[maybe_unused]] const std::vector<CronJob>& jobs) -> bool {
    // TODO: Implement database save
    spdlog::warn("Database storage not yet implemented");
    return false;
}

auto CronStorage::loadJobsFromDatabase() -> std::vector<CronJob> {
    // TODO: Implement database load
    spdlog::warn("Database storage not yet implemented");
    return {};
}

auto CronStorage::saveJobsIncremental([[maybe_unused]] const std::vector<CronJob>& jobs,
                                     [[maybe_unused]] const std::string& filename,
                                     [[maybe_unused]] std::chrono::system_clock::time_point last_save_time) -> bool {
    // TODO: Implement incremental save logic
    spdlog::warn("Incremental save not yet implemented");
    return false;
}

auto CronStorage::validateFileIntegrity(const std::string& filename) -> bool {
    try {
        // Basic validation - check if file exists and is readable
        std::ifstream file(filename);
        if (!file.is_open()) {
            return false;
        }

        // For JSON files, try to parse
        if (filename.ends_with(".json")) {
            json jsonObj;
            file >> jsonObj;
            return jsonObj.is_array();
        }

        return true;

    } catch (const std::exception& e) {
        spdlog::error("File integrity validation failed for {}: {}", filename, e.what());
        return false;
    }
}

auto CronStorage::compressData([[maybe_unused]] const std::vector<uint8_t>& data) -> std::vector<uint8_t> {
    // TODO: Implement compression (e.g., using zlib)
    spdlog::warn("Data compression not yet implemented");
    return {};
}

auto CronStorage::decompressData([[maybe_unused]] const std::vector<uint8_t>& compressed_data) -> std::vector<uint8_t> {
    // TODO: Implement decompression
    spdlog::warn("Data decompression not yet implemented");
    return {};
}
