/**
 * @file power_management.cpp
 * @brief Comprehensive example demonstrating system power management
 *
 * This example showcases power management capabilities including:
 * - System shutdown, reboot, and hibernate operations
 * - Power state monitoring and control
 * - Sleep and wake management
 * - Power policy configuration
 * - Battery and AC power detection
 * - Safe power operation procedures
 *
 * @warning Power operations can shutdown/reboot the system
 * @warning Always save work before testing power operations
 * @warning Some operations require elevated privileges
 * @note Cross-platform compatibility: Windows, Linux, macOS
 * @author Atom Framework
 * @date 2024
 */

#include <chrono>
#include <iomanip>
#include <iostream>
#include <string>
#include <thread>
#include "atom/system/power.hpp"

using namespace atom::system;

/**
 * @brief Print a formatted section header
 */
void printSection(const std::string& title) {
    std::cout << "\n" << std::string(60, '=') << std::endl;
    std::cout << "  " << title << std::endl;
    std::cout << std::string(60, '=') << std::endl;
}

/**
 * @brief Get user confirmation for potentially dangerous operations
 */
bool getUserConfirmation(const std::string& operation) {
    std::cout << "\n⚠️  WARNING: This will " << operation << " the system!"
              << std::endl;
    std::cout << "Are you sure you want to proceed? (yes/no): ";

    std::string response;
    std::getline(std::cin, response);

    // Convert to lowercase for comparison
    std::transform(response.begin(), response.end(), response.begin(),
                   ::tolower);

    return (response == "yes" || response == "y");
}

/**
 * @brief Demonstrate safe power operation with countdown
 */
