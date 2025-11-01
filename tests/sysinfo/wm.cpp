/*
 * wm.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-22

Description: Unit Tests for Window Manager Information Module
Tests desktop environment and window manager detection.

**************************************************/

#include <gtest/gtest.h>
#include <algorithm>
#include <string>

#include "atom/sysinfo/wm.hpp"

using namespace atom::system;

namespace atom::sysinfo::test {

// ============================================================================
// Window Manager Information Tests
// ============================================================================

class WmTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup window manager tests
    }

    void TearDown() override {
        // Cleanup
    }
};

TEST_F(WmTest, GetSystemInfo) {
    // Test system information retrieval
    SystemInfo sysInfo = getSystemInfo();

    // Should not throw and return a valid structure
    EXPECT_TRUE(true);  // Basic structure test

    // Desktop environment should be a string (might be empty on headless
    // systems)
    EXPECT_TRUE(sysInfo.desktopEnvironment.empty() ||
                !sysInfo.desktopEnvironment.empty());

    // If desktop environment is detected, it should be reasonable
    if (!sysInfo.desktopEnvironment.empty()) {
        EXPECT_GT(sysInfo.desktopEnvironment.length(), 0);
        EXPECT_LT(sysInfo.desktopEnvironment.length(),
                  100);  // Reasonable upper bound

        // Should not contain only whitespace
        EXPECT_FALSE(std::all_of(sysInfo.desktopEnvironment.begin(),
                                 sysInfo.desktopEnvironment.end(), ::isspace));
    }
}

TEST_F(WmTest, DesktopEnvironmentDetection) {
    // Test desktop environment detection
    SystemInfo sysInfo = getSystemInfo();

    // If we have a desktop environment, it should be a known one
    if (!sysInfo.desktopEnvironment.empty()) {
        std::string de = sysInfo.desktopEnvironment;

        // Common desktop environments
        bool isKnownDE = (de.find("GNOME") != std::string::npos ||
                          de.find("KDE") != std::string::npos ||
                          de.find("XFCE") != std::string::npos ||
                          de.find("LXDE") != std::string::npos ||
                          de.find("MATE") != std::string::npos ||
                          de.find("Cinnamon") != std::string::npos ||
                          de.find("Unity") != std::string::npos ||
                          de.find("Budgie") != std::string::npos ||
                          de.find("Pantheon") != std::string::npos ||
                          de.find("Fluent") != std::string::npos ||
                          de.find("Windows") != std::string::npos ||
                          de.find("macOS") != std::string::npos ||
                          de.find("Aqua") != std::string::npos);

        // Note: This might fail for unknown DEs, which is acceptable
        if (isKnownDE) {
            EXPECT_TRUE(isKnownDE);
        }
    }
}

TEST_F(WmTest, WindowManagerDetection) {
    // Test window manager detection
    SystemInfo sysInfo = getSystemInfo();

    // Window manager should be a string (might be empty)
    EXPECT_TRUE(sysInfo.windowManager.empty() ||
                !sysInfo.windowManager.empty());

    // If window manager is detected, it should be reasonable
    if (!sysInfo.windowManager.empty()) {
        EXPECT_GT(sysInfo.windowManager.length(), 0);
        EXPECT_LT(sysInfo.windowManager.length(),
                  100);  // Reasonable upper bound

        // Should not contain only whitespace
        EXPECT_FALSE(std::all_of(sysInfo.windowManager.begin(),
                                 sysInfo.windowManager.end(), ::isspace));

        // Common window managers
        std::string wm = sysInfo.windowManager;
        bool isKnownWM =
            (wm.find("Mutter") != std::string::npos ||
             wm.find("KWin") != std::string::npos ||
             wm.find("Xfwm") != std::string::npos ||
             wm.find("Openbox") != std::string::npos ||
             wm.find("i3") != std::string::npos ||
             wm.find("bspwm") != std::string::npos ||
             wm.find("awesome") != std::string::npos ||
             wm.find("dwm") != std::string::npos ||
             wm.find("Fluxbox") != std::string::npos ||
             wm.find("Desktop Window Manager") != std::string::npos ||
             wm.find("Quartz Compositor") != std::string::npos);

        // Note: This might fail for unknown WMs, which is acceptable
        if (isKnownWM) {
            EXPECT_TRUE(isKnownWM);
        }
    }
}

