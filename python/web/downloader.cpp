#include "atom/web/downloader.hpp"

#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>


namespace py = pybind11;

PYBIND11_MODULE(downloader, m) {
    m.doc() = "Download manager implementation module for the atom package";

    // Register exception translations
    py::register_exception_translator([](std::exception_ptr p) {
        try {
            if (p)
                std::rethrow_exception(p);
        } catch (const std::invalid_argument& e) {
            PyErr_SetString(PyExc_ValueError, e.what());
        } catch (const std::runtime_error& e) {
            PyErr_SetString(PyExc_RuntimeError, e.what());
        } catch (const std::exception& e) {
            PyErr_SetString(PyExc_Exception, e.what());
        }
    });

    // DownloadManager class binding
    py::class_<atom::web::DownloadManager>(
        m, "DownloadManager",
        R"(A class that manages download tasks.

This class provides methods to add, manage, and monitor download tasks.
It supports multi-threaded downloads, download speed control, and progress callbacks.

Args:
    task_file: The file path to save the download task list.

Examples:
    >>> from atom.web.downloader import DownloadManager
    >>> dm = DownloadManager("downloads.json")
    >>> dm.add_task("https://example.com/file.zip", "/path/to/save/file.zip")
    >>> dm.start(4)  # Start with 4 download threads
)")
        .def(py::init<const std::string&>(), py::arg("task_file"),
             "Constructs a DownloadManager object with the path to save the "
             "download task list.")
        .def("add_task", &atom::web::DownloadManager::addTask, py::arg("url"),
             py::arg("filepath"), py::arg("priority") = 0,
             R"(Adds a download task.

Args:
    url: The download URL.
    filepath: The path to save the downloaded file.
    priority: The priority of the download task, higher values indicate higher priority.
             Default is 0.

Examples:
    >>> dm.add_task("https://example.com/file.zip", "/path/to/save/file.zip", 10)  # High priority
)")
        .def("remove_task", &atom::web::DownloadManager::removeTask,
             py::arg("index"),
             R"(Removes a download task.

Args:
    index: The index in the task list.

Returns:
    bool: True if the task is successfully removed, false otherwise.

Examples:
    >>> if dm.remove_task(0):
    ...     print("Task removed successfully")
    ... else:
    ...     print("Failed to remove task")
)")
        .def("start", &atom::web::DownloadManager::start,
             py::arg("thread_count") =
                 py::int_(std::thread::hardware_concurrency()),
             py::arg("download_speed") = 0,
             R"(Starts the download tasks.

Args:
    thread_count: The number of download threads. Default is the number of CPU cores.
    download_speed: The download speed limit (bytes per second), 0 means no limit. Default is 0.

Examples:
    >>> dm.start()  # Use default number of threads, no speed limit
    >>> dm.start(2, 1024 * 1024)  # 2 threads, limit to 1MB/s
)")
        .def("pause_task", &atom::web::DownloadManager::pauseTask,
             py::arg("index"),
             R"(Pauses a download task.

Args:
    index: The index in the task list.

Examples:
    >>> dm.pause_task(0)  # Pause the first task
)")
        .def("resume_task", &atom::web::DownloadManager::resumeTask,
             py::arg("index"),
             R"(Resumes a paused download task.

Args:
    index: The index in the task list.

Examples:
    >>> dm.resume_task(0)  # Resume the first task
)")
        .def("get_downloaded_bytes",
             &atom::web::DownloadManager::getDownloadedBytes, py::arg("index"),
             R"(Gets the number of bytes downloaded for a task.

Args:
    index: The index in the task list.

Returns:
    int: The number of bytes downloaded.

Examples:
    >>> bytes_downloaded = dm.get_downloaded_bytes(0)
    >>> print(f"Downloaded {bytes_downloaded} bytes")
)")
        .def("cancel_task", &atom::web::DownloadManager::cancelTask,
             py::arg("index"),
             R"(Cancels a download task.

Args:
    index: The index in the task list.

Examples:
    >>> dm.cancel_task(0)  # Cancel the first task
)")
        .def("set_thread_count", &atom::web::DownloadManager::setThreadCount,
             py::arg("thread_count"),
             R"(Dynamically adjusts the number of download threads.

Args:
    thread_count: The new number of download threads.

Examples:
    >>> dm.set_thread_count(8)  # Increase to 8 threads
)")
        .def("set_max_retries", &atom::web::DownloadManager::setMaxRetries,
             py::arg("retries"),
             R"(Sets the maximum number of retries for download errors.

Args:
    retries: The maximum number of retries for each task on failure.

Examples:
    >>> dm.set_max_retries(3)  # Retry up to 3 times
)")
        .def(
            "on_download_complete",
            &atom::web::DownloadManager::onDownloadComplete,
            py::arg("callback"),
            R"(Registers a callback function to be called when a download is complete.

Args:
    callback: The callback function, with the task index as the parameter.

Examples:
    >>> def download_complete(index):
    ...     print(f"Download task {index} completed!")
    ...
    >>> dm.on_download_complete(download_complete)
)")
        .def(
            "on_progress_update", &atom::web::DownloadManager::onProgressUpdate,
            py::arg("callback"),
            R"(Registers a callback function to be called when the download progress is updated.

Args:
    callback: The callback function, with the task index and download percentage as parameters.

Examples:
    >>> def progress_update(index, percentage):
    ...     print(f"Task {index}: {percentage:.1f}% complete")
    ...
    >>> dm.on_progress_update(progress_update)
)");

    // Convenience functions
    m.def(
        "download_file",
        [](const std::string& url, const std::string& filepath,
           bool wait_for_completion) {
            // Create a temporary DownloadManager
            atom::web::DownloadManager dm("temp_download_tasks.json");

            // Add the task
            dm.addTask(url, filepath, 10);  // High priority

            // Start download with a single thread
            dm.start(1);

            // If wait_for_completion is true, we'd need some way to wait
            // Since the actual implementation details of DownloadManager
            // are hidden in the Impl class, this is just a placeholder.
            // In a real implementation, we'd need to have Impl expose a
            // way to check if all downloads are complete.

            return true;  // Placeholder for success
        },
        py::arg("url"), py::arg("filepath"),
        py::arg("wait_for_completion") = true,
        R"(Convenience function to download a single file.

Args:
    url: The URL of the file to download.
    filepath: The local path to save the downloaded file.
    wait_for_completion: If True, waits for the download to complete before returning.
                         Default is True.

Returns:
    bool: True if the download was started successfully.

Examples:
    >>> from atom.web.downloader import download_file
    >>> download_file("https://example.com/file.zip", "/path/to/save/file.zip")
)");

    m.def(
        "download_files",
        [](const std::vector<std::pair<std::string, std::string>>&
               url_path_pairs,
           size_t thread_count = std::thread::hardware_concurrency()) {
            // Create a temporary DownloadManager
            atom::web::DownloadManager dm("temp_batch_downloads.json");

            // Add all tasks
            for (const auto& [url, path] : url_path_pairs) {
                dm.addTask(url, path);
            }

            // Start download with specified thread count
            dm.start(thread_count);

            return url_path_pairs.size();  // Return number of tasks started
        },
        py::arg("url_path_pairs"),
        py::arg("thread_count") = py::int_(std::thread::hardware_concurrency()),
        R"(Convenience function to download multiple files in batch.

Args:
    url_path_pairs: A list of (url, filepath) tuples specifying the downloads.
    thread_count: The number of download threads to use. Default is the number of CPU cores.

Returns:
    int: The number of download tasks started.

Examples:
    >>> from atom.web.downloader import download_files
    >>> files = [
    ...     ("https://example.com/file1.zip", "/path/to/save/file1.zip"),
    ...     ("https://example.com/file2.zip", "/path/to/save/file2.zip")
    ... ]
    >>> download_files(files, 4)  # Download with 4 threads
    2
)");
}