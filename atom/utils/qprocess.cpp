#include "qprocess.hpp"

#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstring>
#include <filesystem>
#include <future>
#include <mutex>
#include <thread>
#include <utility>

#ifdef _WIN32
#include <windows.h>
#else
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

#include "atom/error/exception.hpp"
#include "atom/log/loguru.hpp"

namespace atom::utils {

namespace {
constexpr size_t BUFFER_SIZE =
    16384;  // Increased buffer size for better performance

// Helper to check if a directory exists and is accessible
bool isDirectoryAccessible(const std::filesystem::path& dir) {
    try {
        return std::filesystem::is_directory(dir) &&
               std::filesystem::exists(dir);
    } catch (const std::filesystem::filesystem_error& e) {
        LOG_F(ERROR, "Filesystem error: {}", e.what());
        return false;
    }
}

// Helper to set non-blocking mode for file descriptors (POSIX only)
#ifndef _WIN32
void setNonBlocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) {
        LOG_F(ERROR, "Failed to get file descriptor flags");
        throw std::runtime_error("Failed to set non-blocking mode for pipe");
    }

    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) {
        LOG_F(ERROR, "Failed to set non-blocking flag");
        throw std::runtime_error("Failed to set non-blocking mode for pipe");
    }
}
#endif
}  // namespace

class QProcess::Impl {
public:
    Impl() = default;
    ~Impl();

    void setWorkingDirectory(std::string_view dir);
    void setEnvironment(std::vector<std::string>&& env);

    void start(std::string&& program, std::vector<std::string>&& args);
    auto waitForStarted(int timeoutMs) -> bool;
    auto waitForFinished(int timeoutMs) -> bool;
    [[nodiscard]] auto isRunning() const -> bool;

    void write(std::string_view data);
    auto readAllStandardOutput() -> std::string;
    auto readAllStandardError() -> std::string;
    void terminate() noexcept;

private:
    void startWindowsProcess();
    void startPosixProcess();

    // Create advanced pipe management
    void setupPipes();
    void closePipes() noexcept;

    // Asynchronous read operations
    void startAsyncReaders();
    void stopAsyncReaders() noexcept;

    std::atomic<bool> running_{false};
    std::atomic<bool> processStarted_{false};
    std::string program_;
    std::vector<std::string> args_;
    std::optional<std::string> workingDirectory_;
    std::vector<std::string> environment_;

    mutable std::mutex mutex_;
    std::condition_variable cv_;

    // Async reading state
    std::atomic<bool> asyncReadersRunning_{false};
    std::future<void> stdoutReaderFuture_;
    std::future<void> stderrReaderFuture_;

    // Thread-safe buffers for output
    std::mutex stdoutMutex_;
    std::mutex stderrMutex_;
    std::string stdoutBuffer_;
    std::string stderrBuffer_;

#ifdef _WIN32
    PROCESS_INFORMATION procInfo_{};
    HANDLE childStdinWrite_{nullptr};
    HANDLE childStdoutRead_{nullptr};
    HANDLE childStderrRead_{nullptr};
#else
    pid_t childPid_{-1};
    int childStdin_{-1};
    int childStdout_{-1};
    int childStderr_{-1};
#endif
};

// Validate environment variable format
void QProcess::validateEnvironmentVariable(std::string_view var) {
    if (var.empty()) {
        throw std::invalid_argument("Environment variable cannot be empty");
    }

    // Environment variables should be in the format NAME=VALUE
    if (var.find('=') == std::string::npos) {
        throw std::invalid_argument(
            "Environment variable must be in format NAME=VALUE");
    }
}

// Implementation of QProcess
QProcess::QProcess() : impl_(std::make_unique<Impl>()) {
    LOG_F(INFO, "QProcess constructor called");
}

QProcess::~QProcess() noexcept {
    try {
        LOG_F(INFO, "QProcess destructor called");
        if (impl_ && impl_->isRunning()) {
            impl_->terminate();
        }
    } catch (...) {
        LOG_F(ERROR, "Exception caught in QProcess destructor");
    }
}

