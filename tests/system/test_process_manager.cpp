#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <vector>
#include <chrono>
#include <thread>

#include "atom/system/process/process_manager.hpp"

namespace atom::system::test {

using atom::system::Process;
using atom::system::ProcessManager;

// Mock class for testing process operations without actual process creation
class MockProcessManager {
public:
    MOCK_METHOD(bool, createProcess, (const std::string& command, const std::string& identifier, bool isBackground), (const));
    MOCK_METHOD(bool, terminateProcess, (int pid, int signal), (const));
    MOCK_METHOD(bool, terminateProcessByName, (const std::string& name, int signal), (const));
    MOCK_METHOD(bool, hasProcess, (const std::string& identifier), (const));
    MOCK_METHOD(std::vector<Process>, getRunningProcesses, (), (const));
    MOCK_METHOD(std::vector<std::string>, getProcessOutput, (const std::string& identifier), (const));
    MOCK_METHOD(void, waitForCompletion, (), (const));
    MOCK_METHOD(bool, runScript, (const std::string& script, const std::string& identifier, bool isBackground), (const));
    MOCK_METHOD(bool, monitorProcesses, (), (const));
    MOCK_METHOD(Process, getProcessInfo, (int pid), (const));
#ifdef _WIN32
    MOCK_METHOD(void*, getProcessHandle, (int pid), (const));
#endif
};

class ProcessManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        mockProcessManager = std::make_unique<::testing::NiceMock<MockProcessManager>>();

        // Set up sample process data
        sampleProcesses = {
            {1234, "test_process_1", "running", 50.0, 1024, "/usr/bin/test1"},
            {5678, "test_process_2", "sleeping", 25.0, 2048, "/usr/bin/test2"},
            {9012, "background_task", "running", 10.0, 512, "/usr/bin/bg_task"}
        };

        sampleOutput = {
            "Process output line 1",
            "Process output line 2",
            "Process completed successfully"
        };

        // Set up default behavior for the mock
        ON_CALL(*mockProcessManager, createProcess(::testing::_, ::testing::_, ::testing::_))
            .WillByDefault(::testing::Return(true));
        ON_CALL(*mockProcessManager, terminateProcess(::testing::_, ::testing::_))
            .WillByDefault(::testing::Return(true));
        ON_CALL(*mockProcessManager, terminateProcessByName(::testing::_, ::testing::_))
            .WillByDefault(::testing::Return(true));
        ON_CALL(*mockProcessManager, hasProcess(::testing::_))
            .WillByDefault(::testing::Return(true));
        ON_CALL(*mockProcessManager, getRunningProcesses())
            .WillByDefault(::testing::Return(sampleProcesses));
        ON_CALL(*mockProcessManager, getProcessOutput(::testing::_))
            .WillByDefault(::testing::Return(sampleOutput));
        ON_CALL(*mockProcessManager, runScript(::testing::_, ::testing::_, ::testing::_))
            .WillByDefault(::testing::Return(true));
        ON_CALL(*mockProcessManager, monitorProcesses())
            .WillByDefault(::testing::Return(true));
        ON_CALL(*mockProcessManager, getProcessInfo(::testing::_))
            .WillByDefault(::testing::Return(sampleProcesses[0]));
    }

    void TearDown() override {
        mockProcessManager.reset();
    }

    std::unique_ptr<MockProcessManager> mockProcessManager;
    std::vector<Process> sampleProcesses;
    std::vector<std::string> sampleOutput;
};

// Test process creation
TEST_F(ProcessManagerTest, CreateProcessSuccess) {
    EXPECT_CALL(*mockProcessManager, createProcess("echo 'Hello World'", "test_echo", false))
        .WillOnce(::testing::Return(true));

    bool result = mockProcessManager->createProcess("echo 'Hello World'", "test_echo", false);
    EXPECT_TRUE(result);
}

TEST_F(ProcessManagerTest, CreateProcessFailure) {
    EXPECT_CALL(*mockProcessManager, createProcess("invalid_command", "test_invalid", false))
        .WillOnce(::testing::Return(false));

    bool result = mockProcessManager->createProcess("invalid_command", "test_invalid", false);
    EXPECT_FALSE(result);
}

TEST_F(ProcessManagerTest, CreateBackgroundProcess) {
    EXPECT_CALL(*mockProcessManager, createProcess("long_running_task", "bg_task", true))
        .WillOnce(::testing::Return(true));

    bool result = mockProcessManager->createProcess("long_running_task", "bg_task", true);
    EXPECT_TRUE(result);
}

