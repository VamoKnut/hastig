#include "StopUtil.h"

#include "Logger.h"

#include <Arduino.h> // millis(), delay()

namespace stoputil {

static const char* threadStateToString(rtos::Thread::State state)
{
  switch (state) {
    case rtos::Thread::Inactive:
      return "Inactive";
    case rtos::Thread::Ready:
      return "Ready";
    case rtos::Thread::Running:
      return "Running";
    case rtos::Thread::WaitingDelay:
      return "WaitingDelay";
    case rtos::Thread::WaitingJoin:
      return "WaitingJoin";
    case rtos::Thread::WaitingThreadFlag:
      return "WaitingThreadFlag";
    case rtos::Thread::WaitingEventFlag:
      return "WaitingEventFlag";
    case rtos::Thread::WaitingMutex:
      return "WaitingMutex";
    case rtos::Thread::WaitingSemaphore:
      return "WaitingSemaphore";
    case rtos::Thread::WaitingMemoryPool:
      return "WaitingMemoryPool";
    case rtos::Thread::WaitingMessageGet:
      return "WaitingMessageGet";
    case rtos::Thread::WaitingMessagePut:
      return "WaitingMessagePut";
    default:
      return "Unknown";
  }
}

void terminateThread(const char* name, rtos::Thread& thread, uint32_t waitMs)
{
  static const char* TAG = "STOP";

  LOGI(TAG, "%s stop: terminate() begin", name);
  const auto st = thread.terminate(); // returns osStatus (type varies across toolchains)
  LOGI(TAG, "%s stop: terminate() returned %ld", name, (long)st);

  // Best-effort: wait a short time for the RTOS to mark the thread inactive.
  const uint32_t start = (uint32_t)millis();
  while (((uint32_t)millis() - start) < waitMs) {
    const auto s = thread.get_state();
    if (s == rtos::Thread::Inactive) {
      LOGI(TAG, "%s stop: state now %s", name, threadStateToString(s));
      return;
    }
    delay(10);
  }

  const auto s = thread.get_state();
  LOGW(TAG, "%s stop: state still %s", name, threadStateToString(s));
}

} // namespace stoputil
