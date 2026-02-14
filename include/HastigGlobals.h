#pragma once

#include <Arduino_PowerManagement.h>

/**
 * @brief Global accessors for board/battery (singletons owned by main.cpp).
 */
Board&   hastig_board();
Battery& hastig_battery();
