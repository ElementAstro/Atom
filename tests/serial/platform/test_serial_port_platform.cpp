/*
 * test_serial_port_platform.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-16

Description: Platform-Specific Unit Tests for SerialPort Implementation
Tests Windows and Unix-specific serial port functionality.

**************************************************/

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <chrono>
#include <string>
#include <vector>

using namespace std::chrono_literals;

// =============================================================================
// Platform Detection Tests
// =============================================================================

TEST(PlatformDetectionTest, IdentifyPlatform) {
#ifdef _WIN32
    EXPECT_TRUE(true) << "Platform: Windows";
#elif defined(__APPLE__)
    EXPECT_TRUE(true) << "Platform: macOS";
#elif defined(__linux__)
    EXPECT_TRUE(true) << "Platform: Linux";
#elif defined(__unix__)
    EXPECT_TRUE(true) << "Platform: Unix";
#else
    EXPECT_TRUE(true) << "Platform: Unknown";
#endif
}

// =============================================================================
// Windows-Specific Tests
// =============================================================================

#ifdef _WIN32

#include <windows.h>

class WindowsSerialPortTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Windows-specific setup
    }

    void TearDown() override {
        // Windows-specific cleanup
    }
};

TEST_F(WindowsSerialPortTest, PortNameFormat) {
    // Windows COM port naming conventions
    std::vector<std::string> validPortNames = {"COM1", "COM2", "COM3", "COM10",
                                               "COM256"};

    for (const auto& name : validPortNames) {
        EXPECT_EQ(name.substr(0, 3), "COM");
    }
}

TEST_F(WindowsSerialPortTest, ExtendedPortNameFormat) {
    // Extended port name format for COM ports > 9
    std::string extendedFormat = "\\\\.\\COM10";
    EXPECT_EQ(extendedFormat.substr(0, 4), "\\\\.\\");
}

TEST_F(WindowsSerialPortTest, DCBStructureSize) {
    // DCB structure is used for serial port configuration
    DCB dcb;
    EXPECT_GT(sizeof(dcb), 0);
}

TEST_F(WindowsSerialPortTest, COMMTIMEOUTSStructureSize) {
    // COMMTIMEOUTS structure is used for timeout configuration
    COMMTIMEOUTS timeouts;
    EXPECT_GT(sizeof(timeouts), 0);
}

TEST_F(WindowsSerialPortTest, BaudRateConstants) {
    // Windows baud rate constants
    EXPECT_EQ(CBR_9600, 9600);
    EXPECT_EQ(CBR_19200, 19200);
    EXPECT_EQ(CBR_38400, 38400);
    EXPECT_EQ(CBR_57600, 57600);
    EXPECT_EQ(CBR_115200, 115200);
}

TEST_F(WindowsSerialPortTest, ParityConstants) {
    EXPECT_EQ(NOPARITY, 0);
    EXPECT_EQ(ODDPARITY, 1);
    EXPECT_EQ(EVENPARITY, 2);
    EXPECT_EQ(MARKPARITY, 3);
    EXPECT_EQ(SPACEPARITY, 4);
}

TEST_F(WindowsSerialPortTest, StopBitConstants) {
    EXPECT_EQ(ONESTOPBIT, 0);
    EXPECT_EQ(ONE5STOPBITS, 1);
    EXPECT_EQ(TWOSTOPBITS, 2);
}

TEST_F(WindowsSerialPortTest, DataBitValues) {
    // Valid data bit values
    std::vector<BYTE> validDataBits = {5, 6, 7, 8};

    for (BYTE bits : validDataBits) {
        EXPECT_GE(bits, 5);
        EXPECT_LE(bits, 8);
    }
}

TEST_F(WindowsSerialPortTest, FlowControlFlags) {
    // Flow control related flags
    DCB dcb = {0};
    dcb.DCBlength = sizeof(DCB);

    // No flow control
    dcb.fOutxCtsFlow = FALSE;
    dcb.fRtsControl = RTS_CONTROL_DISABLE;
    dcb.fOutX = FALSE;
    dcb.fInX = FALSE;

    EXPECT_FALSE(dcb.fOutxCtsFlow);
    EXPECT_EQ(dcb.fRtsControl, RTS_CONTROL_DISABLE);
}

