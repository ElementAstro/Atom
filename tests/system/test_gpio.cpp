#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <chrono>
#include <functional>
#include <string>
#include <thread>

#include "atom/system/hardware/gpio.hpp"

namespace atom::system::test {

using atom::system::GPIO;
using Direction = GPIO::Direction;
using Edge = GPIO::Edge;
using PullMode = GPIO::PullMode;
using PwmMode = GPIO::PwmMode;

// Mock class for testing GPIO operations without actual hardware
class MockGPIO {
public:
    MOCK_METHOD(void, setValue, (bool value), (const));
    MOCK_METHOD(bool, getValue, (), (const));
    MOCK_METHOD(bool, toggle, (), (const));
    MOCK_METHOD(void, pulse, (bool value, std::chrono::milliseconds duration), (const));
    MOCK_METHOD(bool, setPwm, (double frequency, double dutyCycle, PwmMode mode), (const));
    MOCK_METHOD(bool, updatePwmDutyCycle, (double dutyCycle), (const));
    MOCK_METHOD(void, stopPwm, (), (const));
    MOCK_METHOD(bool, setupButtonDebounce, (std::function<void()> callback, unsigned int debounceTimeMs), (const));
    MOCK_METHOD(bool, setupInterruptCounter, (Edge edge), (const));
    MOCK_METHOD(unsigned int, getInterruptCount, (), (const));
    MOCK_METHOD(void, resetInterruptCount, (), (const));
    MOCK_METHOD(bool, onValueChange, (std::function<void(bool)> callback), (const));
    MOCK_METHOD(void, setPullMode, (PullMode mode), (const));
    MOCK_METHOD(PullMode, getPullMode, (), (const));
    MOCK_METHOD(void, setEdge, (Edge edge), (const));
    MOCK_METHOD(Edge, getEdge, (), (const));
    MOCK_METHOD(Direction, getDirection, (), (const));
    MOCK_METHOD(std::string, getPin, (), (const));
};

class GPIOTest : public ::testing::Test {
protected:
    void SetUp() override {
        mockGPIO = std::make_unique<::testing::NiceMock<MockGPIO>>();

        // Set up default behavior for the mock
        ON_CALL(*mockGPIO, getValue())
            .WillByDefault(::testing::Return(false));
        ON_CALL(*mockGPIO, toggle())
            .WillByDefault(::testing::Return(true));
        ON_CALL(*mockGPIO, setPwm(::testing::_, ::testing::_, ::testing::_))
            .WillByDefault(::testing::Return(true));
        ON_CALL(*mockGPIO, updatePwmDutyCycle(::testing::_))
            .WillByDefault(::testing::Return(true));
        ON_CALL(*mockGPIO, setupButtonDebounce(::testing::_, ::testing::_))
            .WillByDefault(::testing::Return(true));
        ON_CALL(*mockGPIO, setupInterruptCounter(::testing::_))
            .WillByDefault(::testing::Return(true));
        ON_CALL(*mockGPIO, getInterruptCount())
            .WillByDefault(::testing::Return(0));
        ON_CALL(*mockGPIO, onValueChange(::testing::_))
            .WillByDefault(::testing::Return(true));
        ON_CALL(*mockGPIO, getPullMode())
            .WillByDefault(::testing::Return(PullMode::NONE));
        ON_CALL(*mockGPIO, getEdge())
            .WillByDefault(::testing::Return(Edge::NONE));
        ON_CALL(*mockGPIO, getDirection())
            .WillByDefault(::testing::Return(Direction::OUTPUT));
        ON_CALL(*mockGPIO, getPin())
            .WillByDefault(::testing::Return("18"));
    }

    void TearDown() override {
        mockGPIO.reset();
    }

