#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "atom/system/crontab.hpp"
#include "atom/type/json.hpp"

using namespace testing;
using namespace std::chrono_literals;

// 辅助函数：创建临时JSON文件
class TempJsonFile {
public:
    explicit TempJsonFile(const std::string& content) {
        filename_ = "test_crontab_" +
                    std::to_string(reinterpret_cast<uintptr_t>(this)) + ".json";
        std::ofstream file(filename_);
        file << content;
        file.close();
    }

    ~TempJsonFile() { std::remove(filename_.c_str()); }

    std::string filename() const { return filename_; }

private:
    std::string filename_;
};

// CronJob 类测试
class CronJobTest : public ::testing::Test {
protected:
    CronJob job{"0 0 * * *", "echo test", true, "test", "Test cron job"};
};

TEST_F(CronJobTest, Constructor) {
    EXPECT_EQ("0 0 * * *", job.time_);
    EXPECT_EQ("echo test", job.command_);
    EXPECT_TRUE(job.enabled_);
    EXPECT_EQ("test", job.category_);
    EXPECT_EQ("Test cron job", job.description_);
    EXPECT_EQ(0, job.run_count_);
}

TEST_F(CronJobTest, GetId) {
    std::string id = job.getId();
    EXPECT_FALSE(id.empty());
    // ID应该包含命令的某种形式
    EXPECT_NE(id.find("echo"), std::string::npos);
}

TEST_F(CronJobTest, ToAndFromJson) {
    nlohmann::json jobJson = job.toJson();

    EXPECT_EQ("0 0 * * *", jobJson["time"]);
    EXPECT_EQ("echo test", jobJson["command"]);
    EXPECT_TRUE(jobJson["enabled"]);
    EXPECT_EQ("test", jobJson["category"]);
    EXPECT_EQ("Test cron job", jobJson["description"]);
    EXPECT_EQ(0, jobJson["run_count"]);

    CronJob reconstructedJob = CronJob::fromJson(jobJson);

    EXPECT_EQ(job.time_, reconstructedJob.time_);
    EXPECT_EQ(job.command_, reconstructedJob.command_);
    EXPECT_EQ(job.enabled_, reconstructedJob.enabled_);
    EXPECT_EQ(job.category_, reconstructedJob.category_);
    EXPECT_EQ(job.description_, reconstructedJob.description_);
    EXPECT_EQ(job.run_count_, reconstructedJob.run_count_);
}

// CronManager 类测试
class CronManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        manager = std::make_unique<CronManager>();

        // 添加一些测试作业
        manager->createCronJob(CronJob("0 0 * * *", "echo test1", true,
                                       "category1", "Test job 1"));
        manager->createCronJob(CronJob("0 12 * * *", "echo test2", true,
                                       "category1", "Test job 2"));
        manager->createCronJob(CronJob("0 6 * * 1", "echo test3", false,
                                       "category2", "Test job 3"));
    }

    void TearDown() override {
        // 清理临时文件
        if (!testJsonFile.empty() && std::filesystem::exists(testJsonFile)) {
            std::filesystem::remove(testJsonFile);
        }
    }

    std::unique_ptr<CronManager> manager;
    std::string testJsonFile = "test_crontab_export.json";
};

TEST_F(CronManagerTest, CreateCronJob) {
    // 测试创建有效的 Cron 作业
    bool result = manager->createCronJob(
        CronJob("0 0 * * *", "echo test4", true, "category3", "Test job 4"));
    EXPECT_TRUE(result);

    // 作业应该被添加到列表中
    auto jobs = manager->listCronJobs();
    EXPECT_EQ(4, jobs.size());

    // 检查添加的作业
    auto foundJobs = manager->searchCronJobs("test4");
    EXPECT_EQ(1, foundJobs.size());
    EXPECT_EQ("echo test4", foundJobs[0].command_);

    // 测试创建无效的 Cron 作业
    result = manager->createCronJob(CronJob("invalid_cron", "echo invalid",
                                            true, "invalid", "Invalid job"));
    EXPECT_FALSE(result);

    // 无效作业不应被添加
    jobs = manager->listCronJobs();
    EXPECT_EQ(4, jobs.size());
}

TEST_F(CronManagerTest, ValidateCronExpression) {
    // 测试有效的 cron 表达式
    auto result = CronManager::validateCronExpression("0 0 * * *");
    EXPECT_TRUE(result.valid);

    result = CronManager::validateCronExpression("*/15 * * * *");
    EXPECT_TRUE(result.valid);

    result = CronManager::validateCronExpression("0 0 * * MON-FRI");
    EXPECT_TRUE(result.valid);

    // 测试无效的 cron 表达式
    result = CronManager::validateCronExpression("invalid");
    EXPECT_FALSE(result.valid);
    EXPECT_FALSE(result.message.empty());

    result = CronManager::validateCronExpression("60 24 32 13 8");
    EXPECT_FALSE(result.valid);
}

