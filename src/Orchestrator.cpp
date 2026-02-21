#include "Orchestrator.h"

#include "AppConfig.h"
#include "EventBus.h"
#include "CommsEgress.h"
#include "SettingsManager.h"
#include "SessionClock.h"
#include "SamplingThread.h"
#include "AggregatorThread.h"
#include "PowerManager.h"
#include "BoardHal.h"
#include "HastigGlobals.h"

#include "Logger.h"
#include "StopUtil.h"
#include "TimeUtil.h"

#include "ProtocolCodec.h"


#include <Arduino.h>
#include <ArduinoJson.h>

#include <string.h>

#include <chrono>
using namespace std::chrono;

static const char* TAG = "ORCH";

/**
 * @brief Construct orchestrator.
 */
Orchestrator::Orchestrator(EventBus& eventBus,
                           CommsEgress& commsEgress,
                           SettingsManager& settings,
                           SessionClock& clock,
                           SamplingThread& sensor,
                           AggregatorThread& agg,
                           PowerManager& powerManager)
    : _eventBus(eventBus),
      _commsEgress(commsEgress),
      _settings(settings),
      _clock(clock),
      _sensor(sensor),
      _agg(agg),
      _powerManager(powerManager),
      _thread(PRIO_ORCH, STACK_ORCH, nullptr, "Orch")
{
}

/**
 * @brief Start orchestrator thread.
 */
void Orchestrator::start()
{
  _thread.start(mbed::callback(Orchestrator::threadEntry, this));
}

/**
 * @brief Thread entry trampoline.
 */
void Orchestrator::threadEntry(void* ctx)
{
  static_cast<Orchestrator*>(ctx)->run();
}


/**
 * @brief Enter new orchestrator state.
 */
void Orchestrator::enterState(State s)
{
  const State prevState = _state;

  const char* previousMode = "";
  switch (prevState) {
    case State::Aware:
      previousMode = "aware";
      break;
    case State::Sampling:
      previousMode = "sampling";
      break;
    case State::Hibernating:
      previousMode = "hibernating";
      break;
    default:
      break;
  }

  const char* newMode = "";
  switch (s) {
    case State::Aware:
      newMode = "aware";
      break;
    case State::Sampling:
      newMode = "sampling";
      break;
    case State::Hibernating:
      newMode = "hibernating";
      break;
    default:
      break;
  }
  const bool  isModeChange = (previousMode[0] != '\0') && (newMode[0] != '\0') && (strcmp(previousMode, newMode) != 0);

  _state        = s;
  _stateEnterMs = timeutil::nowMs();
  _lastActivityMs = _stateEnterMs;

  if (s == State::Aware) {
    LOGI(TAG, "State=aware");

    // Ensure sampling pipeline is stopped.
    _sensor.setEnabled(false);
    _agg.setEnabled(false);

    _lastAckMs   = 0;
    _unackedAggregateCount = 0;
    if (isModeChange) {
      _commsEgress.publishModeChange("aware", previousMode);
    } else {
      _commsEgress.publishAwake();
    }
    return;
  }

  if (s == State::Sampling) {
    LOGI(TAG, "State=sampling");
    _lastAckMs   = timeutil::nowMs();
    _unackedAggregateCount = 0;
    _sensor.setEnabled(true);
    _agg.setEnabled(true);
    if (isModeChange) {
      _commsEgress.publishModeChange("sampling", previousMode);
    }
    return;
  }

  if (s == State::Hibernating) {
    LOGI(TAG, "State=hibernating");
    _sensor.setEnabled(false);
    _agg.setEnabled(false);


    const char* reasonStr = "inactivity";
    if (_hibernateReason == HibernateReason::Forced) {
      reasonStr = "forced";
    } else if (_hibernateReason == HibernateReason::EmergencyPowerSave) {
      reasonStr = "emergencyPowerSave";
    } else if (_hibernateReason == HibernateReason::NoNetwork) {
      reasonStr = "noNetwork";
    }

    if (isModeChange) {
      _commsEgress.publishHibernateModeChange(previousMode, reasonStr, _forcedHibernateS);
    } else {
      _commsEgress.publishHibernating(reasonStr, _forcedHibernateS);
    }
    return;
  }
}

/**
 * @brief Handle server command message.
 *
 * Expected command types:
 *  - {"type":"startSampling", ...}
 *  - {"type":"stopSampling"}
 *  - {"type":"keepSampling"}
 *  - {"type":"hibernate", "sleepSeconds":...}
 *  - {"type":"getConfig"}
 */
