#include "Display.h"

#include "LcdMenu.h"
#include "vamotech_bmp.h"

#include <stdio.h>

#define DISP_FONT_MENU_SELECTED u8g2_font_helvB10_tf // u8g2_font_t0_17b_mr // u8g2_font_helvB08_te
#define DISP_FONT_MENU_NORMAL u8g2_font_helvR10_tf // u8g2_font_t0_18_tf    // u8g2_font_helvR08_tf
#define DISP_FONT_OPTION_LIST_HEADING u8g2_font_squeezed_b7_tr
#define DISP_FONT_LIST u8g2_font_resoledmedium_tr // u8g2_font_5x8_tn
#define DISP_FONT_LIST_SELECTED u8g2_font_resoledbold_tr

#define DISP_MENU_TOP_MARGIN 2
#define DISP_MENU_LINE_HEIGHT 20
#define DISP_MENU_LEFT_MARGIN 2
#define DISP_MENU_SELECTED_INDENT 4
#define DISP_TERMINAL_LINES 8
#define DISP_LIST_LINE_STEP 11
#define DISP_LIST_MARKER_SIZE 7
#define DISP_REV_BOX_W 44
#define DISP_REV_BOX_H 10
#define DISP_REV_TEXT_X 1
#define DISP_REV_TEXT_Y 8
#define DISP_EDITOR_LINE1_Y 0
#define DISP_EDITOR_LINE2_Y 12
#define DISP_EDITOR_GRID_Y 24
#define DISP_EDITOR_GRID_STEP_Y 12
#define DISP_EDITOR_CELL_W 8
#define DISP_EDITOR_CELL_H 10

namespace
{
// Shared display instance used by both early setup() splash and UiThread rendering.
//static U8G2_SSD1306_128X64_NONAME_F_HW_I2C g_u8g2(U8G2_R0, U8X8_PIN_NONE);
static U8G2_SH1106_128X64_NONAME_F_HW_I2C g_u8g2(U8G2_R0, U8X8_PIN_NONE);
} // namespace

Display& Display::getInstance()
{
   static Display instance;
   return instance;
}

Display::Display() : _oled(nullptr), _hardwareStarted(false)
{
}

void Display::beginHardware()
{
   if (!_hardwareStarted)
   {
      g_u8g2.begin();
      _hardwareStarted = true;
   }
   init(&g_u8g2);
   turnOn(true);
}

void Display::init(U8G2* oled)
{
   _oled = oled;
}

void Display::turnOn(bool status)
{
   if (_oled == nullptr)
   {
      return;
   }
   _oled->setPowerSave(status ? 0 : 1);
}

void Display::drawCentered(const char* text, uint8_t y)
{
   if (_oled == nullptr)
   {
      return;
   }
   if (text == nullptr)
   {
      text = "";
   }
   const int w = _oled->getStrWidth(text);
   const int x = (_oled->getDisplayWidth() - w) / 2;
   _oled->drawStr(x > 0 ? x : 0, y, text);
}

void Display::listMenuItems(MenuNode* selectedNode)
{
   if (_oled == nullptr || selectedNode == nullptr)
   {
      return;
   }

   _oled->clearBuffer();
   _oled->setFontPosTop();
   _oled->setFont(DISP_FONT_MENU_SELECTED);
   _oled->drawStr(DISP_MENU_LEFT_MARGIN + DISP_MENU_SELECTED_INDENT,
                  DISP_MENU_TOP_MARGIN + DISP_MENU_LINE_HEIGHT, selectedNode->getLabel().c_str());

   _oled->setFont(DISP_FONT_MENU_NORMAL);
   if (selectedNode->getPrevSibling() != nullptr)
   {
      _oled->drawStr(DISP_MENU_LEFT_MARGIN, DISP_MENU_TOP_MARGIN,
                     selectedNode->getPrevSibling()->getLabel().c_str());
   }
   if (selectedNode->getNextSibling() != nullptr)
   {
      _oled->drawStr(DISP_MENU_LEFT_MARGIN, DISP_MENU_TOP_MARGIN + (2 * DISP_MENU_LINE_HEIGHT),
                     selectedNode->getNextSibling()->getLabel().c_str());
   }

   _oled->sendBuffer();
}

void Display::drawListMarker(bool selected, unsigned int x, unsigned int y, unsigned int size)
{
   if (_oled == nullptr)
   {
      return;
   }

   _oled->drawFrame((uint8_t)x, (uint8_t)y, (uint8_t)size, (uint8_t)size);
   if (selected)
   {
      _oled->drawLine((uint8_t)x, (uint8_t)y, (uint8_t)(x + size - 1u), (uint8_t)(y + size - 1u));
      _oled->drawLine((uint8_t)x, (uint8_t)(y + size - 1u), (uint8_t)(x + size - 1u), (uint8_t)y);
   }
}

