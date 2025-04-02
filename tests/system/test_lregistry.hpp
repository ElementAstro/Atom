#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <string>
#include <thread>
#include <vector>

#include "atom/system/lregistry.hpp"

using namespace atom::system;

class RegistryTest : public ::testing::Test {
protected:
    Registry registry;
    std::string testKeyPath = "TestRoot/TestKey";
    std::string testValueName = "TestValue";
    std::string testValueData = "TestData";
    std::string testBackupPath;
    std::string testExportPath;

    void SetUp() override {
        // Initialize registry with a test file path
        auto tempDir = std::filesystem::temp_directory_path();
        std::string testFilePath = (tempDir / "test_registry.dat").string();
        testBackupPath = (tempDir / "test_registry_backup.dat").string();
        testExportPath = (tempDir / "test_registry_export.dat").string();
        
        // Initialize registry
        ASSERT_EQ(registry.initialize(testFilePath), RegistryResult::SUCCESS);
        
        // Create a test key for most tests
        ASSERT_EQ(registry.createKey(testKeyPath), RegistryResult::SUCCESS);
    }

    void TearDown() override {
        // Clean up test files
        auto tempDir = std::filesystem::temp_directory_path();
        std::string testFilePath = (tempDir / "test_registry.dat").string();
        
        if (std::filesystem::exists(testFilePath)) {
            std::filesystem::remove(testFilePath);
        }
        
        if (std::filesystem::exists(testBackupPath)) {
            std::filesystem::remove(testBackupPath);
        }
        
        if (std::filesystem::exists(testExportPath)) {
            std::filesystem::remove(testExportPath);
        }
    }
};

// 基本键操作测试
TEST_F(RegistryTest, CreateKey) {
    // 测试创建新键
    EXPECT_EQ(registry.createKey("NewKey"), RegistryResult::SUCCESS);
    EXPECT_TRUE(registry.keyExists("NewKey"));
    
    // 测试创建嵌套键
    EXPECT_EQ(registry.createKey("Parent/Child/GrandChild"), RegistryResult::SUCCESS);
    EXPECT_TRUE(registry.keyExists("Parent/Child/GrandChild"));
    
    // 测试创建已存在的键
    EXPECT_EQ(registry.createKey(testKeyPath), RegistryResult::ALREADY_EXISTS);
}

TEST_F(RegistryTest, DeleteKey) {
    // 创建要删除的键
    ASSERT_EQ(registry.createKey("KeyToDelete"), RegistryResult::SUCCESS);
    EXPECT_TRUE(registry.keyExists("KeyToDelete"));
    
    // 测试删除键
    EXPECT_EQ(registry.deleteKey("KeyToDelete"), RegistryResult::SUCCESS);
    EXPECT_FALSE(registry.keyExists("KeyToDelete"));
    
    // 测试删除不存在的键
    EXPECT_EQ(registry.deleteKey("NonExistentKey"), RegistryResult::KEY_NOT_FOUND);
    
    // 测试删除有子键的键
    ASSERT_EQ(registry.createKey("Parent/Child"), RegistryResult::SUCCESS);
    EXPECT_EQ(registry.deleteKey("Parent"), RegistryResult::SUCCESS);
    EXPECT_FALSE(registry.keyExists("Parent"));
    EXPECT_FALSE(registry.keyExists("Parent/Child"));
}

TEST_F(RegistryTest, KeyExists) {
    // 已存在的键
    EXPECT_TRUE(registry.keyExists(testKeyPath));
    
    // 不存在的键
    EXPECT_FALSE(registry.keyExists("NonExistentKey"));
    
    // 删除后的键
    registry.deleteKey(testKeyPath);
    EXPECT_FALSE(registry.keyExists(testKeyPath));
}

