#include "Sensor.h"

#include "AppConfig.h"
#include "Logger.h"
#include "ModbusRtuMaster.h"

#include <Arduino.h>
#include <math.h>
#include <string.h>
#include <stdint.h>

static const char* TAG = "SENSOR";

class FakeSensor : public Sensor
{
 public:
   const char* name() const override
   {
      return "fake";
   }

   bool begin(const AppSettings&) override
   {
      return true;
   }

   void end() override
   {
   }

   bool sample(SensorSampleMsg& out) override
   {
      strncpy(out.k0, "cond", sizeof(out.k0));
      out.k0[sizeof(out.k0) - 1] = '\0';
      out.v0                     = (float)random(50, 501);

      strncpy(out.k1, "temp", sizeof(out.k1));
      out.k1[sizeof(out.k1) - 1] = '\0';
      out.v1                     = 10.0f + ((float)random(0, 300) / 10.0f);

      out.ok = true;
      rtos::ThisThread::sleep_for(std::chrono::milliseconds(500));
      return true;
   }
};

class SeametricsCT2X : public Sensor
{
 public:
   SeametricsCT2X()
       : _modbus(PIN_RS485_TX, PIN_RS485_DE_RE, PIN_RS485_DE_RE)
   {
   }

   const char* name() const override
   {
      return "seametricsCT2X";
   }

   bool begin(const AppSettings& s) override
   {
      if (_started)
      {
         return true;
      }

      if (s.sensor_baud == 0)
      {
         LOGE(TAG, "Invalid sensor baud 0");
         return false;
      }
      if (s.sensor_addr == 0 || s.sensor_addr > 247)
      {
         LOGE(TAG, "Invalid Modbus slave id %u", (unsigned int)s.sensor_addr);
         return false;
      }

      if (!_modbus.begin(s.sensor_baud, kTimeoutMs))
      {
         LOGE(TAG, "ModbusRTUClient begin failed");
         return false;
      }

      _slaveId = s.sensor_addr;
      LOGI(TAG,
           "Seametrics Modbus FC03 tx=PA_9 rx=PA_10 de/re=PA_8 baud=%lu addr=%u start=0x%04X count=%u timeout=%lu",
           (unsigned long)s.sensor_baud,
           (unsigned int)_slaveId,
           (unsigned int)kTelemetryStartReg,
           (unsigned int)kTelemetryRegCount,
           (unsigned long)kTimeoutMs);
      _discardNextSample = true;
      _readErrorLogged = false;
      _started = true;
      return true;
   }

   void end() override
   {
      _modbus.end();
      _started = false;
      _discardNextSample = true;
      _readErrorLogged = false;
   }

   bool sample(SensorSampleMsg& out) override
   {
      if (!_started)
      {
         return false;
      }

      uint16_t regs[kTelemetryRegCount] = {};
      if (!_modbus.readHoldingRegisters(_slaveId,
                                        static_cast<uint16_t>(kTelemetryStartReg),
                                        kTelemetryRegCount,
                                        regs))
      {
         if (!_readErrorLogged)
         {
            const char* error = _modbus.lastError();
            LOGW(TAG, "Modbus FC03 failed addr=%u start=0x%04X count=%u: %s",
                 (unsigned int)_slaveId,
                 (unsigned int)kTelemetryStartReg,
                 (unsigned int)kTelemetryRegCount,
                 error != nullptr ? error : "unspecified Modbus error");
            _readErrorLogged = true;
         }
         return false;
      }
      _readErrorLogged = false;

      // First read after begin() is known invalid on this sensor.
      if (_discardNextSample)
      {
         _discardNextSample = false;
         return false;
      }

      const float temp = doubleWordToFloat(regs[0], regs[1]);
      const float cond = doubleWordToFloat(regs[2], regs[3]);

      strncpy(out.k0, "cond", sizeof(out.k0));
      out.k0[sizeof(out.k0) - 1] = '\0';
      out.v0                     = cond;

      strncpy(out.k1, "temp", sizeof(out.k1));
      out.k1[sizeof(out.k1) - 1] = '\0';
      out.v1                     = temp;

      out.ok = isfinite(out.v0) && isfinite(out.v1);
      return out.ok;
   }

 private:
   static constexpr int           kTelemetryStartReg = 62592;
   static constexpr int           kTelemetryRegCount = 4;
   static constexpr unsigned long kTimeoutMs         = 150;

   static float doubleWordToFloat(uint16_t highWord, uint16_t lowWord)
   {
      const uint32_t bits = ((uint32_t)highWord << 16) | (uint32_t)lowWord;
      float          out  = 0.0f;
      memcpy(&out, &bits, sizeof(out));
      return out;
   }

   ModbusRtuMaster _modbus;
   uint8_t _slaveId = 1;
   bool    _started = false;
   bool    _discardNextSample = true;
   bool    _readErrorLogged = false;
};

class PT12Sensor : public Sensor
{
 public:
   const char* name() const override
   {
      return "PT12";
   }

   bool begin(const AppSettings& s) override
   {
      (void)s;
      return true;
   }

   void end() override
   {
   }

   bool sample(SensorSampleMsg& out) override
   {
      strncpy(out.k0, "level", sizeof(out.k0));
      out.k0[sizeof(out.k0) - 1] = '\0';
      out.v0                     = 3.14f;

      strncpy(out.k1, "temp", sizeof(out.k1));
      out.k1[sizeof(out.k1) - 1] = '\0';
      out.v1                     = 3.14f;

      out.ok = true;
      return true;
   }
};

Sensor* Sensor::create(uint32_t sensorType)
{
   switch (sensorType)
   {
   case 0:
      return new FakeSensor();
   case 1:
      return new SeametricsCT2X();
   case 2:
      return new PT12Sensor();
   default:
      return new FakeSensor();
   }
}
