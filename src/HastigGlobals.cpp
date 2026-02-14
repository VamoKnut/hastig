#include "HastigGlobals.h"

// Defined in main.cpp
extern Board   g_board;
extern Battery g_battery;

Board& hastig_board()
{
  return g_board;
}

Battery& hastig_battery()
{
  return g_battery;
}
