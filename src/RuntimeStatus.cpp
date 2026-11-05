#include "RuntimeStatus.h"

#include <string.h>

RuntimeStatus::RuntimeStatus()
    : _mode((uint8_t)Mode::Aware), _communicationReady(false), _lastActivityMs(0u),
      _awareTimeoutS(0u), _hasSample(false)
{
  memset(&_lastSample, 0, sizeof(_lastSample));
}

void RuntimeStatus::setMode(Mode mode)
{
  _mode.store((uint8_t)mode);
}

RuntimeStatus::Mode RuntimeStatus::mode() const
{
  return (Mode)_mode.load();
}

void RuntimeStatus::setCommunicationReady(bool ready)
{
  _communicationReady.store(ready);
}

bool RuntimeStatus::communicationReady() const
{
  return _communicationReady.load();
}

void RuntimeStatus::setAwareWindow(uint32_t lastActivityMs, uint32_t awareTimeoutS)
{
  _lastActivityMs.store(lastActivityMs);
  _awareTimeoutS.store(awareTimeoutS);
}

uint32_t RuntimeStatus::awareRemainingMs(uint32_t nowMs) const
{
  const uint32_t timeoutMs = _awareTimeoutS.load() * 1000u;
  const uint32_t lastMs = _lastActivityMs.load();
  const uint32_t elapsed = nowMs - lastMs;

  if (elapsed >= timeoutMs) {
    return 0u;
  }
  return timeoutMs - elapsed;
}

void RuntimeStatus::setLastSample(const SensorSampleMsg& sample)
{
  mbed::ScopedLock<rtos::Mutex> lock(_sampleMx);
  _lastSample = sample;
  _hasSample = true;
}

bool RuntimeStatus::getLastSample(SensorSampleMsg& outSample) const
{
  mbed::ScopedLock<rtos::Mutex> lock(_sampleMx);
  if (!_hasSample) {
    return false;
  }
  outSample = _lastSample;
  return true;
}

void RuntimeStatus::setBatteryInfo(float voltage, float currentMa)
{
  mbed::ScopedLock<rtos::Mutex> lock(_deviceInfoMx);
  _deviceInfo.batteryVoltage = voltage;
  _deviceInfo.currentMa = currentMa;
}

void RuntimeStatus::setNetworkInfo(const char* ipAddress, const char* iccid)
{
  mbed::ScopedLock<rtos::Mutex> lock(_deviceInfoMx);
  strncpy(_deviceInfo.ipAddress, ipAddress != nullptr && ipAddress[0] != '\0' ? ipAddress : "Unavailable",
          sizeof(_deviceInfo.ipAddress) - 1u);
  _deviceInfo.ipAddress[sizeof(_deviceInfo.ipAddress) - 1u] = '\0';
  strncpy(_deviceInfo.iccid, iccid != nullptr && iccid[0] != '\0' ? iccid : "Unavailable",
          sizeof(_deviceInfo.iccid) - 1u);
  _deviceInfo.iccid[sizeof(_deviceInfo.iccid) - 1u] = '\0';
}

void RuntimeStatus::setHardwareId(const char* hardwareId)
{
  mbed::ScopedLock<rtos::Mutex> lock(_deviceInfoMx);
  strncpy(_deviceInfo.hardwareId,
          hardwareId != nullptr && hardwareId[0] != '\0' ? hardwareId : "Unavailable",
          sizeof(_deviceInfo.hardwareId) - 1u);
  _deviceInfo.hardwareId[sizeof(_deviceInfo.hardwareId) - 1u] = '\0';
}

RuntimeStatus::DeviceInfo RuntimeStatus::getDeviceInfo() const
{
  mbed::ScopedLock<rtos::Mutex> lock(_deviceInfoMx);
  return _deviceInfo;
}