QProcess::QProcess(QProcess&& other) noexcept : impl_(std::move(other.impl_)) {
    LOG_F(INFO, "QProcess move constructor called");
}

QProcess& QProcess::operator=(QProcess&& other) noexcept {
    if (this != &other) {
        try {
            if (impl_ && impl_->isRunning()) {
                impl_->terminate();
            }
        } catch (...) {
            LOG_F(ERROR,
                  "Exception caught in QProcess move assignment operator");
        }

        impl_ = std::move(other.impl_);
        LOG_F(INFO, "QProcess move assignment called");
    }
    return *this;
}

void QProcess::setWorkingDirectory(std::string_view dir) {
    LOG_F(INFO, "QProcess::setWorkingDirectory called with dir: {}", dir);

    // Validate the directory exists and is accessible
    if (!isDirectoryAccessible(std::filesystem::path(dir))) {
        LOG_F(ERROR,
              "Working directory does not exist or is not accessible: {}", dir);
        throw std::invalid_argument(
            "Working directory does not exist or is not accessible");
    }

    impl_->setWorkingDirectory(dir);
}

void QProcess::setEnvironmentImpl(std::vector<std::string>&& env) {
    LOG_F(INFO, "QProcess::setEnvironment called");
    impl_->setEnvironment(std::move(env));
}

void QProcess::startImpl(std::string&& program,
                         std::vector<std::string>&& args) {
    LOG_F(INFO, "QProcess::start called with program: {}", program);

    // Check if program exists (on POSIX systems)
#ifndef _WIN32
    if (!std::filesystem::exists(program)) {
        LOG_F(WARNING, "Program may not exist: {}", program);
        // Not throwing here as the PATH might resolve the executable
    }
#endif

    try {
        impl_->start(std::move(program), std::move(args));
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Failed to start process: {}", e.what());
        throw;
    }
}

auto QProcess::waitForStartedImpl(int timeoutMs) -> bool {
    LOG_F(INFO, "QProcess::waitForStarted called with timeoutMs: {}",
          timeoutMs);
    try {
        return impl_->waitForStarted(timeoutMs);
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Exception in waitForStarted: {}", e.what());
        return false;
    }
}

auto QProcess::waitForFinishedImpl(int timeoutMs) -> bool {
    LOG_F(INFO, "QProcess::waitForFinished called with timeoutMs: {}",
          timeoutMs);
    try {
        return impl_->waitForFinished(timeoutMs);
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Exception in waitForFinished: {}", e.what());
        return false;
    }
}

auto QProcess::isRunning() const noexcept -> bool {
    try {
        LOG_F(INFO, "QProcess::isRunning called");
        return impl_->isRunning();
    } catch (...) {
        LOG_F(ERROR, "Exception caught in isRunning");
        return false;
    }
}

void QProcess::write(std::string_view data) {
    LOG_F(INFO, "QProcess::write called with data length: {}", data.size());
    if (data.empty()) {
        return;
    }

    try {
        impl_->write(data);
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Failed to write to process: {}", e.what());
        throw std::runtime_error(std::string("Failed to write to process: ") +
                                 e.what());
    }
}

auto QProcess::readAllStandardOutput() -> std::string {
    LOG_F(INFO, "QProcess::readAllStandardOutput called");
    try {
        return impl_->readAllStandardOutput();
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Failed to read standard output: {}", e.what());
        throw std::runtime_error(
            std::string("Failed to read standard output: ") + e.what());
    }
}

auto QProcess::readAllStandardError() -> std::string {
    LOG_F(INFO, "QProcess::readAllStandardError called");
    try {
        return impl_->readAllStandardError();
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Failed to read standard error: {}", e.what());
        throw std::runtime_error(
            std::string("Failed to read standard error: ") + e.what());
    }
}

