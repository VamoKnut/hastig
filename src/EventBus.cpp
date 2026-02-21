#include "EventBus.h"

#include "Logger.h"

#include <Arduino.h>

static const char* TAG = "EVTB";

EventBus::EventBus(rtos::Mail<UiEventMsg, QUEUE_DEPTH_UI_TO_ORCH>& uiToOrchMail,
                   rtos::Mail<CommsEventMsg, QUEUE_DEPTH_COMMS_TO_ORCH>& commsToOrchMail,
                   rtos::Mail<WorkerEventMsg, QUEUE_DEPTH_WORKER_TO_ORCH>& workerToOrchMail)
  : _uiToOrchMail(uiToOrchMail),
    _commsToOrchMail(commsToOrchMail),
    _workerToOrchMail(workerToOrchMail)
{
}

bool EventBus::publish(const CommsEventMsg& evt)
{
  CommsEventMsg* m = _commsToOrchMail.try_alloc();
  if (m == nullptr) {
    LOGW(TAG, "publish: commsToOrchMail alloc failed");
    return false;
  }

  *m = evt;
  const auto st = _commsToOrchMail.put(m);
  if (st != osOK) {
    LOGW(TAG, "publish: commsToOrchMail put failed");
    _commsToOrchMail.free(m);
    return false;
  }

  return true;
}

bool EventBus::publishUi(const UiEventMsg& evt)
{
  UiEventMsg* m = _uiToOrchMail.try_alloc();
  if (m == nullptr) {
    LOGW(TAG, "publishUi: uiToOrchMail alloc failed");
    return false;
  }

  *m = evt;
  const auto st = _uiToOrchMail.put(m);
  if (st != osOK) {
    LOGW(TAG, "publishUi: uiToOrchMail put failed");
    _uiToOrchMail.free(m);
    return false;
  }

  return true;
}

bool EventBus::publishWorker(const WorkerEventMsg& evt)
{
  WorkerEventMsg* m = _workerToOrchMail.try_alloc();
  if (m == nullptr) {
    LOGW(TAG, "publishWorker: workerToOrchMail alloc failed");
    return false;
  }

  *m = evt;
  const auto st = _workerToOrchMail.put(m);
  if (st != osOK) {
    LOGW(TAG, "publishWorker: workerToOrchMail put failed");
    _workerToOrchMail.free(m);
    return false;
  }

  return true;
}

bool EventBus::tryGetNext(DeviceEvent& outEvt, uint32_t timeoutMs)
{
  // Provide a unified view over underlying mailboxes.
  // UI is low priority; comms and worker events are handled first.

  const uint32_t startMs = millis();

  while ((millis() - startMs) < timeoutMs) {
    // Prefer comms events.
    CommsEventMsg* comms = _commsToOrchMail.try_get();
    if (comms != nullptr) {
      outEvt.type = DeviceEvent::Type::Comms;
      outEvt.data.comms = *comms;
      _commsToOrchMail.free(comms);
      return true;
    }

    // Then poll worker events.
    WorkerEventMsg* worker = _workerToOrchMail.try_get();
    if (worker != nullptr) {
      outEvt.type = DeviceEvent::Type::Worker;
      outEvt.data.worker = *worker;
      _workerToOrchMail.free(worker);
      return true;
    }

    // Then poll UI.
    UiEventMsg* ui = _uiToOrchMail.try_get();
    if (ui != nullptr) {
      outEvt.type = DeviceEvent::Type::Ui;
      outEvt.data.ui = *ui;
      _uiToOrchMail.free(ui);
      return true;
    }

    delay(1);
  }

  return false;
}