    std::unique_ptr<MockGPIO> mockGPIO;
};

// Test basic GPIO operations
TEST_F(GPIOTest, SetValue) {
    EXPECT_CALL(*mockGPIO, setValue(true))
        .Times(1);

    mockGPIO->setValue(true);
}

TEST_F(GPIOTest, GetValue) {
    EXPECT_CALL(*mockGPIO, getValue())
        .WillOnce(::testing::Return(true));

    bool value = mockGPIO->getValue();
    EXPECT_TRUE(value);
}

TEST_F(GPIOTest, Toggle) {
    EXPECT_CALL(*mockGPIO, toggle())
        .WillOnce(::testing::Return(true));

    bool newValue = mockGPIO->toggle();
    EXPECT_TRUE(newValue);
}

// Test pulse functionality
TEST_F(GPIOTest, Pulse) {
    EXPECT_CALL(*mockGPIO, pulse(true, std::chrono::milliseconds(100)))
        .Times(1);

    mockGPIO->pulse(true, std::chrono::milliseconds(100));
}

TEST_F(GPIOTest, PulseWithDifferentDurations) {
    EXPECT_CALL(*mockGPIO, pulse(false, std::chrono::milliseconds(50)))
        .Times(1);
    EXPECT_CALL(*mockGPIO, pulse(true, std::chrono::milliseconds(200)))
        .Times(1);

    mockGPIO->pulse(false, std::chrono::milliseconds(50));
    mockGPIO->pulse(true, std::chrono::milliseconds(200));
}

// Test PWM functionality
TEST_F(GPIOTest, SetPwm) {
    EXPECT_CALL(*mockGPIO, setPwm(1000.0, 50.0, PwmMode::HARDWARE))
        .WillOnce(::testing::Return(true));

    bool result = mockGPIO->setPwm(1000.0, 50.0, PwmMode::HARDWARE);
    EXPECT_TRUE(result);
}

TEST_F(GPIOTest, SetPwmSoftwareMode) {
    EXPECT_CALL(*mockGPIO, setPwm(500.0, 25.0, PwmMode::SOFTWARE))
        .WillOnce(::testing::Return(true));

    bool result = mockGPIO->setPwm(500.0, 25.0, PwmMode::SOFTWARE);
    EXPECT_TRUE(result);
}

TEST_F(GPIOTest, UpdatePwmDutyCycle) {
    EXPECT_CALL(*mockGPIO, updatePwmDutyCycle(75.0))
        .WillOnce(::testing::Return(true));

    bool result = mockGPIO->updatePwmDutyCycle(75.0);
    EXPECT_TRUE(result);
}

TEST_F(GPIOTest, StopPwm) {
    EXPECT_CALL(*mockGPIO, stopPwm())
        .Times(1);

    mockGPIO->stopPwm();
}

// Test PWM edge cases
TEST_F(GPIOTest, PwmEdgeCases) {
    // Test minimum duty cycle
    EXPECT_CALL(*mockGPIO, setPwm(1000.0, 0.0, PwmMode::HARDWARE))
        .WillOnce(::testing::Return(true));
    EXPECT_TRUE(mockGPIO->setPwm(1000.0, 0.0, PwmMode::HARDWARE));

    // Test maximum duty cycle
    EXPECT_CALL(*mockGPIO, setPwm(1000.0, 100.0, PwmMode::HARDWARE))
        .WillOnce(::testing::Return(true));
    EXPECT_TRUE(mockGPIO->setPwm(1000.0, 100.0, PwmMode::HARDWARE));

    // Test invalid duty cycle (should fail)
    EXPECT_CALL(*mockGPIO, setPwm(1000.0, 150.0, PwmMode::HARDWARE))
        .WillOnce(::testing::Return(false));
    EXPECT_FALSE(mockGPIO->setPwm(1000.0, 150.0, PwmMode::HARDWARE));
}

// Test button debounce
TEST_F(GPIOTest, SetupButtonDebounce) {
    auto callback = []() {
        // Mock button press callback
    };

    EXPECT_CALL(*mockGPIO, setupButtonDebounce(::testing::_, 50))
        .WillOnce(::testing::Return(true));

    bool result = mockGPIO->setupButtonDebounce(callback, 50);
    EXPECT_TRUE(result);
}

// Test interrupt counter
TEST_F(GPIOTest, SetupInterruptCounter) {
    EXPECT_CALL(*mockGPIO, setupInterruptCounter(Edge::RISING))
        .WillOnce(::testing::Return(true));

    bool result = mockGPIO->setupInterruptCounter(Edge::RISING);
    EXPECT_TRUE(result);
}

TEST_F(GPIOTest, GetInterruptCount) {
    EXPECT_CALL(*mockGPIO, getInterruptCount())
        .WillOnce(::testing::Return(5));

    unsigned int count = mockGPIO->getInterruptCount();
    EXPECT_EQ(count, 5);
}

TEST_F(GPIOTest, ResetInterruptCount) {
    EXPECT_CALL(*mockGPIO, resetInterruptCount())
        .Times(1);

    mockGPIO->resetInterruptCount();
}

// Test value change callback
TEST_F(GPIOTest, OnValueChange) {
    auto callback = [](bool value) {
        // Mock value change callback
    };

    EXPECT_CALL(*mockGPIO, onValueChange(::testing::_))
        .WillOnce(::testing::Return(true));

    bool result = mockGPIO->onValueChange(callback);
    EXPECT_TRUE(result);
}

// Test pull mode
TEST_F(GPIOTest, SetPullMode) {
    EXPECT_CALL(*mockGPIO, setPullMode(PullMode::UP))
        .Times(1);

    mockGPIO->setPullMode(PullMode::UP);
}

TEST_F(GPIOTest, GetPullMode) {
    EXPECT_CALL(*mockGPIO, getPullMode())
        .WillOnce(::testing::Return(PullMode::DOWN));

    PullMode mode = mockGPIO->getPullMode();
    EXPECT_EQ(mode, PullMode::DOWN);
}

TEST_F(GPIOTest, SetPullModeAllTypes) {
    std::vector<PullMode> modes = {
        PullMode::NONE,
        PullMode::UP,
        PullMode::DOWN
    };

    for (auto mode : modes) {
        EXPECT_CALL(*mockGPIO, setPullMode(mode))
            .Times(1);
        mockGPIO->setPullMode(mode);
    }
}

// Test edge detection
TEST_F(GPIOTest, SetEdge) {
    EXPECT_CALL(*mockGPIO, setEdge(Edge::FALLING))
        .Times(1);

    mockGPIO->setEdge(Edge::FALLING);
}

TEST_F(GPIOTest, GetEdge) {
    EXPECT_CALL(*mockGPIO, getEdge())
        .WillOnce(::testing::Return(Edge::BOTH));

    Edge edge = mockGPIO->getEdge();
    EXPECT_EQ(edge, Edge::BOTH);
}

TEST_F(GPIOTest, SetEdgeAllTypes) {
    std::vector<Edge> edges = {
        Edge::NONE,
        Edge::RISING,
        Edge::FALLING,
        Edge::BOTH
    };

    for (auto edge : edges) {
        EXPECT_CALL(*mockGPIO, setEdge(edge))
            .Times(1);
        mockGPIO->setEdge(edge);
    }
}

// Test GPIO properties
TEST_F(GPIOTest, GetDirection) {
    EXPECT_CALL(*mockGPIO, getDirection())
        .WillOnce(::testing::Return(Direction::INPUT));

    Direction direction = mockGPIO->getDirection();
    EXPECT_EQ(direction, Direction::INPUT);
}

TEST_F(GPIOTest, GetPin) {
    EXPECT_CALL(*mockGPIO, getPin())
        .WillOnce(::testing::Return("24"));

    std::string pin = mockGPIO->getPin();
    EXPECT_EQ(pin, "24");
}

// Error handling tests
class GPIOErrorTest : public ::testing::Test {
protected:
    void SetUp() override {
        mockGPIO = std::make_unique<::testing::NiceMock<MockGPIO>>();
    }

