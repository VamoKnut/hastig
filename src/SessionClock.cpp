#include "TimeUtil.h"
\
#include "SessionClock.h"

#include <Arduino.h>
#include <platform/ScopedLock.h>
#include <string.h>
#include <platform/ScopedLock.h>

static uint32_t mix32(uint32_t x)
{
  x ^= x >> 16;
  x *= 0x7feb352d;
  x ^= x >> 15;
  x *= 0x846ca68b;
  x ^= x >> 16;
  return x;
}

/**
 * @brief Initialize session clock data.
 */
void SessionClock::begin()
{
  mbed::ScopedLock<rtos::Mutex> lock(_mx);
  _refMs = timeutil::nowMs();
  strncpy(_sessionId, "none", sizeof(_sessionId));
  _sessionId[sizeof(_sessionId) - 1] = '\0';
}

/**
 * @brief Start new session with optional server-provided session id.
 */
void SessionClock::startNewSession(const char* serverSessionIdOrNull)
{
  mbed::ScopedLock<rtos::Mutex> lock(_mx);

  _refMs = timeutil::nowMs();

  if (serverSessionIdOrNull != nullptr && serverSessionIdOrNull[0] != '\0') {
    strncpy(_sessionId, serverSessionIdOrNull, sizeof(_sessionId));
    _sessionId[sizeof(_sessionId) - 1] = '\0';
  } else {
    generateLocalGuid(_sessionId, sizeof(_sessionId));
  }
}

/**
 * @brief Copy session id into caller buffer.
 */
void SessionClock::getSessionId(char* out, size_t outLen) const
{
  mbed::ScopedLock<rtos::Mutex> lock(_mx);
  if (outLen == 0) {
    return;
  }
  strncpy(out, _sessionId, outLen);
  out[outLen - 1] = '\0';
}

/**
 * @brief Return milliseconds relative to current session reference time.
 */
uint32_t SessionClock::relMs() const
{
  mbed::ScopedLock<rtos::Mutex> lock(_mx);
  const uint32_t now = timeutil::nowMs();
  return (uint32_t)(now - _refMs);
}

/**
 * @brief Generate a local GUID-like id (hex), not cryptographic.
 */
void SessionClock::generateLocalGuid(char* out, size_t outLen)
{
  const uint32_t a = mix32((uint32_t)micros() ^ (uint32_t)analogRead(A0));
  const uint32_t b = mix32((uint32_t)millis() ^ (uint32_t)analogRead(A1));
  const uint32_t c = mix32(a ^ b ^ 0x12345678u);
  const uint32_t d = mix32(b ^ c ^ 0x9abcdef0u);

  snprintf(out,
           outLen,
           "%08lx-%08lx-%08lx-%08lx",
           (unsigned long)a,
           (unsigned long)b,
           (unsigned long)c,
           (unsigned long)d);
}
