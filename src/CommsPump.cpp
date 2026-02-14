#include "CommsPump.h"
#include "DeviceIdentity.h"
#include "ProtocolCodec.h"
#include "BoardHal.h"
#include "Logger.h"
#include "TimeUtil.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <math.h>
#include <string.h>
#include <GSM.h>
#include <PubSubClient.h>
#include <platform/ScopedLock.h>

#include <chrono>
using namespace std::chrono;

static const char* TAG = "COMMS";

CommsPump* CommsPump::_self = nullptr;

// GSM client + MQTT client (as in your working sketch)
static rtos::Mutex  gsmMx;
static GSMClient    gsmClient;
static PubSubClient mqtt(gsmClient);

/**
 * @brief Construct pump.
 */
CommsPump::CommsPump(CommsInbox& inbox,
                     OneShotMail<QUEUE_DEPTH_ONE_SHOT>& oneShotMail,
                     EventBus& eventBus,
                     SettingsManager& settings,
                     SessionClock& clock)
    : _inbox(inbox),
      _oneShotMail(oneShotMail),
      _eventBus(eventBus),
      _settings(settings),
      _clock(clock)
{
  _self = this;
}

/**
 * @brief Initialize pump (setup()).
 */
void CommsPump::begin()
{
  _bootMs = timeutil::nowMs();
  mqtt.setCallback(CommsPump::mqttCallbackTrampoline);
  postEvent(CommsEventType::Boot, "boot", "comms pump ready");
}

void CommsPump::prepareHibernate()
{
  _wantConnected = false;
  _hibernatePending = true;
  teardownLinks(false);
}

/**
 * @brief Post an event to orchestrator.
 */
void CommsPump::postEvent(CommsEventType type, const char* topic, const char* payload)
{
  CommsEventMsg e;
  e.type  = type;
  e.ts_ms = timeutil::nowMs();

  strncpy(e.topic, topic ? topic : "", sizeof(e.topic));
  strncpy(e.payload, payload ? payload : "", sizeof(e.payload));
  e.topic[sizeof(e.topic) - 1]     = '\0';
  e.payload[sizeof(e.payload) - 1] = '\0';

  (void)_eventBus.publish(e);
}

/**
 * @brief Handle orchestrator command.
 */
void CommsPump::handleOrchCommand(const OrchCommandMsg& cmd)
{
  if (cmd.type == OrchCommandType::Connect) {
    _wantConnected = true;
    _hibernatePending = false;
  } else if (cmd.type == OrchCommandType::Disconnect) {
    _wantConnected = false;
    _hibernatePending = false;
    teardownLinks(true);
  } else if (cmd.type == OrchCommandType::PrepareHibernate) {
    _wantConnected = false;
    _hibernatePending = true;
    teardownLinks(false);
  } else if (cmd.type == OrchCommandType::PublishAwake) {
    (void)publishStatus("aware", cmd.payload[0] ? cmd.payload : nullptr);
  } else if (cmd.type == OrchCommandType::PublishHibernating) {
    (void)publishStatus("hibernate", cmd.payload[0] ? cmd.payload : nullptr);
  } else if (cmd.type == OrchCommandType::PublishConfig) {
    (void)publishConfigSnapshot();
  } else if (cmd.type == OrchCommandType::ApplySettingsJson) {
    _settings.applyJson(cmd.payload, true);
    _topicCmd[0] = '\0';
  }
}

/**
 * @brief Tear down TCP/MQTT and optionally end GSM session.
 */
