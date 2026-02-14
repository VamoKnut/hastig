#include "CommandBus.h"

#include <cstring>

#include "Logger.h"
#include "TimeUtil.h"

static const char* TAG = "CMDBUS";

bool CommandBus::sendToComms(OrchCommandType type, const char* payloadOrNull) {
  OrchCommandMsg* msg = _orchToCommsMail.try_alloc();
  if (msg == nullptr) {
    LOGW(TAG, "sendToComms: alloc failed");
    return false;
  }

  msg->type = type;
  msg->ts_ms = timeutil::nowMs();
  if (payloadOrNull != nullptr) {
    strncpy(msg->payload, payloadOrNull, sizeof(msg->payload) - 1);
    msg->payload[sizeof(msg->payload) - 1] = '\0';
  } else {
    msg->payload[0] = '\0';
  }

  _orchToCommsMail.put(msg);
  return true;
}
