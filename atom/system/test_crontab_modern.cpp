#include <gtest/gtest.h>
#include "crontab_modern.hpp"
#include "crontab_errors.hpp"
#include "crontab_types.hpp"
#include <thread>
#include <chrono>
#include <filesystem>

namespace atom::system::test {

class CrontabModernTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Clean up any existing test files
        if (std::filesystem::exists("test_cron_jobs.json")) {
            std::filesystem::remove("test_cron_jobs.json");
        }
    }

    void TearDown() override {
        // Clean up test files
        if (std::filesystem::exists("test_cron_jobs.json")) {
            std::filesystem::remove("test_cron_jobs.json");
        }
    }
};

// Test CronExpression validation
TEST_F(CrontabModernTest, CronExpressionValidation) {
    // Valid expressions
    EXPECT_TRUE(CronExpression::validate("0 0 * * *"));
    EXPECT_TRUE(CronExpression::validate("*/5 * * * *"));
    EXPECT_TRUE(CronExpression::validate("0 9-17 * * 1-5"));
    EXPECT_TRUE(CronExpression::validate("30 2 1 * *"));
    
    // Invalid expressions
    EXPECT_FALSE(CronExpression::validate(""));
    EXPECT_FALSE(CronExpression::validate("0 0 * *"));  // Missing field
    EXPECT_FALSE(CronExpression::validate("60 0 * * *"));  // Invalid minute
    EXPECT_FALSE(CronExpression::validate("0 25 * * *"));  // Invalid hour
    EXPECT_FALSE(CronExpression::validate("0 0 32 * *"));  // Invalid day
    EXPECT_FALSE(CronExpression::validate("0 0 * 13 *"));  // Invalid month
    EXPECT_FALSE(CronExpression::validate("0 0 * * 8"));   // Invalid weekday
}

// Test CronExpression parsing
TEST_F(CrontabModernTest, CronExpressionParsing) {
    auto result = CronExpression::parse("0 9 * * 1-5");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->value(), "0 9 * * 1-5");
    
    auto invalidResult = CronExpression::parse("invalid");
    EXPECT_FALSE(invalidResult.has_value());
    EXPECT_EQ(invalidResult.error(), make_error_code(CrontabError::INVALID_EXPRESSION));
}

// Test strong types
TEST_F(CrontabModernTest, StrongTypes) {
    JobId id{"test-job"};
    CronExpression expr = CronExpression::parse("0 0 * * *").value();
    Command cmd{"echo hello"};
    
    EXPECT_EQ(id.value(), "test-job");
    EXPECT_EQ(expr.value(), "0 0 * * *");
    EXPECT_EQ(cmd.value(), "echo hello");
    
    // Test that different strong types are distinct
    static_assert(!std::is_same_v<JobId, Command>);
    static_assert(!std::is_same_v<JobId, CronExpression>);
    static_assert(!std::is_same_v<Command, CronExpression>);
}

// Test DataView zero-copy semantics
TEST_F(CrontabModernTest, DataViewZeroCopy) {
    std::vector<char> data = {'h', 'e', 'l', 'l', 'o'};
    DataView<char> view(data);
    
    EXPECT_EQ(view.size(), 5);
    EXPECT_FALSE(view.empty());
    EXPECT_EQ(view.data(), data.data());  // Same pointer - zero copy
    EXPECT_EQ(view[0], 'h');
    
    // Test with span
    std::span<const char> span(data);
    DataView<char> spanView(span);
    EXPECT_EQ(spanView.size(), 5);
    EXPECT_EQ(spanView.data(), data.data());
}

