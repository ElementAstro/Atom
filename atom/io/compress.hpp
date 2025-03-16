/*
 * compress.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-3-31

Description: Compressor using ZLib and MiniZip-ng

**************************************************/

#ifndef ATOM_IO_COMPRESS_HPP
#define ATOM_IO_COMPRESS_HPP

#include <future>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace atom::io {

// 前向声明压缩选项结构体
struct CompressionOptions;
struct DecompressionOptions;

// 压缩状态和结果结构
struct CompressionResult {
    bool success{false};
    std::string error_message;
    size_t original_size{0};
    size_t compressed_size{0};
    double compression_ratio{0.0};
};

// 基本的压缩选项
struct CompressionOptions {
    int level{-1};             // 压缩级别 (-1 = 默认, 0-9)
    size_t chunk_size{16384};  // 处理块大小
    bool use_parallel{true};   // 是否使用并行处理
    size_t num_threads{std::thread::hardware_concurrency()};  // 并行线程数
    bool create_backup{false};  // 是否创建备份
    std::string password;       // 加密密码(可选)
};

// 基本的解压选项
struct DecompressionOptions {
    size_t chunk_size{16384};  // 处理块大小
    bool use_parallel{true};   // 是否使用并行处理
    size_t num_threads{std::thread::hardware_concurrency()};  // 并行线程数
    bool verify_checksum{true};  // 是否验证校验和
    std::string password;        // 解密密码(如果需要)
};

/**
 * @brief 压缩单个文件
 * @param file_path 要压缩的文件路径
 * @param output_folder 输出文件夹
 * @param options 压缩选项
 * @return 压缩结果
 */
CompressionResult compressFile(
    std::string_view file_path, std::string_view output_folder,
    const CompressionOptions& options = CompressionOptions{});

/**
 * @brief 解压单个文件
 * @param file_path 要解压的文件路径
 * @param output_folder 输出文件夹
 * @param options 解压选项
 * @return 操作结果
 */
CompressionResult decompressFile(
    std::string_view file_path, std::string_view output_folder,
    const DecompressionOptions& options = DecompressionOptions{});

/**
 * @brief 压缩整个文件夹
 * @param folder_path 要压缩的文件夹路径
 * @param output_path 输出文件路径
 * @param options 压缩选项
 * @return 压缩结果
 */
CompressionResult compressFolder(
    std::string_view folder_path, std::string_view output_path,
    const CompressionOptions& options = CompressionOptions{});

/**
 * @brief 解压ZIP文件
 * @param zip_path ZIP文件路径
 * @param output_folder 输出文件夹
 * @param options 解压选项
 * @return 操作结果
 */
CompressionResult extractZip(
    std::string_view zip_path, std::string_view output_folder,
    const DecompressionOptions& options = DecompressionOptions{});

/**
 * @brief 创建ZIP文件
 * @param source_path 源文件夹或文件路径
 * @param zip_path 目标ZIP文件路径
 * @param options 压缩选项
 * @return 操作结果
 */
CompressionResult createZip(
    std::string_view source_path, std::string_view zip_path,
    const CompressionOptions& options = CompressionOptions{});

// ZIP文件信息结构
struct ZipFileInfo {
    std::string name;
    size_t size;
    size_t compressed_size;
    std::string datetime;
    bool is_directory;
    bool is_encrypted;
    uint32_t crc;
};

/**
 * @brief 列出ZIP文件中的内容
 * @param zip_path ZIP文件路径
 * @return 文件信息列表
 */
std::vector<ZipFileInfo> listZipContents(std::string_view zip_path);

/**
 * @brief 检查文件在ZIP中是否存在
 * @param zip_path ZIP文件路径
 * @param file_path 要检查的文件路径
 * @return 是否存在
 */
bool fileExistsInZip(std::string_view zip_path, std::string_view file_path);

/**
 * @brief 从ZIP文件移除指定文件
 * @param zip_path ZIP文件路径
 * @param file_path 要移除的文件路径
 * @return 操作结果
 */
CompressionResult removeFromZip(std::string_view zip_path,
                                std::string_view file_path);

/**
 * @brief 获取ZIP文件大小
 * @param zip_path ZIP文件路径
 * @return 文件大小(字节)
 */
std::optional<size_t> getZipSize(std::string_view zip_path);

/**
 * @brief 分片压缩大文件
 * @param file_path 要压缩的文件路径
 * @param slice_size 分片大小(字节)
 * @param options 压缩选项
 * @return 操作结果
 */
CompressionResult compressFileInSlices(
    std::string_view file_path, size_t slice_size,
    const CompressionOptions& options = CompressionOptions{});

/**
 * @brief 合并压缩分片
 * @param slice_files 分片文件路径列表
 * @param output_path 输出文件路径
 * @param options 解压选项
 * @return 操作结果
 */
CompressionResult mergeCompressedSlices(
    const std::vector<std::string>& slice_files, std::string_view output_path,
    const DecompressionOptions& options = DecompressionOptions{});

/**
 * @brief 异步处理多个文件
 * @param file_paths 文件路径列表
 * @param options 压缩选项
 * @return future
 */
std::future<std::vector<CompressionResult>> processFilesAsync(
    const std::vector<std::string>& file_paths,
    const CompressionOptions& options = CompressionOptions{});

/**
 * @brief 创建文件备份(可选压缩)
 * @param source_path 源文件路径
 * @param backup_path 备份文件路径
 * @param compress 是否压缩
 * @param options 压缩选项
 * @return 操作结果
 */
CompressionResult createBackup(
    std::string_view source_path, std::string_view backup_path,
    bool compress = false,
    const CompressionOptions& options = CompressionOptions{});

/**
 * @brief 从备份恢复文件
 * @param backup_path 备份文件路径
 * @param restore_path 恢复文件路径
 * @param compressed 备份是否压缩
 * @param options 解压选项
 * @return 操作结果
 */
CompressionResult restoreFromBackup(
    std::string_view backup_path, std::string_view restore_path,
    bool compressed = false,
    const DecompressionOptions& options = DecompressionOptions{});

/**
 * @brief 通用数据压缩模板
 * @tparam T 输入数据类型
 * @param data 要压缩的数据
 * @param options 压缩选项
 * @return 压缩结果和数据
 */
template <typename T>
    requires std::ranges::contiguous_range<T>
std::pair<CompressionResult, std::vector<unsigned char>> compressData(
    const T& data, const CompressionOptions& options = CompressionOptions{});

/**
 * @brief 通用数据解压模板
 * @tparam T 输入数据类型
 * @param compressed_data 压缩数据
 * @param expected_size 预期解压大小(可选)
 * @param options 解压选项
 * @return 解压结果和数据
 */
template <typename T>
    requires std::ranges::contiguous_range<T>
std::pair<CompressionResult, std::vector<unsigned char>> decompressData(
    const T& compressed_data, size_t expected_size = 0,
    const DecompressionOptions& options = DecompressionOptions{});

// 显式模板实例化声明
extern template std::pair<CompressionResult, std::vector<unsigned char>>
compressData<std::vector<unsigned char>>(const std::vector<unsigned char>&,
                                         const CompressionOptions&);

extern template std::pair<CompressionResult, std::vector<unsigned char>>
decompressData<std::vector<unsigned char>>(const std::vector<unsigned char>&,
                                           size_t, const DecompressionOptions&);

}  // namespace atom::io

#endif  // ATOM_IO_COMPRESS_HPP