void QProcess::terminate() noexcept {
    try {
        LOG_F(INFO, "QProcess::terminate called");
        impl_->terminate();
    } catch (...) {
        LOG_F(ERROR, "Exception caught in terminate");
    }
}

// Implementation details of QProcess::Impl
QProcess::Impl::~Impl() {
    try {
        LOG_F(INFO, "QProcess::Impl destructor called");
        if (running_) {
            terminate();
        }
        stopAsyncReaders();
        closePipes();
    } catch (...) {
        LOG_F(ERROR, "Exception caught in QProcess::Impl destructor");
    }
}

void QProcess::Impl::setWorkingDirectory(std::string_view dir) {
    LOG_F(INFO, "QProcess::Impl::setWorkingDirectory called with dir: {}", dir);
    workingDirectory_ = dir;
}

void QProcess::Impl::setEnvironment(std::vector<std::string>&& env) {
    LOG_F(INFO, "QProcess::Impl::setEnvironment called");
    environment_ = std::move(env);
}

void QProcess::Impl::start(std::string&& program,
                           std::vector<std::string>&& args) {
    LOG_F(INFO, "QProcess::Impl::start called with program: {}", program);
    if (running_) {
        LOG_F(ERROR, "Process already running");
        THROW_RUNTIME_ERROR("Process already running");
    }

    this->program_ = std::move(program);
    this->args_ = std::move(args);

#ifdef _WIN32
    startWindowsProcess();
#else
    startPosixProcess();
#endif

    running_ = true;
    {
        std::lock_guard lock(mutex_);
        processStarted_ = true;
    }
    cv_.notify_all();
    LOG_F(INFO, "QProcess::Impl::start completed");
}

auto QProcess::Impl::waitForStarted(int timeoutMs) -> bool {
    LOG_F(INFO, "QProcess::Impl::waitForStarted called with timeoutMs: {}",
          timeoutMs);
    std::unique_lock lock(mutex_);
    if (timeoutMs < 0) {
        cv_.wait(lock, [this] { return processStarted_.load(); });
    } else {
        if (!cv_.wait_for(lock, std::chrono::milliseconds(timeoutMs),
                          [this] { return processStarted_.load(); })) {
            LOG_F(WARNING, "QProcess::Impl::waitForStarted timed out");
            return false;
        }
    }
    LOG_F(INFO, "QProcess::Impl::waitForStarted completed");
    return true;
}

