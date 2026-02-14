#pragma once

#include "CommandBus.h"
#include "CommsEgress.h"
#include "CommsInbox.h"
#include "CommsPump.h"
#include "EventBus.h"
#include "AggregatorThread.h"
#include "Mailboxes.h"
#include "Orchestrator.h"
#include "PowerManager.h"
#include "RestartReason.h"
#include "SamplingThread.h"
#include "SessionClock.h"
#include "SettingsManager.h"
#include "UiThread.h"

/**
 * @brief Centralized ownership of core runtime objects.
 *
 * This is a wiring/structure helper only. It intentionally contains no
 * application logic.
 */
struct SystemContext {
  // Mailboxes
  SystemMailboxes mailboxes;

  // Core services
  SettingsManager settings;
  SessionClock sessionClock;

  // UI + event ingress
  EventBus eventBus;

  // Comms egress façade
  CommandBus commandBus;
  CommsEgress commsEgress;

  // Threads / components
  UiThread uiThread;
  SamplingThread samplingThread;
  AggregatorThread aggThread;

  // Comms
  CommsInbox commsInbox;
  CommsPump commsPump;

  // Power + orchestration
  PowerManager powerManager;
  Orchestrator orchestrator;

  SystemContext(Board& board, RestartReasonStore& rrStore, uint8_t wakePin)
      : settings(),
        sessionClock(),
        eventBus(mailboxes.uiToOrchMail, mailboxes.commsToOrchMail, mailboxes.workerToOrchMail),
        commandBus(mailboxes.orchToCommsMail),
        commsEgress(commandBus, mailboxes.aggToCommsMail),
        uiThread(eventBus),
        samplingThread(mailboxes.sensorToAggMail, mailboxes.oneShotMail, settings, sessionClock,
                       eventBus),
        aggThread(mailboxes.sensorToAggMail, commsEgress, settings, sessionClock,
                  eventBus),
        commsInbox(mailboxes.aggToCommsMail, mailboxes.orchToCommsMail),
        commsPump(commsInbox, mailboxes.oneShotMail, eventBus, settings, sessionClock),
        powerManager(board, rrStore, commsPump, uiThread, aggThread, samplingThread,
                     wakePin),
        orchestrator(eventBus, commsEgress, settings, sessionClock, samplingThread, aggThread,
                     powerManager)
  {
  }
};