// Test JobStatistics thread safety
TEST_F(CrontabModernTest, JobStatisticsThreadSafety) {
    JobStatistics stats;
    
    // Test concurrent access
    std::vector<std::thread> threads;
    constexpr int numThreads = 10;
    constexpr int numOperations = 100;
    
    for (int i = 0; i < numThreads; ++i) {
        threads.emplace_back([&stats, numOperations]() {
            for (int j = 0; j < numOperations; ++j) {
                if (j % 2 == 0) {
                    stats.incrementSuccess();
                } else {
                    stats.incrementFailure();
                }
            }
        });
    }
    
    for (auto& thread : threads) {
        thread.join();
    }
    
    EXPECT_EQ(stats.getTotalRuns(), numThreads * numOperations);
    EXPECT_EQ(stats.getSuccessCount(), numThreads * numOperations / 2);
    EXPECT_EQ(stats.getFailureCount(), numThreads * numOperations / 2);
    EXPECT_DOUBLE_EQ(stats.getSuccessRate(), 0.5);
}

// Test CronJob creation and properties
TEST_F(CrontabModernTest, CronJobCreation) {
    auto id = JobId{"test-job"};
    auto expr = CronExpression::parse("0 9 * * 1-5").value();
    auto cmd = Command{"echo test"};
    
    CronJob job(std::move(id), std::move(expr), std::move(cmd));
    
    EXPECT_EQ(job.getId().value(), "test-job");
    EXPECT_EQ(job.getExpression().value(), "0 9 * * 1-5");
    EXPECT_EQ(job.getCommand().value(), "echo test");
    EXPECT_EQ(job.getStatus(), JobStatus::INACTIVE);
    EXPECT_FALSE(job.isScheduled());
    
    job.setStatus(JobStatus::ACTIVE);
    EXPECT_EQ(job.getStatus(), JobStatus::ACTIVE);
    EXPECT_TRUE(job.isScheduled());
}

// Test JobBuilder pattern
TEST_F(CrontabModernTest, JobBuilderPattern) {
    auto jobResult = JobBuilder{}
        .withId(JobId{"builder-job"})
        .withExpression(CronExpression::parse("0 12 * * *").value())
        .withCommand(Command{"echo builder"})
        .build();
    
    ASSERT_TRUE(jobResult.has_value());
    auto& job = jobResult.value();
    
    EXPECT_EQ(job.getId().value(), "builder-job");
    EXPECT_EQ(job.getExpression().value(), "0 12 * * *");
    EXPECT_EQ(job.getCommand().value(), "echo builder");
    
    // Test incomplete builder
    auto incompleteResult = JobBuilder{}
        .withId(JobId{"incomplete"})
        .build();
    
    EXPECT_FALSE(incompleteResult.has_value());
    EXPECT_EQ(incompleteResult.error(), make_error_code(CrontabError::INVALID_ARGUMENT));
}

// Test CronJob JSON serialization
TEST_F(CrontabModernTest, CronJobSerialization) {
    auto job = JobBuilder{}
        .withId(JobId{"json-job"})
        .withExpression(CronExpression::parse("0 8 * * *").value())
        .withCommand(Command{"echo json"})
        .build().value();
    
    job.setStatus(JobStatus::ACTIVE);
    job.recordSuccess();
    job.recordFailure();
    
    auto json = job.toJson();
    EXPECT_EQ(json["id"], "json-job");
    EXPECT_EQ(json["expression"], "0 8 * * *");
    EXPECT_EQ(json["command"], "echo json");
    EXPECT_EQ(json["status"], static_cast<int>(JobStatus::ACTIVE));
    
    // Test deserialization
    auto deserializedResult = CronJob::fromJson(json);
    ASSERT_TRUE(deserializedResult.has_value());
    auto& deserializedJob = deserializedResult.value();
    
    EXPECT_EQ(deserializedJob.getId().value(), "json-job");
    EXPECT_EQ(deserializedJob.getExpression().value(), "0 8 * * *");
    EXPECT_EQ(deserializedJob.getCommand().value(), "echo json");
    EXPECT_EQ(deserializedJob.getStatus(), JobStatus::ACTIVE);
}

