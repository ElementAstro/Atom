/*
 * compress.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-3-31

Description: Compressor using ZLib and MiniZip-ng

**************************************************/

#include "compress.hpp"
#include "atom/log/loguru.hpp"
#include "atom/type/json.hpp"

#include <minizip-ng/mz.h>
#include <minizip-ng/mz_compat.h>
#include <minizip-ng/mz_strm.h>
#include <minizip-ng/mz_zip.h>
#include <minizip-ng/mz_zip_rw.h>
#include <zlib.h>

using json = nlohmann::json;

#include <algorithm>
#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <future>
#include <mutex>

#ifdef __cpp_lib_format
#include <format>
#else
#include <fmt/format.h>
#endif

#include "atom/error/exception.hpp"

namespace fs = std::filesystem;

namespace {

// 常量定义
constexpr size_t DEFAULT_CHUNK_SIZE = 16384;

// 线程安全的日志包装器
class ThreadSafeLogger {
    std::mutex mutex_;

public:
    template <typename... Args>
    void info(const char* format, Args&&... args) {
        std::lock_guard lock(mutex_);
        LOG_F(INFO, format, std::forward<Args>(args)...);
    }

    template <typename... Args>
    void error(const char* format, Args&&... args) {
        std::lock_guard lock(mutex_);
        LOG_F(ERROR, format, std::forward<Args>(args)...);
    }

    template <typename... Args>
    void warning(const char* format, Args&&... args) {
        std::lock_guard lock(mutex_);
        LOG_F(WARNING, format, std::forward<Args>(args)...);
    }
};

ThreadSafeLogger g_logger;

class ZStreamGuard {
    z_stream stream_;
    bool initialized_{false};

public:
    ZStreamGuard() noexcept {
        stream_.zalloc = Z_NULL;
        stream_.zfree = Z_NULL;
        stream_.opaque = Z_NULL;
    }

    ~ZStreamGuard() {
        if (initialized_) {
            deflateEnd(&stream_);
        }
    }

    bool init(int level) {
        int ret = deflateInit(&stream_, level);
        if (ret == Z_OK) {
            initialized_ = true;
            return true;
        }
        return false;
    }

    z_stream* get() { return &stream_; }
    const z_stream* get() const { return &stream_; }
};

// 错误处理辅助函数
std::string getZlibErrorMessage(int error_code) {
    switch (error_code) {
        case Z_ERRNO:
            return "File operation error";
        case Z_STREAM_ERROR:
            return "Stream state inconsistent";
        case Z_DATA_ERROR:
            return "Input data corrupted";
        case Z_MEM_ERROR:
            return "Out of memory";
        case Z_BUF_ERROR:
            return "Buffer error";
        case Z_VERSION_ERROR:
            return "zlib version incompatible";
        default:
            return "Unknown error";
    }
}

struct ProgressInfo {
    std::atomic<size_t> bytes_processed{0};
    std::atomic<size_t> total_bytes{0};
    std::atomic<bool> cancelled{false};
};

struct ZipCloser {
    void operator()(zipFile file) const {
        if (file) {
            zipClose(file, nullptr);
        }
    }
};

struct UnzipCloser {
    void operator()(unzFile file) const {
        if (file) {
            unzClose(file);
        }
    }
};

}  // anonymous namespace

