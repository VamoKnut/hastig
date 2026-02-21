#pragma once

#include <stddef.h>
#include <stdint.h>

namespace protocol {

// Common JSON keys
static constexpr const char* kKeyType = "type";

// Command types (inbound)
static constexpr const char* kCmdKeepSampling = "keepSampling";
static constexpr const char* kCmdStartSampling = "startSampling";
static constexpr const char* kCmdStopSampling = "stopSampling";
static constexpr const char* kCmdGetConfig = "getConfig";
static constexpr const char* kCmdHibernate = "hibernate";
static constexpr const char* kCmdNudge = "nudge";
static constexpr const char* kCmdResetBatteryStatistics = "resetBatteryStatistics";
static constexpr const char* kCmdFactoryReset = "factoryReset";

// Optional command fields
static constexpr const char* kKeySleepSeconds = "sleepSeconds";
static constexpr const char* kKeySamplingInterval = "samplingInterval";
static constexpr const char* kKeyAggPeriodS = "aggPeriodS";
static constexpr const char* kKeySessionId = "sessionID";

// Outbound payload helpers
static constexpr const char* kKeyReason = "reason";
static constexpr const char* kKeyExpectedDuration = "expectedDuration";

// Encode the extra JSON blob appended to the "hibernating" status message.
// Example output: {"reason":"forced","expectedDuration":30}
bool encodeHibernatingExtra(const char* reason, uint32_t expectedDurationS, char* out, size_t outLen);

struct Command
{
  enum class Type : uint8_t {
    unknown = 0,
    keepSampling,
    startSampling,
    stopSampling,
    getConfig,
    hibernate,
    nudge,
    resetBatteryStatistics,
    factoryReset,
  };

  Type type = Type::unknown;

  // Optional fields
  bool hasSleepSeconds = false;
  uint32_t sleepSeconds = 0;

  bool hasSamplingInterval = false;
  uint32_t samplingInterval = 0;

  bool hasAggPeriodS = false;
  uint32_t aggPeriodS = 0;

  bool hasSessionId = false;
  char sessionId[48] = {0};
};

// Decode an inbound /cmd payload.
// Returns false if JSON is invalid.
bool decodeCommand(const char* json, Command& out);

// ---------------- MQTT topic helpers ----------------
// Topics are typically: <prefix>/<nodeId>/<postfix>

// Build a topic string: "<prefix>/<nodeId>/<postfix>".
// Returns false if output buffer is too small.
bool buildTopic(char* out, size_t outLen, const char* prefix, const char* nodeId, const char* postfix);

// Returns true if topic ends with "/<postfix>".
bool topicHasPostfix(const char* topic, const char* postfix);

} // namespace protocol
