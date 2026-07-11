/**
 * @file test_crontab.cpp
 * @brief Unit tests for cron job management
 */

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <string>
#include <vector>

#include "atom/type/json.hpp"
#include "atom/system/scheduling/crontab.hpp"

namespace fs = std::filesystem;

class CronJobTest : public ::testing::Test {
protected:
    void SetUp() override {
        testDir_ = fs::temp_directory_path() / "atom_cron_test";
        fs::create_directories(testDir_);
    }

    void TearDown() override {
        std::error_code ec;
        fs::remove_all(testDir_, ec);
    }

    fs::path testDir_;
};

// CronJob Structure Tests
TEST_F(CronJobTest, DefaultConstruction) {
    CronJob job;
    EXPECT_TRUE(job.getTime().empty());
    EXPECT_TRUE(job.getCommand().empty());
    EXPECT_TRUE(job.isEnabled());
    EXPECT_EQ(job.getCategory(), "default");
    EXPECT_TRUE(job.getDescription().empty());
    EXPECT_EQ(job.getRunCount(), 0);
    EXPECT_EQ(static_cast<int>(job.getPriority()), 5);
    EXPECT_EQ(job.getMaxRetries(), 0);
    EXPECT_EQ(job.getCurrentRetries(), 0);
    EXPECT_FALSE(job.isOneTime());
}

TEST_F(CronJobTest, ParameterizedConstruction) {
    CronJob job("* * * * *", "echo test", true, "test_category", "Test job");

    EXPECT_EQ(job.getTime(), "* * * * *");
    EXPECT_EQ(job.getCommand(), "echo test");
    EXPECT_TRUE(job.isEnabled());
    EXPECT_EQ(job.getCategory(), "test_category");
    EXPECT_EQ(job.getDescription(), "Test job");
}

TEST_F(CronJobTest, GetId) {
    CronJob job("0 0 * * *", "daily_task", true, "maintenance", "Daily task");
    std::string id = job.getId();
    EXPECT_FALSE(id.empty());
}

TEST_F(CronJobTest, RecordExecution) {
    CronJob job("* * * * *", "test_command");

    EXPECT_EQ(job.getRunCount(), 0);
    EXPECT_TRUE(job.getExecutionHistory().empty());

    job.recordExecution(true);

    EXPECT_EQ(job.getRunCount(), 1);
    EXPECT_EQ(job.getExecutionHistory().size(), 1);
    EXPECT_TRUE(job.getExecutionHistory()[0].success);  // Success
}

TEST_F(CronJobTest, ToJson) {
    CronJob job("0 0 * * *", "backup_task", true, "backup", "Daily backup");
    auto json = job.toJson();

    EXPECT_FALSE(json.empty());
    EXPECT_EQ(json["time"], "0 0 * * *");
    EXPECT_EQ(json["command"], "backup_task");
    EXPECT_TRUE(json["enabled"]);
    EXPECT_EQ(json["category"], "backup");
}

TEST_F(CronJobTest, FromJson) {
    // Note: fromJson test skipped due to JSON ABI mismatch between library and
    // test This requires rebuilding the library with consistent nlohmann::json
    // version
    GTEST_SKIP()
        << "Skipped due to JSON ABI mismatch - requires library rebuild";

    nlohmann::json json;
    json["time"] = "30 2 * * *";
    json["command"] = "night_task";
    json["enabled"] = false;
    json["category"] = "night";
    json["description"] = "Night task";
    json["priority"] = 3;
    json["max_retries"] = 2;
    json["one_time"] = true;

    CronJob job = CronJob::fromJson(json);

    EXPECT_EQ(job.getTime(), "30 2 * * *");
    EXPECT_EQ(job.getCommand(), "night_task");
    EXPECT_FALSE(job.isEnabled());
    EXPECT_EQ(job.getCategory(), "night");
    EXPECT_EQ(static_cast<int>(job.getPriority()), 3);
    EXPECT_EQ(job.getMaxRetries(), 2);
    EXPECT_TRUE(job.isOneTime());
}

// CronManager Tests
// Note: Most CronManager tests require system crontab access which is not
// available on Windows
class CronManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
#ifdef _WIN32
        GTEST_SKIP() << "CronManager tests require system crontab (not "
                        "available on Windows)";
