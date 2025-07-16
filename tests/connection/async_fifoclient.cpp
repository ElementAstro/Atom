// filepath: /home/max/Atom/atom/connection/test_async_fifoclient.hpp
#ifndef ATOM_CONNECTION_TEST_ASYNC_FIFOCLIENT_HPP
#define ATOM_CONNECTION_TEST_ASYNC_FIFOCLIENT_HPP

#include <gtest/gtest.h>
#include "atom/connection/async_fifoclient.hpp"

#include <string>
#include <thread>
#include <chrono>
#include <vector>
#include <future>
#include <iostream> // For logging errors in helpers

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <cstring> // For strerror
#endif

// Helper functions for managing FIFOs and simulating the other end
namespace {
  static int fifo_counter = 0;

  std::string generate_unique_fifo_path() {
#ifdef _WIN32
    // Windows named pipes are in the format \\.\pipe\pipename
    // These tests won't create the server side, so paths are for client side tests.
    return "\\\\.\\pipe\\test_fifo_" + std::to_string(++fifo_counter);
#else
    // POSIX FIFOs are filesystem entries
    return "/tmp/test_fifo_" + std::to_string(++fifo_counter);
#endif
  }

#ifndef _WIN32
  void create_fifo(const std::string& path) {
    if (mkfifo(path.c_str(), 0666) == -1) {
      if (errno != EEXIST) {
        FAIL() << "Failed to create FIFO " << path << ": " << strerror(errno);
      }
    }
  }

  void remove_fifo(const std::string& path) {
    if (unlink(path.c_str()) == -1) {
      // Ignore ENOENT (file not found) as it might have been removed by a test
      if (errno != ENOENT) {
        // Log error but don't fail test teardown
        std::cerr << "Failed to remove FIFO " << path << ": " << strerror(errno) << std::endl;
      }
    }
  }

  // Helper to write to FIFO from test thread (simulating the other end)
  void write_to_fifo(const std::string& path, const std::string& data) {
    int fd = -1;
    // Open blocking write
    while ((fd = ::open(path.c_str(), O_WRONLY)) == -1 && errno == EINTR);

    if (fd == -1) {
      FAIL() << "Failed to open FIFO for writing: " << strerror(errno);
      return;
    }

    size_t total_written = 0;
    while (total_written < data.size()) {
      ssize_t written = ::write(fd, data.c_str() + total_written, data.size() - total_written);
      if (written == -1) {
        if (errno == EINTR) continue;
        ::close(fd);
        FAIL() << "Failed to write to FIFO: " << strerror(errno);
        return;
      }
      if (written == 0) {
         // Should not happen with blocking write unless pipe is closed by reader
         ::close(fd);
         FAIL() << "Zero bytes written to FIFO unexpectedly";
         return;
      }
      total_written += written;
    }
    ::close(fd);
  }

  // Helper to read from FIFO from test thread (simulating the other end)
  std::string read_from_fifo(const std::string& path, size_t expected_size) {
    int fd = -1;
    // Open blocking read
     while ((fd = ::open(path.c_str(), O_RDONLY)) == -1 && errno == EINTR);

    if (fd == -1) {
      FAIL() << "Failed to open FIFO for reading: " << strerror(errno);
      return "";
    }

    std::vector<char> buffer(expected_size);
    size_t total_read = 0;
    while (total_read < expected_size) {
      ssize_t bytes_read = ::read(fd, buffer.data() + total_read, expected_size - total_read);
      if (bytes_read == -1) {
         if (errno == EINTR) continue;
         ::close(fd);
         FAIL() << "Failed to read from FIFO: " << strerror(errno);
         return "";
      }
      if (bytes_read == 0) {
        // EOF before reading expected size
        ::close(fd);
        FAIL() << "EOF encountered before reading expected size from FIFO";
        return "";
      }
      total_read += bytes_read;
    }

    ::close(fd);
    return std::string(buffer.data(), total_read);
  }
#endif // _WIN32
} // namespace


class FifoClientTest : public ::testing::Test {
protected:
  std::string fifo_path_;

  void SetUp() override {
    fifo_path_ = generate_unique_fifo_path();
#ifndef _WIN32
    // Create the FIFO file for POSIX tests
    create_fifo(fifo_path_);
#endif
  }

