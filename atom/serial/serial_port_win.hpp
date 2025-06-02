#pragma once

#ifdef _WIN32

#include <Strsafe.h>
#include <Windows.h>
#include <devguid.h>
#include <initguid.h>
#include <setupapi.h>
#include <spdlog/spdlog.h>
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <shared_mutex>
#include <thread>
#include "serial_port.hpp"

#ifdef _MSC_VER
#pragma comment(lib, "setupapi.lib")
#endif

namespace serial {

/**
 * @brief Windows platform serial port implementation class.
 *
 * This class provides Windows-specific implementation for serial port
 * communication using Win32 API.
 */
class SerialPortImpl {
public:
    /**
     * @brief Default constructor.
     *
     * Initializes a new instance with invalid handle and default settings.
     */
    SerialPortImpl()
        : handle_(INVALID_HANDLE_VALUE),
          config_{},
          portName_(),
          asyncReadThread_(),
          stopAsyncRead_(false),
          asyncReadActive_(false) {}

    /**
     * @brief Destructor.
     *
     * Stops all async read operations and closes opened port.
     */
    ~SerialPortImpl() {
        stopAsyncWorker();
        close();
    }

    /**
     * @brief Opens a serial port with specified configuration.
     *
     * @param portName Serial port name (e.g., "COM1")
     * @param config Serial port configuration
     * @throws SerialException Thrown when unable to open or configure the port
     */
    void open(std::string_view portName, const SerialConfig& config) {
        std::unique_lock<std::shared_mutex> lock(mutex_);

        if (isOpen()) {
            close();
        }

        std::string fullPortName = std::string(portName);
        if (fullPortName.substr(0, 4) != "\\\\.\\") {
            fullPortName = "\\\\.\\" + fullPortName;
        }

        handle_ =
            CreateFileA(fullPortName.c_str(), GENERIC_READ | GENERIC_WRITE, 0,
                        nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);

        if (handle_ == INVALID_HANDLE_VALUE) {
            const DWORD error = GetLastError();
            const std::string errorMsg = getLastErrorAsString(error);
            const std::string message =
                "Cannot open serial port: " + std::string(portName) + " (" +
                errorMsg + ")";
            spdlog::error(message);
            throw SerialException(message);
        }

        portName_ = portName;
        config_ = config;

        try {
            applyConfig();
            spdlog::debug("Successfully opened serial port: {}", portName);
        } catch (...) {
            CloseHandle(handle_);
            handle_ = INVALID_HANDLE_VALUE;
            throw;
        }
    }

    /**
     * @brief Closes the opened serial port.
     */
    void close() {
        std::unique_lock<std::shared_mutex> lock(mutex_);

        if (handle_ != INVALID_HANDLE_VALUE) {
            CloseHandle(handle_);
            handle_ = INVALID_HANDLE_VALUE;
            spdlog::debug("Closed serial port: {}", portName_);
            portName_.clear();
        }
    }

    /**
     * @brief Checks if the serial port is currently open.
     *
     * @return true if port is open, false otherwise
     */
    [[nodiscard]] bool isOpen() const {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        return handle_ != INVALID_HANDLE_VALUE;
    }

    /**
     * @brief Reads data from the serial port.
     *
     * @param maxBytes Maximum number of bytes to read
     * @return Vector of bytes read from the port
     * @throws SerialPortNotOpenException If port is not open
     * @throws SerialTimeoutException If read times out
     * @throws SerialIOException If read error occurs
     */
    std::vector<uint8_t> read(size_t maxBytes) {
        std::unique_lock<std::shared_mutex> lock(mutex_);

        checkPortOpen();

        if (maxBytes == 0) {
            return {};
        }

        std::vector<uint8_t> buffer(maxBytes);
        DWORD bytesRead = 0;

        if (!ReadFile(handle_, buffer.data(), static_cast<DWORD>(maxBytes),
                      &bytesRead, nullptr)) {
            const DWORD error = GetLastError();
            if (error == ERROR_TIMEOUT) {
                throw SerialTimeoutException();
            } else {
                const std::string errorMsg =
                    "Read error: " + getLastErrorAsString(error);
                spdlog::error(errorMsg);
                throw SerialIOException(errorMsg);
            }
        }

        buffer.resize(bytesRead);
        return buffer;
    }