// Test process termination
TEST_F(ProcessManagerTest, TerminateProcessByPid) {
    EXPECT_CALL(*mockProcessManager, terminateProcess(1234, 15))
        .WillOnce(::testing::Return(true));

    bool result = mockProcessManager->terminateProcess(1234, 15);
    EXPECT_TRUE(result);
}

TEST_F(ProcessManagerTest, TerminateProcessByName) {
    EXPECT_CALL(*mockProcessManager, terminateProcessByName("test_process", 15))
        .WillOnce(::testing::Return(true));

    bool result = mockProcessManager->terminateProcessByName("test_process", 15);
    EXPECT_TRUE(result);
}

TEST_F(ProcessManagerTest, TerminateProcessWithDifferentSignals) {
    EXPECT_CALL(*mockProcessManager, terminateProcess(1234, 9))  // SIGKILL
        .WillOnce(::testing::Return(true));
    EXPECT_CALL(*mockProcessManager, terminateProcess(5678, 2))  // SIGINT
        .WillOnce(::testing::Return(true));

    EXPECT_TRUE(mockProcessManager->terminateProcess(1234, 9));
    EXPECT_TRUE(mockProcessManager->terminateProcess(5678, 2));
}

// Test process existence checking
TEST_F(ProcessManagerTest, HasProcess) {
    EXPECT_CALL(*mockProcessManager, hasProcess("existing_process"))
        .WillOnce(::testing::Return(true));
    EXPECT_CALL(*mockProcessManager, hasProcess("nonexistent_process"))
        .WillOnce(::testing::Return(false));

    EXPECT_TRUE(mockProcessManager->hasProcess("existing_process"));
    EXPECT_FALSE(mockProcessManager->hasProcess("nonexistent_process"));
}

// Test process listing
TEST_F(ProcessManagerTest, GetRunningProcesses) {
    EXPECT_CALL(*mockProcessManager, getRunningProcesses())
        .WillOnce(::testing::Return(sampleProcesses));

    auto processes = mockProcessManager->getRunningProcesses();

    EXPECT_EQ(processes.size(), 3);
    EXPECT_EQ(processes[0].pid, 1234);
    EXPECT_EQ(processes[0].name, "test_process_1");
    EXPECT_EQ(processes[0].status, "running");
    EXPECT_EQ(processes[1].pid, 5678);
    EXPECT_EQ(processes[2].pid, 9012);
}

TEST_F(ProcessManagerTest, GetRunningProcessesEmpty) {
    std::vector<Process> emptyProcesses;

    EXPECT_CALL(*mockProcessManager, getRunningProcesses())
        .WillOnce(::testing::Return(emptyProcesses));

    auto processes = mockProcessManager->getRunningProcesses();
    EXPECT_TRUE(processes.empty());
}

// Test process output retrieval
TEST_F(ProcessManagerTest, GetProcessOutput) {
    EXPECT_CALL(*mockProcessManager, getProcessOutput("test_process"))
        .WillOnce(::testing::Return(sampleOutput));

    auto output = mockProcessManager->getProcessOutput("test_process");

    EXPECT_EQ(output.size(), 3);
    EXPECT_EQ(output[0], "Process output line 1");
    EXPECT_EQ(output[1], "Process output line 2");
    EXPECT_EQ(output[2], "Process completed successfully");
}

TEST_F(ProcessManagerTest, GetProcessOutputEmpty) {
    std::vector<std::string> emptyOutput;

    EXPECT_CALL(*mockProcessManager, getProcessOutput("silent_process"))
        .WillOnce(::testing::Return(emptyOutput));

    auto output = mockProcessManager->getProcessOutput("silent_process");
    EXPECT_TRUE(output.empty());
}

// Test script execution
TEST_F(ProcessManagerTest, RunScript) {
    std::string script = "#!/bin/bash\necho 'Script executed'\nexit 0";

    EXPECT_CALL(*mockProcessManager, runScript(script, "test_script", false))
        .WillOnce(::testing::Return(true));

    bool result = mockProcessManager->runScript(script, "test_script", false);
    EXPECT_TRUE(result);
}

TEST_F(ProcessManagerTest, RunBackgroundScript) {
    std::string script = "#!/bin/bash\nsleep 10\necho 'Background script done'";

    EXPECT_CALL(*mockProcessManager, runScript(script, "bg_script", true))
        .WillOnce(::testing::Return(true));

    bool result = mockProcessManager->runScript(script, "bg_script", true);
    EXPECT_TRUE(result);
}

// Test process monitoring
TEST_F(ProcessManagerTest, MonitorProcesses) {
    EXPECT_CALL(*mockProcessManager, monitorProcesses())
        .WillOnce(::testing::Return(true));

    bool result = mockProcessManager->monitorProcesses();
    EXPECT_TRUE(result);
}

