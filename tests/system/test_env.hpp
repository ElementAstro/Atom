#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "atom/system/env.hpp"

using namespace atom::utils;

class EnvTest : public ::testing::Test {
protected:
    Env env;  // 创建实例以调用非静态方法

    void SetUp() override {
        // 保存原始环境变量以便稍后恢复
        originalEnvVars = env.Environ();

        // 创建一个临时文件用于测试文件操作
        tempFilePath = std::filesystem::temp_directory_path() / "env_test.txt";

        // 设置一些测试环境变量
        env.setEnv("ATOM_TEST_VAR1", "test_value1");
        env.setEnv("ATOM_TEST_VAR2", "42");
        env.setEnv("ATOM_TEST_VAR3", "3.14");
        env.setEnv("ATOM_TEST_VAR4", "true");
    }

    void TearDown() override {
        // 清理测试环境变量
        env.unsetEnv("ATOM_TEST_VAR1");
        env.unsetEnv("ATOM_TEST_VAR2");
        env.unsetEnv("ATOM_TEST_VAR3");
        env.unsetEnv("ATOM_TEST_VAR4");
        env.unsetEnv("ATOM_TEST_NEW_VAR");

        // 删除临时文件（如果存在）
        if (std::filesystem::exists(tempFilePath)) {
            std::filesystem::remove(tempFilePath);
        }
    }

    std::unordered_map<std::string, std::string> originalEnvVars;
    std::filesystem::path tempFilePath;

    // 帮助函数，创建带有环境变量的测试文件
    void createTestFile(
        const std::unordered_map<std::string, std::string>& vars) {
        std::ofstream file(tempFilePath);
        ASSERT_TRUE(file.is_open());

        for (const auto& [key, value] : vars) {
            file << key << "=" << value << std::endl;
        }

        file.close();
    }
};

// 基本构造函数测试
TEST_F(EnvTest, DefaultConstructor) {
    Env localEnv;
    // 应该创建一个Env对象，而不会崩溃
    SUCCEED();
}

TEST_F(EnvTest, CommandLineConstructor) {
    char* testArgv[] = {(char*)"program_name", (char*)"--arg1=value1",
                        (char*)"--arg2=value2", nullptr};

    Env localEnv(3, testArgv);
    auto args = localEnv.getAllArgs();

    EXPECT_EQ(args["arg1"], "value1");
    EXPECT_EQ(args["arg2"], "value2");
}

TEST_F(EnvTest, CreateShared) {
    char* testArgv[] = {(char*)"program_name", (char*)"--arg1=value1", nullptr};

    auto sharedEnv = Env::createShared(2, testArgv);
    ASSERT_NE(sharedEnv, nullptr);

    auto args = sharedEnv->getAllArgs();
    EXPECT_EQ(args["arg1"], "value1");
}

// 环境变量管理测试
TEST_F(EnvTest, Environ) {
    auto envVars = env.Environ();

    // 检查我们的测试变量是否存在
    EXPECT_EQ(envVars["ATOM_TEST_VAR1"], "test_value1");
    EXPECT_EQ(envVars["ATOM_TEST_VAR2"], "42");
}

TEST_F(EnvTest, AddAndGet) {
    Env localEnv;

    localEnv.add("test_key1", "test_value1");
    localEnv.add("test_key2", "42");

    EXPECT_EQ(localEnv.get("test_key1"), "test_value1");
    EXPECT_EQ(localEnv.get("test_key2"), "42");
    EXPECT_EQ(localEnv.get("nonexistent_key", "default"), "default");
}

TEST_F(EnvTest, AddMultiple) {
    Env localEnv;

    std::unordered_map<std::string, std::string> vars = {
        {"key1", "value1"}, {"key2", "value2"}, {"key3", "value3"}};

    localEnv.addMultiple(vars);

    EXPECT_EQ(localEnv.get("key1"), "value1");
    EXPECT_EQ(localEnv.get("key2"), "value2");
    EXPECT_EQ(localEnv.get("key3"), "value3");
}

TEST_F(EnvTest, Has) {
    Env localEnv;

    localEnv.add("test_key", "test_value");

    EXPECT_TRUE(localEnv.has("test_key"));
    EXPECT_FALSE(localEnv.has("nonexistent_key"));
}

TEST_F(EnvTest, HasAll) {
    Env localEnv;

    localEnv.add("key1", "value1");
    localEnv.add("key2", "value2");

    EXPECT_TRUE(localEnv.hasAll({"key1", "key2"}));
    EXPECT_FALSE(localEnv.hasAll({"key1", "key2", "key3"}));
}

