#include "atom/sysinfo/os.hpp"
#include <gtest/gtest.h>
#include <chrono>
#include <string>
#include <thread>
#include "atom/sysinfo/locale.hpp"

using namespace atom::system;

namespace atom::sysinfo::test {

// ============================================================================
// Basic OS Tests
// ============================================================================

class OSTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup OS tests
    }

    void TearDown() override {
        // Cleanup
    }
};

TEST_F(OSTest, GetOperatingSystemInfo) {
    // Test getting operating system information
    OperatingSystemInfo osInfo = getOperatingSystemInfo();

    // OS name should not be empty
    EXPECT_FALSE(osInfo.osName.empty());
    EXPECT_GT(osInfo.osName.length(), 0);

    // OS version should not be empty
    EXPECT_FALSE(osInfo.osVersion.empty());
    EXPECT_GT(osInfo.osVersion.length(), 0);

    // Architecture should not be empty
    EXPECT_FALSE(osInfo.architecture.empty());
    EXPECT_GT(osInfo.architecture.length(), 0);

    // Computer name should not be empty
    EXPECT_FALSE(osInfo.computerName.empty());
    EXPECT_GT(osInfo.computerName.length(), 0);

    // String lengths should be reasonable
    EXPECT_LT(osInfo.osName.length(), 200);
    EXPECT_LT(osInfo.osVersion.length(), 200);
    EXPECT_LT(osInfo.architecture.length(), 100);
    EXPECT_LT(osInfo.computerName.length(), 200);
}

TEST_F(OSTest, GetSystemUptime) {
    // Test getting system uptime
    std::chrono::seconds uptime = getSystemUptime();

    // Uptime should be positive
    EXPECT_GT(uptime.count(), 0);

    // Uptime should be reasonable (less than 10 years)
    EXPECT_LT(uptime.count(), 10 * 365 * 24 * 3600);
}

TEST_F(OSTest, GetSystemLanguage) {
    // Test getting system language
    std::string language = getSystemLanguage();

    // Language should not be empty
    EXPECT_FALSE(language.empty());
    EXPECT_GT(language.length(), 0);

    // Language should be reasonable length
    EXPECT_LT(language.length(), 100);
}

TEST_F(OSTest, GetSystemEncoding) {
    // Test getting system encoding
    std::string encoding = getSystemEncoding();

    // Encoding should not be empty
    EXPECT_FALSE(encoding.empty());
    EXPECT_GT(encoding.length(), 0);

    // Encoding should be reasonable length
    EXPECT_LT(encoding.length(), 100);
}

TEST_F(OSTest, GetSystemLanguageInfo) {
    // Test getting detailed system language information
    LocaleInfo localeInfo = getSystemLanguageInfo();

    // Language code should not be empty
    EXPECT_FALSE(localeInfo.languageCode.empty());
    EXPECT_GT(localeInfo.languageCode.length(), 0);

    // Character encoding should not be empty
    EXPECT_FALSE(localeInfo.characterEncoding.empty());
    EXPECT_GT(localeInfo.characterEncoding.length(), 0);

    // String lengths should be reasonable
    EXPECT_LT(localeInfo.languageCode.length(), 50);
    EXPECT_LT(localeInfo.localeName.length(), 200);
    EXPECT_LT(localeInfo.currencySymbol.length(), 20);
    EXPECT_LT(localeInfo.characterEncoding.length(), 100);
}

// ============================================================================
// Real System Tests
// ============================================================================

class RealOSTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup real OS tests
    }

    void TearDown() override {
        // Cleanup
    }
};

TEST_F(RealOSTest, OSInfoConsistency) {
    // Test that OS info is consistent across calls
    OperatingSystemInfo osInfo1 = getOperatingSystemInfo();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    OperatingSystemInfo osInfo2 = getOperatingSystemInfo();

    // Static information should be identical
    EXPECT_EQ(osInfo1.osName, osInfo2.osName);
    EXPECT_EQ(osInfo1.osVersion, osInfo2.osVersion);
    EXPECT_EQ(osInfo1.architecture, osInfo2.architecture);
    EXPECT_EQ(osInfo1.computerName, osInfo2.computerName);
}

TEST_F(RealOSTest, UptimeMonitoring) {
    // Test uptime monitoring
    std::chrono::seconds uptime1 = getSystemUptime();
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    std::chrono::seconds uptime2 = getSystemUptime();

    // Uptime should increase
    EXPECT_GE(uptime2.count(), uptime1.count());

    // Uptime difference should be reasonable
    auto uptimeDiff = uptime2.count() - uptime1.count();
    EXPECT_GE(uptimeDiff, 0);
    EXPECT_LT(uptimeDiff, 10);  // Should be less than 10 seconds for this test
}

TEST_F(RealOSTest, LanguageInfoConsistency) {
    // Test language info consistency
    std::string language1 = getSystemLanguage();
    std::string encoding1 = getSystemEncoding();
    LocaleInfo localeInfo1 = getSystemLanguageInfo();

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    std::string language2 = getSystemLanguage();
    std::string encoding2 = getSystemEncoding();
    LocaleInfo localeInfo2 = getSystemLanguageInfo();

    // Language info should be consistent
    EXPECT_EQ(language1, language2);
    EXPECT_EQ(encoding1, encoding2);
    EXPECT_EQ(localeInfo1.languageCode, localeInfo2.languageCode);
    EXPECT_EQ(localeInfo1.characterEncoding, localeInfo2.characterEncoding);
}

