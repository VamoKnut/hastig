#pragma once

#include <mbed.h>
#include <atomic>

#include "AppConfig.h"
#include "Messages.h"
#include "SettingsManager.h"
#include "SessionClock.h"
#include "EventBus.h"
#include "CommsEgress.h"

template <uint32_t DEPTH>
using AggInMail = rtos::Mail<SensorSampleMsg, DEPTH>;


/**
 * @brief Pure aggregation accumulator (atomic update + emit).
 */
class AggregateAccumulator {
public:
  void reset(uint32_t startMs);
  void add(const SensorSampleMsg& s);
  bool emit(AggregateMsg& out) const;

private:
  static void threadEntry(void* ctx);

  uint32_t _t0 = 0;
  uint32_t _t1 = 0;
  uint32_t _n  = 0;
  bool     _ok = true;

  char  _k0[8] = {0};
  char  _k1[8] = {0};

  float _v0_min = 0;
  float _v0_max = 0;
  float _v0_sum = 0;

  float _v1_min = 0;
  float _v1_max = 0;
  float _v1_sum = 0;
};

/**
 * @brief Aggregation thread: consumes samples and emits aggregated packets.
 */
class AggregatorThread {
public:
  AggregatorThread(AggInMail<QUEUE_DEPTH_SENSOR_TO_AGG>& inMail,
                   CommsEgress& commsEgress,
                   SettingsManager& settings,
                   SessionClock& clock,
                   EventBus& eventBus);

  /**
   * @brief Start RTOS thread.
   */
  void start();
  void stop();

  /**
   * @brief Enable or disable aggregation.
   */
  void setEnabled(bool en);

  /**
   * @brief Reset the current aggregation window.
   */
  void resetWindow();

private:
  static void threadEntry(void* ctx);

  AggInMail<QUEUE_DEPTH_SENSOR_TO_AGG>& _inMail;
  CommsEgress&                          _commsEgress;
  SettingsManager&                        _settings;
  SessionClock&                         _clock;
  EventBus&                             _eventBus;

  rtos::Thread     _thread;
  rtos::EventFlags _flags;

  static constexpr uint32_t FLAG_WAKE  = 1u << 0;
  static constexpr uint32_t FLAG_RESET = 1u << 1;

  std::atomic<bool> _enabled{false};

  AggregateAccumulator _acc;
  void run();
};
