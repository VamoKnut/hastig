#include "RestartReason.h"

#include <Arduino.h>
#include <Arduino_LowPowerPortentaH7.h>
#include <STM32H747_System.h>
#include <mbed.h>

namespace {
static constexpr uint32_t kMagic = 0x48535447UL; // "HSTG"

// DR0-DR8 are reserved by Arduino/bootloader facilities on Portenta H7.
struct BackupSlot {
  RTCBackup magic;
  RTCBackup reason;
  RTCBackup bootCount;
  RTCBackup reasonInv;
};

static constexpr BackupSlot kSlotA = {RTCBackup::DR9, RTCBackup::DR10, RTCBackup::DR11, RTCBackup::DR12};
static constexpr BackupSlot kSlotB = {RTCBackup::DR13, RTCBackup::DR14, RTCBackup::DR15, RTCBackup::DR16};

static bool isValidReason(RestartReasonCode code)
{
  switch (code) {
    case RestartReasonCode::UnexpectedReboot:
    case RestartReasonCode::LowPowerWakeup:
    case RestartReasonCode::NoNetwork:
    case RestartReasonCode::Forced:
    case RestartReasonCode::EmergencyPowerSave:
    case RestartReasonCode::BrownOut:
    case RestartReasonCode::WatchdogReset:
    case RestartReasonCode::ColdReboot:
      return true;
    default:
      return false;
  }
}

static void prepareBackupDomainAccess()
{
  RCC->APB4ENR |= RCC_APB4ENR_RTCAPBEN;
  RCC->APB4LPENR |= RCC_APB4LPENR_RTCAPBLPEN;
#if defined(RCC_D3AMR_RTCAMEN)
  RCC->D3AMR |= RCC_D3AMR_RTCAMEN;
#endif
  RCC->BDCR |= RCC_BDCR_RTCEN;

#if defined(RCC_AHB4ENR_BKPRAMEN)
  RCC->AHB4ENR |= RCC_AHB4ENR_BKPRAMEN;
#endif
#if defined(RCC_AHB4LPENR_BKPRAMLPEN)
  RCC->AHB4LPENR |= RCC_AHB4LPENR_BKPRAMLPEN;
#endif
#if defined(RCC_D3AMR_BKPRAMAMEN)
  RCC->D3AMR |= RCC_D3AMR_BKPRAMAMEN;
#endif

  HAL_PWR_EnableBkUpAccess();
  __DSB();
  __ISB();
}

static bool readSlot(const BackupSlot& slot, RestartReasonCode& outReason, uint32_t& outBootCount)
{
  prepareBackupDomainAccess();
  const uint32_t magic = STM32H747::readBackupRegister(slot.magic);
  const uint32_t raw = STM32H747::readBackupRegister(slot.reason);
  const uint32_t rawInv = STM32H747::readBackupRegister(slot.reasonInv);

  if (magic != kMagic || rawInv != ~raw) {
    outReason = RestartReasonCode::Unknown;
    outBootCount = 0;
    return false;
  }

  const RestartReasonCode stored = (RestartReasonCode)raw;
  if (!isValidReason(stored)) {
    outReason = RestartReasonCode::Unknown;
    outBootCount = 0;
    return false;
  }

  outReason = stored;
  outBootCount = STM32H747::readBackupRegister(slot.bootCount);
  return true;
}

static bool readStoredReason(RestartReasonCode& outReason, uint32_t& outBootCount)
{
  RestartReasonCode reasonA = RestartReasonCode::Unknown;
  RestartReasonCode reasonB = RestartReasonCode::Unknown;
  uint32_t bootCountA = 0;
  uint32_t bootCountB = 0;
  const bool validA = readSlot(kSlotA, reasonA, bootCountA);
  const bool validB = readSlot(kSlotB, reasonB, bootCountB);

  if (validA && (!validB || bootCountA >= bootCountB)) {
    outReason = reasonA;
    outBootCount = bootCountA;
    return true;
  }
  if (validB) {
    outReason = reasonB;
    outBootCount = bootCountB;
    return true;
  }

  outReason = RestartReasonCode::Unknown;
  outBootCount = 0;
  return false;
}

static void writeSlot(const BackupSlot& slot, RestartReasonCode code, uint32_t bootCount)
{
  prepareBackupDomainAccess();
  const uint32_t raw = (uint32_t)code;
  STM32H747::writeBackupRegister(slot.magic, kMagic);
  STM32H747::writeBackupRegister(slot.reason, raw);
  STM32H747::writeBackupRegister(slot.bootCount, bootCount);
  STM32H747::writeBackupRegister(slot.reasonInv, ~raw);
  __DSB();
}

static bool verifySlot(const BackupSlot& slot, RestartReasonCode code, uint32_t bootCount)
{
  prepareBackupDomainAccess();
  const uint32_t raw = (uint32_t)code;
  return STM32H747::readBackupRegister(slot.magic) == kMagic &&
         STM32H747::readBackupRegister(slot.reason) == raw &&
         STM32H747::readBackupRegister(slot.bootCount) == bootCount &&
         STM32H747::readBackupRegister(slot.reasonInv) == ~raw;
}

static bool writeStoredReason(RestartReasonCode code, uint32_t bootCount)
{
  prepareBackupDomainAccess();
  writeSlot(kSlotA, code, bootCount);
  writeSlot(kSlotB, code, bootCount);
  return verifySlot(kSlotA, code, bootCount) && verifySlot(kSlotB, code, bootCount);
}

static bool wasInLowPowerStandby()
{
  return LowPower.wasInCPUMode(CPUMode::standby) ||
         LowPower.wasInCPUMode(CPUMode::d1DomainStandby) ||
         LowPower.wasInCPUMode(CPUMode::d2DomainStandby);
}

static RestartReasonCode classifyHardwareReset(RestartReasonCode stored,
                                               bool hasStoredReason,
                                               reset_reason_t hwReason,
                                               bool lowPowerStandbyWake)
{
  switch (hwReason) {
    case RESET_REASON_WATCHDOG:
      return RestartReasonCode::WatchdogReset;
    case RESET_REASON_BROWN_OUT:
      return RestartReasonCode::BrownOut;
    case RESET_REASON_WAKE_LOW_POWER:
      return isValidReason(stored) ? stored : RestartReasonCode::LowPowerWakeup;
    default:
      if (lowPowerStandbyWake) {
        return isValidReason(stored) ? stored : RestartReasonCode::LowPowerWakeup;
      }
      if (!hasStoredReason) {
        return RestartReasonCode::ColdReboot;
      }
      return isValidReason(stored) ? stored : RestartReasonCode::UnexpectedReboot;
  }
}
} // namespace