auto QProcess::Impl::waitForFinished(int timeoutMs) -> bool {
    LOG_F(INFO, "QProcess::Impl::waitForFinished called with timeoutMs: {}",
          timeoutMs);
#ifdef _WIN32
    DWORD waitResult = WaitForSingleObject(
        procInfo_.hProcess, timeoutMs < 0 ? INFINITE : timeoutMs);
    bool result = waitResult == WAIT_OBJECT_0;
    LOG_F(INFO, "QProcess::Impl::waitForFinished completed with result: {}",
          result ? "true" : "false");
    return result;
#else
    if (childPid_ == -1) {
        LOG_F(WARNING,
              "QProcess::Impl::waitForFinished called with invalid childPid");
        return false;
    }

    int status;
    if (timeoutMs < 0) {
        waitpid(childPid_, &status, 0);
    } else {
        auto startTime = std::chrono::steady_clock::now();
        while (true) {
            auto currentTime = std::chrono::steady_clock::now();
            auto elapsed =
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    currentTime - startTime);
            if (elapsed.count() >= timeoutMs) {
                LOG_F(WARNING, "QProcess::Impl::waitForFinished timed out");
                return false;
            }
            if (waitpid(childPid_, &status, WNOHANG) > 0) {
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
    LOG_F(INFO, "QProcess::Impl::waitForFinished completed");
    return true;
#endif
}

auto QProcess::Impl::isRunning() const -> bool {
    LOG_F(INFO, "QProcess::Impl::isRunning called");
#ifdef _WIN32
    DWORD exitCode;
    GetExitCodeProcess(procInfo_.hProcess, &exitCode);
    bool result = exitCode == STILL_ACTIVE;
    LOG_F(INFO, "QProcess::Impl::isRunning returning: {}",
          result ? "true" : "false");
    return result;
#else
    if (childPid_ == -1) {
        LOG_F(WARNING,
              "QProcess::Impl::isRunning called with invalid childPid");
        return false;
    }
    int status;
    bool result = waitpid(childPid_, &status, WNOHANG) == 0;
    LOG_F(INFO, "QProcess::Impl::isRunning returning: {}",
          result ? "true" : "false");
    return result;
#endif
}

void QProcess::Impl::terminate() noexcept {
    LOG_F(INFO, "QProcess::Impl::terminate called");
    if (running_) {
#ifdef _WIN32
        TerminateProcess(procInfo_.hProcess, 0);
        CloseHandle(procInfo_.hProcess);
        CloseHandle(procInfo_.hThread);
#else
        kill(childPid_, SIGTERM);
#endif
        running_ = false;
    }
    LOG_F(INFO, "QProcess::Impl::terminate completed");
}

#ifdef _WIN32
void QProcess::Impl::startWindowsProcess() {
    LOG_F(INFO, "QProcess::Impl::startWindowsProcess called");
    try {
        SECURITY_ATTRIBUTES saAttr;
        saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
        saAttr.bInheritHandle = TRUE;
        saAttr.lpSecurityDescriptor = nullptr;

        HANDLE childStdoutWrite = nullptr;
        HANDLE childStdinRead = nullptr;
        HANDLE childStderrWrite = nullptr;

        // Create pipes with error handling
        if ((CreatePipe(&childStdoutRead_, &childStdoutWrite, &saAttr, 0) ==
             0) ||
            (SetHandleInformation(childStdoutRead_, HANDLE_FLAG_INHERIT, 0) ==
             0)) {
            LOG_F(ERROR, "Failed to create stdout pipe");
            THROW_SYSTEM_COLLAPSE("Failed to create stdout pipe");
        }

        if ((CreatePipe(&childStdinRead, &childStdinWrite_, &saAttr, 0) == 0) ||
            (SetHandleInformation(childStdinWrite_, HANDLE_FLAG_INHERIT, 0) ==
             0)) {
            LOG_F(ERROR, "Failed to create stdin pipe");
            CloseHandle(childStdoutRead_);
            CloseHandle(childStdoutWrite);
            THROW_SYSTEM_COLLAPSE("Failed to create stdin pipe");
        }

        if ((CreatePipe(&childStderrRead_, &childStderrWrite, &saAttr, 0) ==
             0) ||
            (SetHandleInformation(childStderrRead_, HANDLE_FLAG_INHERIT, 0) ==
             0)) {
            LOG_F(ERROR, "Failed to create stderr pipe");
            CloseHandle(childStdoutRead_);
            CloseHandle(childStdoutWrite);
            CloseHandle(childStdinRead);
            CloseHandle(childStdinWrite_);
            THROW_SYSTEM_COLLAPSE("Failed to create stderr pipe");
        }

        STARTUPINFO siStartInfo;
        ZeroMemory(&siStartInfo, sizeof(STARTUPINFO));
        siStartInfo.cb = sizeof(STARTUPINFO);
        siStartInfo.hStdError = childStderrWrite;
        siStartInfo.hStdOutput = childStdoutWrite;
        siStartInfo.hStdInput = childStdinRead;
        siStartInfo.dwFlags |= STARTF_USESTDHANDLES;

        ZeroMemory(&procInfo_, sizeof(PROCESS_INFORMATION));

        // Build command line using proper quoting for spaces
        std::string cmdLine = program_;
        for (const auto& arg : args_) {
            // Add proper quoting for arguments with spaces
            if (arg.find(' ') != std::string::npos) {
                cmdLine += " \"" + arg + "\"";
            } else {
                cmdLine += " " + arg;
            }
        }

        // Prepare environment block if needed
        std::vector<char> envBlock;
        if (!environment_.empty()) {
            std::string tempBlock;
            for (const auto& envVar : environment_) {
                tempBlock += envVar + '\0';
            }
            tempBlock += '\0';
            envBlock.assign(tempBlock.begin(), tempBlock.end());
        }

        // Start the child process
        if (!CreateProcess(
                nullptr, cmdLine.data(), nullptr, nullptr, TRUE, 0,
                envBlock.empty() ? nullptr : envBlock.data(),
                workingDirectory_ ? workingDirectory_->c_str() : nullptr,
                &siStartInfo, &procInfo_)) {
            LOG_F(ERROR, "Failed to start process: {}", GetLastError());

            CloseHandle(childStdoutRead_);
            CloseHandle(childStdoutWrite);
            CloseHandle(childStdinRead);
            CloseHandle(childStdinWrite_);
            CloseHandle(childStderrRead_);
            CloseHandle(childStderrWrite);

            THROW_SYSTEM_COLLAPSE("Failed to start process");
        }

        // Close pipe ends that belong to the child process
        CloseHandle(childStdoutWrite);
        CloseHandle(childStdinRead);
        CloseHandle(childStderrWrite);

        // Start asynchronous readers
        setupPipes();
        startAsyncReaders();
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Exception in startWindowsProcess: {}", e.what());
        throw;
    }

    LOG_F(INFO, "QProcess::Impl::startWindowsProcess completed");
}
#else
void QProcess::Impl::startPosixProcess() {
    LOG_F(INFO, "QProcess::Impl::startPosixProcess called");
    int stdinPipe[2] = {-1, -1};
    int stdoutPipe[2] = {-1, -1};
    int stderrPipe[2] = {-1, -1};

    try {
        // Create pipes with error handling
        if (pipe(stdinPipe) == -1) {
            LOG_F(ERROR, "Failed to create stdin pipe: {}", strerror(errno));
            throw std::runtime_error("Failed to create stdin pipe");
        }

        if (pipe(stdoutPipe) == -1) {
            LOG_F(ERROR, "Failed to create stdout pipe: {}", strerror(errno));
            close(stdinPipe[0]);
            close(stdinPipe[1]);
            throw std::runtime_error("Failed to create stdout pipe");
        }

        if (pipe(stderrPipe) == -1) {
            LOG_F(ERROR, "Failed to create stderr pipe: {}", strerror(errno));
            close(stdinPipe[0]);
            close(stdinPipe[1]);
            close(stdoutPipe[0]);
            close(stdoutPipe[1]);
            throw std::runtime_error("Failed to create stderr pipe");
        }

        // Fork with proper error handling
        childPid_ = fork();

        if (childPid_ == -1) {
            LOG_F(ERROR, "Failed to fork process: {}", strerror(errno));
            close(stdinPipe[0]);
            close(stdinPipe[1]);
            close(stdoutPipe[0]);
            close(stdoutPipe[1]);
            close(stderrPipe[0]);
            close(stderrPipe[1]);
            throw std::runtime_error("Failed to fork process");
        }

        if (childPid_ == 0) {  // Child process
            try {
                // Close unnecessary pipe ends
                close(stdinPipe[1]);
                close(stdoutPipe[0]);
                close(stderrPipe[0]);

                // Redirect standard input/output/error
                if (dup2(stdinPipe[0], STDIN_FILENO) == -1 ||
                    dup2(stdoutPipe[1], STDOUT_FILENO) == -1 ||
                    dup2(stderrPipe[1], STDERR_FILENO) == -1) {
                    LOG_F(ERROR, "Failed to duplicate file descriptors: {}",
                          strerror(errno));
                    exit(1);
                }

                // Close original pipe descriptors
                close(stdinPipe[0]);
                close(stdoutPipe[1]);
                close(stderrPipe[1]);

                // Change directory if set
                if (workingDirectory_ && !workingDirectory_->empty()) {
                    if (chdir(workingDirectory_->c_str()) != 0) {
                        LOG_F(ERROR, "Failed to change directory to {}: {}",
                              *workingDirectory_, strerror(errno));
                        exit(1);
                    }
                }

                // Set environment variables
                if (!environment_.empty()) {
                    for (const auto& envVar : environment_) {
                        if (putenv(const_cast<char*>(envVar.c_str())) != 0) {
                            LOG_F(ERROR,
                                  "Failed to set environment variable: {}",
                                  strerror(errno));
                        }
                    }
                }

                // Build exec argument list
                std::vector<char*> execArgs;
                execArgs.reserve(args_.size() +
                                 2);  // +1 for program, +1 for nullptr
                execArgs.push_back(const_cast<char*>(program_.c_str()));

                for (const auto& arg : args_) {
                    execArgs.push_back(const_cast<char*>(arg.c_str()));
                }
                execArgs.push_back(nullptr);

                // Execute new program
                execvp(execArgs[0], execArgs.data());

                // If we get here, execvp failed
                LOG_F(ERROR, "Failed to execute process '{}': {}", program_,
                      strerror(errno));
                exit(1);
            } catch (...) {
                LOG_F(ERROR, "Unexpected exception in child process");
                exit(1);
            }
        } else {  // Parent process
            // Close unnecessary pipe ends
            close(stdinPipe[0]);
            close(stdoutPipe[1]);
            close(stderrPipe[1]);

            // Store pipe file descriptors for communication
            childStdin_ = stdinPipe[1];
            childStdout_ = stdoutPipe[0];
            childStderr_ = stderrPipe[0];

            // Set up pipes and start async readers
            setupPipes();
            startAsyncReaders();
        }
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Exception in startPosixProcess: {}", e.what());
        throw;
    }

    LOG_F(INFO, "QProcess::Impl::startPosixProcess completed");
}
#endif

void QProcess::Impl::setupPipes() {
    LOG_F(INFO, "QProcess::Impl::setupPipes called");
#ifndef _WIN32
    // Set non-blocking mode for stdout/stderr pipes for asynchronous read
    try {
        if (childStdout_ != -1) {
            setNonBlocking(childStdout_);
        }
        if (childStderr_ != -1) {
            setNonBlocking(childStderr_);
        }
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Failed to set non-blocking mode: {}", e.what());
        throw;
    }
#endif
    LOG_F(INFO, "QProcess::Impl::setupPipes completed");
}

void QProcess::Impl::closePipes() noexcept {
    LOG_F(INFO, "QProcess::Impl::closePipes called");
    try {
#ifdef _WIN32
        if (childStdinWrite_ != nullptr) {
            CloseHandle(childStdinWrite_);
            childStdinWrite_ = nullptr;
        }
        if (childStdoutRead_ != nullptr) {
            CloseHandle(childStdoutRead_);
            childStdoutRead_ = nullptr;
        }
        if (childStderrRead_ != nullptr) {
            CloseHandle(childStderrRead_);
            childStderrRead_ = nullptr;
        }
#else
        if (childStdin_ != -1) {
            close(childStdin_);
            childStdin_ = -1;
        }
        if (childStdout_ != -1) {
            close(childStdout_);
            childStdout_ = -1;
        }
        if (childStderr_ != -1) {
            close(childStderr_);
            childStderr_ = -1;
        }
#endif
    } catch (...) {
        LOG_F(ERROR, "Exception caught in closePipes");
    }
    LOG_F(INFO, "QProcess::Impl::closePipes completed");
}

void QProcess::Impl::startAsyncReaders() {
    LOG_F(INFO, "QProcess::Impl::startAsyncReaders called");

    if (asyncReadersRunning_) {
        LOG_F(WARNING, "Async readers already running");
        return;
    }

    asyncReadersRunning_ = true;

    // Start stdout reader thread
    stdoutReaderFuture_ = std::async(std::launch::async, [this]() {
        std::array<char, BUFFER_SIZE> buffer;
        while (asyncReadersRunning_) {
            try {
#ifdef _WIN32
                DWORD read;
                BOOL success = ReadFile(childStdoutRead_, buffer.data(),
                                        buffer.size(), &read, nullptr);
                if (success && read > 0) {
                    std::lock_guard<std::mutex> lock(stdoutMutex_);
                    stdoutBuffer_.append(buffer.data(), read);
                } else if (!success || read == 0) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                }
#else
                pollfd pfd = {childStdout_, POLLIN, 0};
                int poll_result = poll(&pfd, 1, 100);
                
                if (poll_result > 0 && (pfd.revents & POLLIN)) {
                    ssize_t bytesRead = ::read(childStdout_, buffer.data(), buffer.size());
                    if (bytesRead > 0) {
                        std::lock_guard<std::mutex> lock(stdoutMutex_);
                        stdoutBuffer_.append(buffer.data(), bytesRead);
                    } else if (bytesRead == 0) {
                        // End of file
                        break;
                    }
                } else if (poll_result == 0) {
                    // Timeout - no data available
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                }
#endif
            } catch (const std::exception& e) {
                LOG_F(ERROR, "Exception in stdout reader thread: {}", e.what());
                break;
            }
        }
        LOG_F(INFO, "Stdout reader thread exiting");
    });

    // Start stderr reader thread
    stderrReaderFuture_ = std::async(std::launch::async, [this]() {
        std::array<char, BUFFER_SIZE> buffer;
        while (asyncReadersRunning_) {
            try {
#ifdef _WIN32
                DWORD read;
                BOOL success = ReadFile(childStderrRead_, buffer.data(),
                                        buffer.size(), &read, nullptr);
                if (success && read > 0) {
                    std::lock_guard<std::mutex> lock(stderrMutex_);
                    stderrBuffer_.append(buffer.data(), read);
                } else if (!success || read == 0) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                }
#else
                pollfd pfd = {childStderr_, POLLIN, 0};
                int poll_result = poll(&pfd, 1, 100);
                
                if (poll_result > 0 && (pfd.revents & POLLIN)) {
                    ssize_t bytesRead = ::read(childStderr_, buffer.data(), buffer.size());
                    if (bytesRead > 0) {
                        std::lock_guard<std::mutex> lock(stderrMutex_);
                        stderrBuffer_.append(buffer.data(), bytesRead);
                    } else if (bytesRead == 0) {
                        // End of file
                        break;
                    }
                } else if (poll_result == 0) {
                    // Timeout - no data available
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                }
#endif
            } catch (const std::exception& e) {
                LOG_F(ERROR, "Exception in stderr reader thread: {}", e.what());
                break;
            }
        }
        LOG_F(INFO, "Stderr reader thread exiting");
    });

    LOG_F(INFO, "QProcess::Impl::startAsyncReaders completed");
}

