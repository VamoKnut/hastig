#pragma once

#include "Messages.h"
#include "SettingsManager.h"

/**
 * @brief Abstract sensor interface.
 */
class Sensor {
public:
  virtual ~Sensor() = default;

  /** @brief Human-readable sensor name. */
  virtual const char* name() const = 0;

  /** @brief Initialize sensor comms (after power/warmup). */
  virtual bool begin(const AppSettings& s) = 0;

  /** @brief Deinitialize sensor comms (power saving). */
  virtual void end() = 0;

  /** @brief Take one sample. */
  virtual bool sample(SensorSampleMsg& out) = 0;

  /** @brief Factory: create sensor by sensorType. */
  static Sensor* create(uint32_t sensorType);
};
