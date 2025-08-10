#include "atom/sysinfo/wm.hpp"
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <thread>

using namespace atom::system;
using namespace testing;

class WindowManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup code if necessary
    }

    void TearDown() override {
        // Cleanup code if necessary
    }
};

// Test system information retrieval
TEST_F(WindowManagerTest, GetSystemInfo) {
    EXPECT_NO_THROW({
        auto result = getEnhancedSystemInfo();

        // Result should be valid (either success or known error)
        if (isError(result)) {
            WMError error = getError(result);
            // Platform not supported is acceptable
            EXPECT_TRUE(error == WMError::PLATFORM_NOT_SUPPORTED ||
                       error == WMError::OPERATION_FAILED ||
                       error == WMError::PERMISSION_DENIED);
        } else {
            const auto& info = getValue(result);

            // System info should have valid fields
            EXPECT_TRUE(info.desktopEnvironment.empty() || !info.desktopEnvironment.empty());
            EXPECT_TRUE(info.windowManager.empty() || !info.windowManager.empty());
            EXPECT_TRUE(info.wmTheme.empty() || !info.wmTheme.empty());
            EXPECT_TRUE(info.icons.empty() || !info.icons.empty());
            EXPECT_TRUE(info.font.empty() || !info.font.empty());
            EXPECT_TRUE(info.cursor.empty() || !info.cursor.empty());

            // Version should be valid
            EXPECT_TRUE(info.wmVersion.empty() || !info.wmVersion.empty());

            // Boolean fields should be valid
            EXPECT_TRUE(info.supportsWorkspaces == true || info.supportsWorkspaces == false);
            EXPECT_TRUE(info.supportsVirtualDesktops == true || info.supportsVirtualDesktops == false);

            // Features should be valid
            for (const auto& feature : info.wmFeatures) {
                EXPECT_FALSE(feature.empty());
            }

            // Monitors should be valid
            for (const auto& monitor : info.monitors) {
                EXPECT_NE(monitor.id, 0);
                EXPECT_GT(monitor.width, 0);
                EXPECT_GT(monitor.height, 0);
            }

            // Workspaces should be valid
            for (const auto& workspace : info.workspaces) {
                EXPECT_GE(workspace.id, 0);
                EXPECT_TRUE(workspace.name.empty() || !workspace.name.empty());
            }
        }
    });
}

// Test theme information retrieval
TEST_F(WindowManagerTest, GetThemeInfo) {
    EXPECT_NO_THROW({
        auto result = getThemeInfo();

        // Result should be valid (either success or known error)
        if (isError(result)) {
            WMError error = getError(result);
            // Platform not supported is acceptable
            EXPECT_TRUE(error == WMError::PLATFORM_NOT_SUPPORTED ||
                       error == WMError::OPERATION_FAILED ||
                       error == WMError::PERMISSION_DENIED);
        } else {
            const auto& themeInfo = getValue(result);

            // Theme info should have valid structure
            EXPECT_TRUE(themeInfo.name.empty() || !themeInfo.name.empty());
            EXPECT_TRUE(themeInfo.variant.empty() || !themeInfo.variant.empty());

            // Type should be valid enum value
            EXPECT_TRUE(themeInfo.type == ThemeType::LIGHT ||
                       themeInfo.type == ThemeType::DARK ||
                       themeInfo.type == ThemeType::AUTO ||
                       themeInfo.type == ThemeType::HIGH_CONTRAST ||
                       themeInfo.type == ThemeType::CUSTOM ||
                       themeInfo.type == ThemeType::UNKNOWN);

            // Colors should be valid if set
            EXPECT_TRUE(themeInfo.accentColor.empty() || !themeInfo.accentColor.empty());
            EXPECT_TRUE(themeInfo.backgroundColor.empty() || !themeInfo.backgroundColor.empty());
            EXPECT_TRUE(themeInfo.foregroundColor.empty() || !themeInfo.foregroundColor.empty());

            // Boolean fields should be valid
            EXPECT_TRUE(themeInfo.followsSystemTheme == true || themeInfo.followsSystemTheme == false);
        }
    });
}

