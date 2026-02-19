#pragma once

#include <ArduinoJson.h>

#include <mbed.h>

#include "AppConfig.h"
#include "CommsInbox.h"
#include "CommsCommands.h"
#include "Messages.h"
#include "SamplingThread.h"
#include "SessionClock.h"
#include "SettingsManager.h"
#include "EventBus.h"

template <uint32_t DEPTH>
using AggMail = rtos::Mail<AggregateMsg, DEPTH>;

template <uint32_t DEPTH>
using OrchToCommsMail = rtos::Mail<OrchCommandMsg, DEPTH>;


/**
 * @brief GSM + TCP + MQTT pump that runs in Arduino loop() context.
 *
 * Rationale:
 * Some GSM.h / GSMClient integrations behave reliably only when called from the
 * Arduino "main" context (setup/loop), not from a preempted RTOS thread.
 *
 * This class keeps the same mailbox wiring as CommsThread, but is executed by
 * main loop() via loopOnce().
 */
class CommsPump {
public:
  CommsPump(CommsInbox& inbox,
            OneShotMail<QUEUE_DEPTH_ONE_SHOT>& oneShotMail,
            EventBus& eventBus,
            SettingsManager& settings,
            SessionClock& clock);

  /**
   * @brief Initialize the pump (call from setup()).
   */
  void begin();

  /**
   * @brief One iteration of comms processing (call frequently from loop()).
   */
  void loopOnce();

  /** @brief True if MQTT is connected. */
  bool is_mqtt_connected() const { return _mqttConnected; }

  /** @brief Milliseconds since begin(). */
  uint32_t uptimeMs() const;

  /** @brief Disconnect MQTT/TCP and end GSM session. */
  void shutdown();
  void shutdownForHibernate();

  /**
   * @brief Prepare for hibernate: stop new connect attempts and tear down links non-blocking.
   *
   * This is safe to call multiple times.
   */
  void prepareHibernate();

private:
  CommsInbox&                         _inbox;
  OneShotMail<QUEUE_DEPTH_ONE_SHOT>&  _oneShotMail;
  EventBus&                                    _eventBus;
  SettingsManager&                              _settings;
  SessionClock&                               _clock;

  bool _wantConnected = true;
  bool _hibernatePending = false;

  bool     _netConnected  = false;
  bool     _mqttConnected = false;
  bool     _subscriptionsReady = false;
  uint32_t _lastNetOkMs   = 0;
  uint32_t _lastMqttOkMs  = 0;

  uint32_t _netFailCount  = 0;
  uint32_t _mqttFailCount = 0;

  uint32_t _bootMs = 0;

  char _topicCmd[96]    = {0};
  char _topicCfg[96]    = {0};
  char _topicData[96]   = {0};
  char _topicStatus[96] = {0};

  void postEvent(CommsEventType type, const char* topic, const char* payload);

  void handleOrchCommand(const OrchCommandMsg& cmd);

  bool ensureNetwork();
  bool ensureMqtt();

  void teardownLinks(bool endGsm);

  bool publishStatus(const char* mode, const char* extraJsonKVsOrNull);
  bool publishConfigSnapshot();
  bool publishAggregate(const AggregateMsg& a);
  bool publishOneShot(const SensorSampleMsg& s);

  bool publishJson(const char* topic, const JsonDocument& doc);

  void onMqttMessage(char* topic, uint8_t* payload, unsigned int len);
  static void mqttCallbackTrampoline(char* topic, uint8_t* payload, unsigned int len);

  static CommsPump* _self;
};
