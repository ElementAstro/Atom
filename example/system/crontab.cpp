#include "atom/system/crontab.hpp"

#include <iostream>

int main() {
    // Create a CronManager object
    CronManager manager;

    // Create a new Cron job
    CronJob job1{"* * * * *", "echo 'Hello, World!'"};
    bool created = manager.createCronJob(job1);
    std::cout << "Cron job created: " << std::boolalpha << created << std::endl;

    // List all current Cron jobs
    auto jobs = manager.listCronJobs();
    std::cout << "Current Cron jobs:" << std::endl;
    for (const auto& job : jobs) {
        std::cout << "Time: " << job.time_ << ", Command: " << job.command_
                  << std::endl;
    }

    // Update an existing Cron job
    CronJob job2{"0 0 * * *", "echo 'Goodnight, World!'"};
    bool updated = manager.updateCronJob("echo 'Hello, World!'", job2);
    std::cout << "Cron job updated: " << std::boolalpha << updated << std::endl;

    // View the details of a Cron job
    CronJob viewedJob = manager.viewCronJob("echo 'Goodnight, World!'");
    std::cout << "Viewed Cron job - Time: " << viewedJob.time_
              << ", Command: " << viewedJob.command_ << std::endl;

    // Search for Cron jobs that match a query
    auto searchResults = manager.searchCronJobs("echo");
    std::cout << "Search results:" << std::endl;
    for (const auto& job : searchResults) {
        std::cout << "Time: " << job.time_ << ", Command: " << job.command_
                  << std::endl;
    }

    // Delete a Cron job
    bool deleted = manager.deleteCronJob("echo 'Goodnight, World!'");
    std::cout << "Cron job deleted: " << std::boolalpha << deleted << std::endl;

    // Export all Cron jobs to a JSON file
    bool exported = manager.exportToJSON("cron_jobs.json");
    std::cout << "Cron jobs exported to JSON: " << std::boolalpha << exported
              << std::endl;

    // Import Cron jobs from a JSON file
    bool imported = manager.importFromJSON("cron_jobs.json");
    std::cout << "Cron jobs imported from JSON: " << std::boolalpha << imported
              << std::endl;

    // Get statistics about the current Cron jobs
    int stats = manager.statistics();
    std::cout << "Cron job statistics: " << stats << std::endl;

    return 0;
}