  void TearDown() override {
#ifndef _WIN32
    // Remove the FIFO file after POSIX tests
    remove_fifo(fifo_path_);
#endif
  }
};

TEST_F(FifoClientTest, DefaultConstructor) {
  atom::async::connection::FifoClient client;
  EXPECT_FALSE(client.isOpen());
  EXPECT_EQ(client.getPath(), "");
}

TEST_F(FifoClientTest, PathConstructor) {
#ifndef _WIN32 // Path constructor opens the pipe on POSIX
  atom::async::connection::FifoClient client(fifo_path_);
  EXPECT_TRUE(client.isOpen());
  EXPECT_EQ(client.getPath(), fifo_path_);
#else // Windows path constructor doesn't open, open() must be called
  atom::async::connection::FifoClient client(fifo_path_);
  EXPECT_FALSE(client.isOpen()); // Should not be open until open() is called
  EXPECT_EQ(client.getPath(), fifo_path_); // Path should be stored
#endif
}

TEST_F(FifoClientTest, OpenClose) {
  atom::async::connection::FifoClient client;
  EXPECT_FALSE(client.isOpen());
  client.open(fifo_path_);
  EXPECT_TRUE(client.isOpen());
  EXPECT_EQ(client.getPath(), fifo_path_);
  client.close();
  EXPECT_FALSE(client.isOpen());
}

TEST_F(FifoClientTest, OpenAlreadyOpen) {
  atom::async::connection::FifoClient client;
  client.open(fifo_path_);
  EXPECT_TRUE(client.isOpen());
  EXPECT_THROW({ client.open(fifo_path_); }, std::runtime_error);
  EXPECT_TRUE(client.isOpen()); // Should still be open
}

TEST_F(FifoClientTest, OpenInvalidPath) {
  atom::async::connection::FifoClient client;
  // Use a path that should definitely fail to open
#ifndef _WIN32
  std::string bad_path = "/sys/test_fifo_bad"; // System directory, should fail mkfifo/open
#else
  // On Windows, CreateFileA with OPEN_EXISTING will fail if pipe server isn't running.
  // This test verifies the exception is thrown.
  std::string bad_path = "\\\\.\\pipe\\nonexistent_pipe_12345";
#endif
  EXPECT_THROW({ client.open(bad_path); }, std::runtime_error);
  EXPECT_FALSE(client.isOpen());
}

TEST_F(FifoClientTest, IsOpen) {
  atom::async::connection::FifoClient client;
  EXPECT_FALSE(client.isOpen());
  client.open(fifo_path_);
  EXPECT_TRUE(client.isOpen());
  client.close();
  EXPECT_FALSE(client.isOpen());
}

TEST_F(FifoClientTest, GetPath) {
  atom::async::connection::FifoClient client;
  EXPECT_EQ(client.getPath(), "");
  client.open(fifo_path_);
  EXPECT_EQ(client.getPath(), fifo_path_);
  client.close();
  EXPECT_EQ(client.getPath(), fifo_path_); // Path should be retained after close
}

// POSIX specific tests requiring FIFO read/write simulation
#ifndef _WIN32
TEST_F(FifoClientTest, WriteSync) {
  atom::async::connection::FifoClient client;
  client.open(fifo_path_);
  std::string test_data = "Hello, FIFO!\n";

  // Simulate the reader in a separate thread
  std::thread reader_thread([&]() {
    std::string read_data = read_from_fifo(fifo_path_, test_data.size());
    EXPECT_EQ(read_data, test_data);
  });

  // Client writes
  bool success = client.writeSync(test_data);
  EXPECT_TRUE(success);

  reader_thread.join();
  client.close();
}

TEST_F(FifoClientTest, ReadSync) {
  atom::async::connection::FifoClient client;
  client.open(fifo_path_);
  std::string test_data = "Data from writer\n";

  // Simulate the writer in a separate thread
  std::thread writer_thread([&]() {
    // Give the client read a moment to start blocking
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    write_to_fifo(fifo_path_, test_data);
  });

  // Client reads
  auto result = client.readSync();
  EXPECT_TRUE(result.has_value());
  EXPECT_EQ(result.value(), test_data);

  writer_thread.join();
  client.close();
}

