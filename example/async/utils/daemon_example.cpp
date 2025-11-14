/**
 * @file daemon_advanced.cpp
 * @brief Advanced daemon utilities demonstration (foreground-safe)
 *
 * Demonstrates:
 *  - DaemonGuard in foreground mode (isDaemon=false)
 *  - PID file management (setPidFilePath, checkPidFile, cleanup)
 *  - Signal registration (platform-guarded)
 *  - isProcessBackground()
 *  - Restart interval configuration (set/get)
 *  - DaemonGuard::toString(), getRestartCount()
 */

#include <atom/async/utils/daemon.hpp>

#include <array>
#include <chrono>
#include <csignal>
#include <filesystem>
#include <iostream>
#include <optional>
#include <string>
#include <thread>
#include <vector>

using namespace std::chrono_literals;
namespace fs = std::filesystem;

static std::string pidPath() {
#if defined(_WIN32)
    return "./lithium-daemon.pid";  // Windows: avoid temp perms issues
#else
    return "./lithium-daemon.pid";  // keep near cwd for CI simplicity
#endif
}

void log(const std::string& tag, const std::string& msg) {
    std::cout << "[" << tag << "] " << msg << std::endl;
}

// The worker callback used by DaemonGuard::startDaemon
static int worker_main(int /*argc*/, char** /*argv*/) {
    log("worker", "running main task in foreground mode");
    // Simulate short work
    std::this_thread::sleep_for(50ms);
    log("worker", "work complete");
    return 0;
}

int main(int argc, char** argv) {
    log("daemon", "advanced demo starting (foreground-safe)");

    // Configure restart interval
    atom::async::setDaemonRestartInterval(5);
    int ri = atom::async::getDaemonRestartInterval();
    log("daemon", std::string("restartInterval=") + std::to_string(ri) + "s");

    // Register signal handlers (best-effort, platform-guarded)
#if defined(SIGINT)
    try {
        std::array<int, 2> sigs{SIGINT, SIGTERM};
        bool ok = atom::async::registerSignalHandlers(sigs);
        log("daemon", ok ? "signal handlers registered for SIGINT/SIGTERM"
                         : "signal registration returned false");
    } catch (...) {
        log("daemon", "signal registration not supported on this platform");
    }
#endif

    // Prepare guard and PID file path
    atom::async::DaemonGuard guard;
    const auto path = pidPath();
    guard.setPidFilePath(path);

    log("daemon", std::string("isProcessBackground=") +
                      (atom::async::isProcessBackground() ? "true" : "false"));
    log("daemon", std::string("guard(before).toString= ") + guard.toString());

    // Run in foreground (isDaemon=false); this will also write the PID file
    int rc = guard.startDaemon(argc, argv, worker_main, /*isDaemon=*/false);
    log("daemon", std::string("startDaemon rc=") + std::to_string(rc));

    // Inspect guard state
    log("daemon", std::string("guard(after).toString= ") + guard.toString());
    log("daemon",
        std::string("restartCount=") + std::to_string(guard.getRestartCount()));

    // Verify PID file exists
    try {
        bool exists = atom::async::checkPidFile(path);
        log("daemon", exists ? "pid file OK" : "pid file not found or invalid");
    } catch (const std::exception& e) {
        log("daemon", std::string("pid file check error: ") + e.what());
    }

    // Cleanup PID file (foreground demo should tidy up)
    if (fs::exists(path)) {
        std::error_code ec;
        fs::remove(path, ec);
        log("daemon",
            std::string("pid file removed: ") + (ec ? ec.message() : "ok"));
    }

    log("daemon", "advanced demo done");
    return rc;
}
