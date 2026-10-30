#pragma once

#include <mbed.h>

#include "AppConfig.h"
#include "CommsCommands.h"
#include "Messages.h"

/**
 * @brief Centralized mailbox ownership.
 *
 * This keeps mailbox ownership in one place and limits shared queues to
 * producer/consumer boundaries.
 */
struct SystemMailboxes {
  rtos::Mail<SensorSampleMsg, QUEUE_DEPTH_SENSOR_TO_AGG> sensorToAggMail;
  rtos::Mail<AggregateMsg, QUEUE_DEPTH_AGG_TO_COMMS>     aggToCommsMail;

  rtos::Mail<DeviceEvent, QUEUE_DEPTH_ORCH_EVENTS>      eventsToOrchMail;
  rtos::Mail<OrchCommandMsg, QUEUE_DEPTH_ORCH_TO_COMMS> orchToCommsMail;
};