// Test CronManager basic operations
TEST_F(CrontabModernTest, CronManagerBasicOperations) {
    CronManager manager("test_cron_jobs.json");
    
    // Test initial state
    EXPECT_EQ(manager.getJobCount(), 0);
    EXPECT_EQ(manager.getActiveJobCount(), 0);
    EXPECT_TRUE(manager.listJobs().empty());
    
    // Add a job
    auto job = JobBuilder{}
        .withId(JobId{"manager-job"})
        .withExpression(CronExpression::parse("0 10 * * *").value())
        .withCommand(Command{"echo manager"})
        .build().value();
    
    auto addResult = manager.addJob(std::move(job));
    EXPECT_TRUE(addResult.has_value());
    EXPECT_EQ(manager.getJobCount(), 1);
    EXPECT_EQ(manager.getActiveJobCount(), 1);
    
    // Get the job
    auto getResult = manager.getJob(JobId{"manager-job"});
    ASSERT_TRUE(getResult.has_value());
    const auto& retrievedJob = getResult.value().get();
    EXPECT_EQ(retrievedJob.getId().value(), "manager-job");
    
    // List jobs
    auto jobs = manager.listJobs();
    EXPECT_EQ(jobs.size(), 1);
    EXPECT_EQ(jobs[0].get().getId().value(), "manager-job");
    
    auto jobIds = manager.listJobIds();
    EXPECT_EQ(jobIds.size(), 1);
    EXPECT_EQ(jobIds[0].value(), "manager-job");
    
    // Remove the job
    auto removeResult = manager.removeJob(JobId{"manager-job"});
    EXPECT_TRUE(removeResult.has_value());
    EXPECT_EQ(manager.getJobCount(), 0);
}

// Test CronManager duplicate job handling
TEST_F(CrontabModernTest, CronManagerDuplicateJobs) {
    CronManager manager("test_cron_jobs.json");
    
    auto job1 = JobBuilder{}
        .withId(JobId{"duplicate-job"})
        .withExpression(CronExpression::parse("0 10 * * *").value())
        .withCommand(Command{"echo first"})
        .build().value();
    
    auto job2 = JobBuilder{}
        .withId(JobId{"duplicate-job"})
        .withExpression(CronExpression::parse("0 11 * * *").value())
        .withCommand(Command{"echo second"})
        .build().value();
    
    // First job should succeed
    auto addResult1 = manager.addJob(std::move(job1));
    EXPECT_TRUE(addResult1.has_value());
    
    // Second job with same ID should fail
    auto addResult2 = manager.addJob(std::move(job2));
    EXPECT_FALSE(addResult2.has_value());
    EXPECT_EQ(addResult2.error(), make_error_code(CrontabError::JOB_EXISTS));
}

// Test CronManager job not found
TEST_F(CrontabModernTest, CronManagerJobNotFound) {
    CronManager manager("test_cron_jobs.json");
    
    auto getResult = manager.getJob(JobId{"nonexistent"});
    EXPECT_FALSE(getResult.has_value());
    EXPECT_EQ(getResult.error(), make_error_code(CrontabError::JOB_NOT_FOUND));
    
    auto removeResult = manager.removeJob(JobId{"nonexistent"});
    EXPECT_FALSE(removeResult.has_value());
    EXPECT_EQ(removeResult.error(), make_error_code(CrontabError::JOB_NOT_FOUND));
}

// Test CronManager clear operation
TEST_F(CrontabModernTest, CronManagerClear) {
    CronManager manager("test_cron_jobs.json");
    
    // Add multiple jobs
    for (int i = 0; i < 5; ++i) {
        auto job = JobBuilder{}
            .withId(JobId{"job-" + std::to_string(i)})
            .withExpression(CronExpression::parse("0 " + std::to_string(i) + " * * *").value())
            .withCommand(Command{"echo " + std::to_string(i)})
            .build().value();
        
        auto addResult = manager.addJob(std::move(job));
        EXPECT_TRUE(addResult.has_value());
    }
    
    EXPECT_EQ(manager.getJobCount(), 5);
    
    auto clearResult = manager.clear();
    EXPECT_TRUE(clearResult.has_value());
    EXPECT_EQ(manager.getJobCount(), 0);
}