    /**
     * @brief Reads exactly the specified number of bytes with timeout.
     *
     * @param bytes Number of bytes to read
     * @param timeout Maximum time to wait for requested data
     * @return Vector containing the requested number of bytes
     * @throws SerialPortNotOpenException If port is not open
     * @throws SerialTimeoutException If timeout occurs before reading all bytes
     */
    std::vector<uint8_t> readExactly(size_t bytes,
                                     std::chrono::milliseconds timeout) {
        checkPortOpen();

        if (bytes == 0) {
            return {};
        }

        std::vector<uint8_t> result;
        result.reserve(bytes);

        const auto startTime = std::chrono::steady_clock::now();

        while (result.size() < bytes) {
            const auto now = std::chrono::steady_clock::now();
            const auto elapsed =
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    now - startTime);

            if (elapsed >= timeout) {
                const std::string errorMsg =
                    "Reading " + std::to_string(bytes) +
                    " bytes timed out, only read " +
                    std::to_string(result.size()) + " bytes";
                spdlog::warn(errorMsg);
                throw SerialTimeoutException(errorMsg);
            }

            const auto remainingTimeout = timeout - elapsed;

            COMMTIMEOUTS originalTimeouts;
            GetCommTimeouts(handle_, &originalTimeouts);

            COMMTIMEOUTS tempTimeouts{};
            tempTimeouts.ReadIntervalTimeout = MAXDWORD;
            tempTimeouts.ReadTotalTimeoutMultiplier = MAXDWORD;
            tempTimeouts.ReadTotalTimeoutConstant =
                static_cast<DWORD>(remainingTimeout.count());
            SetCommTimeouts(handle_, &tempTimeouts);

            try {
                std::unique_lock<std::shared_mutex> lock(mutex_);
                auto chunk = read(bytes - result.size());
                if (!chunk.empty()) {
                    result.insert(result.end(), chunk.begin(), chunk.end());
                }
                SetCommTimeouts(handle_, &originalTimeouts);
            } catch (...) {
                SetCommTimeouts(handle_, &originalTimeouts);
                throw;
            }

            if (result.size() < bytes) {
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
            }
        }

        return result;
    }

    /**
     * @brief Sets up asynchronous reading from the serial port.
     *
     * @param maxBytes Maximum number of bytes to read in each operation
     * @param callback Function to call with the read data
     * @throws SerialPortNotOpenException If port is not open
     */
    void asyncRead(size_t maxBytes,
                   std::function<void(std::vector<uint8_t>)> callback) {
        checkPortOpen();
        stopAsyncWorker();

        stopAsyncRead_ = false;
        asyncReadActive_ = true;
        asyncReadThread_ = std::thread([this, maxBytes,
                                        callback = std::move(callback)]() {
            spdlog::debug("Starting async read thread");
            try {
                while (!stopAsyncRead_) {
                    try {
                        auto data = read(maxBytes);
                        if (!data.empty() && !stopAsyncRead_) {
                            callback(std::move(data));
                        }
                    } catch (const SerialTimeoutException&) {
                        // Timeout is normal, continue
                    } catch (const std::exception& e) {
                        if (!stopAsyncRead_) {
                            spdlog::error("Serial async read error: {}",
                                          e.what());
                            break;
                        }
                    }

                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                }
            } catch (...) {
                spdlog::error("Unexpected error in async read thread");
            }

            {
                std::unique_lock<std::mutex> lock(asyncMutex_);
                asyncReadActive_ = false;
                asyncCv_.notify_all();
            }
            spdlog::debug("Async read thread stopped");
        });
    }

