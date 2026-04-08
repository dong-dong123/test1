#include "ButtonDriver.h"

ButtonDriver::ButtonDriver(int pin, PullMode pullMode, unsigned long debounceMs)
    : m_pin(pin)
    , m_pullMode(pullMode)
    , m_debounceMs(debounceMs)
    , m_holdTimeMs(0)
    , m_lastStableState(false)
    , m_lastRawState(false)
    , m_lastDebounceTime(0)
    , m_pressStartTime(0)
    , m_holdEventTriggered(false)
    , m_lastEvent(Event::NONE)
{
    // For pull-up mode, the resting state is HIGH (button not pressed)
    // For pull-down mode, the resting state is LOW (button not pressed)
    m_lastStableState = (m_pullMode == PullMode::PULL_UP);
}

bool ButtonDriver::begin()
{
    if (m_pin < 0)
    {
        // No button configured
        return true;
    }

    switch (m_pullMode)
    {
    case PullMode::PULL_UP:
        pinMode(m_pin, INPUT_PULLUP);
        break;
    case PullMode::PULL_DOWN:
        pinMode(m_pin, INPUT_PULLDOWN);
        break;
    case PullMode::NONE:
        pinMode(m_pin, INPUT);
        break;
    }

    // Read initial state
    m_lastRawState = readRawState();
    m_lastStableState = m_lastRawState;

    return true;
}

ButtonDriver::Event ButtonDriver::update()
{
    if (m_pin < 0)
    {
        return Event::NONE;
    }

    bool currentRawState = readRawState();
    unsigned long currentTime = millis();

    // Check for state change (for debouncing)
    if (currentRawState != m_lastRawState)
    {
        m_lastDebounceTime = currentTime;
    }

    // Debounce period has elapsed, update stable state if different
    if ((currentTime - m_lastDebounceTime) > m_debounceMs)
    {
        if (currentRawState != m_lastStableState)
        {
            m_lastStableState = currentRawState;

            // Detect edges based on pull mode
            if (m_pullMode == PullMode::PULL_UP)
            {
                // Pull-up: pressed = LOW, released = HIGH
                if (m_lastStableState == false) // LOW = pressed
                {
                    m_lastEvent = Event::PRESSED;
                    m_pressStartTime = currentTime;
                    m_holdEventTriggered = false;
                }
                else // HIGH = released
                {
                    m_lastEvent = Event::RELEASED;
                    m_pressStartTime = 0;
                }
            }
            else // PULL_DOWN or NONE (assuming active HIGH)
            {
                // Pull-down: pressed = HIGH, released = LOW
                if (m_lastStableState == true) // HIGH = pressed
                {
                    m_lastEvent = Event::PRESSED;
                    m_pressStartTime = currentTime;
                    m_holdEventTriggered = false;
                }
                else // LOW = released
                {
                    m_lastEvent = Event::RELEASED;
                    m_pressStartTime = 0;
                }
            }
        }
    }

    // Check for hold event
    if (m_lastStableState == ((m_pullMode == PullMode::PULL_UP) ? false : true))
    {
        // Button is currently pressed
        if (m_holdTimeMs > 0 && !m_holdEventTriggered)
        {
            if ((currentTime - m_pressStartTime) > m_holdTimeMs)
            {
                m_lastEvent = Event::HELD;
                m_holdEventTriggered = true;
            }
        }
    }

    m_lastRawState = currentRawState;
    return m_lastEvent;
}

bool ButtonDriver::isPressed() const
{
    if (m_pin < 0)
    {
        return false;
    }

    // For pull-up mode, pressed = LOW, so !m_lastStableState
    // For pull-down mode, pressed = HIGH, so m_lastStableState
    if (m_pullMode == PullMode::PULL_UP)
    {
        return !m_lastStableState;
    }
    else
    {
        return m_lastStableState;
    }
}

unsigned long ButtonDriver::getHoldTime() const
{
    if (!isPressed() || m_pressStartTime == 0)
    {
        return 0;
    }

    return millis() - m_pressStartTime;
}

void ButtonDriver::setHoldTime(unsigned long holdTimeMs)
{
    m_holdTimeMs = holdTimeMs;
}

ButtonDriver::Event ButtonDriver::getLastEvent() const
{
    return m_lastEvent;
}

void ButtonDriver::clearEvent()
{
    m_lastEvent = Event::NONE;
}

bool ButtonDriver::readRawState() const
{
    if (m_pin < 0)
    {
        return false;
    }

    return digitalRead(m_pin);
}