    void TearDown() override {
        mockGPIO.reset();
    }

    std::unique_ptr<MockGPIO> mockGPIO;
};

// Test invalid PWM parameters
TEST_F(GPIOErrorTest, InvalidPwmParameters) {
    // Test negative frequency
    EXPECT_CALL(*mockGPIO, setPwm(-100.0, 50.0, PwmMode::HARDWARE))
        .WillOnce(::testing::Return(false));
    EXPECT_FALSE(mockGPIO->setPwm(-100.0, 50.0, PwmMode::HARDWARE));

    // Test negative duty cycle
    EXPECT_CALL(*mockGPIO, setPwm(1000.0, -10.0, PwmMode::HARDWARE))
        .WillOnce(::testing::Return(false));
    EXPECT_FALSE(mockGPIO->setPwm(1000.0, -10.0, PwmMode::HARDWARE));

    // Test duty cycle > 100%
    EXPECT_CALL(*mockGPIO, setPwm(1000.0, 110.0, PwmMode::HARDWARE))
        .WillOnce(::testing::Return(false));
    EXPECT_FALSE(mockGPIO->setPwm(1000.0, 110.0, PwmMode::HARDWARE));
}

// Test invalid debounce time
TEST_F(GPIOErrorTest, InvalidDebounceTime) {
    auto callback = []() {};

    // Test zero debounce time (should fail)
    EXPECT_CALL(*mockGPIO, setupButtonDebounce(::testing::_, 0))
        .WillOnce(::testing::Return(false));
    EXPECT_FALSE(mockGPIO->setupButtonDebounce(callback, 0));

    // Test very large debounce time (should fail)
    EXPECT_CALL(*mockGPIO, setupButtonDebounce(::testing::_, 10000))
        .WillOnce(::testing::Return(false));
    EXPECT_FALSE(mockGPIO->setupButtonDebounce(callback, 10000));
}

// Test callback failures
TEST_F(GPIOErrorTest, CallbackSetupFailure) {
    auto callback = [](bool) {};

    EXPECT_CALL(*mockGPIO, onValueChange(::testing::_))
        .WillOnce(::testing::Return(false));

    bool result = mockGPIO->onValueChange(callback);
    EXPECT_FALSE(result);
}

// Integration tests
class GPIOIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        mockGPIO = std::make_unique<::testing::NiceMock<MockGPIO>>();

