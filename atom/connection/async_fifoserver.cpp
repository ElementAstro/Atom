#include "async_fifoserver.hpp"

#include <asio.hpp>
#include <filesystem>
#include <functional>
#include <future>
#include <memory>
#include <spdlog/spdlog.h>
#include <string>
#include <string_view>
#include <thread>

#ifdef _WIN32
#include <asio/windows/stream_handle.hpp>
#else
#include <asio/posix/stream_descriptor.hpp>
#include <fcntl.h>
#include <sys/stat.h>
#endif

namespace atom::async::connection {

class FifoServer::Impl {
public:
  explicit Impl(std::string_view fifoPath)
      : fifoPath_(fifoPath), io_context_(),
#ifdef _WIN32
        pipe_(io_context_),
#else
        pipe_(io_context_),
#endif
        running_(false) {
  }

  ~Impl() {
    stop();
    std::filesystem::remove(fifoPath_);
  }

  void start(MessageHandler handler) {
    if (running_) {
      return;
    }

    handler_ = std::move(handler);
    running_ = true;

#ifdef _WIN32
    // Windows-specific implementation for named pipes
#else
    if (mkfifo(fifoPath_.c_str(), 0666) == -1 && errno != EEXIST) {
      spdlog::error("Failed to create FIFO: {}", strerror(errno));
      throw std::runtime_error("Failed to create FIFO");
    }
#endif

    io_thread_ = std::thread([this] { io_context_.run(); });
    acceptConnection();
  }

  void stop() {
    if (!running_) {
      return;
    }

    running_ = false;
    io_context_.stop();
    if (io_thread_.joinable()) {
      io_thread_.join();
    }
  }

  void setClientHandler(ClientHandler handler) { clientHandler_ = std::move(handler); }

  void setErrorHandler(ErrorHandler handler) { errorHandler_ = std::move(handler); }

  auto write(std::string_view data) -> std::future<bool> {
    auto promise = std::make_shared<std::promise<bool>>();
    auto future = promise->get_future();

    asio::async_write(pipe_, asio::buffer(data),
                      [this, promise](const asio::error_code &ec, size_t) {
                        if (ec) {
                          if (errorHandler_) {
                            errorHandler_(ec);
                          }
                          promise->set_value(false);
                        } else {
                          promise->set_value(true);
                        }
                      });

    return future;
  }

  [[nodiscard]] auto isRunning() const -> bool { return running_; }

  [[nodiscard]] auto getPath() const -> std::string { return fifoPath_; }

  void cancel() { pipe_.cancel(); }

private:
  void acceptConnection() {
#ifdef _WIN32
    // Windows-specific implementation for named pipes
#else
    int fd = open(fifoPath_.c_str(), O_RDWR | O_NONBLOCK);
    if (fd == -1) {
      if (errorHandler_) {
        errorHandler_({errno, std::system_category()});
      }
      return;
    }
    pipe_.assign(fd);
#endif
    if (clientHandler_) {
      clientHandler_(ClientEvent::Connected);
    }
    readMessage();
  }

  void readMessage() {
    asio::async_read_until(
        pipe_, asio::dynamic_buffer(buffer_), '\n',
        [this](const asio::error_code &ec, size_t length) {
          if (!ec) {
            std::string message(buffer_.substr(0, length));
            buffer_.erase(0, length);
            if (handler_) {
              handler_(message);
            }
            readMessage(); // Continue reading
          } else {
            if (clientHandler_) {
              clientHandler_(ClientEvent::Disconnected);
            }
            if (ec != asio::error::eof) {
              if (errorHandler_) {
                errorHandler_(ec);
              }
            }
          }
        });
  }

  std::string fifoPath_;
  asio::io_context io_context_;
#ifdef _WIN32
  asio::windows::stream_handle pipe_;
#else
  asio::posix::stream_descriptor pipe_;
#endif
  std::thread io_thread_;
  std::string buffer_;
  MessageHandler handler_;
  ClientHandler clientHandler_;
  ErrorHandler errorHandler_;
  bool running_ = false;
};

FifoServer::FifoServer(std::string_view fifoPath)
    : pimpl_(std::make_unique<Impl>(fifoPath)) {}

FifoServer::~FifoServer() = default;

void FifoServer::start(MessageHandler handler) { pimpl_->start(handler); }

void FifoServer::stop() { pimpl_->stop(); }

void FifoServer::setClientHandler(ClientHandler handler) {
  pimpl_->setClientHandler(std::move(handler));
}

void FifoServer::setErrorHandler(ErrorHandler handler) {
  pimpl_->setErrorHandler(std::move(handler));
}

auto FifoServer::write(std::string_view data) -> std::future<bool> {
  return pimpl_->write(data);
}

auto FifoServer::writeSync(std::string_view data) -> bool {
  return write(data).get();
}

bool FifoServer::isRunning() const { return pimpl_->isRunning(); }

auto FifoServer::getPath() const -> std::string { return pimpl_->getPath(); }

void FifoServer::cancel() { pimpl_->cancel(); }

} // namespace atom::async::connection
