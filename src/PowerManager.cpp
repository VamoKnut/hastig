#include "PowerManager.h"

#include <Arduino.h>
#include <mbed.h>
#include <chrono>
using namespace std::chrono;

#include <platform/ScopedLock.h>

#include "AppConfig.h"
#include "Logger.h"
#include "PowerUtil.h"
#include "WatchdogService.h"

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
                           PinName wakePin)
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
  mbed::ScopedLock<rtos::Mutex> lock(_requestMutex);
  if (_inProgress || _pending) {
    return;
  }
  _req.reasonCode        = reasonCode;
  _req.expectedDurationS = clampSleepS(expectedDurationS);
  _pending = true;
}

void PowerManager::deferSleep(const SleepRequest& request)
{
  mbed::ScopedLock<rtos::Mutex> lock(_requestMutex);
  _req = request;
  _pending = true;
  _inProgress = false;
}

bool PowerManager::service()
{
  SleepRequest request;
  {
    mbed::ScopedLock<rtos::Mutex> lock(_requestMutex);
    if (!_pending) {
      return false;
    }
    _pending = false;
    _inProgress = true;
    request = _req;
  }

  LOGI(TAG, "Sleep requested: code=%lu duration=%lu s",
       (unsigned long)request.reasonCode,
       (unsigned long)request.expectedDurationS);

  if (!_commsPrepared) {
    // Give queued hibernation status a chance to publish before preventing
    // new connections and tearing down the current MQTT/TCP session.
    const uint32_t graceStart = millis();
    while ((uint32_t)(millis() - graceStart) < HIBERNATE_STATUS_GRACE_MS) {
      _comms.loopOnce();
      WatchdogService::kick();
      rtos::ThisThread::sleep_for(milliseconds(20));
    }

    _comms.prepareHibernate();
    _commsPrepared = true;
  }

  // 2) Stop producers first.
  WatchdogService::kick();
  LOGI(TAG, "Sleep step: disable producers");
  _sampling.setEnabled(false);
  _agg.setEnabled(false);

  // 3) Stop policy before workers, then wait for sensor cleanup before
  // allowing aggregation to discard any remaining samples.
  WatchdogService::kick();
  LOGI(TAG, "Sleep step: stop threads");
  if (_orch != nullptr && !_orch->stop()) {
    LOGE(TAG, "Orchestrator did not stop; deferring hibernate");
    deferSleep(request);
    return false;
  }
  if (!_sampling.stop()) {
    LOGE(TAG, "Sampling worker did not stop; deferring hibernate");
    deferSleep(request);
    return false;
  }
  if (!_agg.stop()) {
    LOGE(TAG, "Aggregation worker did not stop; deferring hibernate");
    deferSleep(request);
    return false;
  }
  if (!_ui.stop()) {
    LOGE(TAG, "UI worker did not stop; deferring hibernate");
    deferSleep(request);
    return false;
  }

  // 4) Shutdown comms without modem full end (avoid blocking).
  WatchdogService::kick();
  LOGI(TAG, "Sleep step: shutdown comms");
  _comms.shutdownForHibernate();
  LOGI(TAG, "Sleep step: comms shutdown returned");

  // 5) Persist restart reason.
  WatchdogService::kick();
  LOGI(TAG, "Sleep step: write restart reason");
  _restartReason.write(request.reasonCode);

  // 6) Enter hibernate.
  WatchdogService::prepareForHibernate();
  LOGI(TAG, "Sleep step: entering hibernate");
  Serial.flush();
  powerutil::hibernate(_board, _wakePin, request.expectedDurationS);

  LOGW(TAG, "Returned from hibernate (unexpected)");
  _commsPrepared = false;
  {
    mbed::ScopedLock<rtos::Mutex> lock(_requestMutex);
    _inProgress = false;
  }
  return true;
}
