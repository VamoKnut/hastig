#pragma once

#include <mbed.h>
#include <stddef.h>
#include <stdint.h>

/**
 * @brief Hardware transport for Modbus RTU over an ArduinoRS485 interface.
 */
class ModbusRtuMaster {
public:
  ModbusRtuMaster(PinName txPin, PinName dePin, PinName rePin);

  bool begin(uint32_t baud, uint32_t timeoutMs);
  void end();
  bool readHoldingRegisters(uint8_t slaveId,
                            uint16_t startAddress,
                            uint16_t registerCount,
                            uint16_t* values);
  const char* lastError();

private:
  PinName _txPin;
  PinName _dePin;
  PinName _rePin;
  uint32_t _baud = 0;
  uint32_t _timeoutMs = 0;
  bool _started = false;
};
