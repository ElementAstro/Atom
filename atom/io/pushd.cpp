#include "pushd.hpp"

#ifdef ATOM_USE_BOOST
#include <boost/asio.hpp>
#include <boost/filesystem.hpp>
#include <boost/log/trivial.hpp>
namespace fs = boost::filesystem;
namespace asio = boost::asio;
#else
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <shared_mutex>
#include <stack>
#include <thread>
#include "atom/log/loguru.hpp"
namespace fs = std::filesystem;
#endif

namespace atom::io {

namespace {
// Helper function for path validation
[[nodiscard]] bool isValidPath(const fs::path& path) noexcept {
    try {
        if (path.empty())
            return false;

        // Check for basic format validity
        if (!path.has_filename() && !path.has_parent_path())
            return false;

        // Attempt to make canonical path
        // This catches many issues like invalid characters on Windows
        (void)fs::weakly_canonical(path);

        return true;
    } catch (const std::exception&) {
        return false;
    }
}
}  // namespace

class DirectoryStackImpl {
public:
    explicit DirectoryStackImpl(asio::io_context& io_context)
        : strand_(io_context) {}

    // Thread-safe access to directory stack
    mutable std::shared_mutex stackMutex_;

    void asyncPushd(
        const fs::path& new_dir,
        const std::function<void(const std::error_code&)>& handler) {
#ifdef ATOM_USE_BOOST
        BOOST_LOG_TRIVIAL(info)
            << "asyncPushd called with new_dir: " << new_dir.string();
#else
        LOG_F(INFO, "asyncPushd called with new_dir: %s",
              new_dir.string().c_str());
#endif
        // Validate path before proceeding
        if (!isValidPath(new_dir)) {
            std::error_code ec =
                std::make_error_code(std::errc::invalid_argument);
            handler(ec);
            return;
        }

        asio::post(strand_, [this, new_dir, handler]() {
            try {
                std::error_code errorCode;
                fs::path currentDir = fs::current_path(errorCode);

                if (!errorCode) {
                    {
                        std::unique_lock lock(stackMutex_);
                        dirStack_.push(currentDir);
                    }
                    fs::current_path(new_dir, errorCode);
                }

#ifdef ATOM_USE_BOOST
                BOOST_LOG_TRIVIAL(info)
                    << "asyncPushd completed with error code: "
                    << errorCode.value();
#else
                LOG_F(INFO, "asyncPushd completed with error code: %d", errorCode.value());
#endif
                handler(errorCode);
            } catch (const std::exception& e) {
                std::error_code errorCode =
                    std::make_error_code(std::errc::io_error);
#ifdef ATOM_USE_BOOST
                BOOST_LOG_TRIVIAL(error)
                    << "Exception in asyncPushd: " << e.what();
#else
                LOG_F(ERROR, "Exception in asyncPushd: %s", e.what());
#endif
                handler(errorCode);
            }
        });
    }

    void asyncPopd(const std::function<void(const std::error_code&)>& handler) {
#ifdef ATOM_USE_BOOST
        BOOST_LOG_TRIVIAL(info) << "asyncPopd called";
#else
        LOG_F(INFO, "asyncPopd called");
#endif
        asio::post(strand_, [this, handler]() {
            try {
                std::error_code errorCode;
                fs::path prevDir;

                {
                    std::unique_lock lock(stackMutex_);
                    if (!dirStack_.empty()) {
                        prevDir = dirStack_.top();
                        dirStack_.pop();
                    } else {
                        errorCode =
                            std::make_error_code(std::errc::invalid_argument);
                        handler(errorCode);
                        return;
                    }
                }

                // Validate the path before changing to it
                if (!isValidPath(prevDir)) {
                    errorCode =
                        std::make_error_code(std::errc::invalid_argument);
                    handler(errorCode);
                    return;
                }

                fs::current_path(prevDir, errorCode);

#ifdef ATOM_USE_BOOST
                BOOST_LOG_TRIVIAL(info)
                    << "asyncPopd completed with error code: "
                    << errorCode.value();
#else
                LOG_F(INFO, "asyncPopd completed with error code: %d", errorCode.value());
#endif
                handler(errorCode);
            } catch (const std::exception& e) {
                std::error_code errorCode =
                    std::make_error_code(std::errc::io_error);
#ifdef ATOM_USE_BOOST
                BOOST_LOG_TRIVIAL(error)
                    << "Exception in asyncPopd: " << e.what();
#else
                LOG_F(ERROR, "Exception in asyncPopd: %s", e.what());
#endif
                handler(errorCode);
            }
        });
    }

