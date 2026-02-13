#ifndef ATOM_IO_FILESYSTEM_DIRECTORY_STACK_IMPL_HPP
#define ATOM_IO_FILESYSTEM_DIRECTORY_STACK_IMPL_HPP

#include "directory_stack.hpp"
#include "../core/path_utils.hpp"

#include <algorithm>
#include <fstream>
#include <mutex>
#include <shared_mutex>
#include <stack>

#include <spdlog/spdlog.h>

#ifdef ATOM_USE_BOOST
#include <boost/asio.hpp>
#include <boost/filesystem.hpp>
namespace asio = boost::asio;
#elif defined(ATOM_USE_ASIO)
#include <asio.hpp>
#endif

namespace atom::io {

namespace {
[[nodiscard]] bool isValidPath(const fs::path& path) noexcept {
    return detail::isValidPath(path);
}
}  // namespace

class DirectoryStackImpl {
public:
#if defined(ATOM_USE_BOOST) || defined(ATOM_USE_ASIO)
    explicit DirectoryStackImpl(asio::io_context& io_context)
        : strand_(asio::make_strand(io_context)) {}
#else
    explicit DirectoryStackImpl(void* = nullptr) {}
#endif

    mutable std::shared_mutex stackMutex_;
    std::stack<fs::path> dirStack_;
#if defined(ATOM_USE_BOOST) || defined(ATOM_USE_ASIO)
    asio::strand<asio::io_context::executor_type> strand_;
#endif

    template <PathLike P>
    void asyncPushd(
        const P& new_dir_param,
        const std::function<void(const std::error_code&)>& handler) {
        fs::path new_dir = new_dir_param;

        spdlog::info("asyncPushd called with new_dir: {}", new_dir.string());

        if (!isValidPath(new_dir)) {
            std::error_code ec =
                std::make_error_code(std::errc::invalid_argument);
            spdlog::warn("asyncPushd: Invalid path provided - {}",
                         new_dir.string());
#if defined(ATOM_USE_BOOST) || defined(ATOM_USE_ASIO)
            asio::post(strand_, [handler, ec]() { handler(ec); });
#else
            handler(ec);
#endif
            return;
        }

#if defined(ATOM_USE_BOOST) || defined(ATOM_USE_ASIO)
        asio::post(strand_, [this, new_dir, handler]() {
#endif
            try {
                std::error_code ec;
                fs::path currentDir = fs::current_path(ec);

                if (!ec) {
                    {
                        std::unique_lock lock(stackMutex_);
                        dirStack_.push(currentDir);
                    }
                    fs::current_path(new_dir, ec);
                    if (ec) {
                        spdlog::warn(
                            "asyncPushd: Failed to change directory to {}, "
                            "rolling back stack push. Error: {}",
                            new_dir.string(), ec.message());
                        std::unique_lock lock(stackMutex_);
                        if (!dirStack_.empty() &&
                            dirStack_.top() == currentDir) {
                            dirStack_.pop();
                        }
                    }
                } else {
                    spdlog::error(
                        "asyncPushd: Failed to get current path. Error: {}",
                        ec.message());
                }

                spdlog::info("asyncPushd completed with error code: {} ({})",
                             ec.value(), ec.message());
                handler(ec);
            } catch (const fs::filesystem_error& e) {
                std::error_code ec = e.code();
                spdlog::error("Filesystem exception in asyncPushd: {}",
                              e.what());
                handler(ec);
            } catch (const std::exception& e) {
                std::error_code ec = std::make_error_code(std::errc::io_error);
                spdlog::error("Generic exception in asyncPushd: {}", e.what());
                handler(ec);
            }
#if defined(ATOM_USE_BOOST) || defined(ATOM_USE_ASIO)
        });
#endif
    }

    void asyncPopd(const std::function<void(const std::error_code&)>& handler) {
        spdlog::info("asyncPopd called");

#if defined(ATOM_USE_BOOST) || defined(ATOM_USE_ASIO)
        asio::post(strand_, [this, handler]() {
#endif
            try {
                std::error_code ec;
                fs::path prevDir;

                {
                    std::unique_lock lock(stackMutex_);
                    if (!dirStack_.empty()) {
                        prevDir = dirStack_.top();
                        dirStack_.pop();
                    } else {
                        spdlog::warn("asyncPopd: Stack is empty");
                        ec = std::make_error_code(
                            std::errc::operation_not_permitted);
                        handler(ec);
                        return;
                    }
                }

                if (!isValidPath(prevDir)) {
                    spdlog::error("asyncPopd: Invalid path found in stack - {}",
                                  prevDir.string());
                    ec = std::make_error_code(std::errc::invalid_argument);
                    handler(ec);
                    return;
                }

                fs::current_path(prevDir, ec);
                if (ec) {
                    spdlog::error(
                        "asyncPopd: Failed to change directory to {}. Error: "
                        "{}",
                        prevDir.string(), ec.message());
                }

                spdlog::info("asyncPopd completed with error code: {} ({})",
                             ec.value(), ec.message());
                handler(ec);
            } catch (const fs::filesystem_error& e) {
                std::error_code ec = e.code();
                spdlog::error("Filesystem exception in asyncPopd: {}",
                              e.what());
                handler(ec);
            } catch (const std::exception& e) {
                std::error_code ec = std::make_error_code(std::errc::io_error);
                spdlog::error("Generic exception in asyncPopd: {}", e.what());
                handler(ec);
            }
#if defined(ATOM_USE_BOOST) || defined(ATOM_USE_ASIO)
        });
#endif
    }