void QProcess::Impl::stopAsyncReaders() noexcept {
    LOG_F(INFO, "QProcess::Impl::stopAsyncReaders called");

    try {
        if (asyncReadersRunning_) {
            asyncReadersRunning_ = false;

            // Wait for reader threads to finish
            if (stdoutReaderFuture_.valid()) {
                stdoutReaderFuture_.wait_for(std::chrono::seconds(2));
            }

            if (stderrReaderFuture_.valid()) {
                stderrReaderFuture_.wait_for(std::chrono::seconds(2));
            }
        }
    } catch (...) {
        LOG_F(ERROR, "Exception caught in stopAsyncReaders");
    }

    LOG_F(INFO, "QProcess::Impl::stopAsyncReaders completed");
}

auto QProcess::Impl::readAllStandardOutput() -> std::string {
    LOG_F(INFO, "QProcess::Impl::readAllStandardOutput called");

    if (asyncReadersRunning_) {
        // Get data from the asynchronous buffer
        std::lock_guard<std::mutex> lock(stdoutMutex_);
        std::string output = std::move(stdoutBuffer_);
        stdoutBuffer_.clear();
        LOG_F(
            INFO,
            "QProcess::Impl::readAllStandardOutput from async buffer: {} bytes",
            output.size());
        return output;
    } else {
        // Fall back to synchronous read
#ifdef _WIN32
        std::string output;
        DWORD bytesRead;
        std::array<char, BUFFER_SIZE> buffer;

        while (ReadFile(childStdoutRead_, buffer.data(), buffer.size(),
                        &bytesRead, nullptr) &&
               bytesRead > 0) {
            output.append(buffer.data(), bytesRead);
        }
#else
        std::string output;
        std::array<char, BUFFER_SIZE> buffer;
        ssize_t bytesRead;

        while ((bytesRead =
                    ::read(childStdout_, buffer.data(), buffer.size())) > 0) {
            output.append(buffer.data(), bytesRead);
        }
#endif
        LOG_F(INFO,
              "QProcess::Impl::readAllStandardOutput from sync read: {} bytes",
              output.size());
        return output;
    }
}

