#include "atom/async/async.hpp"
#include <gtest/gtest.h>
#include <spdlog/spdlog.h>

class AsyncWorkerTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}

    bool validateResult(const std::function<bool(int)>& validator, int result) {
        return validator(result);
    }
};

TEST_F(AsyncWorkerTest, StartAsync_ValidFunction_ReturnsExpectedResult) {
    atom::async::AsyncWorker<int> asyncWorker;
    std::function<int()> task = []() { return 42; };
    asyncWorker.startAsync(task);
    EXPECT_TRUE(asyncWorker.isActive());
}

TEST_F(AsyncWorkerTest, GetResult_ValidTask_ReturnsExpectedResult) {
    atom::async::AsyncWorker<int> asyncWorker;
    std::function<int()> task = []() { return 42; };
    asyncWorker.startAsync(task);
    int result = asyncWorker.getResult();
    EXPECT_EQ(result, 42);
}

TEST_F(AsyncWorkerTest, Cancel_ActiveTask_WaitsForCompletion) {
    atom::async::AsyncWorker<int> asyncWorker;
    std::function<int()> task = []() {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        return 42;
    };
    asyncWorker.startAsync(task);
    asyncWorker.cancel();
    EXPECT_FALSE(asyncWorker.isActive());
}

TEST_F(AsyncWorkerTest, Validate_ValidResult_ReturnsTrue) {
    atom::async::AsyncWorker<int> asyncWorker;
    std::function<int()> task = []() { return 42; };
    asyncWorker.startAsync(task);
    std::function<bool(int)> validator = [](int result) {
        return result == 42;
    };
    bool isValid = asyncWorker.validate(validator);
    EXPECT_TRUE(isValid);
}

TEST_F(AsyncWorkerTest, Validate_InvalidResult_ReturnsFalse) {
    atom::async::AsyncWorker<int> asyncWorker;
    std::function<int()> task = []() { return 42; };
    asyncWorker.startAsync(task);
    std::function<bool(int)> validator = [](int result) {
        return result == 43;
    };
    bool isValid = asyncWorker.validate(validator);
    EXPECT_FALSE(isValid);
}

TEST_F(AsyncWorkerTest, SetCallback_ValidCallback_CallsCallbackWithResult) {
    atom::async::AsyncWorker<int> asyncWorker;
    std::function<int()> task = []() { return 42; };
    std::function<void(int)> callback = [](int result) {
        EXPECT_EQ(result, 42);
    };
    asyncWorker.setCallback(callback);
    asyncWorker.startAsync(task);
    asyncWorker.waitForCompletion();
}

TEST_F(AsyncWorkerTest, SetTimeout_ValidTimeout_WaitsForTimeout) {
    atom::async::AsyncWorker<int> asyncWorker;
    std::function<int()> task = []() {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        return 42;
    };
    asyncWorker.setTimeout(std::chrono::seconds(1));
    asyncWorker.startAsync(task);
    asyncWorker.waitForCompletion();
    EXPECT_FALSE(asyncWorker.isActive());
}

class AsyncWorkerManagerTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}

    std::shared_ptr<atom::async::AsyncWorker<int>> createAndStartTask(
        const std::function<int()>& task) {
        auto worker = asyncWorkerManager.createWorker(task);
        worker->startAsync(task);
        return worker;
    }

    atom::async::AsyncWorkerManager<int> asyncWorkerManager;
};

TEST_F(AsyncWorkerManagerTest, CreateWorker_ValidFunction_ReturnsValidWorker) {
    std::function<int()> task = []() { return 42; };
    auto worker = asyncWorkerManager.createWorker(task);
    EXPECT_TRUE(worker != nullptr);
    EXPECT_TRUE(worker->isActive());
}

TEST_F(AsyncWorkerManagerTest, CancelAll_AllTasks_CancelsAllTasks) {
    std::function<int()> task1 = []() { return 42; };
    std::function<int()> task2 = []() { return 43; };
    auto worker1 = createAndStartTask(task1);
    auto worker2 = createAndStartTask(task2);
    asyncWorkerManager.cancelAll();
    EXPECT_FALSE(worker1->isActive());
    EXPECT_FALSE(worker2->isActive());
}

TEST_F(AsyncWorkerManagerTest, AllDone_AllTasksDone_ReturnsTrue) {
    std::function<int()> task1 = []() { return 42; };
    std::function<int()> task2 = []() { return 43; };
    createAndStartTask(task1);
    createAndStartTask(task2);
    bool allDone = asyncWorkerManager.allDone();
    EXPECT_TRUE(allDone);
}

TEST_F(AsyncWorkerManagerTest, WaitForAll_AllTasks_WaitsForAllTasks) {
    std::function<int()> task1 = []() { return 42; };
    std::function<int()> task2 = []() { return 43; };
    createAndStartTask(task1);
    createAndStartTask(task2);
    asyncWorkerManager.waitForAll();
    EXPECT_FALSE(asyncWorkerManager.allDone());
}

TEST_F(AsyncWorkerManagerTest, IsDone_ValidWorker_ReturnsExpectedResult) {
    std::function<int()> task = []() { return 42; };
    auto worker = createAndStartTask(task);
    bool isDone = asyncWorkerManager.isDone(worker);
    EXPECT_TRUE(isDone);
}

TEST_F(AsyncWorkerManagerTest, Cancel_ValidWorker_CancelsWorker) {
    std::function<int()> task = []() { return 42; };
    auto worker = createAndStartTask(task);
    asyncWorkerManager.cancel(worker);
    EXPECT_FALSE(worker->isActive());
}
