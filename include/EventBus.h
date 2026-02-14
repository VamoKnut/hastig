#pragma once

#include <mbed.h>

#include "AppConfig.h"
#include "Messages.h"

/**
 * @brief Unified device event from UI or Comms.
 *
 * This is a small step towards a single internal stream, without changing
 * the underlying mailboxes.
 */
struct DeviceEvent {
  enum class Type : uint8_t { Ui, Comms, Worker };

  Type type;

  union {
    UiEventMsg ui;
    CommsEventMsg comms;
    WorkerEventMsg worker;
  } data;
};

/**
 * @brief Minimal event-bus facade (incremental internal pub/sub).
 *
 * This facade wraps existing mailboxes to present a single stream of
 * DeviceEvent to the orchestrator. Publishing currently supports the
 * comms->orchestrator direction.
 */
class EventBus {
public:
  EventBus(rtos::Mail<UiEventMsg, QUEUE_DEPTH_UI_TO_ORCH>& uiToOrchMail,
           rtos::Mail<CommsEventMsg, QUEUE_DEPTH_COMMS_TO_ORCH>& commsToOrchMail,
           rtos::Mail<WorkerEventMsg, QUEUE_DEPTH_WORKER_TO_ORCH>& workerToOrchMail);

  // Publish a comms-originated event to the orchestrator stream.
  bool publish(const CommsEventMsg& evt);

  // Publish a UI-originated event to the orchestrator stream.
  bool publishUi(const UiEventMsg& evt);

  // Publish a worker-originated event to the orchestrator stream.
  bool publishWorker(const WorkerEventMsg& evt);

  // Retrieve next UI or Comms event. Returns true if an event was received.
  bool tryGetNext(DeviceEvent& outEvt, uint32_t timeoutMs);

private:
  rtos::Mail<UiEventMsg, QUEUE_DEPTH_UI_TO_ORCH>& _uiToOrchMail;
  rtos::Mail<CommsEventMsg, QUEUE_DEPTH_COMMS_TO_ORCH>& _commsToOrchMail;
  rtos::Mail<WorkerEventMsg, QUEUE_DEPTH_WORKER_TO_ORCH>& _workerToOrchMail;
};