auto QProcess::Impl::readAllStandardError() -> std::string {
    LOG_F(INFO, "QProcess::Impl::readAllStandardError called");

    if (asyncReadersRunning_) {
        // Get data from the asynchronous buffer
        std::lock_guard<std::mutex> lock(stderrMutex_);
        std::string output = std::move(stderrBuffer_);
        stderrBuffer_.clear();
        LOG_F(
            INFO,
            "QProcess::Impl::readAllStandardError from async buffer: {} bytes",
            output.size());
        return output;
    } else {
        // Fall back to synchronous read
#ifdef _WIN32
        std::string output;
        DWORD bytesRead;
        std::array<char, BUFFER_SIZE> buffer;

        while (ReadFile(childStderrRead_, buffer.data(), buffer.size(),
                        &bytesRead, nullptr) &&
               bytesRead > 0) {
            output.append(buffer.data(), bytesRead);
        }
#else
        std::string output;
        std::array<char, BUFFER_SIZE> buffer;
        ssize_t bytesRead;

        while ((bytesRead =
                    ::read(childStderr_, buffer.data(), buffer.size())) > 0) {
            output.append(buffer.data(), bytesRead);
        }
#endif
        LOG_F(INFO,
              "QProcess::Impl::readAllStandardError from sync read: {} bytes",
              output.size());
        return output;
    }
}

