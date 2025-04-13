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
    [[nodiscard]] auto workingDirectory() const -> std::optional<std::string>;
    void setEnvironment(std::vector<std::string>&& env);
    [[nodiscard]] auto environment() const -> std::vector<std::string>;

    void start(std::string&& program, std::vector<std::string>&& args);
    bool startDetached(std::string&& program, std::vector<std::string>&& args);
    auto waitForStarted(int timeoutMs) -> bool;
    auto waitForFinished(int timeoutMs) -> bool;
    [[nodiscard]] auto isRunning() const -> bool;

    void write(std::string_view data);
    void closeWriteChannel();
    auto readAllStandardOutput() -> std::string;
    auto readAllStandardError() -> std::string;
    void terminate() noexcept;
    void kill() noexcept;

    [[nodiscard]] auto state() const noexcept -> QProcess::ProcessState;
    [[nodiscard]] auto error() const noexcept -> QProcess::ProcessError;
    [[nodiscard]] auto exitCode() const noexcept -> int;
    [[nodiscard]] auto exitStatus() const noexcept -> QProcess::ExitStatus;

    void setStartedCallback(QProcess::StartedCallback callback);
    void setFinishedCallback(QProcess::FinishedCallback callback);
    void setErrorCallback(QProcess::ErrorCallback callback);
    void setReadyReadStandardOutputCallback(
        QProcess::ReadyReadStandardOutputCallback callback);
    void setReadyReadStandardErrorCallback(
        QProcess::ReadyReadStandardErrorCallback callback);

private:
    void startWindowsProcess();
    void startPosixProcess();

    // Create advanced pipe management
    void setupPipes();
    void closePipes() noexcept;

    // Asynchronous read operations
    void startAsyncReaders();
    void stopAsyncReaders() noexcept;

    // Helper methods for callbacks
    void emitStarted();
    void emitFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void emitError(QProcess::ProcessError error);
    void checkProcessStatus();

    std::atomic<bool> running_{false};
    std::atomic<bool> processStarted_{false};
    std::string program_;
    std::vector<std::string> args_;
    std::optional<std::string> workingDirectory_;
    std::vector<std::string> environment_;

    // Process state tracking
    std::atomic<QProcess::ProcessState> state_{
        QProcess::ProcessState::NotRunning};
    std::atomic<QProcess::ProcessError> lastError_{
        QProcess::ProcessError::NoError};
    std::atomic<QProcess::ExitStatus> exitStatus_{
        QProcess::ExitStatus::NormalExit};
    std::atomic<int> exitCode_{-1};

    // Callbacks
    QProcess::StartedCallback startedCallback_;
    QProcess::FinishedCallback finishedCallback_;
    QProcess::ErrorCallback errorCallback_;
    QProcess::ReadyReadStandardOutputCallback readyReadStdoutCallback_;
    QProcess::ReadyReadStandardErrorCallback readyReadStderrCallback_;
    std::mutex callbackMutex_;

    // Status monitoring
    std::atomic<bool> statusMonitorRunning_{false};
    std::future<void> statusMonitorFuture_;

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
    std::atomic<bool> writeChannelClosed_{false};

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

void QProcess::kill() noexcept {
    try {
        LOG_F(INFO, "QProcess::kill called");
        impl_->kill();
    } catch (...) {
        LOG_F(ERROR, "Exception caught in kill");
    }
}

void QProcess::closeWriteChannel() {
    LOG_F(INFO, "QProcess::closeWriteChannel called");
    try {
        impl_->closeWriteChannel();
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Failed to close write channel: {}", e.what());
        throw std::runtime_error(
            std::string("Failed to close write channel: ") + e.what());
    }
}

auto QProcess::state() const noexcept -> ProcessState {
    try {
        return impl_->state();
    } catch (...) {
        LOG_F(ERROR, "Exception caught in state()");
        return ProcessState::NotRunning;
    }
}

auto QProcess::error() const noexcept -> ProcessError {
    try {
        return impl_->error();
    } catch (...) {
        LOG_F(ERROR, "Exception caught in error()");
        return ProcessError::UnknownError;
    }
}

auto QProcess::exitCode() const noexcept -> int {
    try {
        return impl_->exitCode();
    } catch (...) {
        LOG_F(ERROR, "Exception caught in exitCode()");
        return -1;
    }
}

