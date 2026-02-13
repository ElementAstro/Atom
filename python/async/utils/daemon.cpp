#include "atom/async/daemon.hpp"

#include <pybind11/chrono.h>
#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <filesystem>

namespace py = pybind11;

PYBIND11_MODULE(daemon, m) {
    m.doc() = R"pbdoc(
        Daemon Process Management Module
        -------------------------------

        This module provides tools for creating and managing daemon processes
        on both Unix-like systems and Windows.

        Features:
        - Create daemon processes that run in the background
        - Monitor and control daemon processes
        - Handle daemon restarts and failure recovery
        - Manage daemon PID files

        Example:
            >>> from atom.async.daemon import DaemonGuard, check_pid_file, write_pid_file
            >>>
            >>> # Check if daemon is already running
            >>> if not check_pid_file("my-daemon"):
            >>>     # Create a daemon process
            >>>     daemon = DaemonGuard()
            >>>
            >>>     # Define the main process function
            >>>     def main_process(argc, argv):
            >>>         # Your daemon code here
            >>>         write_pid_file("my-daemon")
            >>>         return 0
            >>>
            >>>     # Start the daemon
            >>>     daemon.start_daemon(0, [], main_process, True)
    )pbdoc";

    // Register exception translations
    py::register_exception<atom::async::DaemonException>(m, "DaemonException",
                                                         PyExc_RuntimeError);

    py::register_exception_translator([](std::exception_ptr p) {
        try {
            if (p)
                std::rethrow_exception(p);
        } catch (const atom::async::DaemonException& e) {
            PyErr_SetString(PyExc_RuntimeError, e.what());
        } catch (const std::filesystem::filesystem_error& e) {
            PyErr_SetString(PyExc_OSError, e.what());
        } catch (const std::invalid_argument& e) {
            PyErr_SetString(PyExc_ValueError, e.what());
        } catch (const std::runtime_error& e) {
            PyErr_SetString(PyExc_RuntimeError, e.what());
        } catch (const std::exception& e) {
            PyErr_SetString(PyExc_Exception, e.what());
        }
    });

    // Register DaemonGuard class
    py::class_<atom::async::DaemonGuard>(m, "DaemonGuard",
                                         R"pbdoc(
        Class for managing daemon processes.

        This class provides methods to start, monitor and control daemon processes
        on both Unix-like systems and Windows.

        Examples:
            >>> daemon = DaemonGuard()
            >>>
            >>> # Define the main process function
            >>> def main_process(argc, argv):
            >>>     # Your daemon code here
            >>>     return 0
            >>>
            >>> # Start a daemon process
            >>> daemon.start_daemon(0, [], main_process, True)
        )pbdoc")
        .def(py::init<>(), "Constructs a new DaemonGuard object.")
        .def("to_string", &atom::async::DaemonGuard::toString,
             "Returns a string representation of the daemon process.")
        .def(
            "real_start",
            [](atom::async::DaemonGuard& self, int argc,
               const std::vector<std::string>& argv_vec, py::function main_cb) {
                // Convert vector of strings to char**
                std::vector<char*> argv;
                for (auto& arg : argv_vec) {
                    // This is safe because we ensure the vector outlives the
                    // call
                    argv.push_back(const_cast<char*>(arg.c_str()));
                }
                argv.push_back(nullptr);  // Null-terminate

                // Pass to C++ function
                return self.realStart(
                    argc, argv.data(),
                    [main_cb](int arg_c, char** arg_v) -> int {
                        py::gil_scoped_acquire acquire;  // Ensure GIL is held
                        // Convert char** to Python list
                        py::list py_argv;
                        for (int i = 0; i < arg_c; i++) {
                            py_argv.append(py::str(arg_v[i]));
                        }
                        return main_cb(arg_c, py_argv).cast<int>();
                    });
            },
            py::arg("argc"), py::arg("argv"), py::arg("main_cb"),
            R"pbdoc(
             Starts a child process to execute the actual task.

             Args:
                 argc: The number of command line arguments
                 argv: A list of command line arguments
                 main_cb: The main callback function to be executed in the child process

             Returns:
                 The return value of the main callback function

             Raises:
                 DaemonException: If process creation fails
             )pbdoc")
        .def(
            "real_daemon",
            [](atom::async::DaemonGuard& self, int argc,
               const std::vector<std::string>& argv_vec, py::function main_cb) {
                // Convert vector of strings to char**
                std::vector<char*> argv;
                for (auto& arg : argv_vec) {
                    argv.push_back(const_cast<char*>(arg.c_str()));
                }
                argv.push_back(nullptr);  // Null-terminate

                // Pass to C++ function
                return self.realDaemon(
                    argc, argv.data(),
                    [main_cb](int arg_c, char** arg_v) -> int {
                        py::gil_scoped_acquire acquire;  // Ensure GIL is held
                        // Convert char** to Python list
                        py::list py_argv;
                        for (int i = 0; i < arg_c; i++) {
                            py_argv.append(py::str(arg_v[i]));
                        }
                        return main_cb(arg_c, py_argv).cast<int>();
                    });
            },
            py::arg("argc"), py::arg("argv"), py::arg("main_cb"),
            R"pbdoc(
             Starts a child process as a daemon to execute the actual task.

             Args:
                 argc: The number of command line arguments
                 argv: A list of command line arguments
                 main_cb: The main callback function to be executed in the daemon process

             Returns:
                 The return value of the main callback function

             Raises:
                 DaemonException: If daemon process creation fails
             )pbdoc")
        .def(
            "start_daemon",
            [](atom::async::DaemonGuard& self, int argc,
               const std::vector<std::string>& argv_vec, py::function main_cb,
               bool is_daemon) {
                // Convert vector of strings to char**
                std::vector<char*> argv;
                for (auto& arg : argv_vec) {
                    argv.push_back(const_cast<char*>(arg.c_str()));
                }
                argv.push_back(nullptr);  // Null-terminate

                // Pass to C++ function
                return self.startDaemon(
                    argc, argv.data(),
                    [main_cb](int arg_c, char** arg_v) -> int {
                        py::gil_scoped_acquire acquire;  // Ensure GIL is held
                        // Convert char** to Python list
                        py::list py_argv;
                        for (int i = 0; i < arg_c; i++) {
                            py_argv.append(py::str(arg_v[i]));
                        }
                        return main_cb(arg_c, py_argv).cast<int>();
                    },
                    is_daemon);
            },
            py::arg("argc"), py::arg("argv"), py::arg("main_cb"),
            py::arg("is_daemon"),
            R"pbdoc(
             Starts the process. If a daemon process needs to be created,
             it will create the daemon process first.

             Args:
                 argc: The number of command line arguments
                 argv: A list of command line arguments
                 main_cb: The main callback function to be executed
                 is_daemon: Determines if a daemon process should be created

             Returns:
                 The return value of the main callback function

             Raises:
                 DaemonException: If process creation fails
             )pbdoc")
        .def("get_restart_count", &atom::async::DaemonGuard::getRestartCount,
             "Gets the number of restart attempts.")
        .def("is_running", &atom::async::DaemonGuard::isRunning,
             "Checks if the daemon is running.")
        .def(
            "set_pid_file_path",
            [](atom::async::DaemonGuard& self, const std::string& path) {
                self.setPidFilePath(std::filesystem::path(path));
            },
            py::arg("path"),
            R"pbdoc(
            Set the PID file path for the daemon.

            Args:
                path: Path to the PID file

            Examples:
                >>> daemon.set_pid_file_path("/var/run/my-daemon.pid")
            )pbdoc")
        .def(
            "get_pid_file_path",
            [](const atom::async::DaemonGuard& self) -> py::object {
                auto path = self.getPidFilePath();
                if (path.has_value()) {
                    return py::str(path->string());
                }
                return py::none();
            },
            R"pbdoc(
            Get the PID file path for the daemon.

            Returns:
                The PID file path as a string, or None if not set

            Examples:
                >>> path = daemon.get_pid_file_path()
                >>> if path:
                ...     print(f"PID file: {path}")
            )pbdoc");

    // Register utility functions
    m.def("signal_handler", &atom::async::signalHandler, py::arg("signum"),
          "Signal handler function for daemon processes.");

    m.def(
        "write_pid_file",
        [](const std::string& file_path) {
            return atom::async::writePidFile(file_path);
        },
        py::arg("file_path") = "lithium-daemon",
        R"pbdoc(
         Writes the process ID to a file.

         Args:
             file_path: Path to write the PID file (default: "lithium-daemon")

         Raises:
             OSError: If file operation fails
         )pbdoc");

    m.def(
        "check_pid_file",
        [](const std::string& file_path) {
            return atom::async::checkPidFile(file_path);
        },
        py::arg("file_path") = "lithium-daemon",
        R"pbdoc(
         Checks if the process ID file exists and the process is running.

         Args:
             file_path: Path to the PID file (default: "lithium-daemon")

         Returns:
             True if the PID file exists and the process is running, False otherwise
         )pbdoc");

    m.def("set_daemon_restart_interval", &atom::async::setDaemonRestartInterval,
          py::arg("seconds"),
          R"pbdoc(
          Sets the restart interval for daemon processes.

          Args:
              seconds: Interval in seconds

          Raises:
              ValueError: If seconds is less than or equal to zero
          )pbdoc");

    m.def("get_daemon_restart_interval", &atom::async::getDaemonRestartInterval,
          "Gets the current daemon restart interval in seconds.")

        .def(
            "create_daemon_manager",
            []() -> py::dict {
                py::dict manager;
                manager[py::str("daemon")] =
                    std::make_unique<atom::async::DaemonGuard>();
                manager[py::str("active")] = false;
                manager[py::str("restart_count")] = 0;
                manager[py::str("pid_file")] = py::none();

                return manager;
            },
            R"pbdoc(
        Create a daemon manager for organizing daemon processes.

        Returns:
            dict: Daemon manager with daemon instance and status information

        Examples:
            >>> manager = create_daemon_manager()
            >>> daemon = manager["daemon"]
            >>> manager["active"] = True
        )pbdoc")

        .def(
            "benchmark_daemon_startup",
            [](py::function main_cb, int num_iterations) -> py::dict {
                using namespace std::chrono;

                py::dict results;
                std::vector<double> startup_times;
                startup_times.reserve(num_iterations);

                for (int i = 0; i < num_iterations; ++i) {
                    atom::async::DaemonGuard daemon;

                    auto start = high_resolution_clock::now();

                    try {
                        // Simulate daemon startup
                        std::vector<std::string> args = {"test-daemon"};
                        daemon.realStart(
                            1, args, [main_cb](int argc, char** argv) -> int {
                                py::gil_scoped_acquire acquire;
                                py::list py_argv;
                                for (int j = 0; j < argc; j++) {
                                    py_argv.append(py::str(argv[j]));
                                }
                                return main_cb(argc, py_argv).cast<int>();
                            });
                    } catch (...) {
                        // Ignore errors for benchmarking
                    }

                    auto end = high_resolution_clock::now();
                    auto duration = duration_cast<microseconds>(end - start);
                    startup_times.push_back(duration.count());
                }

                // Calculate statistics
                double total_time = 0;
                for (double time : startup_times) {
                    total_time += time;
                }
                double avg_time = total_time / num_iterations;

                double min_time = *std::min_element(startup_times.begin(),
                                                    startup_times.end());
                double max_time = *std::max_element(startup_times.begin(),
                                                    startup_times.end());

                results[py::str("num_iterations")] = num_iterations;
                results[py::str("avg_startup_time_us")] = avg_time;
                results[py::str("min_startup_time_us")] = min_time;
                results[py::str("max_startup_time_us")] = max_time;
                results[py::str("total_time_us")] = total_time;

                return results;
            },
            py::arg("main_cb"), py::arg("num_iterations") = 10,
            R"pbdoc(
        Benchmark daemon startup performance.

        Args:
            main_cb: Main callback function for the daemon
            num_iterations: Number of startup iterations to test (default: 10)

        Returns:
            dict: Benchmark results with timing statistics

        Examples:
            >>> def dummy_main(argc, argv):
            ...     return 0
            >>> results = benchmark_daemon_startup(dummy_main, 5)
            >>> print(f"Avg startup time: {results['avg_startup_time_us']:.2f} μs")
        )pbdoc")

        .def(
            "create_daemon_pool",
            [](size_t pool_size) -> py::list {
                py::list daemons;
                for (size_t i = 0; i < pool_size; ++i) {
                    daemons.append(
                        std::make_unique<atom::async::DaemonGuard>());
                }
                return daemons;
            },
            py::arg("pool_size"),
            R"pbdoc(
        Create a pool of daemon instances for load distribution.

        Args:
            pool_size: Number of daemon instances to create

        Returns:
            list: List of DaemonGuard instances

        Examples:
            >>> daemon_pool = create_daemon_pool(3)
            >>> # Use different daemons for different services
            >>> daemon_pool[0].set_pid_file_path("/var/run/service1.pid")
            >>> daemon_pool[1].set_pid_file_path("/var/run/service2.pid")
        )pbdoc");

// Platform-specific information
#ifdef _WIN32
    m.attr("PLATFORM") = "Windows";
    m.attr("HAS_FORK") = false;
    m.attr("HAS_SETSID") = false;
#else
    m.attr("PLATFORM") = "Unix";
    m.attr("HAS_FORK") = true;
    m.attr("HAS_SETSID") = true;
#endif

    // Feature detection
    m.attr("HAS_PID_FILES") = true;
    m.attr("HAS_SIGNAL_HANDLING") = true;
    m.attr("HAS_PROCESS_MONITORING") = true;

    // Add version information
    m.attr("__version__") = "1.0.0";
}
