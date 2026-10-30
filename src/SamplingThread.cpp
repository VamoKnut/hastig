#include "SamplingThread.h"

#include "Logger.h"
#include "BoardHal.h"
#include <Arduino.h>
#include <chrono>
#include <string.h>
#include <cmsis_os2.h>

using namespace std::chrono;

static const char* TAG = "SENS";

namespace
{
AppSettings readSettingsSnapshot(SettingsManager& settings, uint32_t& revision)
{
   AppSettings snapshot;
   do
   {
      revision = settings.revision();
      snapshot = settings.getCopy();
   } while (revision != settings.revision());
   return snapshot;
}

bool sensorConfigurationChanged(const AppSettings& current, const AppSettings& updated)
{
   return current.sensor_type != updated.sensor_type ||
          current.sensor_addr != updated.sensor_addr ||
          current.sensor_baud != updated.sensor_baud ||
          current.sensor_warmup_ms != updated.sensor_warmup_ms;
}

uint32_t waitSlice(uint32_t remainingMs)
{
   return remainingMs < 200u ? remainingMs : 200u;
}
} // namespace

SamplingThread::SamplingThread(SensorMail<QUEUE_DEPTH_SENSOR_TO_AGG>& outMail,
                               SettingsManager& settings, SessionClock& clock, EventBus& eventBus,
                               RuntimeStatus& runtimeStatus)
    : _outMail(outMail),
      _settings(settings),
      _clock(clock),
      _eventBus(eventBus),
      _runtimeStatus(runtimeStatus),
      _thread(PRIO_SENS, STACK_SENS, nullptr, "SENS")
{
}

/**
 * @brief Start the sensor thread.
 */
void SamplingThread::start()
{
   _thread.start(mbed::callback(threadEntry, this));
   _thread.set_priority(PRIO_SENS);
}

bool SamplingThread::stop(uint32_t timeoutMs)
{
   _stopRequested.store(true);
   _enabled.store(false);
   _flags.set(FLAG_WAKE);
   return (_completion.wait_any(FLAG_STOPPED, timeoutMs, false) & FLAG_STOPPED) != 0u;
}

/**
 * @brief Enable/disable continuous sampling.
 */
void SamplingThread::setEnabled(bool en)
{
   _enabled.store(en);
   _flags.set(FLAG_WAKE);
}

void SamplingThread::threadEntry(void* ctx)
{
   auto* self = static_cast<SamplingThread*>(ctx);
   self->run();
   self->_completion.set(FLAG_STOPPED);
}