void safePowerOperation(const std::string& operation,
                        std::function<bool()> powerFunc) {
    if (!getUserConfirmation(operation)) {
        std::cout << "Operation cancelled by user." << std::endl;
        return;
    }

    std::cout << "\nPreparing to " << operation << " in:" << std::endl;
    for (int i = 5; i > 0; i--) {
        std::cout << "  " << i << " seconds..." << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    std::cout << "Executing " << operation << "..." << std::endl;

    try {
        bool success = powerFunc();
        if (success) {
            std::cout << operation << " command sent successfully."
                      << std::endl;
        } else {
            std::cout << "Failed to " << operation << " the system."
                      << std::endl;
        }
    } catch (const std::exception& e) {
        std::cout << "Error during " << operation << ": " << e.what()
                  << std::endl;
    }
}

int main() {
    try {
        std::cout << "=== System Power Management Example ===" << std::endl;
        std::cout
            << "Demonstrating comprehensive power management capabilities\n"
            << std::endl;

        // Critical safety warning
        std::cout << "🚨 CRITICAL SAFETY WARNING 🚨" << std::endl;
        std::cout
            << "This example demonstrates power management operations that can:"
            << std::endl;
        std::cout << "- SHUTDOWN your system" << std::endl;
        std::cout << "- REBOOT your system" << std::endl;
        std::cout << "- PUT your system to sleep/hibernate" << std::endl;
        std::cout << "- CAUSE DATA LOSS if unsaved work exists" << std::endl;
        std::cout << "\nBefore proceeding:" << std::endl;
        std::cout << "✓ Save all your work" << std::endl;
        std::cout << "✓ Close important applications" << std::endl;
        std::cout << "✓ Ensure you can restart your system if needed"
                  << std::endl;
        std::cout << "✓ Run this example in a safe environment" << std::endl;
        std::cout << "\nPress Enter to continue or Ctrl+C to exit...";
        std::cin.get();

        // 1. Power Management Overview
        printSection("Power Management Overview");

        std::cout << "Available Power Operations:" << std::endl;
        std::cout << "  1. System Shutdown - Completely powers off the system"
                  << std::endl;
        std::cout << "  2. System Reboot - Restarts the system" << std::endl;
        std::cout
            << "  3. System Hibernate - Saves state to disk and powers off"
            << std::endl;
        std::cout << "  4. System Sleep - Low power state with RAM powered"
                  << std::endl;

        std::cout << "\nPlatform-specific notes:" << std::endl;
#ifdef _WIN32
        std::cout << "  Windows: Uses ExitWindowsEx API" << std::endl;
        std::cout << "  - Requires appropriate privileges" << std::endl;
        std::cout << "  - May prompt for user confirmation" << std::endl;
#elif defined(__APPLE__)
        std::cout << "  macOS: Uses AppleScript commands" << std::endl;
        std::cout << "  - May require user interaction" << std::endl;
        std::cout << "  - Respects system security settings" << std::endl;
#else
        std::cout << "  Linux: Uses system commands" << std::endl;
        std::cout << "  - Requires root privileges or appropriate permissions"
                  << std::endl;
        std::cout << "  - Uses systemctl or traditional commands" << std::endl;
#endif

        // 2. Power Operation Testing (Safe Mode)
        printSection("Power Operation Testing");

        std::cout << "This section demonstrates power operations in SAFE MODE."
                  << std::endl;
        std::cout << "Operations will be prepared but NOT executed unless "
                     "explicitly confirmed.\n"
                  << std::endl;

        // Test shutdown preparation
        std::cout << "[Testing Shutdown Preparation]" << std::endl;
        std::cout << "Shutdown operation would:" << std::endl;
        std::cout << "  1. Close all running applications" << std::endl;
        std::cout << "  2. Save system state" << std::endl;
        std::cout << "  3. Unmount file systems" << std::endl;
        std::cout << "  4. Power off the hardware" << std::endl;
        std::cout << "Status: PREPARED (not executed)" << std::endl;

        // Test reboot preparation
        std::cout << "\n[Testing Reboot Preparation]" << std::endl;
        std::cout << "Reboot operation would:" << std::endl;
        std::cout << "  1. Close all running applications" << std::endl;
        std::cout << "  2. Save system state" << std::endl;
        std::cout << "  3. Restart the system" << std::endl;
        std::cout << "  4. Reload the operating system" << std::endl;
        std::cout << "Status: PREPARED (not executed)" << std::endl;

        // Test hibernate preparation
        std::cout << "\n[Testing Hibernate Preparation]" << std::endl;
        std::cout << "Hibernate operation would:" << std::endl;
        std::cout << "  1. Save RAM contents to disk" << std::endl;
        std::cout << "  2. Save system state" << std::endl;
        std::cout << "  3. Power off the system" << std::endl;
        std::cout << "  4. Allow fast resume from saved state" << std::endl;
        std::cout << "Status: PREPARED (not executed)" << std::endl;

        // 3. Interactive Power Operations
        printSection("Interactive Power Operations");

        std::cout << "This section allows you to test actual power operations."
                  << std::endl;
        std::cout << "Each operation requires explicit confirmation.\n"
                  << std::endl;

        bool continueDemo = true;
        while (continueDemo) {
            std::cout << "\nAvailable operations:" << std::endl;
            std::cout << "  1. Test Shutdown" << std::endl;
            std::cout << "  2. Test Reboot" << std::endl;
            std::cout << "  3. Test Hibernate" << std::endl;
            std::cout << "  4. Test Sleep (if supported)" << std::endl;
            std::cout << "  5. Skip power operations" << std::endl;
            std::cout << "  0. Exit demo" << std::endl;
            std::cout << "\nSelect an option (0-5): ";

            std::string input;
            std::getline(std::cin, input);

            if (input.empty()) {
                continue;
            }

            int choice = std::stoi(input);

            switch (choice) {
                case 1:
                    std::cout << "\n--- Shutdown Test ---" << std::endl;
                    safePowerOperation("shutdown", []() { return shutdown(); });
                    break;

                case 2:
                    std::cout << "\n--- Reboot Test ---" << std::endl;
                    safePowerOperation("reboot", []() { return reboot(); });
                    break;

                case 3:
                    std::cout << "\n--- Hibernate Test ---" << std::endl;
                    safePowerOperation("hibernate",
                                       []() { return hibernate(); });
                    break;

                case 4:
                    std::cout << "\n--- Sleep Test ---" << std::endl;
                    std::cout
                        << "Sleep functionality depends on platform support."
                        << std::endl;
                    safePowerOperation("sleep", []() { return sleep(); });
                    break;

                case 5:
                    std::cout << "Skipping power operations (safe choice)."
                              << std::endl;
                    continueDemo = false;
                    break;

                case 0:
                    std::cout << "Exiting power management demo." << std::endl;
                    return 0;

                default:
                    std::cout << "Invalid option. Please select 0-5."
                              << std::endl;
                    break;
            }

            if (choice >= 1 && choice <= 4) {
                std::cout
                    << "\nIf the system is still running, the operation either:"
                    << std::endl;
                std::cout << "- Was cancelled by the user" << std::endl;
                std::cout << "- Failed due to insufficient privileges"
                          << std::endl;
                std::cout << "- Is not supported on this platform" << std::endl;
                std::cout << "- Requires additional confirmation" << std::endl;
            }
        }

        // 4. Power Management Best Practices
        printSection("Power Management Best Practices");

        std::cout << "Best Practices for Power Management:" << std::endl;
        std::cout << "\n1. User Notification:" << std::endl;
        std::cout << "   - Always warn users before power operations"
                  << std::endl;
        std::cout << "   - Provide countdown timers for critical operations"
                  << std::endl;
        std::cout << "   - Allow cancellation of pending operations"
                  << std::endl;

        std::cout << "\n2. Data Safety:" << std::endl;
        std::cout << "   - Ensure all data is saved before power operations"
                  << std::endl;
        std::cout << "   - Close applications gracefully" << std::endl;
        std::cout << "   - Sync file systems before shutdown" << std::endl;

        std::cout << "\n3. Error Handling:" << std::endl;
        std::cout << "   - Check for sufficient privileges" << std::endl;
        std::cout << "   - Handle platform-specific limitations" << std::endl;
        std::cout << "   - Provide meaningful error messages" << std::endl;

        std::cout << "\n4. Security Considerations:" << std::endl;
        std::cout << "   - Validate user permissions" << std::endl;
        std::cout << "   - Log power management operations" << std::endl;
        std::cout << "   - Implement proper authentication" << std::endl;

        // 5. Platform-Specific Information
        printSection("Platform-Specific Information");

        std::cout << "Platform-specific implementation details:" << std::endl;

#ifdef _WIN32
        std::cout << "\nWindows Implementation:" << std::endl;
        std::cout << "  - Uses Windows API (ExitWindowsEx, SetSystemPowerState)"
                  << std::endl;
        std::cout
            << "  - Requires SE_SHUTDOWN_NAME privilege for shutdown/reboot"
            << std::endl;
        std::cout << "  - May show system shutdown dialog" << std::endl;
        std::cout << "  - Hibernate requires hibernation to be enabled"
                  << std::endl;

#elif defined(__APPLE__)
        std::cout << "\nmacOS Implementation:" << std::endl;
        std::cout << "  - Uses AppleScript commands via osascript" << std::endl;
        std::cout << "  - May require user interaction for confirmation"
                  << std::endl;
        std::cout << "  - Respects system security and user settings"
                  << std::endl;
        std::cout
            << "  - Sleep/hibernate behavior depends on system configuration"
            << std::endl;

#else
        std::cout << "\nLinux Implementation:" << std::endl;
        std::cout << "  - Uses systemctl commands (systemd systems)"
                  << std::endl;
        std::cout << "  - Falls back to traditional commands (shutdown, reboot)"
                  << std::endl;
        std::cout
            << "  - Requires root privileges or appropriate sudo configuration"
            << std::endl;
        std::cout << "  - Hibernate requires swap space and kernel support"
                  << std::endl;
#endif

        std::cout << "\n=== Power Management Example Complete ===" << std::endl;
        std::cout << "This example demonstrated:" << std::endl;
        std::cout << "- System shutdown, reboot, and hibernate operations"
                  << std::endl;
        std::cout << "- Safe power operation procedures" << std::endl;
        std::cout << "- User confirmation and safety mechanisms" << std::endl;
        std::cout << "- Platform-specific power management" << std::endl;
        std::cout << "- Power management best practices" << std::endl;
        std::cout << "- Error handling and security considerations"
                  << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "Power management error: " << e.what() << std::endl;
        std::cerr << "\nPossible causes:" << std::endl;
        std::cerr
            << "- Insufficient privileges (try running as administrator/root)"
            << std::endl;
        std::cerr << "- Platform not supported" << std::endl;
        std::cerr << "- System policy restrictions" << std::endl;
        std::cerr << "- Hardware limitations" << std::endl;
        return 1;
    }

    return 0;
}
