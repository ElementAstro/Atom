#include "atom/async/daemon.hpp"
#include <gtest/gtest.h>

TEST(DaemonGuardTest, ToStringTest) {
    atom::async::DaemonGuard daemonGuard;
    std::string expected;
    std::string actual = daemonGuard.toString();
    EXPECT_EQ(expected, actual);
}

TEST(DaemonGuardTest, RealStartTest) {
    int argc = 0;
    char **argv = nullptr;
    std::function<int(int argc, char **argv)> mainCb = nullptr;
    atom::async::DaemonGuard daemonGuard;
    int expected = 0;
    int actual = daemonGuard.realStart(argc, argv, mainCb);
    EXPECT_EQ(expected, actual);
}

TEST(DaemonGuardTest, RealDaemonTest) {
    int argc = 0;
    char **argv = nullptr;
    std::function<int(int argc, char **argv)> mainCb = nullptr;
    atom::async::DaemonGuard daemonGuard;
    int expected = 0;
    int actual = daemonGuard.realDaemon(argc, argv, mainCb);
    EXPECT_EQ(expected, actual);
}

TEST(DaemonGuardTest, StartDaemonTest) {
    int argc = 0;
    char **argv = nullptr;
    std::function<int(int argc, char **argv)> mainCb = nullptr;
    bool isDaemon = false;
    atom::async::DaemonGuard daemonGuard;
    int expected = 0;
    int actual = daemonGuard.startDaemon(argc, argv, mainCb, isDaemon);
    EXPECT_EQ(expected, actual);
}

TEST(DaemonGuardTest, SignalHandlerTest) {
    int signum = 0;
    atom::async::signalHandler(signum);
}

TEST(DaemonGuardTest, WritePidFileTest) {
    std::filesystem::path testPidFile = "test_write.pid";
    atom::async::writePidFile(testPidFile);
    // Clean up the test file
    if (std::filesystem::exists(testPidFile)) {
        std::filesystem::remove(testPidFile);
    }
}

TEST(DaemonGuardTest, CheckPidFileTest) {
    bool expected = false;
    std::filesystem::path testPidFile = "test_nonexistent.pid";
    bool actual = atom::async::checkPidFile(testPidFile);
    EXPECT_EQ(expected, actual);
}