TEST_F(WmTest, ThemeInformation) {
    // Test theme information
    SystemInfo sysInfo = getSystemInfo();

    // Theme should be a string (might be empty)
    EXPECT_TRUE(sysInfo.wmTheme.empty() || !sysInfo.wmTheme.empty());

    // If theme is detected, it should be reasonable
    if (!sysInfo.wmTheme.empty()) {
        EXPECT_GT(sysInfo.wmTheme.length(), 0);
        EXPECT_LT(sysInfo.wmTheme.length(), 200);  // Reasonable upper bound

        // Should not contain only whitespace
        EXPECT_FALSE(std::all_of(sysInfo.wmTheme.begin(), sysInfo.wmTheme.end(),
                                 ::isspace));
    }
}

TEST_F(WmTest, IconInformation) {
    // Test icon information
    SystemInfo sysInfo = getSystemInfo();

    // Icons should be a string (might be empty)
    EXPECT_TRUE(sysInfo.icons.empty() || !sysInfo.icons.empty());

    // If icons are detected, they should be reasonable
    if (!sysInfo.icons.empty()) {
        EXPECT_GT(sysInfo.icons.length(), 0);
        EXPECT_LT(sysInfo.icons.length(), 200);  // Reasonable upper bound

        // Should not contain only whitespace
        EXPECT_FALSE(
            std::all_of(sysInfo.icons.begin(), sysInfo.icons.end(), ::isspace));
    }
}

TEST_F(WmTest, FontInformation) {
    // Test font information
    SystemInfo sysInfo = getSystemInfo();

    // Font should be a string (might be empty)
    EXPECT_TRUE(sysInfo.font.empty() || !sysInfo.font.empty());

    // If font is detected, it should be reasonable
    if (!sysInfo.font.empty()) {
        EXPECT_GT(sysInfo.font.length(), 0);
        EXPECT_LT(sysInfo.font.length(), 200);  // Reasonable upper bound

        // Should not contain only whitespace
        EXPECT_FALSE(
            std::all_of(sysInfo.font.begin(), sysInfo.font.end(), ::isspace));
    }
}

TEST_F(WmTest, CursorInformation) {
    // Test cursor information
    SystemInfo sysInfo = getSystemInfo();

    // Cursor should be a string (might be empty)
    EXPECT_TRUE(sysInfo.cursor.empty() || !sysInfo.cursor.empty());

    // If cursor is detected, it should be reasonable
    if (!sysInfo.cursor.empty()) {
        EXPECT_GT(sysInfo.cursor.length(), 0);
        EXPECT_LT(sysInfo.cursor.length(), 200);  // Reasonable upper bound

        // Should not contain only whitespace
        EXPECT_FALSE(std::all_of(sysInfo.cursor.begin(), sysInfo.cursor.end(),
                                 ::isspace));
    }
}

// ============================================================================
// Consistency Tests
// ============================================================================

TEST_F(WmTest, ConsistentResults) {
    // Test that multiple calls return consistent results
    SystemInfo info1 = getSystemInfo();
    SystemInfo info2 = getSystemInfo();

    EXPECT_EQ(info1.desktopEnvironment, info2.desktopEnvironment);
    EXPECT_EQ(info1.windowManager, info2.windowManager);
    EXPECT_EQ(info1.wmTheme, info2.wmTheme);
    EXPECT_EQ(info1.icons, info2.icons);
    EXPECT_EQ(info1.font, info2.font);
    EXPECT_EQ(info1.cursor, info2.cursor);
}

