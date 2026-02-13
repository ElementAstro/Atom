#include "ttybase.hpp"

#include <array>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <condition_variable>
#include <cstring>
#include <functional>
#include <mutex>
#include <optional>
#include <queue>
#include <shared_mutex>
#include <stdexcept>
#include <system_error>
#include <thread>
#include <utility>
#include <vector>

#ifdef _WIN32
// clang-format off
#include <winsock2.h>
#include <windows.h>
#include <io.h>
// clang-format on
#else
#include <fcntl.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>
#endif

#include <spdlog/spdlog.h>
#include "atom/error/exception.hpp"

namespace atom::connection {

namespace {

#ifndef _WIN32
// Baud rate lookup table for Unix platforms
struct BaudRateEntry {
    uint32_t rate;
    speed_t speed;
};

constexpr std::array<BaudRateEntry, 20> BAUD_RATE_TABLE = {{
    {0, B0},         {50, B50},         {75, B75},         {110, B110},
    {134, B134},     {150, B150},       {200, B200},       {300, B300},
    {600, B600},     {1200, B1200},     {1800, B1800},     {2400, B2400},
    {4800, B4800},   {9600, B9600},     {19200, B19200},   {38400, B38400},
    {57600, B57600}, {115200, B115200}, {230400, B230400},
}};

[[nodiscard]] constexpr auto findBaudRate(uint32_t bitRate) noexcept
    -> std::optional<speed_t> {
    for (const auto& entry : BAUD_RATE_TABLE) {
        if (entry.rate == bitRate) {
            return entry.speed;
        }
    }
    return std::nullopt;
}
#endif

}  // anonymous namespace

class TTYBase::Impl {
public:
    explicit Impl(std::string_view driverName)
        : m_PortFD(-1),
          m_Debug(false),
          m_DriverName(std::string(driverName)),
          m_IsRunning(false),
          m_ShouldExit(false) {}

    ~Impl() noexcept {
        // Signal exit first without holding lock to avoid deadlock
        m_ShouldExit.store(true, std::memory_order_release);

        // Join thread outside of any lock
        if (m_WorkerThread.joinable()) {
            m_WorkerThread.join();
        }

        m_IsRunning.store(false, std::memory_order_release);

        // Now safe to disconnect
        if (m_PortFD != -1) {
            (void)disconnectInternal();
        }
    }

    // Helper: Set Windows comm timeouts
#ifdef _WIN32
    [[nodiscard]]
    bool setWindowsTimeouts(HANDLE hPort, uint32_t timeoutMs) noexcept {
        COMMTIMEOUTS timeouts = {};
        timeouts.ReadIntervalTimeout = timeoutMs;
        timeouts.ReadTotalTimeoutConstant = timeoutMs;
        timeouts.ReadTotalTimeoutMultiplier = 0;
        timeouts.WriteTotalTimeoutConstant = timeoutMs;
        timeouts.WriteTotalTimeoutMultiplier = 0;
        return SetCommTimeouts(hPort, &timeouts) != 0;
    }
#endif

    // Helper: Validate port is open
    [[nodiscard]]
    bool validatePortOpen() const noexcept {
        return m_PortFD != -1;
    }

    // Helper: Log debug message
    template <typename... Args>
    void debugLog(spdlog::level::level_enum level,
                  fmt::format_string<Args...> fmt, Args&&... args) {
        if (m_Debug) {
            spdlog::log(level, fmt, std::forward<Args>(args)...);
        }
    }

    [[nodiscard]]
    TTYResponse checkTimeout(uint8_t timeout) {
        if (!validatePortOpen()) {
            return TTYResponse::Errno;
        }

#ifdef _WIN32
        auto hPort = reinterpret_cast<HANDLE>(m_PortFD);
        if (!setWindowsTimeouts(hPort, static_cast<uint32_t>(timeout) * 1000)) {
            return TTYResponse::Errno;
        }
        return TTYResponse::OK;
#else
        fd_set readout;
        FD_ZERO(&readout);
        FD_SET(m_PortFD, &readout);

        struct timeval tv = {timeout, 0};
        int retval = select(m_PortFD + 1, &readout, nullptr, nullptr, &tv);

        if (retval > 0) {
            return TTYResponse::OK;
        }
        if (retval == -1) {
            if (errno == EINTR) {
                debugLog(spdlog::level::info, "select() interrupted by signal");
                return TTYResponse::Timeout;
            }
            debugLog(spdlog::level::err, "select() error: {}", strerror(errno));
            return TTYResponse::SelectError;
        }
        return TTYResponse::Timeout;
#endif
    }