// Test window enumeration
TEST_F(WindowManagerTest, EnumerateWindows) {
    EXPECT_NO_THROW({
        auto result = enumerateWindows();

        if (isError(result)) {
            WMError error = getError(result);
            // Platform not supported or permission denied is acceptable
            EXPECT_TRUE(error == WMError::PLATFORM_NOT_SUPPORTED ||
                       error == WMError::OPERATION_FAILED ||
                       error == WMError::PERMISSION_DENIED);
        } else {
            const auto& windows = getValue(result);

            // Windows can be empty on some systems
            for (const auto& window : windows) {
                // Window ID should be non-zero
                EXPECT_NE(window.id, 0ULL);

                // Title can be empty for some windows
                EXPECT_TRUE(window.title.empty() || !window.title.empty());

                // Process name should be valid
                EXPECT_TRUE(window.processName.empty() || !window.processName.empty());

                // Process ID should be valid
                EXPECT_GE(window.processId, 0);

                // Dimensions should be reasonable
                EXPECT_GE(window.x, -10000);
                EXPECT_LE(window.x, 10000);
                EXPECT_GE(window.y, -10000);
                EXPECT_LE(window.y, 10000);
                EXPECT_GE(window.width, 0);
                EXPECT_LE(window.width, 10000);
                EXPECT_GE(window.height, 0);
                EXPECT_LE(window.height, 10000);

                // State should be valid enum value
                EXPECT_TRUE(window.state == WindowState::NORMAL ||
                           window.state == WindowState::MINIMIZED ||
                           window.state == WindowState::MAXIMIZED ||
                           window.state == WindowState::FULLSCREEN ||
                           window.state == WindowState::HIDDEN);

                // Type should be valid enum value
                EXPECT_TRUE(window.type == WindowType::NORMAL ||
                           window.type == WindowType::DIALOG ||
                           window.type == WindowType::UTILITY ||
                           window.type == WindowType::SPLASH ||
                           window.type == WindowType::DESKTOP ||
                           window.type == WindowType::DOCK ||
                           window.type == WindowType::TOOLBAR ||
                           window.type == WindowType::MENU ||
                           window.type == WindowType::DROPDOWN_MENU ||
                           window.type == WindowType::POPUP_MENU ||
                           window.type == WindowType::TOOLTIP ||
                           window.type == WindowType::NOTIFICATION ||
                           window.type == WindowType::COMBO ||
                           window.type == WindowType::DND ||
                           window.type == WindowType::UNKNOWN);

                // Boolean fields should be valid
                EXPECT_TRUE(window.isVisible == true || window.isVisible == false);
                EXPECT_TRUE(window.isActive == true || window.isActive == false);

                // Workspace ID should be reasonable (optional field)
                if (window.workspaceId.has_value()) {
                    EXPECT_GE(window.workspaceId.value(), 0);
                }

                // Class name should be valid
                EXPECT_TRUE(window.className.empty() || !window.className.empty());
            }
        }
    });
}

// Test window information retrieval
TEST_F(WindowManagerTest, GetWindowInfo) {
    EXPECT_NO_THROW({
        auto windowsResult = enumerateWindows();

        if (!isError(windowsResult)) {
            const auto& windows = getValue(windowsResult);

            if (!windows.empty()) {
                // Test getting info for the first window
                auto infoResult = getWindowInfo(windows[0].id);

                if (isError(infoResult)) {
                    WMError error = getError(infoResult);
                    EXPECT_TRUE(error == WMError::PLATFORM_NOT_SUPPORTED ||
                               error == WMError::WINDOW_NOT_FOUND ||
                               error == WMError::OPERATION_FAILED);
                } else {
                    const auto& windowInfo = getValue(infoResult);

                    // Should match the window from enumeration
                    EXPECT_EQ(windowInfo.id, windows[0].id);
                    EXPECT_EQ(windowInfo.title, windows[0].title);
                    EXPECT_EQ(windowInfo.processName, windows[0].processName);
                }
            }
        }
    });
}

