#include "SamplingThread.h"

#include "Logger.h"
#include "BoardHal.h"
#include "StopUtil.h"
#include <Arduino.h>
#include <chrono>
#include <string.h>
#include <cmsis_os2.h>

using namespace std::chrono;

static const char* TAG = "SENS";

SamplingThread::SamplingThread(SensorMail<QUEUE_DEPTH_SENSOR_TO_AGG>& outMail,
                               OneShotMail<QUEUE_DEPTH_ONE_SHOT>&     oneShotMail,
                               SettingsManager& settings, SessionClock& clock, EventBus& eventBus)
    : _outMail(outMail),
      _oneShotMail(oneShotMail),
      _settings(settings),
      _clock(clock),
      _eventBus(eventBus),
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

void SamplingThread::stop()
{
   stoputil::terminateThread("SamplingThread", _thread);
}

/**
 * @brief Enable/disable continuous sampling.
 */
void SamplingThread::setEnabled(bool en)
{
   _enabled.store(en);
   _flags.set(FLAG_WAKE);
}

/**
 * @brief Request a one-shot sample.
 */
void SamplingThread::requestOneShot()
{
   _flags.set(FLAG_ONE_SHOT);
}

void SamplingThread::threadEntry(void* ctx)
{
   static_cast<SamplingThread*>(ctx)->run();
}

void SamplingThread::run()
{
   const osThreadId_t tid        = osThreadGetId();
   const uint32_t     stackSize  = osThreadGetStackSize(tid);
   const uint32_t     stackSpace = osThreadGetStackSpace(tid);
   LOGI(TAG, "Thread started (stack size=%lu free=%lu)", (unsigned long)stackSize, (unsigned long)stackSpace);

   while (true)
   {
      _flags.wait_any(FLAG_WAKE | FLAG_ONE_SHOT);

      bool oneShot = (_flags.get() & FLAG_ONE_SHOT) != 0u;
      if (oneShot)
      {
         _flags.clear(FLAG_ONE_SHOT);
      }

      if (!_enabled.load() && !oneShot)
      {
         continue;
      }

      const AppSettings s = _settings.getCopy();
      BoardHal::setSensorPower(true);
      rtos::ThisThread::sleep_for(milliseconds(s.sensor_warmup_ms));

      if (_sensor != nullptr)
      {
         _sensor->end();
         delete _sensor;
         _sensor = nullptr;
      }

      LOGI(TAG, "Creating sensor type=%lu", (unsigned long)s.sensor_type);

      _sensor = Sensor::create(s.sensor_type);
      if (_sensor == nullptr)
      {
         LOGE(TAG, "Sensor create failed");
         BoardHal::setSensorPower(false);
         continue;
      }

      if (!_sensor->begin(s))
      {
         LOGE(TAG, "Sensor begin failed (%s)", _sensor->name());
         _sensor->end();
         delete _sensor;
         _sensor = nullptr;
         BoardHal::setSensorPower(false);
         continue;
      }

      uint32_t periodMs = s.sample_period_ms;
      if (periodMs < MIN_SAMPLE_PERIOD_MS)
      {
         periodMs = MIN_SAMPLE_PERIOD_MS;
      }

      while (_enabled.load() || oneShot)
      {
         SensorSampleMsg tmp;
         memset(&tmp, 0, sizeof(tmp));
         tmp.relMs     = _clock.relMs();
         const bool ok = _sensor->sample(tmp);
         tmp.ok        = ok;
         if (tmp.ok)
         {
            SensorSampleMsg* m = oneShot ? _oneShotMail.try_alloc() : _outMail.try_alloc();
            if (m != nullptr)
            {
               memcpy(m, &tmp, sizeof(*m));
               if (oneShot)
               {
                  _oneShotMail.put(m);
               }
               else
               {
                  _outMail.put(m);
               }
               LOGD(TAG, "Produced sample t=%lu %s=%.2f %s=%.2f ok=%d", (unsigned long)m->relMs, m->k0,
                    (double)m->v0, m->k1, (double)m->v1, m->ok ? 1 : 0);

               WorkerEventMsg w;
               memset(&w, 0, sizeof(w));
               w.type  = WorkerEventType::SampleTaken;
               w.ts_ms = millis();
               w.relMs = tmp.relMs;
               w.n     = 1;
               w.ok    = ok;
               _eventBus.publishWorker(w);
            }
            else
            {
               LOGW(TAG, "Drop sample: mail full");
            }
         }
         else
         {
            LOGW(TAG, "Get sample failed");
         }

         // Always yield here so main loop and comms get scheduling opportunities.
         rtos::ThisThread::sleep_for(milliseconds(1));

         if (oneShot)
         {
            break;
         }

         rtos::ThisThread::sleep_for(milliseconds(periodMs));
      }

      if (_sensor != nullptr)
      {
         _sensor->end();
         delete _sensor;
         _sensor = nullptr;
      }

      BoardHal::setSensorPower(false);
   }
}
