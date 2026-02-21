#include "ProtocolCodec.h"

#include <ArduinoJson.h>
#include <stdio.h>
#include <string.h>

namespace protocol {

static Command::Type decodeType(const char* type)
{
  if (type == nullptr) {
    return Command::Type::unknown;
  }

  if (strcmp(type, kCmdKeepSampling) == 0) {
    return Command::Type::keepSampling;
  }
  if (strcmp(type, kCmdStartSampling) == 0) {
    return Command::Type::startSampling;
  }
  if (strcmp(type, kCmdStopSampling) == 0) {
    return Command::Type::stopSampling;
  }
  if (strcmp(type, kCmdGetConfig) == 0) {
    return Command::Type::getConfig;
  }
  if (strcmp(type, kCmdHibernate) == 0) {
    return Command::Type::hibernate;
  }
  if (strcmp(type, kCmdNudge) == 0) {
    return Command::Type::nudge;
  }
  if (strcmp(type, kCmdResetBatteryStatistics) == 0) {
    return Command::Type::resetBatteryStatistics;
  }
  if (strcmp(type, kCmdFactoryReset) == 0) {
    return Command::Type::factoryReset;
  }

  return Command::Type::unknown;
}

bool decodeCommand(const char* json, Command& out)
{
  out = Command{};

  JsonDocument doc;
  const auto err = deserializeJson(doc, json);
  if (err) {
    return false;
  }

  const char* typeStr = doc[kKeyType] | "";
  out.type = decodeType(typeStr);

  if (doc[kKeySleepSeconds].is<uint32_t>()) {
    out.hasSleepSeconds = true;
    out.sleepSeconds = doc[kKeySleepSeconds].as<uint32_t>();
  }

  if (doc[kKeySamplingInterval].is<uint32_t>()) {
    out.hasSamplingInterval = true;
    out.samplingInterval = doc[kKeySamplingInterval].as<uint32_t>();
  }

  if (doc[kKeyAggPeriodS].is<uint32_t>()) {
    out.hasAggPeriodS = true;
    out.aggPeriodS = doc[kKeyAggPeriodS].as<uint32_t>();
  }

  if (doc[kKeySessionId].is<const char*>()) {
    const char* sid = doc[kKeySessionId].as<const char*>();
    if (sid != nullptr && sid[0] != '\0') {
      out.hasSessionId = true;
      strncpy(out.sessionId, sid, sizeof(out.sessionId));
      out.sessionId[sizeof(out.sessionId) - 1] = '\0';
    }
  }

  return true;
}

bool buildTopic(char* out, size_t outLen, const char* prefix, const char* nodeId, const char* postfix)
{
  if ((out == nullptr) || (outLen == 0) || (prefix == nullptr) || (nodeId == nullptr) || (postfix == nullptr)) {
    return false;
  }

  const int n = snprintf(out, outLen, "%s/%s/%s", prefix, nodeId, postfix);
  if (n < 0) {
    out[0] = '\0';
    return false;
  }
  // snprintf returns number of chars that would have been written excluding '\0'
  if ((size_t)n >= outLen) {
    out[0] = '\0';
    return false;
  }
  return true;
}

bool topicHasPostfix(const char* topic, const char* postfix)
{
  if ((topic == nullptr) || (postfix == nullptr)) {
    return false;
  }
  const size_t tl = strlen(topic);
  const size_t pl = strlen(postfix);
  // Need at least: "/" + postfix
  if (tl < (pl + 1u)) {
    return false;
  }
  if (topic[tl - pl - 1u] != '/') {
    return false;
  }
  return (strcmp(topic + (tl - pl), postfix) == 0);
}

bool encodeHibernatingExtra(const char* reason, uint32_t expectedDurationS, char* out, size_t outLen)
{
  if (out == nullptr || outLen == 0) {
    return false;
  }

  JsonDocument doc;
  doc[kKeyReason] = (reason != nullptr) ? reason : "";
  doc[kKeyExpectedDuration] = expectedDurationS;

  const size_t n = serializeJson(doc, out, outLen);
  return n > 0;
}

} // namespace protocol
