#include "BoardHal.h"

#include <rtos/EventFlags.h>
#include <stdio.h>

#include "HastigGlobals.h"
#include "Logger.h"
#include "SettingsManager.h"

// Portenta LEDs are active-low.
static const char* TAG = "BOARD";

namespace
{
static constexpr uint8_t KTD_LED_CONNECTION   = 1;
static constexpr uint8_t KTD_LED_SENSOR_POWER = 3;
static constexpr uint8_t KTD_LED_MAX          = KTD2052::MAX_CURRENT;
static rtos::Mutex       ledDriverMutex;
static bool              ledDriverReady           = false;
static bool              connectionLedStateValid  = false;
static bool              connectionLedErrorLogged = false;
static bool              sensorLedStateValid      = false;
static bool              sensorLedPowerOn         = false;
static uint8_t           connectionLedRgb[3]      = {0, 0, 0};

static mbed::DigitalOut ledRed(PIN_LED_RED, 1);
static mbed::DigitalOut ledGreen(PIN_LED_GREEN, 1);
static mbed::DigitalOut ledBlue(PIN_LED_BLUE, 1);
static mbed::DigitalOut sensorPowerEnable(PIN_POWER_ENABLE, 0);
static mbed::DigitalOut rs485DriverEnable(PIN_RS485_DE_RE, 0);

static mbed::InterruptIn buttonLeft(PIN_BTN_LEFT, PullUp);
static mbed::InterruptIn buttonRight(PIN_BTN_RIGHT, PullUp);
static mbed::InterruptIn buttonUp(PIN_BTN_UP, PullUp);
static mbed::InterruptIn buttonDown(PIN_BTN_DOWN, PullUp);

static mbed::DigitalOut& ledOutput(BoardHal::LedColor color)
{
   switch (color)
   {
   case BoardHal::LedColor::Red:
      return ledRed;
   case BoardHal::LedColor::Green:
      return ledGreen;
   case BoardHal::LedColor::Blue:
      return ledBlue;
   default:
      return ledRed;
   }
}

// IRQ-based button detection (ring-buffer mailbox).
static rtos::EventFlags        g_btnFlags;
static constexpr uint32_t      BTN_FLAG_ANY      = 1u;
static constexpr unsigned long kButtonDebounceMs = 180ul;

static volatile uint8_t g_btnHead = 0;
static volatile uint8_t g_btnTail = 0;
static BoardHal::Button g_btnQueue[16];
static unsigned long    g_btnLastPressMs[4] = {0, 0, 0, 0};
static bool             g_btnLatched[4]     = {false, false, false, false};

static mbed::InterruptIn& buttonInput(BoardHal::Button button)
{
   switch (button)
   {
   case BoardHal::Button::Left:
      return buttonLeft;
   case BoardHal::Button::Right:
      return buttonRight;
   case BoardHal::Button::Up:
      return buttonUp;
   case BoardHal::Button::Down:
      return buttonDown;
   default:
      return buttonLeft;
   }
}

static inline void refreshButtonLatches()
{
   for (uint8_t i = 0; i < 4u; ++i)
   {
      if (!g_btnLatched[i])
      {
         continue;
      }
      if (buttonInput((BoardHal::Button)i).read() != 0)
      {
         g_btnLatched[i] = false;
      }
   }
}

static inline void pushButtonIsr(BoardHal::Button b)
{
   const uint8_t next = (uint8_t)((g_btnHead + 1u) & 0x0Fu);
   if (next == g_btnTail)
   {
      return; // full, drop
   }
   g_btnQueue[g_btnHead] = b;
   g_btnHead             = next;
   g_btnFlags.set(BTN_FLAG_ANY);
}

static inline void pushButtonPressIsrDebounced(BoardHal::Button b)
{
   if (buttonInput(b).read() != 0)
   {
      return;
   }

   const uint8_t index = (uint8_t)b;
   if (index >= 4u)
   {
      return;
   }
   if (g_btnLatched[index])
   {
      return;
   }

   unsigned long&      lastMs = g_btnLastPressMs[index];
   const unsigned long nowMs  = millis();
   if ((unsigned long)(nowMs - lastMs) <= kButtonDebounceMs)
   {
      return;
   }

   pushButtonIsr(b);
   g_btnLatched[index] = true;
   lastMs              = nowMs;
}

static void onButtonLeftIrq()
{
   pushButtonPressIsrDebounced(BoardHal::Button::Left);
}

static void onButtonRightIrq()
{
   pushButtonPressIsrDebounced(BoardHal::Button::Right);
}

static void onButtonUpIrq()
{
   pushButtonPressIsrDebounced(BoardHal::Button::Up);
}

static void onButtonDownIrq()
{
   pushButtonPressIsrDebounced(BoardHal::Button::Down);
}

static const uint16_t kBatteryCapacityMah    = 10000;
static const float    kRecoveryVoltageOffset = 0.0f; // per requirement: equal to charge voltage
static const uint16_t kEndOfChargeCurrentMa  = 5;
} // namespace