TEST_F(WindowsSerialPortTest, ModemStatusFlags) {
    // Modem status flags
    DWORD modemStatus = 0;

    // These are the flags that can be set
    EXPECT_EQ(MS_CTS_ON, 0x0010);
    EXPECT_EQ(MS_DSR_ON, 0x0020);
    EXPECT_EQ(MS_RING_ON, 0x0040);
    EXPECT_EQ(MS_RLSD_ON, 0x0080);
}

TEST_F(WindowsSerialPortTest, PurgeFlags) {
    // Purge flags for clearing buffers
    EXPECT_NE(PURGE_TXABORT, 0);
    EXPECT_NE(PURGE_RXABORT, 0);
    EXPECT_NE(PURGE_TXCLEAR, 0);
    EXPECT_NE(PURGE_RXCLEAR, 0);
}

TEST_F(WindowsSerialPortTest, EscapeCommFunctions) {
    // Escape functions for modem control
    EXPECT_EQ(SETDTR, 5);
    EXPECT_EQ(CLRDTR, 6);
    EXPECT_EQ(SETRTS, 3);
    EXPECT_EQ(CLRRTS, 4);
}

TEST_F(WindowsSerialPortTest, ErrorConstants) {
    // Common Windows error codes for serial ports
    EXPECT_EQ(ERROR_FILE_NOT_FOUND, 2);
    EXPECT_EQ(ERROR_ACCESS_DENIED, 5);
    EXPECT_EQ(ERROR_INVALID_HANDLE, 6);
}

TEST_F(WindowsSerialPortTest, SetupAPIGUIDs) {
    // GUID for serial ports class
    // GUID_DEVCLASS_PORTS = {4D36E978-E325-11CE-BFC1-08002BE10318}
    GUID portsGuid = {0x4D36E978,
                      0xE325,
                      0x11CE,
                      {0xBF, 0xC1, 0x08, 0x00, 0x2B, 0xE1, 0x03, 0x18}};

    EXPECT_EQ(portsGuid.Data1, 0x4D36E978);
}

#endif  // _WIN32

// =============================================================================
// Unix/Linux-Specific Tests
// =============================================================================

#if defined(__unix__) || defined(__APPLE__)

#include <fcntl.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

class UnixSerialPortTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Unix-specific setup
    }

    void TearDown() override {
        // Unix-specific cleanup
    }
};

TEST_F(UnixSerialPortTest, PortNameFormat) {
    // Unix serial port naming conventions
    std::vector<std::string> validPortNames = {"/dev/ttyS0",   "/dev/ttyS1",
                                               "/dev/ttyUSB0", "/dev/ttyUSB1",
                                               "/dev/ttyACM0", "/dev/ttyACM1"};

    for (const auto& name : validPortNames) {
        EXPECT_EQ(name.substr(0, 5), "/dev/");
        EXPECT_TRUE(name.find("tty") != std::string::npos);
    }
}

#ifdef __APPLE__
TEST_F(UnixSerialPortTest, MacOSPortNameFormat) {
    // macOS-specific port naming
    std::vector<std::string> macPorts = {
        "/dev/cu.usbserial", "/dev/cu.usbmodem", "/dev/tty.usbserial",
        "/dev/tty.Bluetooth-Incoming-Port"};

    for (const auto& name : macPorts) {
        EXPECT_EQ(name.substr(0, 5), "/dev/");
    }
}
#endif

TEST_F(UnixSerialPortTest, TermiosStructureSize) {
    struct termios tio;
    EXPECT_GT(sizeof(tio), 0);
}

TEST_F(UnixSerialPortTest, BaudRateConstants) {
    // POSIX baud rate constants
    EXPECT_EQ(B9600, 9600);
    EXPECT_EQ(B19200, 19200);
    EXPECT_EQ(B38400, 38400);
    EXPECT_EQ(B57600, 57600);
    EXPECT_EQ(B115200, 115200);
}

TEST_F(UnixSerialPortTest, CharacterSizeFlags) {
    // Character size flags
    EXPECT_NE(CS5, 0);
    EXPECT_NE(CS6, 0);
    EXPECT_NE(CS7, 0);
    EXPECT_NE(CS8, 0);
}