TEST_F(CronManagerTest, DeleteCronJob) {
    // 删除现有作业
    bool result = manager->deleteCronJob("echo test1");
    EXPECT_TRUE(result);

    // 作业应该从列表中删除
    auto jobs = manager->listCronJobs();
    EXPECT_EQ(2, jobs.size());

    // 删除不存在的作业
    result = manager->deleteCronJob("nonexistent");
    EXPECT_FALSE(result);
}

TEST_F(CronManagerTest, DeleteCronJobById) {
    // 获取作业ID
    auto jobs = manager->listCronJobs();
    ASSERT_GE(jobs.size(), 3);
    std::string jobId = jobs[0].getId();

    // 通过ID删除作业
    bool result = manager->deleteCronJobById(jobId);
    EXPECT_TRUE(result);

    // 作业应该从列表中删除
    jobs = manager->listCronJobs();
    EXPECT_EQ(2, jobs.size());

    // 删除不存在的ID
    result = manager->deleteCronJobById("nonexistent-id");
    EXPECT_FALSE(result);
}

TEST_F(CronManagerTest, ListCronJobs) {
    auto jobs = manager->listCronJobs();

    // 应该有3个作业
    EXPECT_EQ(3, jobs.size());

    // 验证作业内容
    bool foundJob1 = false, foundJob2 = false, foundJob3 = false;

    for (const auto& job : jobs) {
        if (job.command_ == "echo test1")
            foundJob1 = true;
        if (job.command_ == "echo test2")
            foundJob2 = true;
        if (job.command_ == "echo test3")
            foundJob3 = true;
    }

    EXPECT_TRUE(foundJob1);
    EXPECT_TRUE(foundJob2);
    EXPECT_TRUE(foundJob3);
}

TEST_F(CronManagerTest, ListCronJobsByCategory) {
    // 测试类别1
    auto category1Jobs = manager->listCronJobsByCategory("category1");
    EXPECT_EQ(2, category1Jobs.size());

    // 验证类别1作业
    bool foundJob1 = false, foundJob2 = false;

    for (const auto& job : category1Jobs) {
        if (job.command_ == "echo test1")
            foundJob1 = true;
        if (job.command_ == "echo test2")
            foundJob2 = true;
    }

    EXPECT_TRUE(foundJob1);
    EXPECT_TRUE(foundJob2);

    // 测试类别2
    auto category2Jobs = manager->listCronJobsByCategory("category2");
    EXPECT_EQ(1, category2Jobs.size());
    EXPECT_EQ("echo test3", category2Jobs[0].command_);

    // 测试不存在的类别
    auto nonexistentJobs = manager->listCronJobsByCategory("nonexistent");
    EXPECT_TRUE(nonexistentJobs.empty());
}

TEST_F(CronManagerTest, GetCategories) {
    auto categories = manager->getCategories();

    // 应该有两个类别
    EXPECT_EQ(2, categories.size());
    EXPECT_THAT(categories, UnorderedElementsAre("category1", "category2"));
}

TEST_F(CronManagerTest, ExportImportJSON) {
    // 导出作业到JSON文件
    bool exportResult = manager->exportToJSON(testJsonFile);
    EXPECT_TRUE(exportResult);
    EXPECT_TRUE(std::filesystem::exists(testJsonFile));

    // 创建新管理器并导入JSON文件
    auto newManager = std::make_unique<CronManager>();
    bool importResult = newManager->importFromJSON(testJsonFile);
    EXPECT_TRUE(importResult);

    // 验证导入的作业
    auto importedJobs = newManager->listCronJobs();
    EXPECT_EQ(3, importedJobs.size());

    // 验证导入的类别
    auto categories = newManager->getCategories();
    EXPECT_EQ(2, categories.size());

    // 尝试导入不存在的文件
    importResult = newManager->importFromJSON("nonexistent.json");
    EXPECT_FALSE(importResult);
}

