#pragma once

#include <stdint.h>

#include <mbed.h>
#include <Arduino_PowerManagement.h>

/**
 * @brief Utilities for preparing Portenta H7 for low-power hibernate.
 *
 * Note:
 * We execute the final standby call from Arduino loop() for best current draw.
 */
namespace powerutil {

/**
 * @brief Prepare pins that must be in a defined state during hibernate.
 */
void preparePinsForLowPower(mbed::DigitalIn& wakeInput);

/**
 * @brief Enter deep standby until a wake event occurs.
 *
 * - Cuts board rails/peripherals
 * - Arms wake-on-pin and RTC wake (duration)
 * - Calls standByUntilWakeupEvent()
 *
 * @param board Board power object.
 * @param wakePin mbed pin name used as wake pin.
 * @param expectedDurationS Desired hibernate duration in seconds (clamped to 12h).
 */
void hibernate(Board& board, PinName wakePin, uint32_t expectedDurationS);

} // namespace powerutil