void Display::listSelectableItems(MenuNode* selectedNode, IMenuItemSelectedEventListener* dcp)
{
   if (_oled == nullptr || selectedNode == nullptr)
   {
      return;
   }
   if (selectedNode->getParent() == nullptr)
   {
      listMenuItems(selectedNode);
      return;
   }

   _oled->clearBuffer();
   _oled->setFont(DISP_FONT_OPTION_LIST_HEADING);
   _oled->setFontPosTop();

   drawCentered(selectedNode->getParent()->getLabel().c_str(), DISP_MENU_TOP_MARGIN);

   const unsigned int selectedIndex  = selectedNode->positionInSiblinglist();
   unsigned int       selectedLineNo = 2;
   if (selectedIndex < 2u)
   {
      selectedLineNo = 1;
   }

   for (MenuNode* currentNode = selectedNode->getParent()->getChild(); currentNode != nullptr;
        currentNode           = currentNode->getNextSibling())
   {
      const unsigned int siblingIndex = currentNode->positionInSiblinglist();
      const int currentLineNo = (int)siblingIndex - (int)selectedIndex + (int)selectedLineNo;
      if (currentLineNo < 1 || currentLineNo > 4)
      {
         continue;
      }

      const int yPos =
          DISP_MENU_TOP_MARGIN + DISP_MENU_LINE_HEIGHT + (currentLineNo - 1) * DISP_LIST_LINE_STEP;

      if (currentNode == selectedNode)
      {
         _oled->setFont(DISP_FONT_LIST_SELECTED);
         _oled->drawStr(12 + DISP_MENU_LEFT_MARGIN + DISP_MENU_SELECTED_INDENT, yPos, currentNode->getLabel().c_str());
      }
      else
      {
         _oled->setFont(DISP_FONT_LIST);
         _oled->drawStr(12 + DISP_MENU_LEFT_MARGIN, yPos, currentNode->getLabel().c_str());
      }

      bool isSelected = false;
      if (dcp != nullptr)
      {
         isSelected = dcp->onIsItemSelectedEvent(currentNode->getMeta()["retval"]);
      }
      drawListMarker(isSelected, DISP_MENU_LEFT_MARGIN, (unsigned int)(yPos), DISP_LIST_MARKER_SIZE);
   }

   _oled->sendBuffer();
}

void Display::renderStatusAware(uint32_t remainingMs)
{
   if (_oled == nullptr)
   {
      return;
   }

   const uint32_t totalSec = remainingMs / 1000u;
   const uint32_t mm = totalSec / 60u;
   const uint32_t ss = totalSec % 60u;

   char timeBuf[12];
   snprintf(timeBuf, sizeof(timeBuf), "%02lu:%02lu", (unsigned long)mm, (unsigned long)ss);

   _oled->clearBuffer();
   _oled->setFontPosTop();
   _oled->setFont(DISP_FONT_MENU_NORMAL);
   drawCentered("AWARE", 0);

   _oled->setFont(DISP_FONT_LIST);
   _oled->drawStr(DISP_MENU_LEFT_MARGIN, 18, "Time til hiberate");
   drawCentered(timeBuf, 30);
   _oled->sendBuffer();
}

void Display::renderStatusSampling(const SensorSampleMsg& sample, bool hasSample)
{
   if (_oled == nullptr)
   {
      return;
   }

   _oled->clearBuffer();
   _oled->setFontPosTop();
   _oled->setFont(DISP_FONT_MENU_NORMAL);
   drawCentered("SAMPLING", 0);

   _oled->setFont(DISP_FONT_LIST);

   if (!hasSample)
   {
      _oled->drawStr(DISP_MENU_LEFT_MARGIN, 18, "No sample");
      _oled->sendBuffer();
      return;
   }

   const char* k0 = (sample.k0[0] != '\0') ? sample.k0 : "k0";
   const char* k1 = (sample.k1[0] != '\0') ? sample.k1 : "k1";
   char line0[28];
   char line1[28];
   snprintf(line0, sizeof(line0), "%s: %.2f", k0, (double)sample.v0);
   snprintf(line1, sizeof(line1), "%s: %.2f", k1, (double)sample.v1);
   _oled->drawStr(DISP_MENU_LEFT_MARGIN, 18, line0);
   _oled->drawStr(DISP_MENU_LEFT_MARGIN, 30, line1);
   _oled->sendBuffer();
}