void SamplingThread::run()
{
   const osThreadId_t tid        = osThreadGetId();
   const uint32_t     stackSize  = osThreadGetStackSize(tid);
   const uint32_t     stackSpace = osThreadGetStackSpace(tid);
   LOGI(TAG, "Thread started (stack size=%lu free=%lu)", (unsigned long)stackSize, (unsigned long)stackSpace);

   while (!_stopRequested.load())
   {
      if (!_enabled.load())
      {
         _flags.wait_any(FLAG_WAKE);
         _flags.clear(FLAG_WAKE);
         continue;
      }

      uint32_t settingsRevision = 0;
      AppSettings settings = readSettingsSnapshot(_settings, settingsRevision);
      BoardHal::setSensorPower(true);

      bool restartForSettings = false;
      const uint32_t warmupStart = millis();
      while (_enabled.load() && !_stopRequested.load() &&
             (uint32_t)(millis() - warmupStart) < settings.sensor_warmup_ms)
      {
         const uint32_t elapsed = millis() - warmupStart;
         (void)_flags.wait_any(FLAG_WAKE, waitSlice(settings.sensor_warmup_ms - elapsed));
         _flags.clear(FLAG_WAKE);

         if (_settings.revision() != settingsRevision)
         {
            uint32_t updatedRevision = 0;
            const AppSettings updated = readSettingsSnapshot(_settings, updatedRevision);
            restartForSettings = sensorConfigurationChanged(settings, updated);
            settings = updated;
            settingsRevision = updatedRevision;
            if (restartForSettings)
            {
               break;
            }
         }
      }

      if (_stopRequested.load() || !_enabled.load())
      {
         BoardHal::setSensorPower(false);
         continue;
      }
      if (restartForSettings)
      {
         BoardHal::setSensorPower(false);
         continue;
      }

      if (_sensor != nullptr)
      {
         _sensor->end();
         delete _sensor;
         _sensor = nullptr;
      }

      LOGI(TAG, "Creating sensor type=%lu", (unsigned long)settings.sensor_type);

      _sensor = Sensor::create(settings.sensor_type);
      if (_sensor == nullptr)
      {
         LOGE(TAG, "Sensor create failed");
         BoardHal::setSensorPower(false);
         (void)_flags.wait_any(FLAG_WAKE, 1000u);
         _flags.clear(FLAG_WAKE);
         continue;
      }

      if (!_sensor->begin(settings))
      {
         LOGE(TAG, "Sensor begin failed (%s)", _sensor->name());
         _sensor->end();
         delete _sensor;
         _sensor = nullptr;
         BoardHal::setSensorPower(false);
         (void)_flags.wait_any(FLAG_WAKE, 1000u);
         _flags.clear(FLAG_WAKE);
         continue;
      }

      restartForSettings = false;
      while (_enabled.load() && !_stopRequested.load())
      {
         SensorSampleMsg tmp;
         memset(&tmp, 0, sizeof(tmp));
         tmp.relMs     = _clock.relMs();
         const bool ok = _sensor->sample(tmp);
         tmp.ok        = ok;

         if (_settings.revision() != settingsRevision)
         {
            uint32_t updatedRevision = 0;
            const AppSettings updated = readSettingsSnapshot(_settings, updatedRevision);
            restartForSettings = sensorConfigurationChanged(settings, updated);
            settings = updated;
            settingsRevision = updatedRevision;
         }

         if (restartForSettings)
         {
            break;
         }

         if (_enabled.load() && !_stopRequested.load() && tmp.ok)
         {
            _runtimeStatus.setLastSample(tmp);
            SensorSampleMsg* m = _outMail.try_alloc();
            if (m != nullptr)
            {
               memcpy(m, &tmp, sizeof(*m));
               _outMail.put(m);
               LOGD(TAG, "Produced sample t=%lu %s=%.2f %s=%.2f ok=%d", (unsigned long)m->relMs, m->k0,
                    (double)m->v0, m->k1, (double)m->v1, m->ok ? 1 : 0);
            }
            else
            {
               LOGW(TAG, "Drop sample: mail full");
            }
         }
         else if (_enabled.load() && !_stopRequested.load())
         {
            LOGW(TAG, "Get sample failed");
         }

         const uint32_t waitStart = millis();
         while (_enabled.load() && !_stopRequested.load())
         {
            const uint32_t elapsed = millis() - waitStart;
            if (elapsed >= settings.sample_period_ms)
            {
               break;
            }

            (void)_flags.wait_any(FLAG_WAKE, waitSlice(settings.sample_period_ms - elapsed));
            _flags.clear(FLAG_WAKE);

            if (_settings.revision() != settingsRevision)
            {
               uint32_t updatedRevision = 0;
               const AppSettings updated = readSettingsSnapshot(_settings, updatedRevision);
               restartForSettings = sensorConfigurationChanged(settings, updated);
               settings = updated;
               settingsRevision = updatedRevision;
               if (restartForSettings)
               {
                  break;
               }
            }
         }

         if (restartForSettings)
         {
            break;
         }
      }

      if (_sensor != nullptr)
      {
         _sensor->end();
         delete _sensor;
         _sensor = nullptr;
      }

      BoardHal::setSensorPower(false);
   }

   if (_sensor != nullptr)
   {
      _sensor->end();
      delete _sensor;
      _sensor = nullptr;
   }
   BoardHal::setSensorPower(false);
}
