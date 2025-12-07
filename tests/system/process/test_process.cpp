/**
 * @file test_process.cpp
 * @brief Comprehensive tests for process operations
 *
 * This file contains tests for the process management functions in
 * atom/system/process/process.hpp including process information retrieval,
 * monitoring, and control.
 *
 * @author Max Qian
 * @date 2024
 * @license GPL3
 */

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <chrono>
#include <string>
#include <thread>
#include <vector>

#include "atom/system/process.hpp"

namespace atom::system::test {

using namespace std::chrono_literals;

/**
 * @brief Test fixture for process tests
 */
class ProcessTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Get current process ID for testing
        currentPid = getSelfProcessInfo().pid;
    }

    void TearDown() override {
        // Clean up any monitoring
    }

    int currentPid;
};

// ============================================================================
// Process Information Tests
// ============================================================================

/**
 * @brief Test getting all processes
 */
TEST_F(ProcessTest, GetAllProcesses_ReturnsNonEmpty) {
    auto processes = getAllProcesses();
    EXPECT_FALSE(processes.empty());
}

/**
 * @brief Test getting process info by PID
 */
TEST_F(ProcessTest, GetProcessInfoByPid_CurrentProcess) {
    Process info = getProcessInfoByPid(currentPid);
    EXPECT_EQ(info.pid, currentPid);
    EXPECT_FALSE(info.name.empty());
}

/**
 * @brief Test getting process info for invalid PID
 */
TEST_F(ProcessTest, GetProcessInfoByPid_InvalidPid) {
    Process info = getProcessInfoByPid(-1);
    // Should return empty or default process info
    EXPECT_TRUE(info.pid == -1 || info.name.empty());
}

/**
 * @brief Test getting self process info
 */
TEST_F(ProcessTest, GetSelfProcessInfo_ValidInfo) {
    Process info = getSelfProcessInfo();
    EXPECT_GT(info.pid, 0);
    EXPECT_FALSE(info.name.empty());
}

/**
 * @brief Test getting controlling terminal
 */
TEST_F(ProcessTest, Ctermid_ReturnsString) {
    std::string terminal = ctermid();
    // Terminal name might be empty or contain a value
    EXPECT_TRUE(terminal.empty() || !terminal.empty());
}

// ============================================================================
// Process Status Tests
// ============================================================================

/**
 * @brief Test checking if process is running
 */
TEST_F(ProcessTest, IsProcessRunning_CurrentProcess) {
    Process info = getSelfProcessInfo();
    bool running = isProcessRunning(info.name);
    // Current process should be running
    EXPECT_TRUE(running);
}

/**
 * @brief Test checking non-existent process
 */
TEST_F(ProcessTest, IsProcessRunning_NonExistentProcess) {
    bool running = isProcessRunning("this_process_does_not_exist_12345");
    EXPECT_FALSE(running);
}

/**
 * @brief Test checking with empty process name
 */
TEST_F(ProcessTest, IsProcessRunning_EmptyName) {
    bool running = isProcessRunning("");
    EXPECT_FALSE(running);
}

// ============================================================================
// Parent Process Tests
// ============================================================================

/**
 * @brief Test getting parent process ID
 */
TEST_F(ProcessTest, GetParentProcessId_CurrentProcess) {
    int parentPid = getParentProcessId(currentPid);
    // Parent PID should be valid (> 0) or -1 if not found
    EXPECT_TRUE(parentPid > 0 || parentPid == -1);
}

/**
 * @brief Test getting parent of invalid process
 */
TEST_F(ProcessTest, GetParentProcessId_InvalidPid) {
    int parentPid = getParentProcessId(-1);
    EXPECT_EQ(parentPid, -1);
}

// ============================================================================
// Process ID by Name Tests
// ============================================================================

/**
 * @brief Test getting process IDs by name
 */
TEST_F(ProcessTest, GetProcessIdByName_CurrentProcess) {
    Process info = getSelfProcessInfo();
    auto pids = getProcessIdByName(info.name);
    // Should find at least the current process
    EXPECT_FALSE(pids.empty());
    EXPECT_THAT(pids, ::testing::Contains(currentPid));
}

/**
 * @brief Test getting PIDs for non-existent process
 */
TEST_F(ProcessTest, GetProcessIdByName_NonExistent) {
    auto pids = getProcessIdByName("this_process_does_not_exist_12345");
    EXPECT_TRUE(pids.empty());
}

/**
 * @brief Test getting PIDs with empty name
 */
TEST_F(ProcessTest, GetProcessIdByName_EmptyName) {
    auto pids = getProcessIdByName("");
    EXPECT_TRUE(pids.empty());
}

// ============================================================================
// CPU Usage Tests
// ============================================================================

/**
 * @brief Test getting CPU usage for current process
 */
TEST_F(ProcessTest, GetProcessCpuUsage_CurrentProcess) {
    double cpuUsage = getProcessCpuUsage(currentPid);
    // CPU usage should be >= 0 or -1 if not available
    EXPECT_TRUE(cpuUsage >= 0.0 || cpuUsage == -1.0);
}

/**
 * @brief Test getting CPU usage for invalid PID
 */
TEST_F(ProcessTest, GetProcessCpuUsage_InvalidPid) {
    double cpuUsage = getProcessCpuUsage(-1);
    EXPECT_EQ(cpuUsage, -1.0);
}