TEST_F(CronManagerTest, UpdateCronJob) {
    // 创建新作业
    CronJob updatedJob("30 12 * * *", "echo test1", false, "updated_category",
                       "Updated job");

    // 更新现有作业
    bool result = manager->updateCronJob("echo test1", updatedJob);
    EXPECT_TRUE(result);

    // 验证更新
    auto job = manager->viewCronJob("echo test1");
    EXPECT_EQ("30 12 * * *", job.time_);
    EXPECT_FALSE(job.enabled_);
    EXPECT_EQ("updated_category", job.category_);
    EXPECT_EQ("Updated job", job.description_);

    // 尝试更新不存在的作业
    result = manager->updateCronJob("nonexistent", updatedJob);
    EXPECT_FALSE(result);
}

TEST_F(CronManagerTest, UpdateCronJobById) {
    // 获取作业ID
    auto jobs = manager->listCronJobs();
    ASSERT_GE(jobs.size(), 1);
    std::string jobId = jobs[0].getId();

    // 创建新作业
    CronJob updatedJob("30 12 * * *", jobs[0].command_, false,
                       "updated_category", "Updated job");

    // 通过ID更新作业
    bool result = manager->updateCronJobById(jobId, updatedJob);
    EXPECT_TRUE(result);

    // 验证更新
    auto job = manager->viewCronJobById(jobId);
    EXPECT_EQ("30 12 * * *", job.time_);
    EXPECT_FALSE(job.enabled_);
    EXPECT_EQ("updated_category", job.category_);
    EXPECT_EQ("Updated job", job.description_);

    // 尝试更新不存在的ID
    result = manager->updateCronJobById("nonexistent-id", updatedJob);
    EXPECT_FALSE(result);
}

TEST_F(CronManagerTest, ViewCronJob) {
    // 查看现有作业
    auto job = manager->viewCronJob("echo test1");
    EXPECT_EQ("0 0 * * *", job.time_);
    EXPECT_EQ("echo test1", job.command_);
    EXPECT_EQ("category1", job.category_);

    // 尝试查看不存在的作业
    EXPECT_THROW(manager->viewCronJob("nonexistent"), std::runtime_error);
}

TEST_F(CronManagerTest, ViewCronJobById) {
    // 获取作业ID
    auto jobs = manager->listCronJobs();
    ASSERT_GE(jobs.size(), 1);
    std::string jobId = jobs[0].getId();

    // 通过ID查看作业
    auto job = manager->viewCronJobById(jobId);
    EXPECT_EQ(jobs[0].time_, job.time_);
    EXPECT_EQ(jobs[0].command_, job.command_);
    EXPECT_EQ(jobs[0].category_, job.category_);

    // 尝试查看不存在的ID
    EXPECT_THROW(manager->viewCronJobById("nonexistent-id"),
                 std::runtime_error);
}

TEST_F(CronManagerTest, SearchCronJobs) {
    // 搜索命令
    auto results = manager->searchCronJobs("test1");
    EXPECT_EQ(1, results.size());
    EXPECT_EQ("echo test1", results[0].command_);

    // 搜索类别
    results = manager->searchCronJobs("category1");
    EXPECT_EQ(2, results.size());

    // 搜索描述
    results = manager->searchCronJobs("Test job");
    EXPECT_EQ(3, results.size());

    // 搜索不存在的内容
    results = manager->searchCronJobs("nonexistent");
    EXPECT_TRUE(results.empty());
}

TEST_F(CronManagerTest, Statistics) {
    auto stats = manager->statistics();

    // 验证统计信息
    EXPECT_EQ(3, stats["total"]);
    EXPECT_EQ(2, stats["enabled"]);
    EXPECT_EQ(1, stats["disabled"]);
    EXPECT_EQ(2, stats["category1"]);
    EXPECT_EQ(1, stats["category2"]);
}

TEST_F(CronManagerTest, EnableDisableCronJob) {
    // 禁用启用的作业
    bool result = manager->disableCronJob("echo test1");
    EXPECT_TRUE(result);

    // 验证作业已禁用
    auto job = manager->viewCronJob("echo test1");
    EXPECT_FALSE(job.enabled_);

    // 启用禁用的作业
    result = manager->enableCronJob("echo test1");
    EXPECT_TRUE(result);

    // 验证作业已启用
    job = manager->viewCronJob("echo test1");
    EXPECT_TRUE(job.enabled_);

    // 尝试禁用不存在的作业
    result = manager->disableCronJob("nonexistent");
    EXPECT_FALSE(result);
}

