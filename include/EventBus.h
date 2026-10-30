#pragma once

#include <mbed.h>

#include "AppConfig.h"
#include "Messages.h"

/**
 * @brief FIFO event channel consumed by the orchestrator.
 */
class EventBus {
public:
  using Mail = rtos::Mail<DeviceEvent, QUEUE_DEPTH_ORCH_EVENTS>;

  explicit EventBus(Mail& eventsToOrchMail);

  bool publish(const CommsEventMsg& evt);
  bool publish(const UiEventMsg& evt);

  // Waits up to timeoutMs for the next event.
  bool tryGetNext(DeviceEvent& outEvt, uint32_t timeoutMs);

private:
  Mail& _eventsToOrchMail;
  bool publishEvent(const DeviceEvent& event);
};
