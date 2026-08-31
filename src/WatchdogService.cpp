#include "WatchdogService.h"

#include <Arduino.h>
#include <mbed.h>
#include <atomic>

#include "AppConfig.h"
#include "Logger.h"

namespace {
static const char* TAG = "WDT";

static constexpr uint32_t kLsiHz = 32000UL;
static constexpr uint32_t kPrescalerValue = 256UL;

IWDG_HandleTypeDef g_iwdg{};
bool g_running = false;
bool g_blockKicksForTest = false;
bool g_startedAttempted = false;
bool g_assistThreadStarted = false;
bool g_standbyFreeze = false;
bool g_stopFreeze = false;
bool g_softwareIwdg = false;
uint32_t g_actualTimeoutMs = 0;
std::atomic<uint32_t> g_longOperationDepth{0};
std::atomic<uint32_t> g_longOperationDeadlineMs{0};
rtos::Thread g_assistThread(osPriorityLow, 1024, nullptr, "WdtAssist");

enum class StartStatus : uint8_t {
  NotAttempted,
  Disabled,
  AlreadyRunning,
  OptionByteUnlockFailed,
  OptionByteProgramFailed,
  OptionByteLaunchReturned,
  StandbyFreezeMissing,
  HalInitFailed,
  Running,
};

StartStatus g_startStatus = StartStatus::NotAttempted;

uint32_t reloadForTimeoutMs(uint32_t timeoutMs)
{
  uint64_t ticks = ((uint64_t)timeoutMs * kLsiHz) / (kPrescalerValue * 1000ULL);
  if (ticks == 0) {
    ticks = 1;
  }
  if (ticks > 4096ULL) {
    ticks = 4096ULL;
  }
  return (uint32_t)(ticks - 1ULL);
}

uint32_t actualTimeoutMs(uint32_t reload)
{
  return (uint32_t)(((uint64_t)(reload + 1UL) * kPrescalerValue * 1000ULL) / kLsiHz);
}

void readIwdgOptionBytes()
{
  FLASH_OBProgramInitTypeDef ob{};
  HAL_FLASHEx_OBGetConfig(&ob);
  g_softwareIwdg = (ob.USERConfig & OB_IWDG1_SW) == OB_IWDG1_SW;
  g_stopFreeze = (ob.USERConfig & OB_IWDG_STOP_ACTIVE) == 0U;
  g_standbyFreeze = (ob.USERConfig & OB_IWDG_STDBY_ACTIVE) == 0U;
}

bool iwdgOptionBytesPrepared()
{
  readIwdgOptionBytes();
  return g_softwareIwdg && g_stopFreeze && g_standbyFreeze;
}

bool prepareIwdgOptionBytes()
{
  if (iwdgOptionBytesPrepared()) {
    return true;
  }

  LOGW(TAG, "Preparing IWDG option bytes; MCU should reset once");

  FLASH_OBProgramInitTypeDef ob{};
  ob.Banks = FLASH_BANK_1;
  ob.OptionType = OPTIONBYTE_USER;
  ob.USERType = OB_USER_IWDG1_SW | OB_USER_IWDG_STOP | OB_USER_IWDG_STDBY;
  ob.USERConfig = OB_IWDG1_SW | OB_IWDG_STOP_FREEZE | OB_IWDG_STDBY_FREEZE;

  if (HAL_FLASH_Unlock() != HAL_OK || HAL_FLASH_OB_Unlock() != HAL_OK) {
    HAL_FLASH_OB_Lock();
    HAL_FLASH_Lock();
    g_startStatus = StartStatus::OptionByteUnlockFailed;
    LOGE(TAG, "IWDG option byte unlock failed");
    return false;
  }

  if (HAL_FLASHEx_OBProgram(&ob) != HAL_OK) {
    HAL_FLASH_OB_Lock();
    HAL_FLASH_Lock();
    g_startStatus = StartStatus::OptionByteProgramFailed;
    LOGE(TAG, "IWDG option byte programming failed");
    return false;
  }

  if (HAL_FLASH_OB_Launch() != HAL_OK) {
    HAL_FLASH_OB_Lock();
    HAL_FLASH_Lock();
    g_startStatus = StartStatus::OptionByteLaunchReturned;
    LOGE(TAG, "IWDG option byte launch failed");
    return false;
  }

  HAL_FLASH_OB_Lock();
  HAL_FLASH_Lock();
  LOGW(TAG, "IWDG option byte launch complete; resetting MCU");
  delay(100);
  NVIC_SystemReset();
  delay(1000);

  g_startStatus = StartStatus::OptionByteLaunchReturned;
  LOGE(TAG, "IWDG option byte reset request returned");
  return false;
}

bool deadlineActive(uint32_t nowMs, uint32_t deadlineMs)
{
  return deadlineMs != 0u && (int32_t)(deadlineMs - nowMs) > 0;
}

void assistThreadEntry()
{
  while (true) {
    rtos::ThisThread::sleep_for(std::chrono::milliseconds(HASTIG_WATCHDOG_LONG_OP_KICK_MS));

    if (!g_running || g_blockKicksForTest || g_longOperationDepth.load() == 0u) {
      continue;
    }

    const uint32_t nowMs = millis();
    const uint32_t deadlineMs = g_longOperationDeadlineMs.load();
    if (deadlineActive(nowMs, deadlineMs)) {
      HAL_IWDG_Refresh(&g_iwdg);
    }
  }
}

void startAssistThread()
{
  if (g_assistThreadStarted) {
    return;
  }

  if (g_assistThread.start(assistThreadEntry) == osOK) {
    g_assistThreadStarted = true;
  } else {
    LOGW(TAG, "Long-operation watchdog assist thread failed to start");
  }
}
} // namespace

