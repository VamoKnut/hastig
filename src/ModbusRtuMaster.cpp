#include "ModbusRtuMaster.h"

#include <Arduino.h>
#include <ArduinoModbus.h>
#include <ArduinoRS485.h>
#include <pinDefinitions.h>

namespace {
constexpr uint16_t kMaxHoldingRegisters = 125u;
}

ModbusRtuMaster::ModbusRtuMaster(PinName txPin, PinName dePin, PinName rePin)
    : _txPin(txPin), _dePin(dePin), _rePin(rePin)
{
}

bool ModbusRtuMaster::begin(uint32_t baud, uint32_t timeoutMs)
{
  if (baud == 0u || timeoutMs == 0u) {
    return false;
  }
  if (_started) {
    return _baud == baud && _timeoutMs == timeoutMs;
  }

  RS485.setPins(PinNameToIndex(_txPin), PinNameToIndex(_dePin), PinNameToIndex(_rePin));
  const int frameDelayUs = static_cast<int>((35UL * 1000000UL + (baud - 1UL)) / baud);
  RS485.setDelays(frameDelayUs, frameDelayUs);

  if (!ModbusRTUClient.begin(baud, SERIAL_8N1)) {
    return false;
  }

  ModbusRTUClient.setTimeout(timeoutMs);
  _baud = baud;
  _timeoutMs = timeoutMs;
  _started = true;
  return true;
}

void ModbusRtuMaster::end()
{
  if (_started) {
    ModbusRTUClient.end();
    _started = false;
    _baud = 0;
    _timeoutMs = 0;
  }
}

bool ModbusRtuMaster::readHoldingRegisters(uint8_t slaveId,
                                          uint16_t startAddress,
                                          uint16_t registerCount,
                                          uint16_t* values)
{
  if (!_started || slaveId == 0u || slaveId > 247u ||
      registerCount == 0u || registerCount > kMaxHoldingRegisters || values == nullptr ||
      static_cast<uint32_t>(startAddress) + registerCount > 0x10000UL) {
    return false;
  }

  uint16_t response[kMaxHoldingRegisters] = {};
  const int requested = ModbusRTUClient.requestFrom(
      static_cast<int>(slaveId), HOLDING_REGISTERS, startAddress, registerCount);
  if (requested != registerCount) {
    return false;
  }

  for (uint16_t i = 0; i < registerCount; ++i) {
    if (ModbusRTUClient.available() <= 0) {
      return false;
    }

    const long value = ModbusRTUClient.read();
    if (value < 0 || value > 0xFFFFL) {
      return false;
    }
    response[i] = static_cast<uint16_t>(value);
  }

  for (uint16_t i = 0; i < registerCount; ++i) {
    values[i] = response[i];
  }
  return true;
}

const char* ModbusRtuMaster::lastError()
{
  return ModbusRTUClient.lastError();
}
