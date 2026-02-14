#pragma once

#include <mbed.h>
#include <platform/ScopedLock.h>
#include <stdint.h>

/**
 * @brief Session reference time helper + sessionId.
 */
class SessionClock {
public:
  /**
   * @brief Initialize session clock.
   */
  void begin();

  /**
   * @brief Start new session reference time.
   */
  void startNewSession(const char* serverSessionIdOrNull);

  /**
   * @brief Get current session id string.
   */
  void getSessionId(char* out, size_t outLen) const;

  /**
   * @brief Milliseconds since reference start.
   */
  uint32_t relMs() const;

private:
  mutable rtos::Mutex _mx;
  uint32_t            _refMs = 0;
  char                _sessionId[48] = {0};

  void generateLocalGuid(char* out, size_t outLen);
};
