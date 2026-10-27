#include "HastigGlobals.h"

// Defined in main.cpp
extern Board   g_board;
extern Battery g_battery;

RgbLedStrip g_rgbLedStrip;

Board& hastig_board()
{
  return g_board;
}

Battery& hastig_battery()
{
  return g_battery;
}

RgbLedStrip& hastig_led_strip()
{
  return g_rgbLedStrip;
}
