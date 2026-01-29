/*
 * test_opencl_utils.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include <gtest/gtest.h>
#include <vector>

#include "atom/algorithm/core/opencl_utils.hpp"

namespace atom::algorithm::opencl::test {

class OpenCLUtilsTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

#if ATOM_OPENCL_AVAILABLE

TEST_F(OpenCLUtilsTest, PlatformEnumeration) {
    auto platforms = Platform::getPlatforms();
    // May be empty if no OpenCL runtime installed
    SUCCEED();
}

TEST_F(OpenCLUtilsTest, DeviceEnumeration) {
    auto platforms = Platform::getPlatforms();
    if (platforms.empty()) {
        GTEST_SKIP() << "No OpenCL platforms available";
    }

    auto devices = Platform::getDevices(platforms[0], DeviceType::ALL);
    SUCCEED();
}

TEST_F(OpenCLUtilsTest, ContextCreation) {
    auto platforms = Platform::getPlatforms();
    if (platforms.empty()) {
        GTEST_SKIP() << "No OpenCL platforms available";
    }

    auto devices = Platform::getDevices(platforms[0], DeviceType::ALL);
    if (devices.empty()) {
        GTEST_SKIP() << "No OpenCL devices available";
    }

    auto context = Platform::createContext(devices);
    EXPECT_TRUE(context.valid());
}

TEST_F(OpenCLUtilsTest, CommandQueueCreation) {
    auto platforms = Platform::getPlatforms();
    if (platforms.empty()) {
        GTEST_SKIP() << "No OpenCL platforms available";
    }

    auto devices = Platform::getDevices(platforms[0], DeviceType::ALL);
    if (devices.empty()) {
        GTEST_SKIP() << "No OpenCL devices available";
    }

    auto context = Platform::createContext(devices);
    if (!context.valid()) {
        GTEST_SKIP() << "Failed to create context";
    }

    auto queue = Platform::createCommandQueue(context, devices[0]);
    EXPECT_TRUE(queue.valid());
}

TEST_F(OpenCLUtilsTest, BufferCreation) {
    auto platforms = Platform::getPlatforms();
    if (platforms.empty()) {
        GTEST_SKIP() << "No OpenCL platforms available";
    }

    auto devices = Platform::getDevices(platforms[0], DeviceType::ALL);
    if (devices.empty()) {
        GTEST_SKIP() << "No OpenCL devices available";
    }

    auto context = Platform::createContext(devices);
    if (!context.valid()) {
        GTEST_SKIP() << "Failed to create context";
    }

    constexpr size_t buffer_size = 1024 * sizeof(float);
    auto buffer =
        Platform::createBuffer(context, MemoryFlags::READ_WRITE, buffer_size);
    EXPECT_TRUE(buffer.valid());
}

#endif  // ATOM_OPENCL_AVAILABLE

TEST_F(OpenCLUtilsTest, ComputeManagerSingleton) {
    auto& manager1 = ComputeManager::getInstance();
    auto& manager2 = ComputeManager::getInstance();
    EXPECT_EQ(&manager1, &manager2);
}

TEST_F(OpenCLUtilsTest, ComputeManagerAvailability) {
    auto& manager = ComputeManager::getInstance();
    // Just test that this doesn't crash
    bool available = manager.isAvailable();
    SUCCEED();
}

}  // namespace atom::algorithm::opencl::test
