#include "AggregatorThread.h"

#include "Logger.h"
#include <Arduino.h>
#include <chrono>
#include <string.h>

using namespace std::chrono;

static const char* TAG = "AGG";

static void reset_stats(float& mn, float& mx, float& sum)
{
   mn  = 1e30f;
   mx  = -1e30f;
   sum = 0.0f;
}

void AggregateAccumulator::reset(uint32_t startMs)
{
   _t0 = startMs;
   _t1 = startMs;
   _n  = 0;
   _ok = true;

   _k0[0] = '\0';
   _k1[0] = '\0';

   reset_stats(_v0_min, _v0_max, _v0_sum);
   reset_stats(_v1_min, _v1_max, _v1_sum);
}

void AggregateAccumulator::add(const SensorSampleMsg& s)
{
   if (_n == 0)
   {
      strncpy(_k0, s.k0, sizeof(_k0));
      strncpy(_k1, s.k1, sizeof(_k1));
      _k0[sizeof(_k0) - 1] = '\0';
      _k1[sizeof(_k1) - 1] = '\0';
      _t0                  = s.relMs;
   }

   _t1 = s.relMs;

   _v0_sum += s.v0;
   if (s.v0 < _v0_min)
      _v0_min = s.v0;
   if (s.v0 > _v0_max)
      _v0_max = s.v0;

   if (_k1[0] != '\0')
   {
      _v1_sum += s.v1;
      if (s.v1 < _v1_min)
         _v1_min = s.v1;
      if (s.v1 > _v1_max)
         _v1_max = s.v1;
   }

   _ok = _ok && s.ok;
   _n++;
}

bool AggregateAccumulator::emit(AggregateMsg& out) const
{
   if (_n == 0)
   {
      return false;
   }

   memset(&out, 0, sizeof(out));
   out.rel_start_ms = _t0;
   out.rel_end_ms   = _t1;

   strncpy(out.k0, _k0, sizeof(out.k0));
   strncpy(out.k1, _k1, sizeof(out.k1));
   out.k0[sizeof(out.k0) - 1] = '\0';
   out.k1[sizeof(out.k1) - 1] = '\0';

   out.n  = _n;
   out.ok = _ok;

   out.v0_avg = _v0_sum / (float)_n;
   out.v0_min = _v0_min;
   out.v0_max = _v0_max;

   if (out.k1[0] != '\0')
   {
      out.v1_avg = _v1_sum / (float)_n;
      out.v1_min = _v1_min;
      out.v1_max = _v1_max;
   }

   return true;
}

AggregatorThread::AggregatorThread(AggInMail<QUEUE_DEPTH_SENSOR_TO_AGG>& inMail,
                                   CommsEgress& commsEgress, SettingsManager& settings,
                                   SessionClock& clock, EventBus& eventBus)
    : _inMail(inMail), _commsEgress(commsEgress), _settings(settings), _clock(clock),
      _eventBus(eventBus)
{
}

void AggregatorThread::start()
{
   _thread.start(mbed::callback(AggregatorThread::threadEntry, this));
}

bool AggregatorThread::stop(uint32_t timeoutMs)
{
   _stopRequested.store(true);
   _enabled.store(false);
   _flags.set(FLAG_WAKE);
   return (_completion.wait_any(FLAG_STOPPED, timeoutMs, false) & FLAG_STOPPED) != 0u;
}

void AggregatorThread::setEnabled(bool en)
{
   _enabled.store(en);
   _flags.set(FLAG_WAKE);
}

void AggregatorThread::threadEntry(void* ctx)
{
   auto* self = static_cast<AggregatorThread*>(ctx);
   self->run();
   self->_completion.set(FLAG_STOPPED);
}

void AggregatorThread::run()
{
   LOGI(TAG, "Thread started");

   while (!_stopRequested.load())
   {
      if (!_enabled.load())
      {
         _flags.wait_any(FLAG_WAKE);
         _flags.clear(FLAG_WAKE);
         if (_stopRequested.load())
         {
            break;
         }
         continue;
      }

      SensorSampleMsg* staleSample = nullptr;
      while ((staleSample = _inMail.try_get()) != nullptr)
      {
         _inMail.free(staleSample);
      }

      if (!_enabled.load())
      {
         continue;
      }

      const AppSettings s        = _settings.getCopy();
      const uint32_t    windowMs = s.agg_period_s * 1000u;

      AggregateAccumulator acc;
      acc.reset(_clock.relMs());

      const uint32_t startWall = millis();

      while (_enabled.load() && !_stopRequested.load())
      {
         SensorSampleMsg* sm = _inMail.try_get_for(milliseconds(50));
         if (sm != nullptr)
         {
            acc.add(*sm);
            _inMail.free(sm);
            LOGD(TAG, "Consumed sample");
         }

         if ((uint32_t)(millis() - startWall) >= windowMs)
         {
            break;
         }
      }

      if (_stopRequested.load())
      {
         break;
      }
      if (!_enabled.load())
      {
         while ((staleSample = _inMail.try_get()) != nullptr)
         {
            _inMail.free(staleSample);
         }
         continue;
      }

      AggregateMsg out;
      if (!acc.emit(out))
      {
         continue;
      }
      out.sessionId[0] = '\0';
      (void)_clock.getServerSessionId(out.sessionId, sizeof(out.sessionId));

      if (!_commsEgress.sendAggregate(out))
      {
         LOGW(TAG, "Drop aggregate: comms egress full");
         continue;
      }

      LOGI(TAG, "Produced aggregate %s/%s n=%lu", out.k0, out.k1, (unsigned long)out.n);

      WorkerEventMsg w;
      memset(&w, 0, sizeof(w));
      w.type  = WorkerEventType::AggregateReady;
      w.ts_ms = millis();
      w.relMs = out.rel_end_ms;
      w.n     = out.n;
      w.ok    = out.ok;
      _eventBus.publishWorker(w);
   }

   SensorSampleMsg* sample = nullptr;
   while ((sample = _inMail.try_get()) != nullptr)
   {
      _inMail.free(sample);
   }
}
