#include "HastigGlobals.h"

// Defined in main.cpp
extern Board   g_board;
extern Battery g_battery;

KTD2052 g_ledDriver;

Board& hastig_board()
{
  return g_board;
}

Battery& hastig_battery()
{
  return g_battery;
}

KTD2052& hastig_led_driver()
{
  return g_ledDriver;
}
