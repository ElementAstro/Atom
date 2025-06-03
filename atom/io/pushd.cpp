#include "pushd.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <shared_mutex>
#include <stack>

#include <spdlog/spdlog.h>

#ifdef ATOM_USE_BOOST
#include <boost/asio.hpp>
#include <boost/filesystem.hpp>
namespace fs = boost::filesystem;
namespace asio = boost::asio;
#else
namespace fs = std::filesystem;
#ifdef ATOM_USE_ASIO
#include <asio.hpp>
#endif
#endif

#include "atom/containers/high_performance.hpp"

using atom::containers::String;
using atom::containers::Vector;

namespace atom::io {

namespace {
[[nodiscard]] bool isValidPath(const fs::path& path) noexcept {
    try {
        if (path.empty())
            return false;

        std::error_code ec;
        [[maybe_unused]] auto canonical_path = fs::weakly_canonical(path, ec);
        return !ec;
    } catch (const std::exception&) {
        return false;
    }
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

DirectoryStack::DirectoryStack(
#if defined(ATOM_USE_BOOST) || defined(ATOM_USE_ASIO)
    asio::io_context& io_context
#else
    void* io_context
#endif
    )
    : impl_(std::make_unique<DirectoryStackImpl>(
#if defined(ATOM_USE_BOOST) || defined(ATOM_USE_ASIO)
          io_context
#else
          io_context
#endif
          )) {
#if defined(ATOM_USE_BOOST) || defined(ATOM_USE_ASIO)
    executor_ = io_context.get_executor();
#endif
}

DirectoryStack::~DirectoryStack() noexcept = default;
DirectoryStack::DirectoryStack(DirectoryStack&& other) noexcept = default;
auto DirectoryStack::operator=(DirectoryStack&& other) noexcept
    -> DirectoryStack& = default;

template <PathLike P>
void DirectoryStack::asyncPushd(
    const P& new_dir,
    const std::function<void(const std::error_code&)>& handler) {
    impl_->asyncPushd<P>(new_dir, handler);
}

template <PathLike P>
auto DirectoryStack::pushd(const P& new_dir_param) -> Task<void> {
    fs::path new_dir = new_dir_param;
    co_await std::suspend_never{};

    std::error_code ec;
    try {
        if (!isValidPath(new_dir)) {
            spdlog::error("pushd: Invalid path provided - {}",
                          new_dir.string());
            throw fs::filesystem_error(
                "Invalid path provided", new_dir,
                std::make_error_code(std::errc::invalid_argument));
        }

        auto currentPath = fs::current_path(ec);
        if (ec) {
            spdlog::error("pushd: Failed to get current path. Error: {}",
                          ec.message());
            throw fs::filesystem_error("Failed to get current path", ec);
        }

        {
            std::unique_lock lock(impl_->stackMutex_);
            impl_->dirStack_.push(currentPath);
        }

        fs::current_path(new_dir, ec);
        if (ec) {
            spdlog::warn(
                "pushd: Failed to change directory to {}, "
                "rolling back stack push. Error: {}",
                new_dir.string(), ec.message());
            std::unique_lock lock(impl_->stackMutex_);
            if (!impl_->dirStack_.empty() &&
                impl_->dirStack_.top() == currentPath) {
                impl_->dirStack_.pop();
            }
            throw fs::filesystem_error("Failed to change directory", new_dir,
                                       ec);
        }
        spdlog::info("pushd successful to {}", new_dir.string());
    } catch (const fs::filesystem_error&) {
        throw;
    } catch (const std::exception& e) {
        spdlog::error("Generic exception in pushd: {}", e.what());
        throw;
    }

    co_return;
}

void DirectoryStack::asyncPopd(
    const std::function<void(const std::error_code&)>& handler) {
    impl_->asyncPopd(handler);
}

auto DirectoryStack::popd() -> Task<void> {
    co_await std::suspend_never{};

    std::error_code ec;
    try {
        fs::path prevDir;

        {
            std::unique_lock lock(impl_->stackMutex_);
            if (impl_->dirStack_.empty()) {
                spdlog::warn("popd: Directory stack is empty");
                throw fs::filesystem_error(
                    "Directory stack is empty",
                    std::make_error_code(std::errc::operation_not_permitted));
            }
            prevDir = impl_->dirStack_.top();
            impl_->dirStack_.pop();
        }

        if (!isValidPath(prevDir)) {
            spdlog::error("popd: Invalid path found in stack - {}",
                          prevDir.string());
            throw fs::filesystem_error(
                "Invalid path in stack", prevDir,
                std::make_error_code(std::errc::invalid_argument));
        }

        fs::current_path(prevDir, ec);
        if (ec) {
            spdlog::error("popd: Failed to change directory to {}. Error: {}",
                          prevDir.string(), ec.message());
            throw fs::filesystem_error("Failed to change directory", prevDir,
                                       ec);
        }
        spdlog::info("popd successful to {}", prevDir.string());
    } catch (const fs::filesystem_error&) {
        throw;
    } catch (const std::exception& e) {
        spdlog::error("Generic exception in popd: {}", e.what());
        throw;
    }

    co_return;
}

auto DirectoryStack::peek() const -> fs::path {
    std::shared_lock lock(impl_->stackMutex_);
    if (impl_->dirStack_.empty()) {
        throw std::runtime_error("Directory stack is empty");
    }
    return impl_->dirStack_.top();
}

auto DirectoryStack::dirs() const noexcept -> Vector<fs::path> {
    Vector<fs::path> contents = impl_->getStackContents();
    std::reverse(contents.begin(), contents.end());
    return contents;
}

void DirectoryStack::clear() noexcept {
    std::unique_lock lock(impl_->stackMutex_);
    std::stack<fs::path>().swap(impl_->dirStack_);
}

void DirectoryStack::swap(size_t index1, size_t index2) {
    std::unique_lock lock(impl_->stackMutex_);
    std::stack<fs::path> tempStack = impl_->dirStack_;
    Vector<fs::path> contents;
    contents.reserve(tempStack.size());
    while (!tempStack.empty()) {
        contents.push_back(tempStack.top());
        tempStack.pop();
    }
    std::reverse(contents.begin(), contents.end());

    size_t size = contents.size();
    if (index1 >= size || index2 >= size) {
        spdlog::warn("swap: Index out of bounds");
        throw std::out_of_range("Index out of bounds for directory stack swap");
    }

    size_t vec_index1 = size - 1 - index1;
    size_t vec_index2 = size - 1 - index2;

    std::swap(contents[vec_index1], contents[vec_index2]);

    std::stack<fs::path> newStack;
    for (const auto& path : contents) {
        newStack.push(path);
    }
    impl_->dirStack_ = std::move(newStack);
}

void DirectoryStack::remove(size_t index) {
    std::unique_lock lock(impl_->stackMutex_);
    std::stack<fs::path> tempStack = impl_->dirStack_;
    Vector<fs::path> contents;
    contents.reserve(tempStack.size());
    while (!tempStack.empty()) {
        contents.push_back(tempStack.top());
        tempStack.pop();
    }
    std::reverse(contents.begin(), contents.end());

    size_t size = contents.size();
    if (index >= size) {
        spdlog::warn("remove: Index out of bounds");
        throw std::out_of_range(
            "Index out of bounds for directory stack remove");
    }

    size_t vec_index = size - 1 - index;
    contents.erase(contents.begin() +
                   static_cast<Vector<fs::path>::difference_type>(vec_index));

    std::stack<fs::path> newStack;
    for (const auto& path : contents) {
        newStack.push(path);
    }
    impl_->dirStack_ = std::move(newStack);
}

void DirectoryStack::asyncGotoIndex(
    size_t index, const std::function<void(const std::error_code&)>& handler) {
    impl_->asyncGotoIndex(index, handler);
}

auto DirectoryStack::gotoIndex(size_t index) -> Task<void> {
    co_await std::suspend_never{};

    std::error_code ec;
    try {
        Vector<fs::path> contents;
        {
            std::shared_lock lock(impl_->stackMutex_);
            std::stack<fs::path> tempStack = impl_->dirStack_;
            contents.reserve(tempStack.size());
            while (!tempStack.empty()) {
                contents.push_back(tempStack.top());
                tempStack.pop();
            }
            std::reverse(contents.begin(), contents.end());
        }

        size_t size = contents.size();
        if (index >= size) {
            spdlog::warn("gotoIndex: Index out of bounds");
            throw fs::filesystem_error(
                "Index out of bounds",
                std::make_error_code(std::errc::invalid_argument));
        }

        size_t vec_index = size - 1 - index;
        const fs::path& targetPath = contents[vec_index];

        if (!isValidPath(targetPath)) {
            spdlog::error("gotoIndex: Invalid path in stack at index {}",
                          index);
            throw fs::filesystem_error(
                "Invalid path in stack", targetPath,
                std::make_error_code(std::errc::invalid_argument));
        }

        fs::current_path(targetPath, ec);
        if (ec) {
            spdlog::error("gotoIndex: Failed to change directory. Error: {}",
                          ec.message());
            throw fs::filesystem_error("Failed to change directory", targetPath,
                                       ec);
        }
    } catch (const fs::filesystem_error&) {
        throw;
    } catch (const std::exception& e) {
        spdlog::error("Generic exception in gotoIndex: {}", e.what());
        throw;
    }
    co_return;
}

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

auto DirectoryStack::size() const noexcept -> size_t {
    std::shared_lock lock(impl_->stackMutex_);
    return impl_->dirStack_.size();
}

auto DirectoryStack::isEmpty() const noexcept -> bool {
    std::shared_lock lock(impl_->stackMutex_);
    return impl_->dirStack_.empty();
}

void DirectoryStack::asyncGetCurrentDirectory(
    const std::function<void(const std::filesystem::path&,
                             const std::error_code&)>& handler) const {
    impl_->asyncGetCurrentDirectory(handler);
}

auto DirectoryStack::getCurrentDirectory() const -> Task<fs::path> {
    co_await std::suspend_never{};

    std::error_code ec;
    fs::path currentPath;
    try {
        currentPath = fs::current_path(ec);
        if (ec) {
            spdlog::error(
                "getCurrentDirectory: Failed to get current path. Error: {}",
                ec.message());
            throw fs::filesystem_error("Failed to get current path", ec);
        }
    } catch (const fs::filesystem_error&) {
        throw;
    } catch (const std::exception& e) {
        spdlog::error("Generic exception in getCurrentDirectory: {}", e.what());
        throw;
    }
    co_return currentPath;
}

template void DirectoryStack::asyncPushd<fs::path>(
    const fs::path&, const std::function<void(const std::error_code&)>&);
template void DirectoryStack::asyncPushd<std::string>(
    const std::string&, const std::function<void(const std::error_code&)>&);

template auto DirectoryStack::pushd<fs::path>(const fs::path&) -> Task<void>;
template auto DirectoryStack::pushd<std::string>(const std::string&)
    -> Task<void>;

}  // namespace atom::io