TEST_F(FifoClientTest, WriteSyncTimeout) {
  atom::async::connection::FifoClient client;
  client.open(fifo_path_);
  std::string test_data = "Timeout test\n";

  // Client writes with a short timeout. No reader is present, so it should time out.
  auto start_time = std::chrono::steady_clock::now();
  bool success = client.writeSync(test_data, std::chrono::milliseconds(50));
  auto end_time = std::chrono::steady_clock::now();

  EXPECT_FALSE(success);
  // Check if it actually waited approximately the timeout duration
  EXPECT_GE(std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count(), 40); // Allow some jitter
  EXPECT_LE(std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count(), 200); // Upper bound

  client.close();
}

TEST_F(FifoClientTest, ReadSyncTimeout) {
  atom::async::connection::FifoClient client;
  client.open(fifo_path_);

  // Client reads with a short timeout. No writer is present, so it should time out.
  auto start_time = std::chrono::steady_clock::now();
  auto result = client.readSync(std::chrono::milliseconds(50));
  auto end_time = std::chrono::steady_clock::now();

  EXPECT_FALSE(result.has_value());
  // Check if it actually waited approximately the timeout duration
  EXPECT_GE(std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count(), 40); // Allow some jitter
  EXPECT_LE(std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count(), 200); // Upper bound

  client.close();
}

TEST_F(FifoClientTest, WriteAsync) {
  atom::async::connection::FifoClient client;
  client.open(fifo_path_);
  std::string test_data = "Async write test\n";

  // Simulate the reader in a separate thread
  std::thread reader_thread([&]() {
    std::string read_data = read_from_fifo(fifo_path_, test_data.size());
    EXPECT_EQ(read_data, test_data);
  });

  // Client writes asynchronously
  auto future = client.write(test_data);

  // Wait for the write to complete
  bool success = future.get();
  EXPECT_TRUE(success);

  reader_thread.join();
  client.close();
}

TEST_F(FifoClientTest, ReadAsync) {
  atom::async::connection::FifoClient client;
  client.open(fifo_path_);
  std::string test_data = "Async read test\n";

  // Simulate the writer in a separate thread
  std::thread writer_thread([&]() {
    // Give the client read a moment to start blocking
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    write_to_fifo(fifo_path_, test_data);
  });

  // Client reads asynchronously
  auto future = client.read();

  // Wait for the read to complete
  auto result = future.get();
  EXPECT_TRUE(result.has_value());
  EXPECT_EQ(result.value(), test_data);

  writer_thread.join();
  client.close();
}

TEST_F(FifoClientTest, WriteAsyncTimeout) {
  atom::async::connection::FifoClient client;
  client.open(fifo_path_);
  std::string test_data = "Async timeout write\n";

  // Client writes asynchronously with a short timeout. No reader is present.
  auto start_time = std::chrono::steady_clock::now();
  auto future = client.write(test_data, std::chrono::milliseconds(50));

  // Wait for the future to complete (due to timeout)
  bool success = future.get();
  auto end_time = std::chrono::steady_clock::now();

  EXPECT_FALSE(success);
  EXPECT_GE(std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count(), 40); // Allow some jitter
  EXPECT_LE(std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count(), 200); // Upper bound

  client.close();
}

TEST_F(FifoClientTest, ReadAsyncTimeout) {
  atom::async::connection::FifoClient client;
  client.open(fifo_path_);

  // Client reads asynchronously with a short timeout. No writer is present.
  auto start_time = std::chrono::steady_clock::now();
  auto future = client.read(std::chrono::milliseconds(50));

  // Wait for the future to complete (due to timeout)
  auto result = future.get();
  auto end_time = std::chrono::steady_clock::now();

  EXPECT_FALSE(result.has_value());
  EXPECT_GE(std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count(), 40); // Allow some jitter
  EXPECT_LE(std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count(), 200); // Upper bound

  client.close();
}

