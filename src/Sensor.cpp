#include "Sensor.h"

#include "AppConfig.h"
#include "Logger.h"

#include <Arduino.h>
#include <ArduinoModbus.h>
#include <ArduinoRS485.h>
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

      RS485.setPins(PIN_RS485_TX, PIN_RS485_DE_RE, PIN_RS485_DE_RE);
      const int frameDelayUs = calcFrameDelayUs(s.sensor_baud);
      RS485.setDelays(frameDelayUs, frameDelayUs);

      if (!ModbusRTUClient.begin(s.sensor_baud, SERIAL_8N1))
      {
         LOGE(TAG, "ModbusRTUClient begin failed");
         return false;
      }

      ModbusRTUClient.setTimeout(kTimeoutMs);
      _slaveId = s.sensor_addr;
      LOGI(TAG,
           "Seametrics modbus uart tx=D%d rx=D%d de/re=%d baud=%lu addr=%u timeout=%lu",
           (int)PIN_RS485_TX,
           (int)PIN_RS485_RX,
           (int)PIN_RS485_DE_RE,
           (unsigned long)s.sensor_baud,
           (unsigned int)_slaveId,
           (unsigned long)kTimeoutMs);
      _discardNextSample = true;
      _started = true;
      return true;
   }

   void end() override
   {
      ModbusRTUClient.end();
      _started = false;
      _discardNextSample = true;
   }

   bool sample(SensorSampleMsg& out) override
   {
      if (!_started)
      {
         return false;
      }

      uint16_t regs[kTelemetryRegCount] = {};
      const int requested =
          ModbusRTUClient.requestFrom((int)_slaveId, HOLDING_REGISTERS, kTelemetryStartReg, kTelemetryRegCount);
      if (requested != kTelemetryRegCount)
      {
         return false;
      }

      for (int i = 0; i < kTelemetryRegCount; i++)
      {
         if (ModbusRTUClient.available() <= 0)
         {
            return false;
         }
         const long v = ModbusRTUClient.read();
         if (v < 0 || v > 0xFFFF)
         {
            return false;
         }
         regs[i] = (uint16_t)v;
      }

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

   static int calcFrameDelayUs(unsigned long baud)
   {
      if (baud == 0)
      {
         return 0;
      }
      return (int)((35UL * 1000000UL + (baud - 1UL)) / baud);
   }

   static float doubleWordToFloat(uint16_t highWord, uint16_t lowWord)
   {
      const uint32_t bits = ((uint32_t)highWord << 16) | (uint32_t)lowWord;
      float          out  = 0.0f;
      memcpy(&out, &bits, sizeof(out));
      return out;
   }

   uint8_t _slaveId = 1;
   bool    _started = false;
   bool    _discardNextSample = true;
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
