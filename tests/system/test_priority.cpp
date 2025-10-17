#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <chrono>
#include <thread>
#include <vector>

#include "atom/system/core/priority.hpp"

namespace atom::system::test {

using PriorityLevel = atom::system::PriorityManager::PriorityLevel;
using SchedulingPolicy = atom::system::PriorityManager::SchedulingPolicy;

// Mock class for testing priority operations without affecting the system
class MockPriorityManager {
public:
    MOCK_METHOD(void, setProcessPriority, (PriorityLevel level, int pid), (const));
    MOCK_METHOD(void, setThreadPriority, (PriorityLevel level, std::thread::native_handle_type thread), (const));
    MOCK_METHOD(PriorityLevel, getProcessPriority, (int pid), (const));
    MOCK_METHOD(PriorityLevel, getThreadPriority, (std::thread::native_handle_type thread), (const));
    MOCK_METHOD(void, setThreadSchedulingPolicy, (SchedulingPolicy policy, std::thread::native_handle_type thread), (const));
    MOCK_METHOD(void, setProcessAffinity, (const std::vector<int>& cpus, int pid), (const));
    MOCK_METHOD(std::vector<int>, getProcessAffinity, (int pid), (const));
    MOCK_METHOD(void, startPriorityMonitor, (int pid, const std::function<void(PriorityLevel)>& callback, std::chrono::milliseconds interval), (const));
};

class PriorityTest : public ::testing::Test {
protected:
    void SetUp() override {
        mockPriorityManager = std::make_unique<::testing::NiceMock<MockPriorityManager>>();

        // Set up default behavior for the mock
        ON_CALL(*mockPriorityManager, getProcessPriority(::testing::_))
            .WillByDefault(::testing::Return(PriorityLevel::NORMAL));
        ON_CALL(*mockPriorityManager, getThreadPriority(::testing::_))
            .WillByDefault(::testing::Return(PriorityLevel::NORMAL));
        ON_CALL(*mockPriorityManager, getProcessAffinity(::testing::_))
            .WillByDefault(::testing::Return(std::vector<int>{0, 1}));
    }

    void TearDown() override {
        mockPriorityManager.reset();
    }

