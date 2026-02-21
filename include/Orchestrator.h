#pragma once

#include <mbed.h>
#include <stdint.h>

class EventBus;
class CommsEgress;
class SettingsManager;
class SessionClock;
class SamplingThread;
class AggregatorThread;
class PowerManager;


/**
 * @brief Application orchestrator (state machine).
 *
 * States:
 *  - Aware: network+MQTT up, awaiting commands
 *  - Sampling: sampling+aggregation+publishing
 *  - Hibernating: shutdown requested; deep sleep executed by PowerManager from loop()
 *
 * NOTE: Low-level deep sleep entry is executed from Arduino loop via PowerManager.
 */

class Orchestrator {
public:
  Orchestrator(EventBus& eventBus,
               CommsEgress& commsEgress,
               SettingsManager& settings,
               SessionClock& clock,
               SamplingThread& sensor,
               AggregatorThread& agg,
               PowerManager& powerManager);

  /**
   * @brief Start orchestrator RTOS thread.
   */
  void start();
  void stop();

private:
  enum class State : uint8_t { Aware, Sampling, Hibernating };

  enum class HibernateReason : uint8_t { Inactivity, Forced, EmergencyPowerSave, NoNetwork };

  EventBus& _eventBus;
  CommsEgress& _commsEgress;

  SettingsManager&  _settings;
  SessionClock&   _clock;
  SamplingThread&   _sensor;
  AggregatorThread& _agg;
  PowerManager&     _powerManager;

  rtos::Thread _thread;

  State    _state          = State::Aware;
  uint32_t _stateEnterMs   = 0;

  uint32_t _lastAckMs      = 0;
  uint32_t _unackedAggregateCount = 0;

  uint32_t _bootMs         = 0;
  uint32_t _mqttUpMs       = 0;
  bool     _noNetworkHibernateRequested = false;

  uint32_t _lastActivityMs = 0;
  uint32_t _lastStatusMs   = 0;

  uint32_t _forcedHibernateS          = 0;
  HibernateReason _hibernateReason    = HibernateReason::Inactivity;

  bool     _emergencyArmed = false;
  uint32_t _emergencyAtMs  = 0;

  static void threadEntry(void* ctx);
  void run();

  void enterState(State s);

  void handleServerCommand(const char* topic, const char* json);
  void handleAck();

  void checkTimeouts();
};