auto QProcess::exitStatus() const noexcept -> ExitStatus {
    try {
        return impl_->exitStatus();
    } catch (...) {
        LOG_F(ERROR, "Exception caught in exitStatus()");
        return ExitStatus::NormalExit;
    }
}

auto QProcess::workingDirectory() const -> std::optional<std::string> {
    LOG_F(INFO, "QProcess::workingDirectory called");
    return impl_->workingDirectory();
}

auto QProcess::environment() const -> std::vector<std::string> {
    LOG_F(INFO, "QProcess::environment called");
    return impl_->environment();
}

void QProcess::setStartedCallback(StartedCallback callback) {
    LOG_F(INFO, "QProcess::setStartedCallback called");
    impl_->setStartedCallback(std::move(callback));
}

void QProcess::setFinishedCallback(FinishedCallback callback) {
    LOG_F(INFO, "QProcess::setFinishedCallback called");
    impl_->setFinishedCallback(std::move(callback));
}

void QProcess::setErrorCallback(ErrorCallback callback) {
    LOG_F(INFO, "QProcess::setErrorCallback called");
    impl_->setErrorCallback(std::move(callback));
}

void QProcess::setReadyReadStandardOutputCallback(
    ReadyReadStandardOutputCallback callback) {
    LOG_F(INFO, "QProcess::setReadyReadStandardOutputCallback called");
    impl_->setReadyReadStandardOutputCallback(std::move(callback));
}

void QProcess::setReadyReadStandardErrorCallback(
    ReadyReadStandardErrorCallback callback) {
    LOG_F(INFO, "QProcess::setReadyReadStandardErrorCallback called");
    impl_->setReadyReadStandardErrorCallback(std::move(callback));
}