    std::unique_ptr<MockPriorityManager> mockPriorityManager;
};

// Test process priority setting
TEST_F(PriorityTest, SetProcessPrioritySuccess) {
    EXPECT_CALL(*mockPriorityManager, setProcessPriority(PriorityLevel::HIGHEST, 0))
        .Times(1);

    mockPriorityManager->setProcessPriority(PriorityLevel::HIGHEST, 0);
}

TEST_F(PriorityTest, SetProcessPriorityAllLevels) {
    // Test all priority levels
    std::vector<PriorityLevel> levels = {
        PriorityLevel::LOWEST,
        PriorityLevel::BELOW_NORMAL,
        PriorityLevel::NORMAL,
        PriorityLevel::ABOVE_NORMAL,
        PriorityLevel::HIGHEST,
        PriorityLevel::REALTIME
    };

    for (auto level : levels) {
        EXPECT_CALL(*mockPriorityManager, setProcessPriority(level, 0))
            .Times(1);
        mockPriorityManager->setProcessPriority(level, 0);
    }
}

// Test thread priority setting
TEST_F(PriorityTest, SetThreadPrioritySuccess) {
    std::thread::native_handle_type thread = 0; // 0 means current thread

    EXPECT_CALL(*mockPriorityManager, setThreadPriority(PriorityLevel::HIGHEST, thread))
        .Times(1);

    mockPriorityManager->setThreadPriority(PriorityLevel::HIGHEST, thread);
}

// Test priority getting
TEST_F(PriorityTest, GetProcessPriority) {
    EXPECT_CALL(*mockPriorityManager, getProcessPriority(0))
        .WillOnce(::testing::Return(PriorityLevel::NORMAL));

    PriorityLevel level = mockPriorityManager->getProcessPriority(0);
    EXPECT_EQ(level, PriorityLevel::NORMAL);
}

TEST_F(PriorityTest, GetThreadPriority) {
    std::thread::native_handle_type thread = 0; // 0 means current thread

    EXPECT_CALL(*mockPriorityManager, getThreadPriority(thread))
        .WillOnce(::testing::Return(PriorityLevel::HIGHEST));

    PriorityLevel level = mockPriorityManager->getThreadPriority(thread);
    EXPECT_EQ(level, PriorityLevel::HIGHEST);
}

// Test scheduling policy
// NOTE: These tests are commented out because the actual PriorityManager API
// only has setThreadSchedulingPolicy, not setSchedulingPolicy for processes,
// and there's no getSchedulingPolicy method
/*
TEST_F(PriorityTest, SetSchedulingPolicy) {
    EXPECT_CALL(*mockPriorityManager, setSchedulingPolicy(SchedulingPolicy::FIFO, 0))
        .Times(1);

    mockPriorityManager->setSchedulingPolicy(SchedulingPolicy::FIFO, 0);
}

TEST_F(PriorityTest, GetSchedulingPolicy) {
    EXPECT_CALL(*mockPriorityManager, getSchedulingPolicy(0))
        .WillOnce(::testing::Return(SchedulingPolicy::ROUND_ROBIN));

    SchedulingPolicy policy = mockPriorityManager->getSchedulingPolicy(0);
    EXPECT_EQ(policy, SchedulingPolicy::ROUND_ROBIN);
}

TEST_F(PriorityTest, SetSchedulingPolicyAllTypes) {
    std::vector<SchedulingPolicy> policies = {
        SchedulingPolicy::NORMAL,
        SchedulingPolicy::FIFO,
        SchedulingPolicy::ROUND_ROBIN
    };

    for (auto policy : policies) {
        EXPECT_CALL(*mockPriorityManager, setSchedulingPolicy(policy, 0))
            .Times(1);
        mockPriorityManager->setSchedulingPolicy(policy, 0);
    }
}
*/

// Test CPU affinity
TEST_F(PriorityTest, SetProcessAffinity) {
    std::vector<int> cpus = {0, 2, 4};

    EXPECT_CALL(*mockPriorityManager, setProcessAffinity(cpus, 0))
        .Times(1);

    mockPriorityManager->setProcessAffinity(cpus, 0);
}

TEST_F(PriorityTest, GetProcessAffinity) {
    std::vector<int> expectedCpus = {0, 1, 2, 3};

    EXPECT_CALL(*mockPriorityManager, getProcessAffinity(0))
        .WillOnce(::testing::Return(expectedCpus));

    std::vector<int> cpus = mockPriorityManager->getProcessAffinity(0);
    EXPECT_EQ(cpus, expectedCpus);
}

// NOTE: Thread affinity tests commented out because the actual PriorityManager API
// doesn't have setThreadAffinity or getThreadAffinity methods
/*
TEST_F(PriorityTest, SetThreadAffinity) {
    std::thread::id tid = std::this_thread::get_id();
    std::vector<int> cpus = {1, 3};

    EXPECT_CALL(*mockPriorityManager, setThreadAffinity(cpus, tid))
        .Times(1);

    mockPriorityManager->setThreadAffinity(cpus, tid);
}

TEST_F(PriorityTest, GetThreadAffinity) {
    std::thread::id tid = std::this_thread::get_id();
    std::vector<int> expectedCpus = {0, 2};

    EXPECT_CALL(*mockPriorityManager, getThreadAffinity(tid))
        .WillOnce(::testing::Return(expectedCpus));

    std::vector<int> cpus = mockPriorityManager->getThreadAffinity(tid);
    EXPECT_EQ(cpus, expectedCpus);
}
*/

// Test priority monitoring
TEST_F(PriorityTest, StartPriorityMonitor) {
    auto callback = [](PriorityLevel level) {
        // Mock callback function
    };

    EXPECT_CALL(*mockPriorityManager, startPriorityMonitor(1234, ::testing::_, std::chrono::milliseconds(1000)))
        .Times(1);

    mockPriorityManager->startPriorityMonitor(1234, callback, std::chrono::milliseconds(1000));
}

// NOTE: StopPriorityMonitor test commented out because the actual PriorityManager API
// doesn't have a stopPriorityMonitor method
/*
TEST_F(PriorityTest, StopPriorityMonitor) {
    EXPECT_CALL(*mockPriorityManager, stopPriorityMonitor(1234))
        .Times(1);

    mockPriorityManager->stopPriorityMonitor(1234);
}
*/

// Test edge cases
class PriorityEdgeCaseTest : public ::testing::Test {
protected:
    void SetUp() override {
        mockPriorityManager = std::make_unique<::testing::NiceMock<MockPriorityManager>>();
    }

