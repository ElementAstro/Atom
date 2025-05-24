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

#include "atom/log/loguru.hpp"
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
        if (ec) {
            return false;
        }

        return true;
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
    explicit DirectoryStackImpl(void* io_context) {}
#endif

    mutable std::shared_mutex stackMutex_;

    template <PathLike P>
    void asyncPushd(
        const P& new_dir_param,
        const std::function<void(const std::error_code&)>& handler) {
        fs::path new_dir = new_dir_param;

#ifdef ATOM_USE_BOOST
        BOOST_LOG_TRIVIAL(info)
            << "asyncPushd called with new_dir: " << new_dir.string();
#else
        LOG_F(INFO, "asyncPushd called with new_dir: %s",
              new_dir.string().c_str());
#endif

        if (!isValidPath(new_dir)) {
            std::error_code ec =
                std::make_error_code(std::errc::invalid_argument);
#ifdef ATOM_USE_BOOST
            BOOST_LOG_TRIVIAL(warning)
                << "asyncPushd: Invalid path provided - " << new_dir.string();
#else
            LOG_F(WARNING, "asyncPushd: Invalid path provided - %s",
                  new_dir.string().c_str());
#endif
#if defined(ATOM_USE_BOOST) || defined(ATOM_USE_ASIO)
            asio::post(strand_, [handler, ec]() { handler(ec); });
#else
            handler(ec);
#endif
            return;
        }

#if defined(ATOM_USE_BOOST) || defined(ATOM_USE_ASIO)
        asio::post(strand_, [this, new_dir, handler]() {
#else
        // Execute synchronously when asio is not available
        {
#endif
            try {
                std::error_code errorCode;
                fs::path currentDir = fs::current_path(errorCode);

                if (!errorCode) {
                    {
                        std::unique_lock lock(stackMutex_);
                        dirStack_.push(currentDir);
                    }
                    fs::current_path(new_dir, errorCode);
                    if (errorCode) {
#ifdef ATOM_USE_BOOST
                        BOOST_LOG_TRIVIAL(warning)
                            << "asyncPushd: Failed to change directory to "
                            << new_dir.string()
                            << ", rolling back stack push. Error: "
                            << errorCode.message();
#else
                        LOG_F(WARNING,
                              "asyncPushd: Failed to change directory to %s, "
                              "rolling back stack push. Error: %s",
                              new_dir.string().c_str(),
                              errorCode.message().c_str());
#endif
                        std::unique_lock lock(stackMutex_);
                        if (!dirStack_.empty() &&
                            dirStack_.top() == currentDir) {
                            dirStack_.pop();
                        }
                    }
                } else {
#ifdef ATOM_USE_BOOST
                    BOOST_LOG_TRIVIAL(error)
                        << "asyncPushd: Failed to get current path. Error: "
                        << errorCode.message();
#else
                    LOG_F(ERROR,
                          "asyncPushd: Failed to get current path. Error: %s",
                          errorCode.message().c_str());
#endif
                }

#ifdef ATOM_USE_BOOST
                BOOST_LOG_TRIVIAL(info)
                    << "asyncPushd completed with error code: "
                    << errorCode.value() << " (" << errorCode.message() << ")";
#else
                LOG_F(INFO, "asyncPushd completed with error code: %d (%s)",
                      errorCode.value(), errorCode.message().c_str());
#endif
                handler(errorCode);
            } catch (const fs::filesystem_error& e) {
                std::error_code errorCode = e.code();
#ifdef ATOM_USE_BOOST
                BOOST_LOG_TRIVIAL(error)
                    << "Filesystem exception in asyncPushd: " << e.what();
#else
                LOG_F(ERROR, "Filesystem exception in asyncPushd: %s",
                      e.what());
#endif
                handler(errorCode);
            } catch (const std::exception& e) {
                std::error_code errorCode =
                    std::make_error_code(std::errc::io_error);
#ifdef ATOM_USE_BOOST
                BOOST_LOG_TRIVIAL(error)
                    << "Generic exception in asyncPushd: " << e.what();
#else
                LOG_F(ERROR, "Generic exception in asyncPushd: %s", e.what());
#endif
                handler(errorCode);
            }
#if defined(ATOM_USE_BOOST) || defined(ATOM_USE_ASIO)
        });
#else
        }
#endif
    }

    void asyncPopd(const std::function<void(const std::error_code&)>& handler) {
#ifdef ATOM_USE_BOOST
        BOOST_LOG_TRIVIAL(info) << "asyncPopd called";
#else
        LOG_F(INFO, "asyncPopd called");
#endif
#if defined(ATOM_USE_BOOST) || defined(ATOM_USE_ASIO)
        asio::post(strand_, [this, handler]() {
#else
        {
#endif
            try {
                std::error_code errorCode;
                fs::path prevDir;

                {
                    std::unique_lock lock(stackMutex_);
                    if (!dirStack_.empty()) {
                        prevDir = dirStack_.top();
                        dirStack_.pop();
                    } else {
#ifdef ATOM_USE_BOOST
                        BOOST_LOG_TRIVIAL(warning)
                            << "asyncPopd: Stack is empty.";
#else
                        LOG_F(WARNING, "asyncPopd: Stack is empty.");
#endif
                        errorCode = std::make_error_code(
                            std::errc::operation_not_permitted);
                        handler(errorCode);
                        return;
                    }
                }

                if (!isValidPath(prevDir)) {
#ifdef ATOM_USE_BOOST
                    BOOST_LOG_TRIVIAL(error)
                        << "asyncPopd: Invalid path found in stack - "
                        << prevDir.string();
#else
                    LOG_F(ERROR, "asyncPopd: Invalid path found in stack - %s",
                          prevDir.string().c_str());
#endif
                    errorCode =
                        std::make_error_code(std::errc::invalid_argument);
                    handler(errorCode);
                    return;
                }

                fs::current_path(prevDir, errorCode);
                if (errorCode) {
#ifdef ATOM_USE_BOOST
                    BOOST_LOG_TRIVIAL(error)
                        << "asyncPopd: Failed to change directory to "
                        << prevDir.string()
                        << ". Error: " << errorCode.message();
#else
                    LOG_F(ERROR,
                          "asyncPopd: Failed to change directory to %s. Error: "
                          "%s",
                          prevDir.string().c_str(),
                          errorCode.message().c_str());
#endif
                }

#ifdef ATOM_USE_BOOST
                BOOST_LOG_TRIVIAL(info)
                    << "asyncPopd completed with error code: "
                    << errorCode.value() << " (" << errorCode.message() << ")";
#else
                LOG_F(INFO, "asyncPopd completed with error code: %d (%s)",
                      errorCode.value(), errorCode.message().c_str());
#endif
                handler(errorCode);
            } catch (const fs::filesystem_error& e) {
                std::error_code errorCode = e.code();
#ifdef ATOM_USE_BOOST
                BOOST_LOG_TRIVIAL(error)
                    << "Filesystem exception in asyncPopd: " << e.what();
#else
                LOG_F(ERROR, "Filesystem exception in asyncPopd: %s", e.what());
#endif
                handler(errorCode);
            } catch (const std::exception& e) {
                std::error_code errorCode =
                    std::make_error_code(std::errc::io_error);
#ifdef ATOM_USE_BOOST
                BOOST_LOG_TRIVIAL(error)
                    << "Generic exception in asyncPopd: " << e.what();
#else
                LOG_F(ERROR, "Generic exception in asyncPopd: %s", e.what());
#endif
                handler(errorCode);
            }
#if defined(ATOM_USE_BOOST) || defined(ATOM_USE_ASIO)
        });
#else
        }
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
#ifdef ATOM_USE_BOOST
        BOOST_LOG_TRIVIAL(info)
            << "asyncGotoIndex called with index: " << index;
#else
        LOG_F(INFO, "asyncGotoIndex called with index: %zu", index);
#endif
#if defined(ATOM_USE_BOOST) || defined(ATOM_USE_ASIO)
        asio::post(strand_, [this, index, handler]() {
#else
        {
#endif
            try {
                std::error_code errorCode;
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
#ifdef ATOM_USE_BOOST
                        BOOST_LOG_TRIVIAL(error)
                            << "asyncGotoIndex: Invalid path found in stack at "
                               "index "
                            << index << " - " << targetPath.string();
#else
                        LOG_F(ERROR,
                              "asyncGotoIndex: Invalid path found in stack at "
                              "index %zu - %s",
                              index, targetPath.string().c_str());
#endif
                        errorCode =
                            std::make_error_code(std::errc::invalid_argument);
                        handler(errorCode);
                        return;
                    }

                    fs::current_path(targetPath, errorCode);
                    if (errorCode) {
#ifdef ATOM_USE_BOOST
                        BOOST_LOG_TRIVIAL(error)
                            << "asyncGotoIndex: Failed to change directory to "
                            << targetPath.string()
                            << ". Error: " << errorCode.message();
#else
                        LOG_F(ERROR,
                              "asyncGotoIndex: Failed to change directory to "
                              "%s. Error: %s",
                              targetPath.string().c_str(),
                              errorCode.message().c_str());
#endif
                    }
                } else {
#ifdef ATOM_USE_BOOST
                    BOOST_LOG_TRIVIAL(warning)
                        << "asyncGotoIndex: Index " << index
                        << " out of bounds (stack size " << contents.size()
                        << ").";
#else
                    LOG_F(WARNING,
                          "asyncGotoIndex: Index %zu out of bounds (stack size "
                          "%zu).",
                          index, contents.size());
#endif
                    errorCode =
                        std::make_error_code(std::errc::invalid_argument);
                }

#ifdef ATOM_USE_BOOST
                BOOST_LOG_TRIVIAL(info)
                    << "asyncGotoIndex completed with error code: "
                    << errorCode.value() << " (" << errorCode.message() << ")";
#else
                LOG_F(INFO, "asyncGotoIndex completed with error code: %d (%s)",
                      errorCode.value(), errorCode.message().c_str());
#endif
                handler(errorCode);
            } catch (const fs::filesystem_error& e) {
                std::error_code errorCode = e.code();
#ifdef ATOM_USE_BOOST
                BOOST_LOG_TRIVIAL(error)
                    << "Filesystem exception in asyncGotoIndex: " << e.what();
#else
                LOG_F(ERROR, "Filesystem exception in asyncGotoIndex: %s",
                      e.what());
#endif
                handler(errorCode);
            } catch (const std::exception& e) {
                std::error_code errorCode =
                    std::make_error_code(std::errc::io_error);
#ifdef ATOM_USE_BOOST
                BOOST_LOG_TRIVIAL(error)
                    << "Generic exception in asyncGotoIndex: " << e.what();
#else
                LOG_F(ERROR, "Generic exception in asyncGotoIndex: %s",
                      e.what());
#endif
                handler(errorCode);
            }
#if defined(ATOM_USE_BOOST) || defined(ATOM_USE_ASIO)
        });
