#include "DeviceIdentity.h"

/**
 * @brief Read STM32H7 unique device ID registers.
 *
 * The Portenta H7 (STM32H747) exposes a 96-bit unique ID at 0x1FF1E800.
 */
void DeviceIdentity::getHardwareId(char* out, size_t outSize)
{
  if (out == nullptr || outSize == 0u) {
    return;
  }

  const uint32_t* uid = (const uint32_t*)0x1FF1E800UL;
  const uint32_t  u0  = uid[0];
  const uint32_t  u1  = uid[1];
  const uint32_t  u2  = uid[2];

  // 24 hex chars + null
  snprintf(out, outSize, "%08lX%08lX%08lX", (unsigned long)u0, (unsigned long)u1,
           (unsigned long)u2);
}