TEST_F(RegistryTest, GetAllKeys) {
    // 创建几个测试键
    registry.createKey("Key1");
    registry.createKey("Key2");
    registry.createKey("Key3/SubKey");
    
    // 获取所有键
    auto keys = registry.getAllKeys();
    
    // 验证结果包含所有创建的键
    EXPECT_THAT(keys, testing::Contains(testKeyPath));
    EXPECT_THAT(keys, testing::Contains("Key1"));
    EXPECT_THAT(keys, testing::Contains("Key2"));
    EXPECT_THAT(keys, testing::Contains("Key3/SubKey"));
    
    // 删除键后重新验证
    registry.deleteKey("Key1");
    keys = registry.getAllKeys();
    EXPECT_THAT(keys, testing::Not(testing::Contains("Key1")));
}

// 值操作测试
TEST_F(RegistryTest, SetAndGetValue) {
    // 设置一个普通值
    EXPECT_EQ(registry.setValue(testKeyPath, testValueName, testValueData), 
              RegistryResult::SUCCESS);
    
    // 获取该值
    auto value = registry.getValue(testKeyPath, testValueName);
    EXPECT_TRUE(value.has_value());
    EXPECT_EQ(value.value(), testValueData);
    
    // 设置不存在键的值
    EXPECT_EQ(registry.setValue("NonExistentKey", testValueName, testValueData), 
              RegistryResult::KEY_NOT_FOUND);
    
    // 用空值覆盖现有值
    EXPECT_EQ(registry.setValue(testKeyPath, testValueName, ""), 
              RegistryResult::SUCCESS);
    value = registry.getValue(testKeyPath, testValueName);
    EXPECT_TRUE(value.has_value());
    EXPECT_EQ(value.value(), "");
}

TEST_F(RegistryTest, SetAndGetTypedValue) {
    // 设置一个带类型的值
    EXPECT_EQ(registry.setTypedValue(testKeyPath, testValueName, testValueData, "string"), 
              RegistryResult::SUCCESS);
    
    // 获取该值及其类型
    std::string type;
    auto value = registry.getTypedValue(testKeyPath, testValueName, type);
    EXPECT_TRUE(value.has_value());
    EXPECT_EQ(value.value(), testValueData);
    EXPECT_EQ(type, "string");
    
    // 设置不同类型的值
    EXPECT_EQ(registry.setTypedValue(testKeyPath, "IntValue", "42", "int"), 
              RegistryResult::SUCCESS);
    value = registry.getTypedValue(testKeyPath, "IntValue", type);
    EXPECT_TRUE(value.has_value());
    EXPECT_EQ(value.value(), "42");
    EXPECT_EQ(type, "int");
}

TEST_F(RegistryTest, DeleteValue) {
    // 设置一个值然后删除它
    ASSERT_EQ(registry.setValue(testKeyPath, testValueName, testValueData), 
              RegistryResult::SUCCESS);
    EXPECT_TRUE(registry.valueExists(testKeyPath, testValueName));
    
    EXPECT_EQ(registry.deleteValue(testKeyPath, testValueName), 
              RegistryResult::SUCCESS);
    EXPECT_FALSE(registry.valueExists(testKeyPath, testValueName));
    
    // 删除不存在的值
    EXPECT_EQ(registry.deleteValue(testKeyPath, "NonExistentValue"), 
              RegistryResult::VALUE_NOT_FOUND);
    
    // 从不存在的键中删除值
    EXPECT_EQ(registry.deleteValue("NonExistentKey", testValueName), 
              RegistryResult::KEY_NOT_FOUND);
}

TEST_F(RegistryTest, ValueExists) {
    // 检查不存在的值
    EXPECT_FALSE(registry.valueExists(testKeyPath, testValueName));
    
    // 设置值
    registry.setValue(testKeyPath, testValueName, testValueData);
    EXPECT_TRUE(registry.valueExists(testKeyPath, testValueName));
    
    // 删除值后再检查
    registry.deleteValue(testKeyPath, testValueName);
    EXPECT_FALSE(registry.valueExists(testKeyPath, testValueName));
    
    // 检查不存在键中的值
    EXPECT_FALSE(registry.valueExists("NonExistentKey", testValueName));
}

