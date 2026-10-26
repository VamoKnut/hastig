#include "PowerUtil.h"

#include "AppConfig.h"
#include "BoardHal.h"
#include "Logger.h"

static const char* TAG = "PWR";

namespace powerutil {

void preparePinsForLowPower(mbed::DigitalIn& wakeInput)
{
  // Put shield-controlled rails and buses in a state that will not source
  // current into unpowered peripherals.
  BoardHal::setSensorPower(false);
  BoardHal::setRs485TransmitEnabled(false);

  static mbed::DigitalOut rs485Transmit(PIN_RS485_TX, 0);
  static mbed::DigitalIn rs485Receive(PIN_RS485_RX, PullNone);
  rs485Transmit.write(0);
  rs485Receive.mode(PullNone);

  // I2C display lines: no internal pulls before standby. External pullups, if
  // present, can hold the bus idle without the MCU sourcing current.
  static mbed::DigitalIn displaySda(PIN_DISPLAY_SDA, PullNone);
  static mbed::DigitalIn displayScl(PIN_DISPLAY_SCL, PullNone);
  displaySda.mode(PullNone);
  displayScl.mode(PullNone);

  // Typical: wake when the pin is pulled LOW. Keep the input configured last
  // in case it shares a pin with one of the shield lines above.
  wakeInput.mode(PullUp);
}

void hibernate(Board& board, PinName wakePin, uint32_t expectedDurationS)
{
  // Clamp to 12 hours to avoid very long sleep windows.
  if (expectedDurationS > 43200UL) {
    expectedDurationS = 43200UL;
  }
  if (expectedDurationS < 5UL) {
    expectedDurationS = 5UL;
  }

  const uint32_t h = expectedDurationS / 3600UL;
  const uint32_t m = (expectedDurationS % 3600UL) / 60UL;
  const uint32_t s = expectedDurationS % 60UL;

  LOGI(TAG, "Hibernate: wakePin=%u duration=%lu s (RTC %lu:%lu:%lu)",
       (unsigned)wakePin,
       (unsigned long)expectedDurationS,
       (unsigned long)h,
       (unsigned long)m,
       (unsigned long)s);

  // Key for uA: cut board rails/peripherals.
  board.setAllPeripheralsPower(false);
  board.setExternalPowerEnabled(false);

  mbed::DigitalIn wakeInput(wakePin, PullUp);
  preparePinsForLowPower(wakeInput);

  // Arm wake sources.
  board.enableWakeupFromPin();
  board.enableWakeupFromRTC((int)h, (int)m, (int)s);

  // Enter standby until wake event occurs.
  board.standByUntilWakeupEvent();
}

} // namespace powerutil
