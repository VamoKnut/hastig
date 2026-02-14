#pragma once

#include <stdint.h>
#include <atomic>

#include "RestartReason.h"

class CommsPump;
class UiThread;
class Orchestrator;
class AggregatorThread;
class SamplingThread;
class Board;
class RestartReasonStore;

/**
 * @brief Executes the "sleep transaction" from the Arduino loop context.
 *
 * Orchestrator requests sleep via requestSleep(). The loop calls service()
 * to perform: comms grace -> thread shutdown -> comms teardown -> enter hibernate.
 */
class PowerManager {
public:
  struct SleepRequest {
    RestartReasonCode reasonCode = RestartReasonCode::UnexpectedReboot;
    uint32_t expectedDurationS   = 0;
  };

  PowerManager(Board& board,
               RestartReasonStore& restartReason,
               CommsPump& comms,
               UiThread& ui,
               AggregatorThread& agg,
               SamplingThread& sampling,
               uint8_t wakePin);

  void setOrchestrator(Orchestrator& orch);

  void requestSleep(RestartReasonCode reasonCode, uint32_t expectedDurationS);

  /**
   * @brief Called frequently from loop(). Performs the sleep transaction if requested.
   * @return true if a sleep request was handled (should not return if hibernate succeeds).
   */
  bool service();

private:
  static constexpr uint32_t kMinSleepS = 5;
  static constexpr uint32_t kMaxSleepS = 43200; // 12h safety cap

  uint32_t clampSleepS(uint32_t s) const;

  Board& _board;
  RestartReasonStore& _restartReason;
  CommsPump& _comms;
  UiThread& _ui;
  Orchestrator* _orch = nullptr;
  AggregatorThread& _agg;
  SamplingThread& _sampling;

  const uint8_t _wakePin;

  std::atomic_bool _pending{false};
  std::atomic_bool _inProgress{false};
  SleepRequest _req{};
};
