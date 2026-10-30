#include "EventBus.h"

#include "Logger.h"

#include <chrono>
using namespace std::chrono;

static const char* TAG = "EVTB";

EventBus::EventBus(Mail& eventsToOrchMail)
  : _eventsToOrchMail(eventsToOrchMail)
{
}

bool EventBus::publishEvent(const DeviceEvent& event)
{
  DeviceEvent* m = _eventsToOrchMail.try_alloc();
  if (m == nullptr) {
    LOGW(TAG, "publish: event mailbox full");
    return false;
  }

  *m = event;
  const auto st = _eventsToOrchMail.put(m);
  if (st != osOK) {
    LOGW(TAG, "publish: event mailbox put failed");
    _eventsToOrchMail.free(m);
    return false;
  }

  return true;
}

bool EventBus::publish(const CommsEventMsg& evt)
{
  DeviceEvent event{};
  event.type = DeviceEvent::Type::Comms;
  event.data.comms = evt;
  return publishEvent(event);
}

bool EventBus::publish(const UiEventMsg& evt)
{
  DeviceEvent event{};
  event.type = DeviceEvent::Type::Ui;
  event.data.ui = evt;
  return publishEvent(event);
}

bool EventBus::tryGetNext(DeviceEvent& outEvt, uint32_t timeoutMs)
{
  DeviceEvent* event = _eventsToOrchMail.try_get_for(milliseconds(timeoutMs));
  if (event == nullptr) {
    return false;
  }

  outEvt = *event;
  _eventsToOrchMail.free(event);
  return true;
}