#else
        }
#endif
    }

    void asyncSaveStackToFile(
        const String& filename,
        const std::function<void(const std::error_code&)>& handler) {
#ifdef ATOM_USE_BOOST
        BOOST_LOG_TRIVIAL(info) << "asyncSaveStackToFile called with filename: "
                                << filename.c_str();
#else
        LOG_F(INFO, "asyncSaveStackToFile called with filename: %s",
              filename.c_str());
#endif
        if (filename.empty()) {
#ifdef ATOM_USE_BOOST
            BOOST_LOG_TRIVIAL(warning)
                << "asyncSaveStackToFile: Empty filename provided.";
#else
            LOG_F(WARNING, "asyncSaveStackToFile: Empty filename provided.");
#endif
            std::error_code errorCode =
                std::make_error_code(std::errc::invalid_argument);
#if defined(ATOM_USE_BOOST) || defined(ATOM_USE_ASIO)
            asio::post(strand_, [handler, errorCode]() { handler(errorCode); });
#else
            handler(errorCode);
#endif
            return;
        }

        std::string filename_str = filename.c_str();

#if defined(ATOM_USE_BOOST) || defined(ATOM_USE_ASIO)
        asio::post(strand_, [this, filename_str, handler]() {
#else
        {
#endif
            try {
                std::error_code errorCode;
                std::ofstream file(filename_str);

                if (file) {
                    Vector<fs::path> contents = getStackContents();

                    for (const auto& dir : contents) {
                        file << dir.string() << '\n';
                    }

                    if (!file.good()) {
                        errorCode = std::make_error_code(std::errc::io_error);
#ifdef ATOM_USE_BOOST
                        BOOST_LOG_TRIVIAL(error)
                            << "asyncSaveStackToFile: IO error while writing "
                               "to file "
                            << filename_str;
#else
                        LOG_F(ERROR,
                              "asyncSaveStackToFile: IO error while writing to "
                              "file %s",
                              filename_str.c_str());
#endif
                    }
                } else {
                    errorCode =
                        std::make_error_code(std::errc::permission_denied);
#ifdef ATOM_USE_BOOST
                    BOOST_LOG_TRIVIAL(error)
                        << "asyncSaveStackToFile: Failed to open file "
                        << filename_str << " for writing.";
#else
                    LOG_F(ERROR,
                          "asyncSaveStackToFile: Failed to open file %s for "
                          "writing.",
                          filename_str.c_str());
#endif
                }

#ifdef ATOM_USE_BOOST
                BOOST_LOG_TRIVIAL(info)
                    << "asyncSaveStackToFile completed with error code: "
                    << errorCode.value() << " (" << errorCode.message() << ")";
#else
                LOG_F(INFO,
                      "asyncSaveStackToFile completed with error code: %d (%s)",
                      errorCode.value(), errorCode.message().c_str());
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
#if defined(ATOM_USE_BOOST) || defined(ATOM_USE_ASIO)
        });
#else
        }
#endif
    }

    void asyncLoadStackFromFile(
        const String& filename,
        const std::function<void(const std::error_code&)>& handler) {
#ifdef ATOM_USE_BOOST
        BOOST_LOG_TRIVIAL(info)
            << "asyncLoadStackFromFile called with filename: "
            << filename.c_str();
#else
        LOG_F(INFO, "asyncLoadStackFromFile called with filename: %s",
              filename.c_str());
#endif

        std::string filename_str = filename.c_str();

        if (filename_str.empty()) {
#ifdef ATOM_USE_BOOST
            BOOST_LOG_TRIVIAL(warning)
                << "asyncLoadStackFromFile: Empty filename provided.";
#else
            LOG_F(WARNING, "asyncLoadStackFromFile: Empty filename provided.");
#endif
            std::error_code errorCode =
                std::make_error_code(std::errc::invalid_argument);
#if defined(ATOM_USE_BOOST) || defined(ATOM_USE_ASIO)
            asio::post(strand_, [handler, errorCode]() { handler(errorCode); });
#else
            handler(errorCode);
#endif
            return;
        }
        std::error_code exists_ec;
        if (!fs::exists(filename_str, exists_ec) || exists_ec) {
#ifdef ATOM_USE_BOOST
            BOOST_LOG_TRIVIAL(warning) << "asyncLoadStackFromFile: File not "
                                          "found or error checking existence: "
                                       << filename_str;
#else
            LOG_F(WARNING,
                  "asyncLoadStackFromFile: File not found or error checking "
                  "existence: %s",
                  filename_str.c_str());
#endif
            std::error_code errorCode =
                std::make_error_code(std::errc::no_such_file_or_directory);
#if defined(ATOM_USE_BOOST) || defined(ATOM_USE_ASIO)
            asio::post(strand_, [handler, errorCode]() { handler(errorCode); });
#else
            handler(errorCode);
#endif
            return;
        }

#if defined(ATOM_USE_BOOST) || defined(ATOM_USE_ASIO)
        asio::post(strand_, [this, filename_str, handler]() {
#else
        {
#endif
            try {
                std::error_code errorCode;
                std::ifstream file(filename_str);

                if (file) {
                    std::vector<fs::path> loadedPaths;
                    std::string line;

                    while (std::getline(file, line)) {
                        if (line.empty())
                            continue;

                        fs::path currentPath(line);
                        if (!isValidPath(currentPath)) {
#ifdef ATOM_USE_BOOST
                            BOOST_LOG_TRIVIAL(error)
                                << "asyncLoadStackFromFile: Invalid path found "
                                   "in file "
                                << filename_str << " - " << line;
#else
                            LOG_F(ERROR,
                                  "asyncLoadStackFromFile: Invalid path found "
                                  "in file %s - %s",
                                  filename_str.c_str(), line.c_str());
#endif
                            errorCode = std::make_error_code(
                                std::errc::invalid_argument);
                            handler(errorCode);
                            return;
                        }
                        loadedPaths.push_back(std::move(currentPath));
                    }

                    if (!file.eof() && file.fail()) {
                        errorCode = std::make_error_code(std::errc::io_error);
#ifdef ATOM_USE_BOOST
                        BOOST_LOG_TRIVIAL(error)
                            << "asyncLoadStackFromFile: IO error while reading "
                               "file "
                            << filename_str;
#else
                        LOG_F(ERROR,
                              "asyncLoadStackFromFile: IO error while reading "
                              "file %s",
                              filename_str.c_str());
#endif
                        handler(errorCode);
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
                    errorCode =
                        std::make_error_code(std::errc::permission_denied);
#ifdef ATOM_USE_BOOST
                    BOOST_LOG_TRIVIAL(error)
                        << "asyncLoadStackFromFile: Failed to open file "
                        << filename_str << " for reading.";
#else
                    LOG_F(ERROR,
                          "asyncLoadStackFromFile: Failed to open file %s for "
                          "reading.",
                          filename_str.c_str());
#endif
                }

#ifdef ATOM_USE_BOOST
                BOOST_LOG_TRIVIAL(info)
                    << "asyncLoadStackFromFile completed with error code: "
                    << errorCode.value() << " (" << errorCode.message() << ")";
#else
                LOG_F(
                    INFO,
                    "asyncLoadStackFromFile completed with error code: %d (%s)",
                    errorCode.value(), errorCode.message().c_str());
#endif
                handler(errorCode);
            } catch (const std::exception& e) {
                std::error_code errorCode =
                    std::make_error_code(std::errc::io_error);
#ifdef ATOM_USE_BOOST
                BOOST_LOG_TRIVIAL(error)
                    << "Exception in asyncLoadStackFromFile: " << e.what();
#else
                LOG_F(ERROR, "Exception in asyncLoadStackFromFile: %s",
                      e.what());
#endif
                handler(errorCode);
            }
#if defined(ATOM_USE_BOOST) || defined(ATOM_USE_ASIO)
        });
#else
        }
#endif
    }

    void asyncGetCurrentDirectory(
        const std::function<void(const fs::path&, const std::error_code&)>&
            handler) const {
#ifdef ATOM_USE_BOOST
        BOOST_LOG_TRIVIAL(info) << "asyncGetCurrentDirectory called";
#else
        LOG_F(INFO, "asyncGetCurrentDirectory called");
#endif
#if defined(ATOM_USE_BOOST) || defined(ATOM_USE_ASIO)
        asio::post(strand_, [handler]() {
#else
        {
#endif
            std::error_code ec;
            fs::path currentPath;
            try {
                currentPath = fs::current_path(ec);
                if (ec) {
#ifdef ATOM_USE_BOOST
                    BOOST_LOG_TRIVIAL(error)
                        << "asyncGetCurrentDirectory: Failed to get current "
                           "path. Error: "
                        << ec.message();
#else
                    LOG_F(ERROR,
                          "asyncGetCurrentDirectory: Failed to get current "
                          "path. Error: %s",
                          ec.message().c_str());
#endif
                } else {
#ifdef ATOM_USE_BOOST
                    BOOST_LOG_TRIVIAL(info) << "asyncGetCurrentDirectory "
                                               "completed with current path: "
                                            << currentPath.string();
#else
                    LOG_F(INFO,
                          "asyncGetCurrentDirectory completed with current "
                          "path: %s",
                          currentPath.string().c_str());
#endif
                }
                handler(currentPath, ec);
            } catch (const fs::filesystem_error& e) {
                ec = e.code();
#ifdef ATOM_USE_BOOST
                BOOST_LOG_TRIVIAL(error)
                    << "Filesystem exception in asyncGetCurrentDirectory: "
                    << e.what();
#else
                LOG_F(ERROR,
                      "Filesystem exception in asyncGetCurrentDirectory: %s",
                      e.what());
#endif
                handler(fs::path(), ec);
            } catch (const std::exception& e) {
                ec = std::make_error_code(std::errc::io_error);
#ifdef ATOM_USE_BOOST
                BOOST_LOG_TRIVIAL(error)
                    << "Generic exception in asyncGetCurrentDirectory: "
                    << e.what();
#else
                LOG_F(ERROR,
                      "Generic exception in asyncGetCurrentDirectory: %s",
                      e.what());
#endif
                handler(fs::path(), ec);
            }
#if defined(ATOM_USE_BOOST) || defined(ATOM_USE_ASIO)
        });
#else
        }
#endif
    }

    std::stack<fs::path> dirStack_;
#if defined(ATOM_USE_BOOST) || defined(ATOM_USE_ASIO)
    asio::strand<asio::io_context::executor_type> strand_;
#endif
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
#ifdef ATOM_USE_BOOST
            BOOST_LOG_TRIVIAL(error)
                << "pushd: Invalid path provided - " << new_dir.string();
#else
            LOG_F(ERROR, "pushd: Invalid path provided - %s",
                  new_dir.string().c_str());
#endif
            throw fs::filesystem_error(
                "Invalid path provided", new_dir,
                std::make_error_code(std::errc::invalid_argument));
        }

        auto currentPath = fs::current_path(ec);
        if (ec) {
#ifdef ATOM_USE_BOOST
            BOOST_LOG_TRIVIAL(error)
                << "pushd: Failed to get current path. Error: " << ec.message();
#else
            LOG_F(ERROR, "pushd: Failed to get current path. Error: %s",
                  ec.message().c_str());
#endif
            throw fs::filesystem_error("Failed to get current path", ec);
        }

        {
            std::unique_lock lock(impl_->stackMutex_);
            impl_->dirStack_.push(currentPath);
        }

        fs::current_path(new_dir, ec);
        if (ec) {
#ifdef ATOM_USE_BOOST
            BOOST_LOG_TRIVIAL(warning)
                << "pushd: Failed to change directory to " << new_dir.string()
                << ", rolling back stack push. Error: " << ec.message();
#else
            LOG_F(WARNING,
                  "pushd: Failed to change directory to %s, rolling back stack "
                  "push. Error: %s",
                  new_dir.string().c_str(), ec.message().c_str());
#endif
            std::unique_lock lock(impl_->stackMutex_);
            if (!impl_->dirStack_.empty() &&
                impl_->dirStack_.top() == currentPath) {
                impl_->dirStack_.pop();
            }
            throw fs::filesystem_error("Failed to change directory", new_dir,
                                       ec);
        }
#ifdef ATOM_USE_BOOST
        BOOST_LOG_TRIVIAL(info) << "pushd successful to " << new_dir.string();
#else
        LOG_F(INFO, "pushd successful to %s", new_dir.string().c_str());
#endif
    } catch (const fs::filesystem_error& e) {
        throw;
    } catch (const std::exception& e) {
#ifdef ATOM_USE_BOOST
        BOOST_LOG_TRIVIAL(error) << "Generic exception in pushd: " << e.what();
#else
        LOG_F(ERROR, "Generic exception in pushd: %s", e.what());
#endif
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
#ifdef ATOM_USE_BOOST
                BOOST_LOG_TRIVIAL(warning) << "popd: Directory stack is empty";
#else
                LOG_F(WARNING, "popd: Directory stack is empty");
#endif
                throw fs::filesystem_error(
                    "Directory stack is empty",
                    std::make_error_code(std::errc::operation_not_permitted));
            }
            prevDir = impl_->dirStack_.top();
            impl_->dirStack_.pop();
        }

        if (!isValidPath(prevDir)) {
#ifdef ATOM_USE_BOOST
            BOOST_LOG_TRIVIAL(error)
                << "popd: Invalid path found in stack - " << prevDir.string();
#else
            LOG_F(ERROR, "popd: Invalid path found in stack - %s",
                  prevDir.string().c_str());
#endif
            throw fs::filesystem_error(
                "Invalid path in stack", prevDir,
                std::make_error_code(std::errc::invalid_argument));
        }

        fs::current_path(prevDir, ec);
        if (ec) {
#ifdef ATOM_USE_BOOST
            BOOST_LOG_TRIVIAL(error)
                << "popd: Failed to change directory to " << prevDir.string()
                << ". Error: " << ec.message();
#else
            LOG_F(ERROR, "popd: Failed to change directory to %s. Error: %s",
                  prevDir.string().c_str(), ec.message().c_str());
#endif
            throw fs::filesystem_error("Failed to change directory", prevDir,
                                       ec);
        }