TEST_F(RegistryTest, GetValueNames) {
    // 设置多个值
    registry.setValue(testKeyPath, "Value1", "Data1");
    registry.setValue(testKeyPath, "Value2", "Data2");
    registry.setValue(testKeyPath, "Value3", "Data3");
    
    // 获取值名
    auto valueNames = registry.getValueNames(testKeyPath);
    
    // 验证结果
    EXPECT_THAT(valueNames, testing::Contains("Value1"));
    EXPECT_THAT(valueNames, testing::Contains("Value2"));
    EXPECT_THAT(valueNames, testing::Contains("Value3"));
    EXPECT_EQ(valueNames.size(), 3);
    
    // 删除一个值后再检查
    registry.deleteValue(testKeyPath, "Value2");
    valueNames = registry.getValueNames(testKeyPath);
    EXPECT_THAT(valueNames, testing::Not(testing::Contains("Value2")));
    EXPECT_EQ(valueNames.size(), 2);
    
    // 检查不存在键的值名
    valueNames = registry.getValueNames("NonExistentKey");
    EXPECT_TRUE(valueNames.empty());
}

TEST_F(RegistryTest, GetValueInfo) {
    // 设置一个带类型的值
    std::string testType = "string";
    ASSERT_EQ(registry.setTypedValue(testKeyPath, testValueName, testValueData, testType), 
              RegistryResult::SUCCESS);
    
    // 获取值信息
    auto valueInfo = registry.getValueInfo(testKeyPath, testValueName);
    EXPECT_TRUE(valueInfo.has_value());
    EXPECT_EQ(valueInfo->name, testValueName);
    EXPECT_EQ(valueInfo->type, testType);
    EXPECT_EQ(valueInfo->size, testValueData.size());
    // 我们不能严格测试lastModified的具体值，但可以确保它是近期时间
    auto now = std::time(nullptr);
    EXPECT_LE(std::abs(std::difftime(valueInfo->lastModified, now)), 60); // 60秒内
    
    // 获取不存在值的信息
    valueInfo = registry.getValueInfo(testKeyPath, "NonExistentValue");
    EXPECT_FALSE(valueInfo.has_value());
    
    // 从不存在的键获取值信息
    valueInfo = registry.getValueInfo("NonExistentKey", testValueName);
    EXPECT_FALSE(valueInfo.has_value());
}

// 文件操作测试
TEST_F(RegistryTest, LoadRegistryFromFile) {
    // 首先存储一些数据
    ASSERT_EQ(registry.setValue(testKeyPath, testValueName, testValueData), 
              RegistryResult::SUCCESS);
    
    // 获取测试文件路径
    auto tempDir = std::filesystem::temp_directory_path();
    std::string testFilePath = (tempDir / "test_registry.dat").string();
    
    // 创建一个新的注册表实例
    Registry newRegistry;
    
    // 加载文件
    EXPECT_EQ(newRegistry.loadRegistryFromFile(testFilePath), 
              RegistryResult::SUCCESS);
    
    // 验证数据已正确加载
    EXPECT_TRUE(newRegistry.keyExists(testKeyPath));
    auto value = newRegistry.getValue(testKeyPath, testValueName);
    EXPECT_TRUE(value.has_value());
    EXPECT_EQ(value.value(), testValueData);
    
    // 尝试加载不存在的文件
    EXPECT_EQ(newRegistry.loadRegistryFromFile("non_existent_file.dat"), 
              RegistryResult::FILE_ERROR);
}

