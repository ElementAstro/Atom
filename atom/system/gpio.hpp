#ifndef ATOM_SYSTEM_GPIO_HPP
#define ATOM_SYSTEM_GPIO_HPP

#include <functional>
#include <memory>
#include <string>

namespace atom::system {

/**
 * @class GPIO
 * @brief A class to manage GPIO (General Purpose Input/Output) pins.
 */
class GPIO {
public:
    /**
     * @brief Constructs a GPIO object for a specific pin.
     * @param pin The pin number as a string.
     */
    GPIO(const std::string& pin);

    /**
     * @brief Destructs the GPIO object.
     */
    ~GPIO();

    /**
     * @brief Sets the value of the GPIO pin.
     * @param value The value to set (true for HIGH, false for LOW).
     */
    void setValue(bool value);

    /**
     * @brief Gets the current value of the GPIO pin.
     * @return The current value of the pin (true for HIGH, false for LOW).
     */
    bool getValue();

    /**
     * @brief Sets the direction of the GPIO pin.
     * @param direction The direction to set ("in" for input, "out" for output).
     */
    void setDirection(const std::string& direction);

    /**
     * @brief Sets up a notification callback for changes on the GPIO pin.
     * @param pin The pin number as a string.
     * @param callback The callback function to call when the pin value changes.
     */
    static void notifyOnChange(const std::string& pin,
                               std::function<void(bool)> callback);

private:
    class Impl;  ///< Forward declaration of the implementation class.
    std::unique_ptr<Impl> impl_;  ///< Pointer to the implementation.
};

}  // namespace atom::system

#endif  // ATOM_SYSTEM_GPIO_HPP