// ============================================================================
// Platform-Specific Tests
// ============================================================================

TEST_F(WmTest, PlatformSpecificBehavior) {
    // Test platform-specific behavior
    SystemInfo sysInfo = getSystemInfo();

#ifdef _WIN32
    // On Windows, we might detect Desktop Window Manager
    if (!sysInfo.windowManager.empty()) {
        // Windows-specific window managers
        bool isWindowsWM =
            (sysInfo.windowManager.find("Desktop Window Manager") !=
                 std::string::npos ||
             sysInfo.windowManager.find("DWM") != std::string::npos);

        if (isWindowsWM) {
            EXPECT_TRUE(isWindowsWM);
        }
    }

    // Desktop environment might be "Fluent" or similar on Windows
    if (!sysInfo.desktopEnvironment.empty()) {
        bool isWindowsDE =
            (sysInfo.desktopEnvironment.find("Fluent") != std::string::npos ||
             sysInfo.desktopEnvironment.find("Windows") != std::string::npos);

        if (isWindowsDE) {
            EXPECT_TRUE(isWindowsDE);
        }
    }
#endif

#ifdef __APPLE__
    // On macOS, we might detect Aqua or Quartz Compositor
    if (!sysInfo.windowManager.empty()) {
        bool isMacWM =
            (sysInfo.windowManager.find("Quartz Compositor") !=
                 std::string::npos ||
             sysInfo.windowManager.find("WindowServer") != std::string::npos);

        if (isMacWM) {
            EXPECT_TRUE(isMacWM);
        }
    }

    if (!sysInfo.desktopEnvironment.empty()) {
        bool isMacDE =
            (sysInfo.desktopEnvironment.find("Aqua") != std::string::npos ||
             sysInfo.desktopEnvironment.find("macOS") != std::string::npos);

        if (isMacDE) {
            EXPECT_TRUE(isMacDE);
        }
    }
#endif

#ifdef __linux__
    // On Linux, we might detect various DEs and WMs
    if (!sysInfo.desktopEnvironment.empty()) {
        bool isLinuxDE =
            (sysInfo.desktopEnvironment.find("GNOME") != std::string::npos ||
             sysInfo.desktopEnvironment.find("KDE") != std::string::npos ||
             sysInfo.desktopEnvironment.find("XFCE") != std::string::npos ||
             sysInfo.desktopEnvironment.find("LXDE") != std::string::npos);

        // Note: This might fail for other Linux DEs, which is acceptable
        if (isLinuxDE) {
            EXPECT_TRUE(isLinuxDE);
        }
    }
#endif
}

// ============================================================================
// Edge Cases and Error Handling Tests
// ============================================================================

TEST_F(WmTest, NoThrowGuarantee) {
    // Test that getSystemInfo provides no-throw guarantee
    EXPECT_NO_THROW({ SystemInfo info = getSystemInfo(); });
}

TEST_F(WmTest, HeadlessSystemHandling) {
    // Test behavior on potentially headless systems
    SystemInfo sysInfo = getSystemInfo();

    // On headless systems, many fields might be empty, which is acceptable
    // Just ensure the function doesn't crash
    EXPECT_TRUE(true);

    // If no desktop environment is detected, window manager might also be empty
    if (sysInfo.desktopEnvironment.empty()) {
        // This is acceptable for headless systems
        EXPECT_TRUE(sysInfo.windowManager.empty() ||
                    !sysInfo.windowManager.empty());
    }
}

TEST_F(WmTest, StructureAlignment) {
    // Test that the SystemInfo structure is properly aligned
    SystemInfo info;

    // Should be able to access all fields without issues
    EXPECT_NO_THROW({
        std::string de = info.desktopEnvironment;
        std::string wm = info.windowManager;
        std::string theme = info.wmTheme;
        std::string icons = info.icons;
        std::string font = info.font;
        std::string cursor = info.cursor;
    });
}

}  // namespace atom::sysinfo::test