TEST_F(RegistryTest, BackupAndRestoreRegistry) {
    // 设置测试数据
    ASSERT_EQ(registry.setValue(testKeyPath, testValueName, testValueData), 
              RegistryResult::SUCCESS);
    
    // 备份注册表
    EXPECT_EQ(registry.backupRegistryData(testBackupPath), 
              RegistryResult::SUCCESS);
    EXPECT_TRUE(std::filesystem::exists(testBackupPath));
    
    // 修改注册表数据
    ASSERT_EQ(registry.setValue(testKeyPath, testValueName, "ModifiedData"), 
              RegistryResult::SUCCESS);
    auto value = registry.getValue(testKeyPath, testValueName);
    EXPECT_EQ(value.value(), "ModifiedData");
    
    // 从备份恢复
    EXPECT_EQ(registry.restoreRegistryData(testBackupPath), 
              RegistryResult::SUCCESS);
    
    // 验证数据已恢复
    value = registry.getValue(testKeyPath, testValueName);
    EXPECT_TRUE(value.has_value());
    EXPECT_EQ(value.value(), testValueData);
    
    // 尝试从不存在的文件恢复
    EXPECT_EQ(registry.restoreRegistryData("non_existent_backup.dat"), 
              RegistryResult::FILE_ERROR);
}

TEST_F(RegistryTest, ExportAndImportRegistry) {
    // 设置测试数据
    ASSERT_EQ(registry.setValue(testKeyPath, testValueName, testValueData), 
              RegistryResult::SUCCESS);
    ASSERT_EQ(registry.setValue(testKeyPath, "AnotherValue", "AnotherData"), 
              RegistryResult::SUCCESS);
    
    // 导出为不同格式
    for (auto format : {RegistryFormat::TEXT, RegistryFormat::JSON, RegistryFormat::XML}) {
        // 导出注册表
        EXPECT_EQ(registry.exportRegistry(testExportPath, format), 
                  RegistryResult::SUCCESS);
        EXPECT_TRUE(std::filesystem::exists(testExportPath));
        
        // 创建新的注册表实例
        Registry importedRegistry;
        ASSERT_EQ(importedRegistry.initialize(testExportPath + ".import"), 
                  RegistryResult::SUCCESS);
        
        // 导入注册表
        EXPECT_EQ(importedRegistry.importRegistry(testExportPath, format), 
                  RegistryResult::SUCCESS);
        
        // 验证数据已正确导入
        EXPECT_TRUE(importedRegistry.keyExists(testKeyPath));
        auto value = importedRegistry.getValue(testKeyPath, testValueName);
        EXPECT_TRUE(value.has_value());
        EXPECT_EQ(value.value(), testValueData);
        
        value = importedRegistry.getValue(testKeyPath, "AnotherValue");
        EXPECT_TRUE(value.has_value());
        EXPECT_EQ(value.value(), "AnotherData");
        
        // 删除测试文件
        std::filesystem::remove(testExportPath);
        std::filesystem::remove(testExportPath + ".import");
    }
    
    // 测试导入同时合并现有数据
    ASSERT_EQ(registry.exportRegistry(testExportPath, RegistryFormat::JSON), 
              RegistryResult::SUCCESS);
    
    // 创建一个带有部分不同数据的新注册表
    Registry mergeRegistry;
    ASSERT_EQ(mergeRegistry.initialize(testExportPath + ".merge"), 
              RegistryResult::SUCCESS);
    ASSERT_EQ(mergeRegistry.createKey("UniqueKey"), 
              RegistryResult::SUCCESS);
    ASSERT_EQ(mergeRegistry.setValue("UniqueKey", "UniqueValue", "UniqueData"), 
              RegistryResult::SUCCESS);
    
    // 导入并合并
    EXPECT_EQ(mergeRegistry.importRegistry(testExportPath, RegistryFormat::JSON, true), 
              RegistryResult::SUCCESS);
    
    // 验证原数据和导入的数据都存在
    EXPECT_TRUE(mergeRegistry.keyExists("UniqueKey"));
    EXPECT_TRUE(mergeRegistry.keyExists(testKeyPath));
    
    auto value = mergeRegistry.getValue("UniqueKey", "UniqueValue");
    EXPECT_TRUE(value.has_value());
    EXPECT_EQ(value.value(), "UniqueData");
    
    value = mergeRegistry.getValue(testKeyPath, testValueName);
    EXPECT_TRUE(value.has_value());
    EXPECT_EQ(value.value(), testValueData);
    
    // 删除测试文件
    std::filesystem::remove(testExportPath);
    std::filesystem::remove(testExportPath + ".merge");
}

