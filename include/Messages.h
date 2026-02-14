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
 * @brief Lightweight worker events for orchestrator visibility.
 *
 * These are intentionally small and do not replace the existing data
 * mailboxes (sensor->agg, agg->comms). They provide observability and a
 * single event stream in the orchestrator.
 */
enum class WorkerEventType : uint8_t {
  SampleTaken,
  AggregateReady,
};

struct WorkerEventMsg {
  WorkerEventType type;
  uint32_t        ts_ms;
  uint32_t        relMs;
  uint32_t        n;
  bool            ok;
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
  ServerCommand,
  PublishFailed,
};

struct CommsEventMsg {
  CommsEventType type;
  uint32_t       ts_ms;
  char           topic[64];
  char           payload[256];
};