TEST_F(UnixSerialPortTest, ParityFlags) {
    // Parity flags
    EXPECT_NE(PARENB, 0);  // Enable parity
    EXPECT_NE(PARODD, 0);  // Odd parity
}

TEST_F(UnixSerialPortTest, StopBitFlags) {
    // Stop bit flags
    EXPECT_NE(CSTOPB, 0);  // 2 stop bits
}

TEST_F(UnixSerialPortTest, FlowControlFlags) {
    // Hardware flow control
    EXPECT_NE(CRTSCTS, 0);

    // Software flow control
    EXPECT_NE(IXON, 0);
    EXPECT_NE(IXOFF, 0);
    EXPECT_NE(IXANY, 0);
}

TEST_F(UnixSerialPortTest, ModemControlFlags) {
    // Modem control line flags
    EXPECT_NE(TIOCM_DTR, 0);
    EXPECT_NE(TIOCM_RTS, 0);
    EXPECT_NE(TIOCM_CTS, 0);
    EXPECT_NE(TIOCM_DSR, 0);
    EXPECT_NE(TIOCM_RI, 0);
    EXPECT_NE(TIOCM_CD, 0);
}

TEST_F(UnixSerialPortTest, IoctlCommands) {
    // Common ioctl commands for serial ports
    EXPECT_NE(TIOCMGET, 0);  // Get modem status
    EXPECT_NE(TIOCMSET, 0);  // Set modem status
    EXPECT_NE(TIOCMBIS, 0);  // Set modem bits
    EXPECT_NE(TIOCMBIC, 0);  // Clear modem bits
    EXPECT_NE(FIONREAD, 0);  // Get bytes available
}

TEST_F(UnixSerialPortTest, OpenFlags) {
    // Flags for opening serial ports
    EXPECT_NE(O_RDWR, 0);
    EXPECT_NE(O_NOCTTY, 0);
    EXPECT_NE(O_NONBLOCK, 0);
}

TEST_F(UnixSerialPortTest, TermiosControlModes) {
    // Control mode flags
    EXPECT_NE(CLOCAL, 0);  // Ignore modem control lines
    EXPECT_NE(CREAD, 0);   // Enable receiver
}

TEST_F(UnixSerialPortTest, TermiosInputModes) {
    // Input mode flags
    EXPECT_NE(IGNBRK, 0);  // Ignore break
    EXPECT_NE(IGNPAR, 0);  // Ignore parity errors
    EXPECT_NE(INPCK, 0);   // Enable parity checking
    EXPECT_NE(ISTRIP, 0);  // Strip 8th bit
    EXPECT_NE(ICRNL, 0);   // Map CR to NL
}

TEST_F(UnixSerialPortTest, TermiosOutputModes) {
    // Output mode flags
    EXPECT_NE(OPOST, 0);  // Post-process output
}

TEST_F(UnixSerialPortTest, TermiosLocalModes) {
    // Local mode flags
    EXPECT_NE(ECHO, 0);    // Echo input
    EXPECT_NE(ECHOE, 0);   // Echo erase
    EXPECT_NE(ECHOK, 0);   // Echo kill
    EXPECT_NE(ICANON, 0);  // Canonical mode
    EXPECT_NE(ISIG, 0);    // Enable signals
}

TEST_F(UnixSerialPortTest, TermiosSpecialCharacters) {
    // Special character indices
    EXPECT_GE(VMIN, 0);
    EXPECT_GE(VTIME, 0);
}

TEST_F(UnixSerialPortTest, TcsetattrActions) {
    // Actions for tcsetattr
    EXPECT_EQ(TCSANOW, 0);    // Change immediately
    EXPECT_NE(TCSADRAIN, 0);  // Drain output first
    EXPECT_NE(TCSAFLUSH, 0);  // Drain output and flush input
}

TEST_F(UnixSerialPortTest, TcflushQueues) {
    // Queue selectors for tcflush
    EXPECT_EQ(TCIFLUSH, 0);   // Flush input
    EXPECT_NE(TCOFLUSH, 0);   // Flush output
    EXPECT_NE(TCIOFLUSH, 0);  // Flush both
}