void CommsPump::teardownLinks(bool endGsm)
{
  LOGI(TAG, "teardownLinks(endGsm=%d) begin", endGsm ? 1 : 0);
  mbed::ScopedLock<rtos::Mutex> lock(gsmMx);

  if (mqtt.connected()) {
    if (endGsm) {
      LOGI(TAG, "teardownLinks: mqtt.disconnect()");
      mqtt.disconnect();
      LOGI(TAG, "teardownLinks: mqtt.disconnect() returned");
    } else {
      // Avoid potentially blocking network writes during hibernate.
      LOGI(TAG, "teardownLinks: skip mqtt.disconnect() (hibernate)");
    }
  }
  _mqttConnected = false;

  if (gsmClient.connected()) {
    LOGI(TAG, "teardownLinks: gsmClient.stop()");
    gsmClient.stop();
    LOGI(TAG, "teardownLinks: gsmClient.stop() returned");
  }

  if (endGsm) {
    LOGI(TAG, "teardownLinks: GSM.end()");
    GSM.end();
    LOGI(TAG, "teardownLinks: GSM.end() returned");
    _netConnected = false;
  }
  LOGI(TAG, "teardownLinks end");
}

/**
 * @brief Ensure cellular network is up.
 */
bool CommsPump::ensureNetwork()
{
  if (!_wantConnected) {
    return false;
  }

  if (_netConnected) {
    return true;
  }

  const AppSettings s = _settings.getCopy();


  LOGI(TAG, "Connecting to 4G network (APN=%s)...", s.apn);

  // We keep the recovery minimal; no external power toggling here.
  bool ok = false;
  {
    mbed::ScopedLock<rtos::Mutex> lock(gsmMx);
    ok = GSM.begin(s.sim_pin, s.apn, s.apn_user, s.apn_pass, CATM1, 524288UL, true);
  }

  if (ok) {
    _netConnected = true;
    _netFailCount = 0;
    _lastNetOkMs  = timeutil::nowMs();
    postEvent(CommsEventType::NetUp, "net", "up");
    LOGI(TAG, "GSM.begin OK");
    return true;
  }

  _netConnected = false;
  _netFailCount++;
  postEvent(CommsEventType::NetDown, "net", "down");
  LOGW(TAG, "GSM.begin failed (count=%lu)", (unsigned long)_netFailCount);

  // Backoff and occasionally reset modem stack.
  if ((_netFailCount % 3u) == 0u) {
    mbed::ScopedLock<rtos::Mutex> lock(gsmMx);
    GSM.reset();
  }

  rtos::ThisThread::sleep_for(milliseconds((_netFailCount < 5) ? 1500u : 5000u));
  return false;
}

/**
 * @brief Ensure MQTT is connected, using explicit TCP connect first (your proven pattern).
 */
