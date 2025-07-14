#include "async_fifoclient.hpp"

#include <spdlog/spdlog.h>
#include <asio.hpp>
#include <chrono>
#include <future>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <thread>

#ifdef _WIN32
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#endif

namespace atom::async::connection {

struct FifoClient::Impl {
    asio::io_context io_context_;
    std::thread io_thread_;
    std::string fifoPath_;
#ifdef _WIN32
    asio::windows::stream_handle pipe_;
#else
    asio::posix::stream_descriptor pipe_;
#endif

    explicit Impl()
#ifdef _WIN32
        : pipe_(io_context_)
#else
        : pipe_(io_context_)
#endif
    {
    }

    explicit Impl(std::string_view fifoPath)
#ifdef _WIN32
        : pipe_(io_context_),
#else
        : pipe_(io_context_),
#endif
          io_thread_([this] { io_context_.run(); }) {
        open(fifoPath);
    }

    ~Impl() {
        io_context_.stop();
        if (io_thread_.joinable()) {
            io_thread_.join();
        }
        close();
    }

    void open(std::string_view fifoPath) {
        if (isOpen()) {
            throw std::runtime_error("FIFO is already open");
        }
        fifoPath_ = fifoPath;
#ifdef _WIN32
        HANDLE handle =
            CreateFileA(fifoPath_.c_str(), GENERIC_READ | GENERIC_WRITE, 0,
                        nullptr, OPEN_EXISTING, 0, nullptr);
        if (handle == INVALID_HANDLE_VALUE) {
            spdlog::error("Failed to open FIFO: {}", GetLastError());
            throw std::runtime_error("Failed to open FIFO");
        }
        pipe_.assign(handle);
#else
        if (mkfifo(fifoPath_.c_str(), 0666) == -1 && errno != EEXIST) {
            spdlog::error("Failed to create FIFO: {}", strerror(errno));
            throw std::runtime_error("Failed to create FIFO");
        }
        int fd = ::open(fifoPath_.c_str(), O_RDWR | O_NONBLOCK);
        if (fd == -1) {
            spdlog::error("Failed to open FIFO: {}", strerror(errno));
            throw std::runtime_error("Failed to open FIFO");
        }
        pipe_.assign(fd);
#endif
        spdlog::info("FIFO opened successfully: {}", fifoPath_);
        if (!io_thread_.joinable()) {
            io_thread_ = std::thread([this] { io_context_.run(); });
        }
    }

    auto isOpen() const -> bool { return pipe_.is_open(); }

    void close() {
        if (isOpen()) {
            asio::error_code ec;
            if (pipe_.close(ec)) {
                spdlog::info("FIFO closed successfully.");
            }
            if (ec) {
                spdlog::error("Failed to close FIFO: {}", ec.message());
            }
        }
    }

    void cancel() { pipe_.cancel(); }

    auto getPath() const -> std::string { return fifoPath_; }

    auto write(std::string_view data,
               const std::optional<std::chrono::milliseconds> &timeout)
        -> std::future<bool> {
        auto promise = std::make_shared<std::promise<bool>>();
        auto future = promise->get_future();

        asio::async_write(pipe_, asio::buffer(data),
                          [promise](const asio::error_code &ec, size_t) {
                              if (ec) {
                                  spdlog::error("Write error: {}",
                                                ec.message());
                                  promise->set_value(false);
                              } else {
                                  promise->set_value(true);
                              }
                          });

        if (timeout) {
            auto timer = std::make_shared<asio::steady_timer>(io_context_);
            timer->expires_after(*timeout);
            timer->async_wait([promise, timer](const asio::error_code &ec) {
                if (!ec) {
                    promise->set_value(false);
                }
            });
        }

        return future;
    }

    auto read(const std::optional<std::chrono::milliseconds> &timeout)
        -> std::future<std::optional<std::string>> {
        auto promise =
            std::make_shared<std::promise<std::optional<std::string>>>();
        auto future = promise->get_future();
        auto buffer = std::make_shared<asio::streambuf>();

        asio::async_read_until(
            pipe_, *buffer, '\n',
            [promise, buffer](const asio::error_code &ec,
                              size_t bytes_transferred) {
                if (!ec) {
                    std::string data(asio::buffers_begin(buffer->data()),
                                     asio::buffers_begin(buffer->data()) +
                                         bytes_transferred);
                    promise->set_value(data);
                } else if (ec == asio::error::eof) {
                    promise->set_value(std::nullopt);
                } else {
                    spdlog::error("Read error: {}", ec.message());
                    promise->set_value(std::nullopt);
                }
            });

        if (timeout) {
            auto timer = std::make_shared<asio::steady_timer>(io_context_);
            timer->expires_after(*timeout);
            timer->async_wait([promise, timer](const asio::error_code &ec) {
                if (!ec) {
                    promise->set_value(std::nullopt);
                }
            });
        }

        return future;
    }
};

FifoClient::FifoClient() : pimpl_(std::make_unique<Impl>()) {}

FifoClient::FifoClient(std::string_view fifoPath)
    : pimpl_(std::make_unique<Impl>(fifoPath)) {}

FifoClient::~FifoClient() = default;

void FifoClient::open(std::string_view fifoPath) { pimpl_->open(fifoPath); }

auto FifoClient::write(std::string_view data,
                       std::optional<std::chrono::milliseconds> timeout)
    -> std::future<bool> {
    return pimpl_->write(data, timeout);
}

auto FifoClient::writeSync(std::string_view data,
                           std::optional<std::chrono::milliseconds> timeout)
    -> bool {
    return write(data, timeout).get();
}

auto FifoClient::read(std::optional<std::chrono::milliseconds> timeout)
    -> std::future<std::optional<std::string>> {
    return pimpl_->read(timeout);
}

auto FifoClient::readSync(std::optional<std::chrono::milliseconds> timeout)
    -> std::optional<std::string> {
    return read(timeout).get();
}

auto FifoClient::isOpen() const -> bool { return pimpl_->isOpen(); }

void FifoClient::close() { pimpl_->close(); }

void FifoClient::cancel() { pimpl_->cancel(); }

auto FifoClient::getPath() const -> std::string { return pimpl_->getPath(); }

}  // namespace atom::async::connection
