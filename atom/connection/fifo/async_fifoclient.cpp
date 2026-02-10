#include "async_fifoclient.hpp"

#include <asio.hpp>
#include <iostream>
#include <string>
#include <system_error>

#include "fifo_platform.hpp"

namespace atom::connection {

struct AsyncFifoClient::Impl {
    asio::io_context io_context;
    FifoHandle handle;
    std::string fifoPath;
    asio::steady_timer timer;
    ClientConfig config;
    FifoStats stats;
    mutable std::mutex mutex;

    Impl(std::string_view path, const ClientConfig& cfg = {})
        : fifoPath(path), timer(io_context), config(cfg) {
        openFifo();
    }

    ~Impl() { close(); }

    void openFifo() {
        // Use FifoPlatform for cross-platform FIFO operations
        auto createResult = FifoPlatform::createFifo(fifoPath);
        if (!createResult) {
            // Continue even if create fails (may already exist)
        }

        auto openResult = FifoPlatform::openFifo(
            fifoPath, FifoPlatform::OpenMode::ReadWrite, true);
        if (!openResult) {
            throw std::runtime_error("Failed to open FIFO pipe");
        }
        handle.reset(*openResult);
    }

    bool isOpen() const { return handle.isValid(); }

    void close() { handle.close(); }

    bool write(std::string_view data,
               const std::optional<std::chrono::milliseconds>& timeout) {
        if (!isOpen())
            return false;

        auto result = FifoPlatform::write(handle.get(), data.data(),
                                          data.size(), timeout);
        if (result) {
            std::lock_guard<std::mutex> lock(mutex);
            stats.bytes_sent += *result;
            stats.messages_sent++;
            return true;
        }
        return false;
    }

    std::optional<std::string> read(
        const std::optional<std::chrono::milliseconds>& timeout) {
        if (!isOpen())
            return std::nullopt;

        std::array<char, 4096> buffer{};
        auto result = FifoPlatform::read(handle.get(), buffer.data(),
                                         buffer.size(), timeout);

        if (result && *result > 0) {
            std::lock_guard<std::mutex> lock(mutex);
            stats.bytes_received += *result;
            return std::string(buffer.data(), *result);
        }
        return std::nullopt;
    }
};

AsyncFifoClient::AsyncFifoClient(std::string fifoPath)
    : m_impl(std::make_unique<Impl>(fifoPath)) {}

AsyncFifoClient::AsyncFifoClient(std::string fifoPath,
                                 const ClientConfig& config)
    : m_impl(std::make_unique<Impl>(fifoPath, config)) {}

AsyncFifoClient::~AsyncFifoClient() = default;

AsyncFifoClient::AsyncFifoClient(AsyncFifoClient&&) noexcept = default;
AsyncFifoClient& AsyncFifoClient::operator=(AsyncFifoClient&&) noexcept =
    default;

auto AsyncFifoClient::write(std::string_view data,
                            std::optional<std::chrono::milliseconds> timeout)
    -> FifoResult<size_t> {
    if (!m_impl || !m_impl->isOpen()) {
        return type::unexpected(make_error_code(FifoError::NotOpen));
    }
    bool success = m_impl->write(data, timeout);
    if (success) {
        return data.size();
    }
    return type::unexpected(make_error_code(FifoError::WriteFailed));
}

auto AsyncFifoClient::read(std::optional<std::chrono::milliseconds> timeout)
    -> FifoResult<std::string> {
    if (!m_impl || !m_impl->isOpen()) {
        return type::unexpected(make_error_code(FifoError::NotOpen));
    }
    auto result = m_impl->read(timeout);
    if (result) {
        return *result;
    }
    return type::unexpected(make_error_code(FifoError::ReadFailed));
}

bool AsyncFifoClient::isOpen() const { return m_impl && m_impl->isOpen(); }

auto AsyncFifoClient::getPath() const -> std::string {
    return m_impl ? m_impl->fifoPath : "";
}

void AsyncFifoClient::close() {
    if (m_impl) {
        m_impl->close();
    }
}

auto AsyncFifoClient::getConfig() const -> ClientConfig {
    return m_impl ? m_impl->config : ClientConfig{};
}

auto AsyncFifoClient::updateConfig(const ClientConfig& config) -> bool {
    if (m_impl) {
        std::lock_guard<std::mutex> lock(m_impl->mutex);
        m_impl->config = config;
        return true;
    }
    return false;
}

auto AsyncFifoClient::getStatistics() const -> FifoStats {
    return m_impl ? m_impl->stats : FifoStats{};
}

void AsyncFifoClient::resetStatistics() {
    if (m_impl) {
        std::lock_guard<std::mutex> lock(m_impl->mutex);
        m_impl->stats.reset();
    }
}

}  // namespace atom::connection