// 搜索功能测试
TEST_F(RegistryTest, SearchKeys) {
    // 创建一组用于搜索的键
    registry.createKey("SearchTest/Key1");
    registry.createKey("SearchTest/Key2");
    registry.createKey("SearchTest/SubDir/Key3");
    registry.createKey("DifferentPath/Key4");
    
    // 使用模式搜索键
    auto results = registry.searchKeys("SearchTest/*");
    EXPECT_EQ(results.size(), 3);
    EXPECT_THAT(results, testing::Contains("SearchTest/Key1"));
    EXPECT_THAT(results, testing::Contains("SearchTest/Key2"));
    EXPECT_THAT(results, testing::Contains("SearchTest/SubDir/Key3"));
    
    // 使用更具体的模式
    results = registry.searchKeys("SearchTest/Key*");
    EXPECT_EQ(results.size(), 2);
    EXPECT_THAT(results, testing::Contains("SearchTest/Key1"));
    EXPECT_THAT(results, testing::Contains("SearchTest/Key2"));
    
    // 使用不匹配任何内容的模式
    results = registry.searchKeys("NonExistent*");
    EXPECT_TRUE(results.empty());
}

TEST_F(RegistryTest, SearchValues) {
    // 设置一组用于搜索的值
    registry.setValue("SearchTest/Key1", "Value1", "SearchableContent");
    registry.setValue("SearchTest/Key2", "Value2", "DifferentContent");
    registry.setValue("SearchTest/Key3", "Value3", "SearchableContentWithMore");
    registry.setValue("DifferentPath/Key4", "Value4", "SearchableContent");
    
    // 搜索特定内容的值
    auto results = registry.searchValues("Searchable");
    EXPECT_EQ(results.size(), 3);
    
    // 验证结果包含正确的键值对
    bool foundKey1 = false, foundKey3 = false, foundKey4 = false;
    for (const auto& [key, value] : results) {
        if (key == "SearchTest/Key1" && value == "SearchableContent") {
            foundKey1 = true;
        } else if (key == "SearchTest/Key3" && value == "SearchableContentWithMore") {
            foundKey3 = true;
        } else if (key == "DifferentPath/Key4" && value == "SearchableContent") {
            foundKey4 = true;
        }
    }
    EXPECT_TRUE(foundKey1);
    EXPECT_TRUE(foundKey3);
    EXPECT_TRUE(foundKey4);
    
    // 使用更具体的模式
    results = registry.searchValues("SearchableContent$");
    EXPECT_EQ(results.size(), 2);
    
    // 使用不匹配任何内容的模式
    results = registry.searchValues("NonExistentPattern");
    EXPECT_TRUE(results.empty());
}

// 事件回调测试
TEST_F(RegistryTest, EventCallbacks) {
    bool callbackFired = false;
    std::string callbackKey, callbackValue;
    
    // 注册回调
    size_t callbackId = registry.registerEventCallback(
        [&callbackFired, &callbackKey, &callbackValue](const std::string& key, const std::string& value) {
            callbackFired = true;
            callbackKey = key;
            callbackValue = value;
        });
    
    // 触发回调
    registry.setValue(testKeyPath, testValueName, testValueData);
    
    // 验证回调被调用
    EXPECT_TRUE(callbackFired);
    EXPECT_EQ(callbackKey, testKeyPath);
    EXPECT_EQ(callbackValue, testValueName);
    
    // 重置标志
    callbackFired = false;
    
    // 取消注册回调
    EXPECT_TRUE(registry.unregisterEventCallback(callbackId));
    
    // 确认回调不再被触发
    registry.setValue(testKeyPath, "NewValue", "NewData");
    EXPECT_FALSE(callbackFired);
    
    // 尝试取消注册不存在的回调
    EXPECT_FALSE(registry.unregisterEventCallback(99999));
}