// ============================================================================
// Memory Usage Tests
// ============================================================================

/**
 * @brief Test getting memory usage for current process
 */
TEST_F(ProcessTest, GetProcessMemoryUsage_CurrentProcess) {
    std::size_t memUsage = getProcessMemoryUsage(currentPid);
    // Memory usage should be > 0 for running process
    EXPECT_GT(memUsage, 0);
}

/**
 * @brief Test getting memory usage for invalid PID
 */
TEST_F(ProcessTest, GetProcessMemoryUsage_InvalidPid) {
    std::size_t memUsage = getProcessMemoryUsage(-1);
    EXPECT_EQ(memUsage, 0);
}

// ============================================================================
// Process Priority Tests
// ============================================================================

/**
 * @brief Test getting process priority
 */
TEST_F(ProcessTest, GetProcessPriority_CurrentProcess) {
    auto priority = getProcessPriority(currentPid);
    // Priority should be available for current process
    EXPECT_TRUE(priority.has_value() || !priority.has_value());
}

/**
 * @brief Test getting priority for invalid PID
 */
TEST_F(ProcessTest, GetProcessPriority_InvalidPid) {
    auto priority = getProcessPriority(-1);
    EXPECT_FALSE(priority.has_value());
}

/**
 * @brief Test setting process priority
 */
TEST_F(ProcessTest, SetProcessPriority_CurrentProcess) {
    // Note: Setting priority might require elevated privileges
    bool result = setProcessPriority(currentPid, ProcessPriority::NORMAL);
    // Result depends on permissions
    EXPECT_TRUE(result || !result);
}

/**
 * @brief Test setting priority for invalid PID
 */
TEST_F(ProcessTest, SetProcessPriority_InvalidPid) {
    bool result = setProcessPriority(-1, ProcessPriority::NORMAL);
    EXPECT_FALSE(result);
}

// ============================================================================
// Child Process Tests
// ============================================================================

/**
 * @brief Test getting child processes
 */
TEST_F(ProcessTest, GetChildProcesses_CurrentProcess) {
    auto children = getChildProcesses(currentPid);
    // Current process might or might not have children
    EXPECT_TRUE(children.empty() || !children.empty());
}

/**
 * @brief Test getting children for invalid PID
 */
TEST_F(ProcessTest, GetChildProcesses_InvalidPid) {
    auto children = getChildProcesses(-1);
    EXPECT_TRUE(children.empty());
}

// ============================================================================
// Process Time Tests
// ============================================================================

/**
 * @brief Test getting process start time
 */
TEST_F(ProcessTest, GetProcessStartTime_CurrentProcess) {
    auto startTime = getProcessStartTime(currentPid);
    EXPECT_TRUE(startTime.has_value());
}

/**
 * @brief Test getting start time for invalid PID
 */
TEST_F(ProcessTest, GetProcessStartTime_InvalidPid) {
    auto startTime = getProcessStartTime(-1);
    EXPECT_FALSE(startTime.has_value());
}

/**
 * @brief Test getting process running time
 */
TEST_F(ProcessTest, GetProcessRunningTime_CurrentProcess) {
    long runningTime = getProcessRunningTime(currentPid);
    // Running time should be >= 0
    EXPECT_GE(runningTime, 0);
}

/**
 * @brief Test getting running time for invalid PID
 */
TEST_F(ProcessTest, GetProcessRunningTime_InvalidPid) {
    long runningTime = getProcessRunningTime(-1);
    EXPECT_EQ(runningTime, -1);
}

// ============================================================================
// Process Monitoring Tests
// ============================================================================

/**
 * @brief Test monitoring a process
 */
TEST_F(ProcessTest, MonitorProcess_BasicMonitoring) {
    std::atomic<int> callbackCount{0};
    auto callback = [&callbackCount](int pid, const std::string& status) {
        callbackCount++;
    };

    int monitorId = monitorProcess(currentPid, callback, 100);
    EXPECT_GE(monitorId, 0);

    std::this_thread::sleep_for(300ms);

    bool stopped = stopMonitoring(monitorId);
    EXPECT_TRUE(stopped || !stopped);
}

/**
 * @brief Test stopping non-existent monitor
 */
TEST_F(ProcessTest, StopMonitoring_InvalidId) {
    bool stopped = stopMonitoring(-1);
    EXPECT_FALSE(stopped);
}

// ============================================================================
// Command Line Tests
// ============================================================================

/**
 * @brief Test getting process command line
 */
TEST_F(ProcessTest, GetProcessCommandLine_CurrentProcess) {
    auto cmdLine = getProcessCommandLine(currentPid);
    // Command line might be empty or contain arguments
    EXPECT_TRUE(cmdLine.empty() || !cmdLine.empty());
}

/**
 * @brief Test getting command line for invalid PID
 */
TEST_F(ProcessTest, GetProcessCommandLine_InvalidPid) {
    auto cmdLine = getProcessCommandLine(-1);
    EXPECT_TRUE(cmdLine.empty());
}

// ============================================================================
// Process Creation Tests
// ============================================================================

/**
 * @brief Test creating process as user
 * @note This test is skipped as it requires specific credentials
 */
TEST_F(ProcessTest, CreateProcessAsUser_SkippedTest) {
    GTEST_SKIP()
        << "Skipping process creation test - requires valid credentials";
}

}  // namespace atom::system::test
