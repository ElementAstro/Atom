#ifndef ATOM_SYSTEM_GPIO_HPP
#define ATOM_SYSTEM_GPIO_HPP

#include <chrono>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace atom::system {

/**
 * @class GPIO
 * @brief A class to manage GPIO (General Purpose Input/Output) pins.
 */
class GPIO {
public:
    /**
     * @enum Direction
     * @brief GPIO pin direction.
     */
    enum class Direction {
        INPUT,  ///< Input mode
        OUTPUT  ///< Output mode
    };

    /**
     * @enum Edge
     * @brief GPIO pin edge detection mode.
     */
    enum class Edge {
        NONE,     ///< No edge detection
        RISING,   ///< Rising edge detection
        FALLING,  ///< Falling edge detection
        BOTH      ///< Both edges detection
    };

    /**
     * @enum PullMode
     * @brief GPIO pin pull-up/down resistor mode.
     */
    enum class PullMode {
        NONE,  ///< No pull-up/down
        UP,    ///< Pull-up resistor
        DOWN   ///< Pull-down resistor
    };

    /**
     * @enum PwmMode
     * @brief PWM (Pulse Width Modulation) operation mode.
     */
    enum class PwmMode {
        HARDWARE,  ///< Use hardware PWM if available
        SOFTWARE   ///< Use software PWM implementation
    };

    /**
     * @brief Constructs a GPIO object for a specific pin.
     * @param pin The pin number as a string.
     */
    explicit GPIO(const std::string& pin);

    /**
     * @brief Constructs a GPIO object with specific configuration.
     * @param pin The pin number as a string.
     * @param direction The direction of the pin.
     * @param initialValue The initial value for output pins.
     */
    GPIO(const std::string& pin, Direction direction,
         bool initialValue = false);

    /**
     * @brief Destructs the GPIO object.
     */
    ~GPIO();

    /**
     * @brief Move constructor.
     */
    GPIO(GPIO&& other) noexcept;

    /**
     * @brief Move assignment operator.
     */
    GPIO& operator=(GPIO&& other) noexcept;

    /**
     * @brief Sets the value of the GPIO pin.
     * @param value The value to set (true for HIGH, false for LOW).
     */
    void setValue(bool value);

    /**
     * @brief Gets the current value of the GPIO pin.
     * @return The current value of the pin (true for HIGH, false for LOW).
     */
    bool getValue() const;

    /**
     * @brief Sets the direction of the GPIO pin.
     * @param direction The direction to set.
     */
    void setDirection(Direction direction);

    /**
     * @brief Gets the current direction of the GPIO pin.
     * @return The current direction of the pin.
     */
    Direction getDirection() const;

    /**
     * @brief Sets the edge detection mode of the GPIO pin.
     * @param edge The edge detection mode to set.
     */
    void setEdge(Edge edge);

    /**
     * @brief Gets the current edge detection mode of the GPIO pin.
     * @return The current edge detection mode of the pin.
     */
    Edge getEdge() const;

    /**
     * @brief Sets the pull-up/down resistor mode of the GPIO pin.
     * @param mode The pull-up/down mode to set.
     */
    void setPullMode(PullMode mode);

    /**
     * @brief Gets the pull-up/down resistor mode of the GPIO pin.
     * @return The current pull-up/down mode of the pin.
     */
    PullMode getPullMode() const;

    /**
     * @brief Gets the pin number.
     * @return The pin number as a string.
     */
    std::string getPin() const;

    /**
     * @brief Toggles the value of the GPIO pin.
     * @return The new value of the pin after toggling.
     */
    bool toggle();

    /**
     * @brief Pulses the GPIO pin for a specified duration.
     * @param value The value to pulse (true for HIGH, false for LOW).
     * @param duration The duration of the pulse.
     */
    void pulse(bool value, std::chrono::milliseconds duration);

    /**
     * @brief Sets up a callback for pin value changes.
     * @param callback The callback function to call when the pin value changes.
     * @return True if the callback was successfully set up, false otherwise.
     */
    bool onValueChange(std::function<void(bool)> callback);

    /**
     * @brief Sets up a callback for specific edge changes.
     * @param edge The edge detection mode.
     * @param callback The callback function to call when the specified edge is
     * detected.
     * @return True if the callback was successfully set up, false otherwise.
     */
    bool onEdgeChange(Edge edge, std::function<void(bool)> callback);

    /**
     * @brief Stops all callbacks on this pin.
     */
    void stopCallbacks();

    /**
     * @brief Sets up a notification callback for changes on the GPIO pin.
     * @param pin The pin number as a string.
     * @param callback The callback function to call when the pin value changes.
     * @deprecated Use instance method onValueChange() instead.
     */
    static void notifyOnChange(const std::string& pin,
                               std::function<void(bool)> callback);

    /**
     * @brief Sets up PWM (Pulse Width Modulation) on the pin.
     * @param frequency The PWM frequency in hertz.
     * @param dutyCycle The duty cycle (0.0 to 1.0).
     * @param mode The PWM mode (hardware or software).
     * @return True if PWM was successfully set up, false otherwise.
     */
    bool setPwm(double frequency, double dutyCycle,
                PwmMode mode = PwmMode::HARDWARE);