    /**
     * @brief Reads all available data from the serial port.
     *
     * @return Vector containing all available bytes
     * @throws SerialPortNotOpenException If port is not open
     * @throws SerialIOException If error getting port status
     */
    std::vector<uint8_t> readAvailable() {
        std::unique_lock<std::shared_mutex> lock(mutex_);

        checkPortOpen();

        DWORD errors;
        COMSTAT comStat;

        if (!ClearCommError(handle_, &errors, &comStat)) {
            const std::string errorMsg = "Cannot get serial port status: " +
                                         getLastErrorAsString(GetLastError());
            spdlog::error(errorMsg);
            throw SerialIOException(errorMsg);
        }

        if (comStat.cbInQue == 0) {
            return {};
        }

        return read(comStat.cbInQue);
    }

    /**
     * @brief Writes data to the serial port.
     *
     * @param data Data to write to the port
     * @return Number of bytes actually written
     * @throws SerialPortNotOpenException If port is not open
     * @throws SerialTimeoutException If write times out
     * @throws SerialIOException If write error occurs
     */
    size_t write(std::span<const uint8_t> data) {
        std::unique_lock<std::shared_mutex> lock(mutex_);

        checkPortOpen();

        if (data.empty()) {
            return 0;
        }

        DWORD bytesWritten = 0;
        if (!WriteFile(handle_, data.data(), static_cast<DWORD>(data.size()),
                       &bytesWritten, nullptr)) {
            const DWORD error = GetLastError();
            if (error == ERROR_TIMEOUT) {
                spdlog::warn("Write operation timed out");
                throw SerialTimeoutException();
            } else {
                const std::string errorMsg =
                    "Write error: " + getLastErrorAsString(error);
                spdlog::error(errorMsg);
                throw SerialIOException(errorMsg);
            }
        }

        return bytesWritten;
    }

    /**
     * @brief Flushes input and output buffers.
     *
     * @throws SerialPortNotOpenException If port is not open
     * @throws SerialIOException If flush operation fails
     */
    void flush() {
        std::unique_lock<std::shared_mutex> lock(mutex_);

        checkPortOpen();

        if (!PurgeComm(handle_, PURGE_RXCLEAR | PURGE_TXCLEAR)) {
            const std::string errorMsg = "Cannot flush serial port buffers: " +
                                         getLastErrorAsString(GetLastError());
            spdlog::error(errorMsg);
            throw SerialIOException(errorMsg);
        }
    }

    /**
     * @brief Waits for all transmitted data to be sent.
     *
     * @throws SerialPortNotOpenException If port is not open
     * @throws SerialIOException If drain operation fails
     */
    void drain() {
        std::unique_lock<std::shared_mutex> lock(mutex_);

        checkPortOpen();

        if (!FlushFileBuffers(handle_)) {
            const std::string errorMsg = "Cannot complete buffer write: " +
                                         getLastErrorAsString(GetLastError());
            spdlog::error(errorMsg);
            throw SerialIOException(errorMsg);
        }
    }

    /**
     * @brief Gets the number of bytes available to read.
     *
     * @return Number of bytes in the input buffer
     * @throws SerialPortNotOpenException If port is not open
     * @throws SerialIOException If error getting port status
     */
    [[nodiscard]] size_t available() const {
        std::shared_lock<std::shared_mutex> lock(mutex_);

        checkPortOpen();

        DWORD errors;
        COMSTAT comStat;

        if (!ClearCommError(handle_, &errors, &comStat)) {
            const std::string errorMsg = "Cannot get serial port status: " +
                                         getLastErrorAsString(GetLastError());
            spdlog::error(errorMsg);
            throw SerialIOException(errorMsg);
        }

        return comStat.cbInQue;
    }