// Test ScopeGuard RAII
TEST_F(CrontabModernTest, ScopeGuardRAII) {
    bool cleaned = false;
    
    {
        auto guard = make_scope_guard([&cleaned]() {
            cleaned = true;
        });
        
        EXPECT_FALSE(cleaned);
    }  // guard goes out of scope
    
    EXPECT_TRUE(cleaned);
}

// Test ScopeGuard release
TEST_F(CrontabModernTest, ScopeGuardRelease) {
    bool cleaned = false;
    
    {
        auto guard = make_scope_guard([&cleaned]() {
            cleaned = true;
        });
        
        guard.release();  // Disable cleanup
    }  // guard goes out of scope
    
    EXPECT_FALSE(cleaned);  // Should not have been cleaned
}

// Test error code conversion
TEST_F(CrontabModernTest, ErrorCodeConversion) {
    auto ec = make_error_code(CrontabError::JOB_NOT_FOUND);
    EXPECT_EQ(ec.category(), crontab_category());
    EXPECT_EQ(ec.message(), "Job not found");
    
    // Test exception conversion
    try {
        throw CrontabException("Test exception");
    } catch (const CrontabException& e) {
        EXPECT_STREQ(e.what(), "Test exception");
    }
}

// Test thread safety of CronManager
TEST_F(CrontabModernTest, CronManagerThreadSafety) {
    CronManager manager("test_cron_jobs.json");
    
    std::vector<std::thread> threads;
    constexpr int numThreads = 10;
    constexpr int jobsPerThread = 10;
    
    // Add jobs concurrently
    for (int t = 0; t < numThreads; ++t) {
        threads.emplace_back([&manager, t, jobsPerThread]() {
            for (int j = 0; j < jobsPerThread; ++j) {
                auto jobId = "thread-" + std::to_string(t) + "-job-" + std::to_string(j);
                auto job = JobBuilder{}
                    .withId(JobId{jobId})
                    .withExpression(CronExpression::parse("0 0 * * *").value())
                    .withCommand(Command{"echo " + jobId})
                    .build().value();
                
                manager.addJob(std::move(job));
            }
        });
    }
    
    for (auto& thread : threads) {
        thread.join();
    }
    
    EXPECT_EQ(manager.getJobCount(), numThreads * jobsPerThread);
    
    // Read operations concurrently
    threads.clear();
    for (int t = 0; t < numThreads; ++t) {
        threads.emplace_back([&manager]() {
            auto jobs = manager.listJobs();
            auto jobIds = manager.listJobIds();
            auto count = manager.getJobCount();
            auto activeCount = manager.getActiveJobCount();
            
            // Just ensure no crashes
            EXPECT_GE(count, 0);
            EXPECT_GE(activeCount, 0);
        });
    }
    
    for (auto& thread : threads) {
        thread.join();
    }
}

// Performance test for large numbers of jobs
TEST_F(CrontabModernTest, PerformanceTest) {
    CronManager manager("test_cron_jobs.json");
    
    auto start = std::chrono::high_resolution_clock::now();
    
    constexpr int numJobs = 1000;
    for (int i = 0; i < numJobs; ++i) {
        auto job = JobBuilder{}
            .withId(JobId{"perf-job-" + std::to_string(i)})
            .withExpression(CronExpression::parse("0 0 * * *").value())
            .withCommand(Command{"echo " + std::to_string(i)})
            .build().value();
        
        auto result = manager.addJob(std::move(job));
        EXPECT_TRUE(result.has_value());
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    EXPECT_EQ(manager.getJobCount(), numJobs);
    
    // Should be able to add 1000 jobs reasonably quickly
    EXPECT_LT(duration.count(), 5000);  // Less than 5 seconds
    
    std::cout << "Added " << numJobs << " jobs in " << duration.count() << "ms" << std::endl;
}

} // namespace atom::system::test
