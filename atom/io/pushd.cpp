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
#endif

// Include asio explicitly if not using Boost for strand
#ifndef ATOM_USE_BOOST
#include <asio.hpp>
#endif

#include "atom/containers/high_performance.hpp"  // Include high performance containers
#include "atom/io/pushd.hpp"                     // Ensure header is included

// Use type aliases from high_performance.hpp
using atom::containers::String;
using atom::containers::Vector;

namespace atom::io {

namespace {
// Helper function for path validation
[[nodiscard]] bool isValidPath(const fs::path& path) noexcept {
    try {
        if (path.empty())
            return false;

        // Check for basic format validity
        // Allow paths that are just root names (e.g., "C:")
        // if (!path.has_filename() && !path.has_parent_path() &&
        // !path.has_root_name())
        //    return false;

        // Attempt to make canonical path - this might fail for non-existent
        // paths, use weakly_canonical instead for basic syntax checks.
        std::error_code ec;
        [[maybe_unused]] auto canonical_path = fs::weakly_canonical(path, ec);
        if (ec) {
            // Log the error if needed
            // LOG_F(WARNING, "Path validation failed for %s: %s",
            // path.string().c_str(), ec.message().c_str());
            return false;  // Consider invalid if weakly_canonical fails
        }

        return true;
    } catch (const std::exception& e) {
        // Log the exception if needed
        // LOG_F(ERROR, "Exception during path validation for %s: %s",
        // path.string().c_str(), e.what());
        return false;
    }
}
}  // namespace

class DirectoryStackImpl {
public:
    explicit DirectoryStackImpl(asio::io_context& io_context)
        : strand_(asio::make_strand(io_context)) {}  // Use make_strand

    // Thread-safe access to directory stack
    mutable std::shared_mutex stackMutex_;

    // Note: Template function definitions should generally be in the header.
    // Keeping them here requires explicit instantiation or careful linking.
    template <PathLike P>
    void asyncPushd(
        const P& new_dir_param,
        const std::function<void(const std::error_code&)>& handler) {
        fs::path new_dir = new_dir_param;  // Convert PathLike to fs::path

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
#ifdef ATOM_USE_BOOST
            BOOST_LOG_TRIVIAL(warning)
                << "asyncPushd: Invalid path provided - " << new_dir.string();
#else
            LOG_F(WARNING, "asyncPushd: Invalid path provided - %s",
                  new_dir.string().c_str());
#endif
            // Call handler directly or post it to the strand for consistency?
            // Posting ensures handler runs on the strand.
            asio::post(strand_, [handler, ec]() { handler(ec); });
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
                    // Attempt to change directory
                    fs::current_path(new_dir, errorCode);
                    if (errorCode) {
                        // If changing directory failed, rollback the stack push
#ifdef ATOM_USE_BOOST
                        BOOST_LOG_TRIVIAL(warning)
                            << "asyncPushd: Failed to change directory to "
                            << new_dir.string()
                            << ", rolling back stack push. Error: "
                            << errorCode.message();
#else
                        LOG_F(WARNING, "asyncPushd: Failed to change directory to %s, rolling back stack push. Error: %s",
                              new_dir.string().c_str(), errorCode.message().c_str());
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
                    LOG_F(ERROR, "asyncPushd: Failed to get current path. Error: %s", errorCode.message().c_str());
#endif
                }

#ifdef ATOM_USE_BOOST
                BOOST_LOG_TRIVIAL(info)
                    << "asyncPushd completed with error code: "
                    << errorCode.value() << " (" << errorCode.message() << ")";
#else
                LOG_F(INFO, "asyncPushd completed with error code: %d (%s)", errorCode.value(), errorCode.message().c_str());
#endif
                handler(errorCode);
            } catch (const fs::filesystem_error&
                         e) {  // Catch filesystem_error specifically
                std::error_code errorCode =
                    e.code();  // Use error code from exception
#ifdef ATOM_USE_BOOST
                BOOST_LOG_TRIVIAL(error)
                    << "Filesystem exception in asyncPushd: " << e.what();
#else
                LOG_F(ERROR, "Filesystem exception in asyncPushd: %s", e.what());
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
#ifdef ATOM_USE_BOOST
                        BOOST_LOG_TRIVIAL(warning)
                            << "asyncPopd: Stack is empty.";
#else
                        LOG_F(WARNING, "asyncPopd: Stack is empty.");
#endif
                        errorCode = std::make_error_code(
                            std::errc::operation_not_permitted);  // More
                                                                  // specific
                                                                  // error?
                        handler(errorCode);
                        return;
                    }
                }