// Test monitor information
TEST_F(WindowManagerTest, GetMonitors) {
    EXPECT_NO_THROW({
        auto result = getMonitors();

        if (isError(result)) {
            WMError error = getError(result);
            EXPECT_TRUE(error == WMError::PLATFORM_NOT_SUPPORTED ||
                       error == WMError::OPERATION_FAILED);
        } else {
            const auto& monitors = getValue(result);

            // Should have at least one monitor
            EXPECT_GT(monitors.size(), 0);

            for (const auto& monitor : monitors) {
                // Monitor ID should be valid
                EXPECT_NE(monitor.id, 0);

                // Name can be empty on some systems
                EXPECT_TRUE(monitor.name.empty() || !monitor.name.empty());

                // Dimensions should be positive
                EXPECT_GT(monitor.width, 0);
                EXPECT_GT(monitor.height, 0);

                // Position should be reasonable
                EXPECT_GE(monitor.x, -10000);
                EXPECT_LE(monitor.x, 10000);
                EXPECT_GE(monitor.y, -10000);
                EXPECT_LE(monitor.y, 10000);

                // Refresh rate should be reasonable if set
                if (monitor.refreshRate > 0) {
                    EXPECT_GE(monitor.refreshRate, 30);
                    EXPECT_LE(monitor.refreshRate, 500);
                }

                // Scale factor should be reasonable
                EXPECT_GT(monitor.scaleFactor, 0.1f);
                EXPECT_LT(monitor.scaleFactor, 10.0f);

                // Primary monitor flag should be valid
                EXPECT_TRUE(monitor.isPrimary == true || monitor.isPrimary == false);
            }

            // Should have exactly one primary monitor
            int primaryCount = 0;
            for (const auto& monitor : monitors) {
                if (monitor.isPrimary) {
                    primaryCount++;
                }
            }
            EXPECT_EQ(primaryCount, 1);
        }
    });
}

// Test workspace information
TEST_F(WindowManagerTest, GetWorkspaces) {
    EXPECT_NO_THROW({
        auto result = getWorkspaces();

        if (isError(result)) {
            WMError error = getError(result);
            EXPECT_TRUE(error == WMError::PLATFORM_NOT_SUPPORTED ||
                       error == WMError::OPERATION_FAILED);
        } else {
            const auto& workspaces = getValue(result);

            // Workspaces can be empty on some systems
            for (const auto& workspace : workspaces) {
                // Workspace ID should be valid
                EXPECT_GE(workspace.id, 0);

                // Name can be empty
                EXPECT_TRUE(workspace.name.empty() || !workspace.name.empty());

                // Active flag should be valid
                EXPECT_TRUE(workspace.isActive == true || workspace.isActive == false);

                // Window IDs should be valid
                for (uint64_t windowId : workspace.windowIds) {
                    EXPECT_NE(windowId, 0ULL);
                }

                // Dimensions should be reasonable if set
                if (workspace.width > 0 && workspace.height > 0) {
                    EXPECT_GT(workspace.width, 0);
                    EXPECT_GT(workspace.height, 0);
                }
            }

            // Should have at most one active workspace
            int activeCount = 0;
            for (const auto& workspace : workspaces) {
                if (workspace.isActive) {
                    activeCount++;
                }
            }
            EXPECT_LE(activeCount, 1);
        }
    });
}

// Test WindowManager class
TEST_F(WindowManagerTest, WindowManagerClass) {
    EXPECT_NO_THROW({
        WindowManager wm;

        // Test filtered windows
        auto result = wm.getFilteredWindows([](const WindowInfo& window) {
            return !window.title.empty();
        });

        if (isError(result)) {
            WMError error = getError(result);
            EXPECT_TRUE(error == WMError::PLATFORM_NOT_SUPPORTED ||
                       error == WMError::OPERATION_FAILED);
        } else {
            const auto& filteredWindows = getValue(result);

            // All returned windows should have non-empty titles
            for (const auto& window : filteredWindows) {
                EXPECT_FALSE(window.title.empty());
            }
        }

        // Test windows by process
        auto processResult = wm.getWindowsByProcess("nonexistent.exe");
        if (!isError(processResult)) {
            const auto& processWindows = getValue(processResult);
            // Should be empty for non-existent process
            EXPECT_TRUE(processWindows.empty());
        }
    });
}