bool CommsPump::ensureMqtt()
{
  if (_hibernatePending) {
    return false;
  }

  if (!_wantConnected) {
    return false;
  }

  if (!ensureNetwork()) {
    _mqttConnected = false;
    return false;
  }

  const AppSettings s = _settings.getCopy();


  if (_topicCmd[0] == '\0') {
    // Use friendly name for topic segment, fallback to a stable hardware ID.
    const char* node = s.device_name;
    char        hwId[32];
    if (node[0] == '\0') {
      DeviceIdentity::getHardwareId(hwId, sizeof(hwId));
      node = hwId;
    }

    (void)protocol::buildTopic(_topicCmd, sizeof(_topicCmd), MQTT_TOPIC_PREFIX, node, MQTT_TOPIC_POSTFIX_CMD);
    (void)protocol::buildTopic(_topicCfg, sizeof(_topicCfg), MQTT_TOPIC_PREFIX, node, MQTT_TOPIC_POSTFIX_CFG);
    (void)protocol::buildTopic(_topicData, sizeof(_topicData), MQTT_TOPIC_PREFIX, node, "data");
    (void)protocol::buildTopic(_topicStatus, sizeof(_topicStatus), MQTT_TOPIC_PREFIX, node, "status");
  }

  mqtt.setServer(s.mqtt_host, (uint16_t)s.mqtt_port);
  mqtt.setBufferSize(512);

  if (mqtt.connected()) {
    _mqttConnected = true;
    _lastMqttOkMs  = timeutil::nowMs();
    return true;
  }

  // 1) Ensure TCP socket
  bool tcpOk = gsmClient.connected();
  if (!tcpOk) {
    LOGI(TAG, "Opening TCP to MQTT server %s:%u ...", s.mqtt_host, (unsigned)s.mqtt_port);
    for (int i = 0; i < 3 && _wantConnected; i++) {
      {
        mbed::ScopedLock<rtos::Mutex> lock(gsmMx);
        tcpOk = gsmClient.connect(s.mqtt_host, s.mqtt_port);
      }
      if (tcpOk) {
        break;
      }
      LOGW(TAG, "TCP connect failed (%d/3)", i + 1);
      rtos::ThisThread::sleep_for(milliseconds(1000));
    }
  }

  if (!tcpOk) {
    _mqttFailCount++;
    teardownLinks(false);
    postEvent(CommsEventType::MqttDown, "mqtt", "tcp_fail");
    _lastNetOkMs = 0;
    return false;
  }

  // 2) MQTT CONNECT
  LOGI(TAG, "MQTT connecting ...");

  bool connected = false;
  {
    // Keep the CONNECT atomic with respect to other GSM operations.
    mbed::ScopedLock<rtos::Mutex> lock(gsmMx);
    if (strlen(s.mqtt_user) > 0) {
      connected = mqtt.connect(s.mqtt_client_id, s.mqtt_user, s.mqtt_pass);
    } else {
      connected = mqtt.connect(s.mqtt_client_id);
    }
  }

  if (connected) {
  mqtt.subscribe(_topicCmd);
  mqtt.subscribe(_topicCfg);
    _mqttConnected = true;
    _mqttFailCount = 0;
    _lastMqttOkMs  = timeutil::nowMs();
    postEvent(CommsEventType::MqttUp, "mqtt", "up");
    LOGI(TAG, "MQTT connected, subscribed to %s", _topicCmd);
    return true;
  }

  _mqttFailCount++;
  LOGW(TAG, "MQTT connect failed. state=%d", mqtt.state());
  teardownLinks(false);
  postEvent(CommsEventType::MqttDown, "mqtt", "down");
  return false;
}

/**
 * @brief Publish status message.
 */
bool CommsPump::publishStatus(const char* mode, const char* extraJsonKVsOrNull)
{
  if (!ensureMqtt()) {
    return false;
  }

  JsonDocument doc;
  doc["type"] = "status";
  doc["tsMs"] = (uint32_t)millis();
  doc["mode"] = mode;
if (extraJsonKVsOrNull != nullptr && extraJsonKVsOrNull[0] != '\0') {
    JsonDocument extra;
    if (deserializeJson(extra, extraJsonKVsOrNull) == DeserializationError::Ok && extra.is<JsonObject>()) {
      for (JsonPair kv : extra.as<JsonObject>()) {
        doc[kv.key().c_str()] = kv.value();
      }
    }
  }

  char out[512];
  serializeJson(doc, out, sizeof(out));

  const bool ok = mqtt.publish(_topicStatus, out);
  if (!ok) {
    postEvent(CommsEventType::PublishFailed, _topicStatus, "publish status failed");
  }
  return ok;
}