                // Validate the path before changing to it
                if (!isValidPath(prevDir)) {
#ifdef ATOM_USE_BOOST
                    BOOST_LOG_TRIVIAL(error)
                        << "asyncPopd: Invalid path found in stack - "
                        << prevDir.string();
#else
                    LOG_F(ERROR, "asyncPopd: Invalid path found in stack - %s", prevDir.string().c_str());
#endif
                    errorCode =
                        std::make_error_code(std::errc::invalid_argument);
                    // Should we push the invalid path back? Probably not.
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
                    LOG_F(ERROR, "asyncPopd: Failed to change directory to %s. Error: %s",
                          prevDir.string().c_str(), errorCode.message().c_str());
#endif
                    // Attempt to push the directory back onto the stack if
                    // change failed? This might lead to inconsistent state if
                    // the reason for failure is persistent. For now, just
                    // report the error.
                }

#ifdef ATOM_USE_BOOST
                BOOST_LOG_TRIVIAL(info)
                    << "asyncPopd completed with error code: "
                    << errorCode.value() << " (" << errorCode.message() << ")";
#else
                LOG_F(INFO, "asyncPopd completed with error code: %d (%s)", errorCode.value(), errorCode.message().c_str());
#endif
                handler(errorCode);
            } catch (const fs::filesystem_error&
                         e) {  // Catch filesystem_error specifically
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
        });
    }

    [[nodiscard]] auto getStackContents() const
        -> Vector<fs::path> {  // Return Vector
        std::shared_lock lock(stackMutex_);
        std::stack<fs::path> tempStack = dirStack_;
        Vector<fs::path> contents;           // Use Vector
        contents.reserve(tempStack.size());  // Pre-allocate memory

        while (!tempStack.empty()) {
            // Add elements in reverse order temporarily
            contents.push_back(tempStack.top());
            tempStack.pop();
        }

        // Reverse to get the correct stack order (top is last)
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
                Vector<fs::path> contents;  // Use Vector
                {                           // Lock only while reading the stack
                    std::shared_lock lock(stackMutex_);
                    // Need to reconstruct the stack in the correct order for
                    // indexing
                    std::stack<fs::path> tempStack = dirStack_;
                    contents.reserve(tempStack.size());
                    while (!tempStack.empty()) {
                        contents.push_back(tempStack.top());
                        tempStack.pop();
                    }
                    std::reverse(contents.begin(),
                                 contents.end());  // Now contents[0] is bottom,
                                                   // contents[size-1] is top
                }

                if (index < contents.size()) {
                    // Note: The interpretation of index might be ambiguous.
                    // Standard 'dirs' command shows top first (index 0).
                    // Let's assume index 0 refers to the top of the stack (last
                    // pushed). Since `contents` is now ordered bottom-to-top,
                    // the target is at size - 1 - index.
                    size_t effective_index = contents.size() - 1 - index;
                    const fs::path& targetPath = contents[effective_index];

                    // Validate the path before changing to it
                    if (!isValidPath(targetPath)) {
#ifdef ATOM_USE_BOOST
                        BOOST_LOG_TRIVIAL(error)
                            << "asyncGotoIndex: Invalid path found in stack at "
                               "index "
                            << index << " - " << targetPath.string();
#else
                        LOG_F(ERROR, "asyncGotoIndex: Invalid path found in stack at index %zu - %s",
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
                        LOG_F(ERROR, "asyncGotoIndex: Failed to change directory to %s. Error: %s",
                              targetPath.string().c_str(), errorCode.message().c_str());
#endif
                    }
                } else {
#ifdef ATOM_USE_BOOST
                    BOOST_LOG_TRIVIAL(warning)
                        << "asyncGotoIndex: Index " << index
                        << " out of bounds (stack size " << contents.size()
                        << ").";
#else
                    LOG_F(WARNING, "asyncGotoIndex: Index %zu out of bounds (stack size %zu).", index, contents.size());
#endif
                    errorCode =
                        std::make_error_code(std::errc::invalid_argument);
                }

#ifdef ATOM_USE_BOOST
                BOOST_LOG_TRIVIAL(info)
                    << "asyncGotoIndex completed with error code: "
                    << errorCode.value() << " (" << errorCode.message() << ")";
#else
                LOG_F(INFO, "asyncGotoIndex completed with error code: %d (%s)", errorCode.value(), errorCode.message().c_str());
#endif
                handler(errorCode);
            } catch (const fs::filesystem_error&
                         e) {  // Catch filesystem_error specifically
                std::error_code errorCode = e.code();
#ifdef ATOM_USE_BOOST
                BOOST_LOG_TRIVIAL(error)
                    << "Filesystem exception in asyncGotoIndex: " << e.what();
#else
                LOG_F(ERROR, "Filesystem exception in asyncGotoIndex: %s", e.what());
#endif
                handler(errorCode);
            } catch (const std::exception& e) {
                std::error_code errorCode =
                    std::make_error_code(std::errc::io_error);
#ifdef ATOM_USE_BOOST
                BOOST_LOG_TRIVIAL(error)
                    << "Generic exception in asyncGotoIndex: " << e.what();
#else
                LOG_F(ERROR, "Generic exception in asyncGotoIndex: %s", e.what());
#endif
                handler(errorCode);
            }
        });
    }

    void asyncSaveStackToFile(
        const String& filename,  // Use String
        const std::function<void(const std::error_code&)>& handler) {
#ifdef ATOM_USE_BOOST
        BOOST_LOG_TRIVIAL(info)
            << "asyncSaveStackToFile called with filename: "
            << filename.c_str();  // Use c_str() for logging if needed
#else
        LOG_F(INFO, "asyncSaveStackToFile called with filename: %s",
              filename.c_str());
#endif
        // Validate filename
        if (filename.empty()) {
#ifdef ATOM_USE_BOOST
            BOOST_LOG_TRIVIAL(warning)
                << "asyncSaveStackToFile: Empty filename provided.";
#else
            LOG_F(WARNING, "asyncSaveStackToFile: Empty filename provided.");
#endif
            std::error_code errorCode =
                std::make_error_code(std::errc::invalid_argument);
            asio::post(strand_, [handler, errorCode]() { handler(errorCode); });
            return;
        }

        // Convert String to std::string for ofstream if necessary
        std::string filename_str =
            filename.c_str();  // Assuming String has toStdString() or similar

        asio::post(strand_, [this, filename_str,
                             handler]() {  // Capture std::string
            try {
                std::error_code errorCode;
                std::ofstream file(filename_str);  // Use std::string

                if (file) {
                    Vector<fs::path> contents =
                        getStackContents();  // Use Vector

                    // Write stack contents (bottom to top)
                    for (const auto& dir : contents) {
                        // Ensure path strings don't contain newlines
                        // internally, or handle appropriately
                        file << dir.string() << '\n';
                    }

                    if (!file.good()) {  // Check stream state after writing
                        errorCode = std::make_error_code(std::errc::io_error);
#ifdef ATOM_USE_BOOST
                        BOOST_LOG_TRIVIAL(error)
                            << "asyncSaveStackToFile: IO error while writing "
                               "to file "
                            << filename_str;
#else
                        LOG_F(ERROR, "asyncSaveStackToFile: IO error while writing to file %s", filename_str.c_str());
#endif
                    }
                } else {
                    errorCode = std::make_error_code(
                        std::errc::permission_denied);  // Or other appropriate
                                                        // error
#ifdef ATOM_USE_BOOST
                    BOOST_LOG_TRIVIAL(error)
                        << "asyncSaveStackToFile: Failed to open file "
                        << filename_str << " for writing.";
#else
                    LOG_F(ERROR, "asyncSaveStackToFile: Failed to open file %s for writing.", filename_str.c_str());
#endif
                }

#ifdef ATOM_USE_BOOST
                BOOST_LOG_TRIVIAL(info)
                    << "asyncSaveStackToFile completed with error code: "
                    << errorCode.value() << " (" << errorCode.message() << ")";
#else
                LOG_F(INFO, "asyncSaveStackToFile completed with error code: %d (%s)", errorCode.value(), errorCode.message().c_str());
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
        const String& filename,  // Use String
        const std::function<void(const std::error_code&)>& handler) {
#ifdef ATOM_USE_BOOST
        BOOST_LOG_TRIVIAL(info)
            << "asyncLoadStackFromFile called with filename: "
            << filename.c_str();
#else
        LOG_F(INFO, "asyncLoadStackFromFile called with filename: %s",
              filename.c_str());
#endif

        // Convert String to std::string for ifstream and fs::exists
        std::string filename_str =
            filename.c_str();  // Assuming String has toStdString()

        // Validate filename and existence
        if (filename_str.empty()) {
#ifdef ATOM_USE_BOOST
            BOOST_LOG_TRIVIAL(warning)
                << "asyncLoadStackFromFile: Empty filename provided.";
#else
            LOG_F(WARNING, "asyncLoadStackFromFile: Empty filename provided.");
#endif
            std::error_code errorCode =
                std::make_error_code(std::errc::invalid_argument);
            asio::post(strand_, [handler, errorCode]() { handler(errorCode); });
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
            asio::post(strand_, [handler, errorCode]() { handler(errorCode); });
            return;
        }

        asio::post(strand_, [this, filename_str,
                             handler]() {  // Capture std::string
            try {
                std::error_code errorCode;
                std::ifstream file(filename_str);  // Use std::string

                if (file) {
                    std::vector<fs::path>
                        loadedPaths;  // Read into a temporary vector first
                    std::string line;

                    while (std::getline(file, line)) {
                        // Basic validation: skip empty lines
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
                            LOG_F(ERROR, "asyncLoadStackFromFile: Invalid path found in file %s - %s",
                                  filename_str.c_str(), line.c_str());
#endif
                            errorCode = std::make_error_code(
                                std::errc::invalid_argument);
                            // Stop loading on first invalid path
                            handler(errorCode);
                            return;
                        }
                        loadedPaths.push_back(std::move(currentPath));
                    }

                    if (!file.eof() &&
                        file.fail()) {  // Check for read errors other than EOF
                        errorCode = std::make_error_code(std::errc::io_error);
#ifdef ATOM_USE_BOOST
                        BOOST_LOG_TRIVIAL(error)
                            << "asyncLoadStackFromFile: IO error while reading "
                               "file "
                            << filename_str;
#else
                         LOG_F(ERROR, "asyncLoadStackFromFile: IO error while reading file %s", filename_str.c_str());
#endif
                        handler(errorCode);
                        return;
                    }

                    // Construct the new stack (paths are loaded bottom-to-top)
                    std::stack<fs::path> newStack;
                    for (const auto& path : loadedPaths) {
                        newStack.push(path);
                    }

                    {
                        std::unique_lock lock(stackMutex_);
                        dirStack_ = std::move(newStack);
                    }
                } else {
                    errorCode = std::make_error_code(
                        std::errc::permission_denied);  // Or other appropriate
                                                        // error
#ifdef ATOM_USE_BOOST
                    BOOST_LOG_TRIVIAL(error)
                        << "asyncLoadStackFromFile: Failed to open file "
                        << filename_str << " for reading.";
#else
                     LOG_F(ERROR, "asyncLoadStackFromFile: Failed to open file %s for reading.", filename_str.c_str());
#endif
                }

#ifdef ATOM_USE_BOOST
                BOOST_LOG_TRIVIAL(info)
                    << "asyncLoadStackFromFile completed with error code: "
                    << errorCode.value() << " (" << errorCode.message() << ")";
#else
                LOG_F(INFO, "asyncLoadStackFromFile completed with error code: %d (%s)", errorCode.value(), errorCode.message().c_str());
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
        const std::function<void(const fs::path&, const std::error_code&)>&
            handler) const {  // Add error code to handler
#ifdef ATOM_USE_BOOST
        BOOST_LOG_TRIVIAL(info) << "asyncGetCurrentDirectory called";
#else
        LOG_F(INFO, "asyncGetCurrentDirectory called");
#endif
        // No strand needed for read-only fs::current_path? Check thread-safety
        // guarantees. Assuming fs::current_path() is thread-safe for reading.
        // If not, post to strand. Let's post to strand for consistency and
        // safety.
        asio::post(strand_, [handler]() {
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
                    LOG_F(ERROR, "asyncGetCurrentDirectory: Failed to get current path. Error: %s", ec.message().c_str());
#endif
                } else {
#ifdef ATOM_USE_BOOST
                    BOOST_LOG_TRIVIAL(info) << "asyncGetCurrentDirectory "
                                               "completed with current path: "
                                            << currentPath.string();
#else
                    LOG_F(INFO, "asyncGetCurrentDirectory completed with current path: %s",
                          currentPath.string().c_str());
#endif
                }
                handler(currentPath, ec);  // Pass path and error code
            } catch (const fs::filesystem_error& e) {
                ec = e.code();
#ifdef ATOM_USE_BOOST
                BOOST_LOG_TRIVIAL(error)
                    << "Filesystem exception in asyncGetCurrentDirectory: "
                    << e.what();
#else
                LOG_F(ERROR, "Filesystem exception in asyncGetCurrentDirectory: %s", e.what());
#endif
                handler(fs::path(), ec);  // Return empty path and error code
            } catch (const std::exception& e) {
                ec = std::make_error_code(std::errc::io_error);
#ifdef ATOM_USE_BOOST
                BOOST_LOG_TRIVIAL(error)
                    << "Generic exception in asyncGetCurrentDirectory: "
                    << e.what();
#else
                LOG_F(ERROR, "Generic exception in asyncGetCurrentDirectory: %s", e.what());
#endif
                handler(fs::path(), ec);  // Return empty path and error code
            }
        });
    }

    std::stack<fs::path> dirStack_;
    asio::strand<asio::io_context::executor_type>
        strand_;  // Use strand correctly
};

// DirectoryStack public interface methods implementation

DirectoryStack::DirectoryStack(asio::io_context& io_context)
    : impl_(std::make_unique<DirectoryStackImpl>(io_context)) {}

DirectoryStack::~DirectoryStack() noexcept = default;

DirectoryStack::DirectoryStack(DirectoryStack&& other) noexcept = default;

auto DirectoryStack::operator=(DirectoryStack&& other) noexcept
    -> DirectoryStack& = default;

// --- Template Definitions ---
// NOTE: These template definitions should ideally be moved to the header file
// (pushd.hpp)
//       to avoid potential linker errors, unless you explicitly instantiate
//       them for all required PathLike types (like fs::path, std::string, const
//       char*).

template <PathLike P>
void DirectoryStack::asyncPushd(
    const P& new_dir,
    const std::function<void(const std::error_code&)>& handler) {
    // Forward to implementation's template method
    impl_->asyncPushd<P>(new_dir, handler);
}

template <PathLike P>
auto DirectoryStack::pushd(const P& new_dir_param) -> Task<void> {
    fs::path new_dir = new_dir_param;  // Convert PathLike to fs::path
    // co_await asio::post(impl_->strand_, asio::use_awaitable); // Ensure
    // execution on strand if needed, requires C++20 awaitable support in Asio

    // The actual logic needs to be awaitable or run synchronously within the
    // coroutine context. For simplicity, let's perform the operations directly,
    // assuming they are safe to call from the coroutine's context. If they
    // block heavily, consider co_awaiting an async operation posted to a
    // background thread.

    co_await std::suspend_never{};  // Minimal awaitable just to make it a
                                    // coroutine

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
            // How to report error from coroutine? Throwing is standard.
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
            // Rollback if changing directory failed
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
        // Log already happened or will happen in caller
        throw;  // Re-throw filesystem_error
    } catch (const std::exception& e) {
#ifdef ATOM_USE_BOOST
        BOOST_LOG_TRIVIAL(error) << "Generic exception in pushd: " << e.what();
#else
        LOG_F(ERROR, "Generic exception in pushd: %s", e.what());
#endif
        // Wrap generic exception in filesystem_error? Or rethrow as is?
        // Rethrowing as is might be better unless we can map it to a specific
        // filesystem issue.
        throw;
    }

    co_return;  // Indicates success
}

// --- End Template Definitions ---

void DirectoryStack::asyncPopd(
    const std::function<void(const std::error_code&)>& handler) {
    impl_->asyncPopd(handler);
}

auto DirectoryStack::popd() -> Task<void> {
    // co_await asio::post(impl_->strand_, asio::use_awaitable); // Ensure
    // execution on strand if needed

    co_await std::suspend_never{};  // Minimal awaitable

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

        // Validate the path before changing to it
        if (!isValidPath(prevDir)) {
#ifdef ATOM_USE_BOOST
            BOOST_LOG_TRIVIAL(error)
                << "popd: Invalid path found in stack - " << prevDir.string();
#else
            LOG_F(ERROR, "popd: Invalid path found in stack - %s",
                  prevDir.string().c_str());
#endif
            // Don't push back the invalid path.
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
            // Should we push prevDir back? Maybe not, state is uncertain.
            throw fs::filesystem_error("Failed to change directory", prevDir,
                                       ec);
        }
#ifdef ATOM_USE_BOOST
        BOOST_LOG_TRIVIAL(info) << "popd successful to " << prevDir.string();
#else
        LOG_F(INFO, "popd successful to %s", prevDir.string().c_str());
#endif
    } catch (const fs::filesystem_error& e) {
        throw;  // Re-throw filesystem_error
    } catch (const std::exception& e) {
#ifdef ATOM_USE_BOOST
        BOOST_LOG_TRIVIAL(error) << "Generic exception in popd: " << e.what();
#else
        LOG_F(ERROR, "Generic exception in popd: %s", e.what());
#endif
        throw;
    }

    co_return;  // Indicates success
}