TEST_F(EnvTest, HasAny) {
    Env localEnv;

    localEnv.add("key1", "value1");

    EXPECT_TRUE(localEnv.hasAny({"key1", "nonexistent"}));
    EXPECT_FALSE(localEnv.hasAny({"nonexistent1", "nonexistent2"}));
}

TEST_F(EnvTest, Del) {
    Env localEnv;

    localEnv.add("key1", "value1");
    localEnv.add("key2", "value2");

    EXPECT_TRUE(localEnv.has("key1"));
    localEnv.del("key1");
    EXPECT_FALSE(localEnv.has("key1"));
    EXPECT_TRUE(localEnv.has("key2"));
}

TEST_F(EnvTest, DelMultiple) {
    Env localEnv;

    localEnv.add("key1", "value1");
    localEnv.add("key2", "value2");
    localEnv.add("key3", "value3");

    localEnv.delMultiple({"key1", "key3"});

    EXPECT_FALSE(localEnv.has("key1"));
    EXPECT_TRUE(localEnv.has("key2"));
    EXPECT_FALSE(localEnv.has("key3"));
}

// 类型转换测试
TEST_F(EnvTest, GetAs) {
    Env localEnv;

    localEnv.add("int_key", "42");
    localEnv.add("double_key", "3.14");
    localEnv.add("bool_key", "true");
    localEnv.add("invalid_int", "not_a_number");

    EXPECT_EQ(localEnv.getAs<int>("int_key"), 42);
    EXPECT_EQ(localEnv.getAs<double>("double_key"), 3.14);
    EXPECT_TRUE(localEnv.getAs<bool>("bool_key"));
    EXPECT_EQ(localEnv.getAs<int>("invalid_int", 100),
              100);  // 应返回默认值
    EXPECT_EQ(localEnv.getAs<int>("nonexistent", 100),
              100);  // 应返回默认值
}

TEST_F(EnvTest, GetOptional) {
    Env localEnv;

    localEnv.add("int_key", "42");
    localEnv.add("invalid_int", "not_a_number");

    auto result1 = localEnv.getOptional<int>("int_key");
    auto result2 = localEnv.getOptional<int>("nonexistent");
    auto result3 = localEnv.getOptional<int>("invalid_int");

    EXPECT_TRUE(result1.has_value());
    EXPECT_EQ(result1.value(), 42);
    EXPECT_FALSE(result2.has_value());
    EXPECT_FALSE(result3.has_value());
}

// 环境变量系统测试
TEST_F(EnvTest, SetAndGetEnv) {
    EXPECT_TRUE(env.setEnv("ATOM_TEST_NEW_VAR", "new_value"));

    EXPECT_EQ(env.getEnv("ATOM_TEST_NEW_VAR"), "new_value");
    EXPECT_EQ(env.getEnv("NONEXISTENT_ENV_VAR", "default"), "default");
}

TEST_F(EnvTest, GetEnvAs) {
    EXPECT_EQ(env.getEnvAs<int>("ATOM_TEST_VAR2"), 42);
    EXPECT_DOUBLE_EQ(env.getEnvAs<double>("ATOM_TEST_VAR3"), 3.14);
    EXPECT_TRUE(env.getEnvAs<bool>("ATOM_TEST_VAR4"));
    EXPECT_EQ(env.getEnvAs<int>("NONEXISTENT_ENV_VAR", 100), 100);
}

TEST_F(EnvTest, SetEnvMultiple) {
    std::unordered_map<std::string, std::string> vars = {
        {"ATOM_TEST_MULTI1", "value1"}, {"ATOM_TEST_MULTI2", "value2"}};

    EXPECT_TRUE(env.setEnvMultiple(vars));

    EXPECT_EQ(env.getEnv("ATOM_TEST_MULTI1"), "value1");
    EXPECT_EQ(env.getEnv("ATOM_TEST_MULTI2"), "value2");

    // 清理
    env.unsetEnv("ATOM_TEST_MULTI1");
    env.unsetEnv("ATOM_TEST_MULTI2");
}

TEST_F(EnvTest, UnsetEnv) {
    // 首先确保变量存在
    EXPECT_EQ(Env().getEnv("ATOM_TEST_VAR1"), "test_value1");

    // 取消设置
    env.unsetEnv("ATOM_TEST_VAR1");

    // 检查它是否已消失
    EXPECT_EQ(Env().getEnv("ATOM_TEST_VAR1", "deleted"), "deleted");

    // 为其他测试恢复
    env.setEnv("ATOM_TEST_VAR1", "test_value1");
}

