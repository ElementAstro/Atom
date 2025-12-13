/*
 * test_fuzz.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-22

Description: Tests for fuzz testing in atom/tests/performance/fuzz.hpp

**************************************************/

#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <set>
#include <string>
#include <vector>

#include "atom/tests/performance/fuzz.hpp"

namespace atom::test::performance::tests {

using namespace atom::tests;

// ============================================================================
// RandomConfig Tests
// ============================================================================

class RandomConfigTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(RandomConfigTest, DefaultConfig) {
    RandomConfig config;

    EXPECT_GT(config.defaultIntMax, 0);
    EXPECT_LT(config.charMin, config.charMax);
}

TEST_F(RandomConfigTest, SetDefaultIntMax) {
    RandomConfig config;
    config.setDefaultIntMax(500);

    EXPECT_EQ(config.defaultIntMax, 500);
}

TEST_F(RandomConfigTest, SetCharRange) {
    RandomConfig config;
    config.setCharRange(65, 90);  // A-Z

    EXPECT_EQ(config.charMin, 65);
    EXPECT_EQ(config.charMax, 90);
}

TEST_F(RandomConfigTest, FluentConfiguration) {
    RandomConfig config;
    config.setDefaultIntMax(200).setCharRange(48, 57).enableThreadSafety();

    EXPECT_EQ(config.defaultIntMax, 200);
    EXPECT_EQ(config.charMin, 48);
    EXPECT_TRUE(config.threadSafe);
}

TEST_F(RandomConfigTest, InvalidConfigThrows) {
    RandomConfig config;
    EXPECT_THROW(config.setDefaultIntMax(-1), RandomGenerationError);
    EXPECT_THROW(config.setCharRange(100, 50), RandomGenerationError);
}

// ============================================================================
// RandomDataGenerator Construction Tests
// ============================================================================

class RandomDataGeneratorConstructionTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(RandomDataGeneratorConstructionTest, DefaultConstruction) {
    RandomDataGenerator gen;
    // Should not throw
    EXPECT_TRUE(true);
}

TEST_F(RandomDataGeneratorConstructionTest, ConstructionWithSeed) {
    RandomDataGenerator gen(42);
    // Should produce deterministic results
    int first = gen.generateInteger(0, 100);
    
    RandomDataGenerator gen2(42);
    int second = gen2.generateInteger(0, 100);
    
    EXPECT_EQ(first, second);
}

TEST_F(RandomDataGeneratorConstructionTest, ConstructionWithConfig) {
    RandomConfig config;
    config.setDefaultIntMax(50);
    
    RandomDataGenerator gen(config);
    EXPECT_TRUE(true);
}

TEST_F(RandomDataGeneratorConstructionTest, ConstructionWithConfigAndSeed) {
    RandomConfig config;
    config.setDefaultIntMax(50);
    
    RandomDataGenerator gen(config, 42);
    EXPECT_TRUE(true);
}

// ============================================================================
// Integer Generation Tests
// ============================================================================

class IntegerGenerationTest : public ::testing::Test {
protected:
    void SetUp() override { gen = std::make_unique<RandomDataGenerator>(42); }
    void TearDown() override { gen.reset(); }

    std::unique_ptr<RandomDataGenerator> gen;
};

TEST_F(IntegerGenerationTest, GenerateSingleInteger) {
    int value = gen->generateInteger(0, 100);
    EXPECT_GE(value, 0);
    EXPECT_LE(value, 100);
}

TEST_F(IntegerGenerationTest, GenerateIntegersInRange) {
    for (int i = 0; i < 100; ++i) {
        int value = gen->generateInteger(10, 20);
        EXPECT_GE(value, 10);
        EXPECT_LE(value, 20);
    }
}

TEST_F(IntegerGenerationTest, GenerateMultipleIntegers) {
    auto values = gen->generateIntegers(50, 0, 100);
    EXPECT_EQ(values.size(), 50);
    
    for (int value : values) {
        EXPECT_GE(value, 0);
        EXPECT_LE(value, 100);
    }
}

TEST_F(IntegerGenerationTest, NegativeRange) {
    int value = gen->generateInteger(-100, -1);
    EXPECT_GE(value, -100);
    EXPECT_LE(value, -1);
}

// ============================================================================
// Real Number Generation Tests
// ============================================================================

class RealGenerationTest : public ::testing::Test {
protected:
    void SetUp() override { gen = std::make_unique<RandomDataGenerator>(42); }
    void TearDown() override { gen.reset(); }

    std::unique_ptr<RandomDataGenerator> gen;
};

TEST_F(RealGenerationTest, GenerateSingleReal) {
    double value = gen->generateReal(0.0, 1.0);
    EXPECT_GE(value, 0.0);
    EXPECT_LE(value, 1.0);
}

TEST_F(RealGenerationTest, GenerateRealsInRange) {
    for (int i = 0; i < 100; ++i) {
        double value = gen->generateReal(-5.0, 5.0);
        EXPECT_GE(value, -5.0);
        EXPECT_LE(value, 5.0);
    }
}

TEST_F(RealGenerationTest, GenerateMultipleReals) {
    auto values = gen->generateReals(50, 0.0, 10.0);
    EXPECT_EQ(values.size(), 50);
    
    for (double value : values) {
        EXPECT_GE(value, 0.0);
        EXPECT_LE(value, 10.0);
    }
}

// ============================================================================
// String Generation Tests
// ============================================================================

class StringGenerationTest : public ::testing::Test {
protected:
    void SetUp() override { gen = std::make_unique<RandomDataGenerator>(42); }
    void TearDown() override { gen.reset(); }