auto DirectoryStack::peek() const -> fs::path {
    std::shared_lock lock(impl_->stackMutex_);
    if (impl_->dirStack_.empty()) {
        // Returning empty path is conventional, but throwing might be better
        // depending on expected usage. Let's throw for clarity.
        throw std::runtime_error("Directory stack is empty");
    }
    return impl_->dirStack_.top();
}

auto DirectoryStack::dirs() const noexcept
    -> Vector<fs::path> {  // Return Vector
    // The result of getStackContents is already bottom-to-top.
    // If 'dirs' command traditionally shows top-first, we need to reverse it
    // here. Let's assume 'dirs' shows top-first (like Unix `dirs`).
    Vector<fs::path> contents =
        impl_->getStackContents();                   // Gets bottom-to-top
    std::reverse(contents.begin(), contents.end());  // Reverse to top-to-bottom
    return contents;
}

void DirectoryStack::clear() noexcept {
    std::unique_lock lock(impl_->stackMutex_);
    // Efficiently clear the stack
    std::stack<fs::path>().swap(impl_->dirStack_);
}

void DirectoryStack::swap(size_t index1, size_t index2) {
    // Need to lock exclusively as we modify the stack
    std::unique_lock lock(impl_->stackMutex_);
    // Get stack contents (bottom-to-top order)
    std::stack<fs::path> tempStack = impl_->dirStack_;
    Vector<fs::path> contents;
    contents.reserve(tempStack.size());
    while (!tempStack.empty()) {
        contents.push_back(tempStack.top());
        tempStack.pop();
    }
    std::reverse(contents.begin(), contents.end());  // Now bottom-to-top

    // Assuming index 0 is top of stack (like `dirs` output)
    size_t size = contents.size();
    if (index1 >= size || index2 >= size) {
#ifdef ATOM_USE_BOOST
        BOOST_LOG_TRIVIAL(warning) << "swap: Index out of bounds.";
#else
        LOG_F(WARNING, "swap: Index out of bounds.");
#endif
        // Throw or return? Throwing seems more appropriate for invalid args.
        throw std::out_of_range("Index out of bounds for directory stack swap");
    }

    // Convert top-based index to bottom-based index for vector
    size_t vec_index1 = size - 1 - index1;
    size_t vec_index2 = size - 1 - index2;

    std::swap(contents[vec_index1], contents[vec_index2]);

    // Rebuild the stack (pushing bottom element first)
    std::stack<fs::path> newStack;
    for (const auto& path : contents) {
        newStack.push(path);
    }
    impl_->dirStack_ = std::move(newStack);
}