    [[nodiscard]]
    TTYResponse read(std::span<uint8_t> buffer, uint8_t timeout,
                     uint32_t& nbytesRead) {
        if (buffer.empty()) {
            return TTYResponse::ParamError;
        }

        if (!validatePortOpen()) {
            nbytesRead = 0;
            return TTYResponse::Errno;
        }

        nbytesRead = 0;
        const auto nbytes = static_cast<uint32_t>(buffer.size());

#ifdef _WIN32
        auto hPort = reinterpret_cast<HANDLE>(m_PortFD);
        if (!setWindowsTimeouts(hPort, static_cast<uint32_t>(timeout) * 1000)) {
            return TTYResponse::Errno;
        }

        DWORD bytesRead = 0;
        if (!ReadFile(hPort, buffer.data(), nbytes, &bytesRead, nullptr)) {
            debugLog(spdlog::level::err, "ReadFile error: {}", GetLastError());
            return TTYResponse::ReadError;
        }

        nbytesRead = bytesRead;
        return TTYResponse::OK;
#else
        uint32_t numBytesToRead = nbytes;

        while (numBytesToRead > 0) {
            if (auto response = checkTimeout(timeout);
                response != TTYResponse::OK) {
                if (response == TTYResponse::Timeout) {
                    debugLog(spdlog::level::info,
                             "Read operation timed out after reading {} bytes",
                             nbytesRead);
                }
                return response;
            }

            auto bytesRead =
                ::read(m_PortFD, buffer.data() + nbytesRead, numBytesToRead);

            if (bytesRead < 0) {
                if (errno == EINTR) {
                    continue;  // Interrupted, retry
                }
                debugLog(spdlog::level::err, "Read error: {}", strerror(errno));
                return TTYResponse::ReadError;
            }

            if (bytesRead == 0) {
                break;  // EOF
            }

            nbytesRead += static_cast<uint32_t>(bytesRead);
            numBytesToRead -= static_cast<uint32_t>(bytesRead);
        }

        return TTYResponse::OK;
#endif
    }

    [[nodiscard]]
    TTYResponse readSection(std::span<uint8_t> buffer, uint8_t stopByte,
                            uint8_t timeout, uint32_t& nbytesRead) {
        if (buffer.empty()) {
            return TTYResponse::ParamError;
        }

        if (!validatePortOpen()) {
            nbytesRead = 0;
            return TTYResponse::Errno;
        }

        nbytesRead = 0;
        const auto nsize = static_cast<uint32_t>(buffer.size());

#ifdef _WIN32
        auto hPort = reinterpret_cast<HANDLE>(m_PortFD);
        if (!setWindowsTimeouts(hPort, static_cast<uint32_t>(timeout) * 1000)) {
            return TTYResponse::Errno;
        }

        while (nbytesRead < nsize) {
            uint8_t readChar = 0;
            DWORD bytesRead = 0;

            if (!ReadFile(hPort, &readChar, 1, &bytesRead, nullptr)) {
                debugLog(spdlog::level::err,
                         "ReadFile error in readSection: {}", GetLastError());
                return TTYResponse::ReadError;
            }

            if (bytesRead == 0) {
                return TTYResponse::Timeout;
            }

            buffer[nbytesRead++] = readChar;

            if (readChar == stopByte) {
                return TTYResponse::OK;
            }
        }

        return TTYResponse::Overflow;
#else
        while (nbytesRead < nsize) {
            if (auto response = checkTimeout(timeout);
                response != TTYResponse::OK) {
                return response;
            }

            uint8_t readChar = 0;
            auto bytesRead = ::read(m_PortFD, &readChar, 1);

            if (bytesRead < 0) {
                if (errno == EINTR) {
                    continue;
                }
                debugLog(spdlog::level::err, "Read error in readSection: {}",
                         strerror(errno));
                return TTYResponse::ReadError;
            }

            if (bytesRead == 0) {
                break;
            }

            buffer[nbytesRead++] = readChar;

            if (readChar == stopByte) {
                return TTYResponse::OK;
            }
        }

        return TTYResponse::Overflow;
#endif
    }

