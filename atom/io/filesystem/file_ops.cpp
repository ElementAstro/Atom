#include "file_ops.hpp"

#include <filesystem>
#include <iostream>
#include <stdexcept>

#include <spdlog/spdlog.h>

namespace atom::io {

void printFileInfo(const FileInfo& info) {
    try {
        spdlog::debug("Printing file info for: {}", info.filePath);

        std::cout << "File Path: " << info.filePath << std::endl;
        std::cout << "File Name: " << info.fileName << std::endl;
        std::cout << "Extension: " << info.extension << std::endl;
        std::cout << "File Size: " << info.fileSize << " bytes" << std::endl;
        std::cout << "File Type: " << info.fileType << std::endl;
        std::cout << "Creation Time: " << info.creationTime << std::endl;
        std::cout << "Last Modified Time: " << info.lastModifiedTime
                  << std::endl;
        std::cout << "Last Access Time: " << info.lastAccessTime << std::endl;
        std::cout << "Permissions: " << info.permissions << std::endl;
        std::cout << "Is Hidden: " << (info.isHidden ? "Yes" : "No")
                  << std::endl;

#ifdef _WIN32
        std::cout << "Owner: " << info.owner << std::endl;
#else
        std::cout << "Owner: " << info.owner << std::endl;
        std::cout << "Group: " << info.group << std::endl;
        if (!info.symlinkTarget.empty()) {
            std::cout << "Symlink Target: " << info.symlinkTarget << std::endl;
        }
#endif
    } catch (const std::exception& ex) {
        spdlog::error("printFileInfo encountered an error: {}", ex.what());
        std::cerr << "printFileInfo encountered an error: " << ex.what()
                  << std::endl;
    }
}

void deleteFile(const fs::path& filePath) {
    try {
        if (!fs::exists(filePath)) {
            spdlog::error("File does not exist: {}", filePath.string());
            throw std::runtime_error("File does not exist: " +
                                     filePath.string());
        }

        if (!fs::remove(filePath)) {
            spdlog::error("Failed to delete file: {}", filePath.string());
            throw std::runtime_error("Failed to delete file: " +
                                     filePath.string());
        }

        spdlog::info("Successfully deleted file: {}", filePath.string());
    } catch (const fs::filesystem_error& e) {
        spdlog::error("Filesystem error deleting file {}: {}",
                      filePath.string(), e.what());
        throw std::runtime_error("Failed to delete file '" + filePath.string() +
                                 "': " + e.what());
    } catch (...) {
        throw;  // Re-throw any existing runtime_error or other exceptions
    }
}

}  // namespace atom::io