void DirectoryStack::remove(size_t index) {
    std::unique_lock lock(impl_->stackMutex_);
    // Get stack contents (bottom-to-top order)
    std::stack<fs::path> tempStack = impl_->dirStack_;
    Vector<fs::path> contents;
    contents.reserve(tempStack.size());
    while (!tempStack.empty()) {
        contents.push_back(tempStack.top());
        tempStack.pop();
    }
    std::reverse(contents.begin(), contents.end());  // Now bottom-to-top

    // Assuming index 0 is top of stack
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

    // Convert top-based index to bottom-based index
    size_t vec_index = size - 1 - index;
    contents.erase(contents.begin() +
                   static_cast<Vector<fs::path>::difference_type>(
                       vec_index));  // Use correct difference_type

    // Rebuild the stack
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

// Coroutine versions need implementation
auto DirectoryStack::gotoIndex(size_t index) -> Task<void> {
    // co_await asio::post(impl_->strand_, asio::use_awaitable);
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
            std::reverse(contents.begin(), contents.end());  // bottom-to-top
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

        // Assuming index 0 is top
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
    const String& filename,  // Use String
    const std::function<void(const std::error_code&)>& handler) {
    impl_->asyncSaveStackToFile(filename, handler);
}

auto DirectoryStack::saveStackToFile(const String& filename)
    -> Task<void> {  // Use String
    // co_await asio::post(impl_->strand_, asio::use_awaitable);
    co_await std::suspend_never{};

    std::string filename_str = filename.c_str();  // Convert

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

        Vector<fs::path> contents = impl_->getStackContents();  // bottom-to-top

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
    const String& filename,  // Use String
    const std::function<void(const std::error_code&)>& handler) {
    impl_->asyncLoadStackFromFile(filename, handler);
}

auto DirectoryStack::loadStackFromFile(const String& filename)
    -> Task<void> {  // Use String
    // co_await asio::post(impl_->strand_, asio::use_awaitable);
    co_await std::suspend_never{};

    std::string filename_str = filename.c_str();  // Convert

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
                             const std::error_code&)>& handler)
    const {  // Add error code
    impl_->asyncGetCurrentDirectory(handler);
}

auto DirectoryStack::getCurrentDirectory() const -> Task<fs::path> {
    // co_await asio::post(impl_->strand_, asio::use_awaitable);
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

// --- Explicit Instantiations (Required if definitions remain in .cpp) ---
// You MUST explicitly instantiate the template functions for every PathLike
// type you intend to use them with (e.g., fs::path, std::string, const char*).
// If you don't do this, you will get linker errors.
// Example:
template void DirectoryStack::asyncPushd<fs::path>(
    const fs::path&, const std::function<void(const std::error_code&)>&);
template void DirectoryStack::asyncPushd<std::string>(
    const std::string&, const std::function<void(const std::error_code&)>&);
// Add more instantiations as needed...

template auto DirectoryStack::pushd<fs::path>(const fs::path&) -> Task<void>;
template auto DirectoryStack::pushd<std::string>(const std::string&)
    -> Task<void>;
// Add more instantiations as needed...

}  // namespace atom::io