void QProcess::Impl::write(std::string_view data) {
    LOG_F(INFO, "QProcess::Impl::write called with data length: {}",
          data.size());

#ifdef _WIN32
    DWORD bytesWritten = 0;
    DWORD bytesToWrite = static_cast<DWORD>(data.size());
    const char* dataPtr = data.data();

    while (bytesToWrite > 0) {
        if (!WriteFile(childStdinWrite_, dataPtr, bytesToWrite, &bytesWritten,
                       nullptr)) {
            DWORD error = GetLastError();
            LOG_F(ERROR, "Failed to write to process: error code {}", error);
            throw std::runtime_error("Failed to write to process");
        }

        bytesToWrite -= bytesWritten;
        dataPtr += bytesWritten;
    }
#else
    const char* dataPtr = data.data();
    size_t bytesToWrite = data.size();

    while (bytesToWrite > 0) {
        ssize_t bytesWritten = ::write(childStdin_, dataPtr, bytesToWrite);

        if (bytesWritten < 0) {
            if (errno == EINTR) {
                // Interrupted by signal, retry
                continue;
            }
            LOG_F(ERROR, "Failed to write to process: {}", strerror(errno));
            throw std::runtime_error("Failed to write to process");
        }

        bytesToWrite -= bytesWritten;
        dataPtr += bytesWritten;
    }
#endif

    LOG_F(INFO, "QProcess::Impl::write completed");
}

}  // namespace atom::utils