    [[nodiscard]]
    TTYResponse write(std::span<const uint8_t> buffer,
                      uint32_t& nbytesWritten) {
        if (buffer.empty()) {
            nbytesWritten = 0;
            return TTYResponse::OK;
        }

        if (!validatePortOpen()) {
            nbytesWritten = 0;
            return TTYResponse::Errno;
        }

#ifdef _WIN32
        auto hPort = reinterpret_cast<HANDLE>(m_PortFD);
        DWORD bytesWritten = 0;

        if (!WriteFile(hPort, buffer.data(), static_cast<DWORD>(buffer.size()),
                       &bytesWritten, nullptr)) {
            debugLog(spdlog::level::err, "WriteFile error: {}", GetLastError());
            return TTYResponse::WriteError;
        }

        nbytesWritten = bytesWritten;
        return TTYResponse::OK;
#else
        nbytesWritten = 0;
        auto remaining = static_cast<uint32_t>(buffer.size());

        while (remaining > 0) {
            auto bytesW =
                ::write(m_PortFD, buffer.data() + nbytesWritten, remaining);

            if (bytesW < 0) {
                if (errno == EINTR) {
                    continue;  // Interrupted, retry
                }
                debugLog(spdlog::level::err, "Write error: {}",
                         strerror(errno));
                return TTYResponse::WriteError;
            }

            nbytesWritten += static_cast<uint32_t>(bytesW);
            remaining -= static_cast<uint32_t>(bytesW);
        }

        return TTYResponse::OK;
#endif
    }

