#include "CommsEgress.h"

#include <Arduino.h>
#include <ArduinoJson.h>

#include "CommsCommands.h"
#include "CommandBus.h"
#include "Logger.h"
#include "ProtocolCodec.h"
#include "BoardHal.h"

#include <string.h>

static const char* TAG = "EGRESS";

static bool sendOrchCommand(CommandBus& bus, OrchCommandType type, const char* payloadOrNull)
{
  return bus.sendToComms(type, payloadOrNull);
}

static bool sendJsonCommand(CommandBus& bus, OrchCommandType type, const JsonDocument& doc,
                            char* out, size_t outSize)
{
  if (out == nullptr || outSize == 0u) {
    return false;
  }

  const size_t expectedBytes = measureJson(doc);
  if (expectedBytes >= outSize) {
    LOGW(TAG, "JSON command too large (%u bytes, buf=%u)", (unsigned)expectedBytes, (unsigned)outSize);
    return false;
  }

  const size_t n = serializeJson(doc, out, outSize);
  if (n == 0 || n >= outSize) {
    LOGW(TAG, "JSON command serialize failed/truncated (%u bytes, buf=%u)", (unsigned)n, (unsigned)outSize);
    return false;
  }
  out[n] = '\0';
  return sendOrchCommand(bus, type, out);
}

CommsEgress::CommsEgress(CommandBus& commandBus, AggMailT& aggToCommsMail)
    : _commandBus(commandBus), _aggToCommsMail(aggToCommsMail)
{
}

bool CommsEgress::sendAggregate(const AggregateMsg& msg)
{
  AggregateMsg* out = _aggToCommsMail.try_alloc();
  if (out == nullptr) {
    LOGW(TAG, "sendAggregate: alloc failed (mail full)");
    return false;
  }

  memcpy(out, &msg, sizeof(*out));
  _aggToCommsMail.put(out);
  return true;
}

bool CommsEgress::publishModeChange(const char* mode, const char* previousMode, const char* reason)
{
  JsonDocument st;
  st["type"]         = "modeChange";
  st["previousMode"] = previousMode;
  if (reason != nullptr && reason[0] != '\0') {
    st["reason"] = reason;
  }

  if (mode != nullptr && mode[0] != '\0') {
    st["mode"] = mode;
  }

  char out[256];
  const OrchCommandType type = (mode != nullptr && strcmp(mode, "hibernating") == 0)
                                   ? OrchCommandType::PublishHibernating
                                   : OrchCommandType::PublishAwake;

  return sendJsonCommand(_commandBus, type, st, out, sizeof(out));
}

bool CommsEgress::publishStatus(const BoardHal::BatterySnapshot& bs, const char* mode)
{
  JsonDocument st;
  st["type"]           = "status";
  st["mode"]           = mode;
  st["tsMs"]           = (uint32_t)millis();
  st["batteryVoltage"] = bs.voltage;
  st["minimumVoltage"] = bs.minimumVoltage;
  st["batteryCurrent"] = bs.current;
  st["averageCurrent"] = bs.averageCurrent;

  char out[384];
  return sendJsonCommand(_commandBus, OrchCommandType::PublishAwake, st, out, sizeof(out));
}

bool CommsEgress::publishLowBatteryAlert(const BoardHal::BatterySnapshot& bs, const char* mode)
{
  JsonDocument warn;
  warn["type"]          = "alert";
  warn["message"]       = "Critically low battery detected. Emergency hibernate soon.";
  warn["mode"]          = mode;
  warn["minimumVoltage"] = bs.minimumVoltage;

  char out[256];
  return sendJsonCommand(_commandBus, OrchCommandType::PublishAwake, warn, out, sizeof(out));
}

bool CommsEgress::publishAwake()
{
  return sendOrchCommand(_commandBus, OrchCommandType::PublishAwake, nullptr);
}

bool CommsEgress::publishAwakeJson(const char* json)
{
  return sendOrchCommand(_commandBus, OrchCommandType::PublishAwake, json);
}

bool CommsEgress::publishConfig()
{
  return sendOrchCommand(_commandBus, OrchCommandType::PublishConfig, nullptr);
}

bool CommsEgress::applySettingsJson(const char* json)
{
  return sendOrchCommand(_commandBus, OrchCommandType::ApplySettingsJson, json);
}

bool CommsEgress::publishHibernating(const char* reasonStr, uint32_t expectedDurationS)
{
  char extra[128];
  (void)protocol::encodeHibernatingExtra(reasonStr, expectedDurationS, extra, sizeof(extra));
  return sendOrchCommand(_commandBus, OrchCommandType::PublishHibernating, extra);
}

bool CommsEgress::publishHibernatingJson(const char* json)
{
  return sendOrchCommand(_commandBus, OrchCommandType::PublishHibernating, json);
}

bool CommsEgress::publishHibernateModeChange(const char* previousMode, const char* reasonStr, uint32_t expectedDurationS)
{
  JsonDocument st;
  st["type"]             = "modeChange";
  st["mode"]             = "hibernating";
  st["previousMode"]     = previousMode;
  st["reason"]           = reasonStr;
  st["expectedDuration"] = expectedDurationS;

  char out[256];
  return sendJsonCommand(_commandBus, OrchCommandType::PublishHibernating, st, out, sizeof(out));
}
