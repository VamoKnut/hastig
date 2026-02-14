#pragma once

#include <Arduino.h>

/**
 * @brief Device identity helpers for Hastig.
 *
 * Provides a stable hardware-derived identifier that survives resets.
 * (MCU unique ID registers on STM32H7).
 */
namespace DeviceIdentity {

/** @brief Get a stable hardware ID string (hex). */
void getHardwareId(char* out, size_t outSize);

}  // namespace DeviceIdentity