void RestartReasonStore::begin()
{
  RestartReasonCode stored = RestartReasonCode::Unknown;
  uint32_t bootCount = 0;
  prepareBackupDomainAccess();
  const bool hasStoredReason = readStoredReason(stored, bootCount);

  const reset_reason_t hwReason = STM32H747::getResetReason();
  const bool lowPowerStandbyWake = wasInLowPowerStandby();
  _lastReason = classifyHardwareReset(stored, hasStoredReason, hwReason, lowPowerStandbyWake);
  _bootCount = bootCount + 1u;

  writeStoredReason(_lastReason, _bootCount);

  if (lowPowerStandbyWake) {
    LowPower.resetPreviousCPUModeFlags();
  }
}

RestartReasonCode RestartReasonStore::read() const
{
  return _lastReason;
}

void RestartReasonStore::write(RestartReasonCode code) const
{
  const RestartReasonCode safeCode = isValidReason(code) ? code : RestartReasonCode::UnexpectedReboot;
  writeStoredReason(safeCode, _bootCount);
}

uint32_t RestartReasonStore::bootCount() const
{
  return _bootCount;
}

const char* RestartReasonStore::toString(RestartReasonCode code)
{
  switch (code) {
    case RestartReasonCode::UnexpectedReboot:
      return "unexpectedReboot";
    case RestartReasonCode::LowPowerWakeup:
      return "lowPowerWakeup";
    case RestartReasonCode::NoNetwork:
      return "noNetwork";
    case RestartReasonCode::Forced:
      return "forced";
    case RestartReasonCode::EmergencyPowerSave:
      return "emergencyPowerSave";
    case RestartReasonCode::BrownOut:
      return "brownOut";
    case RestartReasonCode::WatchdogReset:
      return "watchdogReset";
    case RestartReasonCode::ColdReboot:
      return "coldReboot";
    default:
      return "unknown";
  }
}