    [[nodiscard]] auto getStackContents() const -> std::vector<fs::path> {
        std::shared_lock lock(stackMutex_);
        std::stack<fs::path> tempStack = dirStack_;
        std::vector<fs::path> contents;
        contents.reserve(tempStack.size());  // Pre-allocate memory

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
#ifdef ATOM_USE_BOOST
        BOOST_LOG_TRIVIAL(info)
            << "asyncGotoIndex called with index: " << index;
#else
        LOG_F(INFO, "asyncGotoIndex called with index: %zu", index);
#endif
        asio::post(strand_, [this, index, handler]() {
            try {
                std::error_code errorCode;
                auto contents = getStackContents();

                if (index < contents.size()) {
                    const fs::path& targetPath = contents[index];

                    // Validate the path before changing to it
                    if (!isValidPath(targetPath)) {
                        errorCode =
                            std::make_error_code(std::errc::invalid_argument);
                        handler(errorCode);
                        return;
                    }

                    fs::current_path(targetPath, errorCode);
                } else {
                    errorCode =
                        std::make_error_code(std::errc::invalid_argument);
                }

#ifdef ATOM_USE_BOOST
                BOOST_LOG_TRIVIAL(info)
                    << "asyncGotoIndex completed with error code: "
                    << errorCode.value();
#else
                LOG_F(INFO, "asyncGotoIndex completed with error code: %d", errorCode.value());
#endif
                handler(errorCode);
            } catch (const std::exception& e) {
                std::error_code errorCode =
                    std::make_error_code(std::errc::io_error);
#ifdef ATOM_USE_BOOST
                BOOST_LOG_TRIVIAL(error)
                    << "Exception in asyncGotoIndex: " << e.what();
#else
                LOG_F(ERROR, "Exception in asyncGotoIndex: %s", e.what());
#endif
                handler(errorCode);
            }
        });
    }

    void asyncSaveStackToFile(
        const std::string& filename,
        const std::function<void(const std::error_code&)>& handler) {
#ifdef ATOM_USE_BOOST
        BOOST_LOG_TRIVIAL(info)
            << "asyncSaveStackToFile called with filename: " << filename;
#else
        LOG_F(INFO, "asyncSaveStackToFile called with filename: %s",
              filename.c_str());
#endif
        // Validate filename
        if (filename.empty()) {
            std::error_code errorCode =
                std::make_error_code(std::errc::invalid_argument);
            handler(errorCode);
            return;
        }

        asio::post(strand_, [this, filename, handler]() {
            try {
                std::error_code errorCode;
                std::ofstream file(filename);

                if (file) {
                    auto contents = getStackContents();

                    // Parallel processing for large stacks
                    if (contents.size() > 100) {
                        std::mutex fileMutex;
                        std::vector<std::thread> threads;
                        const size_t numThreads =
                            std::min(std::thread::hardware_concurrency(),
                                     static_cast<unsigned>(4));

                        size_t chunkSize = contents.size() / numThreads;

                        for (size_t i = 0; i < numThreads; ++i) {
                            size_t start = i * chunkSize;
                            size_t end = (i == numThreads - 1)
                                             ? contents.size()
                                             : (i + 1) * chunkSize;

                            threads.emplace_back([&, start, end]() {
                                std::string buffer;
                                buffer.reserve(128 *
                                               (end - start));  // Estimate size

                                for (size_t j = start; j < end; ++j) {
                                    buffer += contents[j].string() + '\n';
                                }

                                std::unique_lock<std::mutex> lock(fileMutex);
                                file << buffer;
                            });
                        }

                        for (auto& t : threads) {
                            t.join();
                        }
                    } else {
                        // Sequential processing for small stacks
                        for (const auto& dir : contents) {
                            file << dir.string() << '\n';
                        }
                    }

                    if (!file) {
                        errorCode = std::make_error_code(std::errc::io_error);
                    }
                } else {
                    errorCode = std::make_error_code(std::errc::io_error);
                }

#ifdef ATOM_USE_BOOST
                BOOST_LOG_TRIVIAL(info)
                    << "asyncSaveStackToFile completed with error code: "
                    << errorCode.value();
#else
                LOG_F(INFO, "asyncSaveStackToFile completed with error code: %d", errorCode.value());
#endif
                handler(errorCode);
            } catch (const std::exception& e) {
                std::error_code errorCode =
                    std::make_error_code(std::errc::io_error);
#ifdef ATOM_USE_BOOST
                BOOST_LOG_TRIVIAL(error)
                    << "Exception in asyncSaveStackToFile: " << e.what();
#else
                LOG_F(ERROR, "Exception in asyncSaveStackToFile: %s", e.what());
#endif
                handler(errorCode);
            }
        });
    }

