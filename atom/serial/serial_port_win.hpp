#ifdef _WIN32

#include <Strsafe.h>
#include <Windows.h>
#include <devguid.h>
#include <initguid.h>
#include <setupapi.h>
#include <atomic>
#include <mutex>
#include <regex>
#include <thread>
#include "SerialPort.h"
#pragma comment(lib, "setupapi.lib")

namespace serial {

/**
 * @brief Implementation class for SerialPort handling Windows-specific
 * functionality
 *
 * This class provides the platform-specific implementation for serial port
 * communications on Windows systems using the Win32 API.
 */
class SerialPortImpl {
public:
    /**
     * @brief Default constructor
     *
     * Initializes a new instance with invalid handle and default settings.
     */
    SerialPortImpl()
        : handle_(INVALID_HANDLE_VALUE),
          config_{},
          portName_(""),
          asyncReadThread_(),
          stopAsyncRead_(false) {}

    /**
     * @brief Destructor
     *
     * Stops any asynchronous read operations and closes the port if open.
     */
    ~SerialPortImpl() {
        stopAsyncWorker();
        close();
    }

    /**
     * @brief Opens a serial port with the specified configuration
     *
     * @param portName The name of the serial port (e.g., "COM1")
     * @param config The serial port configuration
     * @throws SerialException If the port cannot be opened
     */
    void open(const std::string& portName, const SerialConfig& config) {
        std::lock_guard<std::mutex> lock(mutex_);

        if (isOpen())
            close();

        // Windows requires \\.\\ prefix
        std::string fullPortName = portName;
        if (portName.substr(0, 4) != "\\\\.\\") {
            fullPortName = "\\\\.\\" + portName;
        }

        handle_ =
            CreateFileA(fullPortName.c_str(), GENERIC_READ | GENERIC_WRITE,
                        0,                      // No sharing
                        nullptr,                // Default security attributes
                        OPEN_EXISTING,          // Serial port must exist
                        FILE_ATTRIBUTE_NORMAL,  // Normal file attributes
                        nullptr                 // No template
            );

        if (handle_ == INVALID_HANDLE_VALUE) {
            DWORD error = GetLastError();
            std::string errorMsg = getLastErrorAsString(error);
            throw SerialException("Cannot open serial port: " + portName +
                                  " (Error: " + errorMsg + ")");
        }

        portName_ = portName;
        config_ = config;
        applyConfig();
    }

    /**
     * @brief Closes the serial port if open
     */
    void close() {
        std::lock_guard<std::mutex> lock(mutex_);

        if (handle_ != INVALID_HANDLE_VALUE) {
            CloseHandle(handle_);
            handle_ = INVALID_HANDLE_VALUE;
            portName_ = "";
        }
    }

    /**
     * @brief Checks if the serial port is currently open
     *
     * @return true if the port is open, false otherwise
     */
    bool isOpen() const { return handle_ != INVALID_HANDLE_VALUE; }

    /**
     * @brief Reads data from the serial port
     *
     * @param maxBytes Maximum number of bytes to read
     * @return Vector of bytes read from the port
     * @throws SerialPortNotOpenException If the port is not open
     * @throws SerialTimeoutException If a read timeout occurs
     * @throws SerialIOException If a read error occurs
     */
    std::vector<uint8_t> read(size_t maxBytes) {
        std::lock_guard<std::mutex> lock(mutex_);

        if (!isOpen()) {
            throw SerialPortNotOpenException();
        }

        if (maxBytes == 0) {
            return {};
        }

        std::vector<uint8_t> buffer(maxBytes);
        DWORD bytesRead = 0;

        if (!ReadFile(handle_, buffer.data(), static_cast<DWORD>(maxBytes),
                      &bytesRead, nullptr)) {
            DWORD error = GetLastError();
            if (error == ERROR_TIMEOUT) {
                throw SerialTimeoutException();
            } else {
                throw SerialIOException("Read error: " +
                                        getLastErrorAsString(error));
            }
        }

        buffer.resize(bytesRead);
        return buffer;
    }