    std::unique_ptr<RandomDataGenerator> gen;
};

TEST_F(StringGenerationTest, GenerateString) {
    auto str = gen->generateString(10, false);
    EXPECT_EQ(str.length(), 10);
}

TEST_F(StringGenerationTest, GenerateAlphanumericString) {
    auto str = gen->generateString(20, true);
    EXPECT_EQ(str.length(), 20);
    
    for (char c : str) {
        EXPECT_TRUE(std::isalnum(c));
    }
}

TEST_F(StringGenerationTest, GenerateStringWithCustomCharset) {
    auto str = gen->generateString(10, false, "ABC");
    EXPECT_EQ(str.length(), 10);
    
    for (char c : str) {
        EXPECT_TRUE(c == 'A' || c == 'B' || c == 'C');
    }
}

TEST_F(StringGenerationTest, EmptyStringGeneration) {
    auto str = gen->generateString(0, true);
    EXPECT_TRUE(str.empty());
}

// ============================================================================
// Boolean Generation Tests
// ============================================================================

class BooleanGenerationTest : public ::testing::Test {
protected:
    void SetUp() override { gen = std::make_unique<RandomDataGenerator>(42); }
    void TearDown() override { gen.reset(); }

    std::unique_ptr<RandomDataGenerator> gen;
};

TEST_F(BooleanGenerationTest, GenerateSingleBoolean) {
    bool value = gen->generateBoolean();
    EXPECT_TRUE(value == true || value == false);
}

TEST_F(BooleanGenerationTest, GenerateMultipleBooleans) {
    auto values = gen->generateBooleans(100, 0.5);
    EXPECT_EQ(values.size(), 100);
    
    int trueCount = std::count(values.begin(), values.end(), true);
    // With 50% probability, expect roughly 50 true values (with tolerance)
    EXPECT_GT(trueCount, 20);
    EXPECT_LT(trueCount, 80);
}

TEST_F(BooleanGenerationTest, BiasedBoolean) {
    int trueCount = 0;
    for (int i = 0; i < 1000; ++i) {
        if (gen->generateBoolean(0.9)) {
            trueCount++;
        }
    }
    // With 90% true probability, expect high count
    EXPECT_GT(trueCount, 800);
}

// ============================================================================
// DateTime Generation Tests
// ============================================================================

class DateTimeGenerationTest : public ::testing::Test {
protected:
    void SetUp() override { gen = std::make_unique<RandomDataGenerator>(42); }
    void TearDown() override { gen.reset(); }

    std::unique_ptr<RandomDataGenerator> gen;
};

TEST_F(DateTimeGenerationTest, GenerateDateTimeInRange) {
    auto start = std::chrono::system_clock::now();
    auto end = start + std::chrono::hours(24);
    
    auto dt = gen->generateDateTime(start, end);
    
    EXPECT_GE(dt, start);
    EXPECT_LE(dt, end);
}

// ============================================================================
// IP Address Generation Tests
// ============================================================================

class IPAddressGenerationTest : public ::testing::Test {
protected:
    void SetUp() override { gen = std::make_unique<RandomDataGenerator>(42); }
    void TearDown() override { gen.reset(); }

    std::unique_ptr<RandomDataGenerator> gen;
};

TEST_F(IPAddressGenerationTest, GenerateIPv4) {
    auto ip = gen->generateIPv4Address();
    
    // Should have 3 dots
    int dotCount = std::count(ip.begin(), ip.end(), '.');
    EXPECT_EQ(dotCount, 3);
}

// ============================================================================
// MAC Address Generation Tests
// ============================================================================

class MACAddressGenerationTest : public ::testing::Test {
protected:
    void SetUp() override { gen = std::make_unique<RandomDataGenerator>(42); }
    void TearDown() override { gen.reset(); }

    std::unique_ptr<RandomDataGenerator> gen;
};

TEST_F(MACAddressGenerationTest, GenerateMACAddress) {
    auto mac = gen->generateMACAddress();
    
    // Should have 5 colons (6 segments)
    int colonCount = std::count(mac.begin(), mac.end(), ':');
    EXPECT_EQ(colonCount, 5);
}

TEST_F(MACAddressGenerationTest, GenerateMACAddressUppercase) {
    auto mac = gen->generateMACAddress(true);
    
    // Check for uppercase hex digits
    for (char c : mac) {
        if (std::isalpha(c)) {
            EXPECT_TRUE(std::isupper(c));
        }
    }
}

// ============================================================================
// URL Generation Tests
// ============================================================================

class URLGenerationTest : public ::testing::Test {
protected:
    void SetUp() override { gen = std::make_unique<RandomDataGenerator>(42); }
    void TearDown() override { gen.reset(); }

