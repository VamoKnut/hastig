#pragma once

#include <mbed.h>
#include <stdint.h>

/**
 * @brief Time helpers for mbed-os 6+.
 */
namespace timeutil {

/**
 * @brief Return RTOS uptime in milliseconds (32-bit).
 */
inline uint32_t nowMs()
{
  const auto now = rtos::Kernel::Clock::now().time_since_epoch();
  const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
  return (uint32_t)ms;
}

}  // namespace timeutil
