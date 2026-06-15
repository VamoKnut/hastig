#pragma once

#include <stdint.h>

/**
 * @brief Restart reason codes stored across resets using the RTC backup domain.
 */
enum class RestartReasonCode : uint32_t {
  Unknown              = 0,
  UnexpectedReboot     = 1,
  LowPowerWakeup       = 2,
  NoNetwork            = 3,
  Forced               = 4,
  EmergencyPowerSave   = 5,
  BrownOut             = 6,
  WatchdogReset        = 7,
  ColdReboot           = 8,
};

/**
 * @brief Persistent restart-reason storage.
 */
class RestartReasonStore {
public:
  /** @brief Initialize backend. */
  void begin();

  /** @brief Read the reason classified during begin(). */
  RestartReasonCode read() const;

  /** @brief Write the expected reason for the next boot. */
  void write(RestartReasonCode code) const;

  /** @brief Number of valid boots seen by this store. */
  uint32_t bootCount() const;

  /** @brief Human-readable name for logging and status payloads. */
  static const char* toString(RestartReasonCode code);

private:
  RestartReasonCode _lastReason = RestartReasonCode::Unknown;
  uint32_t          _bootCount = 0;
};