    /**
     * @brief Sets the serial port configuration.
     *
     * @param config New configuration to apply
     * @throws SerialPortNotOpenException If port is not open
     */
    void setConfig(const SerialConfig& config) {
        std::unique_lock<std::shared_mutex> lock(mutex_);

        checkPortOpen();

        config_ = config;
        applyConfigInternal();
    }

    /**
     * @brief Gets the current serial port configuration.
     *
     * @return Current serial port configuration
     */
    [[nodiscard]] SerialConfig getConfig() const {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        return config_;
    }

    /**
     * @brief Sets the DTR (Data Terminal Ready) signal.
     *
     * @param value True to assert DTR, false to clear
     * @throws SerialPortNotOpenException If port is not open
     * @throws SerialIOException If operation fails
     */
    void setDTR(bool value) {
        std::unique_lock<std::shared_mutex> lock(mutex_);

        checkPortOpen();

        const DWORD func = value ? SETDTR : CLRDTR;
        if (!EscapeCommFunction(handle_, func)) {
            const std::string errorMsg = "Cannot set DTR signal: " +
                                         getLastErrorAsString(GetLastError());
            spdlog::error(errorMsg);
            throw SerialIOException(errorMsg);
        }
    }

    /**
     * @brief Sets the RTS (Request To Send) signal.
     *
     * @param value True to assert RTS, false to clear
     * @throws SerialPortNotOpenException If port is not open
     * @throws SerialIOException If operation fails
     */
    void setRTS(bool value) {
        std::unique_lock<std::shared_mutex> lock(mutex_);

        checkPortOpen();

        const DWORD func = value ? SETRTS : CLRRTS;
        if (!EscapeCommFunction(handle_, func)) {
            const std::string errorMsg = "Cannot set RTS signal: " +
                                         getLastErrorAsString(GetLastError());
            spdlog::error(errorMsg);
            throw SerialIOException(errorMsg);
        }
    }

    /**
     * @brief Gets the state of the CTS (Clear To Send) signal.
     *
     * @return True if CTS is asserted, false otherwise
     * @throws SerialPortNotOpenException If port is not open
     * @throws SerialIOException If operation fails
     */
    [[nodiscard]] bool getCTS() const { return getModemStatus(MS_CTS_ON); }

    /**
     * @brief Gets the state of the DSR (Data Set Ready) signal.
     *
     * @return True if DSR is asserted, false otherwise
     * @throws SerialPortNotOpenException If port is not open
     * @throws SerialIOException If operation fails
     */
    [[nodiscard]] bool getDSR() const { return getModemStatus(MS_DSR_ON); }

    /**
     * @brief Gets the state of the RI (Ring Indicator) signal.
     *
     * @return True if RI is asserted, false otherwise
     * @throws SerialPortNotOpenException If port is not open
     * @throws SerialIOException If operation fails
     */
    [[nodiscard]] bool getRI() const { return getModemStatus(MS_RING_ON); }

    /**
     * @brief Gets the state of the CD (Carrier Detect) signal.
     *
     * @return True if CD is asserted, false otherwise
     * @throws SerialPortNotOpenException If port is not open
     * @throws SerialIOException If operation fails
     */
    [[nodiscard]] bool getCD() const { return getModemStatus(MS_RLSD_ON); }

    /**
     * @brief Gets the port name.
     *
     * @return Port name
     */
    [[nodiscard]] std::string getPortName() const {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        return portName_;
    }