// 事务测试
TEST_F(RegistryTest, Transactions) {
    // 开始事务
    EXPECT_TRUE(registry.beginTransaction());
    
    // 在事务中进行一些更改
    registry.setValue(testKeyPath, "TransactionValue1", "Data1");
    registry.setValue(testKeyPath, "TransactionValue2", "Data2");
    registry.createKey("TransactionKey");
    
    // 回滚事务
    EXPECT_EQ(registry.rollbackTransaction(), RegistryResult::SUCCESS);
    
    // 验证更改已被撤销
    EXPECT_FALSE(registry.valueExists(testKeyPath, "TransactionValue1"));
    EXPECT_FALSE(registry.valueExists(testKeyPath, "TransactionValue2"));
    EXPECT_FALSE(registry.keyExists("TransactionKey"));
    
    // 再次开始事务
    EXPECT_TRUE(registry.beginTransaction());
    
    // 进行一些更改
    registry.setValue(testKeyPath, "CommitValue", "CommitData");
    registry.createKey("CommitKey");
    
    // 提交事务
    EXPECT_EQ(registry.commitTransaction(), RegistryResult::SUCCESS);
    
    // 验证更改已被保存
    EXPECT_TRUE(registry.valueExists(testKeyPath, "CommitValue"));
    EXPECT_TRUE(registry.keyExists("CommitKey"));
    
    // 尝试在没有活动事务的情况下回滚
    EXPECT_EQ(registry.rollbackTransaction(), RegistryResult::UNKNOWN_ERROR);
}

// 自动保存测试
TEST_F(RegistryTest, AutoSave) {
    // 设置自动保存
    registry.setAutoSave(true);
    
    // 进行更改
    registry.setValue(testKeyPath, "AutoSaveValue", "AutoSaveData");
    
    // 创建新的注册表实例
    Registry newRegistry;
    
    // 获取测试文件路径
    auto tempDir = std::filesystem::temp_directory_path();
    std::string testFilePath = (tempDir / "test_registry.dat").string();
    
    // 尝试加载文件，验证数据是否已自动保存
    EXPECT_EQ(newRegistry.loadRegistryFromFile(testFilePath), 
              RegistryResult::SUCCESS);
    
    // 验证数据已保存
    EXPECT_TRUE(newRegistry.valueExists(testKeyPath, "AutoSaveValue"));
    auto value = newRegistry.getValue(testKeyPath, "AutoSaveValue");
    EXPECT_TRUE(value.has_value());
    EXPECT_EQ(value.value(), "AutoSaveData");
    
    // 禁用自动保存
    registry.setAutoSave(false);
    
    // 进行新的更改
    registry.setValue(testKeyPath, "ManualSaveValue", "ManualSaveData");
    
    // 创建另一个新的注册表实例
    Registry anotherRegistry;
    
    // 加载文件
    EXPECT_EQ(anotherRegistry.loadRegistryFromFile(testFilePath), 
              RegistryResult::SUCCESS);
    
    // 验证新更改未自动保存
    EXPECT_FALSE(anotherRegistry.valueExists(testKeyPath, "ManualSaveValue"));
}

// 错误处理测试
TEST_F(RegistryTest, ErrorHandling) {
    // 尝试执行一个会失败的操作
    EXPECT_EQ(registry.setValue("NonExistentKey", testValueName, testValueData), 
              RegistryResult::KEY_NOT_FOUND);
    
    // 获取上次操作的错误信息
    std::string errorMsg = registry.getLastError();
    EXPECT_FALSE(errorMsg.empty());
    EXPECT_THAT(errorMsg, testing::HasSubstr("KEY_NOT_FOUND"));
    
    // 执行成功的操作后，错误信息应该被清除或更新
    EXPECT_EQ(registry.setValue(testKeyPath, testValueName, testValueData), 
              RegistryResult::SUCCESS);
    errorMsg = registry.getLastError();
    EXPECT_TRUE(errorMsg.empty() || errorMsg.find("SUCCESS") != std::string::npos);
}