    [[nodiscard]]
    TTYResponse connect(std::string_view device, uint32_t bitRate,
                        uint8_t wordSize, uint8_t parity, uint8_t stopBits) {
        // Validate parameters
        if (device.empty()) {
            debugLog(spdlog::level::err, "Device name cannot be empty");
            return TTYResponse::ParamError;
        }

        if (wordSize < 5 || wordSize > 8) {
            debugLog(spdlog::level::err,
                     "Word size must be between 5 and 8 bits");
            return TTYResponse::ParamError;
        }

        if (parity > 2) {
            debugLog(spdlog::level::err, "Invalid parity value: {}", parity);
            return TTYResponse::ParamError;
        }

        if (stopBits != 1 && stopBits != 2) {
            debugLog(spdlog::level::err, "Stop bits must be 1 or 2");
            return TTYResponse::ParamError;
        }

#ifdef _WIN32
        return connectWindows(device, bitRate, wordSize, parity, stopBits);
#else
        return connectUnix(device, bitRate, wordSize, parity, stopBits);
#endif
    }

private:
#ifdef _WIN32
    [[nodiscard]]
    TTYResponse connectWindows(std::string_view device, uint32_t bitRate,
                               uint8_t wordSize, uint8_t parity,
                               uint8_t stopBits) {
        std::string devicePath(device);

        // Handle COM ports > 9
        if (devicePath.find("COM") != std::string::npos &&
            devicePath.find("\\\\.\\") != 0) {
            try {
                if (std::stoi(devicePath.substr(3)) > 9) {
                    devicePath = "\\\\.\\" + devicePath;
                }
            } catch (...) {
                // Not a standard COM port format, use as-is
            }
        }

        HANDLE hSerial =
            CreateFileA(devicePath.c_str(), GENERIC_READ | GENERIC_WRITE, 0,
                        nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);

        if (hSerial == INVALID_HANDLE_VALUE) {
            debugLog(spdlog::level::err,
                     "Failed to open port {}: Error code {}", devicePath,
                     GetLastError());
            return TTYResponse::PortFailure;
        }

        DCB dcbSerialParams = {};
        dcbSerialParams.DCBlength = sizeof(dcbSerialParams);

        if (!GetCommState(hSerial, &dcbSerialParams)) {
            CloseHandle(hSerial);
            debugLog(spdlog::level::err, "Failed to get comm state for {}",
                     devicePath);
            return TTYResponse::PortFailure;
        }

        dcbSerialParams.BaudRate = bitRate;
        dcbSerialParams.ByteSize = wordSize;
        dcbSerialParams.StopBits = (stopBits == 1) ? ONESTOPBIT : TWOSTOPBITS;

        // Parity: 0=None, 1=Even, 2=Odd
        constexpr BYTE parityMap[] = {NOPARITY, EVENPARITY, ODDPARITY};
        dcbSerialParams.Parity = parityMap[parity];

        dcbSerialParams.fOutxCtsFlow = FALSE;
        dcbSerialParams.fRtsControl = RTS_CONTROL_DISABLE;
        dcbSerialParams.fOutX = FALSE;
        dcbSerialParams.fInX = FALSE;

        if (!SetCommState(hSerial, &dcbSerialParams)) {
            auto error = GetLastError();
            CloseHandle(hSerial);
            debugLog(spdlog::level::err,
                     "Failed to set comm state for {}: Error {}", devicePath,
                     error);
            return TTYResponse::PortFailure;
        }

        // Set default timeouts (non-blocking reads)
        COMMTIMEOUTS timeouts = {};
        timeouts.ReadIntervalTimeout = MAXDWORD;
        timeouts.ReadTotalTimeoutMultiplier = 0;
        timeouts.ReadTotalTimeoutConstant = 0;
        timeouts.WriteTotalTimeoutMultiplier = 0;
        timeouts.WriteTotalTimeoutConstant = 0;

        if (!SetCommTimeouts(hSerial, &timeouts)) {
            CloseHandle(hSerial);
            debugLog(spdlog::level::err, "Failed to set comm timeouts for {}",
                     devicePath);
            return TTYResponse::PortFailure;
        }

        m_PortFD = reinterpret_cast<intptr_t>(hSerial);
        return TTYResponse::OK;
    }
#else
    [[nodiscard]]
    TTYResponse connectUnix(std::string_view device, uint32_t bitRate,
                            uint8_t wordSize, uint8_t parity,
                            uint8_t stopBits) {
        int tFd =
            open(std::string(device).c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK);
        if (tFd == -1) {
            debugLog(spdlog::level::err, "Error opening {}: {}", device,
                     strerror(errno));
            return TTYResponse::PortFailure;
        }

        // Clear O_NONBLOCK flag for blocking I/O
        int flags = fcntl(tFd, F_GETFL, 0);
        if (flags == -1 || fcntl(tFd, F_SETFL, flags & ~O_NONBLOCK) == -1) {
            debugLog(spdlog::level::err, "Error clearing O_NONBLOCK flag: {}",
                     strerror(errno));
            close(tFd);
            return TTYResponse::PortFailure;
        }

        termios ttySetting{};
        if (tcgetattr(tFd, &ttySetting) == -1) {
            debugLog(spdlog::level::err, "Error getting {} tty attributes: {}",
                     device, strerror(errno));
            close(tFd);
            return TTYResponse::PortFailure;
        }

        // Use lookup table for baud rate
        auto bps = findBaudRate(bitRate);
        if (!bps) {
            debugLog(spdlog::level::err, "connect: {} is not a valid bit rate",
                     bitRate);
            close(tFd);
            return TTYResponse::ParamError;
        }

        if (cfsetispeed(&ttySetting, *bps) < 0 ||
            cfsetospeed(&ttySetting, *bps) < 0) {
            debugLog(spdlog::level::err, "connect: failed setting bit rate: {}",
                     strerror(errno));
            close(tFd);
            return TTYResponse::PortFailure;
        }

        ttySetting.c_cflag &=
            ~(CSIZE | CSTOPB | PARENB | PARODD | HUPCL | CRTSCTS);
        ttySetting.c_cflag |= (CLOCAL | CREAD);

        // Word size mapping
        constexpr tcflag_t wordSizeMap[] = {CS5, CS6, CS7, CS8};
        ttySetting.c_cflag |= wordSizeMap[wordSize - 5];

        // Parity
        if (parity == 1) {
            ttySetting.c_cflag |= PARENB;
        } else if (parity == 2) {
            ttySetting.c_cflag |= PARENB | PARODD;
        }

        // Stop bits
        if (stopBits == 2) {
            ttySetting.c_cflag |= CSTOPB;
        }

        ttySetting.c_iflag &=
            ~(PARMRK | ISTRIP | IGNCR | ICRNL | INLCR | IXOFF | IXON | IXANY);
        ttySetting.c_iflag |= INPCK | IGNPAR | IGNBRK;

        // Raw output
        ttySetting.c_oflag &= ~(OPOST | ONLCR);

        ttySetting.c_lflag &=
            ~(ICANON | ECHO | ECHOE | ISIG | IEXTEN | NOFLSH | TOSTOP);
        ttySetting.c_lflag |= NOFLSH;

        ttySetting.c_cc[VMIN] = 1;
        ttySetting.c_cc[VTIME] = 0;

        tcflush(tFd, TCIOFLUSH);
        cfmakeraw(&ttySetting);

        if (tcsetattr(tFd, TCSANOW, &ttySetting) != 0) {
            debugLog(spdlog::level::err,
                     "Failed to set terminal attributes: {}", strerror(errno));
            close(tFd);
            return TTYResponse::PortFailure;
        }

        m_PortFD = tFd;
        return TTYResponse::OK;
    }
#endif

public:
    [[nodiscard]]
    TTYResponse disconnect() noexcept {
        stopAsyncRead();
        return disconnectInternal();
    }

private:
    // Internal disconnect without stopping async read (used by destructor)
    [[nodiscard]]
    TTYResponse disconnectInternal() noexcept {
        if (m_PortFD == -1) {
            return TTYResponse::OK;  // Already disconnected
        }

#ifdef _WIN32
        auto hPort = reinterpret_cast<HANDLE>(m_PortFD);
        if (!CloseHandle(hPort)) {
            debugLog(spdlog::level::err, "Error closing handle: {}",
                     GetLastError());
            m_PortFD = -1;
            return TTYResponse::Errno;
        }
        m_PortFD = -1;
        return TTYResponse::OK;
#else
        tcflush(m_PortFD, TCIOFLUSH);

        if (close(m_PortFD) != 0) {
            debugLog(spdlog::level::err, "Error closing port: {}",
                     strerror(errno));
            m_PortFD = -1;
            return TTYResponse::Errno;
        }
        m_PortFD = -1;
        return TTYResponse::OK;
#endif
    }

public:
    void setDebug(bool enabled) noexcept {
        m_Debug = enabled;
        debugLog(spdlog::level::info, "Debugging {} for {}",
                 enabled ? "enabled" : "disabled", m_DriverName);
    }