    /**
     * @brief Reads exactly the specified number of bytes with timeout
     *
     * @param bytes Number of bytes to read
     * @param timeout Maximum time to wait for the requested bytes
     * @return Vector containing the requested number of bytes
     * @throws SerialPortNotOpenException If the port is not open
     * @throws SerialTimeoutException If the timeout expires before all bytes
     * are read
     */
    std::vector<uint8_t> readExactly(size_t bytes,
                                     std::chrono::milliseconds timeout) {
        if (!isOpen()) {
            throw SerialPortNotOpenException();
        }

        if (bytes == 0) {
            return {};
        }

        std::vector<uint8_t> result;
        result.reserve(bytes);

        auto startTime = std::chrono::steady_clock::now();

        while (result.size() < bytes) {
            auto now = std::chrono::steady_clock::now();
            auto elapsed =
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    now - startTime);

            if (elapsed >= timeout) {
                throw SerialTimeoutException();
            }

            auto remainingTimeout = timeout - elapsed;

            // Save original timeout settings
            COMMTIMEOUTS originalTimeouts;
            GetCommTimeouts(handle_, &originalTimeouts);

            // Set temporary timeout
            COMMTIMEOUTS tempTimeouts{};
            tempTimeouts.ReadIntervalTimeout = MAXDWORD;
            tempTimeouts.ReadTotalTimeoutMultiplier = MAXDWORD;
            tempTimeouts.ReadTotalTimeoutConstant =
                static_cast<DWORD>(remainingTimeout.count());
            SetCommTimeouts(handle_, &tempTimeouts);

            try {
                auto chunk = read(bytes - result.size());
                if (!chunk.empty()) {
                    result.insert(result.end(), chunk.begin(), chunk.end());
                }

                // Restore original timeout settings
                SetCommTimeouts(handle_, &originalTimeouts);
            } catch (...) {
                // Restore original timeout settings and rethrow exception
                SetCommTimeouts(handle_, &originalTimeouts);
                throw;
            }

            // If we haven't read all the requested data, sleep briefly to avoid
            // high CPU usage
            if (result.size() < bytes) {
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
            }
        }

        return result;
    }

