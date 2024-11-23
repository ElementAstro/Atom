#include "atom/utils/qdatetime.hpp"

#include <iostream>

using namespace atom::utils;

int main() {
    // Create a QDateTime object using the default constructor
    QDateTime defaultDateTime;
    std::cout << "Default QDateTime is valid: " << std::boolalpha
              << defaultDateTime.isValid() << std::endl;

    // Create a QDateTime object from a date-time string and format
    QDateTime dateTimeFromString("2023-12-25 15:30:00", "%Y-%m-%d %H:%M:%S");
    std::cout << "QDateTime from string is valid: " << std::boolalpha
              << dateTimeFromString.isValid() << std::endl;

    // Get the current date and time
    QDateTime currentDateTime = QDateTime::currentDateTime();
    std::cout << "Current date and time: "
              << currentDateTime.toString("%Y-%m-%d %H:%M:%S") << std::endl;

    // Convert QDateTime to string
    std::string dateTimeStr = dateTimeFromString.toString("%Y-%m-%d %H:%M:%S");
    std::cout << "QDateTime to string: " << dateTimeStr << std::endl;

    // Convert QDateTime to time_t
    std::time_t timeT = dateTimeFromString.toTimeT();
    std::cout << "QDateTime to time_t: " << timeT << std::endl;

    // Add days to QDateTime
    QDateTime dateTimePlusDays = dateTimeFromString.addDays(10);
    std::cout << "QDateTime plus 10 days: "
              << dateTimePlusDays.toString("%Y-%m-%d %H:%M:%S") << std::endl;

    // Add seconds to QDateTime
    QDateTime dateTimePlusSecs = dateTimeFromString.addSecs(3600);
    std::cout << "QDateTime plus 3600 seconds: "
              << dateTimePlusSecs.toString("%Y-%m-%d %H:%M:%S") << std::endl;

    // Compute the number of days between two QDateTime objects
    int daysBetween = dateTimeFromString.daysTo(dateTimePlusDays);
    std::cout << "Days between: " << daysBetween << std::endl;

    // Compute the number of seconds between two QDateTime objects
    int secsBetween = dateTimeFromString.secsTo(dateTimePlusSecs);
    std::cout << "Seconds between: " << secsBetween << std::endl;

    // Compare two QDateTime objects
    bool isEarlier = dateTimeFromString < dateTimePlusDays;
    std::cout << "dateTimeFromString is earlier than dateTimePlusDays: "
              << std::boolalpha << isEarlier << std::endl;

    return 0;
}