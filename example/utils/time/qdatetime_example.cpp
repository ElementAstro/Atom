/**
 * @file qdatetime_example.cpp
 * @brief Examples for atom::utils QDateTime
 */

#include <iostream>
#include <string>
#include "atom/utils/time/qdatetime.hpp"

using namespace atom::utils;

void printSection(const std::string& title) {
    std::cout << "\n========================================" << std::endl;
    std::cout << "  " << title << std::endl;
    std::cout << "========================================" << std::endl;
}

void demonstrateCurrentDateTime() {
    printSection("1. Current Date and Time");

    QDateTime now = QDateTime::currentDateTime();

    std::cout << "Current date/time:" << std::endl;
    std::cout << "  toString(): " << now.toString() << std::endl;
    std::cout << "  ISO format: " << now.toString("yyyy-MM-dd HH:mm:ss")
              << std::endl;

    auto date = now.date();
    std::cout << "\nDate components:" << std::endl;
    std::cout << "  Year: " << date.year << std::endl;
    std::cout << "  Month: " << date.month << std::endl;
    std::cout << "  Day: " << date.day << std::endl;

    auto time = now.time();
    std::cout << "\nTime components:" << std::endl;
    std::cout << "  Hour: " << time.hour << std::endl;
    std::cout << "  Minute: " << time.minute << std::endl;
    std::cout << "  Second: " << time.second << std::endl;
    std::cout << "  Millisecond: " << time.millisecond << std::endl;
}

void demonstrateDateTimeConstruction() {
    printSection("2. DateTime Construction");

    std::cout << "--- From components ---" << std::endl;
    QDateTime dt1(2024, 12, 25, 10, 30, 0);
    std::cout << "  QDateTime(2024, 12, 25, 10, 30, 0): " << dt1.toString()
              << std::endl;

    QDateTime dt2(2024, 1, 1);  // Midnight
    std::cout << "  QDateTime(2024, 1, 1): " << dt2.toString() << std::endl;

    std::cout << "\n--- From string ---" << std::endl;
    QDateTime dt3("2024-06-15 14:30:00", "yyyy-MM-dd HH:mm:ss");
    std::cout << "  Parsed '2024-06-15 14:30:00': " << dt3.toString()
              << std::endl;
}

void demonstrateDateTimeArithmetic() {
    printSection("3. DateTime Arithmetic");

    QDateTime base(2024, 6, 15, 12, 0, 0);
    std::cout << "Base date: " << base.toString() << std::endl;

    std::cout << "\n--- Adding time ---" << std::endl;
    auto plus1Day = base.addDays(1);
    std::cout << "  +1 day: " << plus1Day.toString() << std::endl;

    auto plus1Week = base.addDays(7);
    std::cout << "  +1 week: " << plus1Week.toString() << std::endl;

    auto plus1Month = base.addMonths(1);
    std::cout << "  +1 month: " << plus1Month.toString() << std::endl;

    auto plus1Year = base.addYears(1);
    std::cout << "  +1 year: " << plus1Year.toString() << std::endl;

    auto plus2Hours = base.addSecs(7200);
    std::cout << "  +2 hours: " << plus2Hours.toString() << std::endl;

    std::cout << "\n--- Subtracting time ---" << std::endl;
    auto minus1Day = base.addDays(-1);
    std::cout << "  -1 day: " << minus1Day.toString() << std::endl;

    auto minus1Month = base.addMonths(-1);
    std::cout << "  -1 month: " << minus1Month.toString() << std::endl;
}

void demonstrateDateTimeComparison() {
    printSection("4. DateTime Comparison");

    QDateTime dt1(2024, 6, 15, 10, 0, 0);
    QDateTime dt2(2024, 6, 15, 14, 0, 0);
    QDateTime dt3(2024, 6, 15, 10, 0, 0);

    std::cout << "dt1: " << dt1.toString() << std::endl;
    std::cout << "dt2: " << dt2.toString() << std::endl;
    std::cout << "dt3: " << dt3.toString() << std::endl;

    std::cout << "\nComparisons:" << std::endl;
    std::cout << "  dt1 == dt3: " << (dt1 == dt3 ? "true" : "false")
              << std::endl;
    std::cout << "  dt1 < dt2: " << (dt1 < dt2 ? "true" : "false") << std::endl;
    std::cout << "  dt2 > dt1: " << (dt2 > dt1 ? "true" : "false") << std::endl;
    std::cout << "  dt1 != dt2: " << (dt1 != dt2 ? "true" : "false")
              << std::endl;
}