    /**
     * @brief Sets up asynchronous reading of data from the serial port
     *
     * @param maxBytes Maximum number of bytes to read in each operation
     * @param callback Function to call with the read data
     * @throws SerialPortNotOpenException If the port is not open
     */
    void asyncRead(size_t maxBytes,
                   std::function<void(std::vector<uint8_t>)> callback) {
        if (!isOpen()) {
            throw SerialPortNotOpenException();
        }

        stopAsyncWorker();

        stopAsyncRead_ = false;
        asyncReadThread_ = std::thread([this, maxBytes, callback]() {
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
                        // Pass error information or handle here
                        std::cerr
                            << "Serial port async read error: " << e.what()
                            << std::endl;
                        break;
                    }
                }

                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        });
    }

    /**
     * @brief Reads all available data from the serial port
     *
     * @return Vector containing all available bytes
     * @throws SerialPortNotOpenException If the port is not open
     * @throws SerialIOException If there's an error getting port status
     */
    std::vector<uint8_t> readAvailable() {
        std::lock_guard<std::mutex> lock(mutex_);

        if (!isOpen()) {
            throw SerialPortNotOpenException();
        }

        DWORD errors;
        COMSTAT comStat;

        if (!ClearCommError(handle_, &errors, &comStat)) {
            throw SerialIOException("Unable to get serial port status: " +
                                    getLastErrorAsString(GetLastError()));
        }

        if (comStat.cbInQue == 0) {
            return {};
        }

        return read(comStat.cbInQue);
    }

    /**
     * @brief Writes data to the serial port
     *
     * @param data Data to write to the port
     * @return Number of bytes actually written
     * @throws SerialPortNotOpenException If the port is not open
     * @throws SerialTimeoutException If a write timeout occurs
     * @throws SerialIOException If a write error occurs
     */
    size_t write(std::span<const uint8_t> data) {
        std::lock_guard<std::mutex> lock(mutex_);

        if (!isOpen()) {
            throw SerialPortNotOpenException();
        }

        if (data.empty()) {
            return 0;
        }

        DWORD bytesWritten = 0;
        if (!WriteFile(handle_, data.data(), static_cast<DWORD>(data.size()),
                       &bytesWritten, nullptr)) {
            DWORD error = GetLastError();
            if (error == ERROR_TIMEOUT) {
                throw SerialTimeoutException();
            } else {
                throw SerialIOException("Write error: " +
                                        getLastErrorAsString(error));
            }
        }

        return bytesWritten;
    }

    /**
     * @brief Flushes both input and output buffers
     *
     * @throws SerialPortNotOpenException If the port is not open
     * @throws SerialIOException If the flush operation fails
     */
    void flush() {
        std::lock_guard<std::mutex> lock(mutex_);

        if (!isOpen()) {
            throw SerialPortNotOpenException();
        }

        if (!PurgeComm(handle_, PURGE_RXCLEAR | PURGE_TXCLEAR)) {
            throw SerialIOException("Unable to flush serial port buffers: " +
                                    getLastErrorAsString(GetLastError()));
        }
    }

    /**
     * @brief Waits for all transmitted data to be sent
     *
     * @throws SerialPortNotOpenException If the port is not open
     * @throws SerialIOException If the drain operation fails
     */
    void drain() {
        std::lock_guard<std::mutex> lock(mutex_);

        if (!isOpen()) {
            throw SerialPortNotOpenException();
        }

        // In Windows, FlushFileBuffers is equivalent to a drain operation
        if (!FlushFileBuffers(handle_)) {
            throw SerialIOException("Unable to complete buffer write: " +
                                    getLastErrorAsString(GetLastError()));
        }
    }

    /**
     * @brief Gets the number of bytes available to read
     *
     * @return Number of bytes in the input buffer
     * @throws SerialPortNotOpenException If the port is not open
     * @throws SerialIOException If there's an error getting port status
     */
    size_t available() const {
        std::lock_guard<std::mutex> lock(mutex_);

        if (!isOpen()) {
            throw SerialPortNotOpenException();
        }

        DWORD errors;
        COMSTAT comStat;

        if (!ClearCommError(handle_, &errors, &comStat)) {
            throw SerialIOException("Unable to get serial port status: " +
                                    getLastErrorAsString(GetLastError()));
        }

        return comStat.cbInQue;
    }

    /**
     * @brief Sets the serial port configuration
     *
     * @param config New configuration to apply
     * @throws SerialPortNotOpenException If the port is not open
     */
    void setConfig(const SerialConfig& config) {
        std::lock_guard<std::mutex> lock(mutex_);

        if (!isOpen()) {
            throw SerialPortNotOpenException();
        }

        config_ = config;
        applyConfig();
    }

    /**
     * @brief Gets the current serial port configuration
     *
     * @return Current serial port configuration
     */
    SerialConfig getConfig() const { return config_; }

    /**
     * @brief Sets the Data Terminal Ready (DTR) signal
     *
     * @param value True to assert DTR, false to clear it
     * @throws SerialPortNotOpenException If the port is not open
     * @throws SerialIOException If the operation fails
     */
    void setDTR(bool value) {
        std::lock_guard<std::mutex> lock(mutex_);

        if (!isOpen()) {
            throw SerialPortNotOpenException();
        }

        DWORD func = value ? SETDTR : CLRDTR;
        if (!EscapeCommFunction(handle_, func)) {
            throw SerialIOException("Unable to set DTR signal: " +
                                    getLastErrorAsString(GetLastError()));
        }
    }

    /**
     * @brief Sets the Request To Send (RTS) signal
     *
     * @param value True to assert RTS, false to clear it
     * @throws SerialPortNotOpenException If the port is not open
     * @throws SerialIOException If the operation fails
     */
    void setRTS(bool value) {
        std::lock_guard<std::mutex> lock(mutex_);

        if (!isOpen()) {
            throw SerialPortNotOpenException();
        }

        DWORD func = value ? SETRTS : CLRRTS;
        if (!EscapeCommFunction(handle_, func)) {
            throw SerialIOException("Unable to set RTS signal: " +
                                    getLastErrorAsString(GetLastError()));
        }
    }

    /**
     * @brief Gets the Clear To Send (CTS) signal state
     *
     * @return True if CTS is asserted, false otherwise
     * @throws SerialPortNotOpenException If the port is not open
     * @throws SerialIOException If the operation fails
     */
    bool getCTS() const { return getModemStatus(MS_CTS_ON); }

    /**
     * @brief Gets the Data Set Ready (DSR) signal state
     *
     * @return True if DSR is asserted, false otherwise
     * @throws SerialPortNotOpenException If the port is not open
     * @throws SerialIOException If the operation fails
     */
    bool getDSR() const { return getModemStatus(MS_DSR_ON); }

    /**
     * @brief Gets the Ring Indicator (RI) signal state
     *
     * @return True if RI is asserted, false otherwise
     * @throws SerialPortNotOpenException If the port is not open
     * @throws SerialIOException If the operation fails
     */
    bool getRI() const { return getModemStatus(MS_RING_ON); }

    /**
     * @brief Gets the Carrier Detect (CD) signal state
     *
     * @return True if CD is asserted, false otherwise
     * @throws SerialPortNotOpenException If the port is not open
     * @throws SerialIOException If the operation fails
     */
    bool getCD() const {
        return getModemStatus(
            MS_RLSD_ON);  // RLSD = Receive Line Signal Detect = CD
    }

    /**
     * @brief Gets the port name
     *
     * @return The name of the port
     */
    std::string getPortName() const { return portName_; }

    /**
     * @brief Gets a list of available serial ports on the system
     *
     * @return Vector of available port names
     */
    static std::vector<std::string> getAvailablePorts() {
        std::vector<std::string> ports;

        // Use SetupDi API to get serial port list
        HDEVINFO deviceInfoSet =
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
            HKEY deviceKey =
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
    HANDLE handle_;                    ///< Windows handle to the serial port
    SerialConfig config_;              ///< Current port configuration
    std::string portName_;             ///< Name of the currently open port
    mutable std::mutex mutex_;         ///< Mutex for thread safety
    std::thread asyncReadThread_;      ///< Thread for asynchronous reading
    std::atomic<bool> stopAsyncRead_;  ///< Flag to stop asynchronous reading

    /**
     * @brief Applies the current configuration to the serial port
     *
     * @throws SerialIOException If unable to configure the port
     */
    void applyConfig() {
        if (!isOpen()) {
            return;
        }

        DCB dcb{};
        dcb.DCBlength = sizeof(DCB);

        if (!GetCommState(handle_, &dcb)) {
            throw SerialIOException(
                "Unable to get serial port configuration: " +
                getLastErrorAsString(GetLastError()));
        }

        // Baud rate
        dcb.BaudRate = config_.baudRate;

        // Data bits
        dcb.ByteSize = config_.dataBits;

        // Parity
        switch (config_.parity) {
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

        // Stop bits
        switch (config_.stopBits) {
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

        // Flow control
        switch (config_.flowControl) {
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
                dcb.XonChar = 17;   // XON = Ctrl+Q
                dcb.XoffChar = 19;  // XOFF = Ctrl+S
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

        // Other settings
        dcb.fBinary = TRUE;  // Binary mode
        dcb.fErrorChar = FALSE;
        dcb.fNull = FALSE;
        dcb.fAbortOnError = FALSE;

        if (!SetCommState(handle_, &dcb)) {
            throw SerialIOException(
                "Unable to set serial port configuration: " +
                getLastErrorAsString(GetLastError()));
        }

        // Set timeouts
        COMMTIMEOUTS timeouts{};
        timeouts.ReadIntervalTimeout = MAXDWORD;
        timeouts.ReadTotalTimeoutMultiplier = MAXDWORD;
        timeouts.ReadTotalTimeoutConstant = config_.readTimeout.count();
        timeouts.WriteTotalTimeoutMultiplier = 0;
        timeouts.WriteTotalTimeoutConstant = config_.writeTimeout.count();

        if (!SetCommTimeouts(handle_, &timeouts)) {
            throw SerialIOException("Unable to set serial port timeouts: " +
                                    getLastErrorAsString(GetLastError()));
        }
    }

    /**
     * @brief Gets modem status flags
     *
     * @param flag The modem status flag to check
     * @return True if the flag is set, false otherwise
     * @throws SerialPortNotOpenException If the port is not open
     * @throws SerialIOException If unable to get modem status
     */
    bool getModemStatus(DWORD flag) const {
        std::lock_guard<std::mutex> lock(mutex_);

        if (!isOpen()) {
            throw SerialPortNotOpenException();
        }

        DWORD status = 0;
        if (!GetCommModemStatus(handle_, &status)) {
            throw SerialIOException("Unable to get Modem status: " +
                                    getLastErrorAsString(GetLastError()));
        }

        return (status & flag) != 0;
    }

    /**
     * @brief Stops the asynchronous read worker thread
     */
    void stopAsyncWorker() {
        if (asyncReadThread_.joinable()) {
            stopAsyncRead_ = true;
            asyncReadThread_.join();
        }
    }

    /**
     * @brief Converts a Windows error code to a human-readable string
     *
     * @param errorCode The Windows error code to convert
     * @return String description of the error
     */
    static std::string getLastErrorAsString(DWORD errorCode) {
        if (errorCode == 0) {
            return "No error";
        }

        LPSTR messageBuffer = nullptr;

        size_t size = FormatMessageA(
            FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
                FORMAT_MESSAGE_IGNORE_INSERTS,
            nullptr, errorCode, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
            (LPSTR)&messageBuffer, 0, nullptr);

        std::string message(messageBuffer, size);
        LocalFree(messageBuffer);

        return message;
    }
};

}  // namespace serial

#endif  // _WIN32