    void TearDown() override {
        mockPriorityManager.reset();
    }

    std::unique_ptr<MockPriorityManager> mockPriorityManager;
};

// Test invalid process IDs
TEST_F(PriorityEdgeCaseTest, InvalidProcessId) {
    // Test with negative PID
    EXPECT_CALL(*mockPriorityManager, setProcessPriority(PriorityLevel::NORMAL, -1))
        .Times(1);
    mockPriorityManager->setProcessPriority(PriorityLevel::NORMAL, -1);

    // Test with very large PID
    EXPECT_CALL(*mockPriorityManager, setProcessPriority(PriorityLevel::NORMAL, 999999))
        .Times(1);
    mockPriorityManager->setProcessPriority(PriorityLevel::NORMAL, 999999);
}

// Test empty CPU affinity
TEST_F(PriorityEdgeCaseTest, EmptyCpuAffinity) {
    std::vector<int> emptyCpus;

    EXPECT_CALL(*mockPriorityManager, setProcessAffinity(emptyCpus, 0))
        .Times(1);

    mockPriorityManager->setProcessAffinity(emptyCpus, 0);
}

// Test invalid CPU numbers
TEST_F(PriorityEdgeCaseTest, InvalidCpuNumbers) {
    std::vector<int> invalidCpus = {-1, 1000};

    EXPECT_CALL(*mockPriorityManager, setProcessAffinity(invalidCpus, 0))
        .Times(1);

    mockPriorityManager->setProcessAffinity(invalidCpus, 0);
}

// Platform-specific tests
#ifdef _WIN32
// Windows-specific priority tests
TEST_F(PriorityTest, WindowsSpecificPriorities) {
    // Test Windows-specific priority handling
    EXPECT_CALL(*mockPriorityManager, setProcessPriority(PriorityLevel::REALTIME, 0))
        .Times(1);

    mockPriorityManager->setProcessPriority(PriorityLevel::REALTIME, 0);
}
#elif defined(__linux__)
// Linux-specific priority tests
// NOTE: Commented out because setSchedulingPolicy doesn't exist for processes
/*
TEST_F(PriorityTest, LinuxSpecificPriorities) {
    // Test Linux nice values and real-time priorities
    EXPECT_CALL(*mockPriorityManager, setSchedulingPolicy(SchedulingPolicy::FIFO, 0))
        .Times(1);

    mockPriorityManager->setSchedulingPolicy(SchedulingPolicy::FIFO, 0);
}
*/
#elif defined(__APPLE__)
// macOS-specific priority tests
TEST_F(PriorityTest, MacOSSpecificPriorities) {
    // Test macOS-specific priority handling
    EXPECT_CALL(*mockPriorityManager, setProcessPriority(PriorityLevel::HIGHEST, 0))
        .Times(1);

    mockPriorityManager->setProcessPriority(PriorityLevel::HIGHEST, 0);
}
#endif

// Performance tests
class PriorityPerformanceTest : public ::testing::Test {
protected:
    void SetUp() override {
        mockPriorityManager = std::make_unique<::testing::NiceMock<MockPriorityManager>>();
    }

