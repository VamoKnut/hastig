#pragma once

#include <stdint.h>

namespace WatchdogService {

bool begin();
void kick();
void beginLongOperation(uint32_t maxDurationMs);
void endLongOperation();
void prepareForHibernate();
bool isRunning();
void blockKicksForTest();
bool kicksBlocked();
void logStatus();

class ScopedLongOperation {
public:
  explicit ScopedLongOperation(uint32_t maxDurationMs)
  {
    beginLongOperation(maxDurationMs);
  }

  ~ScopedLongOperation()
  {
    endLongOperation();
  }

  ScopedLongOperation(const ScopedLongOperation&) = delete;
  ScopedLongOperation& operator=(const ScopedLongOperation&) = delete;
};

} // namespace WatchdogService