    /**
     * @brief Gets a list of available serial ports on the system.
     *
     * @return Vector of available port names
     */
    static std::vector<std::string> getAvailablePorts() {
        std::vector<std::string> ports;

        const HDEVINFO deviceInfoSet =
            SetupDiGetClassDevs(&GUID_DEVINTERFACE_COMPORT, nullptr, nullptr,
                                DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);

        if (deviceInfoSet == INVALID_HANDLE_VALUE) {
            return ports;
        }

        SP_DEVINFO_DATA deviceInfoData{};
        deviceInfoData.cbSize = sizeof(SP_DEVINFO_DATA);

        for (DWORD i = 0;
             SetupDiEnumDeviceInfo(deviceInfoSet, i, &deviceInfoData); i++) {
            char portName[256] = {0};
            const HKEY deviceKey =
                SetupDiOpenDevRegKey(deviceInfoSet, &deviceInfoData,
                                     DICS_FLAG_GLOBAL, 0, DIREG_DEV, KEY_READ);

            if (deviceKey != INVALID_HANDLE_VALUE) {
                DWORD portNameLength = sizeof(portName);
                DWORD regDataType = 0;

                if (RegQueryValueExA(deviceKey, "PortName", nullptr,
                                     &regDataType,
                                     reinterpret_cast<LPBYTE>(portName),
                                     &portNameLength) == ERROR_SUCCESS) {
                    ports.push_back(portName);
                }

                RegCloseKey(deviceKey);
            }
        }

        if (deviceInfoSet) {
            SetupDiDestroyDeviceInfoList(deviceInfoSet);
        }

        return ports;
    }

private:
    HANDLE handle_;
    SerialConfig config_;
    std::string portName_;
    mutable std::shared_mutex mutex_;
    std::thread asyncReadThread_;
    std::atomic<bool> stopAsyncRead_;
    std::atomic<bool> asyncReadActive_;
    std::mutex asyncMutex_;
    std::condition_variable asyncCv_;

    /**
     * @brief Applies current configuration to the serial port.
     *
     * @throws SerialIOException Thrown when unable to configure the port
     */
    void applyConfig() {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        applyConfigInternal();
    }

    /**
     * @brief Checks if the port is open.
     *
     * @throws SerialPortNotOpenException If port is not open
     */
    void checkPortOpen() const {
        if (!isOpen()) {
            throw SerialPortNotOpenException();
        }
    }

    /**
     * @brief Internal method to apply configuration (without locking).
     *
     * @throws SerialIOException Thrown when unable to configure the port
     */
    void applyConfigInternal() {
        if (handle_ == INVALID_HANDLE_VALUE) {
            return;
        }

        DCB dcb{};
        dcb.DCBlength = sizeof(DCB);

        if (!GetCommState(handle_, &dcb)) {
            const std::string errorMsg =
                "Cannot get serial port configuration: " +
                getLastErrorAsString(GetLastError());
            spdlog::error(errorMsg);
            throw SerialIOException(errorMsg);
        }

        dcb.BaudRate = config_.getBaudRate();
        dcb.ByteSize = config_.getDataBits();

        switch (config_.getParity()) {
            case SerialConfig::Parity::None:
                dcb.Parity = NOPARITY;
                break;
            case SerialConfig::Parity::Odd:
                dcb.Parity = ODDPARITY;
                break;
            case SerialConfig::Parity::Even:
                dcb.Parity = EVENPARITY;
                break;
            case SerialConfig::Parity::Mark:
                dcb.Parity = MARKPARITY;
                break;
            case SerialConfig::Parity::Space:
                dcb.Parity = SPACEPARITY;
                break;
        }

        switch (config_.getStopBits()) {
            case SerialConfig::StopBits::One:
                dcb.StopBits = ONESTOPBIT;
                break;
            case SerialConfig::StopBits::OnePointFive:
                dcb.StopBits = ONE5STOPBITS;
                break;
            case SerialConfig::StopBits::Two:
                dcb.StopBits = TWOSTOPBITS;
                break;
        }

        switch (config_.getFlowControl()) {
            case SerialConfig::FlowControl::None:
                dcb.fOutxCtsFlow = FALSE;
                dcb.fOutxDsrFlow = FALSE;
                dcb.fDtrControl = DTR_CONTROL_ENABLE;
                dcb.fRtsControl = RTS_CONTROL_ENABLE;
                dcb.fOutX = FALSE;
                dcb.fInX = FALSE;
                break;
            case SerialConfig::FlowControl::Software:
                dcb.fOutxCtsFlow = FALSE;
                dcb.fOutxDsrFlow = FALSE;
                dcb.fDtrControl = DTR_CONTROL_ENABLE;
                dcb.fRtsControl = RTS_CONTROL_ENABLE;
                dcb.fOutX = TRUE;
                dcb.fInX = TRUE;
                dcb.XonChar = 17;
                dcb.XoffChar = 19;
                dcb.XonLim = 100;
                dcb.XoffLim = 100;
                break;
            case SerialConfig::FlowControl::Hardware:
                dcb.fOutxCtsFlow = TRUE;
                dcb.fOutxDsrFlow = FALSE;
                dcb.fDtrControl = DTR_CONTROL_ENABLE;
                dcb.fRtsControl = RTS_CONTROL_HANDSHAKE;
                dcb.fOutX = FALSE;
                dcb.fInX = FALSE;
                break;
        }

        dcb.fBinary = TRUE;
        dcb.fErrorChar = FALSE;
        dcb.fNull = FALSE;
        dcb.fAbortOnError = FALSE;

        if (!SetCommState(handle_, &dcb)) {
            const std::string errorMsg =
                "Cannot set serial port configuration: " +
                getLastErrorAsString(GetLastError());
            spdlog::error(errorMsg);
            throw SerialIOException(errorMsg);
        }

        COMMTIMEOUTS timeouts{};
        timeouts.ReadIntervalTimeout = MAXDWORD;
        timeouts.ReadTotalTimeoutMultiplier = MAXDWORD;
        timeouts.ReadTotalTimeoutConstant = config_.getReadTimeout().count();
        timeouts.WriteTotalTimeoutMultiplier = 0;
        timeouts.WriteTotalTimeoutConstant = config_.getWriteTimeout().count();

        if (!SetCommTimeouts(handle_, &timeouts)) {
            const std::string errorMsg = "Cannot set serial port timeouts: " +
                                         getLastErrorAsString(GetLastError());
            spdlog::error(errorMsg);
            throw SerialIOException(errorMsg);
        }
    }