namespace atom::io {

CompressionResult compressFile(std::string_view file_path,
                               std::string_view output_folder,
                               const CompressionOptions& options) {
    CompressionResult result;
    try {
        if (file_path.empty() || output_folder.empty()) {
            result.error_message = "Empty file path or output folder";
            return result;
        }

        fs::path input_path(file_path);
        if (!fs::exists(input_path)) {
            result.error_message = "Input file does not exist";
            return result;
        }

        fs::path output_dir(output_folder);
        if (!fs::exists(output_dir)) {
            if (!fs::create_directories(output_dir)) {
                result.error_message = "Failed to create output directory";
                return result;
            }
        }

        fs::path output_path = output_dir / input_path.filename().concat(".gz");

        if (options.create_backup && fs::exists(output_path)) {
            fs::path backup_path = output_path.string() + ".bak";
            fs::rename(output_path, backup_path);
        }

        std::ifstream input(input_path, std::ios::binary);
        if (!input) {
            result.error_message = "Failed to open input file";
            return result;
        }

        input.seekg(0, std::ios::end);
        result.original_size = input.tellg();
        input.seekg(0, std::ios::beg);

        gzFile out = gzopen(output_path.string().c_str(), "wb");
        if (!out) {
            result.error_message = "Failed to create output file";
            return result;
        }

        gzsetparams(out, options.level, Z_DEFAULT_STRATEGY);

        std::unique_ptr<gzFile_s, decltype(&gzclose)> out_guard(out, gzclose);

        std::vector<char> buffer(options.chunk_size);

        while (input.read(buffer.data(), buffer.size())) {
            if (gzwrite(out, buffer.data(),
                        static_cast<unsigned>(input.gcount())) <= 0) {
                result.error_message = "Failed to write compressed data";
                return result;
            }
        }

        if (input.gcount() > 0) {
            if (gzwrite(out, buffer.data(),
                        static_cast<unsigned>(input.gcount())) <= 0) {
                result.error_message = "Failed to write final compressed data";
                return result;
            }
        }

        result.compressed_size = fs::file_size(output_path);
        result.compression_ratio = static_cast<double>(result.compressed_size) /
                                   static_cast<double>(result.original_size);
        result.success = true;

        g_logger.info("Successfully compressed {} -> {} (ratio: {:.2f}%)",
                      file_path, output_path.string(),
                      (1.0 - result.compression_ratio) * 100);

    } catch (const std::exception& e) {
        result.error_message =
            std::string("Exception during compression: ") + e.what();
        g_logger.error(result.error_message.c_str());
    }

    return result;
}

CompressionResult decompressFile(std::string_view file_path,
                                 std::string_view output_folder,
                                 const DecompressionOptions& options) {
    CompressionResult result;
    try {
        // 输入验证
        if (file_path.empty() || output_folder.empty()) {
            result.error_message = "Empty file path or output folder";
            return result;
        }

        fs::path input_path(file_path);
        if (!fs::exists(input_path)) {
            result.error_message = "Input file does not exist";
            return result;
        }

        // 创建输出目录
        fs::path output_dir(output_folder);
        if (!fs::exists(output_dir)) {
            if (!fs::create_directories(output_dir)) {
                result.error_message = "Failed to create output directory";
                return result;
            }
        }

        // 准备输出文件路径
        fs::path output_path = output_dir / input_path.stem();

        // 获取原始文件大小
        result.compressed_size = fs::file_size(input_path);

        // 打开压缩文件
        gzFile in = gzopen(input_path.string().c_str(), "rb");
        if (!in) {
            result.error_message = "Failed to open compressed file";
            return result;
        }

        // 使用智能指针确保自动关闭
        std::unique_ptr<gzFile_s, decltype(&gzclose)> in_guard(in, gzclose);

        // 打开输出文件
        std::ofstream output(output_path, std::ios::binary);
        if (!output) {
            result.error_message = "Failed to create output file";
            return result;
        }

        // 设置缓冲区
        std::vector<char> buffer(options.chunk_size);

        // 解压数据
        int bytes_read;
        size_t total_bytes = 0;
        while ((bytes_read = gzread(in, buffer.data(), buffer.size())) > 0) {
            output.write(buffer.data(), bytes_read);
            total_bytes += bytes_read;
        }

        if (bytes_read < 0) {
            result.error_message = "Error during decompression";
            return result;
        }

        result.original_size = total_bytes;
        result.compression_ratio = static_cast<double>(result.compressed_size) /
                                   static_cast<double>(result.original_size);
        result.success = true;

        g_logger.info("Successfully decompressed {} -> {} (ratio: {:.2f}%)",
                      file_path, output_path.string(),
                      (1.0 - result.compression_ratio) * 100);

    } catch (const std::exception& e) {
        result.error_message =
            std::string("Exception during decompression: ") + e.what();
        g_logger.error(result.error_message.c_str());
    }

    return result;
}

CompressionResult compressFolder(std::string_view folder_path,
                                 std::string_view output_path,
                                 const CompressionOptions& options) {
    CompressionResult result;
    try {
        // 输入验证
        fs::path input_dir(folder_path);
        if (!fs::exists(input_dir) || !fs::is_directory(input_dir)) {
            result.error_message = "Invalid input directory";
            return result;
        }

        // 准备输出文件
        fs::path zip_path(output_path);
        if (zip_path.extension() != ".zip") {
            zip_path += ".zip";
        }

        // 创建ZIP文件
        std::unique_ptr<void, ZipCloser> zip_file(
            zipOpen64(zip_path.string().c_str(), 0));
        if (!zip_file) {
            result.error_message = "Failed to create ZIP file";
            return result;
        }

        // 收集所有文件
        std::vector<fs::path> files;
        for (const auto& entry : fs::recursive_directory_iterator(input_dir)) {
            if (fs::is_regular_file(entry)) {
                files.push_back(entry.path());
            }
        }

        result.original_size = 0;
        result.compressed_size = 0;

        // 并行处理文件
        if (options.use_parallel && files.size() > 1) {
            std::mutex writer_mutex;
            std::atomic<bool> has_error{false};
            std::string error_msg;

            auto process_file = [&](const fs::path& file_path) {
                try {
                    // 计算相对路径
                    auto rel_path = fs::relative(file_path, input_dir);

                    // 读取文件数据
                    std::ifstream file(file_path, std::ios::binary);
                    std::vector<char> buffer(options.chunk_size);

                    zip_fileinfo zi = {};
                    auto ftime = fs::last_write_time(file_path);
                    auto tt = std::chrono::system_clock::to_time_t(
                        std::chrono::clock_cast<std::chrono::system_clock>(
                            ftime));
                    auto* tm = std::localtime(&tt);

                    zi.tmz_date.tm_year = tm->tm_year;
                    zi.tmz_date.tm_mon = tm->tm_mon;
                    zi.tmz_date.tm_mday = tm->tm_mday;
                    zi.tmz_date.tm_hour = tm->tm_hour;
                    zi.tmz_date.tm_min = tm->tm_min;
                    zi.tmz_date.tm_sec = tm->tm_sec;

                    {
                        std::lock_guard lock(writer_mutex);

                        // 添加文件到ZIP
                        if (zipOpenNewFileInZip3_64(
                                zip_file.get(), rel_path.string().c_str(), &zi,
                                nullptr, 0, nullptr, 0, nullptr, Z_DEFLATED,
                                options.level, 0, -MAX_WBITS, DEF_MEM_LEVEL,
                                Z_DEFAULT_STRATEGY, nullptr, 0, 0) != UNZ_OK) {
                            has_error = true;
                            error_msg = "Failed to add file to ZIP: " +
                                        rel_path.string();
                            return;
                        }

                        // 写入数据
                        while (file.read(buffer.data(), buffer.size())) {
                            if (zipWriteInFileInZip(
                                    zip_file.get(), buffer.data(),
                                    static_cast<unsigned int>(file.gcount())) !=
                                UNZ_OK) {
                                has_error = true;
                                error_msg = "Failed to write file data: " +
                                            rel_path.string();
                                return;
                            }
                        }

                        // 关闭当前文件
                        if (zipCloseFileInZip(zip_file.get()) != UNZ_OK) {
                            has_error = true;
                            error_msg = "Failed to close file in ZIP: " +
                                        rel_path.string();
                            return;
                        }
                    }

                } catch (const std::exception& e) {
                    has_error = true;
                    error_msg =
                        std::string("Exception while processing file: ") +
                        e.what();
                }
            };

            // 创建线程池
            std::vector<std::future<void>> futures;
            for (const auto& file : files) {
                futures.push_back(
                    std::async(std::launch::async, process_file, file));
            }

            // 等待所有任务完成
            for (auto& future : futures) {
                future.wait();
            }

            if (has_error) {
                result.error_message = error_msg;
                return result;
            }

        } else {
            // 顺序处理文件
            for (const auto& file_path : files) {
                auto rel_path = fs::relative(file_path, input_dir);

                zip_fileinfo zi = {};
                auto ftime = fs::last_write_time(file_path);
                auto tt = std::chrono::system_clock::to_time_t(
                    std::chrono::clock_cast<std::chrono::system_clock>(ftime));
                auto* tm = std::localtime(&tt);

                zi.tmz_date.tm_year = tm->tm_year;
                zi.tmz_date.tm_mon = tm->tm_mon;
                zi.tmz_date.tm_mday = tm->tm_mday;
                zi.tmz_date.tm_hour = tm->tm_hour;
                zi.tmz_date.tm_min = tm->tm_min;
                zi.tmz_date.tm_sec = tm->tm_sec;

                if (zipOpenNewFileInZip3_64(
                        zip_file.get(), rel_path.string().c_str(), &zi, nullptr,
                        0, nullptr, 0, nullptr, Z_DEFLATED, options.level, 0,
                        -MAX_WBITS, DEF_MEM_LEVEL, Z_DEFAULT_STRATEGY, nullptr,
                        0, 0) != UNZ_OK) {
                    result.error_message =
                        "Failed to add file to ZIP: " + rel_path.string();
                    return result;
                }

                std::ifstream file(file_path, std::ios::binary);
                std::vector<char> buffer(options.chunk_size);

                while (file.read(buffer.data(), buffer.size())) {
                    if (zipWriteInFileInZip(zip_file.get(), buffer.data(),
                                            static_cast<unsigned int>(
                                                file.gcount())) != UNZ_OK) {
                        result.error_message =
                            "Failed to write file data: " + rel_path.string();
                        return result;
                    }
                }

                if (zipCloseFileInZip(zip_file.get()) != UNZ_OK) {
                    result.error_message = "Failed to close file in ZIP";
                    return result;
                }

                result.original_size += fs::file_size(file_path);
            }
        }

        result.compressed_size = fs::file_size(zip_path);
        result.compression_ratio = static_cast<double>(result.compressed_size) /
                                   static_cast<double>(result.original_size);
        result.success = true;

        g_logger.info(
            "Successfully compressed folder {} -> {} (ratio: {:.2f}%)",
            folder_path, zip_path.string(),
            (1.0 - result.compression_ratio) * 100);

    } catch (const std::exception& e) {
        result.error_message =
            std::string("Exception during folder compression: ") + e.what();
        g_logger.error(result.error_message.c_str());
    }

    return result;
}

CompressionResult extractZip(std::string_view zip_path,
                             std::string_view output_folder,
                             const DecompressionOptions& options) {
    CompressionResult result;
    try {
        // 输入验证
        if (zip_path.empty() || output_folder.empty()) {
            result.error_message = "Empty ZIP path or output folder";
            return result;
        }

        fs::path zip_file(zip_path);
        if (!fs::exists(zip_file)) {
            result.error_message = "ZIP file does not exist";
            return result;
        }

        // 创建输出目录
        fs::path output_dir(output_folder);
        if (!fs::exists(output_dir)) {
            if (!fs::create_directories(output_dir)) {
                result.error_message = "Failed to create output directory";
                return result;
            }
        }

        // 打开ZIP文件
        std::unique_ptr<void, UnzipCloser> unz(
            unzOpen64(zip_file.string().c_str()));
        if (!unz) {
            result.error_message = "Failed to open ZIP file";
            return result;
        }

        result.compressed_size = fs::file_size(zip_file);
        result.original_size = 0;

        // 获取ZIP文件信息
        unz_global_info64 gi;
        if (unzGetGlobalInfo64(unz.get(), &gi) != UNZ_OK) {
            result.error_message = "Failed to get ZIP file info";
            return result;
        }

        // 处理所有文件
        if (unzGoToFirstFile(unz.get()) != UNZ_OK) {
            result.error_message = "Failed to go to first file in ZIP";
            return result;
        }

        std::vector<char> buffer(options.chunk_size);

        do {
            // 获取当前文件信息
            unz_file_info64 file_info;
            char filename[512];
            if (unzGetCurrentFileInfo64(unz.get(), &file_info, filename,
                                        sizeof(filename), nullptr, 0, nullptr,
                                        0) != UNZ_OK) {
                result.error_message = "Failed to get file info";
                return result;
            }

            fs::path target_path = output_dir / filename;

            // 如果是目录，创建它
            if (filename[strlen(filename) - 1] == '/') {
                fs::create_directories(target_path);
                continue;
            }

            // 确保父目录存在
            fs::create_directories(target_path.parent_path());

            // 打开ZIP中的文件
            if (unzOpenCurrentFilePassword(
                    unz.get(), options.password.empty()
                                   ? nullptr
                                   : options.password.c_str()) != UNZ_OK) {
                result.error_message =
                    "Failed to open file in ZIP: " + std::string(filename);
                return result;
            }

            // 打开输出文件
            std::ofstream outfile(target_path, std::ios::binary);
            if (!outfile) {
                unzCloseCurrentFile(unz.get());
                result.error_message =
                    "Failed to create output file: " + target_path.string();
                return result;
            }

            // 读取和写入文件内容
            int err = UNZ_OK;
            do {
                err =
                    unzReadCurrentFile(unz.get(), buffer.data(), buffer.size());
                if (err < 0) {
                    unzCloseCurrentFile(unz.get());
                    result.error_message = "Error reading ZIP file";
                    return result;
                }

                if (err > 0) {
                    outfile.write(buffer.data(), err);
                    if (!outfile) {
                        unzCloseCurrentFile(unz.get());
                        result.error_message = "Error writing output file";
                        return result;
                    }
                    result.original_size += err;
                }
            } while (err > 0);

            // 设置文件时间
            fs::last_write_time(target_path, fs::file_time_type::clock::now());

            unzCloseCurrentFile(unz.get());
            outfile.close();

            g_logger.info("Extracted: {}", filename);

        } while (unzGoToNextFile(unz.get()) == UNZ_OK);

        result.success = true;
        result.compression_ratio = static_cast<double>(result.compressed_size) /
                                   static_cast<double>(result.original_size);

        g_logger.info("Successfully extracted {} files from {} -> {}",
                      gi.number_entry, zip_path, output_folder);

    } catch (const std::exception& e) {
        result.error_message =
            std::string("Exception during extraction: ") + e.what();
        g_logger.error(result.error_message.c_str());
    }

    return result;
}

std::vector<ZipFileInfo> listZipContents(std::string_view zip_path) {
    std::vector<ZipFileInfo> result;

    try {
        // 打开ZIP文件
        std::unique_ptr<void, UnzipCloser> unz(unzOpen64(zip_path.data()));
        if (!unz) {
            g_logger.error("Failed to open ZIP file: {}", zip_path);
            return result;
        }

        // 获取ZIP文件信息
        unz_global_info64 gi;
        if (unzGetGlobalInfo64(unz.get(), &gi) != UNZ_OK) {
            g_logger.error("Failed to get ZIP file info");
            return result;
        }

        // 预分配空间
        result.reserve(gi.number_entry);

        // 遍历所有文件
        if (unzGoToFirstFile(unz.get()) != UNZ_OK) {
            g_logger.error("Failed to go to first file in ZIP");
            return result;
        }

        do {
            unz_file_info64 file_info;
            char filename[512];

            if (unzGetCurrentFileInfo64(unz.get(), &file_info, filename,
                                        sizeof(filename), nullptr, 0, nullptr,
                                        0) != UNZ_OK) {
                g_logger.error("Failed to get file info");
                continue;
            }

            ZipFileInfo info;
            info.name = filename;
            info.size = file_info.uncompressed_size;
            info.compressed_size = file_info.compressed_size;
            info.is_directory = (filename[strlen(filename) - 1] == '/');
            info.is_encrypted = (file_info.flag & 1) != 0;
            info.crc = file_info.crc;

            // 构建时间字符串
            char datetime[32];
            std::snprintf(
                datetime, sizeof(datetime), "%04u-%02u-%02u %02u:%02u:%02u",
                file_info.tmu_date.tm_year + 1900,
                file_info.tmu_date.tm_mon + 1, file_info.tmu_date.tm_mday,
                file_info.tmu_date.tm_hour, file_info.tmu_date.tm_min,
                file_info.tmu_date.tm_sec);
            info.datetime = datetime;

            result.push_back(std::move(info));

        } while (unzGoToNextFile(unz.get()) == UNZ_OK);

        g_logger.info("Listed {} files in ZIP: {}", result.size(), zip_path);

    } catch (const std::exception& e) {
        g_logger.error("Exception in listZipContents: {}", e.what());
        result.clear();
    }

    return result;
}

bool fileExistsInZip(std::string_view zip_path, std::string_view file_path) {
    try {
        // 打开ZIP文件
        std::unique_ptr<void, UnzipCloser> unz(unzOpen64(zip_path.data()));
        if (!unz) {
            g_logger.error("Failed to open ZIP file: {}", zip_path);
            return false;
        }

        // 定位文件
        if (unzLocateFile(unz.get(), file_path.data(), 0) != UNZ_OK) {
            g_logger.info("File not found in ZIP: {}", file_path);
            return false;
        }

        g_logger.info("File exists in ZIP: {}", file_path);
        return true;

    } catch (const std::exception& e) {
        g_logger.error("Exception in fileExistsInZip: {}", e.what());
        return false;
    }
}

CompressionResult removeFromZip(std::string_view zip_path,
                                std::string_view file_path) {
    CompressionResult result;
    try {
        // 输入验证
        if (zip_path.empty() || file_path.empty()) {
            result.error_message = "Empty ZIP path or file path";
            return result;
        }

        fs::path zip_file(zip_path);
        if (!fs::exists(zip_file)) {
            result.error_message = "ZIP file does not exist";
            return result;
        }

        // 创建临时ZIP文件
        fs::path temp_zip = zip_file.string() + ".tmp";

        // 打开源ZIP文件
        std::unique_ptr<void, UnzipCloser> src_zip(
            unzOpen64(zip_file.string().c_str()));
        if (!src_zip) {
            result.error_message = "Failed to open source ZIP file";
            return result;
        }

        // 创建目标ZIP文件
        std::unique_ptr<void, ZipCloser> dst_zip(
            zipOpen64(temp_zip.string().c_str(), 0));
        if (!dst_zip) {
            result.error_message = "Failed to create temporary ZIP file";
            return result;
        }

        // 获取源ZIP文件信息
        unz_global_info64 gi;
        if (unzGetGlobalInfo64(src_zip.get(), &gi) != UNZ_OK) {
            result.error_message = "Failed to get ZIP file info";
            return result;
        }

        // 设置缓冲区
        std::vector<char> buffer(DEFAULT_CHUNK_SIZE);

        // 遍历所有文件
        if (unzGoToFirstFile(src_zip.get()) != UNZ_OK) {
            result.error_message = "Failed to go to first file in ZIP";
            return result;
        }

        do {
            unz_file_info64 file_info;
            char filename[512];
            if (unzGetCurrentFileInfo64(src_zip.get(), &file_info, filename,
                                        sizeof(filename), nullptr, 0, nullptr,
                                        0) != UNZ_OK) {
                result.error_message = "Failed to get file info";
                return result;
            }

            // 跳过要删除的文件
            if (filename == std::string(file_path)) {
                continue;
            }

            // 打开源文件
            if (unzOpenCurrentFile(src_zip.get()) != UNZ_OK) {
                result.error_message = "Failed to open file in source ZIP";
                return result;
            }

            // 准备复制到新ZIP
            zip_fileinfo zi = {};
            zi.tmz_date.tm_year = file_info.tmu_date.tm_year;
            zi.tmz_date.tm_mon = file_info.tmu_date.tm_mon;
            zi.tmz_date.tm_mday = file_info.tmu_date.tm_mday;
            zi.tmz_date.tm_hour = file_info.tmu_date.tm_hour;
            zi.tmz_date.tm_min = file_info.tmu_date.tm_min;
            zi.tmz_date.tm_sec = file_info.tmu_date.tm_sec;

            if (zipOpenNewFileInZip3_64(dst_zip.get(), filename, &zi, nullptr,
                                        0, nullptr, 0, nullptr, Z_DEFLATED,
                                        Z_DEFAULT_COMPRESSION, 0, -MAX_WBITS,
                                        DEF_MEM_LEVEL, Z_DEFAULT_STRATEGY,
                                        nullptr, 0, 0) != UNZ_OK) {
                unzCloseCurrentFile(src_zip.get());
                result.error_message =
                    "Failed to create file in destination ZIP";
                return result;
            }

            // 复制数据
            int bytes_read;
            while ((bytes_read = unzReadCurrentFile(
                        src_zip.get(), buffer.data(), buffer.size())) > 0) {
                if (zipWriteInFileInZip(dst_zip.get(), buffer.data(),
                                        static_cast<unsigned>(bytes_read)) !=
                    UNZ_OK) {
                    unzCloseCurrentFile(src_zip.get());
                    zipCloseFileInZip(dst_zip.get());
                    result.error_message = "Failed to write to destination ZIP";
                    return result;
                }
            }

            if (bytes_read < 0) {
                unzCloseCurrentFile(src_zip.get());
                zipCloseFileInZip(dst_zip.get());
                result.error_message = "Error reading from source ZIP";
                return result;
            }

            unzCloseCurrentFile(src_zip.get());
            zipCloseFileInZip(dst_zip.get());

        } while (unzGoToNextFile(src_zip.get()) == UNZ_OK);

        // 关闭所有文件句柄 (智能指针会自动处理)

        // 替换原始文件
        fs::remove(zip_file);
        fs::rename(temp_zip, zip_file);

        result.success = true;
        g_logger.info("Successfully removed {} from ZIP file {}", file_path,
                      zip_path);

    } catch (const std::exception& e) {
        result.error_message =
            std::string("Exception during file removal: ") + e.what();
        g_logger.error(result.error_message.c_str());
    }

    return result;
}

std::optional<size_t> getZipSize(std::string_view zip_path) {
    try {
        if (zip_path.empty()) {
            g_logger.error("Empty ZIP path");
            return std::nullopt;
        }

        fs::path zip_file(zip_path);
        if (!fs::exists(zip_file)) {
            g_logger.error("ZIP file does not exist: {}", zip_path);
            return std::nullopt;
        }

        auto size = fs::file_size(zip_file);
        g_logger.info("ZIP file size: {} bytes", size);
        return size;

    } catch (const std::exception& e) {
        g_logger.error("Exception in getZipSize: {}", e.what());
        return std::nullopt;
    }
}

CompressionResult compressFileInSlices(std::string_view file_path,
                                       size_t slice_size,
                                       const CompressionOptions& options) {
    CompressionResult result;
    try {
        if (file_path.empty() || slice_size == 0) {
            result.error_message = "Invalid parameters";
            return result;
        }

        fs::path input_path(file_path);
        if (!fs::exists(input_path)) {
            result.error_message = "Input file does not exist";
            return result;
        }

        // 获取文件大小
        result.original_size = fs::file_size(input_path);
        result.compressed_size = 0;

        // 计算分片数量
        size_t num_slices =
            (result.original_size + slice_size - 1) / slice_size;

        // 打开输入文件
        std::ifstream input(input_path, std::ios::binary);
        if (!input) {
            result.error_message = "Failed to open input file";
            return result;
        }

        // 创建JSON清单文件
        json manifest;
        manifest["original_file"] = input_path.filename().string();
        manifest["original_size"] = result.original_size;
        manifest["slice_size"] = slice_size;
        manifest["num_slices"] = num_slices;
        manifest["compression_level"] = options.level;
        manifest["created_at"] =
            std::chrono::system_clock::now().time_since_epoch().count();

        // 并行处理分片
        if (options.use_parallel && num_slices > 1) {
            std::vector<std::future<bool>> futures;
            std::atomic<size_t> total_compressed_size = 0;
            std::mutex error_mutex;
            std::string error_message;

            for (size_t i = 0; i < num_slices; ++i) {
                futures.push_back(
                    std::async(std::launch::async, [&, i]() -> bool {
                        try {
                            // 计算当前分片大小
                            size_t current_slice_size =
                                (i == num_slices - 1)
                                    ? (result.original_size - i * slice_size)
                                    : slice_size;

                            // 创建分片文件名
                            fs::path slice_path = input_path.string() +
                                                  ".slice_" +
                                                  std::to_string(i) + ".gz";

                            // 读取数据
                            std::vector<char> buffer(current_slice_size);
                            {
                                std::ifstream slice_input(input_path,
                                                          std::ios::binary);
                                slice_input.seekg(i * slice_size);
                                slice_input.read(buffer.data(),
                                                 current_slice_size);
                            }

                            // 压缩数据
                            gzFile out =
                                gzopen(slice_path.string().c_str(), "wb");
                            if (!out) {
                                std::lock_guard lock(error_mutex);
                                error_message =
                                    "Failed to create compressed slice";
                                return false;
                            }

                            gzsetparams(out, options.level, Z_DEFAULT_STRATEGY);

                            if (gzwrite(out, buffer.data(),
                                        static_cast<unsigned>(
                                            current_slice_size)) <= 0) {
                                gzclose(out);
                                std::lock_guard lock(error_mutex);
                                error_message =
                                    "Failed to write compressed data";
                                return false;
                            }

                            gzclose(out);

                            // 更新总压缩大小
                            total_compressed_size += fs::file_size(slice_path);
                            return true;

                        } catch (const std::exception& e) {
                            std::lock_guard lock(error_mutex);
                            error_message =
                                std::string(
                                    "Exception in slice compression: ") +
                                e.what();
                            return false;
                        }
                    }));
            }

            // 等待所有分片完成
            bool all_success = true;
            for (auto& future : futures) {
                if (!future.get()) {
                    all_success = false;
                    break;
                }
            }

            if (!all_success) {
                result.error_message = error_message;
                return result;
            }

            result.compressed_size = total_compressed_size;

        } else {
            // 顺序处理分片
            std::vector<char> buffer(slice_size);

            for (size_t i = 0; i < num_slices; ++i) {
                size_t current_slice_size =
                    (i == num_slices - 1)
                        ? (result.original_size - i * slice_size)
                        : slice_size;

                // 创建分片文件名
                fs::path slice_path =
                    input_path.string() + ".slice_" + std::to_string(i) + ".gz";

                // 读取数据
                input.read(buffer.data(), current_slice_size);

                // 压缩数据
                gzFile out = gzopen(slice_path.string().c_str(), "wb");
                if (!out) {
                    result.error_message = "Failed to create compressed slice";
                    return result;
                }

                gzsetparams(out, options.level, Z_DEFAULT_STRATEGY);

                if (gzwrite(out, buffer.data(),
                            static_cast<unsigned>(current_slice_size)) <= 0) {
                    gzclose(out);
                    result.error_message = "Failed to write compressed data";
                    return result;
                }

                gzclose(out);
                result.compressed_size += fs::file_size(slice_path);
            }
        }

        // 写入清单文件
        manifest["compressed_size"] = result.compressed_size;
        manifest["compression_ratio"] =
            static_cast<double>(result.compressed_size) /
            static_cast<double>(result.original_size);

        std::ofstream manifest_file(input_path.string() + ".manifest.json");
        manifest_file << manifest.dump(4);

        result.success = true;
        result.compression_ratio = static_cast<double>(result.compressed_size) /
                                   static_cast<double>(result.original_size);

        g_logger.info("Successfully created {} slices for {} (ratio: {:.2f}%)",
                      num_slices, file_path,
                      (1.0 - result.compression_ratio) * 100);

    } catch (const std::exception& e) {
        result.error_message =
            std::string("Exception in slice compression: ") + e.what();
        g_logger.error(result.error_message.c_str());
    }

    return result;
}

CompressionResult mergeCompressedSlices(
    const std::vector<std::string>& slice_files, std::string_view output_path,
    const DecompressionOptions& options) {
    CompressionResult result;
    try {
        if (slice_files.empty() || output_path.empty()) {
            result.error_message = "Invalid parameters";
            return result;
        }

        // 打开输出文件
        std::ofstream output(output_path.data(), std::ios::binary);
        if (!output) {
            result.error_message = "Failed to create output file";
            return result;
        }

        result.original_size = 0;
        result.compressed_size = 0;

        // 并行处理分片
        if (options.use_parallel) {
            std::vector<std::future<std::pair<bool, std::vector<char>>>>
                futures;
            std::string error_message;

            for (const auto& slice_file : slice_files) {
                futures.push_back(std::async(
                    std::launch::async,
                    [&](const std::string& slice)
                        -> std::pair<bool, std::vector<char>> {
                        try {
                            gzFile in = gzopen(slice.c_str(), "rb");
                            if (!in) {
                                THROW_RUNTIME_ERROR(
                                    "Failed to open slice file");
                            }

                            std::vector<char> buffer;
                            std::vector<char> chunk(options.chunk_size);
                            int bytes_read;

                            while ((bytes_read = gzread(in, chunk.data(),
                                                        chunk.size())) > 0) {
                                buffer.insert(buffer.end(), chunk.begin(),
                                              chunk.begin() + bytes_read);
                            }

                            gzclose(in);

                            if (bytes_read < 0) {
                                THROW_RUNTIME_ERROR(
                                    "Error reading compressed data");
                            }

                            return {true, std::move(buffer)};

                        } catch (const std::exception& e) {
                            return {false, std::vector<char>()};
                        }
                    },
                    slice_file));
            }

            // 收集结果
            std::vector<std::pair<size_t, std::vector<char>>> decompressed_data;
            decompressed_data.reserve(futures.size());

            for (size_t i = 0; i < futures.size(); ++i) {
                auto [success, data] = futures[i].get();
                if (!success) {
                    result.error_message =
                        "Failed to decompress slice " + std::to_string(i);
                    return result;
                }
                decompressed_data.emplace_back(i, std::move(data));
            }

            // 按顺序写入
            std::sort(decompressed_data.begin(), decompressed_data.end());
            for (const auto& [_, data] : decompressed_data) {
                output.write(data.data(), data.size());
                result.original_size += data.size();
            }

        } else {
            // 顺序处理
            std::vector<char> buffer(options.chunk_size);

            for (const auto& slice_file : slice_files) {
                gzFile in = gzopen(slice_file.c_str(), "rb");
                if (!in) {
                    result.error_message = "Failed to open slice file";
                    return result;
                }

                int bytes_read;
                while ((bytes_read = gzread(in, buffer.data(), buffer.size())) >
                       0) {
                    output.write(buffer.data(), bytes_read);
                    result.original_size += bytes_read;
                }

                gzclose(in);

                if (bytes_read < 0) {
                    result.error_message = "Error reading compressed data";
                    return result;
                }

                result.compressed_size += fs::file_size(slice_file);
            }
        }

        result.success = true;
        result.compression_ratio = static_cast<double>(result.compressed_size) /
                                   static_cast<double>(result.original_size);

        g_logger.info("Successfully merged {} slices into {} (ratio: {:.2f}%)",
                      slice_files.size(), output_path,
                      (1.0 - result.compression_ratio) * 100);

    } catch (const std::exception& e) {
        result.error_message =
            std::string("Exception in slice merging: ") + e.what();
        g_logger.error(result.error_message.c_str());
    }

    return result;
}

template <typename T>
    requires std::ranges::contiguous_range<T>
std::pair<CompressionResult, std::vector<unsigned char>> compressData(
    const T& data, const CompressionOptions& options) {
    std::pair<CompressionResult, std::vector<unsigned char>> result;
    auto& [compression_result, compressed_data] = result;

    try {
        if (data.empty()) {
            compression_result.error_message = "Empty input data";
            return result;
        }

        compression_result.original_size = data.size();

        // 估算压缩后的大小
        uLong compressed_bound = compressBound(data.size());
        compressed_data.resize(compressed_bound);

        // 压缩数据
        uLongf compressed_size = compressed_bound;
        int ret = compress2(reinterpret_cast<Bytef*>(compressed_data.data()),
                            &compressed_size,
                            reinterpret_cast<const Bytef*>(data.data()),
                            data.size(), options.level);

        if (ret != Z_OK) {
            compression_result.error_message = getZlibErrorMessage(ret);
            compressed_data.clear();
            return result;
        }

        // 调整到实际大小
        compressed_data.resize(compressed_size);
        compression_result.compressed_size = compressed_size;
        compression_result.compression_ratio =
            static_cast<double>(compressed_size) /
            static_cast<double>(data.size());
        compression_result.success = true;

        g_logger.info(
            "Successfully compressed {} bytes to {} bytes (ratio: {:.2f}%)",
            data.size(), compressed_size,
            (1.0 - compression_result.compression_ratio) * 100);

    } catch (const std::exception& e) {
        compression_result.error_message =
            std::string("Exception during compression: ") + e.what();
        g_logger.error(compression_result.error_message.c_str());
        compressed_data.clear();
    }

    return result;
}

template std::pair<CompressionResult, std::vector<unsigned char>>
compressData<std::vector<unsigned char>>(const std::vector<unsigned char>&,
                                         const CompressionOptions&);
template std::pair<CompressionResult, std::vector<unsigned char>>
compressData<std::string>(const std::string&, const CompressionOptions&);
template std::pair<CompressionResult, std::vector<unsigned char>>
compressData<std::vector<char>>(const std::vector<char>&,
                                const CompressionOptions&);
template std::pair<CompressionResult, std::vector<unsigned char>>
compressData<std::span<const unsigned char>>(
    const std::span<const unsigned char>&, const CompressionOptions&);
template std::pair<CompressionResult, std::vector<unsigned char>>
compressData<std::span<const char>>(const std::span<const char>&,
                                    const CompressionOptions&);

template <typename T>
    requires std::ranges::contiguous_range<T>
std::pair<CompressionResult, std::vector<unsigned char>> decompressData(
    const T& compressed_data, size_t expected_size,
    const DecompressionOptions& options) {
    std::pair<CompressionResult, std::vector<unsigned char>> result;
    auto& [compression_result, decompressed_data] = result;

    try {
        if (compressed_data.empty()) {
            compression_result.error_message = "Empty compressed data";
            return result;
        }

        compression_result.compressed_size = compressed_data.size();

        // 如果提供了预期大小，使用它；否则估算
        size_t buffer_size =
            expected_size > 0 ? expected_size : compressed_data.size() * 4;
        decompressed_data.resize(buffer_size);

        // 使用 options.verify_checksum 检测数据完整性
        int window_bits = options.verify_checksum ? 15 : -15;

        z_stream zs = {};
        zs.next_in = const_cast<Bytef*>(
            reinterpret_cast<const Bytef*>(compressed_data.data()));
        zs.avail_in = compressed_data.size();
        zs.next_out = reinterpret_cast<Bytef*>(decompressed_data.data());
        zs.avail_out = buffer_size;

        int ret = inflateInit2(&zs, window_bits);
        if (ret != Z_OK) {
            compression_result.error_message = getZlibErrorMessage(ret);
            return result;
        }

        // 确保资源释放
        std::unique_ptr<z_stream, decltype(&inflateEnd)> guard(&zs, inflateEnd);

        ret = inflate(&zs, Z_FINISH);

        // 处理缓冲区不足的情况
        if (ret == Z_BUF_ERROR && expected_size == 0) {
            // 重新设置解压缩状态
            inflateEnd(&zs);

            // 使用更大的缓冲区重试
            buffer_size *= 4;
            decompressed_data.resize(buffer_size);

            zs = {};
            zs.next_in = const_cast<Bytef*>(
                reinterpret_cast<const Bytef*>(compressed_data.data()));
            zs.avail_in = compressed_data.size();
            zs.next_out = reinterpret_cast<Bytef*>(decompressed_data.data());
            zs.avail_out = buffer_size;

            ret = inflateInit2(&zs, window_bits);
            if (ret != Z_OK) {
                compression_result.error_message = getZlibErrorMessage(ret);
                return result;
            }

            // 更新智能指针
            guard = std::unique_ptr<z_stream, decltype(&inflateEnd)>(
                &zs, inflateEnd);
            ret = inflate(&zs, Z_FINISH);
        }

        if (ret != Z_STREAM_END && ret != Z_OK) {
            compression_result.error_message = getZlibErrorMessage(ret);
            decompressed_data.clear();
            return result;
        }

        // 调整到实际大小
        size_t decompressed_size = zs.total_out;
        decompressed_data.resize(decompressed_size);
        compression_result.original_size = decompressed_size;
        compression_result.compression_ratio =
            static_cast<double>(compressed_data.size()) /
            static_cast<double>(decompressed_size);
        compression_result.success = true;

        g_logger.info(
            "Successfully decompressed {} bytes to {} bytes (ratio: {:.2f}%)",
            compressed_data.size(), decompressed_size,
            (1.0 - compression_result.compression_ratio) * 100);

    } catch (const std::exception& e) {
        compression_result.error_message =
            std::string("Exception during decompression: ") + e.what();
        g_logger.error(compression_result.error_message.c_str());
        decompressed_data.clear();
    }

    return result;
}

template std::pair<CompressionResult, std::vector<unsigned char>>
decompressData<std::vector<unsigned char>>(const std::vector<unsigned char>&,
                                           size_t, const DecompressionOptions&);
template std::pair<CompressionResult, std::vector<unsigned char>>
decompressData<std::string>(const std::string&, size_t,
                            const DecompressionOptions&);
template std::pair<CompressionResult, std::vector<unsigned char>>
decompressData<std::vector<char>>(const std::vector<char>&, size_t,
                                  const DecompressionOptions&);
template std::pair<CompressionResult, std::vector<unsigned char>>
decompressData<std::span<const unsigned char>>(
    const std::span<const unsigned char>&, size_t, const DecompressionOptions&);
template std::pair<CompressionResult, std::vector<unsigned char>>
decompressData<std::span<const char>>(const std::span<const char>&, size_t,
                                      const DecompressionOptions&);

}  // namespace atom::io