    /**
     * @brief Updates the PWM duty cycle.
     * @param dutyCycle The new duty cycle (0.0 to 1.0).
     * @return True if successful, false otherwise.
     */
    bool updatePwmDutyCycle(double dutyCycle);

    /**
     * @brief Stops PWM operation.
     */
    void stopPwm();

    /**
     * @brief Implements button debouncing for input pins.
     * @param callback The function to call when a debounced press is detected.
     * @param debounceTimeMs The debounce time in milliseconds.
     * @return True if debouncing was successfully set up, false otherwise.
     */
    bool setupButtonDebounce(std::function<void()> callback,
                             unsigned int debounceTimeMs = 50);

    /**
     * @brief Sets up an interrupt counter for this pin.
     * @param edge The edge to count (RISING, FALLING, or BOTH).
     * @return True if the counter was successfully set up, false otherwise.
     */
    bool setupInterruptCounter(Edge edge = Edge::RISING);

    /**
     * @brief Gets the current interrupt count.
     * @param resetAfterReading Whether to reset the counter after reading.
     * @return The number of interrupts counted.
     */
    uint64_t getInterruptCount(bool resetAfterReading = false);

    /**
     * @brief Resets the interrupt counter to zero.
     */
    void resetInterruptCount();

    /**
     * @class GPIOGroup
     * @brief A utility class for managing multiple GPIO pins as a group.
     */
    class GPIOGroup {
    public:
        /**
         * @brief Constructs a GPIOGroup with specified pins.
         * @param pins The vector of pin numbers.
         */
        explicit GPIOGroup(const std::vector<std::string>& pins);

        /**
         * @brief Destructs the GPIOGroup.
         */
        ~GPIOGroup();

        /**
         * @brief Sets values for all pins in the group.
         * @param values Vector of boolean values for each pin.
         */
        void setValues(const std::vector<bool>& values);

        /**
         * @brief Gets values from all pins in the group.
         * @return Vector of boolean values from each pin.
         */
        std::vector<bool> getValues() const;

        /**
         * @brief Sets the same direction for all pins in the group.
         * @param direction The direction to set for all pins.
         */
        void setDirection(Direction direction);

    private:
        std::vector<std::unique_ptr<GPIO>> gpios_;
    };

    /**
     * @class ShiftRegister
     * @brief A utility class for managing shift registers (e.g., 74HC595).
     */
    class ShiftRegister {
    public:
        /**
         * @brief Constructs a ShiftRegister with specified pins.
         * @param dataPin The data pin (DS).
         * @param clockPin The clock pin (SH_CP).
         * @param latchPin The latch pin (ST_CP).
         * @param numBits The number of bits in the shift register chain.
         */
        ShiftRegister(const std::string& dataPin, const std::string& clockPin,
                      const std::string& latchPin, uint8_t numBits = 8);

        /**
         * @brief Destructs the ShiftRegister.
         */
        ~ShiftRegister();

        /**
         * @brief Shifts out data to the register.
         * @param data The data to shift out.
         * @param bitOrder True for MSB first, false for LSB first.
         */
        void shiftOut(uint32_t data, bool msbFirst = true);

        /**
         * @brief Sets a single bit in the shift register.
         * @param position The bit position (0-based).
         * @param value The value to set (true/false).
         */
        void setBit(uint8_t position, bool value);

        /**
         * @brief Gets the current state of the output register.
         * @return The current register state.
         */
        uint32_t getState() const;

        /**
         * @brief Clears all bits in the register (sets to 0).
         */
        void clear();

    private:
        std::unique_ptr<GPIO> dataPin_;   ///< Data pin (DS)
        std::unique_ptr<GPIO> clockPin_;  ///< Clock pin (SH_CP)
        std::unique_ptr<GPIO> latchPin_;  ///< Latch pin (ST_CP)
        uint8_t numBits_;                 ///< Number of bits in the register
        uint32_t state_;                  ///< Current register state
    };

private:
    class Impl;  ///< Forward declaration of the implementation class.
    std::unique_ptr<Impl> impl_;  ///< Pointer to the implementation.

    GPIO(const GPIO&) = delete;
    GPIO& operator=(const GPIO&) = delete;
};

/**
 * @brief Converts string to GPIO::Direction enumeration.
 * @param direction The direction as a string ("in" or "out").
 * @return The corresponding Direction enumeration.
 */
GPIO::Direction stringToDirection(const std::string& direction);

/**
 * @brief Converts GPIO::Direction enumeration to string.
 * @param direction The Direction enumeration.
 * @return The corresponding direction as a string.
 */
std::string directionToString(GPIO::Direction direction);

/**
 * @brief Converts string to GPIO::Edge enumeration.
 * @param edge The edge as a string ("none", "rising", "falling", or "both").
 * @return The corresponding Edge enumeration.
 */
GPIO::Edge stringToEdge(const std::string& edge);

/**
 * @brief Converts GPIO::Edge enumeration to string.
 * @param edge The Edge enumeration.
 * @return The corresponding edge as a string.
 */
std::string edgeToString(GPIO::Edge edge);

}  // namespace atom::system

#endif  // ATOM_SYSTEM_GPIO_HPP