#include "PowerManager.h"

#include <Arduino.h>
#include <mbed.h>
#include <chrono>
using namespace std::chrono;

#include "AppConfig.h"
#include "Logger.h"
#include "PowerUtil.h"

#include "CommsPump.h"
#include "UiThread.h"
#include "Orchestrator.h"
#include "AggregatorThread.h"
#include "SamplingThread.h"

static const char* TAG = "PWRM";

PowerManager::PowerManager(Board& board,
                           RestartReasonStore& restartReason,
                           CommsPump& comms,
                           UiThread& ui,
                           AggregatorThread& agg,
                           SamplingThread& sampling,
                           uint8_t wakePin)
    : _board(board),
      _restartReason(restartReason),
      _comms(comms),
      _ui(ui),
      _agg(agg),
      _sampling(sampling),
      _wakePin(wakePin)
{
}

void PowerManager::setOrchestrator(Orchestrator& orch)
{
  _orch = &orch;
}

uint32_t PowerManager::clampSleepS(uint32_t s) const
{
  if (s < kMinSleepS) {
    return kMinSleepS;
  }
  if (s > kMaxSleepS) {
    return kMaxSleepS;
  }
  return s;
}

void PowerManager::requestSleep(RestartReasonCode reasonCode, uint32_t expectedDurationS)
{
  if (_inProgress.load() || _pending.load()) {
    return;
  }
  _req.reasonCode        = reasonCode;
  _req.expectedDurationS = clampSleepS(expectedDurationS);
  _pending.store(true);
}

bool PowerManager::service()
{
  if (!_pending.exchange(false)) {
    return false;
  }

  _inProgress.store(true);

  LOGI(TAG, "Sleep requested: code=%lu duration=%lu s",
       (unsigned long)_req.reasonCode,
       (unsigned long)_req.expectedDurationS);

  // Prevent new TCP/MQTT connect attempts during the grace window.
  _comms.prepareHibernate();

  // 1) Grace window: keep pumping comms so "hibernating" status/config replies can flush out.
  const uint32_t graceStart = millis();
  while ((uint32_t)(millis() - graceStart) < HIBERNATE_STATUS_GRACE_MS) {
    _comms.loopOnce();
    rtos::ThisThread::sleep_for(milliseconds(20));
  }

  // 2) Stop producers first.
  LOGI(TAG, "Sleep step: disable producers");
  _sampling.setEnabled(false);
  _agg.setEnabled(false);

  // 3) Terminate threads (best-effort, order matters).
  LOGI(TAG, "Sleep step: stop threads");
  _ui.stop();
  if (_orch != nullptr) {
    _orch->stop();
  }
  _agg.stop();
  _sampling.stop();

  // 4) Shutdown comms without modem full end (avoid blocking).
  LOGI(TAG, "Sleep step: shutdown comms");
  _comms.shutdownForHibernate();
  LOGI(TAG, "Sleep step: comms shutdown returned");

  // 5) Persist restart reason.
  LOGI(TAG, "Sleep step: write restart reason");
  _restartReason.write(_req.reasonCode);

  // 6) Enter hibernate.
  LOGI(TAG, "Sleep step: entering hibernate");
  Serial.flush();
  powerutil::hibernate(_board, _wakePin, _req.expectedDurationS);

  LOGW(TAG, "Returned from hibernate (unexpected)");
  _inProgress.store(false);
  return true;
}
