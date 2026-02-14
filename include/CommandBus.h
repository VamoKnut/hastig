#pragma once

#include <mbed.h>

#include "AppConfig.h"
#include "CommsCommands.h"

/**
 * @brief Small façade over RTOS mailboxes used for outbound commands.
 *
 * This is an incremental step toward a more bus-like internal architecture.
 * Today it simply wraps the orchestrator->comms mailbox.
 */
class CommandBus {
public:
  explicit CommandBus(rtos::Mail<OrchCommandMsg, QUEUE_DEPTH_ORCH_TO_COMMS>& orchToCommsMail)
      : _orchToCommsMail(orchToCommsMail) {
  }

  bool sendToComms(OrchCommandType type, const char* payloadOrNull);

private:
  rtos::Mail<OrchCommandMsg, QUEUE_DEPTH_ORCH_TO_COMMS>& _orchToCommsMail;
};
