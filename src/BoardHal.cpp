#include "BoardHal.h"

#include <rtos/EventFlags.h>

#include "SettingsManager.h"

// Portenta LEDs are active-low.

namespace {
static uint32_t ledPin(BoardHal::LedColor color)
{
  switch (color) {
    case BoardHal::LedColor::Red:
      return LEDR;
    case BoardHal::LedColor::Green:
      return LEDG;
    case BoardHal::LedColor::Blue:
      return LEDB;
    default:
      return LEDR;
  }
}

// IRQ-based button detection (ring-buffer mailbox).
static rtos::EventFlags g_btnFlags;
static constexpr uint32_t BTN_FLAG_ANY = 1u;

static volatile uint8_t g_btnHead = 0;
static volatile uint8_t g_btnTail = 0;
static BoardHal::Button g_btnQueue[16];

static inline void pushButtonIsr(BoardHal::Button b)
{
  const uint8_t next = (uint8_t)((g_btnHead + 1u) & 0x0Fu);
  if (next == g_btnTail) {
    return; // full, drop
  }
  g_btnQueue[g_btnHead] = b;
  g_btnHead             = next;
  g_btnFlags.set(BTN_FLAG_ANY);
}

static void onButtonLeftIrq()
{
  static unsigned long lastMs = 0;
  const unsigned long nowMs   = millis();
  if ((unsigned long)(nowMs - lastMs) > 50ul) {
    pushButtonIsr(BoardHal::Button::Left);
    lastMs = nowMs;
  }
}

static void onButtonRightIrq()
{
  static unsigned long lastMs = 0;
  const unsigned long nowMs   = millis();
  if ((unsigned long)(nowMs - lastMs) > 50ul) {
    pushButtonIsr(BoardHal::Button::Right);
    lastMs = nowMs;
  }
}

static void onButtonUpIrq()
{
  static unsigned long lastMs = 0;
  const unsigned long nowMs   = millis();
  if ((unsigned long)(nowMs - lastMs) > 50ul) {
    pushButtonIsr(BoardHal::Button::Up);
    lastMs = nowMs;
  }
}

static void onButtonDownIrq()
{
  static unsigned long lastMs = 0;
  const unsigned long nowMs   = millis();
  if ((unsigned long)(nowMs - lastMs) > 50ul) {
    pushButtonIsr(BoardHal::Button::Down);
    lastMs = nowMs;
  }
}

static const uint16_t kBatteryCapacityMah     = 10000;
static const float    kRecoveryVoltageOffset = 0.0f; // per requirement: equal to charge voltage
static const uint16_t kEndOfChargeCurrentMa  = 5;
} // namespace

void BoardHal::configurePins()
{
  // LEDs
  pinMode(LEDR, OUTPUT);
  pinMode(LEDG, OUTPUT);
  pinMode(LEDB, OUTPUT);
  digitalWrite(LEDR, HIGH);
  digitalWrite(LEDG, HIGH);
  digitalWrite(LEDB, HIGH);

  // Buttons (active-low)
  pinMode(PIN_BTN_LEFT, INPUT_PULLUP);
  pinMode(PIN_BTN_RIGHT, INPUT_PULLUP);
  pinMode(PIN_BTN_UP, INPUT_PULLUP);
  pinMode(PIN_BTN_DOWN, INPUT_PULLUP);

  // External sensor/RS485 power rail (default OFF)
  pinMode(PIN_POWER_ENABLE, OUTPUT);
  digitalWrite(PIN_POWER_ENABLE, LOW);

  // RS485 direction pin (default RX)
  pinMode(PIN_RS485_DE_RE, OUTPUT);
  digitalWrite(PIN_RS485_DE_RE, LOW);
}

void BoardHal::setLed(LedColor color, bool on)
{
  const uint32_t pin = ledPin(color);
  pinMode((int)pin, OUTPUT);
  // active-low
  digitalWrite((int)pin, on ? LOW : HIGH);
}

void BoardHal::blinkLed(LedColor color, uint32_t ms, uint8_t times, uint32_t gapMs)
{
  for (uint8_t i = 0; i < times; ++i) {
    setLed(color, true);
    delay(ms);
    setLed(color, false);
    if (i + 1 < times) {
      delay(gapMs);
    }
  }
}

void BoardHal::blinkDualLedFeedback(uint32_t durationMs, uint32_t intervalMs)
{
  const uint32_t start = millis();
  while ((uint32_t)(millis() - start) < durationMs) {
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
  pinMode(PIN_POWER_ENABLE, OUTPUT);
  digitalWrite(PIN_POWER_ENABLE, powerOn ? HIGH : LOW);
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

bool BoardHal::detectFactoryResetButtonCombo(uint32_t windowMs, uint32_t holdMs)
{
  // Factory reset at boot: hold UP+DOWN (active-low buttons).
  uint32_t comboStart           = 0;
  const uint32_t bootWindowStart = millis();
  while ((uint32_t)(millis() - bootWindowStart) < windowMs) {
    const bool combo = (digitalRead(PIN_BTN_UP) == LOW) && (digitalRead(PIN_BTN_DOWN) == LOW);
    if (combo) {
      if (comboStart == 0) {
        comboStart = millis();
      }

      // Visual cue while holding combo: RED on.
      setLed(LedColor::Red, true);

      if ((uint32_t)(millis() - comboStart) >= holdMs) {
        setLed(LedColor::Red, false);
        return true;
      }
    } else {
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
  // Enable IRQ-based button press detection (debounced in ISR).
  attachInterrupt(digitalPinToInterrupt(PIN_BTN_LEFT), onButtonLeftIrq, CHANGE);
  // Portenta quirk: ensure pinMode after attachInterrupt
  pinMode(PIN_BTN_LEFT, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(PIN_BTN_RIGHT), onButtonRightIrq, CHANGE);
  // Portenta quirk: ensure pinMode after attachInterrupt
  pinMode(PIN_BTN_RIGHT, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(PIN_BTN_UP), onButtonUpIrq, CHANGE);
  // Portenta quirk: ensure pinMode after attachInterrupt
  pinMode(PIN_BTN_UP, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(PIN_BTN_DOWN), onButtonDownIrq, CHANGE);
  // Portenta quirk: ensure pinMode after attachInterrupt
  pinMode(PIN_BTN_DOWN, INPUT_PULLUP);
}



bool BoardHal::popButton(BoardHal::Button& outButton)
{
  // Single-consumer (UI thread) pop; ISR is producer.
  if (g_btnTail == g_btnHead) {
    return false;
  }
  outButton = g_btnQueue[g_btnTail];
  g_btnTail = (uint8_t)((g_btnTail + 1u) & 0x0Fu);
  return true;
}


bool BoardHal::waitForButtonEvent(uint32_t timeoutMs)
{
  const uint32_t flags = g_btnFlags.wait_any(BTN_FLAG_ANY, timeoutMs);
  return (flags & BTN_FLAG_ANY) != 0u;
}
