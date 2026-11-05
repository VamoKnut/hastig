#pragma once

#include <Arduino.h>
#include <U8g2lib.h>

#include <stddef.h>
#include <vector>

#include "Messages.h"
#include "RuntimeStatus.h"

class MenuNode;
class IMenuItemSelectedEventListener;

class Display
{
public:
  static Display& getInstance();

  void beginHardware();
  void init(U8G2* oled);
  void turnOn(bool status = true);
  void setFirmwareRevision(const char* revisionText);

  void listMenuItems(MenuNode* selectedNode);
  void listSelectableItems(MenuNode* selectedNode, IMenuItemSelectedEventListener* dcp);
  void renderStatusAware(uint32_t remainingMs, bool communicationReady);
  void renderStatusSampling(const SensorSampleMsg& sample, bool hasSample);
  void renderDeviceInfo(const RuntimeStatus::DeviceInfo& info);
  void renderTextEditor(const char* settingName,
                        const char* settingValue,
                        const char* const* gridCells,
                        size_t cellCount,
                        size_t selectedIndex,
                        size_t columns,
                        size_t cursorPosition);

  void showSplash();
  void showMessage(const char* text);
  void showProgress(const char* text, unsigned int cur, unsigned int max = 100);
  void terminalTest(const std::vector<String>& lines, int lineOffset);

private:
  Display();

  U8G2* _oled;
  bool  _hardwareStarted;
  char  _firmwareRevision[32];

  void drawListMarker(bool selected, unsigned int x, unsigned int y, unsigned int size);
  void drawCentered(const char* text, uint8_t y);
};