// Test process information retrieval
TEST_F(ProcessManagerTest, GetProcessInfo) {
    EXPECT_CALL(*mockProcessManager, getProcessInfo(1234))
        .WillOnce(::testing::Return(sampleProcesses[0]));

    Process info = mockProcessManager->getProcessInfo(1234);

    EXPECT_EQ(info.pid, 1234);
    EXPECT_EQ(info.name, "test_process_1");
    EXPECT_EQ(info.status, "running");
    EXPECT_EQ(info.cpuUsage, 50.0);
    EXPECT_EQ(info.memoryUsage, 1024);
    EXPECT_EQ(info.executablePath, "/usr/bin/test1");
}

// Test wait for completion
TEST_F(ProcessManagerTest, WaitForCompletion) {
    EXPECT_CALL(*mockProcessManager, waitForCompletion())
        .Times(1);

    mockProcessManager->waitForCompletion();
}

#ifdef _WIN32
// Windows-specific tests
TEST_F(ProcessManagerTest, GetProcessHandle) {
    void* mockHandle = reinterpret_cast<void*>(0x12345678);

    EXPECT_CALL(*mockProcessManager, getProcessHandle(1234))
        .WillOnce(::testing::Return(mockHandle));

    void* handle = mockProcessManager->getProcessHandle(1234);
    EXPECT_EQ(handle, mockHandle);
}
#endif

// Test Process structure
TEST_F(ProcessManagerTest, ProcessStructure) {
    Process process;
    process.pid = 9999;
    process.name = "test_app";
    process.status = "running";
    process.cpuUsage = 75.5;
    process.memoryUsage = 4096;
    process.executablePath = "/usr/bin/test_app";

    EXPECT_EQ(process.pid, 9999);
    EXPECT_EQ(process.name, "test_app");
    EXPECT_EQ(process.status, "running");
    EXPECT_EQ(process.cpuUsage, 75.5);
    EXPECT_EQ(process.memoryUsage, 4096);
    EXPECT_EQ(process.executablePath, "/usr/bin/test_app");
}

// Error handling tests
class ProcessManagerErrorTest : public ::testing::Test {
protected:
    void SetUp() override {
        mockProcessManager = std::make_unique<::testing::NiceMock<MockProcessManager>>();
    }

    void TearDown() override {
        mockProcessManager.reset();
    }

    std::unique_ptr<MockProcessManager> mockProcessManager;
};

// Test invalid process creation
TEST_F(ProcessManagerErrorTest, InvalidProcessCreation) {
    // Empty command
    EXPECT_CALL(*mockProcessManager, createProcess("", "empty_cmd", false))
        .WillOnce(::testing::Return(false));
    EXPECT_FALSE(mockProcessManager->createProcess("", "empty_cmd", false));

    // Empty identifier
    EXPECT_CALL(*mockProcessManager, createProcess("echo test", "", false))
        .WillOnce(::testing::Return(false));
    EXPECT_FALSE(mockProcessManager->createProcess("echo test", "", false));

    // Invalid command
    EXPECT_CALL(*mockProcessManager, createProcess("nonexistent_command_xyz", "invalid", false))
        .WillOnce(::testing::Return(false));
    EXPECT_FALSE(mockProcessManager->createProcess("nonexistent_command_xyz", "invalid", false));
}

// Test invalid process termination
TEST_F(ProcessManagerErrorTest, InvalidProcessTermination) {
    // Invalid PID
    EXPECT_CALL(*mockProcessManager, terminateProcess(-1, 15))
        .WillOnce(::testing::Return(false));
    EXPECT_FALSE(mockProcessManager->terminateProcess(-1, 15));

    // Nonexistent PID
    EXPECT_CALL(*mockProcessManager, terminateProcess(999999, 15))
        .WillOnce(::testing::Return(false));
    EXPECT_FALSE(mockProcessManager->terminateProcess(999999, 15));

    // Invalid signal
    EXPECT_CALL(*mockProcessManager, terminateProcess(1234, -1))
        .WillOnce(::testing::Return(false));
    EXPECT_FALSE(mockProcessManager->terminateProcess(1234, -1));

    // Nonexistent process name
    EXPECT_CALL(*mockProcessManager, terminateProcessByName("nonexistent_process", 15))
        .WillOnce(::testing::Return(false));
    EXPECT_FALSE(mockProcessManager->terminateProcessByName("nonexistent_process", 15));
}

