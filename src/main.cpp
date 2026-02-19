#include <Arduino.h>
#include <mbed.h>

#include <Arduino_PowerManagement.h>
#include <Arduino_LowPowerPortentaH7.h>

#include "AppConfig.h"
#include "Logger.h"
#include "Messages.h"
#include "ConsoleCommands.h"
#include "RestartReason.h"
#include "PowerUtil.h"
#include "PowerManager.h"

#include "BoardHal.h"

#include "SystemContext.h"

#include "AggregatorThread.h"
#include "CommsPump.h"
#include "Orchestrator.h"
#include "SamplingThread.h"
#include "UiThread.h"

#include <chrono>
using namespace std::chrono;

// Board / multicore

// Global services
Board             g_board;
Battery           g_battery;
Charger           g_charger;
static RestartReasonStore restartReason;
static const uint8_t kWakePin = 0; // GPIO0 / Arduino D0 terminology

static SystemContext sysCtx(g_board, restartReason, kWakePin);

static const char* TAG = "HASTIG";

// Route mbed stdio to USB CDC so default UART console does not claim D13/D14.
REDIRECT_STDOUT_TO(SerialUSB);


void setup()
{
#if defined(CORE_CM7)
  if (!g_board.begin()) {
    while (1) {
    }
  }

  bootM4();

  Serial.begin(115200);
  Logger::begin(Serial, 115200);
  delay(2500);
  Logger::set_runtime_level(Logger::Level::Debug);

  LOGI(TAG, "=== Hastig-H7-1 Boot (AI Revision: v110) ===");

  

  sysCtx.powerManager.setOrchestrator(sysCtx.orchestrator);

  // Configure board pins early (before reading buttons).
  BoardHal::configurePins();

  if (BoardHal::detectFactoryResetButtonCombo()) {
    LOGW(TAG, "Factory reset key combo held for 3s");
    sysCtx.settings.factoryReset();
    BoardHal::blinkDualLedFeedback();
  }

  sysCtx.settings.begin();

  // Configure PMIC/charger based on settings.
  BoardHal::configurePmicFromSettings(sysCtx.settings, g_battery, g_charger);

  // Print current config at boot.
  printSettingsToSerial(sysCtx.settings, Serial);

  sysCtx.sessionClock.begin();

  restartReason.begin();
  // Mark startup as unexpected reboot until we perform a controlled hibernate.
  restartReason.write(RestartReasonCode::UnexpectedReboot);

  sysCtx.uiThread.start();

  sysCtx.commsPump.begin();
  sysCtx.aggThread.start();
  sysCtx.samplingThread.start();

  sysCtx.orchestrator.start();

  // Enable IRQ-based button detection (prepared for future changes).
  BoardHal::enableButtonIrq();

  LOGI(TAG, "Startup complete");
#else
  LowPower.standbyM4();
#endif
}

/**
 * @brief Arduino loop - RTOS threads do the work.
 */
void loop()
{
  sysCtx.commsPump.loopOnce();

  handleSerialConsole(sysCtx.settings);

  // Execute sleep transaction if requested by Orchestrator.
  sysCtx.powerManager.service();

  // Keep loop responsive; other RTOS threads run independently.
  rtos::ThisThread::sleep_for(std::chrono::milliseconds(20));
}
