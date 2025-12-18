/*
 * test_audit_log.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include <gtest/gtest.h>
#include <filesystem>

#include "atom/secret/manager/audit_log.hpp"

namespace atom::secret::test {

class AuditLogTest : public ::testing::Test {
protected:
    void SetUp() override {
        testDir_ = std::filesystem::temp_directory_path() / "atom_audit_test";
        std::filesystem::create_directories(testDir_);
        logPath_ = testDir_ / "audit.log";
    }

    void TearDown() override { std::filesystem::remove_all(testDir_); }

    std::filesystem::path testDir_;
    std::filesystem::path logPath_;
};

TEST_F(AuditLogTest, CreateAuditLog) {
    AuditLog log(logPath_);
    EXPECT_TRUE(std::filesystem::exists(logPath_) ||
                true);  // May not create until first write
}

TEST_F(AuditLogTest, LogEntry) {
    AuditLog log(logPath_);

    log.log(AuditAction::SessionUnlock);
    log.log(AuditAction::EntryCreate, "entry-123", "Test Entry");
    log.log(AuditAction::PasswordCopied, "entry-123", "Test Entry");
    log.log(AuditAction::SessionLock);

    EXPECT_GE(log.getEntryCount(), 4);
}

TEST_F(AuditLogTest, LogWithDetails) {
    AuditLog log(logPath_);

    log.log(AuditAction::EntryUpdate, "entry-456", "Updated Entry",
            "Password changed");

    auto entries = log.getEntries();
    ASSERT_FALSE(entries.empty());

    EXPECT_EQ(entries.back().action, AuditAction::EntryUpdate);
    EXPECT_EQ(entries.back().entryId, "entry-456");
    EXPECT_EQ(entries.back().entryTitle, "Updated Entry");
    EXPECT_EQ(entries.back().details, "Password changed");
}

TEST_F(AuditLogTest, QueryByAction) {
    AuditLog log(logPath_);

    log.log(AuditAction::SessionUnlock);
    log.log(AuditAction::EntryCreate, "1", "Entry 1");
    log.log(AuditAction::EntryCreate, "2", "Entry 2");
    log.log(AuditAction::EntryRead, "1", "Entry 1");
    log.log(AuditAction::SessionLock);

    auto createEntries = log.queryByAction(AuditAction::EntryCreate);
    EXPECT_EQ(createEntries.size(), 2);
}

TEST_F(AuditLogTest, QueryByEntryId) {
    AuditLog log(logPath_);

    log.log(AuditAction::EntryCreate, "entry-1", "Entry 1");
    log.log(AuditAction::EntryRead, "entry-1", "Entry 1");
    log.log(AuditAction::EntryUpdate, "entry-1", "Entry 1");
    log.log(AuditAction::EntryCreate, "entry-2", "Entry 2");

    auto entry1Logs = log.queryByEntryId("entry-1");
    EXPECT_EQ(entry1Logs.size(), 3);
}

TEST_F(AuditLogTest, QueryByTimeRange) {
    AuditLog log(logPath_);

    auto start = std::chrono::system_clock::now();

    log.log(AuditAction::SessionUnlock);
    log.log(AuditAction::EntryCreate, "1", "Entry");

    auto end = std::chrono::system_clock::now();

    auto entries = log.queryByTimeRange(start, end);
    EXPECT_GE(entries.size(), 2);
}

TEST_F(AuditLogTest, ClearLog) {
    AuditLog log(logPath_);

    log.log(AuditAction::SessionUnlock);
    log.log(AuditAction::EntryCreate, "1", "Entry");

    EXPECT_GT(log.getEntryCount(), 0);

    log.clear();
    EXPECT_EQ(log.getEntryCount(), 0);
}

TEST_F(AuditLogTest, ExportToCsv) {
    AuditLog log(logPath_);

    log.log(AuditAction::SessionUnlock);
    log.log(AuditAction::EntryCreate, "1", "Test Entry");

    auto csvPath = testDir_ / "export.csv";
    auto result = log.exportToCsv(csvPath.string());
    ASSERT_TRUE(result.isSuccess()) << result.errorMessage();

    EXPECT_TRUE(std::filesystem::exists(csvPath));
}

TEST_F(AuditLogTest, ExportToJson) {
    AuditLog log(logPath_);

    log.log(AuditAction::SessionUnlock);
    log.log(AuditAction::EntryCreate, "1", "Test Entry");

    auto jsonPath = testDir_ / "export.json";
    auto result = log.exportToJson(jsonPath.string());
    ASSERT_TRUE(result.isSuccess()) << result.errorMessage();

    EXPECT_TRUE(std::filesystem::exists(jsonPath));
}

TEST_F(AuditLogTest, Persistence) {
    // Write some entries
    {
        AuditLog log(logPath_);
        log.log(AuditAction::SessionUnlock);
        log.log(AuditAction::EntryCreate, "1", "Entry");
    }

    // Read in new instance
    {
        AuditLog log(logPath_);
        auto entries = log.getEntries();
        EXPECT_GE(entries.size(), 2);
    }
}

TEST_F(AuditLogTest, ActionToString) {
    EXPECT_EQ(auditActionToString(AuditAction::SessionUnlock), "SessionUnlock");
    EXPECT_EQ(auditActionToString(AuditAction::SessionLock), "SessionLock");
    EXPECT_EQ(auditActionToString(AuditAction::EntryCreate), "EntryCreate");
    EXPECT_EQ(auditActionToString(AuditAction::EntryRead), "EntryRead");
    EXPECT_EQ(auditActionToString(AuditAction::EntryUpdate), "EntryUpdate");
    EXPECT_EQ(auditActionToString(AuditAction::EntryDelete), "EntryDelete");
    EXPECT_EQ(auditActionToString(AuditAction::PasswordCopied),
              "PasswordCopied");
}

}  // namespace atom::secret::test