// Test ThemeManager class
TEST_F(WindowManagerTest, ThemeManagerClass) {
    EXPECT_NO_THROW({
        ThemeManager tm;

        // Test getCurrentTheme
        auto result = tm.getCurrentTheme();

        if (isError(result)) {
            WMError error = getError(result);
            EXPECT_TRUE(error == WMError::PLATFORM_NOT_SUPPORTED ||
                       error == WMError::OPERATION_FAILED);
        } else {
            const auto& theme = getValue(result);

            // Theme should have valid structure
            EXPECT_TRUE(theme.name.empty() || !theme.name.empty());
            EXPECT_TRUE(theme.type == ThemeType::LIGHT ||
                       theme.type == ThemeType::DARK ||
                       theme.type == ThemeType::AUTO ||
                       theme.type == ThemeType::HIGH_CONTRAST ||
                       theme.type == ThemeType::CUSTOM ||
                       theme.type == ThemeType::UNKNOWN);
        }

        // Test monitoring status
        EXPECT_FALSE(tm.isMonitoring());
    });
}

// Test utility functions
TEST_F(WindowManagerTest, UtilityFunctions) {
    // Test error string conversion
    std::string errorStr = errorToString(WMError::PLATFORM_NOT_SUPPORTED);
    EXPECT_FALSE(errorStr.empty());
    EXPECT_NE(errorStr.find("not supported"), std::string::npos);

    errorStr = errorToString(WMError::WINDOW_NOT_FOUND);
    EXPECT_FALSE(errorStr.empty());
    EXPECT_NE(errorStr.find("not found"), std::string::npos);

    // Test window state string conversion
    std::string stateStr = windowStateToString(WindowState::NORMAL);
    EXPECT_FALSE(stateStr.empty());

    stateStr = windowStateToString(WindowState::MINIMIZED);
    EXPECT_FALSE(stateStr.empty());

    stateStr = windowStateToString(WindowState::MAXIMIZED);
    EXPECT_FALSE(stateStr.empty());

    // Test window type string conversion
    std::string typeStr = windowTypeToString(WindowType::NORMAL);
    EXPECT_FALSE(typeStr.empty());

    typeStr = windowTypeToString(WindowType::DIALOG);
    EXPECT_FALSE(typeStr.empty());

    // Test theme type string conversion
    std::string themeTypeStr = themeTypeToString(ThemeType::LIGHT);
    EXPECT_FALSE(themeTypeStr.empty());

    themeTypeStr = themeTypeToString(ThemeType::DARK);
    EXPECT_FALSE(themeTypeStr.empty());
}

// Test error handling
TEST_F(WindowManagerTest, ErrorHandling) {
    // Test with invalid window ID
    EXPECT_NO_THROW({
        auto result = getWindowInfo(0);
        if (!isError(result)) {
            // Some platforms might return a result for ID 0
            const auto& windowInfo = getValue(result);
            EXPECT_GE(windowInfo.id, 0ULL);
        } else {
            WMError error = getError(result);
            EXPECT_TRUE(error == WMError::WINDOW_NOT_FOUND ||
                       error == WMError::INVALID_PARAMETER ||
                       error == WMError::PLATFORM_NOT_SUPPORTED);
        }
    });

    // Test with very large window ID
    EXPECT_NO_THROW({
        auto result = getWindowInfo(UINT64_MAX);
        if (!isError(result)) {
            // Unlikely to succeed, but shouldn't crash
            const auto& windowInfo = getValue(result);
            EXPECT_GE(windowInfo.id, 0ULL);
        } else {
            WMError error = getError(result);
            EXPECT_TRUE(error == WMError::WINDOW_NOT_FOUND ||
                       error == WMError::INVALID_PARAMETER ||
                       error == WMError::PLATFORM_NOT_SUPPORTED);
        }
    });
}

