#pragma once

#include <Arduino.h>
#include <Arduino_PowerManagement.h>

#include "AppConfig.h"

class SettingsManager;

/**
 * @brief Hardware-near helpers for the current board.
 *
 * Keep this HAL "dumb": pin configuration, simple GPIO helpers,
 * and wrappers for board-specific peripherals (battery/charger, LEDs,
 * external power rails, etc.). Policy belongs elsewhere.
 */
class BoardHal {
public:
  enum class LedColor : uint8_t { Red, Green, Blue };
  enum class Button : uint8_t { Left, Right, Up, Down };

  /**
   * @brief Pop next debounced button event (posted from IRQ).
   * @return true if an event was returned.
   */
  static bool popButton(Button& outButton);

  /**
   * @brief Wait for a button press event to occur (IRQ-based). Returns true if any event occurred.
   */
  static bool waitForButtonEvent(uint32_t timeoutMs);

  struct BatterySnapshot {
    float voltage        = 0.0f;
    float minimumVoltage = 0.0f;
    float current        = 0.0f;
    float averageCurrent = 0.0f;
  };

  /** Configure all GPIO directions and default states early in setup(). */
  static void configurePins();

  /** LED helpers. Portenta LEDs are active-low; callers use colors only. */
  static void setLed(LedColor color, bool on);
  static void blinkLed(LedColor color, uint32_t ms, uint8_t times = 1, uint32_t gapMs = 60);
  static void blinkDualLedFeedback(uint32_t durationMs = 5000u, uint32_t intervalMs = 250u);

  /** External sensor/RS485 rail control. */
  static void setSensorPower(bool powerOn = true);

  /** Battery/charger wrappers (Portenta-specific via Arduino_PowerManagement). */
  static BatterySnapshot readBattery(Battery& battery);
  static void resetBatteryStatistics(Battery& battery);
  static void configurePmicFromSettings(const SettingsManager& settingsManager, Battery& battery, Charger& charger);
  static void getHardwareId(char* out, size_t outSize);

  /**
   * Detect factory reset combo at boot (hold UP+DOWN for holdMs).
   * @return true if combo held for holdMs within windowMs.
   */
  static bool detectFactoryResetButtonCombo(uint32_t windowMs = 3500u, uint32_t holdMs = 3000u);

  /**
   * Prepare for IRQ-based button press detection (active-low FALLING edge).
   * Does not change existing polling-based behavior.
   */
  static void enableButtonIrq();
};