TEST_F(CronManagerTest, SetJobEnabledById) {
    // 获取作业ID
    auto jobs = manager->listCronJobs();
    ASSERT_GE(jobs.size(), 1);
    std::string jobId = jobs[0].getId();

    // 禁用作业
    bool result = manager->setJobEnabledById(jobId, false);
    EXPECT_TRUE(result);

    // 验证作业已禁用
    auto job = manager->viewCronJobById(jobId);
    EXPECT_FALSE(job.enabled_);

    // 启用作业
    result = manager->setJobEnabledById(jobId, true);
    EXPECT_TRUE(result);

    // 验证作业已启用
    job = manager->viewCronJobById(jobId);
    EXPECT_TRUE(job.enabled_);

    // 尝试操作不存在的ID
    result = manager->setJobEnabledById("nonexistent-id", true);
    EXPECT_FALSE(result);
}

TEST_F(CronManagerTest, EnableDisableCronJobsByCategory) {
    // 禁用类别1
    int count = manager->disableCronJobsByCategory("category1");
    EXPECT_EQ(2, count);

    // 验证类别1作业都已禁用
    auto category1Jobs = manager->listCronJobsByCategory("category1");
    for (const auto& job : category1Jobs) {
        EXPECT_FALSE(job.enabled_);
    }

    // 启用类别1
    count = manager->enableCronJobsByCategory("category1");
    EXPECT_EQ(2, count);

    // 验证类别1作业都已启用
    category1Jobs = manager->listCronJobsByCategory("category1");
    for (const auto& job : category1Jobs) {
        EXPECT_TRUE(job.enabled_);
    }

    // 尝试操作不存在的类别
    count = manager->disableCronJobsByCategory("nonexistent");
    EXPECT_EQ(0, count);
}

TEST_F(CronManagerTest, BatchCreateJobs) {
    std::vector<CronJob> newJobs = {
        CronJob("0 1 * * *", "echo batch1", true, "batch", "Batch job 1"),
        CronJob("0 2 * * *", "echo batch2", true, "batch", "Batch job 2"),
        CronJob("invalid", "echo invalid", true, "batch", "Invalid job")};

    // 批量创建作业
    int count = manager->batchCreateJobs(newJobs);
    EXPECT_EQ(2, count);  // 只有两个有效作业应被创建

    // 验证批量创建
    auto batchJobs = manager->listCronJobsByCategory("batch");
    EXPECT_EQ(2, batchJobs.size());
}

TEST_F(CronManagerTest, BatchDeleteJobs) {
    // 准备删除命令
    std::vector<std::string> commands = {"echo test1", "echo test2",
                                         "nonexistent"};

    // 批量删除作业
    int count = manager->batchDeleteJobs(commands);
    EXPECT_EQ(2, count);  // 只有两个有效作业应被删除

    // 验证批量删除
    auto jobs = manager->listCronJobs();
    EXPECT_EQ(1, jobs.size());
    EXPECT_EQ("echo test3", jobs[0].command_);
}

TEST_F(CronManagerTest, RecordJobExecution) {
    // 记录作业执行
    bool result = manager->recordJobExecution("echo test1");
    EXPECT_TRUE(result);

    // 验证执行计数和最后运行时间
    auto job = manager->viewCronJob("echo test1");
    EXPECT_EQ(1, job.run_count_);
    EXPECT_NE(job.last_run_, std::chrono::system_clock::time_point());

    // 记录另一次执行
    result = manager->recordJobExecution("echo test1");
    EXPECT_TRUE(result);

    // 验证执行计数增加
    job = manager->viewCronJob("echo test1");
    EXPECT_EQ(2, job.run_count_);

    // 尝试记录不存在的作业
    result = manager->recordJobExecution("nonexistent");
    EXPECT_FALSE(result);
}

TEST_F(CronManagerTest, ClearAllJobs) {
    // 清除所有作业
    bool result = manager->clearAllJobs();
    EXPECT_TRUE(result);

    // 验证所有作业已清除
    auto jobs = manager->listCronJobs();
    EXPECT_TRUE(jobs.empty());
}

// 边缘案例测试
TEST_F(CronManagerTest, EdgeCases) {
    // 测试空命令
    bool result = manager->createCronJob(CronJob("0 0 * * *", "", true));
    EXPECT_FALSE(result);

    // 测试非常长的命令
    std::string longCommand(10000, 'a');
    result = manager->createCronJob(CronJob("0 0 * * *", longCommand, true));
    // 根据实现，可能接受或拒绝非常长的命令

    // 测试特殊字符
    result = manager->createCronJob(
        CronJob("0 0 * * *", "echo \"special'chars`$\\\"", true));
    EXPECT_TRUE(result);

    // 测试重复创建相同命令的作业
    result = manager->createCronJob(CronJob("0 0 * * *", "echo test1", true));
    EXPECT_FALSE(result);
}