    [[nodiscard]]
    std::string getErrorMessage(TTYResponse code) const noexcept {
        switch (code) {
            case TTYResponse::OK:
                return "No error";
            case TTYResponse::ReadError:
                return "Read error: " + std::string(strerror(errno));
            case TTYResponse::WriteError:
                return "Write error: " + std::string(strerror(errno));
            case TTYResponse::SelectError:
                return "Select error: " + std::string(strerror(errno));
            case TTYResponse::Timeout:
                return "Timeout error";
            case TTYResponse::PortFailure:
#ifndef _WIN32
                if (errno == EACCES) {
                    return "Port failure: Access denied. Try adding your user "
                           "to the dialout group (sudo adduser $USER dialout)";
                }
#endif
                return "Port failure: " + std::string(strerror(errno)) +
                       ". Check if device is connected to this port.";
            case TTYResponse::ParamError:
                return "Parameter error";
            case TTYResponse::Errno:
                return "Error: " + std::string(strerror(errno));
            case TTYResponse::Overflow:
                return "Read overflow error";
            default:
                return "Unknown error";
        }
    }

    [[nodiscard]]
    int getPortFD() const noexcept {
        return m_PortFD;
    }

    [[nodiscard]]
    bool isConnected() const noexcept {
        return m_PortFD != -1;
    }

    void startAsyncRead() {
        if (m_IsRunning.load(std::memory_order_acquire) || m_PortFD == -1) {
            return;
        }

        m_IsRunning.store(true, std::memory_order_release);
        m_ShouldExit.store(false, std::memory_order_release);

        m_WorkerThread = std::thread([this]() { asyncReadLoop(); });

        debugLog(spdlog::level::info, "Started async operations for {}",
                 m_DriverName);
    }

    void stopAsyncRead() {
        if (!m_IsRunning.load(std::memory_order_acquire)) {
            return;
        }

        m_ShouldExit.store(true, std::memory_order_release);
        m_IsRunning.store(false, std::memory_order_release);

        // Notify waiting threads
        m_AsyncCV.notify_all();

        if (m_WorkerThread.joinable()) {
            m_WorkerThread.join();
        }

        // Clear data queue
        {
            std::lock_guard<std::mutex> asyncLock(m_AsyncMutex);
            std::queue<std::vector<uint8_t>> empty;
            std::swap(m_DataQueue, empty);
        }

        debugLog(spdlog::level::info, "Stopped async operations for {}",
                 m_DriverName);
    }

    void setDataCallback(
        std::function<void(const std::vector<uint8_t>&, size_t)> callback) {
        std::lock_guard<std::shared_mutex> lock(m_CallbackMutex);
        m_DataCallback = std::move(callback);
    }