bool CommsPump::publishConfigSnapshot()
{
  if (!ensureMqtt()) {
    return false;
  }


  // PubSubClient buffer size is 512 bytes, but it includes topic/header overhead.
  // To stay safely within the limit, keep payloads well below 512.
  static constexpr size_t MAX_CONFIG_PAYLOAD_BYTES = 320;

  // Try single-message first (backward compatible).
  {
    StaticJsonDocument<512> doc;
    doc["type"] = "config";
    doc["tsMs"] = (uint32_t)millis();
    _settings.addMaskedConfigFields(doc, SettingsManager::ConfigSection::All);

    const size_t bytes = measureJson(doc);
    if (bytes <= MAX_CONFIG_PAYLOAD_BYTES) {
      return publishJson(_topicStatus, doc);
    }

    LOGW(TAG, "Config snapshot too large (%u bytes). Publishing as chunks.", (unsigned)bytes);
  }

  // Chunked publish. Categories keep chunks stable and easy to read.
  // Each chunk is published on the status topic.
  const uint8_t total = 5;

  const auto publishChunk = [&](uint8_t chunk, const char* section, auto fill) -> bool {
    StaticJsonDocument<384> doc;
    doc["type"] = "configChunk";
    doc["tsMs"] = (uint32_t)millis();
    doc["chunk"] = chunk;
    doc["total"] = total;
    doc["section"] = section;

    fill(doc);

    const size_t bytes = measureJson(doc);
    if (bytes > MAX_CONFIG_PAYLOAD_BYTES) {
      LOGW(TAG, "Config chunk %u/%u (%s) is %u bytes (limit %u).",
           (unsigned)chunk,
           (unsigned)total,
           section,
           (unsigned)bytes,
           (unsigned)MAX_CONFIG_PAYLOAD_BYTES);
    }

    return publishJson(_topicStatus, doc);
  };

  bool ok = true;

  ok = ok && publishChunk(1, "network", [&](auto& doc) {
    _settings.addMaskedConfigFields(doc, SettingsManager::ConfigSection::Network);
  });

  ok = ok && publishChunk(2, "mqtt", [&](auto& doc) {
    _settings.addMaskedConfigFields(doc, SettingsManager::ConfigSection::Mqtt);
  });

  ok = ok && publishChunk(3, "device", [&](auto& doc) {
    _settings.addMaskedConfigFields(doc, SettingsManager::ConfigSection::Device);
  });

  ok = ok && publishChunk(4, "schedule", [&](auto& doc) {
    _settings.addMaskedConfigFields(doc, SettingsManager::ConfigSection::Schedule);
  });

  ok = ok && publishChunk(5, "power", [&](auto& doc) {
    _settings.addMaskedConfigFields(doc, SettingsManager::ConfigSection::Power);
  });

  return ok;
}


/**
 * @brief Publish aggregated data packet.
 */
bool CommsPump::publishAggregate(const AggregateMsg& a)
{
  StaticJsonDocument<512> doc;
  doc["type"] = "data";
  doc["t0"] = a.rel_start_ms;
  doc["t1"] = a.rel_end_ms;
  doc["n"]  = a.n;
  doc["ok"] = a.ok ? 1 : 0;

  char k[16];
  snprintf(k, sizeof(k), "%sAvg", a.k0);
  doc[k] = roundf(a.v0_avg * 100.0f) / 100.0f;
  snprintf(k, sizeof(k), "%sMin", a.k0);
  doc[k] = roundf(a.v0_min * 100.0f) / 100.0f;
  snprintf(k, sizeof(k), "%sMax", a.k0);
  doc[k] = roundf(a.v0_max * 100.0f) / 100.0f;

  if (a.k1[0] != '\0') {
    const bool isTemp = (strcmp(a.k1, "temp") == 0);
    const float mul   = isTemp ? 10.0f : 100.0f;

    snprintf(k, sizeof(k), "%sAvg", a.k1);
    doc[k] = roundf(a.v1_avg * mul) / mul;
    snprintf(k, sizeof(k), "%sMin", a.k1);
    doc[k] = roundf(a.v1_min * mul) / mul;
    snprintf(k, sizeof(k), "%sMax", a.k1);
    doc[k] = roundf(a.v1_max * mul) / mul;
  }

  return publishJson(_topicData, doc);
}

/**
 * @brief MQTT callback trampoline.
 */
void CommsPump::mqttCallbackTrampoline(char* topic, uint8_t* payload, unsigned int len)
{
  if (_self != nullptr) {
    _self->onMqttMessage(topic, payload, len);
  }
}

/**
 * @brief Handle inbound MQTT messages and forward to orchestrator.
 */
