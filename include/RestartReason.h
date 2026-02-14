#pragma once

#include <stdint.h>

/**
 * @brief Restart reason codes stored across hibernate using battery-backed domain.
 *
 * Implementation note:
 * On Portenta H7 this should be backed by RTC backup registers.
 * For now we provide a thin abstraction with a stub backend that can be replaced.
 */
enum class RestartReasonCode : uint32_t {
  UnexpectedReboot     = 1,
  LowPowerWakeup       = 2,
  NoNetwork            = 3,
  Forced               = 4,
  EmergencyPowerSave   = 5,
  BrownOut             = 6,
};

/**
 * @brief Persistent restart-reason storage.
 */
class RestartReasonStore {
public:
  /** @brief Initialize backend. */
  void begin();

  /** @brief Read last stored reason. */
  RestartReasonCode read() const;

  /** @brief Write reason to persistent storage. */
  void write(RestartReasonCode code) const;
};