TEST_F(RealOSTest, OSArchitectureValidation) {
    // Test OS architecture validation
    OperatingSystemInfo osInfo = getOperatingSystemInfo();

    // Common architectures
    std::vector<std::string> commonArchitectures = {"x86",     "x64",  "x86_64",
                                                    "amd64",   "arm",  "arm64",
                                                    "aarch64", "i386", "i686"};

    bool hasKnownArchitecture = false;
    for (const auto& arch : commonArchitectures) {
        if (osInfo.architecture.find(arch) != std::string::npos) {
            hasKnownArchitecture = true;
            break;
        }
    }

    // Should have a known architecture or at least some architecture string
    EXPECT_TRUE(hasKnownArchitecture || !osInfo.architecture.empty());
}

TEST_F(RealOSTest, OSNameValidation) {
    // Test OS name validation
    OperatingSystemInfo osInfo = getOperatingSystemInfo();

    // Common OS names
    std::vector<std::string> commonOSNames = {
        "Windows", "Linux",  "macOS", "Darwin",  "Ubuntu",  "CentOS", "RedHat",
        "Debian",  "Fedora", "SUSE",  "FreeBSD", "OpenBSD", "NetBSD"};

    bool hasKnownOS = false;
    for (const auto& osName : commonOSNames) {
        if (osInfo.osName.find(osName) != std::string::npos) {
            hasKnownOS = true;
            break;
        }
    }

    // Should have a known OS or at least some OS name
    EXPECT_TRUE(hasKnownOS || !osInfo.osName.empty());
}

// ============================================================================
// Edge Cases and Error Handling Tests
// ============================================================================

TEST_F(RealOSTest, NoThrowGuarantee) {
    // Test that all OS functions provide no-throw guarantee
    EXPECT_NO_THROW(getOperatingSystemInfo());
    EXPECT_NO_THROW(getSystemUptime());
    EXPECT_NO_THROW(getSystemLanguage());
    EXPECT_NO_THROW(getSystemEncoding());
    EXPECT_NO_THROW(getSystemLanguageInfo());
}

TEST_F(RealOSTest, StringFieldValidation) {
    // Test that string fields don't contain null characters
    OperatingSystemInfo osInfo = getOperatingSystemInfo();
    std::string language = getSystemLanguage();
    std::string encoding = getSystemEncoding();
    LocaleInfo localeInfo = getSystemLanguageInfo();

    // OS info string validation
    EXPECT_EQ(osInfo.osName.find('\0'), std::string::npos);
    EXPECT_EQ(osInfo.osVersion.find('\0'), std::string::npos);
    EXPECT_EQ(osInfo.architecture.find('\0'), std::string::npos);
    EXPECT_EQ(osInfo.computerName.find('\0'), std::string::npos);

    // Language info string validation
    EXPECT_EQ(language.find('\0'), std::string::npos);
    EXPECT_EQ(encoding.find('\0'), std::string::npos);
    EXPECT_EQ(localeInfo.languageCode.find('\0'), std::string::npos);
    EXPECT_EQ(localeInfo.characterEncoding.find('\0'), std::string::npos);

    // Should not contain newlines or carriage returns
    EXPECT_EQ(osInfo.osName.find('\n'), std::string::npos);
    EXPECT_EQ(osInfo.osName.find('\r'), std::string::npos);
    EXPECT_EQ(language.find('\n'), std::string::npos);
    EXPECT_EQ(language.find('\r'), std::string::npos);
}

TEST_F(RealOSTest, BoundaryConditions) {
    // Test boundary conditions

    // Uptime should be within reasonable bounds
    std::chrono::seconds uptime = getSystemUptime();
    EXPECT_GT(uptime.count(), 0);
    EXPECT_LT(uptime.count(), 365LL * 24 * 3600 * 100);  // Less than 100 years

    // String lengths should be reasonable
    OperatingSystemInfo osInfo = getOperatingSystemInfo();
    EXPECT_LT(osInfo.osName.length(), 1000);
    EXPECT_LT(osInfo.osVersion.length(), 1000);
    EXPECT_LT(osInfo.architecture.length(), 500);
    EXPECT_LT(osInfo.computerName.length(), 1000);

    std::string language = getSystemLanguage();
    std::string encoding = getSystemEncoding();
    EXPECT_LT(language.length(), 500);
    EXPECT_LT(encoding.length(), 500);

    LocaleInfo localeInfo = getSystemLanguageInfo();
    EXPECT_LT(localeInfo.languageCode.length(), 200);
    EXPECT_LT(localeInfo.localeName.length(), 500);
    EXPECT_LT(localeInfo.currencySymbol.length(), 50);
    EXPECT_LT(localeInfo.characterEncoding.length(), 200);
}

}  // namespace atom::sysinfo::test
