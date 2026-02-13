#ifndef ATOM_IO_FILESYSTEM_DIRECTORY_STACK_HPP
#define ATOM_IO_FILESYSTEM_DIRECTORY_STACK_HPP

#include <filesystem>
#include <functional>
#include <memory>
#include <system_error>

#ifdef ATOM_USE_BOOST
#include <boost/asio.hpp>
namespace asio = boost::asio;
#elif defined(ATOM_USE_ASIO)
#include <asio.hpp>
#endif

#include "atom/containers/high_performance.hpp"
#include "atom/io/core/io.hpp"
#include "atom/io/filesystem/task.hpp"

using atom::containers::String;
using atom::containers::Vector;

namespace atom::io {

class DirectoryStackImpl;

// PathLike concept is defined in atom/io/core/io.hpp

class DirectoryStack {
public:
    /**
     * @brief Backward-compatible type alias for the coroutine Task type.
     * @deprecated Use atom::io::Task<T> directly instead.
     */
    template <typename T>
    using Task = atom::io::Task<T>;

    /**
     * @brief Constructs a DirectoryStack with optional async executor
     * @param io_context IO context for async operations (nullptr if sync only)
     */
    explicit DirectoryStack(
#if defined(ATOM_USE_BOOST) || defined(ATOM_USE_ASIO)
        asio::io_context& io_context
#else
        void* io_context = nullptr
#endif
    );

    ~DirectoryStack() noexcept;

    DirectoryStack(const DirectoryStack& other) = delete;
    auto operator=(const DirectoryStack& other) -> DirectoryStack& = delete;

    DirectoryStack(DirectoryStack&& other) noexcept;
    auto operator=(DirectoryStack&& other) noexcept -> DirectoryStack&;

    /**
     * @brief Asynchronously push current directory and change to new directory
     * @tparam P Path-like type
     * @param new_dir Directory to change to
     * @param handler Completion handler
     */
    template <PathLike P>
    void asyncPushd(const P& new_dir,
                    const std::function<void(const std::error_code&)>& handler);

    /**
     * @brief Coroutine version of pushd
     * @tparam P Path-like type
     * @param new_dir Directory to change to
     * @return Task for completion
     */
    template <PathLike P>
    [[nodiscard]] auto pushd(const P& new_dir) -> Task<void>;

    /**
     * @brief Asynchronously pop directory from stack and change to it
     * @param handler Completion handler
     */
    void asyncPopd(const std::function<void(const std::error_code&)>& handler);

    /**
     * @brief Coroutine version of popd
     * @return Task for completion
     */
    [[nodiscard]] auto popd() -> Task<void>;

    /**
     * @brief Get top directory from stack without popping
     * @return Top directory path
     */
    [[nodiscard]] auto peek() const -> std::filesystem::path;

    /**
     * @brief Get all directories in stack
     * @return Vector of directory paths
     */
    [[nodiscard]] auto dirs() const noexcept -> Vector<std::filesystem::path>;

    /**
     * @brief Clear all directories from stack
     */
    void clear() noexcept;

    /**
     * @brief Swap two directories in stack
     * @param index1 First index
     * @param index2 Second index
     */
    void swap(size_t index1, size_t index2);

    /**
     * @brief Remove directory at index
     * @param index Index to remove
     */
    void remove(size_t index);

    /**
     * @brief Asynchronously go to directory at index
     * @param index Stack index
     * @param handler Completion handler
     */
    void asyncGotoIndex(
        size_t index,
        const std::function<void(const std::error_code&)>& handler);

    /**
     * @brief Coroutine version of goto index
     * @param index Stack index
     * @return Task for completion
     */
    [[nodiscard]] auto gotoIndex(size_t index) -> Task<void>;

    /**
     * @brief Asynchronously save stack to file
     * @param filename File to save to
     * @param handler Completion handler
     */
    void asyncSaveStackToFile(
        const String& filename,
        const std::function<void(const std::error_code&)>& handler);

    /**
     * @brief Coroutine version of save stack to file
     * @param filename File to save to
     * @return Task for completion
     */
    [[nodiscard]] auto saveStackToFile(const String& filename) -> Task<void>;

    /**
     * @brief Asynchronously load stack from file
     * @param filename File to load from
     * @param handler Completion handler
     */
    void asyncLoadStackFromFile(
        const String& filename,
        const std::function<void(const std::error_code&)>& handler);

    /**
     * @brief Coroutine version of load stack from file
     * @param filename File to load from
     * @return Task for completion
     */
    [[nodiscard]] auto loadStackFromFile(const String& filename) -> Task<void>;

    /**
     * @brief Get stack size
     * @return Number of directories in stack
     */
    [[nodiscard]] auto size() const noexcept -> size_t;

    /**
     * @brief Check if stack is empty
     * @return True if empty
     */
    [[nodiscard]] auto isEmpty() const noexcept -> bool;

    /**
     * @brief Asynchronously get current directory
     * @param handler Completion handler
     */
    void asyncGetCurrentDirectory(
        const std::function<void(const std::filesystem::path&,
                                 const std::error_code&)>& handler) const;

    /**
     * @brief Coroutine version of get current directory
     * @return Task with current directory path
     */
    [[nodiscard]] auto getCurrentDirectory() const
        -> Task<std::filesystem::path>;

private:
    std::unique_ptr<DirectoryStackImpl> impl_;
};

}  // namespace atom::io

#endif  // ATOM_IO_FILESYSTEM_DIRECTORY_STACK_HPP
