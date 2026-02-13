#include "printer_system.hpp"
#include "printer_exceptions.hpp"

#ifdef PRINT_SYSTEM_WINDOWS
#include "printer_system_windows.hpp"
#elif defined(PRINT_SYSTEM_LINUX)
#include "printer_system_linux.hpp"
#elif defined(PRINT_SYSTEM_MACOS)
#include "printer_system_macos.hpp"
#endif

namespace print_system {

// Static instance for the PrintManager singleton
static std::unique_ptr<PrintManager> s_instance;
static std::once_flag s_init_flag;

// Create the platform-specific implementation
std::unique_ptr<PrintManager> PrintManager::create() {
#ifdef PRINT_SYSTEM_WINDOWS
    return std::make_unique<WindowsPrintManager>();
#elif defined(PRINT_SYSTEM_LINUX)
    return std::make_unique<LinuxPrintManager>();
#elif defined(PRINT_SYSTEM_MACOS)
    // Not implemented yet
    throw PrintSystemInitException("macOS printing not implemented");
#else
    throw PrintSystemInitException("Unknown platform");
#endif
}

// Get the singleton instance
PrintManager& PrintManager::getInstance() {
    std::call_once(s_init_flag, []() {
        try {
            s_instance = PrintManager::create();
        }
        catch (const std::exception& e) {
            throw PrintSystemInitException(e.what());
        }
    });

    if (!s_instance) {
        throw PrintSystemInitException("Failed to initialize print system");
    }

    return *s_instance;
}

} // namespace print_system