void CommsPump::onMqttMessage(char* topic, uint8_t* payload, unsigned int len)
{
  BoardHal::blinkLed(BoardHal::LedColor::Red, 100);

  // Copy payload to a null-terminated buffer on stack.
  char buf[256];
  const size_t n = (len < sizeof(buf) - 1u) ? (size_t)len : (sizeof(buf) - 1u);
  memcpy(buf, payload, n);
  buf[n] = '\0';

  const char* t = (topic != nullptr) ? topic : "";

  // Step 1 routing:
  //  - /cfg payloads are applied immediately by SettingsManager
  //  - /cmd payloads are forwarded to Orchestrator
  if (protocol::topicHasPostfix(t, MQTT_TOPIC_POSTFIX_CFG)) {
    const bool ok = _settings.applyJson(buf, true);
    LOGI(TAG, "RX cfg topic=%s applied=%d payload=%s", t, ok ? 1 : 0, buf);
    return;
  }

  postEvent(CommsEventType::ServerCommand, t, buf);

  if (protocol::topicHasPostfix(t, MQTT_TOPIC_POSTFIX_CMD)) {
    LOGI(TAG, "RX cmd topic=%s payload=%s", t, buf);
  } else {
    LOGI(TAG, "RX topic=%s payload=%s", t, buf);
  }
}

/**
 * @brief One iteration of comms pump.
 */
void CommsPump::loopOnce()
{
  // Drain orchestrator commands
  while (true) {
    OrchCommandMsg* cmd = _inbox.tryGetOrch();
    if (cmd == nullptr) {
      break;
    }
    handleOrchCommand(*cmd);
    _inbox.freeOrch(cmd);
  }

  // Maintain connections + process inbound MQTT
  if (_wantConnected) {
    if (!mqtt.connected()) {
      (void)ensureMqtt();
    }

    // PubSubClient::loop() returns bool in most versions; if it fails, drop the link and reconnect later.
    const bool loopOk = mqtt.loop();
    if (!loopOk && !mqtt.connected()) {
      postEvent(CommsEventType::MqttDown, "mqtt", "loop_fail");
      teardownLinks(false);
    }
  }

  // Drain aggregates and publish
  while (true) {
    AggregateMsg* a = _inbox.tryGetAggregate();
    if (a == nullptr) {
      break;
    }
    (void)publishAggregate(*a);
    _inbox.freeAggregate(a);
  }
}

uint32_t CommsPump::uptimeMs() const
{
  const uint32_t now = timeutil::nowMs();
  return now - _bootMs;
}

void CommsPump::shutdown()
{
  _wantConnected = false;
  teardownLinks(true);
}

void CommsPump::shutdownForHibernate()
{
  // In hibernate we will cut power rails anyway; avoid GSM.end() which may block.
  _wantConnected = false;
  teardownLinks(false);
}



bool CommsPump::publishJson(const char* topic, const JsonDocument& doc)
{
  // Always publish valid, null-terminated JSON to avoid downstream parsers that
  // (incorrectly) treat payloads as C strings.
  char buf[512] = {0};

  const size_t expectedBytes = measureJson(doc);
  if (expectedBytes >= sizeof(buf)) {
    LOGW(TAG, "publishJson: payload too large (%u bytes, buf=%u) topic=%s",
         (unsigned)expectedBytes,
         (unsigned)sizeof(buf),
         topic);
    return false;
  }

  const size_t n = serializeJson(doc, buf, sizeof(buf));
  if (n == 0) {
    return false;
  }
  // Ensure termination even if ArduinoJson didn't write it.
  buf[n] = '\0';

  // Use the C-string publish overload so the payload length is derived from strlen().
  // This keeps the MQTT payload clean when receivers assume null-termination.
  const bool ok = mqtt.publish(topic, buf);
  if (ok) {
    BoardHal::blinkLed(BoardHal::LedColor::Blue, 100);
  }
  return ok;
}



bool CommsPump::publishOneShot(const SensorSampleMsg& s)
{
  StaticJsonDocument<256> doc;
  doc["type"] = "oneShotSampleResult";
  doc["t"] = s.relMs;
  doc["ok"] = s.ok ? 1 : 0;

  doc[s.k0] = roundf(s.v0 * 100.0f) / 100.0f;
  if (s.k1[0] != '\0') {
    const bool isTemp = (strcmp(s.k1, "temp") == 0);
    const float mul   = isTemp ? 10.0f : 100.0f;
    doc[s.k1] = roundf(s.v1 * mul) / mul;
  }

  return publishJson(_topicData, doc);
}
