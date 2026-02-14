#pragma once

#include "SettingsManager.h"

#include <Arduino.h>

/**
 * @brief Print current settings to serial.
 *
 * Secrets are masked.
 */
void printSettingsToSerial(const SettingsManager& settingsManager, Stream& out);

/**
 * @brief Non-blocking serial console handler.
 *
 * Supported commands:
 * - help / ?
 * - show / config / settings
 */
void handleSerialConsole(SettingsManager& settingsManager);