void BoardHal::configurePins()
{
   // LEDs
   ledRed.write(1);
   ledGreen.write(1);
   ledBlue.write(1);

   // Buttons (active-low)
   buttonLeft.mode(PullUp);
   buttonRight.mode(PullUp);
   buttonUp.mode(PullUp);
   buttonDown.mode(PullUp);

   // External sensor/RS485 power rail (default OFF)
   sensorPowerEnable.write(0);

   // RS485 direction pin (default RX)
   rs485DriverEnable.write(0);

   KTD2052&                      ledDriver = hastig_led_driver();
   mbed::ScopedLock<rtos::Mutex> lock(ledDriverMutex);
   ledDriverReady = ledDriver.begin();
   if (!ledDriverReady)
   {
      LOGE(TAG, "KTD2052 initialization failed; RGB LEDs may remain in boot pattern");
      return;
   }
   if (!ledDriver.allOff())
   {
      ledDriverReady = false;
      LOGE(TAG, "KTD2052 allOff failed");
   }
}

void BoardHal::setLed(LedColor color, bool on)
{
   // active-low
   ledOutput(color).write(on ? 0 : 1);
}

void BoardHal::setConnectionIndicator(ConnectionIndicator state, bool blinkOn)
{
   const float intensityFactor = 0.25f;
   uint8_t     red             = 0;
   uint8_t     green           = 0;
   uint8_t     blue            = 0;
   switch (state)
   {
   case ConnectionIndicator::Connecting:
      red   = KTD_LED_MAX;
      green = KTD_LED_MAX / 2u;
      break;
   case ConnectionIndicator::Connected:
      green = KTD_LED_MAX;
      break;
   case ConnectionIndicator::Retrying:
      red = blinkOn ? KTD_LED_MAX : 0;
      break;
   case ConnectionIndicator::Off:
      break;
   }

   if (connectionLedStateValid && connectionLedRgb[0] == red && connectionLedRgb[1] == green && connectionLedRgb[2] == blue)
   {
      return;
   }

   mbed::ScopedLock<rtos::Mutex> lock(ledDriverMutex);
   if (!ledDriverReady)
   {
      return;
   }
   if (!hastig_led_driver().setRGB(KTD_LED_CONNECTION, red * intensityFactor, green * intensityFactor, blue * intensityFactor))
   {
      if (!connectionLedErrorLogged)
      {
         LOGE(TAG, "Failed to update KTD2052 connection indicator");
         connectionLedErrorLogged = true;
      }
      return;
   }
   connectionLedErrorLogged = false;
   connectionLedRgb[0]      = red;
   connectionLedRgb[1]      = green;
   connectionLedRgb[2]      = blue;
   connectionLedStateValid  = true;
}

void BoardHal::blinkLed(LedColor color, uint32_t ms, uint8_t times, uint32_t gapMs)
{
   for (uint8_t i = 0; i < times; ++i)
   {
      setLed(color, true);
      delay(ms);
      setLed(color, false);
      if (i + 1 < times)
      {
         delay(gapMs);
      }
   }
}

void BoardHal::blinkDualLedFeedback(uint32_t durationMs, uint32_t intervalMs)
{
   const uint32_t start = millis();
   while ((uint32_t)(millis() - start) < durationMs)
   {
      setLed(LedColor::Red, true);
      setLed(LedColor::Green, false);
      delay(intervalMs);

      setLed(LedColor::Red, false);
      setLed(LedColor::Green, true);
      delay(intervalMs);
   }

   setLed(LedColor::Red, false);
   setLed(LedColor::Green, false);
}

