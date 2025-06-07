#include "archiver.h"
#include "atom/io/compress.hpp"

#include <algorithm>
#include <regex>

namespace modern_log {

LogArchiver::LogArchiver(std::filesystem::path log_dir, ArchiveConfig config)
    : log_dir_(std::move(log_dir)), config_(std::move(config)) {
    std::filesystem::create_directories(log_dir_);
}

void LogArchiver::archive_old_files() {
    try {
        auto files = get_archivable_files();

        std::ranges::sort(files, [](const auto& a, const auto& b) {
            return std::filesystem::last_write_time(a) >
                   std::filesystem::last_write_time(b);
        });

        if (files.size() > config_.max_files) {
            for (size_t i = config_.max_files; i < files.size(); ++i) {
                std::filesystem::remove(files[i]);
            }
            files.resize(config_.max_files);
        }

        if (config_.compress) {
            for (const auto& file : files) {
                if (is_file_old(file)) {
                    compress_file(file);
                }
            }
        }

        cleanup_excess_files();
    } catch (...) {
    }
}

bool LogArchiver::compress_file(const std::filesystem::path& file) {
    try {
        atom::io::CompressionOptions options;
        auto result = atom::io::compressFile(
            file.string(), file.parent_path().string(), options);
        if (result.success) {
            std::filesystem::remove(file);
            return true;
        }
        return false;
    } catch (...) {
        return false;
    }
}

bool LogArchiver::decompress_file(const std::filesystem::path& file) {
    try {
        atom::io::DecompressionOptions options;
        auto result = atom::io::decompressFile(
            file.string(), file.parent_path().string(), options);
        return result.success;
    } catch (...) {
        return false;
    }
}

size_t LogArchiver::get_directory_size() const {
    size_t total_size = 0;
    try {
        for (const auto& entry :
             std::filesystem::recursive_directory_iterator(log_dir_)) {
            if (entry.is_regular_file()) {
                total_size += entry.file_size();
            }
        }
    } catch (...) {
    }
    return total_size;
}

void LogArchiver::cleanup_excess_files() {
    size_t current_size = get_directory_size();
    if (current_size <= config_.max_total_size) {
        return;
    }
    auto files = get_archivable_files();
    std::ranges::sort(files, [](const auto& a, const auto& b) {
        return std::filesystem::last_write_time(a) <
               std::filesystem::last_write_time(b);
    });
    for (const auto& file : files) {
        if (current_size <= config_.max_total_size) {
            break;
        }
        try {
            size_t file_size = std::filesystem::file_size(file);
            std::filesystem::remove(file);
            current_size -= file_size;
        } catch (...) {
        }
    }
}

std::vector<std::filesystem::path> LogArchiver::get_archivable_files() const {
    std::vector<std::filesystem::path> files;
    try {
        for (const auto& entry :
             std::filesystem::directory_iterator(log_dir_)) {
            if (entry.is_regular_file()) {
                auto path = entry.path();
                auto extension = path.extension().string();
                if (extension == ".log" || extension == ".txt" ||
                    extension == ".gz") {
                    files.push_back(entry.path());
                }
            }
        }
    } catch (...) {
    }
    return files;
}

void LogArchiver::set_config(const ArchiveConfig& config) { config_ = config; }

LogArchiver::ArchiveStats LogArchiver::get_stats() const {
    ArchiveStats stats{};
    try {
        auto files = get_archivable_files();
        stats.total_files = files.size();
        for (const auto& file : files) {
            stats.total_size += std::filesystem::file_size(file);
            if (is_file_old(file)) {
                stats.archived_files++;
            }
            if (file.extension() == ".gz") {
                stats.compressed_files++;
            }
        }
        stats.last_archive_time = std::chrono::system_clock::now();
    } catch (...) {
    }
    return stats;
}

bool LogArchiver::is_file_old(const std::filesystem::path& file) const {
    try {
        auto ftime = std::filesystem::last_write_time(file);
        auto sctp =
            std::chrono::time_point_cast<std::chrono::system_clock::duration>(
                ftime - std::filesystem::file_time_type::clock::now() +
                std::chrono::system_clock::now());
        auto now = std::chrono::system_clock::now();
        return now - sctp > config_.max_age;
    } catch (...) {
        return false;
    }
}

std::string LogArchiver::generate_archive_name(
    const std::filesystem::path& original) const {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto tm = *std::localtime(&time_t);
    std::string pattern = config_.archive_pattern;
    std::regex name_regex(R"(\{name\})");
    pattern = std::regex_replace(pattern, name_regex, original.stem().string());
    std::regex date_regex(R"(\{date\})");
    char date_str[32];
    std::strftime(date_str, sizeof(date_str), "%Y%m%d", &tm);
    pattern = std::regex_replace(pattern, date_regex, date_str);
    return pattern;
}

}  // namespace modern_log