void Orchestrator::handleServerCommand(const char* topic, const char* json)
{
  (void)topic;
  protocol::Command cmd;
  if (!protocol::decodeCommand(json, cmd)) {
    LOGW(TAG, "Bad JSON");
    return;
  }

  _lastActivityMs = timeutil::nowMs();

  if (cmd.type == protocol::Command::Type::nudge) {
    // Intentionally no-op. This command exists only to reset inactivity timeout.
    return;
  }

  if (cmd.type == protocol::Command::Type::keepSampling) {
    handleAck();
    return;
  }

  if (cmd.type == protocol::Command::Type::startSampling) {
    // Optional overrides
    JsonDocument patch;

    if (cmd.hasSamplingInterval) {
      uint32_t v = cmd.samplingInterval;
      if (v < MIN_SAMPLE_PERIOD_MS) {
        v = MIN_SAMPLE_PERIOD_MS;
      }
      patch["samplingInterval"] = v;
    }

    if (cmd.hasAggPeriodS) {
      patch["aggPeriodS"] = cmd.aggPeriodS;
    }

    if (cmd.hasSessionId) {
      _clock.startNewSession(cmd.sessionId);
    } else {
      _clock.startNewSession(nullptr);
    }

    if (patch.size() > 0) {
      char buf[256];
      serializeJson(patch, buf, sizeof(buf));
      _commsEgress.applySettingsJson(buf);
    }

    enterState(State::Sampling);
    return;
  }

  if (cmd.type == protocol::Command::Type::stopSampling) {
    enterState(State::Aware);
    return;
  }

  if (cmd.type == protocol::Command::Type::getConfig) {
    // Ask comms layer to publish a (possibly chunked) config snapshot.
    _commsEgress.publishConfig();
    return;
  }

  if (cmd.type == protocol::Command::Type::hibernate) {
    const AppSettings s = _settings.getCopy();

    uint32_t sec = cmd.hasSleepSeconds ? cmd.sleepSeconds : 0u;
    if (sec == 0u) {
      sec = s.default_sleep_s;
    }
    if (sec > s.max_forced_sleep_s) {
      sec = s.max_forced_sleep_s;
    }

    _forcedHibernateS = sec;
    _hibernateReason  = HibernateReason::Forced;

    // Trigger main loop to perform the actual shutdown + standby sequence.
    _powerManager.requestSleep(RestartReasonCode::LowPowerWakeup, sec);

    enterState(State::Hibernating);
    return;
  }

  if (cmd.type == protocol::Command::Type::resetBatteryStatistics) {
    BoardHal::resetBatteryStatistics(hastig_battery());
    return;
  }

  if (cmd.type == protocol::Command::Type::factoryReset) {
    _settings.factoryReset();
    return;
  }

  LOGW(TAG, "Unknown command");
}

/**
 * @brief Handle keep-sampling ack (server heartbeat).
 */
void Orchestrator::handleAck()
{
  _lastAckMs = timeutil::nowMs();
  _unackedAggregateCount = 0;
}

/**
 * @brief Check timeouts in each state.
 */
void Orchestrator::checkTimeouts()
{
  const AppSettings s    = _settings.getCopy();
  const uint32_t    now  = timeutil::nowMs();

  // Periodic battery/status reporting (aware + sampling).
  if (_state == State::Aware || _state == State::Sampling) {
    if (_lastStatusMs == 0 || (now - _lastStatusMs) > (s.status_interval_s * 1000u)) {
      const BoardHal::BatterySnapshot bs = BoardHal::readBattery(hastig_battery());

      const char* modeStr = (_state == State::Sampling) ? "sampling" : "aware";
      _commsEgress.publishStatus(bs, modeStr);

      _lastStatusMs = now;

      // Low battery detection.
      if (bs.minimumVoltage < s.low_batt_min_v) {
        if (!_emergencyArmed) {
          _emergencyArmed = true;
          _emergencyAtMs  = now + (s.emergency_delay_s * 1000u);

          _commsEgress.publishLowBatteryAlert(bs, modeStr);
        }
      } else {
        _emergencyArmed = false;
      }
    }
  }

  // Execute emergency hibernate when armed and timer elapsed.
  if (_emergencyArmed && now >= _emergencyAtMs) {
    LOGW(TAG, "Emergency power save hibernate");
    _hibernateReason = HibernateReason::EmergencyPowerSave;
    _forcedHibernateS = s.emergency_sleep_s;
    _powerManager.requestSleep(RestartReasonCode::EmergencyPowerSave, s.emergency_sleep_s);
    enterState(State::Hibernating);
    return;
  }

  // Inactivity hibernate (aware or sampling) after last activity.
  if ((_state == State::Aware || _state == State::Sampling) &&
      (now - _lastActivityMs) > (s.aware_timeout_s * 1000u)) {
    LOGI(TAG, "Inactivity -> hibernate for %lu s", (unsigned long)s.default_sleep_s);
    _hibernateReason = HibernateReason::Inactivity;
    _forcedHibernateS = s.default_sleep_s;
    _powerManager.requestSleep(RestartReasonCode::LowPowerWakeup, s.default_sleep_s);
    enterState(State::Hibernating);
    return;
  }

  // Sampling keep-alive: go back to aware after max unacked aggregates.
  if (_state == State::Sampling) {
    uint32_t limit = s.max_unacked_packets;
    if (limit == 0u) {
      limit = 1u;
    }
    if (_unackedAggregateCount >= limit) {
      LOGW(TAG,
           "Unacked limit reached (%lu/%lu) -> back to aware",
           (unsigned long)_unackedAggregateCount,
           (unsigned long)limit);
      enterState(State::Aware);
    }
  }
}

