#pragma once

#include <stdint.h>

/**
 * @brief UI-to-orchestrator event message.
 */
struct UiEventMsg {
  uint32_t ts_ms;
  char     topic[64];
  char     value[128];
};

/**
 * @brief Sensor sample message (sensor -> aggregator).
 */
struct SensorSampleMsg {
  uint32_t relMs;
  char     k0[8];
  float    v0;
  char     k1[8];
  float    v1;
  bool     ok;
};

/**
 * @brief Aggregated message (aggregator -> comms).
 */
struct AggregateMsg {
  uint32_t rel_start_ms;
  uint32_t rel_end_ms;
  char     sessionId[48];

  char  k0[8];
  float v0_avg;
  float v0_min;
  float v0_max;

  char  k1[8];
  float v1_avg;
  float v1_min;
  float v1_max;

  uint32_t n;
  bool     ok;
};

/**
 * @brief Events from comms thread to orchestrator.
 */
enum class CommsEventType : uint8_t {
  Boot,
  NetUp,
  NetDown,
  MqttUp,
  MqttDown,
  AggregatePublishAttempted,
  ServerCommand,
  PublishFailed,
};

struct CommsEventMsg {
  CommsEventType type;
  uint32_t       ts_ms;
  char           topic[64];
  char           payload[256];
};

/**
 * @brief Typed event delivered to the orchestrator.
 */
struct DeviceEvent {
  enum class Type : uint8_t { Ui, Comms };

  Type type;
  union {
    UiEventMsg ui;
    CommsEventMsg comms;
  } data;
};
