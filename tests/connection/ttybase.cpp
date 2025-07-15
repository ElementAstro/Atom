#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <string>
#include <thread>
#include <vector>

#include "atom/connection/ttybase.hpp"

// Mock system calls for POSIX environment
#ifdef __linux__
#include <fcntl.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

// Global mocks for system calls
extern "C" {
int mock_open_fd = -1;
int mock_open_errno = 0;
int mock_close_return = 0;
int mock_close_errno = 0;
int mock_read_return = -1;
int mock_read_errno = 0;
std::vector<uint8_t> mock_read_data;
size_t mock_read_data_pos = 0;
int mock_write_return = -1;
int mock_write_errno = 0;
std::vector<uint8_t> mock_written_data;
int mock_tcgetattr_return = 0;
int mock_tcgetattr_errno = 0;
int mock_cfsetispeed_return = 0;
int mock_cfsetospeed_return = 0;
int mock_tcsetattr_return = 0;
int mock_tcsetattr_errno = 0;
int mock_tcflush_return = 0;
int mock_fcntl_return = 0;
int mock_fcntl_errno = 0;
int mock_select_return = 0;  // 0 for timeout, 1 for data, -1 for error
int mock_select_errno = 0;
bool mock_select_has_data = false;

int open(const char* pathname, int flags, ...) {
    if (mock_open_fd != -1) {
        errno = mock_open_errno;
        return mock_open_fd;
    }
    // Default behavior if not mocked
    return -1;
}

int close(int fd) {
    errno = mock_close_errno;
    return mock_close_return;
}

ssize_t read(int fd, void* buf, size_t count) {
    if (mock_read_return != -1) {
        errno = mock_read_errno;
        if (mock_read_return == 0 && mock_read_errno == EINTR) {
            // Simulate EINTR for read
            return -1;
        }
        return mock_read_return;
    }
    // Simulate reading from mock_read_data
    size_t bytes_to_read =
        std::min(count, mock_read_data.size() - mock_read_data_pos);
    if (bytes_to_read > 0) {
        memcpy(buf, mock_read_data.data() + mock_read_data_pos, bytes_to_read);
        mock_read_data_pos += bytes_to_read;
        return bytes_to_read;
    }
    return 0;  // EOF
}

ssize_t write(int fd, const void* buf, size_t count) {
    if (mock_write_return != -1) {
        errno = mock_write_errno;
        return mock_write_return;
    }
    // Capture written data
    const uint8_t* data = static_cast<const uint8_t*>(buf);
    mock_written_data.insert(mock_written_data.end(), data, data + count);
    return count;
}

int tcgetattr(int fd, struct termios* termios_p) {
    errno = mock_tcgetattr_errno;
    return mock_tcgetattr_return;
}

int cfsetispeed(struct termios* termios_p, speed_t speed) {
    return mock_cfsetispeed_return;
}

int cfsetospeed(struct termios* termios_p, speed_t speed) {
    return mock_cfsetospeed_return;
}

int tcsetattr(int fd, int optional_actions, const struct termios* termios_p) {
    errno = mock_tcsetattr_errno;
    return mock_tcsetattr_return;
}

int tcflush(int fd, int queue_selector) { return mock_tcflush_return; }

int fcntl(int fd, int cmd, ...) {
    if (cmd == F_GETFL || cmd == F_SETFL) {
        errno = mock_fcntl_errno;
        return mock_fcntl_return;
    }
    return -1;  // Default for other commands
}

int select(int nfds, fd_set* readfds, fd_set* writefds, fd_set* exceptfds,
           struct timeval* timeout) {
    errno = mock_select_errno;
    if (mock_select_return != 0) {  // If explicitly set to return 1 or -1
        return mock_select_return;
    }
    // Simulate data availability for async read thread
    if (mock_select_has_data) {
        FD_SET(mock_open_fd, readfds);  // Set the bit for the mocked FD
        return 1;
    }
    return 0;  // Timeout
}

// Mock cfmakeraw (it's a macro, so we can't directly mock it.
// We'll assume it works correctly or mock its effects on termios if needed)
void cfmakeraw(struct termios* termios_p) {
    // This is a simplified mock. In a real scenario, you might want to
    // set specific flags to verify its effect.
    termios_p->c_iflag &=
        ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL | IXON);
    termios_p->c_oflag &= ~OPOST;
    termios_p->c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
    termios_p->c_cflag &= ~(CSIZE | PARENB);
    termios_p->c_cflag |= CS8;
    termios_p->c_cc[VMIN] = 1;
    termios_p->c_cc[VTIME] = 0;
}

}  // extern "C"

