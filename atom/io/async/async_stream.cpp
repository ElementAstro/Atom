#include "async_stream.hpp"

namespace atom::io::async {

#ifdef ATOM_USE_ASIO
AsyncStreamOps::AsyncStreamOps(asio::io_context& io_context,
                               std::shared_ptr<AsyncContext> context) noexcept
    : file_impl_(
          std::make_shared<AsyncFile>(io_context, std::move(context))) {}
#else
AsyncStreamOps::AsyncStreamOps(std::shared_ptr<AsyncContext> context) noexcept
    : file_impl_(std::make_shared<AsyncFile>(std::move(context))) {}
#endif

// Explicit template instantiations for common path types
template void AsyncStreamOps::asyncStreamRead<std::string>(
    std::string&&, size_t,
    std::function<void(AsyncResult<std::string>)>,
    std::function<void(AsyncResult<void>)>);
template void AsyncStreamOps::asyncStreamRead<std::string_view>(
    std::string_view&&, size_t,
    std::function<void(AsyncResult<std::string>)>,
    std::function<void(AsyncResult<void>)>);
template void AsyncStreamOps::asyncStreamRead<const char*>(
    const char*&&, size_t,
    std::function<void(AsyncResult<std::string>)>,
    std::function<void(AsyncResult<void>)>);
template void AsyncStreamOps::asyncStreamRead<std::filesystem::path>(
    std::filesystem::path&&, size_t,
    std::function<void(AsyncResult<std::string>)>,
    std::function<void(AsyncResult<void>)>);

template void AsyncStreamOps::asyncStreamWrite<std::string>(
    std::string&&, std::span<const char>, size_t,
    std::function<void(AsyncResult<void>)>);
template void AsyncStreamOps::asyncStreamWrite<std::string_view>(
    std::string_view&&, std::span<const char>, size_t,
    std::function<void(AsyncResult<void>)>);
template void AsyncStreamOps::asyncStreamWrite<const char*>(
    const char*&&, std::span<const char>, size_t,
    std::function<void(AsyncResult<void>)>);
template void AsyncStreamOps::asyncStreamWrite<std::filesystem::path>(
    std::filesystem::path&&, std::span<const char>, size_t,
    std::function<void(AsyncResult<void>)>);

}  // namespace atom::io::async
