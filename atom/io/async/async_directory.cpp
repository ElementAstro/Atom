#include "async_directory.hpp"

namespace atom::io::async {

#ifdef ATOM_USE_ASIO
AsyncDirectoryOps::AsyncDirectoryOps(
    asio::io_context& io_context,
    std::shared_ptr<AsyncContext> context) noexcept
    : file_impl_(std::make_shared<AsyncFile>(io_context, std::move(context))) {}
#else
AsyncDirectoryOps::AsyncDirectoryOps(
    std::shared_ptr<AsyncContext> context) noexcept
    : file_impl_(std::make_shared<AsyncFile>(std::move(context))) {}
#endif

// Legacy AsyncDirectory implementation
#ifdef ATOM_USE_ASIO
AsyncDirectory::AsyncDirectory(asio::io_context& io_context) noexcept
    : file_impl_(std::make_unique<AsyncFile>(io_context)) {}
#else
AsyncDirectory::AsyncDirectory() noexcept
    : file_impl_(std::make_unique<AsyncFile>()) {}
#endif

// Directory operations instantiations for AsyncDirectoryOps
template void AsyncDirectoryOps::asyncCreateDirectory<std::string>(
    std::string&&, std::function<void(AsyncResult<void>)>);
template void AsyncDirectoryOps::asyncCreateDirectory<std::string_view>(
    std::string_view&&, std::function<void(AsyncResult<void>)>);
template void AsyncDirectoryOps::asyncCreateDirectory<const char*>(
    const char*&&, std::function<void(AsyncResult<void>)>);
template void AsyncDirectoryOps::asyncCreateDirectory<std::filesystem::path>(
    std::filesystem::path&&, std::function<void(AsyncResult<void>)>);

template void AsyncDirectoryOps::asyncRemoveDirectory<std::string>(
    std::string&&, std::function<void(AsyncResult<void>)>);
template void AsyncDirectoryOps::asyncRemoveDirectory<std::string_view>(
    std::string_view&&, std::function<void(AsyncResult<void>)>);
template void AsyncDirectoryOps::asyncRemoveDirectory<const char*>(
    const char*&&, std::function<void(AsyncResult<void>)>);
template void AsyncDirectoryOps::asyncRemoveDirectory<std::filesystem::path>(
    std::filesystem::path&&, std::function<void(AsyncResult<void>)>);

template void AsyncDirectoryOps::asyncListDirectory<std::string>(
    std::string&&,
    std::function<void(AsyncResult<std::vector<std::filesystem::path>>)>);
template void AsyncDirectoryOps::asyncListDirectory<std::string_view>(
    std::string_view&&,
    std::function<void(AsyncResult<std::vector<std::filesystem::path>>)>);
template void AsyncDirectoryOps::asyncListDirectory<const char*>(
    const char*&&,
    std::function<void(AsyncResult<std::vector<std::filesystem::path>>)>);
template void AsyncDirectoryOps::asyncListDirectory<std::filesystem::path>(
    std::filesystem::path&&,
    std::function<void(AsyncResult<std::vector<std::filesystem::path>>)>);

}  // namespace atom::io::async
