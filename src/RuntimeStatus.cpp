#include "RuntimeStatus.h"

#include <string.h>

RuntimeStatus::RuntimeStatus()
    : _mode((uint8_t)Mode::Aware), _lastActivityMs(0u), _awareTimeoutS(0u), _hasSample(false)
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