// 多线程测试
TEST_F(RegistryTest, ThreadSafety) {
    const int numThreads = 5;
    const int operationsPerThread = 100;
    
    std::vector<std::thread> threads;
    
    // 启动多个线程同时读写注册表
    for (int t = 0; t < numThreads; ++t) {
        threads.emplace_back([&, t]() {
            std::string threadKeyPath = "ThreadTest/Thread" + std::to_string(t);
            registry.createKey(threadKeyPath);
            
            for (int i = 0; i < operationsPerThread; ++i) {
                std::string valueName = "Value" + std::to_string(i);
                std::string valueData = "Data" + std::to_string(i) + "_" + std::to_string(t);
                
                registry.setValue(threadKeyPath, valueName, valueData);
                
                auto value = registry.getValue(threadKeyPath, valueName);
                if (value.has_value()) {
                    EXPECT_EQ(value.value(), valueData);
                }
                
                if (i % 10 == 0) {
                    registry.deleteValue(threadKeyPath, "Value" + std::to_string(i / 10));
                }
            }
        });
    }
    
    // 等待所有线程完成
    for (auto& thread : threads) {
        thread.join();
    }
    
    // 验证结果
    for (int t = 0; t < numThreads; ++t) {
        std::string threadKeyPath = "ThreadTest/Thread" + std::to_string(t);
        EXPECT_TRUE(registry.keyExists(threadKeyPath));
        
        auto valueNames = registry.getValueNames(threadKeyPath);
        EXPECT_FALSE(valueNames.empty());
        
        // 验证一些随机值
        for (int i = operationsPerThread - 5; i < operationsPerThread; ++i) {
            std::string valueName = "Value" + std::to_string(i);
            if (i % 10 != 0) { // 非删除的值
                EXPECT_TRUE(registry.valueExists(threadKeyPath, valueName));
                auto value = registry.getValue(threadKeyPath, valueName);
                EXPECT_TRUE(value.has_value());
                EXPECT_EQ(value.value(), "Data" + std::to_string(i) + "_" + std::to_string(t));
            }
        }
    }
}

// 性能测试
TEST_F(RegistryTest, DISABLED_PerformanceTest) {
    const int numKeys = 1000;
    const int valuesPerKey = 10;
    
    auto start = std::chrono::high_resolution_clock::now();
    
    // 创建大量键和值
    for (int i = 0; i < numKeys; ++i) {
        std::string keyPath = "PerfTest/Key" + std::to_string(i);
        registry.createKey(keyPath);
        
        for (int j = 0; j < valuesPerKey; ++j) {
            std::string valueName = "Value" + std::to_string(j);
            std::string valueData = "Data" + std::to_string(i) + "_" + std::to_string(j);
            registry.setValue(keyPath, valueName, valueData);
        }
    }
    
    auto createEnd = std::chrono::high_resolution_clock::now();
    auto createDuration = std::chrono::duration_cast<std::chrono::milliseconds>(
        createEnd - start).count();
    
    std::cout << "Created " << numKeys << " keys with " << valuesPerKey 
              << " values each in " << createDuration << "ms" << std::endl;
    
    // 读取所有值
    int readCount = 0;
    start = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < numKeys; ++i) {
        std::string keyPath = "PerfTest/Key" + std::to_string(i);
        
        for (int j = 0; j < valuesPerKey; ++j) {
            std::string valueName = "Value" + std::to_string(j);
            auto value = registry.getValue(keyPath, valueName);
            if (value.has_value()) {
                readCount++;
            }
        }
    }
    
    auto readEnd = std::chrono::high_resolution_clock::now();
    auto readDuration = std::chrono::duration_cast<std::chrono::milliseconds>(
        readEnd - start).count();
    
    std::cout << "Read " << readCount << " values in " << readDuration << "ms" << std::endl;
    
    // 验证读取的数量正确
    EXPECT_EQ(readCount, numKeys * valuesPerKey);
    
    // 输出每秒操作数
    double createOpsPerSecond = (double)(numKeys * valuesPerKey) / ((double)createDuration / 1000.0);
    double readOpsPerSecond = (double)readCount / ((double)readDuration / 1000.0);
    
    std::cout << "Create operations per second: " << createOpsPerSecond << std::endl;
    std::cout << "Read operations per second: " << readOpsPerSecond << std::endl;
}