bool QProcess::startDetachedImpl(std::string&& program,
                                 std::vector<std::string>&& args) {
    LOG_F(INFO, "QProcess::startDetached called with program: {}", program);
    try {
        return impl_->startDetached(std::move(program), std::move(args));
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Failed to start detached process: {}", e.what());
        return false;
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
        emitError(QProcess::ProcessError::FailedToStart);
        THROW_RUNTIME_ERROR("Process already running");
    }

    // Update state
    state_ = QProcess::ProcessState::Starting;
    lastError_ = QProcess::ProcessError::NoError;
    exitCode_ = -1;
    exitStatus_ = QProcess::ExitStatus::NormalExit;

    this->program_ = std::move(program);
    this->args_ = std::move(args);

    try {
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

        // Start status monitor if not already running
        if (!statusMonitorRunning_) {
            statusMonitorRunning_ = true;
            statusMonitorFuture_ = std::async(std::launch::async, [this]() {
                while (statusMonitorRunning_) {
                    checkProcessStatus();
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                }
            });
        }

        // Emit started callback
        emitStarted();

        LOG_F(INFO, "QProcess::Impl::start completed");
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Failed to start process: {}", e.what());
        emitError(QProcess::ProcessError::FailedToStart);
        throw;
    }
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

auto QProcess::Impl::state() const noexcept -> QProcess::ProcessState {
    return state_.load();
}

auto QProcess::Impl::error() const noexcept -> QProcess::ProcessError {
    return lastError_.load();
}

auto QProcess::Impl::exitCode() const noexcept -> int {
    if (state() == QProcess::ProcessState::Running) {
        return -1;
    }

#ifdef _WIN32
    if (procInfo_.hProcess) {
        DWORD code;
        if (GetExitCodeProcess(procInfo_.hProcess, &code)) {
            return static_cast<int>(code);
        }
    }
#else
    if (childPid_ > 0) {
        int status;
        if (waitpid(childPid_, &status, WNOHANG) > 0) {
            if (WIFEXITED(status)) {
                return WEXITSTATUS(status);
            }
        }
    }
#endif
    return exitCode_.load();
}

auto QProcess::Impl::exitStatus() const noexcept -> QProcess::ExitStatus {
    return exitStatus_.load();
}

auto QProcess::Impl::workingDirectory() const -> std::optional<std::string> {
    return workingDirectory_;
}

auto QProcess::Impl::environment() const -> std::vector<std::string> {
    return environment_;
}

void QProcess::Impl::setStartedCallback(QProcess::StartedCallback callback) {
    std::lock_guard<std::mutex> lock(callbackMutex_);
    startedCallback_ = std::move(callback);
}

void QProcess::Impl::setFinishedCallback(QProcess::FinishedCallback callback) {
    std::lock_guard<std::mutex> lock(callbackMutex_);
    finishedCallback_ = std::move(callback);
}

void QProcess::Impl::setErrorCallback(QProcess::ErrorCallback callback) {
    std::lock_guard<std::mutex> lock(callbackMutex_);
    errorCallback_ = std::move(callback);
}

void QProcess::Impl::setReadyReadStandardOutputCallback(
    QProcess::ReadyReadStandardOutputCallback callback) {
    std::lock_guard<std::mutex> lock(callbackMutex_);
    readyReadStdoutCallback_ = std::move(callback);
}

void QProcess::Impl::setReadyReadStandardErrorCallback(
    QProcess::ReadyReadStandardErrorCallback callback) {
    std::lock_guard<std::mutex> lock(callbackMutex_);
    readyReadStderrCallback_ = std::move(callback);
}

void QProcess::Impl::closeWriteChannel() {
    LOG_F(INFO, "QProcess::Impl::closeWriteChannel called");

    if (writeChannelClosed_) {
        return;
    }

    writeChannelClosed_ = true;

#ifdef _WIN32
    if (childStdinWrite_ != nullptr) {
        CloseHandle(childStdinWrite_);
        childStdinWrite_ = nullptr;
    }
#else
    if (childStdin_ != -1) {
        close(childStdin_);
        childStdin_ = -1;
    }
#endif

    LOG_F(INFO, "QProcess::Impl::closeWriteChannel completed");
}

void QProcess::Impl::kill() noexcept {
    LOG_F(INFO, "QProcess::Impl::kill called");
    if (running_) {
#ifdef _WIN32
        // More forceful than terminate
        TerminateProcess(procInfo_.hProcess, 9);
        CloseHandle(procInfo_.hProcess);
        CloseHandle(procInfo_.hThread);
#else
        ::kill(childPid_, SIGKILL);  // SIGKILL is more forceful than SIGTERM
#endif
        running_ = false;
        state_ = QProcess::ProcessState::NotRunning;
        exitStatus_ = QProcess::ExitStatus::CrashExit;
        exitCode_ = 9;  // Default kill exit code
    }
    LOG_F(INFO, "QProcess::Impl::kill completed");
}

bool QProcess::Impl::startDetached(std::string&& program,
                                   std::vector<std::string>&& args) {
    LOG_F(INFO, "QProcess::Impl::startDetached called with program: {}",
          program);

#ifdef _WIN32
    STARTUPINFO siStartInfo;
    PROCESS_INFORMATION piProcInfo;

    ZeroMemory(&siStartInfo, sizeof(STARTUPINFO));
    siStartInfo.cb = sizeof(STARTUPINFO);
    ZeroMemory(&piProcInfo, sizeof(PROCESS_INFORMATION));

    // Build command line using proper quoting for spaces
    std::string cmdLine = program;
    for (const auto& arg : args) {
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
    if (!CreateProcess(nullptr, cmdLine.data(), nullptr, nullptr, FALSE,
                       DETACHED_PROCESS | CREATE_NEW_PROCESS_GROUP,
                       envBlock.empty() ? nullptr : envBlock.data(),
                       workingDirectory_ ? workingDirectory_->c_str() : nullptr,
                       &siStartInfo, &piProcInfo)) {
        LOG_F(ERROR, "Failed to start detached process: {}", GetLastError());
        return false;
    }

    // Close process and thread handles as we're detaching
    CloseHandle(piProcInfo.hProcess);
    CloseHandle(piProcInfo.hThread);

    return true;
#else
    pid_t pid = fork();

    if (pid == -1) {
        // Fork failed
        LOG_F(ERROR, "Failed to fork process: {}", strerror(errno));
        return false;
    }

    if (pid == 0) {
        // Child process

        // Create a new session to detach from parent terminal
        if (setsid() == -1) {
            LOG_F(ERROR, "Failed to create new session: {}", strerror(errno));
            exit(1);
        }

        // Close all open file descriptors
        for (int i = 0; i < sysconf(_SC_OPEN_MAX); i++) {
            close(i);
        }

        // Open /dev/null for stdin, stdout, stderr
        int nullfd = open("/dev/null", O_RDWR);
        if (nullfd == -1) {
            exit(1);
        }

        // Duplicate to stdin, stdout, stderr
        dup2(nullfd, STDIN_FILENO);
        dup2(nullfd, STDOUT_FILENO);
        dup2(nullfd, STDERR_FILENO);

        if (nullfd > 2) {
            close(nullfd);
        }

        // Change directory if set
        if (workingDirectory_ && !workingDirectory_->empty()) {
            if (chdir(workingDirectory_->c_str()) != 0) {
                exit(1);
            }
        }

        // Set environment variables
        if (!environment_.empty()) {
            for (const auto& envVar : environment_) {
                putenv(const_cast<char*>(envVar.c_str()));
            }
        }

        // Build exec argument list
        std::vector<char*> execArgs;
        execArgs.reserve(args.size() + 2);  // +1 for program, +1 for nullptr
        execArgs.push_back(const_cast<char*>(program.c_str()));

        for (const auto& arg : args) {
            execArgs.push_back(const_cast<char*>(arg.c_str()));
        }
        execArgs.push_back(nullptr);

        // Execute the program
        execvp(execArgs[0], execArgs.data());

        // If we get here, exec failed
        exit(1);
    }

    // Parent process - return success
    return true;
#endif
}

void QProcess::Impl::emitStarted() {
    std::lock_guard<std::mutex> lock(callbackMutex_);
    state_ = QProcess::ProcessState::Running;
    if (startedCallback_) {
        try {
            startedCallback_();
        } catch (const std::exception& e) {
            LOG_F(ERROR, "Exception in started callback: {}", e.what());
        }
    }
}

void QProcess::Impl::emitFinished(int exitCode,
                                  QProcess::ExitStatus exitStatus) {
    std::lock_guard<std::mutex> lock(callbackMutex_);
    state_ = QProcess::ProcessState::NotRunning;
    exitCode_ = exitCode;
    exitStatus_ = exitStatus;
    if (finishedCallback_) {
        try {
            finishedCallback_(exitCode, exitStatus);
        } catch (const std::exception& e) {
            LOG_F(ERROR, "Exception in finished callback: {}", e.what());
        }
    }
}

void QProcess::Impl::emitError(QProcess::ProcessError error) {
    std::lock_guard<std::mutex> lock(callbackMutex_);
    lastError_ = error;
    if (errorCallback_) {
        try {
            errorCallback_(error);
        } catch (const std::exception& e) {
            LOG_F(ERROR, "Exception in error callback: {}", e.what());
        }
    }
}

void QProcess::Impl::checkProcessStatus() {
    // This method should be called periodically to check the process status
    // and emit appropriate signals
    bool isRunning = false;
    int exitCode = -1;

#ifdef _WIN32
    if (procInfo_.hProcess) {
        DWORD code;
        if (GetExitCodeProcess(procInfo_.hProcess, &code)) {
            isRunning = (code == STILL_ACTIVE);
            if (!isRunning) {
                exitCode = static_cast<int>(code);
            }
        }
    }
#else
    if (childPid_ > 0) {
        int status;
        pid_t result = waitpid(childPid_, &status, WNOHANG);
        if (result == 0) {
            // Process still running
            isRunning = true;
        } else if (result > 0) {
            // Process finished
            isRunning = false;
            if (WIFEXITED(status)) {
                exitCode = WEXITSTATUS(status);
                exitStatus_ = QProcess::ExitStatus::NormalExit;
            } else if (WIFSIGNALED(status)) {
                exitCode = WTERMSIG(status);
                exitStatus_ = QProcess::ExitStatus::CrashExit;
            }
        } else {
            // Error occurred
            LOG_F(ERROR, "waitpid failed: {}", strerror(errno));
            isRunning = false;
            emitError(QProcess::ProcessError::UnknownError);
        }
    }
#endif

    if (running_ && !isRunning) {
        // Process just finished
        running_ = false;
        emitFinished(exitCode, exitStatus_);
    } else if (!running_ && isRunning) {
        // Process just started (shouldn't happen, but handle it anyway)
        running_ = true;
        processStarted_ = true;
        state_ = QProcess::ProcessState::Running;
        emitStarted();
    }
}

}  // namespace atom::utils
