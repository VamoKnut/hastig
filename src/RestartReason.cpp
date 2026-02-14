#include "RestartReason.h"

#include <mbed.h>

/**
 * @brief Stub backend.
 *
 * Replace with RTC backup register implementation when ready.
 */
static volatile uint32_t g_reason = (uint32_t)RestartReasonCode::UnexpectedReboot;

void RestartReasonStore::begin() {}

RestartReasonCode RestartReasonStore::read() const
{
  return (RestartReasonCode)g_reason;
}

void RestartReasonStore::write(RestartReasonCode code) const
{
  g_reason = (uint32_t)code;
}