#ifdef ATOM_USE_BOOST
        BOOST_LOG_TRIVIAL(info) << "popd successful to " << prevDir.string();
#else
        LOG_F(INFO, "popd successful to %s", prevDir.string().c_str());
#endif
    } catch (const fs::filesystem_error& e) {
        throw;
    } catch (const std::exception& e) {
#ifdef ATOM_USE_BOOST
        BOOST_LOG_TRIVIAL(error) << "Generic exception in popd: " << e.what();
#else
        LOG_F(ERROR, "Generic exception in popd: %s", e.what());
#endif
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
#ifdef ATOM_USE_BOOST
        BOOST_LOG_TRIVIAL(warning) << "swap: Index out of bounds.";
#else
        LOG_F(WARNING, "swap: Index out of bounds.");
#endif
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
#ifdef ATOM_USE_BOOST
        BOOST_LOG_TRIVIAL(warning) << "remove: Index out of bounds.";
#else
        LOG_F(WARNING, "remove: Index out of bounds.");
#endif
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
#ifdef ATOM_USE_BOOST
            BOOST_LOG_TRIVIAL(warning) << "gotoIndex: Index out of bounds.";
#else
            LOG_F(WARNING, "gotoIndex: Index out of bounds.");
#endif
            throw fs::filesystem_error(
                "Index out of bounds",
                std::make_error_code(std::errc::invalid_argument));
        }

        size_t vec_index = size - 1 - index;
        const fs::path& targetPath = contents[vec_index];

        if (!isValidPath(targetPath)) {
#ifdef ATOM_USE_BOOST
            BOOST_LOG_TRIVIAL(error)
                << "gotoIndex: Invalid path in stack at index " << index;
#else
            LOG_F(ERROR, "gotoIndex: Invalid path in stack at index %zu",
                  index);
#endif
            throw fs::filesystem_error(
                "Invalid path in stack", targetPath,
                std::make_error_code(std::errc::invalid_argument));
        }

        fs::current_path(targetPath, ec);
        if (ec) {
#ifdef ATOM_USE_BOOST
            BOOST_LOG_TRIVIAL(error)
                << "gotoIndex: Failed to change directory. Error: "
                << ec.message();
#else
            LOG_F(ERROR, "gotoIndex: Failed to change directory. Error: %s",
                  ec.message().c_str());
#endif
            throw fs::filesystem_error("Failed to change directory", targetPath,
                                       ec);
        }
    } catch (const fs::filesystem_error& e) {
        throw;
    } catch (const std::exception& e) {
#ifdef ATOM_USE_BOOST
        BOOST_LOG_TRIVIAL(error)
            << "Generic exception in gotoIndex: " << e.what();
#else
        LOG_F(ERROR, "Generic exception in gotoIndex: %s", e.what());
#endif
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
#ifdef ATOM_USE_BOOST
            BOOST_LOG_TRIVIAL(warning)
                << "saveStackToFile: Empty filename provided.";
#else
            LOG_F(WARNING, "saveStackToFile: Empty filename provided.");
#endif
            throw fs::filesystem_error(
                "Empty filename provided",
                std::make_error_code(std::errc::invalid_argument));
        }

        std::ofstream file(filename_str);
        if (!file) {
#ifdef ATOM_USE_BOOST
            BOOST_LOG_TRIVIAL(error)
                << "saveStackToFile: Failed to open file " << filename_str;
#else
            LOG_F(ERROR, "saveStackToFile: Failed to open file %s",
                  filename_str.c_str());
#endif
            throw fs::filesystem_error(
                "Failed to open file", filename_str,
                std::make_error_code(std::errc::permission_denied));
        }

        Vector<fs::path> contents = impl_->getStackContents();

        for (const auto& dir : contents) {
            file << dir.string() << '\n';
        }

        if (!file.good()) {
#ifdef ATOM_USE_BOOST
            BOOST_LOG_TRIVIAL(error)
                << "saveStackToFile: IO error writing to file " << filename_str;
#else
            LOG_F(ERROR, "saveStackToFile: IO error writing to file %s",
                  filename_str.c_str());
#endif
            throw fs::filesystem_error(
                "IO error writing to file", filename_str,
                std::make_error_code(std::errc::io_error));
        }

    } catch (const fs::filesystem_error& e) {
        throw;
    } catch (const std::exception& e) {
#ifdef ATOM_USE_BOOST
        BOOST_LOG_TRIVIAL(error)
            << "Generic exception in saveStackToFile: " << e.what();
#else
        LOG_F(ERROR, "Generic exception in saveStackToFile: %s", e.what());
#endif
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
#ifdef ATOM_USE_BOOST
            BOOST_LOG_TRIVIAL(warning)
                << "loadStackFromFile: Empty filename provided.";
#else
            LOG_F(WARNING, "loadStackFromFile: Empty filename provided.");
#endif
            throw fs::filesystem_error(
                "Empty filename provided",
                std::make_error_code(std::errc::invalid_argument));
        }
        std::error_code exists_ec;
        if (!fs::exists(filename_str, exists_ec) || exists_ec) {
#ifdef ATOM_USE_BOOST
            BOOST_LOG_TRIVIAL(warning) << "loadStackFromFile: File not found "
                                          "or error checking existence: "
                                       << filename_str;
#else
            LOG_F(WARNING,
                  "loadStackFromFile: File not found or error checking "
                  "existence: %s",
                  filename_str.c_str());
#endif
            throw fs::filesystem_error(
                "File not found", filename_str,
                std::make_error_code(std::errc::no_such_file_or_directory));
        }

        std::ifstream file(filename_str);
        if (!file) {
#ifdef ATOM_USE_BOOST
            BOOST_LOG_TRIVIAL(error)
                << "loadStackFromFile: Failed to open file " << filename_str;
#else
            LOG_F(ERROR, "loadStackFromFile: Failed to open file %s",
                  filename_str.c_str());
#endif
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
#ifdef ATOM_USE_BOOST
                BOOST_LOG_TRIVIAL(error)
                    << "loadStackFromFile: Invalid path in file: " << line;
#else
                LOG_F(ERROR, "loadStackFromFile: Invalid path in file: %s",
                      line.c_str());
#endif
                throw fs::filesystem_error(
                    "Invalid path in file", currentPath,
                    std::make_error_code(std::errc::invalid_argument));
            }
            loadedPaths.push_back(std::move(currentPath));
        }

        if (!file.eof() && file.fail()) {
#ifdef ATOM_USE_BOOST
            BOOST_LOG_TRIVIAL(error)
                << "loadStackFromFile: IO error reading file " << filename_str;
#else
            LOG_F(ERROR, "loadStackFromFile: IO error reading file %s",
                  filename_str.c_str());
#endif
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

    } catch (const fs::filesystem_error& e) {
        throw;
    } catch (const std::exception& e) {
#ifdef ATOM_USE_BOOST
        BOOST_LOG_TRIVIAL(error)
            << "Generic exception in loadStackFromFile: " << e.what();
#else
        LOG_F(ERROR, "Generic exception in loadStackFromFile: %s", e.what());
#endif
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
#ifdef ATOM_USE_BOOST
            BOOST_LOG_TRIVIAL(error)
                << "getCurrentDirectory: Failed to get current path. Error: "
                << ec.message();
#else
            LOG_F(ERROR,
                  "getCurrentDirectory: Failed to get current path. Error: %s",
                  ec.message().c_str());
#endif
            throw fs::filesystem_error("Failed to get current path", ec);
        }
    } catch (const fs::filesystem_error& e) {
        throw;
    } catch (const std::exception& e) {
#ifdef ATOM_USE_BOOST
        BOOST_LOG_TRIVIAL(error)
            << "Generic exception in getCurrentDirectory: " << e.what();
#else
        LOG_F(ERROR, "Generic exception in getCurrentDirectory: %s", e.what());
#endif
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