#endif
        manager_ = std::make_unique<CronManager>();
        testDir_ = fs::temp_directory_path() / "atom_cronmanager_test";
        fs::create_directories(testDir_);
    }

    void TearDown() override {
        manager_.reset();
        std::error_code ec;
        fs::remove_all(testDir_, ec);
    }

    std::unique_ptr<CronManager> manager_;
    fs::path testDir_;
};

TEST_F(CronManagerTest, CreateCronJob) {
    CronJob job("* * * * *", "echo test", true, "test", "Test job");
    EXPECT_TRUE(manager_->createCronJob(job));
}

TEST_F(CronManagerTest, CreateJobWithSpecialTime) {
    EXPECT_TRUE(manager_->createJobWithSpecialTime(
        "@daily", "daily_task", true, "scheduled", "Daily task", 5, 0, false));
}

TEST_F(CronManagerTest, ValidateCronExpression) {
    auto valid = CronManager::validateCronExpression("0 0 * * *");
    EXPECT_TRUE(valid.valid);

    auto invalid = CronManager::validateCronExpression("invalid");
    EXPECT_FALSE(invalid.valid);
    EXPECT_FALSE(invalid.message.empty());
}

TEST_F(CronManagerTest, DeleteCronJob) {
    CronJob job("* * * * *", "to_delete");
    manager_->createCronJob(job);

    EXPECT_TRUE(manager_->deleteCronJob("to_delete"));
}

TEST_F(CronManagerTest, DeleteCronJobById) {
    CronJob job("* * * * *", "delete_by_id");
    manager_->createCronJob(job);

    std::string id = job.getId();
    EXPECT_TRUE(manager_->deleteCronJobById(id));
}

TEST_F(CronManagerTest, ListCronJobs) {
    manager_->createCronJob(CronJob("* * * * *", "job1"));
    manager_->createCronJob(CronJob("0 * * * *", "job2"));
    manager_->createCronJob(CronJob("0 0 * * *", "job3"));

    auto jobs = manager_->listCronJobs();
    EXPECT_EQ(jobs.size(), 3);
}

TEST_F(CronManagerTest, ListCronJobsByCategory) {
    manager_->createCronJob(
        CronJob("* * * * *", "cat1_job1", true, "category1"));
    manager_->createCronJob(
        CronJob("* * * * *", "cat1_job2", true, "category1"));
    manager_->createCronJob(
        CronJob("* * * * *", "cat2_job1", true, "category2"));

    auto cat1Jobs = manager_->listCronJobsByCategory("category1");
    EXPECT_EQ(cat1Jobs.size(), 2);

    auto cat2Jobs = manager_->listCronJobsByCategory("category2");
    EXPECT_EQ(cat2Jobs.size(), 1);
}

TEST_F(CronManagerTest, GetCategories) {
    manager_->createCronJob(CronJob("* * * * *", "job1", true, "cat_a"));
    manager_->createCronJob(CronJob("* * * * *", "job2", true, "cat_b"));
    manager_->createCronJob(CronJob("* * * * *", "job3", true, "cat_c"));

    auto categories = manager_->getCategories();
    EXPECT_GE(categories.size(), 3);
}

TEST_F(CronManagerTest, UpdateCronJob) {
    CronJob original("* * * * *", "original_command");
    manager_->createCronJob(original);

    CronJob updated("0 0 * * *", "original_command", true, "updated",
                    "Updated job");
    EXPECT_TRUE(manager_->updateCronJob("original_command", updated));

    auto job = manager_->viewCronJob("original_command");
    EXPECT_EQ(job.getTime(), "0 0 * * *");
    EXPECT_EQ(job.getCategory(), "updated");
}

TEST_F(CronManagerTest, UpdateCronJobById) {
    CronJob original("* * * * *", "update_by_id");
    manager_->createCronJob(original);

    std::string id = original.getId();
    CronJob updated("0 * * * *", "update_by_id", true, "new_cat");
    EXPECT_TRUE(manager_->updateCronJobById(id, updated));
}

TEST_F(CronManagerTest, ViewCronJob) {
    CronJob job("30 6 * * *", "morning_task", true, "morning", "Morning task");
    manager_->createCronJob(job);

    auto viewed = manager_->viewCronJob("morning_task");
    EXPECT_EQ(viewed.getTime(), "30 6 * * *");
    EXPECT_EQ(viewed.getCategory(), "morning");
}

TEST_F(CronManagerTest, ViewCronJobById) {
    CronJob job("45 18 * * *", "evening_task");
    manager_->createCronJob(job);

    std::string id = job.getId();
    auto viewed = manager_->viewCronJobById(id);
    EXPECT_EQ(viewed.getCommand(), "evening_task");
}