    void asyncLoadStackFromFile(
        const std::string& filename,
        const std::function<void(const std::error_code&)>& handler) {
#ifdef ATOM_USE_BOOST
        BOOST_LOG_TRIVIAL(info)
            << "asyncLoadStackFromFile called with filename: " << filename;
#else
        LOG_F(INFO, "asyncLoadStackFromFile called with filename: %s",
              filename.c_str());
#endif
        // Validate filename
        if (filename.empty() || !fs::exists(filename)) {
            std::error_code errorCode =
                std::make_error_code(std::errc::no_such_file_or_directory);
            handler(errorCode);
            return;
        }

        asio::post(strand_, [this, filename, handler]() {
            try {
                std::error_code errorCode;
                std::ifstream file(filename);

                if (file) {
                    // First read all lines to validate them
                    std::vector<std::string> lines;
                    std::string line;

                    while (std::getline(file, line)) {
                        lines.push_back(std::move(line));
                    }

                    // Validate all paths before modifying the stack
                    for (const auto& path : lines) {
                        fs::path testPath(path);
                        if (!isValidPath(testPath)) {
                            errorCode = std::make_error_code(
                                std::errc::invalid_argument);
                            handler(errorCode);
                            return;
                        }
                    }

                    std::stack<fs::path> newStack;
                    for (auto it = lines.rbegin(); it != lines.rend(); ++it) {
                        newStack.emplace(*it);
                    }

                    {
                        std::unique_lock lock(stackMutex_);
                        dirStack_ = std::move(newStack);
                    }
                } else {
                    errorCode = std::make_error_code(std::errc::io_error);
                }

#ifdef ATOM_USE_BOOST
                BOOST_LOG_TRIVIAL(info)
                    << "asyncLoadStackFromFile completed with error code: "
                    << errorCode.value();
#else
                LOG_F(INFO, "asyncLoadStackFromFile completed with error code: %d", errorCode.value());
#endif
                handler(errorCode);
            } catch (const std::exception& e) {
                std::error_code errorCode =
                    std::make_error_code(std::errc::io_error);
#ifdef ATOM_USE_BOOST
                BOOST_LOG_TRIVIAL(error)
                    << "Exception in asyncLoadStackFromFile: " << e.what();
#else
                LOG_F(ERROR, "Exception in asyncLoadStackFromFile: %s", e.what());
#endif
                handler(errorCode);
            }
        });
    }

    void asyncGetCurrentDirectory(
        const std::function<void(const fs::path&)>& handler) const {
#ifdef ATOM_USE_BOOST
        BOOST_LOG_TRIVIAL(info) << "asyncGetCurrentDirectory called";
#else
        LOG_F(INFO, "asyncGetCurrentDirectory called");
#endif
        asio::post(strand_, [handler]() {
            try {
                auto currentPath = fs::current_path();
#ifdef ATOM_USE_BOOST
                BOOST_LOG_TRIVIAL(info)
                    << "asyncGetCurrentDirectory completed with current path: "
                    << currentPath.string();
#else
                LOG_F(INFO, "asyncGetCurrentDirectory completed with current path: %s", 
                      currentPath.string().c_str());
#endif
                handler(currentPath);
            } catch (const std::exception& e) {
                // In case of error, return empty path
                fs::path emptyPath;
#ifdef ATOM_USE_BOOST
                BOOST_LOG_TRIVIAL(error)
                    << "Exception in asyncGetCurrentDirectory: " << e.what();
#else
                LOG_F(ERROR, "Exception in asyncGetCurrentDirectory: %s", e.what());
#endif
                handler(emptyPath);
            }
        });
    }