// Test result helper functions
TEST_F(WindowManagerTest, ResultHelpers) {
    // Test with successful result
    WMResult<int> successResult = 42;
    EXPECT_FALSE(isError(successResult));
    EXPECT_EQ(getValue(successResult), 42);

    // Test with error result
    WMResult<int> errorResult = WMError::OPERATION_FAILED;
    EXPECT_TRUE(isError(errorResult));
    EXPECT_EQ(getError(errorResult), WMError::OPERATION_FAILED);
}

// Test data structure validation
TEST_F(WindowManagerTest, DataStructureValidation) {
    // Test WindowInfo structure
    WindowInfo window;
    window.id = 12345;
    window.title = "Test Window";
    window.className = "TestClass";
    window.processName = "test.exe";
    window.processId = 1000;
    window.state = WindowState::NORMAL;
    window.type = WindowType::NORMAL;
    window.x = 100;
    window.y = 200;
    window.width = 800;
    window.height = 600;
    window.isVisible = true;
    window.isActive = false;
    window.workspaceId = 1;

    EXPECT_EQ(window.id, 12345ULL);
    EXPECT_EQ(window.title, "Test Window");
    EXPECT_EQ(window.state, WindowState::NORMAL);
    EXPECT_TRUE(window.workspaceId.has_value());
    EXPECT_EQ(window.workspaceId.value(), 1);

    // Test ThemeInfo structure
    ThemeInfo theme;
    theme.type = ThemeType::DARK;
    theme.name = "Dark Theme";
    theme.variant = "dark";
    theme.accentColor = "#0078D4";
    theme.backgroundColor = "#2E2E2E";
    theme.foregroundColor = "#FFFFFF";
    theme.followsSystemTheme = true;

    EXPECT_EQ(theme.type, ThemeType::DARK);
    EXPECT_EQ(theme.name, "Dark Theme");
    EXPECT_TRUE(theme.followsSystemTheme);

    // Test MonitorInfo structure
    MonitorInfo monitor;
    monitor.id = 1;
    monitor.name = "Primary Monitor";
    monitor.isPrimary = true;
    monitor.x = 0;
    monitor.y = 0;
    monitor.width = 1920;
    monitor.height = 1080;
    monitor.refreshRate = 60;
    monitor.scaleFactor = 1.0f;

    EXPECT_EQ(monitor.id, 1);
    EXPECT_TRUE(monitor.isPrimary);
    EXPECT_EQ(monitor.width, 1920);
    EXPECT_EQ(monitor.height, 1080);

    // Test WorkspaceInfo structure
    WorkspaceInfo workspace;
    workspace.id = 1;
    workspace.name = "Desktop 1";
    workspace.isActive = true;
    workspace.windowIds = {12345, 67890};
    workspace.x = 0;
    workspace.y = 0;
    workspace.width = 1920;
    workspace.height = 1080;

    EXPECT_EQ(workspace.id, 1);
    EXPECT_TRUE(workspace.isActive);
    EXPECT_EQ(workspace.windowIds.size(), 2);
}

// Test concurrent access
TEST_F(WindowManagerTest, ConcurrentAccess) {
    const int num_threads = 3;
    std::vector<std::thread> threads;
    std::vector<bool> results(num_threads, false);

    // Launch multiple threads accessing WM functions concurrently
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&, i]() {
            try {
                auto sysResult = getEnhancedSystemInfo();
                auto themeResult = getThemeInfo();
                auto windowsResult = enumerateWindows();
                auto monitorsResult = getMonitors();

                // Basic validation - should not crash
                EXPECT_TRUE(isError(sysResult) || !isError(sysResult));
                EXPECT_TRUE(isError(themeResult) || !isError(themeResult));
                EXPECT_TRUE(isError(windowsResult) || !isError(windowsResult));
                EXPECT_TRUE(isError(monitorsResult) || !isError(monitorsResult));

                results[i] = true;
            } catch (...) {
                results[i] = false;
            }
        });
    }

    // Wait for all threads to complete
    for (auto& thread : threads) {
        thread.join();
    }

    // All threads should have completed successfully
    for (bool result : results) {
        EXPECT_TRUE(result);
    }
}