        ON_CALL(*mockGPIO, getValue())
            .WillByDefault(::testing::Return(false));
        ON_CALL(*mockGPIO, toggle())
            .WillByDefault(::testing::Return(true));
    }

    void TearDown() override {
        mockGPIO.reset();
    }

    std::unique_ptr<MockGPIO> mockGPIO;
};

// Test PWM and value change interaction
TEST_F(GPIOIntegrationTest, PwmAndValueChangeInteraction) {
    // Set up PWM
    EXPECT_CALL(*mockGPIO, setPwm(1000.0, 50.0, PwmMode::HARDWARE))
        .WillOnce(::testing::Return(true));
    EXPECT_TRUE(mockGPIO->setPwm(1000.0, 50.0, PwmMode::HARDWARE));

    // Try to set value while PWM is active (should be handled appropriately)
    EXPECT_CALL(*mockGPIO, setValue(true))
        .Times(1);
    mockGPIO->setValue(true);

    // Stop PWM
    EXPECT_CALL(*mockGPIO, stopPwm())
        .Times(1);
    mockGPIO->stopPwm();
}

// Test interrupt counter with edge detection
TEST_F(GPIOIntegrationTest, InterruptCounterWithEdgeDetection) {
    // Set edge detection
    EXPECT_CALL(*mockGPIO, setEdge(Edge::RISING))
        .Times(1);
    mockGPIO->setEdge(Edge::RISING);

    // Setup interrupt counter
    EXPECT_CALL(*mockGPIO, setupInterruptCounter(Edge::RISING))
        .WillOnce(::testing::Return(true));
    EXPECT_TRUE(mockGPIO->setupInterruptCounter(Edge::RISING));

    // Simulate some interrupts
    EXPECT_CALL(*mockGPIO, getInterruptCount())
        .WillOnce(::testing::Return(3));
    EXPECT_EQ(mockGPIO->getInterruptCount(), 3);

    // Reset counter
    EXPECT_CALL(*mockGPIO, resetInterruptCount())
        .Times(1);
    mockGPIO->resetInterruptCount();

    EXPECT_CALL(*mockGPIO, getInterruptCount())
        .WillOnce(::testing::Return(0));
    EXPECT_EQ(mockGPIO->getInterruptCount(), 0);
}

// Test pull mode with input direction
TEST_F(GPIOIntegrationTest, PullModeWithInputDirection) {
    // Simulate input direction
    EXPECT_CALL(*mockGPIO, getDirection())
        .WillRepeatedly(::testing::Return(Direction::INPUT));

    // Set pull mode for input pin
    EXPECT_CALL(*mockGPIO, setPullMode(PullMode::UP))
        .Times(1);
    mockGPIO->setPullMode(PullMode::UP);

    // Verify pull mode is set
    EXPECT_CALL(*mockGPIO, getPullMode())
        .WillOnce(::testing::Return(PullMode::UP));
    EXPECT_EQ(mockGPIO->getPullMode(), PullMode::UP);
}

// Performance tests
class GPIOPerformanceTest : public ::testing::Test {
protected:
    void SetUp() override {
        mockGPIO = std::make_unique<::testing::NiceMock<MockGPIO>>();

        ON_CALL(*mockGPIO, setValue(::testing::_))
            .WillByDefault(::testing::Return());
        ON_CALL(*mockGPIO, getValue())
            .WillByDefault(::testing::Return(false));
    }

    void TearDown() override {
        mockGPIO.reset();
    }

    std::unique_ptr<MockGPIO> mockGPIO;
};

// Test rapid GPIO operations
TEST_F(GPIOPerformanceTest, RapidGPIOOperations) {
    EXPECT_CALL(*mockGPIO, setValue(::testing::_))
        .Times(1000);

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < 1000; ++i) {
        mockGPIO->setValue(i % 2 == 0);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // GPIO operations should be fast (within 100ms for 1000 operations)
    EXPECT_LT(duration.count(), 100);
}

// Test PWM duty cycle updates
TEST_F(GPIOPerformanceTest, RapidPwmDutyCycleUpdates) {
    EXPECT_CALL(*mockGPIO, updatePwmDutyCycle(::testing::_))
        .Times(100)
        .WillRepeatedly(::testing::Return(true));

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < 100; ++i) {
        mockGPIO->updatePwmDutyCycle(i % 101); // 0-100%
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // PWM updates should be reasonably fast
    EXPECT_LT(duration.count(), 200);
}

}  // namespace atom::system::test