    [[nodiscard]] auto getStackContents() const -> Vector<fs::path> {
        std::shared_lock lock(stackMutex_);
        std::stack<fs::path> tempStack = dirStack_;
        Vector<fs::path> contents;
        contents.reserve(tempStack.size());

        while (!tempStack.empty()) {
            contents.push_back(tempStack.top());
            tempStack.pop();
        }

        std::reverse(contents.begin(), contents.end());
        return contents;
    }

    void asyncGotoIndex(
        size_t index,
        const std::function<void(const std::error_code&)>& handler) {
        spdlog::info("asyncGotoIndex called with index: {}", index);

#if defined(ATOM_USE_BOOST) || defined(ATOM_USE_ASIO)
        asio::post(strand_, [this, index, handler]() {
#endif
            try {
                std::error_code ec;
                Vector<fs::path> contents;
                {
                    std::shared_lock lock(stackMutex_);
                    std::stack<fs::path> tempStack = dirStack_;
                    contents.reserve(tempStack.size());
                    while (!tempStack.empty()) {
                        contents.push_back(tempStack.top());
                        tempStack.pop();
                    }
                    std::reverse(contents.begin(), contents.end());
                }

                if (index < contents.size()) {
                    size_t effective_index = contents.size() - 1 - index;
                    const fs::path& targetPath = contents[effective_index];

                    if (!isValidPath(targetPath)) {
                        spdlog::error(
                            "asyncGotoIndex: Invalid path found in stack at "
                            "index {} - {}",
                            index, targetPath.string());
                        ec = std::make_error_code(std::errc::invalid_argument);
                        handler(ec);
                        return;
                    }

                    fs::current_path(targetPath, ec);
                    if (ec) {
                        spdlog::error(
                            "asyncGotoIndex: Failed to change directory to {}. "
                            "Error: {}",
                            targetPath.string(), ec.message());
                    }
                } else {
                    spdlog::warn(
                        "asyncGotoIndex: Index {} out of bounds (stack size "
                        "{})",
                        index, contents.size());
                    ec = std::make_error_code(std::errc::invalid_argument);
                }

                spdlog::info(
                    "asyncGotoIndex completed with error code: {} ({})",
                    ec.value(), ec.message());
                handler(ec);
            } catch (const fs::filesystem_error& e) {
                std::error_code ec = e.code();
                spdlog::error("Filesystem exception in asyncGotoIndex: {}",
                              e.what());
                handler(ec);
            } catch (const std::exception& e) {
                std::error_code ec = std::make_error_code(std::errc::io_error);
                spdlog::error("Generic exception in asyncGotoIndex: {}",
                              e.what());
                handler(ec);
            }
#if defined(ATOM_USE_BOOST) || defined(ATOM_USE_ASIO)
        });
#endif
    }

    void asyncSaveStackToFile(
        const String& filename,
        const std::function<void(const std::error_code&)>& handler) {
        spdlog::info("asyncSaveStackToFile called with filename: {}",
                     filename.c_str());

        if (filename.empty()) {
            spdlog::warn("asyncSaveStackToFile: Empty filename provided");
            std::error_code ec =
                std::make_error_code(std::errc::invalid_argument);
#if defined(ATOM_USE_BOOST) || defined(ATOM_USE_ASIO)
            asio::post(strand_, [handler, ec]() { handler(ec); });
#else
            handler(ec);
#endif
            return;
        }

        std::string filename_str = filename.c_str();

#if defined(ATOM_USE_BOOST) || defined(ATOM_USE_ASIO)
        asio::post(strand_, [this, filename_str, handler]() {
#endif
            try {
                std::error_code ec;
                std::ofstream file(filename_str);

                if (file) {
                    Vector<fs::path> contents = getStackContents();
                    for (const auto& dir : contents) {
                        file << dir.string() << '\n';
                    }

                    if (!file.good()) {
                        ec = std::make_error_code(std::errc::io_error);
                        spdlog::error(
                            "asyncSaveStackToFile: IO error while writing to "
                            "file {}",
                            filename_str);
                    }
                } else {
                    ec = std::make_error_code(std::errc::permission_denied);
                    spdlog::error(
                        "asyncSaveStackToFile: Failed to open file {} for "
                        "writing",
                        filename_str);
                }

                spdlog::info(
                    "asyncSaveStackToFile completed with error code: {} ({})",
                    ec.value(), ec.message());
                handler(ec);
            } catch (const std::exception& e) {
                std::error_code ec = std::make_error_code(std::errc::io_error);
                spdlog::error("Exception in asyncSaveStackToFile: {}",
                              e.what());
                handler(ec);
            }
#if defined(ATOM_USE_BOOST) || defined(ATOM_USE_ASIO)
        });
#endif
    }