/**
 * @brief Orchestrator main loop.
 */
void Orchestrator::run()
{
  LOGI(TAG, "Thread started");

  const uint32_t bootMs = timeutil::nowMs();
  _bootMs = bootMs;
  _lastActivityMs = bootMs;
  _lastStatusMs = 0;

  
  enterState(State::Aware);

  while (true) {
    const uint32_t nowMs = timeutil::nowMs();

    // If MQTT never comes up within timeout, conserve power.
    // Request hibernate only once; stay alive but quiet until PowerManager completes the transition.
    if (!_noNetworkHibernateRequested && _state != State::Hibernating && _mqttUpMs == 0 &&
        (nowMs - bootMs) > HASTIG_MQTT_CONNECT_TIMEOUT_MS) {
      _noNetworkHibernateRequested = true;
      LOGW(TAG, "No network/MQTT within timeout. Hibernating for %lu s", (unsigned long)HASTIG_NO_NETWORK_HIBERNATE_S);
      _hibernateReason  = HibernateReason::NoNetwork;
      _forcedHibernateS = (uint32_t)HASTIG_NO_NETWORK_HIBERNATE_S;
      _powerManager.requestSleep(RestartReasonCode::NoNetwork, (uint32_t)HASTIG_NO_NETWORK_HIBERNATE_S);
      enterState(State::Hibernating);
    }

    // Unified event stream (UI + Comms)
    DeviceEvent evt;
    if (_eventBus.tryGetNext(evt, 20)) {
      if (evt.type == DeviceEvent::Type::Ui) {
        // UI events (currently just logged; you will replace with your menu system)
        LOGD(TAG, "UI event %s=%s", evt.data.ui.topic, evt.data.ui.value);
      } else if (evt.type == DeviceEvent::Type::Worker) {
        const WorkerEventMsg& w = evt.data.worker;
        _lastActivityMs = nowMs;
        switch (w.type) {
          case WorkerEventType::SampleTaken:
            // Sampling tick observed (lightweight observability only)
            break;
          case WorkerEventType::AggregateReady:
            // Aggregate produced (n=%lu, ok=%d)
            break;
          default:
            break;
        }
      } else {
        // Comms events
        const CommsEventMsg& commEvt = evt.data.comms;
        switch (commEvt.type) {
          case CommsEventType::Boot:
            _lastActivityMs = nowMs;
            break;

          case CommsEventType::NetUp:
            LOGI(TAG, "NetUp");
            _lastActivityMs = nowMs;
            break;

          case CommsEventType::NetDown:
            LOGW(TAG, "NetDown");
            _lastActivityMs = nowMs;
            _mqttUpMs = 0;
            break;

          case CommsEventType::MqttUp:
            LOGI(TAG, "MqttUp");
            _lastActivityMs = nowMs;
            _mqttUpMs = nowMs;
            break;

          case CommsEventType::MqttDown:
            LOGW(TAG, "MqttDown");
            _lastActivityMs = nowMs;
            _mqttUpMs = 0;
            break;

          case CommsEventType::AggregatePublishAttempted:
            _lastActivityMs = nowMs;
            if (_state == State::Sampling) {
              _unackedAggregateCount++;
            }
            break;

          case CommsEventType::ServerCommand:
            _lastActivityMs = nowMs;
            handleServerCommand(commEvt.topic, commEvt.payload);
            break;

          case CommsEventType::PublishFailed:
            _lastActivityMs = nowMs;
            break;

          default:
            break;
        }
      }
    }

    checkTimeouts();
    rtos::ThisThread::sleep_for(milliseconds(20));
  }
}

void Orchestrator::stop()
{
  stoputil::terminateThread("Orchestrator", _thread);
}
