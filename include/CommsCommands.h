#pragma once

#include <stdint.h>

/**
 * @brief Commands from orchestrator-facing code to the comms subsystem.
 *
 * These are intentionally kept separate from Messages.h to avoid leaking
 * comms command details into unrelated modules.
 */

enum class OrchCommandType : uint8_t {
  Connect,
  Disconnect,
  PrepareHibernate,
  PublishAwake,
  PublishHibernating,
  ApplySettingsJson,
  StartSamplingSession,
  StopSamplingSession,
  PublishConfig,
};

struct OrchCommandMsg {
  OrchCommandType type;
  uint32_t        ts_ms;
  char            payload[256];
};
