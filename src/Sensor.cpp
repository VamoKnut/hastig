#include "Sensor.h"

#include <Arduino.h>
#include <ArduinoModbus.h>
#include <math.h>
#include <string.h>

class FakeSensor : public Sensor {
public:
  const char* name() const override { return "fake"; }

  bool begin(const AppSettings&) override { return true; }

  void end() override {}

  bool sample(SensorSampleMsg& out) override
  {
    strncpy(out.k0, "cond", sizeof(out.k0));
    out.k0[sizeof(out.k0) - 1] = '\0';
    out.v0 = (float)random(50, 501);

    strncpy(out.k1, "temp", sizeof(out.k1));
    out.k1[sizeof(out.k1) - 1] = '\0';
    out.v1 = 10.0f + ((float)random(0, 300) / 10.0f);

    out.ok = true;
    return true;
  }
};

static bool read_float_input(uint8_t addr, uint16_t reg, float& outVal)
{
  if (!ModbusRTUClient.requestFrom(addr, INPUT_REGISTERS, reg, 2)) {
    return false;
  }
  const uint16_t hi = ModbusRTUClient.read();
  const uint16_t lo = ModbusRTUClient.read();
  const uint32_t u  = ((uint32_t)hi << 16) | (uint32_t)lo;
  memcpy(&outVal, &u, sizeof(outVal));
  return true;
}

class SeaMetricsCT2X : public Sensor {
public:
  const char* name() const override { return "seametricsCT2X"; }

  bool begin(const AppSettings& s) override
  {
    _addr = s.sensor_addr;
    return ModbusRTUClient.begin((int)s.sensor_baud);
  }

  void end() override { ModbusRTUClient.end(); }

  bool sample(SensorSampleMsg& out) override
  {
    float cond = NAN;
    float temp = NAN;
    const bool ok0 = read_float_input(_addr, 0x0000, cond);
    const bool ok1 = read_float_input(_addr, 0x0002, temp);

    strncpy(out.k0, "cond", sizeof(out.k0));
    out.k0[sizeof(out.k0) - 1] = '\0';
    out.v0 = cond;

    strncpy(out.k1, "temp", sizeof(out.k1));
    out.k1[sizeof(out.k1) - 1] = '\0';
    out.v1 = temp;

    out.ok = ok0 && ok1;
    return out.ok;
  }

private:
  uint8_t _addr = 1;
};

class PT12Sensor : public Sensor {
public:
  const char* name() const override { return "PT12"; }

  bool begin(const AppSettings& s) override
  {
    _addr = s.sensor_addr;
    return ModbusRTUClient.begin((int)s.sensor_baud);
  }

  void end() override { ModbusRTUClient.end(); }

  bool sample(SensorSampleMsg& out) override
  {
    float level = NAN;
    float temp  = NAN;
    const bool ok0 = read_float_input(_addr, 0x0000, level);
    const bool ok1 = read_float_input(_addr, 0x0002, temp);

    strncpy(out.k0, "level", sizeof(out.k0));
    out.k0[sizeof(out.k0) - 1] = '\0';
    out.v0 = level;

    strncpy(out.k1, "temp", sizeof(out.k1));
    out.k1[sizeof(out.k1) - 1] = '\0';
    out.v1 = temp;

    out.ok = ok0 && ok1;
    return out.ok;
  }

private:
  uint8_t _addr = 1;
};

Sensor* Sensor::create(uint32_t sensorType)
{
  switch (sensorType) {
    case 0:
      return new FakeSensor();
    case 1:
      return new SeaMetricsCT2X();
    case 2:
      return new PT12Sensor();
    default:
      return new FakeSensor();
  }
}
