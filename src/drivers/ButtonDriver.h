#ifndef BUTTON_DRIVER_H
#define BUTTON_DRIVER_H

#include <Arduino.h>

/**
 * @brief Simple button driver with debouncing and edge detection
 *
 * This class handles button input with configurable pin, pull-up/down,
 * debounce time, and callback support for press/release events.
 */
class ButtonDriver
{
public:
    enum class PullMode {
        PULL_UP,    // Internal pull-up resistor (button connects to GND)
        PULL_DOWN,  // Internal pull-down resistor (button connects to VCC)
        NONE        // No internal pull, external resistor required
    };

    enum class Event {
        NONE,       // No event
        PRESSED,    // Button pressed (falling edge with debouncing)
        RELEASED,   // Button released (rising edge with debouncing)
        HELD        // Button held for specified duration
    };

    /**
     * @brief Construct a new ButtonDriver object
     *
     * @param pin GPIO pin number
     * @param pullMode Internal pull resistor mode (default: PULL_UP)
     * @param debounceMs Debounce time in milliseconds (default: 50ms)
     */
    ButtonDriver(int pin, PullMode pullMode = PullMode::PULL_UP, unsigned long debounceMs = 50);

    /**
     * @brief Initialize the button pin
     *
     * @return true if initialization successful, false otherwise
     */
    bool begin();

    /**
     * @brief Update button state (call in main loop)
     *
     * @return Event detected since last update
     */
    Event update();

    /**
     * @brief Check if button is currently pressed
     *
     * @return true if button is pressed, false otherwise
     */
    bool isPressed() const;

    /**
     * @brief Get the time the button has been held (if currently pressed)
     *
     * @return unsigned long hold time in milliseconds, 0 if not pressed
     */
    unsigned long getHoldTime() const;

    /**
     * @brief Set the hold event detection time
     *
     * @param holdTimeMs Time in milliseconds before HELD event is triggered (0 to disable)
     */
    void setHoldTime(unsigned long holdTimeMs);

    /**
     * @brief Get the last detected event (without clearing it)
     *
     * @return Event last detected event
     */
    Event getLastEvent() const;

    /**
     * @brief Clear the last detected event
     */
    void clearEvent();

private:
    int m_pin;
    PullMode m_pullMode;
    unsigned long m_debounceMs;
    unsigned long m_holdTimeMs;

    bool m_lastStableState;
    bool m_lastRawState;
    unsigned long m_lastDebounceTime;
    unsigned long m_pressStartTime;
    bool m_holdEventTriggered;
    Event m_lastEvent;

    /**
     * @brief Read raw button state (accounting for pull mode)
     *
     * @return true if button is active (pressed), false otherwise
     */
    bool readRawState() const;
};

#endif // BUTTON_DRIVER_H