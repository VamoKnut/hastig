#pragma once

#include <mbed.h>
#include <platform/ScopedLock.h>

#include <atomic>

#include "Messages.h"

class RuntimeStatus
{
public:
  enum class Mode : uint8_t { Aware = 0, Sampling = 1, Hibernating = 2 };

  RuntimeStatus();

  void setMode(Mode mode);
  Mode mode() const;

  void setAwareWindow(uint32_t lastActivityMs, uint32_t awareTimeoutS);
  uint32_t awareRemainingMs(uint32_t nowMs) const;

  void setLastSample(const SensorSampleMsg& sample);
  bool getLastSample(SensorSampleMsg& outSample) const;

private:
  std::atomic<uint8_t>  _mode;
  std::atomic<uint32_t> _lastActivityMs;
  std::atomic<uint32_t> _awareTimeoutS;

  mutable rtos::Mutex _sampleMx;
  SensorSampleMsg     _lastSample;
  bool                _hasSample;
};