// 边缘情况测试
TEST_F(RegistryTest, EdgeCases) {
    // 非常长的键路径
    std::string longKeyPath(1000, 'a');
    EXPECT_EQ(registry.createKey(longKeyPath), RegistryResult::SUCCESS);
    EXPECT_TRUE(registry.keyExists(longKeyPath));
    
    // 非常长的值名称
    std::string longValueName(1000, 'b');
    EXPECT_EQ(registry.setValue(testKeyPath, longValueName, "TestData"), 
              RegistryResult::SUCCESS);
    EXPECT_TRUE(registry.valueExists(testKeyPath, longValueName));
    
    // 非常长的值数据
    std::string longValueData(10000, 'c');
    EXPECT_EQ(registry.setValue(testKeyPath, "LongDataValue", longValueData), 
              RegistryResult::SUCCESS);
    auto value = registry.getValue(testKeyPath, "LongDataValue");
    EXPECT_TRUE(value.has_value());
    EXPECT_EQ(value.value(), longValueData);
    
    // 空键路径
    EXPECT_EQ(registry.createKey(""), RegistryResult::INVALID_FORMAT);
    
    // 空值名称
    EXPECT_EQ(registry.setValue(testKeyPath, "", "EmptyNameData"), 
              RegistryResult::INVALID_FORMAT);
    
    // 嵌套级别非常深的键
    std::string deepKeyPath;
    for (int i = 0; i < 100; ++i) {
        deepKeyPath += "Level" + std::to_string(i) + "/";
    }
    deepKeyPath += "FinalKey";
    
    EXPECT_EQ(registry.createKey(deepKeyPath), RegistryResult::SUCCESS);
    EXPECT_TRUE(registry.keyExists(deepKeyPath));
}

// 加密测试（如果支持）
TEST_F(RegistryTest, Encryption) {
    // 初始化一个新的带加密的注册表
    Registry encryptedRegistry;
    
    auto tempDir = std::filesystem::temp_directory_path();
    std::string encryptedFilePath = (tempDir / "encrypted_registry.dat").string();
    
    EXPECT_EQ(encryptedRegistry.initialize(encryptedFilePath, true), 
              RegistryResult::SUCCESS);
    
    // 设置一些数据
    EXPECT_EQ(encryptedRegistry.createKey("EncryptedKey"), RegistryResult::SUCCESS);
    EXPECT_EQ(encryptedRegistry.setValue("EncryptedKey", "SecretValue", "SecretData"), 
              RegistryResult::SUCCESS);
    
    // 确保数据已正确存储
    auto value = encryptedRegistry.getValue("EncryptedKey", "SecretValue");
    EXPECT_TRUE(value.has_value());
    EXPECT_EQ(value.value(), "SecretData");
    
    // 检查文件是否存在
    EXPECT_TRUE(std::filesystem::exists(encryptedFilePath));
    
    // 尝试不用加密打开加密的文件（应该会失败或者读取数据错误）
    Registry nonEncryptedRegistry;
    EXPECT_EQ(nonEncryptedRegistry.initialize(encryptedFilePath, false), 
              RegistryResult::SUCCESS);
    
    // 尝试读取数据，这可能成功或失败，取决于实现
    // 但即使成功，也不应该匹配原始数据
    auto attemptedValue = nonEncryptedRegistry.getValue("EncryptedKey", "SecretValue");
    if (attemptedValue.has_value()) {
        EXPECT_NE(attemptedValue.value(), "SecretData");
    }
    
    // 清理
    std::filesystem::remove(encryptedFilePath);
}