TEST_F(EnvTest, UnsetEnvMultiple) {
    // 首先确保变量存在
    EXPECT_EQ(Env().getEnv("ATOM_TEST_VAR1"), "test_value1");
    EXPECT_EQ(Env().getEnv("ATOM_TEST_VAR2"), "42");

    // 取消设置它们
    env.unsetEnvMultiple({"ATOM_TEST_VAR1", "ATOM_TEST_VAR2"});

    // 检查它们是否已消失
    EXPECT_EQ(Env().getEnv("ATOM_TEST_VAR1", "deleted"), "deleted");
    EXPECT_EQ(Env().getEnv("ATOM_TEST_VAR2", "deleted"), "deleted");

    // 为其他测试恢复
    env.setEnv("ATOM_TEST_VAR1", "test_value1");
    env.setEnv("ATOM_TEST_VAR2", "42");
}

// 环境变量列表和过滤测试
TEST_F(EnvTest, ListVariables) {
    auto vars = env.listVariables();

    // 检查我们的测试变量是否在列表中
    EXPECT_TRUE(std::find(vars.begin(), vars.end(), "ATOM_TEST_VAR1") !=
                vars.end());
    EXPECT_TRUE(std::find(vars.begin(), vars.end(), "ATOM_TEST_VAR2") !=
                vars.end());
}

TEST_F(EnvTest, FilterVariables) {
    auto filtered =
        env.filterVariables([](const std::string& key, const std::string&) {
            return key.find("ATOM_TEST_") == 0;
        });

    EXPECT_TRUE(filtered.find("ATOM_TEST_VAR1") != filtered.end());
    EXPECT_TRUE(filtered.find("ATOM_TEST_VAR2") != filtered.end());
    EXPECT_EQ(filtered["ATOM_TEST_VAR1"], "test_value1");
}

TEST_F(EnvTest, GetVariablesWithPrefix) {
    auto prefixed = env.getVariablesWithPrefix("ATOM_TEST_");

    EXPECT_TRUE(prefixed.find("ATOM_TEST_VAR1") != prefixed.end());
    EXPECT_TRUE(prefixed.find("ATOM_TEST_VAR2") != prefixed.end());
    EXPECT_EQ(prefixed["ATOM_TEST_VAR1"], "test_value1");
    EXPECT_EQ(prefixed.size(), 4);  // 所有 ATOM_TEST_ 变量
}

// 文件操作测试
TEST_F(EnvTest, SaveToFile) {
    std::unordered_map<std::string, std::string> testVars = {
        {"TEST_KEY1", "value1"}, {"TEST_KEY2", "value2"}};

    EXPECT_TRUE(env.saveToFile(tempFilePath, testVars));

    // 验证文件内容
    std::ifstream file(tempFilePath);
    ASSERT_TRUE(file.is_open());

    std::string line;
    std::unordered_map<std::string, std::string> readVars;

    while (std::getline(file, line)) {
        size_t pos = line.find('=');
        if (pos != std::string::npos) {
            std::string key = line.substr(0, pos);
            std::string value = line.substr(pos + 1);
            readVars[key] = value;
        }
    }

    EXPECT_EQ(readVars["TEST_KEY1"], "value1");
    EXPECT_EQ(readVars["TEST_KEY2"], "value2");
}

TEST_F(EnvTest, LoadFromFile) {
    // 创建测试文件
    std::unordered_map<std::string, std::string> testVars = {
        {"FILE_TEST_VAR1", "file_value1"}, {"FILE_TEST_VAR2", "file_value2"}};
    createTestFile(testVars);

    // 从文件加载
    EXPECT_TRUE(env.loadFromFile(tempFilePath));

    // 检查变量是否已加载
    EXPECT_EQ(env.getEnv("FILE_TEST_VAR1"), "file_value1");
    EXPECT_EQ(env.getEnv("FILE_TEST_VAR2"), "file_value2");

    // 清理
    env.unsetEnv("FILE_TEST_VAR1");
    env.unsetEnv("FILE_TEST_VAR2");
}

TEST_F(EnvTest, LoadFromFileWithOverwrite) {
    // 设置初始值
    env.setEnv("OVERWRITE_TEST_VAR", "original_value");

    // 创建带有新值的测试文件
    std::unordered_map<std::string, std::string> testVars = {
        {"OVERWRITE_TEST_VAR", "new_value"}};
    createTestFile(testVars);

    // 测试不覆盖
    EXPECT_TRUE(env.loadFromFile(tempFilePath, false));
    EXPECT_EQ(env.getEnv("OVERWRITE_TEST_VAR"), "original_value");

    // 测试覆盖
    EXPECT_TRUE(env.loadFromFile(tempFilePath, true));
    EXPECT_EQ(env.getEnv("OVERWRITE_TEST_VAR"), "new_value");

    // 清理
    env.unsetEnv("OVERWRITE_TEST_VAR");
}

