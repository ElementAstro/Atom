#include "atom/system/crontab.hpp"

#include <pybind11/chrono.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>


namespace py = pybind11;

PYBIND11_MODULE(crontab, m) {
    m.doc() = "Crontab management module for the atom package";

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

    // CronValidationResult struct binding
    py::class_<CronValidationResult>(m, "CronValidationResult",
                                     R"(Result of cron expression validation.

Contains a validity flag and an optional message explaining validation issues.

Attributes:
    valid (bool): Whether the cron expression is valid.
    message (str): Error message if invalid, or empty string if valid.
)")
        .def_readwrite("valid", &CronValidationResult::valid,
                       "Flag indicating if the cron expression is valid.")
        .def_readwrite("message", &CronValidationResult::message,
                       "Error message if invalid, or empty string if valid.")
        .def(
            "__bool__",
            [](const CronValidationResult& self) { return self.valid; },
            "Support for boolean evaluation.")
        .def(
            "__str__",
            [](const CronValidationResult& self) {
                return self.valid ? "Valid cron expression"
                                  : "Invalid cron expression: " + self.message;
            },
            "String representation of validation result.");

    // CronJob struct binding
    py::class_<CronJob>(
        m, "CronJob",
        R"(Represents a Cron job with a scheduled time and command.

Cron jobs are scheduled tasks that run at specified times. This class represents
a single job with its schedule, command, and metadata.

Args:
    time: Scheduled time for the Cron job (crontab format).
    command: Command to be executed by the Cron job.
    enabled: Status of the Cron job.
    category: Category of the Cron job for organization.
    description: Description of what the job does.

Examples:
    >>> from atom.system.crontab import CronJob
    >>> job = CronJob("0 * * * *", "echo 'Hourly task'", True, "maintenance", "Hourly maintenance task")
    >>> print(job.time)
    0 * * * *
)")
        .def(py::init<const std::string&, const std::string&, bool,
                      const std::string&, const std::string&>(),
             py::arg("time") = "", py::arg("command") = "",
             py::arg("enabled") = true, py::arg("category") = "default",
             py::arg("description") = "",
             "Constructs a new CronJob with the specified parameters.")
        .def_readwrite("time", &CronJob::time_,
                       "Scheduled time for the Cron job in crontab format.")
        .def_readwrite("command", &CronJob::command_,
                       "Command to be executed by the Cron job.")
        .def_readwrite("enabled", &CronJob::enabled_,
                       "Status of the Cron job (enabled/disabled).")
        .def_readwrite("category", &CronJob::category_,
                       "Category of the Cron job for organization.")
        .def_readwrite("description", &CronJob::description_,
                       "Description of what the job does.")
        .def_readonly("created_at", &CronJob::created_at_,
                      "Creation timestamp of the job.")
        .def_readonly("last_run", &CronJob::last_run_,
                      "Last execution timestamp of the job.")
        .def_readonly("run_count", &CronJob::run_count_,
                      "Number of times this job has been executed.")
        .def("to_json", &CronJob::toJson,
             "Converts the CronJob object to a JSON representation.")
        .def_static("from_json", &CronJob::fromJson, py::arg("json_obj"),
                    "Creates a CronJob object from a JSON representation.")
        .def("get_id", &CronJob::getId,
             "Gets a unique identifier for this job.")
        .def(
            "__str__",
            [](const CronJob& self) {
                return self.time_ + " " + self.command_ +
                       (self.enabled_ ? " (enabled)" : " (disabled)");
            },
            "String representation of the cron job.");

    // CronManager class binding
    py::class_<CronManager>(m, "CronManager",
                            R"(Manages a collection of Cron jobs.

This class provides methods to create, update, delete, and list Cron jobs,
as well as import and export them to JSON files or the system crontab.

Examples:
    >>> from atom.system.crontab import CronManager, CronJob
    >>> manager = CronManager()
    >>> job = CronJob("0 * * * *", "echo 'Hourly task'")
    >>> manager.create_cron_job(job)
    >>> jobs = manager.list_cron_jobs()
)")
        .def(py::init<>(), "Default constructor for CronManager.")
        .def("create_cron_job", &CronManager::createCronJob, py::arg("job"),
             R"(Adds a new Cron job.

Args:
    job: The CronJob object to be added.

Returns:
    True if the job was added successfully, false otherwise.
)")
        .def_static("validate_cron_expression",
                    &CronManager::validateCronExpression, py::arg("cron_expr"),
                    R"(Validates a cron expression.

Args:
    cron_expr: The cron expression to validate.

Returns:
    A CronValidationResult with validity flag and message.
)")
        .def("delete_cron_job", &CronManager::deleteCronJob, py::arg("command"),
             R"(Deletes a Cron job with the specified command.

Args:
    command: The command of the Cron job to be deleted.

Returns:
    True if the job was deleted successfully, false otherwise.
)")
        .def("delete_cron_job_by_id", &CronManager::deleteCronJobById,
             py::arg("id"),
             R"(Deletes a Cron job by its unique identifier.

Args:
    id: The unique identifier of the job.

Returns:
    True if the job was deleted successfully, false otherwise.
)")
        .def("list_cron_jobs", &CronManager::listCronJobs,
             R"(Lists all current Cron jobs.

Returns:
    A list of all current CronJob objects.
)")
        .def("list_cron_jobs_by_category", &CronManager::listCronJobsByCategory,
             py::arg("category"),
             R"(Lists all current Cron jobs in a specific category.

Args:
    category: The category to filter by.

Returns:
    A list of CronJob objects in the specified category.
)")
        .def("get_categories", &CronManager::getCategories,
             R"(Gets all available job categories.

Returns:
    A list of category names.
)")
        .def("export_to_json", &CronManager::exportToJSON, py::arg("filename"),
             R"(Exports all Cron jobs to a JSON file.

Args:
    filename: The name of the file to export to.

Returns:
    True if the export was successful, false otherwise.
)")
        .def("import_from_json", &CronManager::importFromJSON,
             py::arg("filename"),
             R"(Imports Cron jobs from a JSON file.

Args:
    filename: The name of the file to import from.

Returns:
    True if the import was successful, false otherwise.
)")
        .def("update_cron_job", &CronManager::updateCronJob,
             py::arg("old_command"), py::arg("new_job"),
             R"(Updates an existing Cron job.

Args:
    old_command: The command of the Cron job to be updated.
    new_job: The new CronJob object to replace the old one.

Returns:
    True if the job was updated successfully, false otherwise.
)")
        .def("update_cron_job_by_id", &CronManager::updateCronJobById,
             py::arg("id"), py::arg("new_job"),
             R"(Updates a Cron job by its unique identifier.

Args:
    id: The unique identifier of the job.
    new_job: The new CronJob object to replace the old one.

Returns:
    True if the job was updated successfully, false otherwise.
)")
        .def("view_cron_job", &CronManager::viewCronJob, py::arg("command"),
             R"(Views the details of a Cron job with the specified command.

Args:
    command: The command of the Cron job to view.

Returns:
    The CronJob object with the specified command.

Raises:
    RuntimeError: If the job is not found.
)")
        .def("view_cron_job_by_id", &CronManager::viewCronJobById,
             py::arg("id"),
             R"(Views the details of a Cron job by its unique identifier.

Args:
    id: The unique identifier of the job.

Returns:
    The CronJob object with the specified id.

Raises:
    RuntimeError: If the job is not found.
)")
        .def("search_cron_jobs", &CronManager::searchCronJobs, py::arg("query"),
             R"(Searches for Cron jobs that match the specified query.

Args:
    query: The query string to search for.

Returns:
    A list of CronJob objects that match the query.
)")
        .def("statistics", &CronManager::statistics,
             R"(Gets statistics about the current Cron jobs.

Returns:
    A dictionary with statistics about the jobs.
)")
        .def("enable_cron_job", &CronManager::enableCronJob, py::arg("command"),
             R"(Enables a Cron job with the specified command.

Args:
    command: The command of the Cron job to enable.

Returns:
    True if the job was enabled successfully, false otherwise.
)")
        .def("disable_cron_job", &CronManager::disableCronJob,
             py::arg("command"),
             R"(Disables a Cron job with the specified command.

Args:
    command: The command of the Cron job to disable.

Returns:
    True if the job was disabled successfully, false otherwise.
)")
        .def("set_job_enabled_by_id", &CronManager::setJobEnabledById,
             py::arg("id"), py::arg("enabled"),
             R"(Enable or disable a Cron job by its unique identifier.

Args:
    id: The unique identifier of the job.
    enabled: Whether to enable or disable the job.

Returns:
    True if the operation was successful, false otherwise.
)")
        .def("enable_cron_jobs_by_category",
             &CronManager::enableCronJobsByCategory, py::arg("category"),
             R"(Enables all Cron jobs in a specific category.

Args:
    category: The category of jobs to enable.

Returns:
    Number of jobs successfully enabled.
)")
        .def("disable_cron_jobs_by_category",
             &CronManager::disableCronJobsByCategory, py::arg("category"),
             R"(Disables all Cron jobs in a specific category.

Args:
    category: The category of jobs to disable.

Returns:
    Number of jobs successfully disabled.
)")
        .def("export_to_crontab", &CronManager::exportToCrontab,
             R"(Exports enabled Cron jobs to the system crontab.

Returns:
    True if the export was successful, false otherwise.
)")
        .def("batch_create_jobs", &CronManager::batchCreateJobs,
             py::arg("jobs"),
             R"(Batch creation of multiple Cron jobs.

Args:
    jobs: List of CronJob objects to create.

Returns:
    Number of jobs successfully created.
)")
        .def("batch_delete_jobs", &CronManager::batchDeleteJobs,
             py::arg("commands"),
             R"(Batch deletion of multiple Cron jobs.

Args:
    commands: List of commands identifying jobs to delete.

Returns:
    Number of jobs successfully deleted.
)")
        .def("record_job_execution", &CronManager::recordJobExecution,
             py::arg("command"),
             R"(Records that a job has been executed.

Args:
    command: The command of the executed job.

Returns:
    True if the job was found and updated, false otherwise.
)")
        .def("clear_all_jobs", &CronManager::clearAllJobs,
             R"(Clears all cron jobs in memory and from system crontab.

Returns:
    True if all jobs were cleared successfully, false otherwise.
)");

    // Additional examples in module docstring
    m.attr("__doc__") = R"(Crontab management module for the atom package.

This module provides classes for managing cron jobs in both memory and the system crontab.

Examples:
    >>> from atom.system.crontab import CronManager, CronJob
    >>> 
    >>> # Create a new cron manager
    >>> manager = CronManager()
    >>> 
    >>> # Create a job that runs every day at midnight
    >>> job = CronJob("0 0 * * *", "backup.sh", True, "backups", "Daily backup")
    >>> 
    >>> # Add the job to the manager
    >>> manager.create_cron_job(job)
    >>> 
    >>> # Validate a cron expression
    >>> result = CronManager.validate_cron_expression("0 0 * * *")
    >>> if result.valid:
    ...     print("Valid cron expression")
    >>> 
    >>> # Export jobs to system crontab
    >>> manager.export_to_crontab()
)";
}