    /**
     * @brief Gets modem status flag.
     *
     * @param flag Modem status flag to check
     * @return True if flag is set, false otherwise
     * @throws SerialPortNotOpenException If port is not open
     * @throws SerialIOException Thrown when unable to get modem status
     */
    [[nodiscard]] bool getModemStatus(DWORD flag) const {
        std::shared_lock<std::shared_mutex> lock(mutex_);

        checkPortOpen();

        DWORD status = 0;
        if (!GetCommModemStatus(handle_, &status)) {
            const std::string errorMsg = "Cannot get modem status: " +
                                         getLastErrorAsString(GetLastError());
            spdlog::error(errorMsg);
            throw SerialIOException(errorMsg);
        }

        return (status & flag) != 0;
    }

    /**
     * @brief Stops the async read worker thread.
     */
    void stopAsyncWorker() {
        if (asyncReadThread_.joinable()) {
            stopAsyncRead_ = true;

            {
                std::unique_lock<std::mutex> lock(asyncMutex_);
                if (asyncReadActive_) {
                    asyncCv_.wait(lock, [this]() { return !asyncReadActive_; });
                }
            }

            asyncReadThread_.join();
        }
    }

    /**
     * @brief Converts Windows error code to readable string.
     *
     * @param errorCode Windows error code to convert
     * @return Error description string
     */
    static std::string getLastErrorAsString(DWORD errorCode) {
        if (errorCode == 0) {
            return "No error";
        }

        LPSTR messageBuffer = nullptr;

        const size_t size = FormatMessageA(
            FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
                FORMAT_MESSAGE_IGNORE_INSERTS,
            nullptr, errorCode, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
            reinterpret_cast<LPSTR>(&messageBuffer), 0, nullptr);

        std::string message(messageBuffer, size);
        LocalFree(messageBuffer);

        return message;
    }
};

}  // namespace serial

#endif  // _WIN32