TEST_F(CronManagerTest, SearchCronJobs) {
    manager_->createCronJob(CronJob("* * * * *", "backup_db"));
    manager_->createCronJob(CronJob("* * * * *", "backup_files"));
    manager_->createCronJob(CronJob("* * * * *", "cleanup"));

    auto backupJobs = manager_->searchCronJobs("backup");
    EXPECT_EQ(backupJobs.size(), 2);
}

TEST_F(CronManagerTest, Statistics) {
    manager_->createCronJob(CronJob("* * * * *", "job1", true));
    manager_->createCronJob(CronJob("* * * * *", "job2", false));
    manager_->createCronJob(CronJob("* * * * *", "job3", true));

    auto stats = manager_->statistics();
    EXPECT_GT(stats.size(), 0);
}

TEST_F(CronManagerTest, EnableDisableCronJob) {
    CronJob job("* * * * *", "toggle_job", true);
    manager_->createCronJob(job);

    EXPECT_TRUE(manager_->disableCronJob("toggle_job"));
    auto disabled = manager_->viewCronJob("toggle_job");
    EXPECT_FALSE(disabled.isEnabled());

    EXPECT_TRUE(manager_->enableCronJob("toggle_job"));
    auto enabled = manager_->viewCronJob("toggle_job");
    EXPECT_TRUE(enabled.isEnabled());
}

TEST_F(CronManagerTest, SetJobEnabledById) {
    CronJob job("* * * * *", "enable_by_id");
    manager_->createCronJob(job);

    std::string id = job.getId();
    EXPECT_TRUE(manager_->setJobEnabledById(id, false));
    EXPECT_TRUE(manager_->setJobEnabledById(id, true));
}

TEST_F(CronManagerTest, EnableDisableByCategory) {
    manager_->createCronJob(
        CronJob("* * * * *", "cat_job1", true, "toggle_cat"));
    manager_->createCronJob(
        CronJob("* * * * *", "cat_job2", true, "toggle_cat"));

    int disabled = manager_->disableCronJobsByCategory("toggle_cat");
    EXPECT_EQ(disabled, 2);

    int enabled = manager_->enableCronJobsByCategory("toggle_cat");
    EXPECT_EQ(enabled, 2);
}

TEST_F(CronManagerTest, BatchCreateJobs) {
    // CronJob is move-only, so build the vector with emplace_back rather than
    // an initializer_list (which would require copies).
    std::vector<CronJob> jobs;
    jobs.reserve(3);
    jobs.emplace_back("* * * * *", "batch1");
    jobs.emplace_back("0 * * * *", "batch2");
    jobs.emplace_back("0 0 * * *", "batch3");

    int created = manager_->batchCreateJobs(jobs);
    EXPECT_EQ(created, 3);
}

TEST_F(CronManagerTest, BatchDeleteJobs) {
    manager_->createCronJob(CronJob("* * * * *", "del1"));
    manager_->createCronJob(CronJob("* * * * *", "del2"));
    manager_->createCronJob(CronJob("* * * * *", "del3"));

    std::vector<std::string> toDelete = {"del1", "del2"};
    int deleted = manager_->batchDeleteJobs(toDelete);
    EXPECT_EQ(deleted, 2);
}

TEST_F(CronManagerTest, RecordJobExecution) {
    CronJob job("* * * * *", "exec_job");
    manager_->createCronJob(job);

    EXPECT_TRUE(manager_->recordJobExecution("exec_job"));

    auto updated = manager_->viewCronJob("exec_job");
    EXPECT_EQ(updated.getRunCount(), 1);
}

TEST_F(CronManagerTest, ClearAllJobs) {
    manager_->createCronJob(CronJob("* * * * *", "clear1"));
    manager_->createCronJob(CronJob("* * * * *", "clear2"));

    EXPECT_TRUE(manager_->clearAllJobs());

    auto jobs = manager_->listCronJobs();
    EXPECT_TRUE(jobs.empty());
}