TEST_F(UnixSerialPortTest, ErrorCodes) {
    // Common errno values for serial port operations
    EXPECT_NE(ENOENT, 0);  // No such file
    EXPECT_NE(EACCES, 0);  // Permission denied
    EXPECT_NE(EBUSY, 0);   // Device busy
    EXPECT_NE(EAGAIN, 0);  // Try again
    EXPECT_NE(EIO, 0);     // I/O error
    EXPECT_NE(ENOTTY, 0);  // Not a typewriter
}

#endif  // __unix__ || __APPLE__

// =============================================================================
// Cross-Platform Compatibility Tests
// =============================================================================

class CrossPlatformSerialTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(CrossPlatformSerialTest, CommonBaudRates) {
    // These baud rates should work on all platforms
    std::vector<int> commonBaudRates = {300,   1200,  2400,  4800,  9600,
                                        19200, 38400, 57600, 115200};

    for (int rate : commonBaudRates) {
        EXPECT_GT(rate, 0);
    }
}

TEST_F(CrossPlatformSerialTest, DataBitValues) {
    // Valid data bit values on all platforms
    std::vector<int> validDataBits = {5, 6, 7, 8};

    for (int bits : validDataBits) {
        EXPECT_GE(bits, 5);
        EXPECT_LE(bits, 8);
    }
}

TEST_F(CrossPlatformSerialTest, StopBitValues) {
    // Stop bit values
    enum class StopBits { One, OnePointFive, Two };

    EXPECT_NE(static_cast<int>(StopBits::One), static_cast<int>(StopBits::Two));
}

TEST_F(CrossPlatformSerialTest, ParityValues) {
    // Parity values
    enum class Parity { None, Odd, Even, Mark, Space };

    EXPECT_EQ(static_cast<int>(Parity::None), 0);
}

TEST_F(CrossPlatformSerialTest, FlowControlValues) {
    // Flow control values
    enum class FlowControl { None, Software, Hardware };

    EXPECT_EQ(static_cast<int>(FlowControl::None), 0);
}

TEST_F(CrossPlatformSerialTest, TimeoutValues) {
    // Timeout values in milliseconds
    auto shortTimeout = 100ms;
    auto normalTimeout = 1000ms;
    auto longTimeout = 5000ms;
    auto noTimeout = 0ms;

    EXPECT_LT(shortTimeout, normalTimeout);
    EXPECT_LT(normalTimeout, longTimeout);
    EXPECT_EQ(noTimeout.count(), 0);
}

TEST_F(CrossPlatformSerialTest, BufferSizes) {
    // Common buffer sizes
    constexpr size_t smallBuffer = 64;
    constexpr size_t mediumBuffer = 1024;
    constexpr size_t largeBuffer = 65536;

    EXPECT_LT(smallBuffer, mediumBuffer);
    EXPECT_LT(mediumBuffer, largeBuffer);
}

// =============================================================================
// Thread Safety Platform Tests
// =============================================================================

#include <atomic>
#include <mutex>
#include <thread>

class PlatformThreadSafetyTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(PlatformThreadSafetyTest, AtomicOperations) {
    std::atomic<int> counter{0};

    constexpr int numThreads = 4;
    constexpr int incrementsPerThread = 1000;

    std::vector<std::thread> threads;

    for (int i = 0; i < numThreads; ++i) {
        threads.emplace_back([&counter]() {
            for (int j = 0; j < incrementsPerThread; ++j) {
                ++counter;
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_EQ(counter.load(), numThreads * incrementsPerThread);
}

TEST_F(PlatformThreadSafetyTest, MutexProtection) {
    int counter = 0;
    std::mutex mutex;

    constexpr int numThreads = 4;
    constexpr int incrementsPerThread = 1000;

    std::vector<std::thread> threads;

    for (int i = 0; i < numThreads; ++i) {
        threads.emplace_back([&counter, &mutex]() {
            for (int j = 0; j < incrementsPerThread; ++j) {
                std::lock_guard<std::mutex> lock(mutex);
                ++counter;
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_EQ(counter, numThreads * incrementsPerThread);
}