// Reset mocks before each test
void reset_mocks() {
    mock_open_fd = -1;
    mock_open_errno = 0;
    mock_close_return = 0;
    mock_close_errno = 0;
    mock_read_return = -1;
    mock_read_errno = 0;
    mock_read_data.clear();
    mock_read_data_pos = 0;
    mock_write_return = -1;
    mock_write_errno = 0;
    mock_written_data.clear();
    mock_tcgetattr_return = 0;
    mock_tcgetattr_errno = 0;
    mock_cfsetispeed_return = 0;
    mock_cfsetospeed_return = 0;
    mock_tcsetattr_return = 0;
    mock_tcsetattr_errno = 0;
    mock_tcflush_return = 0;
    mock_fcntl_return = 0;
    mock_fcntl_errno = 0;
    mock_select_return = 0;
    mock_select_errno = 0;
    mock_select_has_data = false;
}

#endif  // __linux__

namespace atom::connection::test {

class TTYBaseTest : public ::testing::Test {
protected:
    void SetUp() override {
#ifdef __linux__
        reset_mocks();
#endif
    }

    void TearDown() override {
        // Ensure any active TTYBase objects are disconnected
        // This is important for tests that might leave the port open
        // or async threads running.
    }

    // Helper to mock a successful connection
    void mock_successful_connect() {
#ifdef __linux__
        mock_open_fd = 100;  // A dummy file descriptor
        mock_open_errno = 0;
        mock_tcgetattr_return = 0;
        mock_cfsetispeed_return = 0;
        mock_cfsetospeed_return = 0;
        mock_tcsetattr_return = 0;
        mock_fcntl_return = 0;  // For clearing O_NONBLOCK
#endif
    }
};

// Test: Constructor and Destructor
TEST_F(TTYBaseTest, ConstructorDestructor) {
    EXPECT_NO_THROW({ TTYBase tty("TestDriver"); });
}

// Test: Move Constructor
TEST_F(TTYBaseTest, MoveConstructor) {
    TTYBase original("OriginalDriver");
    mock_successful_connect();
    original.connect("/dev/ttyS0", 9600, 8, 0, 1);

    TTYBase moved = std::move(original);

    // Check state of moved object
    EXPECT_EQ(moved.getPortFD(), 100);  // Should have original's FD
    EXPECT_TRUE(moved.isConnected());
    EXPECT_EQ(moved.getErrorMessage(TTYBase::TTYResponse::OK),
              "No error");  // Check a basic function

    // Original object should be in a valid but unspecified state, typically
    // disconnected and with default values. We can't assert much about its
    // internal state after move, but it shouldn't crash on destruction.
    EXPECT_EQ(original.getPortFD(), -1);  // Should be reset
    EXPECT_FALSE(original.isConnected());
}

// Test: Move Assignment
TEST_F(TTYBaseTest, MoveAssignment) {
    TTYBase original("OriginalDriver");
    mock_successful_connect();
    original.connect("/dev/ttyS0", 9600, 8, 0, 1);

    TTYBase target("TargetDriver");
    // Target is initially disconnected, but its state will be overwritten

    target = std::move(original);

    // Check state of target object
    EXPECT_EQ(target.getPortFD(), 100);
    EXPECT_TRUE(target.isConnected());

    // Original object should be in a valid but unspecified state
    EXPECT_EQ(original.getPortFD(), -1);
    EXPECT_FALSE(original.isConnected());
}

// Test: setDebug
TEST_F(TTYBaseTest, SetDebug) {
    TTYBase tty("TestDriver");
    EXPECT_NO_THROW(tty.setDebug(true));
    EXPECT_NO_THROW(tty.setDebug(false));
}

// Test: getErrorMessage
TEST_F(TTYBaseTest, GetErrorMessage) {
    TTYBase tty("TestDriver");
    EXPECT_EQ(tty.getErrorMessage(TTYBase::TTYResponse::OK), "No error");
    EXPECT_THAT(tty.getErrorMessage(TTYBase::TTYResponse::ReadError),
                testing::HasSubstr("Read error"));
    EXPECT_THAT(tty.getErrorMessage(TTYBase::TTYResponse::WriteError),
                testing::HasSubstr("Write error"));
    EXPECT_THAT(tty.getErrorMessage(TTYBase::TTYResponse::SelectError),
                testing::HasSubstr("Select error"));
    EXPECT_EQ(tty.getErrorMessage(TTYBase::TTYResponse::Timeout),
              "Timeout error");
    EXPECT_THAT(tty.getErrorMessage(TTYBase::TTYResponse::PortFailure),
                testing::HasSubstr("Port failure"));
    EXPECT_EQ(tty.getErrorMessage(TTYBase::TTYResponse::ParamError),
              "Parameter error");
    EXPECT_THAT(tty.getErrorMessage(TTYBase::TTYResponse::Errno),
                testing::HasSubstr("Error:"));
    EXPECT_EQ(tty.getErrorMessage(TTYBase::TTYResponse::Overflow),
              "Read overflow error");
}

// Test: getPortFD and isConnected
TEST_F(TTYBaseTest, GetPortFDAndIsConnected) {
    TTYBase tty("TestDriver");
    EXPECT_EQ(tty.getPortFD(), -1);
    EXPECT_FALSE(tty.isConnected());
}

// Test: Connect and Disconnect Success
TEST_F(TTYBaseTest, ConnectDisconnectSuccess) {
    TTYBase tty("TestDriver");
    mock_successful_connect();

    TTYBase::TTYResponse response = tty.connect("/dev/ttyS0", 9600, 8, 0, 1);
    EXPECT_EQ(response, TTYBase::TTYResponse::OK);
    EXPECT_EQ(tty.getPortFD(), 100);
    EXPECT_TRUE(tty.isConnected());

    response = tty.disconnect();
    EXPECT_EQ(response, TTYBase::TTYResponse::OK);
    EXPECT_EQ(tty.getPortFD(), -1);
    EXPECT_FALSE(tty.isConnected());
}

// Test: Connect Invalid Parameters
TEST_F(TTYBaseTest, ConnectInvalidParameters) {
    TTYBase tty("TestDriver");

    // Empty device name
    EXPECT_EQ(tty.connect("", 9600, 8, 0, 1), TTYBase::TTYResponse::ParamError);
    // Invalid word size
    EXPECT_EQ(tty.connect("/dev/ttyS0", 9600, 4, 0, 1),
              TTYBase::TTYResponse::ParamError);
    EXPECT_EQ(tty.connect("/dev/ttyS0", 9600, 9, 0, 1),
              TTYBase::TTYResponse::ParamError);
    // Invalid parity
    EXPECT_EQ(tty.connect("/dev/ttyS0", 9600, 8, 3, 1),
              TTYBase::TTYResponse::ParamError);
    // Invalid stop bits
    EXPECT_EQ(tty.connect("/dev/ttyS0", 9600, 8, 0, 0),
              TTYBase::TTYResponse::ParamError);
    EXPECT_EQ(tty.connect("/dev/ttyS0", 9600, 8, 0, 3),
              TTYBase::TTYResponse::ParamError);
    // Invalid bit rate (for Linux, as it's checked in Impl)
#ifdef __linux__
    EXPECT_EQ(tty.connect("/dev/ttyS0", 1, 8, 0, 1),
              TTYBase::TTYResponse::ParamError);
#endif
}

// Test: Connect Port Failure
TEST_F(TTYBaseTest, ConnectPortFailure) {
    TTYBase tty("TestDriver");
#ifdef __linux__
    mock_open_fd = -1;         // Simulate open failing
    mock_open_errno = EACCES;  // Permission denied
#endif
    EXPECT_EQ(tty.connect("/dev/ttyS0", 9600, 8, 0, 1),
              TTYBase::TTYResponse::PortFailure);
}

// Test: Read/Write Success
TEST_F(TTYBaseTest, ReadWriteSuccess) {
    TTYBase tty("TestDriver");
    mock_successful_connect();
    tty.connect("/dev/ttyS0", 9600, 8, 0, 1);

    std::vector<uint8_t> write_data = {0x01, 0x02, 0x03, 0x04};
    uint32_t nbytesWritten = 0;
    TTYBase::TTYResponse write_response = tty.write(write_data, nbytesWritten);
    EXPECT_EQ(write_response, TTYBase::TTYResponse::OK);
    EXPECT_EQ(nbytesWritten, write_data.size());
#ifdef __linux__
    EXPECT_EQ(mock_written_data, write_data);
#endif

    std::vector<uint8_t> read_buffer(4);
    uint32_t nbytesRead = 0;
#ifdef __linux__
    mock_read_data = {0x05, 0x06, 0x07, 0x08};
    mock_read_data_pos = 0;
#endif
    TTYBase::TTYResponse read_response = tty.read(read_buffer, 1, nbytesRead);
    EXPECT_EQ(read_response, TTYBase::TTYResponse::OK);
    EXPECT_EQ(nbytesRead, read_buffer.size());
#ifdef __linux__
    EXPECT_EQ(read_buffer, mock_read_data);
#endif

    tty.disconnect();
}

// Test: Read/Write Errors
TEST_F(TTYBaseTest, ReadWriteErrors) {
    TTYBase tty("TestDriver");
    mock_successful_connect();
    tty.connect("/dev/ttyS0", 9600, 8, 0, 1);

    std::vector<uint8_t> data = {0x01};
    uint32_t bytes = 0;

#ifdef __linux__
    mock_write_return = -1;  // Simulate write error
    mock_write_errno = EIO;
#endif
    EXPECT_EQ(tty.write(data, bytes), TTYBase::TTYResponse::WriteError);

#ifdef __linux__
    mock_read_return = -1;  // Simulate read error
    mock_read_errno = EIO;
#endif
    EXPECT_EQ(tty.read(data, 1, bytes), TTYBase::TTYResponse::ReadError);

    tty.disconnect();
}

// Test: Read Timeout
TEST_F(TTYBaseTest, ReadTimeout) {
    TTYBase tty("TestDriver");
    mock_successful_connect();
    tty.connect("/dev/ttyS0", 9600, 8, 0, 1);

    std::vector<uint8_t> read_buffer(1);
    uint32_t nbytesRead = 0;

#ifdef __linux__
    mock_select_return = 0;  // Simulate select timeout
    mock_read_return = 0;    // Simulate no data read after timeout
#endif
    TTYBase::TTYResponse response = tty.read(read_buffer, 1, nbytesRead);
    EXPECT_EQ(response, TTYBase::TTYResponse::Timeout);
    EXPECT_EQ(nbytesRead, 0);

    tty.disconnect();
}

// Test: Read Section Success
TEST_F(TTYBaseTest, ReadSectionSuccess) {
    TTYBase tty("TestDriver");
    mock_successful_connect();
    tty.connect("/dev/ttyS0", 9600, 8, 0, 1);

    std::vector<uint8_t> read_buffer(10);
    uint32_t nbytesRead = 0;
    uint8_t stopByte = 0x0A;  // Newline

#ifdef __linux__
    mock_read_data = {0x01, 0x02, 0x0A, 0x03, 0x04};  // Data with stop byte
    mock_read_data_pos = 0;
    // For readSection, we need to mock read byte by byte
    // This is tricky with the global mock, so we'll rely on the default
    // mock_read_return = -1 behavior which reads from mock_read_data
    // and mock_select_return = 1 to indicate data is always available
    mock_select_return = 1;
#endif

    TTYBase::TTYResponse response =
        tty.readSection(read_buffer, stopByte, 1, nbytesRead);
    EXPECT_EQ(response, TTYBase::TTYResponse::OK);
    EXPECT_EQ(nbytesRead, 3);  // Should read 0x01, 0x02, 0x0A
    EXPECT_EQ(read_buffer[0], 0x01);
    EXPECT_EQ(read_buffer[1], 0x02);
    EXPECT_EQ(read_buffer[2], 0x0A);

    tty.disconnect();
}

// Test: Read Section Overflow
TEST_F(TTYBaseTest, ReadSectionOverflow) {
    TTYBase tty("TestDriver");
    mock_successful_connect();
    tty.connect("/dev/ttyS0", 9600, 8, 0, 1);

    std::vector<uint8_t> read_buffer(3);  // Small buffer
    uint32_t nbytesRead = 0;
    uint8_t stopByte = 0x0A;

#ifdef __linux__
    mock_read_data = {0x01, 0x02, 0x03, 0x04,
                      0x0A};  // Stop byte is beyond buffer size
    mock_read_data_pos = 0;
    mock_select_return = 1;
#endif

    TTYBase::TTYResponse response =
        tty.readSection(read_buffer, stopByte, 1, nbytesRead);
    EXPECT_EQ(response, TTYBase::TTYResponse::Overflow);
    EXPECT_EQ(nbytesRead, 3);  // Buffer should be full
    EXPECT_EQ(read_buffer[0], 0x01);
    EXPECT_EQ(read_buffer[1], 0x02);
    EXPECT_EQ(read_buffer[2], 0x03);

    tty.disconnect();
}

// Test: Read Async
TEST_F(TTYBaseTest, ReadAsync) {
    TTYBase tty("TestDriver");
    mock_successful_connect();
    tty.connect("/dev/ttyS0", 9600, 8, 0, 1);

    std::vector<uint8_t> read_buffer(4);
#ifdef __linux__
    mock_read_data = {0x10, 0x11, 0x12, 0x13};
    mock_read_data_pos = 0;
    mock_select_return = 1;  // Ensure select always returns data for async read
#endif

    std::future<std::pair<TTYBase::TTYResponse, uint32_t>> future =
        tty.readAsync(read_buffer, 1);
    auto result = future.get();

    EXPECT_EQ(result.first, TTYBase::TTYResponse::OK);
    EXPECT_EQ(result.second, 4);
#ifdef __linux__
    EXPECT_EQ(read_buffer, mock_read_data);
#endif

    tty.disconnect();
}

// Test: Write Async
TEST_F(TTYBaseTest, WriteAsync) {
    TTYBase tty("TestDriver");
    mock_successful_connect();
    tty.connect("/dev/ttyS0", 9600, 8, 0, 1);

    std::vector<uint8_t> write_data = {0x20, 0x21, 0x22};
#ifdef __linux__
    mock_written_data.clear();  // Clear any previous writes
#endif

    std::future<std::pair<TTYBase::TTYResponse, uint32_t>> future =
        tty.writeAsync(write_data);
    auto result = future.get();

    EXPECT_EQ(result.first, TTYBase::TTYResponse::OK);
    EXPECT_EQ(result.second, 3);
#ifdef __linux__
    EXPECT_EQ(mock_written_data, write_data);
#endif

    tty.disconnect();
}

// Test: Async Read Thread with Data Callback
// This test is difficult to implement reliably without a more sophisticated
// mock for the internal Impl class's data callback mechanism.
// The current mock for `read` and `select` is global and doesn't directly
// expose the internal callback.
// For now, we'll skip direct testing of the `m_DataCallback` and `m_DataQueue`
// interaction, as it's an internal implementation detail.
// The `readAsync` and `writeAsync` tests cover the public async interface.

// Test: Async Read Thread Stop (implicitly by disconnect)
TEST_F(TTYBaseTest, AsyncReadThreadStop) {
    TTYBase tty("TestDriver");
    mock_successful_connect();
    tty.connect("/dev/ttyS0", 9600, 8, 0, 1);

    // Give some time for the async thread to start
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Disconnect should stop the async thread
    TTYBase::TTYResponse response = tty.disconnect();
    EXPECT_EQ(response, TTYBase::TTYResponse::OK);

    // Verify that the thread has indeed stopped (no crash, no lingering
    // activity) This is hard to assert directly, but the test passing without
    // crash is a good sign. A more robust test would involve checking thread
    // join status or internal flags.
}

// Test: makeByteSpan helper
TEST(TTYBaseHelperTest, MakeByteSpan) {
    std::vector<char> char_vec = {'a', 'b', 'c'};
    auto span_char = makeByteSpan(char_vec);
    EXPECT_EQ(span_char.size(), 3);
    EXPECT_EQ(span_char[0], 'a');

    std::array<uint8_t, 2> uint8_arr = {0xDE, 0xAD};
    auto span_uint8 = makeByteSpan(uint8_arr);
    EXPECT_EQ(span_uint8.size(), 2);
    EXPECT_EQ(span_uint8[0], 0xDE);

    // Test with a non-byte-like type (should fail to compile if concept is
    // strict) std::vector<int> int_vec = {1, 2, 3}; auto span_int =
    // makeByteSpan(int_vec); // This line should cause a compile error
}

}  // namespace atom::connection::test