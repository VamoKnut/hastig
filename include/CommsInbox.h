#pragma once

#include <mbed.h>

#include "AppConfig.h"
#include "CommsCommands.h"
#include "Messages.h"

/**
 * @brief Lightweight facade for CommsPump inbound mail.
 *
 * This keeps CommsPump from directly owning multiple mailbox references and is
 * a stepping stone towards a more bus-like internal architecture.
 */
class CommsInbox {
public:
  // Keep these types explicit and toolchain-friendly. In this Arduino+mbed
  // build we do not have template aliases like AggMail<> available.
  using AggMailT = rtos::Mail<AggregateMsg, QUEUE_DEPTH_AGG_TO_COMMS>;
  using OrchToCommsMailT = rtos::Mail<OrchCommandMsg, QUEUE_DEPTH_ORCH_TO_COMMS>;

  CommsInbox(AggMailT& aggToCommsMail, OrchToCommsMailT& orchToCommsMail);

  OrchCommandMsg* tryGetOrch();
  void            freeOrch(OrchCommandMsg* msg);

  AggregateMsg* tryGetAggregate();
  void          freeAggregate(AggregateMsg* msg);

private:
  AggMailT&         _aggToCommsMail;
  OrchToCommsMailT& _orchToCommsMail;
};
