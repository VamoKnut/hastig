#pragma once

#include <Arduino_PowerManagement.h>

#include "RgbLedStrip.h"

/**
 * @brief Global accessors for shared board, battery, and LED-driver resources.
 */
Board&   hastig_board();
Battery& hastig_battery();
RgbLedStrip& hastig_led_strip();
