#include "Display.h"

#include "LcdMenu.h"
#include "vamotech_bmp.h"

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