TEST_F(FifoClientTest, CancelAsyncRead) {
  atom::async::connection::FifoClient client;
  client.open(fifo_path_);

  // Start an async read that will block
  auto future = client.read();

  // Give the read operation time to start blocking
  std::this_thread::sleep_for(std::chrono::milliseconds(10));

  // Cancel the operation
  client.cancel();

  // Wait for the future to complete (due to cancellation)
  auto result = future.get();

  // Expect the result to be empty due to cancellation/error
  EXPECT_FALSE(result.has_value());

  client.close();
}

TEST_F(FifoClientTest, CancelAsyncWrite) {
  atom::async::connection::FifoClient client;
  client.open(fifo_path_);
  std::string test_data = "Data to cancel\n";

  // Start an async write that will block (no reader)
  auto future = client.write(test_data);

  // Give the write operation time to start blocking
  std::this_thread::sleep_for(std::chrono::milliseconds(10));

  // Cancel the operation
  client.cancel();

  // Wait for the future to complete (due to cancellation)
  bool success = future.get();

  // Expect the write to fail due to cancellation/error
  EXPECT_FALSE(success);

  client.close();
}
#endif // _WIN32

TEST_F(FifoClientTest, MoveConstructor) {
  atom::async::connection::FifoClient original_client;
  original_client.open(fifo_path_);
  EXPECT_TRUE(original_client.isOpen());
  EXPECT_EQ(original_client.getPath(), fifo_path_);

  atom::async::connection::FifoClient moved_client = std::move(original_client);

  EXPECT_TRUE(moved_client.isOpen());
  EXPECT_EQ(moved_client.getPath(), fifo_path_);

  // Original client should be in a valid but unspecified state (likely closed/invalid)
  // The unique_ptr pimpl_ will be null in the original object.
  // Calling methods on the moved-from object is undefined behavior,
  // but checking isOpen() might be safe if the implementation handles null pimpl_.
  // Let's just check the moved_client state.

  moved_client.close();
  EXPECT_FALSE(moved_client.isOpen());
}

TEST_F(FifoClientTest, MoveAssignment) {
  atom::async::connection::FifoClient client1;
  client1.open(fifo_path_);
  EXPECT_TRUE(client1.isOpen());

  // Create a second client, potentially with a different path if needed, but here just default
  atom::async::connection::FifoClient client2;
  // client2 is not open

  client1 = std::move(client2); // Move client2 (closed) into client1 (open)

  // client1 should now be in the state of client2 (closed)
  EXPECT_FALSE(client1.isOpen());
  // Path might be empty or the original path of client2, depending on impl
  // Let's assume path is moved/cleared.
  // EXPECT_EQ(client1.getPath(), ""); // This might depend on Impl move semantics

  // Test moving an open client
  atom::async::connection::FifoClient client3;
  client3.open(fifo_path_);
  EXPECT_TRUE(client3.isOpen());
  std::string other_fifo_path = generate_unique_fifo_path();
#ifndef _WIN32
  create_fifo(other_fifo_path);
#endif
  atom::async::connection::FifoClient client4;
  client4.open(other_fifo_path);
  EXPECT_TRUE(client4.isOpen());
  EXPECT_EQ(client4.getPath(), other_fifo_path);

  client3 = std::move(client4); // Move client4 (open) into client3 (open)

  // client3 should now be in the state of client4
  EXPECT_TRUE(client3.isOpen());
  EXPECT_EQ(client3.getPath(), other_fifo_path);

#ifndef _WIN32
  remove_fifo(other_fifo_path);
#endif
}

TEST_F(FifoClientTest, WriteToClosed) {
  atom::async::connection::FifoClient client; // Starts closed
  std::string test_data = "Should fail\n";

  // Sync write
  bool sync_success = client.writeSync(test_data);
  EXPECT_FALSE(sync_success);

  // Async write
  auto future = client.write(test_data);
  bool async_success = future.get();
  EXPECT_FALSE(async_success);
}

TEST_F(FifoClientTest, ReadFromClosed) {
  atom::async::connection::FifoClient client; // Starts closed

  // Sync read
  auto sync_result = client.readSync();
  EXPECT_FALSE(sync_result.has_value());

  // Async read
  auto future = client.read();
  auto async_result = future.get();
  EXPECT_FALSE(async_result.has_value());
}

#endif // ATOM_CONNECTION_TEST_ASYNC_FIFOCLIENT_HPP
