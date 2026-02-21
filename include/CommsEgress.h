#pragma once

#include <mbed.h>

#include "AppConfig.h"
#include "BoardHal.h"
#include "Messages.h"
class CommandBus;

/**
 * @brief Single façade for all egress toward the comms subsystem.
 *
 * This is an incremental refactor step to reduce direct mailbox dependencies
 * outside the comms layer, while keeping the underlying mailboxes intact.
 */
class CommsEgress {
public:
  using AggMailT = rtos::Mail<AggregateMsg, QUEUE_DEPTH_AGG_TO_COMMS>;
  CommsEgress(CommandBus& commandBus, AggMailT& aggToCommsMail);

  bool sendAggregate(const AggregateMsg& msg);

  // Higher-level helpers: keep callers decoupled from OrchCommandType.
  bool publishAwake();
  bool publishAwakeJson(const char* json);
  bool publishModeChange(const char* mode, const char* previousMode);
  bool publishStatus(const BoardHal::BatterySnapshot& bs, const char* mode);
  bool publishLowBatteryAlert(const BoardHal::BatterySnapshot& bs, const char* mode);
  bool publishConfig();
  bool applySettingsJson(const char* json);

  // Hibernate-related helpers.
  bool publishHibernating(const char* reasonStr, uint32_t expectedDurationS);
  bool publishHibernatingJson(const char* json);
  bool publishHibernateModeChange(const char* previousMode, const char* reasonStr, uint32_t expectedDurationS);

private:
  CommandBus& _commandBus;
  AggMailT&   _aggToCommsMail;
};