void Display::renderTextEditor(const char* settingName,
                               const char* settingValue,
                               const char* const* gridCells,
                               size_t cellCount,
                               size_t selectedIndex,
                               size_t columns,
                               size_t cursorPosition)
{
   if (_oled == nullptr || gridCells == nullptr || columns == 0u)
   {
      return;
   }

   _oled->clearBuffer();
   _oled->setFontPosTop();
   _oled->setFont(DISP_FONT_LIST_SELECTED);

   const char* safeName  = (settingName != nullptr) ? settingName : "";
   const char* safeValue = (settingValue != nullptr) ? settingValue : "";

   drawCentered(safeName, DISP_EDITOR_LINE1_Y);

   _oled->setFont(DISP_FONT_LIST);

   String valueLine(safeValue);
   if (valueLine.length() > 18u)
   {
      valueLine = valueLine.substring(0u, 18u);
   }

   int x = DISP_MENU_LEFT_MARGIN;
   const int y = DISP_EDITOR_LINE2_Y;
   for (size_t i = 0u; i < valueLine.length(); ++i)
   {
      const char c = valueLine.charAt(i);
      char chr[2] = {c, '\0'};
      int cw = _oled->getStrWidth(chr);
      if (cw < 4) {
         cw = 4;
      }

      if (i == cursorPosition)
      {
         _oled->setDrawColor(1);
         _oled->drawBox(x, y, cw + 1, DISP_EDITOR_CELL_H);
         _oled->setDrawColor(0);
         _oled->drawStr(x, y + 1, chr);
         _oled->setDrawColor(1);
      }
      else
      {
         _oled->drawStr(x, y + 1, chr);
      }
      x += cw;
   }
   if (cursorPosition >= valueLine.length())
   {
      _oled->setDrawColor(1);
      _oled->drawFrame(x, y, 4, DISP_EDITOR_CELL_H);
   }

   const size_t maxCells = columns * 3u;
   const size_t drawCount = (cellCount < maxCells) ? cellCount : maxCells;

   for (size_t i = 0; i < drawCount; ++i)
   {
      const size_t row = i / columns;
      const size_t col = i % columns;
      const int x = (int)(col * DISP_EDITOR_CELL_W);
      const int y = DISP_EDITOR_GRID_Y + (int)(row * DISP_EDITOR_GRID_STEP_Y);

      if (i == selectedIndex)
      {
         _oled->setDrawColor(1);
         _oled->drawBox(x, y, DISP_EDITOR_CELL_W, DISP_EDITOR_CELL_H);
         _oled->setDrawColor(0);
      }

      const char* label = (gridCells[i] != nullptr) ? gridCells[i] : " ";
      _oled->drawStr(x + 1, y + 1, label);

      if (i == selectedIndex)
      {
         _oled->setDrawColor(1);
      }
   }

   _oled->sendBuffer();
}

void Display::terminalTest(const std::vector<String>& lines, int lineOffset)
{
   if (_oled == nullptr)
   {
      return;
   }

   _oled->clearBuffer();
   _oled->setFont(DISP_FONT_LIST);

   int displayLine = (int)lines.size() - lineOffset - 1;
   if (displayLine > DISP_TERMINAL_LINES)
   {
      displayLine = DISP_TERMINAL_LINES;
   }

   int bufferLine = (int)lines.size() - 1 - lineOffset;
   while (displayLine >= 0 && bufferLine >= 0)
   {
      const int yPos = (_oled->getDisplayHeight() / DISP_TERMINAL_LINES) * displayLine + 8;
      _oled->drawStr(DISP_MENU_LEFT_MARGIN, yPos, lines[(size_t)bufferLine].c_str());
      displayLine--;
      bufferLine--;
   }

   _oled->sendBuffer();
}

void Display::showSplash(const char* revisionText)
{
   if (_oled == nullptr)
   {
      beginHardware();
      if (_oled == nullptr)
      {
         return;
      }
   }

   _oled->clearBuffer();
   _oled->drawXBMP(0, 0, 128, 64, epd_bitmap_Hastig_v1);

   if (revisionText != nullptr && revisionText[0] != '\0')
   {
      _oled->setDrawColor(0);
      _oled->drawBox(0, 0, DISP_REV_BOX_W, DISP_REV_BOX_H);
      _oled->setDrawColor(1);
      _oled->setFont(DISP_FONT_LIST);
      _oled->drawStr(DISP_REV_TEXT_X, DISP_REV_TEXT_Y, revisionText);
   }

   _oled->sendBuffer();
}

void Display::showMessage(const char* text)
{
   if (_oled == nullptr)
   {
      return;
   }

   _oled->clearBuffer();
   _oled->setFont(DISP_FONT_MENU_NORMAL);
   drawCentered(text, 20);
   _oled->sendBuffer();
}

void Display::showProgress(const char* text, unsigned int cur, unsigned int max)
{
   if (_oled == nullptr || max == 0u)
   {
      return;
   }

   const unsigned int progress = (cur * 100u) / max;
   const unsigned int barWidth = (unsigned int)(_oled->getDisplayWidth() - 8);
   const unsigned int filled   = (barWidth * progress) / 100u;

   _oled->clearBuffer();
   _oled->setFont(DISP_FONT_MENU_NORMAL);
   drawCentered(text, 14);
   _oled->drawFrame(4, 24, (uint8_t)barWidth, 12);
   if (filled > 0u)
   {
      _oled->drawBox(5, 25, (uint8_t)(filled > 2u ? filled - 2u : 0u), 10);
   }
   _oled->sendBuffer();
}