namespace WatchdogService {

bool begin()
{
  g_startedAttempted = true;
  if (!HASTIG_WATCHDOG_ENABLE) {
    g_startStatus = StartStatus::Disabled;
    return false;
  }
  if (g_running) {
    kick();
    g_startStatus = StartStatus::AlreadyRunning;
    return true;
  }

  if (HASTIG_WATCHDOG_PREPARE_OPTION_BYTES && !prepareIwdgOptionBytes()) {
    return false;
  }

  readIwdgOptionBytes();
  if (HASTIG_WATCHDOG_REQUIRE_STANDBY_FREEZE && !g_standbyFreeze) {
    g_startStatus = StartStatus::StandbyFreezeMissing;
    LOGW(TAG, "Not starting IWDG: standby freeze option is not set (sw=%d stopFreeze=%d)",
         g_softwareIwdg ? 1 : 0,
         g_stopFreeze ? 1 : 0);
    return false;
  }

  const uint32_t reload = reloadForTimeoutMs(HASTIG_WATCHDOG_TIMEOUT_MS);
  g_actualTimeoutMs = actualTimeoutMs(reload);
  g_iwdg.Instance = IWDG1;
  g_iwdg.Init.Prescaler = IWDG_PRESCALER_256;
  g_iwdg.Init.Reload = reload;
  g_iwdg.Init.Window = IWDG_WINDOW_DISABLE;

  if (HAL_IWDG_Init(&g_iwdg) != HAL_OK) {
    g_startStatus = StartStatus::HalInitFailed;
    LOGE(TAG, "IWDG start failed");
    return false;
  }

  g_running = true;
  g_startStatus = StartStatus::Running;
  startAssistThread();
  return true;
}

void kick()
{
  if (g_running && !g_blockKicksForTest) {
    HAL_IWDG_Refresh(&g_iwdg);
  }
}

void beginLongOperation(uint32_t maxDurationMs)
{
  if (!g_running) {
    return;
  }

  kick();
  g_longOperationDeadlineMs.store(millis() + maxDurationMs);
  g_longOperationDepth.fetch_add(1);
}

void endLongOperation()
{
  if (!g_running) {
    return;
  }

  const uint32_t depth = g_longOperationDepth.load();
  if (depth <= 1u) {
    g_longOperationDepth.store(0);
    g_longOperationDeadlineMs.store(0);
  } else {
    g_longOperationDepth.store(depth - 1u);
  }
  kick();
}

void prepareForHibernate()
{
  if (g_running) {
    kick();
    LOGI(TAG, "IWDG kicked before hibernate");
  }
}

bool isRunning()
{
  return g_running;
}

void blockKicksForTest()
{
  if (!g_running) {
    LOGW(TAG, "Block kick requested, but IWDG is not running (status=%u sw=%d stopFreeze=%d standbyFreeze=%d)",
         (unsigned)g_startStatus,
         g_softwareIwdg ? 1 : 0,
         g_stopFreeze ? 1 : 0,
         g_standbyFreeze ? 1 : 0);
    return;
  }

  g_blockKicksForTest = true;
  LOGW(TAG, "IWDG kicks blocked for reboot test");
}

bool kicksBlocked()
{
  return g_blockKicksForTest;
}

void logStatus()
{
  if (!g_startedAttempted) {
    LOGW(TAG, "IWDG start was not attempted");
    return;
  }

  switch (g_startStatus) {
    case StartStatus::Disabled:
      LOGI(TAG, "IWDG disabled by config");
      break;
    case StartStatus::AlreadyRunning:
    case StartStatus::Running:
      LOGI(TAG, "IWDG running: timeout=%lu ms sw=%d stopFreeze=%d standbyFreeze=%d",
           (unsigned long)g_actualTimeoutMs,
           g_softwareIwdg ? 1 : 0,
           g_stopFreeze ? 1 : 0,
           g_standbyFreeze ? 1 : 0);
      break;
    case StartStatus::OptionByteUnlockFailed:
      LOGE(TAG, "IWDG not running: option byte unlock failed");
      break;
    case StartStatus::OptionByteProgramFailed:
      LOGE(TAG, "IWDG not running: option byte programming failed");
      break;
    case StartStatus::OptionByteLaunchReturned:
      LOGE(TAG, "IWDG not running: option byte launch returned without reset");
      break;
    case StartStatus::StandbyFreezeMissing:
      LOGW(TAG, "IWDG not running: standby freeze option is not set (sw=%d stopFreeze=%d)",
           g_softwareIwdg ? 1 : 0,
           g_stopFreeze ? 1 : 0);
      break;
    case StartStatus::HalInitFailed:
      LOGE(TAG, "IWDG not running: HAL init failed");
      break;
    default:
      LOGW(TAG, "IWDG not running: start status unknown");
      break;
  }
}

} // namespace WatchdogService
