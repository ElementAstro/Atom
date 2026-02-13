#include "directory_stack_impl.hpp"

namespace atom::io {

void DirectoryStack::asyncSaveStackToFile(
    const String& filename,
    const std::function<void(const std::error_code&)>& handler) {
    impl_->asyncSaveStackToFile(filename, handler);
}

auto DirectoryStack::saveStackToFile(const String& filename) -> Task<void> {
    co_await std::suspend_never{};

    std::string filename_str = filename.c_str();

    try {
        if (filename_str.empty()) {
            spdlog::warn("saveStackToFile: Empty filename provided");
            throw fs::filesystem_error(
                "Empty filename provided",
                std::make_error_code(std::errc::invalid_argument));
        }

        std::ofstream file(filename_str);
        if (!file) {
            spdlog::error("saveStackToFile: Failed to open file {}",
                          filename_str);
            throw fs::filesystem_error(
                "Failed to open file", filename_str,
                std::make_error_code(std::errc::permission_denied));
        }

        Vector<fs::path> contents = impl_->getStackContents();
        for (const auto& dir : contents) {
            file << dir.string() << '\n';
        }

        if (!file.good()) {
            spdlog::error("saveStackToFile: IO error writing to file {}",
                          filename_str);
            throw fs::filesystem_error(
                "IO error writing to file", filename_str,
                std::make_error_code(std::errc::io_error));
        }

    } catch (const fs::filesystem_error&) {
        throw;
    } catch (const std::exception& e) {
        spdlog::error("Generic exception in saveStackToFile: {}", e.what());
        throw;
    }
    co_return;
}

void DirectoryStack::asyncLoadStackFromFile(
    const String& filename,
    const std::function<void(const std::error_code&)>& handler) {
    impl_->asyncLoadStackFromFile(filename, handler);
}

auto DirectoryStack::loadStackFromFile(const String& filename) -> Task<void> {
    co_await std::suspend_never{};

    std::string filename_str = filename.c_str();

    try {
        if (filename_str.empty()) {
            spdlog::warn("loadStackFromFile: Empty filename provided");
            throw fs::filesystem_error(
                "Empty filename provided",
                std::make_error_code(std::errc::invalid_argument));
        }

        std::error_code exists_ec;
        if (!fs::exists(filename_str, exists_ec) || exists_ec) {
            spdlog::warn(
                "loadStackFromFile: File not found or error checking "
                "existence: {}",
                filename_str);
            throw fs::filesystem_error(
                "File not found", filename_str,
                std::make_error_code(std::errc::no_such_file_or_directory));
        }

        std::ifstream file(filename_str);
        if (!file) {
            spdlog::error("loadStackFromFile: Failed to open file {}",
                          filename_str);
            throw fs::filesystem_error(
                "Failed to open file", filename_str,
                std::make_error_code(std::errc::permission_denied));
        }

        std::vector<fs::path> loadedPaths;
        std::string line;
        while (std::getline(file, line)) {
            if (line.empty())
                continue;

            fs::path currentPath(line);
            if (!isValidPath(currentPath)) {
                spdlog::error("loadStackFromFile: Invalid path in file: {}",
                              line);
                throw fs::filesystem_error(
                    "Invalid path in file", currentPath,
                    std::make_error_code(std::errc::invalid_argument));
            }
            loadedPaths.push_back(std::move(currentPath));
        }

        if (!file.eof() && file.fail()) {
            spdlog::error("loadStackFromFile: IO error reading file {}",
                          filename_str);
            throw fs::filesystem_error(
                "IO error reading file", filename_str,
                std::make_error_code(std::errc::io_error));
        }

        std::stack<fs::path> newStack;
        for (const auto& path : loadedPaths) {
            newStack.push(path);
        }

        {
            std::unique_lock lock(impl_->stackMutex_);
            impl_->dirStack_ = std::move(newStack);
        }

    } catch (const fs::filesystem_error&) {
        throw;
    } catch (const std::exception& e) {
        spdlog::error("Generic exception in loadStackFromFile: {}", e.what());
        throw;
    }
    co_return;
}

}  // namespace atom::io