    void asyncLoadStackFromFile(
        const String& filename,
        const std::function<void(const std::error_code&)>& handler) {
        spdlog::info("asyncLoadStackFromFile called with filename: {}",
                     filename.c_str());

        std::string filename_str = filename.c_str();

        if (filename_str.empty()) {
            spdlog::warn("asyncLoadStackFromFile: Empty filename provided");
            std::error_code ec =
                std::make_error_code(std::errc::invalid_argument);
#if defined(ATOM_USE_BOOST) || defined(ATOM_USE_ASIO)
            asio::post(strand_, [handler, ec]() { handler(ec); });
#else
            handler(ec);
#endif
            return;
        }

        std::error_code exists_ec;
        if (!fs::exists(filename_str, exists_ec) || exists_ec) {
            spdlog::warn(
                "asyncLoadStackFromFile: File not found or error checking "
                "existence: {}",
                filename_str);
            std::error_code ec =
                std::make_error_code(std::errc::no_such_file_or_directory);
#if defined(ATOM_USE_BOOST) || defined(ATOM_USE_ASIO)
            asio::post(strand_, [handler, ec]() { handler(ec); });
#else
            handler(ec);
#endif
            return;
        }

#if defined(ATOM_USE_BOOST) || defined(ATOM_USE_ASIO)
        asio::post(strand_, [this, filename_str, handler]() {
#endif
            try {
                std::error_code ec;
                std::ifstream file(filename_str);

                if (file) {
                    std::vector<fs::path> loadedPaths;
                    std::string line;

                    while (std::getline(file, line)) {
                        if (line.empty())
                            continue;

                        fs::path currentPath(line);
                        if (!isValidPath(currentPath)) {
                            spdlog::error(
                                "asyncLoadStackFromFile: Invalid path found in "
                                "file {} - {}",
                                filename_str, line);
                            ec = std::make_error_code(
                                std::errc::invalid_argument);
                            handler(ec);
                            return;
                        }
                        loadedPaths.push_back(std::move(currentPath));
                    }

                    if (!file.eof() && file.fail()) {
                        ec = std::make_error_code(std::errc::io_error);
                        spdlog::error(
                            "asyncLoadStackFromFile: IO error while reading "
                            "file {}",
                            filename_str);
                        handler(ec);
                        return;
                    }

                    std::stack<fs::path> newStack;
                    for (const auto& path : loadedPaths) {
                        newStack.push(path);
                    }

                    {
                        std::unique_lock lock(stackMutex_);
                        dirStack_ = std::move(newStack);
                    }
                } else {
                    ec = std::make_error_code(std::errc::permission_denied);
                    spdlog::error(
                        "asyncLoadStackFromFile: Failed to open file {} for "
                        "reading",
                        filename_str);
                }

                spdlog::info(
                    "asyncLoadStackFromFile completed with error code: {} ({})",
                    ec.value(), ec.message());
                handler(ec);
            } catch (const std::exception& e) {
                std::error_code ec = std::make_error_code(std::errc::io_error);
                spdlog::error("Exception in asyncLoadStackFromFile: {}",
                              e.what());
                handler(ec);
            }
#if defined(ATOM_USE_BOOST) || defined(ATOM_USE_ASIO)
        });
#endif
    }

    void asyncGetCurrentDirectory(
        const std::function<void(const fs::path&, const std::error_code&)>&
            handler) const {
        spdlog::info("asyncGetCurrentDirectory called");

#if defined(ATOM_USE_BOOST) || defined(ATOM_USE_ASIO)
        asio::post(strand_, [handler]() {
#endif
            std::error_code ec;
            fs::path currentPath;
            try {
                currentPath = fs::current_path(ec);
                if (ec) {
                    spdlog::error(
                        "asyncGetCurrentDirectory: Failed to get current path. "
                        "Error: {}",
                        ec.message());
                } else {
                    spdlog::info(
                        "asyncGetCurrentDirectory completed with current path: "
                        "{}",
                        currentPath.string());
                }
                handler(currentPath, ec);
            } catch (const fs::filesystem_error& e) {
                ec = e.code();
                spdlog::error(
                    "Filesystem exception in asyncGetCurrentDirectory: {}",
                    e.what());
                handler(fs::path(), ec);
            } catch (const std::exception& e) {
                ec = std::make_error_code(std::errc::io_error);
                spdlog::error(
                    "Generic exception in asyncGetCurrentDirectory: {}",
                    e.what());
                handler(fs::path(), ec);
            }
#if defined(ATOM_USE_BOOST) || defined(ATOM_USE_ASIO)
        });
#endif
    }
};

}  // namespace atom::io

#endif  // ATOM_IO_FILESYSTEM_DIRECTORY_STACK_IMPL_HPP
