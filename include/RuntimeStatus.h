#pragma once

#include <mbed.h>
#include <platform/ScopedLock.h>

#include <atomic>

#include "Messages.h"

class RuntimeStatus
{
public:
  enum class Mode : uint8_t { Aware = 0, Sampling = 1, Hibernating = 2 };

  struct DeviceInfo {
    float batteryVoltage = 0.0f;
    float currentMa = 0.0f;
    char ipAddress[48] = "Unavailable";
    char iccid[24] = "Unavailable";
    char hardwareId[32] = "Unavailable";
  };

  RuntimeStatus();

  void setMode(Mode mode);
  Mode mode() const;
  void setCommunicationReady(bool ready);
  bool communicationReady() const;

  void setAwareWindow(uint32_t lastActivityMs, uint32_t awareTimeoutS);
  uint32_t awareRemainingMs(uint32_t nowMs) const;

  void setLastSample(const SensorSampleMsg& sample);
  bool getLastSample(SensorSampleMsg& outSample) const;
  void setBatteryInfo(float voltage, float currentMa);
  void setNetworkInfo(const char* ipAddress, const char* iccid);
  void setHardwareId(const char* hardwareId);
  DeviceInfo getDeviceInfo() const;

private:
  std::atomic<uint8_t>  _mode;
  std::atomic<bool>     _communicationReady;
  std::atomic<uint32_t> _lastActivityMs;
  std::atomic<uint32_t> _awareTimeoutS;

  mutable rtos::Mutex _sampleMx;
  SensorSampleMsg     _lastSample;
  bool                _hasSample;

  mutable rtos::Mutex _deviceInfoMx;
  DeviceInfo _deviceInfo;
};