    std::unique_ptr<RandomDataGenerator> gen;
};

TEST_F(URLGenerationTest, GenerateURL) {
    auto url = gen->generateURL();
    
    // Should start with http:// or https://
    EXPECT_TRUE(url.find("http://") == 0 || url.find("https://") == 0);
}

TEST_F(URLGenerationTest, GenerateURLWithProtocol) {
    auto url = gen->generateURL("https");
    
    EXPECT_TRUE(url.find("https://") == 0);
}

// ============================================================================
// Distribution Generation Tests
// ============================================================================

class DistributionGenerationTest : public ::testing::Test {
protected:
    void SetUp() override { gen = std::make_unique<RandomDataGenerator>(42); }
    void TearDown() override { gen.reset(); }

    std::unique_ptr<RandomDataGenerator> gen;
};

TEST_F(DistributionGenerationTest, NormalDistribution) {
    auto values = gen->generateNormalDistribution(1000, 50.0, 10.0);
    EXPECT_EQ(values.size(), 1000);
    
    // Calculate mean
    double sum = 0.0;
    for (double v : values) {
        sum += v;
    }
    double mean = sum / values.size();
    
    // Mean should be close to 50
    EXPECT_NEAR(mean, 50.0, 2.0);
}

TEST_F(DistributionGenerationTest, ExponentialDistribution) {
    auto values = gen->generateExponentialDistribution(1000, 1.0);
    EXPECT_EQ(values.size(), 1000);
    
    // All values should be positive
    for (double v : values) {
        EXPECT_GE(v, 0.0);
    }
}

// ============================================================================
// JSON Generation Tests
// ============================================================================

class JSONGenerationTest : public ::testing::Test {
protected:
    void SetUp() override { gen = std::make_unique<RandomDataGenerator>(42); }
    void TearDown() override { gen.reset(); }

    std::unique_ptr<RandomDataGenerator> gen;
};

TEST_F(JSONGenerationTest, GenerateRandomJSON) {
    auto json = gen->generateRandomJSON(2, 3);
    
    EXPECT_FALSE(json.empty());
    // Should start with { or [
    EXPECT_TRUE(json[0] == '{' || json[0] == '[');
}

// ============================================================================
// XML Generation Tests
// ============================================================================

class XMLGenerationTest : public ::testing::Test {
protected:
    void SetUp() override { gen = std::make_unique<RandomDataGenerator>(42); }
    void TearDown() override { gen.reset(); }

    std::unique_ptr<RandomDataGenerator> gen;
};

TEST_F(XMLGenerationTest, GenerateRandomXML) {
    auto xml = gen->generateRandomXML(2, 3);
    
    EXPECT_FALSE(xml.empty());
    // Should start with <
    EXPECT_EQ(xml[0], '<');
}

// ============================================================================
// Tree Generation Tests
// ============================================================================

class TreeGenerationTest : public ::testing::Test {
protected:
    void SetUp() override { gen = std::make_unique<RandomDataGenerator>(42); }
    void TearDown() override { gen.reset(); }

    std::unique_ptr<RandomDataGenerator> gen;
};

TEST_F(TreeGenerationTest, GenerateTree) {
    auto tree = gen->generateTree(3, 2);
    
    // Tree should have a value
    EXPECT_TRUE(true);  // Just verify it doesn't throw
}

// ============================================================================
// Graph Generation Tests
// ============================================================================

class GraphGenerationTest : public ::testing::Test {
protected:
    void SetUp() override { gen = std::make_unique<RandomDataGenerator>(42); }
    void TearDown() override { gen.reset(); }

    std::unique_ptr<RandomDataGenerator> gen;
};

TEST_F(GraphGenerationTest, GenerateGraph) {
    auto graph = gen->generateGraph(10, 0.5);
    
    EXPECT_EQ(graph.size(), 10);
}

// ============================================================================
// Thread Local Generator Tests
// ============================================================================

class ThreadLocalGeneratorTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(ThreadLocalGeneratorTest, GetThreadLocalGenerator) {
    auto& gen = RandomDataGenerator::threadLocal();
    
    int value = gen.generateInteger(0, 100);
    EXPECT_GE(value, 0);
    EXPECT_LE(value, 100);
}

// ============================================================================
// Reseed Tests
// ============================================================================

class ReseedTest : public ::testing::Test {
protected:
    void SetUp() override { gen = std::make_unique<RandomDataGenerator>(); }
    void TearDown() override { gen.reset(); }

    std::unique_ptr<RandomDataGenerator> gen;
};

TEST_F(ReseedTest, ReseedProducesDeterministicResults) {
    gen->reseed(123);
    int first = gen->generateInteger(0, 1000);
    
    gen->reseed(123);
    int second = gen->generateInteger(0, 1000);
    
    EXPECT_EQ(first, second);
}

}  // namespace atom::test::performance::tests
