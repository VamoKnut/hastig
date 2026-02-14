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

bool CommsEgress::publishModeChange(const char* mode, const char* previousMode)
{
  StaticJsonDocument<192> st;
  st["type"]         = "modeChange";
  st["previousMode"] = previousMode;

  if (mode != nullptr && mode[0] != '\0') {
    st["mode"] = mode;
  }

  char out[256];
  serializeJson(st, out, sizeof(out));

  if (mode != nullptr && strcmp(mode, "hibernate") == 0) {
    return sendOrchCommand(_commandBus, OrchCommandType::PublishHibernating, out);
  }
  return sendOrchCommand(_commandBus, OrchCommandType::PublishAwake, out);
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
  serializeJson(st, out, sizeof(out));
  return publishAwakeJson(out);
}

bool CommsEgress::publishLowBatteryAlert(const BoardHal::BatterySnapshot& bs, const char* mode)
{
  JsonDocument warn;
  warn["type"]          = "alert";
  warn["message"]       = "Critically low battery detected. Emergency hibernate soon.";
  warn["mode"]          = mode;
  warn["minimumVoltage"] = bs.minimumVoltage;

  char out[256];
  serializeJson(warn, out, sizeof(out));
  return publishAwakeJson(out);
}

bool CommsEgress::connect()
{
  return sendOrchCommand(_commandBus, OrchCommandType::Connect, nullptr);
}

bool CommsEgress::publishAwake()
{
  return sendOrchCommand(_commandBus, OrchCommandType::PublishAwake, nullptr);
}

bool CommsEgress::publishAwakeJson(const char* json)
{
  return sendOrchCommand(_commandBus, OrchCommandType::PublishAwake, json);
}

bool CommsEgress::startSamplingSession()
{
  return sendOrchCommand(_commandBus, OrchCommandType::StartSamplingSession, nullptr);
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
  StaticJsonDocument<224> st;
  st["type"]             = "modeChange";
  st["mode"]             = "hibernate";
  st["previousMode"]     = previousMode;
  st["reason"]           = reasonStr;
  st["expectedDuration"] = expectedDurationS;

  char out[256];
  serializeJson(st, out, sizeof(out));
  return publishHibernatingJson(out);
}

