#pragma once

#include <mbed.h>
#include <atomic>

#include "AppConfig.h"
#include "Messages.h"
#include "SettingsManager.h"
#include "SessionClock.h"
#include "Sensor.h"
#include "EventBus.h"
#include "RuntimeStatus.h"

template <uint32_t DEPTH>
using SensorMail = rtos::Mail<SensorSampleMsg, DEPTH>;

/**
 * @brief Sensor sampling thread.
 */
class SamplingThread {
public:
  SamplingThread(SensorMail<QUEUE_DEPTH_SENSOR_TO_AGG>& outMail,
             SettingsManager& settings,
             SessionClock& clock,
             EventBus& eventBus,
             RuntimeStatus& runtimeStatus);

  /**
   * @brief Start the sensor worker.
   */
  void start();
  bool stop(uint32_t timeoutMs = 10000u);

  /**
   * @brief Enable or disable sampling.
   */
  void setEnabled(bool en);

private:
  SensorMail<QUEUE_DEPTH_SENSOR_TO_AGG>& _outMail;
  SettingsManager&                         _settings;
  SessionClock&                          _clock;
  EventBus&                              _eventBus;
  RuntimeStatus&                         _runtimeStatus;

  rtos::Thread     _thread;
  rtos::EventFlags _flags;
  rtos::EventFlags _completion;

  static constexpr uint32_t FLAG_WAKE = 1u << 0;
  static constexpr uint32_t FLAG_STOPPED = 1u << 0;

  std::atomic<bool> _enabled{false};
  std::atomic<bool> _stopRequested{false};

  Sensor* _sensor = nullptr;

  static void threadEntry(void* ctx);
  void run();
};