    std::stack<fs::path> dirStack_;
    asio::io_context::strand strand_;
};

// DirectoryStack public interface methods implementation

DirectoryStack::DirectoryStack(asio::io_context& io_context)
    : impl_(std::make_unique<DirectoryStackImpl>(io_context)) {}

DirectoryStack::~DirectoryStack() noexcept = default;

DirectoryStack::DirectoryStack(DirectoryStack&& other) noexcept = default;

auto DirectoryStack::operator=(DirectoryStack&& other) noexcept
    -> DirectoryStack& = default;

void DirectoryStack::asyncPushd(
    const fs::path& new_dir,
    const std::function<void(const std::error_code&)>& handler) {
    impl_->asyncPushd(new_dir, handler);
}

auto DirectoryStack::pushd(const fs::path& new_dir) -> Task<void> {
    std::error_code ec;
    co_await std::suspend_never{};

    try {
        if (!isValidPath(new_dir)) {
            throw std::invalid_argument("Invalid path provided");
        }

        auto currentPath = fs::current_path(ec);
        if (ec) {
            throw std::system_error(ec, "Failed to get current path");
        }

        {
            std::unique_lock lock(impl_->stackMutex_);
            impl_->dirStack_.push(currentPath);
        }

        fs::current_path(new_dir, ec);
        if (ec) {
            // Rollback if changing directory failed
            std::unique_lock lock(impl_->stackMutex_);
            impl_->dirStack_.pop();
            throw std::system_error(ec, "Failed to change directory");
        }
    } catch (const std::exception& e) {
#ifdef ATOM_USE_BOOST
        BOOST_LOG_TRIVIAL(error) << "Exception in pushd: " << e.what();
#else
        LOG_F(ERROR, "Exception in pushd: %s", e.what());
#endif
        throw;  // Re-throw to be captured by coroutine mechanism
    }

    co_return;
}

void DirectoryStack::asyncPopd(
    const std::function<void(const std::error_code&)>& handler) {
    impl_->asyncPopd(handler);
}

auto DirectoryStack::popd() -> Task<void> {
    std::error_code ec;
    co_await std::suspend_never{};

    try {
        fs::path prevDir;

        {
            std::unique_lock lock(impl_->stackMutex_);
            if (impl_->dirStack_.empty()) {
                throw std::runtime_error("Directory stack is empty");
            }
            prevDir = impl_->dirStack_.top();
            impl_->dirStack_.pop();
        }

        // Validate the path before changing to it
        if (!isValidPath(prevDir)) {
            throw std::invalid_argument("Invalid path in stack");
        }

        fs::current_path(prevDir, ec);
        if (ec) {
            throw std::system_error(ec, "Failed to change directory");
        }
    } catch (const std::exception& e) {
#ifdef ATOM_USE_BOOST
        BOOST_LOG_TRIVIAL(error) << "Exception in popd: " << e.what();
#else
        LOG_F(ERROR, "Exception in popd: %s", e.what());
#endif
        throw;  // Re-throw to be captured by coroutine mechanism
    }

    co_return;
}

auto DirectoryStack::peek() const -> fs::path {
    std::shared_lock lock(impl_->stackMutex_);
    return impl_->dirStack_.empty() ? fs::path() : impl_->dirStack_.top();
}

auto DirectoryStack::dirs() const noexcept -> std::vector<fs::path> {
    return impl_->getStackContents();
}

void DirectoryStack::clear() noexcept {
    std::unique_lock lock(impl_->stackMutex_);
    impl_->dirStack_ = std::stack<fs::path>();
}

void DirectoryStack::swap(size_t index1, size_t index2) {
    auto contents = impl_->getStackContents();
    if (index1 < contents.size() && index2 < contents.size()) {
        std::swap(contents[index1], contents[index2]);
        std::unique_lock lock(impl_->stackMutex_);
        impl_->dirStack_ = std::stack<fs::path>(
            std::deque<fs::path>(contents.begin(), contents.end()));
    }
}

void DirectoryStack::remove(size_t index) {
    auto contents = impl_->getStackContents();
    if (index < contents.size()) {
        contents.erase(contents.begin() + static_cast<long>(index));
        std::unique_lock lock(impl_->stackMutex_);
        impl_->dirStack_ = std::stack<fs::path>(
            std::deque<fs::path>(contents.begin(), contents.end()));
    }
}

void DirectoryStack::asyncGotoIndex(
    size_t index, const std::function<void(const std::error_code&)>& handler) {
    impl_->asyncGotoIndex(index, handler);
}

void DirectoryStack::asyncSaveStackToFile(
    const std::string& filename,
    const std::function<void(const std::error_code&)>& handler) {
    impl_->asyncSaveStackToFile(filename, handler);
}

void DirectoryStack::asyncLoadStackFromFile(
    const std::string& filename,
    const std::function<void(const std::error_code&)>& handler) {
    impl_->asyncLoadStackFromFile(filename, handler);
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
    const std::function<void(const fs::path&)>& handler) const {
    impl_->asyncGetCurrentDirectory(handler);
}

}  // namespace atom::io