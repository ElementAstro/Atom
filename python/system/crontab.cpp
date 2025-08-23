#include "atom/system/crontab.hpp"

#include <pybind11/chrono.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include "python/pybind11_json.hpp"

namespace py = pybind11;

/**
 * @brief Registers exception translations for the crontab module.
 *
 * This function sets up proper exception handling to translate C++ exceptions
 * to appropriate Python exceptions for better error reporting.
 *
 * @param m The pybind11 module to register exceptions for
 */
void registerExceptionTranslations(py::module_& m) {
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
}

/**
 * @brief Binds the CronValidationResult struct to Python.
 *
 * This function creates Python bindings for the CronValidationResult struct
 * which contains validation results for cron expressions.
 *
 * @param m The pybind11 module to bind to
 */
void bindCronValidationResult(py::module_& m) {
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
}

/**
 * @brief Binds the CronJob struct to Python.
 *
 * This function creates Python bindings for the CronJob struct which represents
 * a single cron job with its schedule, command, and metadata.
 *
 * @param m The pybind11 module to bind to
 */
void bindCronJob(py::module_& m) {
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
}

/**
 * @brief Binds the CronManager class to Python.
 *
 * This function creates Python bindings for the CronManager class which provides
 * comprehensive cron job management functionality.
 *
 * @param m The pybind11 module to bind to
 */
void bindCronManager(py::module_& m) {
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

        // Job creation and deletion methods
        .def("create_cron_job", &CronManager::createCronJob, py::arg("job"),
             "Adds a new Cron job.")
        .def("delete_cron_job", &CronManager::deleteCronJob, py::arg("command"),
             "Deletes a Cron job with the specified command.")
        .def("delete_cron_job_by_id", &CronManager::deleteCronJobById, py::arg("id"),
             "Deletes a Cron job by its unique identifier.")
        .def("batch_create_jobs", &CronManager::batchCreateJobs, py::arg("jobs"),
             "Batch creation of multiple Cron jobs.")
        .def("batch_delete_jobs", &CronManager::batchDeleteJobs, py::arg("commands"),
             "Batch deletion of multiple Cron jobs.")
        .def("clear_all_jobs", &CronManager::clearAllJobs,
             "Clears all cron jobs in memory and from system crontab.")

        // Job listing and searching methods
        .def("list_cron_jobs", &CronManager::listCronJobs,
             "Lists all current Cron jobs.")
        .def("list_cron_jobs_by_category", &CronManager::listCronJobsByCategory, py::arg("category"),
             "Lists all current Cron jobs in a specific category.")
        .def("search_cron_jobs", &CronManager::searchCronJobs, py::arg("query"),
             "Searches for Cron jobs that match the specified query.")
        .def("view_cron_job", &CronManager::viewCronJob, py::arg("command"),
             "Views the details of a Cron job with the specified command.")
        .def("view_cron_job_by_id", &CronManager::viewCronJobById, py::arg("id"),
             "Views the details of a Cron job by its unique identifier.")

        // Job update and status methods
        .def("update_cron_job", &CronManager::updateCronJob, py::arg("old_command"), py::arg("new_job"),
             "Updates an existing Cron job.")
        .def("update_cron_job_by_id", &CronManager::updateCronJobById, py::arg("id"), py::arg("new_job"),
             "Updates a Cron job by its unique identifier.")
        .def("enable_cron_job", &CronManager::enableCronJob, py::arg("command"),
             "Enables a Cron job with the specified command.")
        .def("disable_cron_job", &CronManager::disableCronJob, py::arg("command"),
             "Disables a Cron job with the specified command.")
        .def("set_job_enabled_by_id", &CronManager::setJobEnabledById, py::arg("id"), py::arg("enabled"),
             "Enable or disable a Cron job by its unique identifier.")
        .def("enable_cron_jobs_by_category", &CronManager::enableCronJobsByCategory, py::arg("category"),
             "Enables all Cron jobs in a specific category.")
        .def("disable_cron_jobs_by_category", &CronManager::disableCronJobsByCategory, py::arg("category"),
             "Disables all Cron jobs in a specific category.")

        // Utility and management methods
        .def_static("validate_cron_expression", &CronManager::validateCronExpression, py::arg("cron_expr"),
                    "Validates a cron expression.")
        .def("get_categories", &CronManager::getCategories,
             "Gets all available job categories.")
        .def("statistics", &CronManager::statistics,
             "Gets statistics about the current Cron jobs.")
        .def("record_job_execution", &CronManager::recordJobExecution, py::arg("command"),
             "Records that a job has been executed.")

        // Import/Export methods
        .def("export_to_json", &CronManager::exportToJSON, py::arg("filename"),
             "Exports all Cron jobs to a JSON file.")
        .def("import_from_json", &CronManager::importFromJSON, py::arg("filename"),
             "Imports Cron jobs from a JSON file.")
        .def("export_to_crontab", &CronManager::exportToCrontab,
             "Exports enabled Cron jobs to the system crontab.");
}

/**
 * @brief Adds comprehensive module documentation and usage examples.
 *
 * This function sets the module's __doc__ attribute with detailed documentation
 * including usage examples for cron job management.
 *
 * @param m The pybind11 module to add documentation to
 */
void addModuleDocumentation(py::module_& m) {
    m.attr("__doc__") = R"(Crontab management module for the atom package.

This module provides classes for managing cron jobs in both memory and the system crontab.

Key Features:
- Create, update, delete, and list cron jobs
- Validate cron expressions
- Import/export jobs to JSON files
- Export jobs to system crontab
- Batch operations for multiple jobs
- Category-based job organization
- Job execution tracking and statistics

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
    >>> # List all jobs in a category
    >>> backup_jobs = manager.list_cron_jobs_by_category("backups")
    >>>
    >>> # Export jobs to system crontab
    >>> manager.export_to_crontab()
    >>>
    >>> # Get statistics
    >>> stats = manager.statistics()
    >>> print(f"Total jobs: {stats['total_jobs']}")
)";
}

PYBIND11_MODULE(crontab, m) {
    m.doc() = "Crontab management module for the atom package";

    // Register exception translations
    registerExceptionTranslations(m);

    // Bind core data structures
    bindCronValidationResult(m);
    bindCronJob(m);
    bindCronManager(m);

    // Add module documentation
    addModuleDocumentation(m);
}