    [[nodiscard]]
    bool getQueuedData(std::vector<uint8_t>& data,
                       std::chrono::milliseconds timeout) {
        std::unique_lock<std::mutex> lock(m_AsyncMutex);

        if (m_DataQueue.empty()) {
            auto result = m_AsyncCV.wait_for(lock, timeout, [this]() {
                return !m_DataQueue.empty() ||
                       !m_IsRunning.load(std::memory_order_acquire);
            });

            if (!result || !m_IsRunning.load(std::memory_order_acquire)) {
                return false;
            }
        }

        if (!m_DataQueue.empty()) {
            data = std::move(m_DataQueue.front());
            m_DataQueue.pop();
            return true;
        }

        return false;
    }

    void setReadBufferSize(size_t size) {
        if (size > 0) {
            m_ReadBufferSize.store(size, std::memory_order_release);
        }
    }

private:
    // Async read worker loop
    void asyncReadLoop() {
        std::vector<uint8_t> buffer(
            m_ReadBufferSize.load(std::memory_order_acquire));

        while (!m_ShouldExit.load(std::memory_order_acquire)) {
            if (m_PortFD == -1) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                continue;
            }

#ifdef _WIN32
            // Windows: Use overlapped I/O or polling
            uint32_t bytesRead = 0;
            auto response = read(std::span<uint8_t>(buffer), 0, bytesRead);

            if (response == TTYResponse::OK && bytesRead > 0) {
                processAsyncData(buffer, bytesRead);
            } else {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
#else
            // Unix: Use select for efficient waiting
            fd_set readSet;
            FD_ZERO(&readSet);
            FD_SET(m_PortFD, &readSet);

            struct timeval tv = {0, 100000};  // 100ms timeout
            int result = select(m_PortFD + 1, &readSet, nullptr, nullptr, &tv);

            if (result > 0) {
                uint32_t bytesRead = 0;
                auto response = read(std::span<uint8_t>(buffer), 0, bytesRead);

                if (response == TTYResponse::OK && bytesRead > 0) {
                    processAsyncData(buffer, bytesRead);
                }
            } else if (result < 0 && errno != EINTR) {
                debugLog(spdlog::level::err, "Async read select error: {}",
                         strerror(errno));
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
#endif
        }
    }

    // Process data received in async read
    void processAsyncData(const std::vector<uint8_t>& buffer,
                          uint32_t bytesRead) {
        // Thread-safe callback access
        std::shared_lock<std::shared_mutex> lock(m_CallbackMutex);
        if (m_DataCallback) {
            m_DataCallback(buffer, bytesRead);
        } else {
            lock.unlock();
            // Queue data for later processing
            std::vector<uint8_t> data(buffer.begin(),
                                      buffer.begin() + bytesRead);
            {
                std::lock_guard<std::mutex> asyncLock(m_AsyncMutex);
                m_DataQueue.push(std::move(data));
            }
            m_AsyncCV.notify_one();
        }
    }

    // Member variables
    int m_PortFD{-1};
    bool m_Debug{false};
    std::string m_DriverName;
    std::atomic<bool> m_IsRunning{false};
    std::atomic<bool> m_ShouldExit{false};
    std::atomic<size_t> m_ReadBufferSize{1024};

    // Thread-safe callback with shared_mutex for reader-writer pattern
    mutable std::shared_mutex m_CallbackMutex;
    std::function<void(const std::vector<uint8_t>&, size_t)> m_DataCallback;

    // Async read thread and data queue
    std::thread m_WorkerThread;
    std::condition_variable m_AsyncCV;
    std::mutex m_AsyncMutex;
    std::queue<std::vector<uint8_t>> m_DataQueue;
};

// TTYBase implementation using delegation to Impl class

TTYBase::TTYBase(std::string_view driverName)
    : m_pImpl(std::make_unique<Impl>(driverName)) {}

TTYBase::~TTYBase() = default;

TTYBase::TTYBase(TTYBase&& other) noexcept = default;
TTYBase& TTYBase::operator=(TTYBase&& other) noexcept = default;

TTYBase::TTYResponse TTYBase::read(std::span<uint8_t> buffer, uint8_t timeout,
                                   uint32_t& nbytesRead) {
    if (!m_pImpl) {
        nbytesRead = 0;
        return TTYResponse::Errno;  // Object has been moved
    }
    return m_pImpl->read(buffer, timeout, nbytesRead);
}

TTYBase::TTYResponse TTYBase::readSection(std::span<uint8_t> buffer,
                                          uint8_t stopByte, uint8_t timeout,
                                          uint32_t& nbytesRead) {
    if (!m_pImpl) {
        nbytesRead = 0;
        return TTYResponse::Errno;  // Object has been moved
    }
    return m_pImpl->readSection(buffer, stopByte, timeout, nbytesRead);
}

TTYBase::TTYResponse TTYBase::write(std::span<const uint8_t> buffer,
                                    uint32_t& nbytesWritten) {
    if (!m_pImpl) {
        nbytesWritten = 0;
        return TTYResponse::Errno;
    }
    return m_pImpl->write(buffer, nbytesWritten);
}

TTYBase::TTYResponse TTYBase::writeString(std::string_view string,
                                          uint32_t& nbytesWritten) {
    if (!m_pImpl) {
        nbytesWritten = 0;
        return TTYResponse::Errno;
    }
    return m_pImpl->write(
        std::span<const uint8_t>(
            reinterpret_cast<const uint8_t*>(string.data()), string.size()),
        nbytesWritten);
}

std::future<std::pair<TTYBase::TTYResponse, uint32_t>> TTYBase::readAsync(
    std::span<uint8_t> buffer, uint8_t timeout) {
    if (!m_pImpl) {
        return std::async(std::launch::deferred, []() {
            return std::make_pair(TTYResponse::Errno, uint32_t{0});
        });
    }
    return std::async(std::launch::async, [this, buffer, timeout]() {
        uint32_t bytesRead = 0;
        auto response = this->read(buffer, timeout, bytesRead);
        return std::make_pair(response, bytesRead);
    });
}

std::future<std::pair<TTYBase::TTYResponse, uint32_t>> TTYBase::writeAsync(
    std::span<const uint8_t> buffer) {
    if (!m_pImpl) {
        return std::async(std::launch::deferred, []() {
            return std::make_pair(TTYResponse::Errno, uint32_t{0});
        });
    }
    return std::async(std::launch::async, [this, buffer]() {
        uint32_t bytesWritten = 0;
        auto response = this->write(buffer, bytesWritten);
        return std::make_pair(response, bytesWritten);
    });
}

TTYBase::TTYResponse TTYBase::connect(std::string_view device, uint32_t bitRate,
                                      uint8_t wordSize, uint8_t parity,
                                      uint8_t stopBits) {
    if (!m_pImpl) {
        return TTYResponse::Errno;  // Object has been moved
    }
    return m_pImpl->connect(device, bitRate, wordSize, parity, stopBits);
}

TTYBase::TTYResponse TTYBase::disconnect() noexcept {
    if (!m_pImpl) {
        return TTYResponse::OK;  // Already disconnected (moved-from object)
    }
    return m_pImpl->disconnect();
}

void TTYBase::setDebug(bool enabled) noexcept {
    if (!m_pImpl) {
        return;  // No-op for moved-from object
    }
    m_pImpl->setDebug(enabled);
}

std::string TTYBase::getErrorMessage(TTYResponse code) const noexcept {
    if (!m_pImpl) {
        return "Object has been moved";
    }
    return m_pImpl->getErrorMessage(code);
}

int TTYBase::getPortFD() const noexcept {
    if (!m_pImpl) {
        return -1;  // Default value for moved-from object
    }
    return m_pImpl->getPortFD();
}

bool TTYBase::isConnected() const noexcept {
    if (!m_pImpl) {
        return false;  // Default value for moved-from object
    }
    return m_pImpl->isConnected();
}

void TTYBase::startAsyncRead() {
    if (m_pImpl) {
        m_pImpl->startAsyncRead();
    }
}

void TTYBase::stopAsyncRead() {
    if (m_pImpl) {
        m_pImpl->stopAsyncRead();
    }
}

void TTYBase::setDataCallback(
    std::function<void(const std::vector<uint8_t>&, size_t)> callback) {
    if (m_pImpl) {
        m_pImpl->setDataCallback(std::move(callback));
    }
}

bool TTYBase::getQueuedData(std::vector<uint8_t>& data,
                            std::chrono::milliseconds timeout) {
    if (!m_pImpl) {
        return false;
    }
    return m_pImpl->getQueuedData(data, timeout);
}

void TTYBase::setReadBufferSize(size_t size) {
    if (m_pImpl) {
        m_pImpl->setReadBufferSize(size);
    }
}

}  // namespace atom::connection