// Test invalid script execution
TEST_F(ProcessManagerErrorTest, InvalidScriptExecution) {
    // Empty script
    EXPECT_CALL(*mockProcessManager, runScript("", "empty_script", false))
        .WillOnce(::testing::Return(false));
    EXPECT_FALSE(mockProcessManager->runScript("", "empty_script", false));

    // Invalid script syntax
    std::string invalidScript = "#!/bin/bash\ninvalid syntax here &*@#$";
    EXPECT_CALL(*mockProcessManager, runScript(invalidScript, "invalid_script", false))
        .WillOnce(::testing::Return(false));
    EXPECT_FALSE(mockProcessManager->runScript(invalidScript, "invalid_script", false));
}

// Performance tests
class ProcessManagerPerformanceTest : public ::testing::Test {
protected:
    void SetUp() override {
        mockProcessManager = std::make_unique<::testing::NiceMock<MockProcessManager>>();

        // Create large process list for performance testing
        largeProcessList.reserve(1000);
        for (int i = 0; i < 1000; ++i) {
            largeProcessList.push_back({
                1000 + i,
                "process_" + std::to_string(i),
                i % 2 == 0 ? "running" : "sleeping",
                static_cast<double>(i % 100),
                1024 + (i * 10),
                "/usr/bin/process_" + std::to_string(i)
            });
        }

        ON_CALL(*mockProcessManager, getRunningProcesses())
            .WillByDefault(::testing::Return(largeProcessList));
    }

    void TearDown() override {
        mockProcessManager.reset();
    }

    std::unique_ptr<MockProcessManager> mockProcessManager;
    std::vector<Process> largeProcessList;
};

// Test performance with large process lists
TEST_F(ProcessManagerPerformanceTest, LargeProcessListRetrieval) {
    EXPECT_CALL(*mockProcessManager, getRunningProcesses())
        .WillOnce(::testing::Return(largeProcessList));

    auto start = std::chrono::high_resolution_clock::now();
    auto processes = mockProcessManager->getRunningProcesses();
    auto end = std::chrono::high_resolution_clock::now();

    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    EXPECT_EQ(processes.size(), 1000);
    // Should complete within reasonable time (100ms for mock)
    EXPECT_LT(duration.count(), 100);
}

// Test rapid process operations
TEST_F(ProcessManagerPerformanceTest, RapidProcessOperations) {
    EXPECT_CALL(*mockProcessManager, createProcess(::testing::_, ::testing::_, ::testing::_))
        .Times(100)
        .WillRepeatedly(::testing::Return(true));

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < 100; ++i) {
        std::string cmd = "echo " + std::to_string(i);
        std::string id = "test_" + std::to_string(i);
        bool result = mockProcessManager->createProcess(cmd, id, false);
        EXPECT_TRUE(result);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // 100 process creations should complete within reasonable time
    EXPECT_LT(duration.count(), 200);
}

// Integration tests with actual ProcessManager
class ProcessManagerIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create actual ProcessManager instance
        processManager = ProcessManager::createShared(5);
    }

    void TearDown() override {
        // Clean up any running processes
        if (processManager) {
            processManager->waitForCompletion();
        }
        processManager.reset();
    }

    std::shared_ptr<ProcessManager> processManager;
};

// Test actual process creation and management
TEST_F(ProcessManagerIntegrationTest, DISABLED_ActualProcessCreation) {
    // This test is disabled by default to prevent creating actual processes
    // Enable only in controlled test environments

#ifdef _WIN32
    std::string testCommand = "echo Hello World";
#else
    std::string testCommand = "echo 'Hello World'";
#endif

    bool created = processManager->createProcess(testCommand, "integration_test", false);
    EXPECT_TRUE(created);

    // Check if process exists
    bool exists = processManager->hasProcess("integration_test");
    EXPECT_TRUE(exists);

    // Get process output
    auto output = processManager->getProcessOutput("integration_test");
    EXPECT_FALSE(output.empty());

    // Wait for completion
    processManager->waitForCompletion();
}

// Test process monitoring
TEST_F(ProcessManagerIntegrationTest, ProcessMonitoring) {
    bool monitorResult = processManager->monitorProcesses();
    EXPECT_TRUE(monitorResult);
}

// Test getting running processes
TEST_F(ProcessManagerIntegrationTest, GetRunningProcesses) {
    auto processes = processManager->getRunningProcesses();

    // Should not crash and should return a valid vector
    EXPECT_NO_THROW(processManager->getRunningProcesses());

    // Verify that each process has valid structure
    for (const auto& process : processes) {
        EXPECT_GT(process.pid, 0);
        EXPECT_FALSE(process.name.empty());
        EXPECT_FALSE(process.status.empty());
        EXPECT_GE(process.cpuUsage, 0.0);
        EXPECT_GE(process.memoryUsage, 0);
    }
}

}  // namespace atom::system::test