// 程序信息测试
TEST_F(EnvTest, GetExecutablePath) {
    std::string exePath = env.getExecutablePath();

    // 这个测试依赖于平台，所以只检查它是否非空
    EXPECT_FALSE(exePath.empty());
}

TEST_F(EnvTest, GetWorkingDirectory) {
    std::string workDir = env.getWorkingDirectory();

    // 检查工作目录是否存在
    EXPECT_TRUE(std::filesystem::exists(workDir));
}

TEST_F(EnvTest, GetProgramName) {
    char* testArgv[] = {(char*)"/path/to/program_name", nullptr};

    Env localEnv(1, testArgv);
    EXPECT_EQ(localEnv.getProgramName(), "program_name");
}

// 类型转换边缘情况
TEST_F(EnvTest, ConvertFromStringEdgeCases) {
    Env localEnv;

    localEnv.add("max_int", std::to_string(std::numeric_limits<int>::max()));
    localEnv.add("min_int", std::to_string(std::numeric_limits<int>::min()));
    localEnv.add("max_double",
                 std::to_string(std::numeric_limits<double>::max()));
    localEnv.add("true_values", "true");
    localEnv.add("yes_value", "yes");
    localEnv.add("on_value", "on");
    localEnv.add("one_value", "1");
    localEnv.add("false_value", "false");

    EXPECT_EQ(localEnv.getAs<int>("max_int"), std::numeric_limits<int>::max());
    EXPECT_EQ(localEnv.getAs<int>("min_int"), std::numeric_limits<int>::min());
    EXPECT_EQ(localEnv.getAs<double>("max_double"),
              std::numeric_limits<double>::max());

    EXPECT_TRUE(localEnv.getAs<bool>("true_values"));
    EXPECT_TRUE(localEnv.getAs<bool>("yes_value"));
    EXPECT_TRUE(localEnv.getAs<bool>("on_value"));
    EXPECT_TRUE(localEnv.getAs<bool>("one_value"));
    EXPECT_FALSE(localEnv.getAs<bool>("false_value"));
}

// 命令行参数测试
TEST_F(EnvTest, CommandLineArgumentParsing) {
    char* testArgv[] = {(char*)"program_name",
                        (char*)"--flag1",
                        (char*)"--key1=value1",
                        (char*)"--key2=value2",
                        (char*)"positional1",
                        (char*)"positional2",
                        nullptr};

    Env localEnv(6, testArgv);
    auto args = localEnv.getAllArgs();

    EXPECT_EQ(args["key1"], "value1");
    EXPECT_EQ(args["key2"], "value2");
    EXPECT_TRUE(args.find("flag1") != args.end());
    EXPECT_EQ(args["0"], "positional1");
    EXPECT_EQ(args["1"], "positional2");
}

// 多线程测试
TEST_F(EnvTest, ThreadSafety) {
    const int numThreads = 10;
    std::vector<std::thread> threads;
    Env threadEnv;  // 使用实例而非静态方法

    for (int i = 0; i < numThreads; ++i) {
        threads.emplace_back([i, &threadEnv]() {
            // 每个线程设置并获取不同的环境变量
            std::string varName = "THREAD_TEST_VAR_" + std::to_string(i);
            std::string varValue = "value_" + std::to_string(i);

            threadEnv.setEnv(varName, varValue);
            std::this_thread::sleep_for(std::chrono::milliseconds(10));

            EXPECT_EQ(threadEnv.getEnv(varName), varValue);

            threadEnv.unsetEnv(varName);
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }
}

#if ATOM_ENABLE_DEBUG
// 调试方法测试
TEST_F(EnvTest, PrintAllVariables) {
    // 这个测试只是验证方法运行时不会崩溃
    testing::internal::CaptureStdout();
    env.printAllVariables();
    std::string output = testing::internal::GetCapturedStdout();

    EXPECT_FALSE(output.empty());
    EXPECT_TRUE(output.find("ATOM_TEST_VAR1") != std::string::npos);
}

TEST_F(EnvTest, PrintAllArgs) {
    char* testArgv[] = {(char*)"program_name", (char*)"--key1=value1", nullptr};

    Env localEnv(2, testArgv);

    testing::internal::CaptureStdout();
    localEnv.printAllArgs();
    std::string output = testing::internal::GetCapturedStdout();

    EXPECT_FALSE(output.empty());
    EXPECT_TRUE(output.find("key1") != std::string::npos);
}
#endif
