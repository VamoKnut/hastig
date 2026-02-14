#pragma once

#include <mbed.h>
#include <atomic>

#include "AppConfig.h"
#include "Messages.h"
#include "SettingsManager.h"
#include "SessionClock.h"
#include "Sensor.h"
#include "EventBus.h"

template <uint32_t DEPTH>
using SensorMail = rtos::Mail<SensorSampleMsg, DEPTH>;

template <uint32_t DEPTH>
using OneShotMail = rtos::Mail<SensorSampleMsg, DEPTH>;

/**
 * @brief Sensor sampling thread (Modbus RTU conductivity sensor).
 */
class SamplingThread {
public:
  SamplingThread(SensorMail<QUEUE_DEPTH_SENSOR_TO_AGG>& outMail,
             OneShotMail<QUEUE_DEPTH_ONE_SHOT>& oneShotMail,
             SettingsManager& settings,
             SessionClock& clock,
             EventBus& eventBus);

  /**
   * @brief Start RTOS thread.
   */
  void start();
  void stop();

  /**
   * @brief Enable or disable sampling.
   */
  void setEnabled(bool en);

  void requestOneShot();

private:
  SensorMail<QUEUE_DEPTH_SENSOR_TO_AGG>& _outMail;
  OneShotMail<QUEUE_DEPTH_ONE_SHOT>&      _oneShotMail;
  SettingsManager&                         _settings;
  SessionClock&                          _clock;
  EventBus&                              _eventBus;

  rtos::Thread     _thread;
  rtos::EventFlags _flags;

  static constexpr uint32_t FLAG_WAKE = 1u << 0;
  static constexpr uint32_t FLAG_ONE_SHOT = 1u << 1;

  std::atomic<bool> _enabled{false};

  Sensor* _sensor = nullptr;

  bool _modbusReady = false;

  static void threadEntry(void* ctx);
  void run();

  bool initModbus();
  void deinitModbus();

  bool readConductivity(float& outValue);
  bool readTemperature(float& outTempC);
};
