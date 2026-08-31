#include <Arduino.h>
#include <mbed.h>

#include <Arduino_PowerManagement.h>
#include <Arduino_LowPowerPortentaH7.h>

#include "AppConfig.h"
#include "Logger.h"
#include "Messages.h"
#include "ConsoleCommands.h"
#include "Display.h"
#include "RestartReason.h"
#include "PowerUtil.h"
#include "PowerManager.h"
#include "WatchdogService.h"

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
  
  // Configure board pins early (before reading buttons).
  //BoardHal::configurePins();

  Display::getInstance().beginHardware();
  Display::getInstance().showSplash(HASTIG_AI_REVISION);

  Serial.begin(115200);
  Logger::begin(Serial, 115200);
  restartReason.begin();
  const RestartReasonCode lastRestartReason = restartReason.read();
  // Mark startup as unexpected reboot until we perform a controlled hibernate.
  restartReason.write(RestartReasonCode::UnexpectedReboot);

  if (WatchdogService::begin()) {
    // Once IWDG is active, any uncontrolled reboot should be reported as watchdog-driven.
    // Controlled hibernate paths overwrite this marker before entering low power.
    restartReason.write(RestartReasonCode::WatchdogReset);
  }
  delay(2500);
  WatchdogService::kick();
  Logger::set_runtime_level(Logger::Level::Debug);

  LOGI(TAG, "=== Hastig-H7-1 Boot (AI Revision: %s) ===", HASTIG_AI_REVISION);
  LOGI(TAG, "Last restart reason: %s (bootCount=%lu)",
       RestartReasonStore::toString(lastRestartReason),
       (unsigned long)restartReason.bootCount());

  sysCtx.powerManager.setOrchestrator(sysCtx.orchestrator);

  // Configure board pins early (before reading buttons).
  // 26.08.10: Moved to start of method to ensure that the I2C power is enabled before any other peripherals are initialized.
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

  WatchdogService::kick();

  // Keep loop responsive; other RTOS threads run independently.
  rtos::ThisThread::sleep_for(std::chrono::milliseconds(20));
}