void demonstrateDateTimeFormatting() {
    printSection("5. DateTime Formatting");

    QDateTime dt(2024, 12, 25, 14, 30, 45, 123);

    std::cout << "DateTime: " << dt.toString() << std::endl;
    std::cout << "\nFormat patterns:" << std::endl;
    std::cout << "  'yyyy-MM-dd': " << dt.toString("yyyy-MM-dd") << std::endl;
    std::cout << "  'HH:mm:ss': " << dt.toString("HH:mm:ss") << std::endl;
    std::cout << "  'yyyy/MM/dd HH:mm': " << dt.toString("yyyy/MM/dd HH:mm")
              << std::endl;
    std::cout << "  'dd.MM.yyyy': " << dt.toString("dd.MM.yyyy") << std::endl;
}

void demonstrateDateTimeValidity() {
    printSection("6. DateTime Validity");

    QDateTime valid(2024, 6, 15);
    QDateTime invalid;

    std::cout << "Valid datetime:" << std::endl;
    std::cout << "  isValid(): " << (valid.isValid() ? "true" : "false")
              << std::endl;
    std::cout << "  toString(): " << valid.toString() << std::endl;

    std::cout << "\nDefault (invalid) datetime:" << std::endl;
    std::cout << "  isValid(): " << (invalid.isValid() ? "true" : "false")
              << std::endl;
}

void demonstrateDayOfWeek() {
    printSection("7. Day of Week");

    std::cout << "Days of the week for dates in June 2024:" << std::endl;

    const char* dayNames[] = {"Sunday",   "Monday", "Tuesday", "Wednesday",
                              "Thursday", "Friday", "Saturday"};

    for (int day = 1; day <= 7; ++day) {
        QDateTime dt(2024, 6, day);
        int dow = dt.dayOfWeek();
        std::cout << "  June " << day << ", 2024: " << dayNames[dow]
                  << std::endl;
    }
}

void demonstrateUnixTimestamp() {
    printSection("8. Unix Timestamp");

    QDateTime now = QDateTime::currentDateTime();

    std::cout << "Current datetime: " << now.toString() << std::endl;
    std::cout << "Unix timestamp: " << now.toSecsSinceEpoch() << std::endl;
    std::cout << "Milliseconds since epoch: " << now.toMSecsSinceEpoch()
              << std::endl;

    std::cout << "\n--- From timestamp ---" << std::endl;
    int64_t timestamp = 1700000000;  // Example timestamp
    QDateTime fromTs = QDateTime::fromSecsSinceEpoch(timestamp);
    std::cout << "Timestamp " << timestamp << " = " << fromTs.toString()
              << std::endl;
}

void demonstrateRealWorldExamples() {
    printSection("9. Real-World Examples");

    std::cout << "--- Age calculation ---" << std::endl;
    QDateTime birthDate(1990, 5, 15);
    QDateTime today = QDateTime::currentDateTime();

    int years = today.date().year - birthDate.date().year;
    if (today.date().month < birthDate.date().month ||
        (today.date().month == birthDate.date().month &&
         today.date().day < birthDate.date().day)) {
        years--;
    }
    std::cout << "  Birth date: " << birthDate.toString("yyyy-MM-dd")
              << std::endl;
    std::cout << "  Age: " << years << " years" << std::endl;

    std::cout << "\n--- Days until event ---" << std::endl;
    QDateTime christmas(today.date().year, 12, 25);
    if (christmas < today) {
        christmas = christmas.addYears(1);
    }
    auto diff = christmas.toSecsSinceEpoch() - today.toSecsSinceEpoch();
    int daysUntil = static_cast<int>(diff / 86400);
    std::cout << "  Days until Christmas: " << daysUntil << std::endl;

    std::cout << "\n--- Log timestamp ---" << std::endl;
    QDateTime logTime = QDateTime::currentDateTime();
    std::cout << "  [" << logTime.toString("yyyy-MM-dd HH:mm:ss.zzz")
              << "] Log message" << std::endl;
}

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "  QDateTime Examples" << std::endl;
    std::cout << "========================================" << std::endl;

    try {
        demonstrateCurrentDateTime();
        demonstrateDateTimeConstruction();
        demonstrateDateTimeArithmetic();
        demonstrateDateTimeComparison();
        demonstrateDateTimeFormatting();
        demonstrateDateTimeValidity();
        demonstrateDayOfWeek();
        demonstrateUnixTimestamp();
        demonstrateRealWorldExamples();

        std::cout << "\n========================================" << std::endl;
        std::cout << "  All QDateTime examples completed!" << std::endl;
        std::cout << "========================================" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
