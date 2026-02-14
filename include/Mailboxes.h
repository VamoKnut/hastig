#pragma once

#include <mbed.h>

#include "AppConfig.h"
#include "CommsCommands.h"
#include "Messages.h"

/**
 * @brief Centralized mailbox ownership.
 *
 * This keeps the "wiring" in one place and makes later migration to a bus-like
 * mechanism easier, while preserving current behavior.
 */
struct SystemMailboxes {
  rtos::Mail<SensorSampleMsg, QUEUE_DEPTH_SENSOR_TO_AGG> sensorToAggMail;
  rtos::Mail<SensorSampleMsg, QUEUE_DEPTH_ONE_SHOT>      oneShotMail;
  rtos::Mail<AggregateMsg, QUEUE_DEPTH_AGG_TO_COMMS>     aggToCommsMail;

  rtos::Mail<UiEventMsg, QUEUE_DEPTH_UI_TO_ORCH>        uiToOrchMail;
  rtos::Mail<CommsEventMsg, QUEUE_DEPTH_COMMS_TO_ORCH>  commsToOrchMail;
  rtos::Mail<WorkerEventMsg, QUEUE_DEPTH_WORKER_TO_ORCH> workerToOrchMail;
  rtos::Mail<OrchCommandMsg, QUEUE_DEPTH_ORCH_TO_COMMS> orchToCommsMail;
};
