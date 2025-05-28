#ifndef ATOM_UTILS_QTIMEZONE_HPP
#define ATOM_UTILS_QTIMEZONE_HPP

#include <chrono>
#include <concepts>
#include <ctime>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "atom/error/exception.hpp"

namespace atom::utils {
class GetTimeException : public error::Exception {
    using Exception::Exception;
};

#define THROW_GET_TIME_ERROR(...)                                       \
    throw atom::utils::GetTimeException(ATOM_FILE_NAME, ATOM_FILE_LINE, \
                                        ATOM_FUNC_NAME, __VA_ARGS__)

#define THROW_NESTED_GET_TIME_ERROR(...)          \
    atom::utils::GetTimeException::rethrowNested( \
        ATOM_FILE_NAME, ATOM_FILE_LINE, ATOM_FUNC_NAME, __VA_ARGS__)

template <typename T>
concept StringLike = std::convertible_to<T, std::string_view>;

class QDateTime;

/**
 * @brief A class representing a time zone.
 *
 * The `QTimeZone` class provides functionality for managing and interacting
 * with time zones. It includes methods to obtain time zone identifiers, offsets
 * from UTC, and information about daylight saving time.
 */
class QTimeZone {
public:
    /**
     * @brief Default constructor for `QTimeZone`.
     *
     * Initializes an invalid `QTimeZone` instance with no time zone identifier.
     */
    QTimeZone() noexcept;

    /**
     * @brief Constructs a `QTimeZone` object from a time zone identifier.
     *
     * @param timeZoneId The identifier of the time zone to use.
     * @throws error::Exception if the time zone ID is invalid.
     *
     * This constructor initializes the `QTimeZone` object using the specified
     * time zone identifier. The identifier should be a valid time zone ID
     * recognized by the system.
     */
    template <StringLike TZId>
    explicit QTimeZone(TZId&& timeZoneId);

    /**
     * @brief Returns a list of available time zone identifiers.
     *
     * @return A vector of strings, each representing a valid time zone
     * identifier.
     *
     * This static method provides a list of all time zone identifiers that are
     * available on the system. These identifiers can be used to create
     * `QTimeZone` objects.
     */
    static auto availableTimeZoneIds() noexcept -> std::vector<std::string>;

    /**
     * @brief Gets the time zone identifier.
     *
     * @return The time zone identifier as a string.
     *
     * This method returns the identifier of the time zone associated with the
     * `QTimeZone` object. The identifier is a string that represents the time
     * zone, such as "America/New_York".
     */
    [[nodiscard]] auto identifier() const noexcept -> std::string_view;

    /**
     * @brief Gets the time zone identifier as a string (alias for backward
     * compatibility).
     *
     * @return The time zone identifier as a string.
     */
    [[nodiscard]] auto id() const noexcept -> std::string {
        return std::string(identifier());
    }

    /**
     * @brief Gets the display name of the time zone.
     *
     * @return The display name of the time zone as a string.
     *
     * This method returns a human-readable name of the time zone, which is
     * often used for user interfaces.
     */
    [[nodiscard]] auto displayName() const noexcept -> std::string_view;

    /**
     * @brief Checks if the `QTimeZone` object is valid.
     *
     * @return `true` if the `QTimeZone` object is valid, `false` otherwise.
     *
     * This method determines whether the `QTimeZone` object represents a valid
     * time zone. An invalid time zone means the time zone ID was not recognized
     * or initialized properly.
     */
    [[nodiscard]] auto isValid() const noexcept -> bool;

    /**
     * @brief Gets the offset from UTC for a specific date and time.
     *
     * @param dateTime The `QDateTime` object for which to calculate the offset.
     * @throws GetTimeException if the time conversion fails.
     *
     * @return The offset from UTC as a `std::chrono::seconds` value.
     *
     * This method returns the offset from UTC for the given `QDateTime` object
     * in the time zone represented by the `QTimeZone` object. The offset may
     * vary depending on daylight saving time.
     */
    [[nodiscard]] auto offsetFromUtc(const QDateTime& dateTime) const
        -> std::chrono::seconds;

    /**
     * @brief Gets the standard time offset from UTC.
     *
     * @return The standard time offset from UTC as a `std::chrono::seconds`
     * value.
     *
     * This method returns the standard time offset from UTC for the time zone,
     * excluding any daylight saving time adjustments.
     */
    [[nodiscard]] auto standardTimeOffset() const noexcept
        -> std::chrono::seconds;

    /**
     * @brief Gets the daylight saving time offset from UTC.
     *
     * @return The daylight saving time offset from UTC as a
     * `std::chrono::seconds` value.
     *
     * This method returns the additional offset from UTC during daylight saving
     * time, if applicable.
     */
    [[nodiscard]] auto daylightTimeOffset() const noexcept
        -> std::chrono::seconds;

    /**
     * @brief Checks if the time zone observes daylight saving time.
     *
     * @return `true` if the time zone observes daylight saving time, `false`
     * otherwise.
     *
     * This method determines whether the time zone includes daylight saving
     * time adjustments.
     */
    [[nodiscard]] auto hasDaylightTime() const noexcept -> bool;

    /**
     * @brief Checks if a specific date and time is within the daylight saving
     * time period.
     *
     * @param dateTime The `QDateTime` object to check.
     * @throws GetTimeException if the time conversion fails.
     *
     * @return `true` if the specified date and time falls within the daylight
     * saving time period, `false` otherwise.
     *
     * This method checks whether the given `QDateTime` object falls within the
     * period when daylight saving time is observed in the time zone.
     */
    [[nodiscard]] auto isDaylightTime(const QDateTime& dateTime) const -> bool;

    auto operator<=>(const QTimeZone& other) const noexcept = delete;

private:
    std::string timeZoneId_;
    std::string displayName_;
    std::optional<std::chrono::seconds> offset_;

    mutable std::unordered_map<time_t, bool> dstCache_;

    void initialize();
};

}  // namespace atom::utils

#ifndef ATOM_UTILS_QTIMEZONE_TPP
#define ATOM_UTILS_QTIMEZONE_TPP

#include <algorithm>

namespace atom::utils {

template <StringLike TZId>
QTimeZone::QTimeZone(TZId&& timeZoneId) {
    std::string tzId{std::forward<TZId>(timeZoneId)};

    auto availableIds = availableTimeZoneIds();
    if (std::ranges::find(availableIds, tzId) == availableIds.end()) {
        THROW_INVALID_ARGUMENT("Invalid time zone ID: " + tzId);
    }

    timeZoneId_ = std::move(tzId);
    initialize();
}

}  // namespace atom::utils

#endif  // ATOM_UTILS_QTIMEZONE_TPP

#endif
