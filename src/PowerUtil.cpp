#include "PowerUtil.h"

#include "AppConfig.h"
#include "Logger.h"

static const char* TAG = "PWR";

namespace powerutil {

void preparePinsForLowPower(uint8_t wakePin)
{
  // Put shield-controlled rails and buses in a state that will not source
  // current into unpowered peripherals.
  pinMode(PIN_POWER_ENABLE, OUTPUT);
  digitalWrite(PIN_POWER_ENABLE, LOW);

  pinMode(PIN_RS485_DE_RE, OUTPUT);
  digitalWrite(PIN_RS485_DE_RE, LOW);

  pinMode(PIN_RS485_TX, OUTPUT);
  digitalWrite(PIN_RS485_TX, LOW);

  pinMode(PIN_RS485_RX, INPUT);

  // I2C display lines: no internal pulls before standby. External pullups, if
  // present, can hold the bus idle without the MCU sourcing current.
  pinMode(D11, INPUT);
  pinMode(D12, INPUT);

  // Typical: wake when pin is pulled LOW. Configure this last so it wins even
  // if the wake pin is also one of the shield pins above.
  pinMode(wakePin, INPUT_PULLUP);
}

void hibernate(Board& board, uint8_t wakePin, uint32_t expectedDurationS)
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

  preparePinsForLowPower(wakePin);

  // Arm wake sources.
  board.enableWakeupFromPin();
  board.enableWakeupFromRTC((int)h, (int)m, (int)s);

  // Enter standby until wake event occurs.
  board.standByUntilWakeupEvent();
}

} // namespace powerutil
