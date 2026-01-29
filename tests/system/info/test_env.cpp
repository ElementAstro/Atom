/**
 * @file test_env.cpp
 * @brief Unit tests for environment variable management
 */

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>

#include "atom/system/info/env.hpp"

namespace atom::utils::test {

namespace fs = std::filesystem;

class EnvTest : public ::testing::Test {
protected:
    void SetUp() override {
        env_ = std::make_unique<Env>();
        testDir_ = fs::temp_directory_path() / "atom_env_test";
        fs::create_directories(testDir_);
    }

    void TearDown() override {
        env_.reset();
        std::error_code ec;
        fs::remove_all(testDir_, ec);
    }

    std::unique_ptr<Env> env_;
    fs::path testDir_;
};

// Basic Environment Variable Operations
TEST_F(EnvTest, AddAndGetVariable) {
    env_->add("TEST_VAR", "test_value");
    EXPECT_TRUE(env_->has("TEST_VAR"));
    EXPECT_EQ(env_->get("TEST_VAR"), "test_value");
}

TEST_F(EnvTest, GetNonexistentVariable) {
    EXPECT_FALSE(env_->has("NONEXISTENT_VAR"));
    EXPECT_EQ(env_->get("NONEXISTENT_VAR", "default"), "default");
}

TEST_F(EnvTest, DeleteVariable) {
    env_->add("DELETE_ME", "value");
    EXPECT_TRUE(env_->has("DELETE_ME"));
    env_->del("DELETE_ME");
    EXPECT_FALSE(env_->has("DELETE_ME"));
}

TEST_F(EnvTest, AddMultipleVariables) {
    HashMap<String, String> vars;
    vars["VAR1"] = "value1";
    vars["VAR2"] = "value2";
    vars["VAR3"] = "value3";
    env_->addMultiple(vars);

    EXPECT_TRUE(env_->has("VAR1"));
    EXPECT_TRUE(env_->has("VAR2"));
    EXPECT_TRUE(env_->has("VAR3"));
}

TEST_F(EnvTest, HasAll) {
    env_->add("A", "1");
    env_->add("B", "2");
    env_->add("C", "3");

    Vector<String> present = {"A", "B", "C"};
    EXPECT_TRUE(env_->hasAll(present));

    Vector<String> missing = {"A", "B", "MISSING"};
    EXPECT_FALSE(env_->hasAll(missing));
}

TEST_F(EnvTest, HasAny) {
    env_->add("PRESENT", "value");

    Vector<String> withPresent = {"PRESENT", "MISSING1", "MISSING2"};
    EXPECT_TRUE(env_->hasAny(withPresent));

    Vector<String> allMissing = {"MISSING1", "MISSING2"};
    EXPECT_FALSE(env_->hasAny(allMissing));
}

// Type Conversion Tests
TEST_F(EnvTest, GetAsInt) {
    env_->add("INT_VAR", "42");
    EXPECT_EQ(env_->getAs<int>("INT_VAR", 0), 42);
}

TEST_F(EnvTest, GetAsDouble) {
    env_->add("DOUBLE_VAR", "3.14159");
    EXPECT_NEAR(env_->getAs<double>("DOUBLE_VAR", 0.0), 3.14159, 0.00001);
}

TEST_F(EnvTest, GetAsBool) {
    env_->add("BOOL_TRUE", "true");
    env_->add("BOOL_YES", "yes");
    env_->add("BOOL_ONE", "1");
    env_->add("BOOL_FALSE", "false");

    EXPECT_TRUE(env_->getAs<bool>("BOOL_TRUE", false));
    EXPECT_TRUE(env_->getAs<bool>("BOOL_YES", false));
    EXPECT_TRUE(env_->getAs<bool>("BOOL_ONE", false));
    EXPECT_FALSE(env_->getAs<bool>("BOOL_FALSE", true));
}

TEST_F(EnvTest, GetOptional) {
    env_->add("OPT_VAR", "123");
    auto value = env_->getOptional<int>("OPT_VAR");
    EXPECT_TRUE(value.has_value());
    EXPECT_EQ(value.value(), 123);

    auto missing = env_->getOptional<int>("MISSING");
    EXPECT_FALSE(missing.has_value());
}

// System Environment Variable Tests
TEST_F(EnvTest, SetAndGetEnv) {
    EXPECT_TRUE(Env::setEnv("ATOM_TEST_ENV", "test_value"));
    EXPECT_EQ(Env::getEnv("ATOM_TEST_ENV"), "test_value");
    Env::unsetEnv("ATOM_TEST_ENV");
}

TEST_F(EnvTest, GetEnvWithDefault) {
    EXPECT_EQ(Env::getEnv("NONEXISTENT_SYS_VAR", "default"), "default");
}

TEST_F(EnvTest, UnsetEnv) {
    Env::setEnv("TO_UNSET", "value");
    Env::unsetEnv("TO_UNSET");
    EXPECT_EQ(Env::getEnv("TO_UNSET", ""), "");
}

// Directory Operations Tests
TEST_F(EnvTest, GetHomeDir) {
    String homeDir = Env::getHomeDir();
    EXPECT_FALSE(homeDir.empty());
    EXPECT_TRUE(fs::exists(std::string(homeDir.data(), homeDir.size())));
}

TEST_F(EnvTest, GetTempDir) {
    String tempDir = Env::getTempDir();
    EXPECT_FALSE(tempDir.empty());
    EXPECT_TRUE(fs::exists(std::string(tempDir.data(), tempDir.size())));
}

TEST_F(EnvTest, GetConfigDir) {
    String configDir = Env::getConfigDir();
    EXPECT_FALSE(configDir.empty());
}

TEST_F(EnvTest, GetDataDir) {
    String dataDir = Env::getDataDir();
    EXPECT_FALSE(dataDir.empty());
}

// System Information Tests
TEST_F(EnvTest, GetSystemName) {
    String sysName = Env::getSystemName();
    EXPECT_FALSE(sysName.empty());
#ifdef _WIN32
    EXPECT_EQ(sysName, "Windows");
#elif defined(__linux__)
    EXPECT_EQ(sysName, "Linux");
#elif defined(__APPLE__)
    EXPECT_EQ(sysName, "macOS");
#endif
}

TEST_F(EnvTest, GetSystemArch) {
    String arch = Env::getSystemArch();
    EXPECT_FALSE(arch.empty());
}

TEST_F(EnvTest, GetCurrentUser) {
    String user = Env::getCurrentUser();
    EXPECT_FALSE(user.empty());
}

TEST_F(EnvTest, GetHostName) {
    String hostName = Env::getHostName();
    EXPECT_FALSE(hostName.empty());
}

// Path Operations Tests
TEST_F(EnvTest, GetPathEntries) {
    auto paths = Env::getPathEntries();
    EXPECT_GT(paths.size(), 0);
}

TEST_F(EnvTest, AddToPath) {
    String testPath = "/test/path/for/atom";
    EXPECT_TRUE(Env::addToPath(testPath));
    EXPECT_TRUE(Env::isInPath(testPath));
    Env::removeFromPath(testPath);
}

TEST_F(EnvTest, RemoveFromPath) {
    String testPath = "/test/path/to/remove";
    Env::addToPath(testPath);
    EXPECT_TRUE(Env::removeFromPath(testPath));
    EXPECT_FALSE(Env::isInPath(testPath));
}

// Variable Expansion Tests
TEST_F(EnvTest, ExpandVariables) {
    Env::setEnv("EXPAND_TEST", "expanded_value");
#ifdef _WIN32
    String result = Env::expandVariables("%EXPAND_TEST%");
#else
    String result = Env::expandVariables("$EXPAND_TEST");
#endif
    EXPECT_EQ(result, "expanded_value");
    Env::unsetEnv("EXPAND_TEST");
}

// Environment Listing Tests
TEST_F(EnvTest, Environ) {
    auto envVars = Env::Environ();
    EXPECT_GT(envVars.size(), 0);
}

TEST_F(EnvTest, ListVariables) {
    auto vars = Env::listVariables();
    EXPECT_GT(vars.size(), 0);
}

TEST_F(EnvTest, GetVariablesWithPrefix) {
    Env::setEnv("ATOM_PREFIX_TEST1", "value1");
    Env::setEnv("ATOM_PREFIX_TEST2", "value2");

    auto prefixedVars = Env::getVariablesWithPrefix("ATOM_PREFIX_");
    EXPECT_GE(prefixedVars.size(), 2);

    Env::unsetEnv("ATOM_PREFIX_TEST1");
    Env::unsetEnv("ATOM_PREFIX_TEST2");
}

// Scoped Environment Tests
TEST_F(EnvTest, ScopedEnv) {
    String originalValue = Env::getEnv("SCOPED_TEST", "");

    {
        Env::ScopedEnv scoped("SCOPED_TEST", "temporary_value");
        EXPECT_EQ(Env::getEnv("SCOPED_TEST"), "temporary_value");
    }

    // Value should be restored after scope ends
    EXPECT_EQ(Env::getEnv("SCOPED_TEST", ""), originalValue);
}

// File Operations Tests
TEST_F(EnvTest, SaveAndLoadFromFile) {
    fs::path envFile = testDir_ / "test_env.txt";

    HashMap<String, String> varsToSave;
    varsToSave["SAVE_VAR1"] = "value1";
    varsToSave["SAVE_VAR2"] = "value2";

    EXPECT_TRUE(Env::saveToFile(envFile, varsToSave));
    EXPECT_TRUE(fs::exists(envFile));

    EXPECT_TRUE(Env::loadFromFile(envFile, false));
}

// Change Notification Tests
TEST_F(EnvTest, RegisterChangeNotification) {
    bool callbackCalled = false;
    auto id = Env::registerChangeNotification(
        [&callbackCalled](const String&, const String&, const String&) {
            callbackCalled = true;
        });

    EXPECT_GT(id, 0);
    EXPECT_TRUE(Env::unregisterChangeNotification(id));
}

// Environment Merge and Diff Tests
TEST_F(EnvTest, MergeEnvironments) {
    HashMap<String, String> base;
    base["KEY1"] = "value1";
    base["KEY2"] = "value2";

    HashMap<String, String> overlay;
    overlay["KEY2"] = "new_value2";
    overlay["KEY3"] = "value3";

    auto merged = Env::mergeEnvironments(base, overlay, true);
    EXPECT_EQ(merged["KEY1"], "value1");
    EXPECT_EQ(merged["KEY2"], "new_value2");
    EXPECT_EQ(merged["KEY3"], "value3");
}

TEST_F(EnvTest, DiffEnvironments) {
    HashMap<String, String> env1;
    env1["KEY1"] = "value1";
    env1["KEY2"] = "value2";

    HashMap<String, String> env2;
    env2["KEY2"] = "modified_value2";
    env2["KEY3"] = "value3";

    auto [added, removed, modified] = Env::diffEnvironments(env1, env2);

    EXPECT_EQ(added.count("KEY3"), 1);
    EXPECT_EQ(removed.count("KEY1"), 1);
    EXPECT_EQ(modified.count("KEY2"), 1);
}

// Constructor Tests
TEST_F(EnvTest, ConstructorWithArgs) {
    char* argv[] = {const_cast<char*>("program"), const_cast<char*>("--arg1"),
                    const_cast<char*>("value1")};
    int argc = 3;

    Env envWithArgs(argc, argv);
    EXPECT_EQ(envWithArgs.getProgramName(), "program");
}

TEST_F(EnvTest, CreateShared) {
    char* argv[] = {const_cast<char*>("test_program")};
    auto sharedEnv = Env::createShared(1, argv);
    EXPECT_NE(sharedEnv, nullptr);
}

}  // namespace atom::utils::test