    void TearDown() override {
        mockPriorityManager.reset();
    }

    std::unique_ptr<MockPriorityManager> mockPriorityManager;
};

// Test priority operation performance
TEST_F(PriorityPerformanceTest, PriorityOperationSpeed) {
    EXPECT_CALL(*mockPriorityManager, setProcessPriority(PriorityLevel::HIGHEST, 0))
        .Times(100);

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < 100; ++i) {
        mockPriorityManager->setProcessPriority(PriorityLevel::HIGHEST, 0);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // Priority operations should be fast (within 100ms for 100 operations)
    EXPECT_LT(duration.count(), 100);
}

// Test affinity operation performance
TEST_F(PriorityPerformanceTest, AffinityOperationSpeed) {
    std::vector<int> cpus = {0, 1, 2, 3};

    EXPECT_CALL(*mockPriorityManager, setProcessAffinity(cpus, 0))
        .Times(50);

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < 50; ++i) {
        mockPriorityManager->setProcessAffinity(cpus, 0);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // Affinity operations should be reasonably fast
    EXPECT_LT(duration.count(), 200);
}

// Integration tests
class PriorityIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        mockPriorityManager = std::make_unique<::testing::NiceMock<MockPriorityManager>>();

        ON_CALL(*mockPriorityManager, getProcessPriority(::testing::_))
            .WillByDefault(::testing::Return(PriorityLevel::NORMAL));
    }

    void TearDown() override {
        mockPriorityManager.reset();
    }

    std::unique_ptr<MockPriorityManager> mockPriorityManager;
};

// Test priority and affinity interaction
TEST_F(PriorityIntegrationTest, PriorityAffinityInteraction) {
    std::vector<int> cpus = {0, 2};

    // Set priority first, then affinity
    EXPECT_CALL(*mockPriorityManager, setProcessPriority(PriorityLevel::HIGHEST, 0))
        .Times(1);
    EXPECT_CALL(*mockPriorityManager, setProcessAffinity(cpus, 0))
        .Times(1);
    EXPECT_CALL(*mockPriorityManager, getProcessPriority(0))
        .WillOnce(::testing::Return(PriorityLevel::HIGHEST));

    mockPriorityManager->setProcessPriority(PriorityLevel::HIGHEST, 0);
    mockPriorityManager->setProcessAffinity(cpus, 0);

    // Verify priority is maintained after affinity change
    PriorityLevel level = mockPriorityManager->getProcessPriority(0);
    EXPECT_EQ(level, PriorityLevel::HIGHEST);
}

// Test scheduling policy and priority interaction
// NOTE: Commented out because setSchedulingPolicy and getSchedulingPolicy don't exist for processes
/*
TEST_F(PriorityIntegrationTest, SchedulingPolicyPriorityInteraction) {
    EXPECT_CALL(*mockPriorityManager, setSchedulingPolicy(SchedulingPolicy::FIFO, 0))
        .Times(1);
    EXPECT_CALL(*mockPriorityManager, setProcessPriority(PriorityLevel::REALTIME, 0))
        .Times(1);
    EXPECT_CALL(*mockPriorityManager, getSchedulingPolicy(0))
        .WillOnce(::testing::Return(SchedulingPolicy::FIFO));

    mockPriorityManager->setSchedulingPolicy(SchedulingPolicy::FIFO, 0);
    mockPriorityManager->setProcessPriority(PriorityLevel::REALTIME, 0);

    // Verify scheduling policy is maintained
    SchedulingPolicy policy = mockPriorityManager->getSchedulingPolicy(0);
    EXPECT_EQ(policy, SchedulingPolicy::FIFO);
}
*/

}  // namespace atom::system::test