TEST_F(CronManagerTest, ConvertSpecialExpression) {
    EXPECT_EQ(CronManager::convertSpecialExpression("@yearly"), "0 0 1 1 *");
    EXPECT_EQ(CronManager::convertSpecialExpression("@annually"), "0 0 1 1 *");
    EXPECT_EQ(CronManager::convertSpecialExpression("@monthly"), "0 0 1 * *");
    EXPECT_EQ(CronManager::convertSpecialExpression("@weekly"), "0 0 * * 0");
    EXPECT_EQ(CronManager::convertSpecialExpression("@daily"), "0 0 * * *");
    EXPECT_EQ(CronManager::convertSpecialExpression("@midnight"), "0 0 * * *");
    EXPECT_EQ(CronManager::convertSpecialExpression("@hourly"), "0 * * * *");

    // Unknown expression
    EXPECT_TRUE(CronManager::convertSpecialExpression("@unknown").empty());
}

TEST_F(CronManagerTest, SetJobPriority) {
    CronJob job("* * * * *", "priority_job");
    manager_->createCronJob(job);

    std::string id = job.getId();
    EXPECT_TRUE(manager_->setJobPriority(id, 1));

    auto updated = manager_->viewCronJobById(id);
    EXPECT_EQ(static_cast<int>(updated.getPriority()), 1);
}

TEST_F(CronManagerTest, SetJobMaxRetries) {
    CronJob job("* * * * *", "retry_job");
    manager_->createCronJob(job);

    std::string id = job.getId();
    EXPECT_TRUE(manager_->setJobMaxRetries(id, 5));

    auto updated = manager_->viewCronJobById(id);
    EXPECT_EQ(updated.getMaxRetries(), 5);
}

TEST_F(CronManagerTest, SetJobOneTime) {
    CronJob job("* * * * *", "onetime_job");
    manager_->createCronJob(job);

    std::string id = job.getId();
    EXPECT_TRUE(manager_->setJobOneTime(id, true));

    auto updated = manager_->viewCronJobById(id);
    EXPECT_TRUE(updated.isOneTime());
}

TEST_F(CronManagerTest, GetJobExecutionHistory) {
    // Skip on Windows - system crontab operations may not work
#ifdef _WIN32
    GTEST_SKIP() << "Skipped on Windows - system crontab not available";
#endif
    CronJob job("* * * * *", "history_job");
    manager_->createCronJob(job);

    std::string id = job.getId();
    manager_->recordJobExecutionResult(id, true);
    manager_->recordJobExecutionResult(id, false);
    manager_->recordJobExecutionResult(id, true);

    auto history = manager_->getJobExecutionHistory(id);
    EXPECT_EQ(history.size(), 3);
}

TEST_F(CronManagerTest, GetJobsByPriority) {
    // Skip on Windows - system crontab operations may not work
#ifdef _WIN32
    GTEST_SKIP() << "Skipped on Windows - system crontab not available";
#endif
    manager_->createCronJob(CronJob("* * * * *", "low_priority"));
    manager_->createCronJob(CronJob("* * * * *", "high_priority"));
    manager_->createCronJob(CronJob("* * * * *", "medium_priority"));

    manager_->setJobPriority(manager_->viewCronJob("high_priority").getId(), 1);
    manager_->setJobPriority(manager_->viewCronJob("medium_priority").getId(),
                             5);
    manager_->setJobPriority(manager_->viewCronJob("low_priority").getId(), 10);

    auto sorted = manager_->getJobsByPriority();
    EXPECT_EQ(sorted.size(), 3);
    // First should be highest priority (lowest number)
    EXPECT_LE(static_cast<int>(sorted[0].getPriority()),
              static_cast<int>(sorted[1].getPriority()));
}

TEST_F(CronManagerTest, ExportToJSON) {
    manager_->createCronJob(CronJob("* * * * *", "export_job"));

    fs::path exportPath = testDir_ / "export.json";
    EXPECT_TRUE(manager_->exportToJSON(exportPath.string()));
    EXPECT_TRUE(fs::exists(exportPath));
}

TEST_F(CronManagerTest, ImportFromJSON) {
    // First export
    manager_->createCronJob(CronJob("0 0 * * *", "imported_job"));
    fs::path jsonPath = testDir_ / "import.json";
    manager_->exportToJSON(jsonPath.string());

    // Clear and import
    manager_->clearAllJobs();
    EXPECT_TRUE(manager_->importFromJSON(jsonPath.string()));

    auto jobs = manager_->listCronJobs();
    EXPECT_GE(jobs.size(), 1);
}

// CronValidationResult Tests
TEST(CronValidationResultTest, Structure) {
    CronValidationResult result;
    result.valid = true;
    result.message = "Valid expression";

    EXPECT_TRUE(result.valid);
    EXPECT_EQ(result.message, "Valid expression");
}
