#pragma once

#include <Arduino_PowerManagement.h>

#include "KTD2052.h"

/**
 * @brief Global accessors for shared board, battery, and LED-driver resources.
 */
Board&   hastig_board();
Battery& hastig_battery();
KTD2052& hastig_led_driver();