void BoardHal::setSensorPower(bool powerOn)
{
   sensorPowerEnable.write(powerOn ? 1 : 0);

   mbed::ScopedLock<rtos::Mutex> lock(ledDriverMutex);
   if (!ledDriverReady || (sensorLedStateValid && sensorLedPowerOn == powerOn))
   {
      return;
   }
   if (!hastig_led_driver().setRGB(KTD_LED_SENSOR_POWER, 0, powerOn ? KTD_LED_MAX : 0, 0))
   {
      LOGE(TAG, "Failed to update KTD2052 sensor-power indicator");
      return;
   }
   sensorLedPowerOn    = powerOn;
   sensorLedStateValid = true;
}

void BoardHal::setRs485TransmitEnabled(bool enabled)
{
   rs485DriverEnable.write(enabled ? 1 : 0);
}

BoardHal::BatterySnapshot BoardHal::readBattery(Battery& battery)
{
   BatterySnapshot s;
   s.voltage        = battery.voltage();
   s.minimumVoltage = battery.minimumVoltage();
   s.current        = battery.current();
   s.averageCurrent = battery.averageCurrent();
   return s;
}

void BoardHal::resetBatteryStatistics(Battery& battery)
{
   battery.resetMaximumMinimumCurrent();
   battery.resetMaximumMinimumVoltage();
}

void BoardHal::configurePmicFromSettings(const SettingsManager& settingsManager, Battery& battery, Charger& charger)
{
   const AppSettings s = settingsManager.getCopy();

   BatteryCharacteristics characteristics;
   characteristics.capacity           = kBatteryCapacityMah;
   characteristics.emptyVoltage       = s.low_batt_min_v;
   characteristics.chargeVoltage      = s.max_charging_voltage;
   characteristics.endOfChargeCurrent = kEndOfChargeCurrentMa;
   characteristics.recoveryVoltage    = s.max_charging_voltage + kRecoveryVoltageOffset;

   battery = Battery(characteristics);

   charger.setChargeCurrent(s.max_charging_current);
   charger.setChargeVoltage(s.max_charging_voltage);
   charger.begin();
}

void BoardHal::getHardwareId(char* out, size_t outSize)
{
   if (out == nullptr || outSize == 0u)
   {
      return;
   }

   // STM32H747 unique ID words (Portenta H7)
   const uint32_t* uid = (const uint32_t*)0x1FF1E800UL;
   const uint32_t  u0  = uid[0];
   const uint32_t  u1  = uid[1];
   const uint32_t  u2  = uid[2];

   // 24 hex chars + null
   snprintf(out, outSize, "%08lX%08lX%08lX", (unsigned long)u0, (unsigned long)u1, (unsigned long)u2);
}

bool BoardHal::detectFactoryResetButtonCombo(uint32_t windowMs, uint32_t holdMs)
{
   // Factory reset at boot: hold UP+DOWN (active-low buttons).
   uint32_t       comboStart      = 0;
   const uint32_t bootWindowStart = millis();
   while ((uint32_t)(millis() - bootWindowStart) < windowMs)
   {
      const bool combo = (buttonUp.read() == 0) && (buttonDown.read() == 0);
      if (combo)
      {
         if (comboStart == 0)
         {
            comboStart = millis();
         }

         // Visual cue while holding combo: RED on.
         setLed(LedColor::Red, true);

         if ((uint32_t)(millis() - comboStart) >= holdMs)
         {
            setLed(LedColor::Red, false);
            return true;
         }
      }
      else
      {
         comboStart = 0;
         setLed(LedColor::Red, false);
      }

      delay(50);
   }

   setLed(LedColor::Red, false);
   return false;
}

void BoardHal::enableButtonIrq()
{
   for (uint8_t i = 0; i < 4u; ++i)
   {
      g_btnLatched[i] = false;
   }

   // Active-low buttons report presses on falling edges.
   buttonLeft.fall(onButtonLeftIrq);
   buttonRight.fall(onButtonRightIrq);
   buttonUp.fall(onButtonUpIrq);
   buttonDown.fall(onButtonDownIrq);
}

bool BoardHal::popButton(BoardHal::Button& outButton)
{
   refreshButtonLatches();

   // Single-consumer (UI thread) pop; ISR is producer.
   if (g_btnTail == g_btnHead)
   {
      return false;
   }
   outButton = g_btnQueue[g_btnTail];
   g_btnTail = (uint8_t)((g_btnTail + 1u) & 0x0Fu);
   return true;
}

bool BoardHal::waitForButtonEvent(uint32_t timeoutMs)
{
   refreshButtonLatches();
   const uint32_t flags = g_btnFlags.wait_any(BTN_FLAG_ANY, timeoutMs);
   refreshButtonLatches();
   return (flags & BTN_FLAG_ANY) != 0u;
}
