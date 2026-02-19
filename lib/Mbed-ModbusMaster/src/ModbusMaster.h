#pragma once
#include <mbed.h>

template <size_t rxBufSize>
struct TransactionSerial : mbed::SerialBase, mbed::NonCopyable<TransactionSerial<rxBufSize>>
{
   enum struct Result : uint8_t
   {
      success = 0x00,
      timeout = 0xE0,
      busy    = 0xE1,
   };

   events::EventQueue* queue;
   rtos::Semaphore     txnLock; // Mutex doesn't work for some reason...
   mbed::Timeout       frameTimeout;

   std::chrono::milliseconds rxTimeout;
   std::chrono::microseconds frameDelim;
   size_t                    rxIdx            = 0;
   uint8_t                   rxBuf[rxBufSize] = {};
   int                       timeoutEvent     = 0;

   mbed::Callback<void()>       preTransmit  = NULL;
   mbed::Callback<void()>       postTransmit = NULL;
   mbed::Callback<void(Result)> complete     = NULL;

   inline TransactionSerial(events::EventQueue* queue, PinName txPin, PinName rxPin, int baud,
                            std::chrono::microseconds frameDelim,
                            std::chrono::milliseconds rxTimeout = std::chrono::milliseconds{50})
       : SerialBase(txPin, rxPin, baud), queue(queue), txnLock(1, 1), rxTimeout(rxTimeout),
         frameDelim(frameDelim)
   {
   }

   inline void setTimeout(std::chrono::milliseconds t)
   {
      rxTimeout = t;
   };
   inline void attachPreTransmit(mbed::Callback<void()> f)
   {
      preTransmit = f;
   };
   inline void attachPostTransmit(mbed::Callback<void()> f)
   {
      postTransmit = f;
   };

   void rxHandler()
   {
      if (!readable())
         return;
      if (rxIdx >= rxBufSize)
         rxIdx = 0;
      lock();
      rxBuf[rxIdx++] = _base_getc();
      unlock();
      frameTimeout.attach(
          [this]()
          {
             queue->call(
                 [this]()
                 {
                    if (txnLock.try_acquire())
                    { // Guard rx outside txn
                       txnLock.release();
                       return;
                    }
                    rxCompleteHandler();
                 });
          },
          frameDelim);
   }

   void rxCompleteHandler()
   {
      attach(NULL);
      queue->cancel(timeoutEvent);
      txnLock.release();
      complete(Result::success);
   }

   void timeoutHandler()
   {
      attach(NULL);
      frameTimeout.detach();
      txnLock.release();
      complete(Result::timeout);
   }

   void transact(uint8_t* txBuf, int txLen, mbed::Callback<void(Result)> cb)
   {
      if (!txnLock.try_acquire())
      {
         return cb(Result::busy);
      }
      auto fail_now = [this](Result r)
      {
         attach(NULL);
         frameTimeout.detach();
         if (timeoutEvent != 0)
         {
            queue->cancel(timeoutEvent);
         }
         txnLock.release();
         complete(r);
      };

      rxIdx        = 0;
      complete     = cb;
      if (preTransmit)
      {
         preTransmit();
      }
      // Arduino-mbed may not enable DEVICE_SERIAL_ASYNCH; use blocking TX.
      for (int i = 0; i < txLen; i++)
      {
         mbed::Timer txWait;
         txWait.start();
         while (!writeable())
         {
            if (txWait.elapsed_time() >= std::chrono::milliseconds(20))
            {
               fail_now(Result::timeout);
               return;
            }
            rtos::ThisThread::sleep_for(std::chrono::milliseconds(1));
         }
         _base_putc(txBuf[i]);
      }
      if (postTransmit)
      {
         // In blocking-TX mode, switch RS485 direction immediately so early
         // response bytes are not missed while waiting for queue dispatch.
         postTransmit();
      }

      // Synchronous RX path: poll bytes until inter-frame gap or timeout.
      mbed::Timer rxTimer;
      rxTimer.start();
      int64_t lastByteUs = 0;
      bool    gotAny     = false;
      const int64_t timeoutUs = std::chrono::duration_cast<std::chrono::microseconds>(rxTimeout).count();
      const int64_t gapUs     = frameDelim.count();

      while (true)
      {
         if (readable())
         {
            if (rxIdx < rxBufSize)
            {
               rxBuf[rxIdx++] = _base_getc();
            }
            else
            {
               (void)_base_getc();
            }
            gotAny     = true;
            lastByteUs = rxTimer.elapsed_time().count();
            continue;
         }

         const int64_t nowUs = rxTimer.elapsed_time().count();
         if (gotAny && (nowUs - lastByteUs) >= gapUs)
         {
            break;
         }
         if (nowUs >= timeoutUs)
         {
            break;
         }
         rtos::ThisThread::sleep_for(std::chrono::milliseconds(1));
      }

      txnLock.release();
      complete(gotAny ? Result::success : Result::timeout);
   }
};

template <size_t txBufSize, size_t rxBufSize>
struct ModbusMaster : mbed::NonCopyable<ModbusMaster<txBufSize, rxBufSize>>
{
   enum struct Result : uint8_t
   {
      success = 0x00,
      // modbus exceptions
      illegalFunction     = 0x01,
      illegalDataAddress  = 0x02,
      illegalDataValue    = 0x03,
      slaveDeviceFailure  = 0x04,
      acknowledge         = 0x05,
      slaveDeviceBusy     = 0x06,
      negativeAcknowledge = 0x07,
      memoryParityError   = 0x08,
      // response errors
      timeout            = 0xE0,
      busy               = 0xE1,
      incompleteResponse = 0xE2,
      invalidSlaveId     = 0xE3,
      invalidFunction    = 0xE4,
      invalidCRC         = 0xE5,
   };

   enum struct FC : uint8_t
   { // Modbus Function Codes
      none = 0x00,
      // bit access
      readCoils          = 0x01,
      readDiscreteInputs = 0x02,
      writeSingleCoil    = 0x05,
      writeMultipleCoils = 0x0F,
      // word access
      readHoldingRegisters   = 0x03,
      readInputRegisters     = 0x04,
      writeSingleRegister    = 0x06,
      writeMultipleRegisters = 0x10,
   };

   typedef TransactionSerial<rxBufSize> STX;
   typedef mbed::Callback<void(Result)> CB;

   STX                                                  stx;
   uint8_t                                              slaveId;
   bool                                                 checkCrc    = true;
   CB                                                   complete    = NULL;
   mbed::Callback<uint16_t(uint8_t* adu, uint16_t len)> postReceive = NULL;

   size_t  txIdx = 0;
   uint8_t adu[txBufSize];
   FC      reqFc = FC::none;

   static std::chrono::microseconds calcFrameDelim(int baud)
   {
      if (baud <= 0)
      {
         return std::chrono::microseconds{3500};
      }
      return std::chrono::microseconds((35 * 1000 * 1000) / baud);
   }

   inline ModbusMaster(events::EventQueue* queue, PinName txPin, PinName rxPin, int baud,
                       uint8_t                   slaveId,
                       std::chrono::milliseconds rxTimeout = std::chrono::milliseconds{50})
       : stx(queue, txPin, rxPin, baud, calcFrameDelim(baud), rxTimeout),
         slaveId(slaveId)
   {
   }

   inline void setSlaveId(uint8_t id)
   {
      slaveId = id;
   };
   inline void setTimeout(std::chrono::milliseconds t)
   {
      stx.setTimeout(t);
   };
   inline void setCrcCheck(bool c)
   {
      checkCrc = c;
   };
   inline void attachPreTransmit(mbed::Callback<void()> f)
   {
      stx.attachPreTransmit(f);
   };
   inline void attachPostTransmit(mbed::Callback<void()> f)
   {
      stx.attachPostTransmit(f);
   };
   inline void attachPostReceive(mbed::Callback<uint16_t(uint8_t* adu, uint16_t len)> f)
   {
      postReceive = f;
   };

   inline uint8_t* getCoils()
   {
      return stx.rxBuf + 3;
   };
   inline uint16_t* getRegisters()
   {
      return reinterpret_cast<uint16_t*>(stx.rxBuf + 3);
   }

   void writeUInt16(uint16_t val)
   {
      adu[txIdx++] = val >> 8;
      adu[txIdx++] = val;
   }

   uint32_t crc16(uint8_t* buf, int len)
   {
      uint32_t crc = 0xFFFF;
      for (int pos = 0; pos < len; pos++)
      {
         crc ^= (uint32_t)buf[pos]; // XOR byte into least sig. byte of crc
         for (int i = 8; i != 0; i--)
         { // Loop over each bit
            if ((crc & 0x0001) != 0)
            {             // If the LSB is set
               crc >>= 1; // Shift right and XOR 0xA001
               crc ^= 0xA001;
            }
            else
            {             // Else LSB is not set
               crc >>= 1; // Just shift right
            }
         }
      }
      return crc;
   }

   void transaction(FC fc, uint16_t addr, uint16_t num, uint8_t* val, CB cb)
   {
      txIdx    = 0;
      reqFc    = fc;
      complete = cb;

      // Build Request
      // ==============================================
      adu[txIdx++] = slaveId;
      adu[txIdx++] = static_cast<uint8_t>(fc);
      writeUInt16(addr);
      switch (fc)
      {
      case FC::readCoils:
      case FC::readDiscreteInputs:
      case FC::writeMultipleCoils:
      case FC::readHoldingRegisters:
      case FC::readInputRegisters:
      case FC::writeMultipleRegisters:
         writeUInt16(num);
      default:
         break;
      }
      uint8_t payloadLen = 0;
      switch (fc)
      {
      case FC::writeSingleCoil:
         payloadLen = 2;
         break;
      case FC::writeMultipleCoils:
         payloadLen   = (num / 8) + 1;
         adu[txIdx++] = payloadLen;
         break;
      case FC::writeSingleRegister:
         payloadLen = 2;
         break;
      case FC::writeMultipleRegisters:
         payloadLen   = num * 2;
         adu[txIdx++] = payloadLen;
         break;
      default:
         break;
      }
      for (int i = 0; i < payloadLen; i++) adu[txIdx++] = val[i];
      uint32_t crc = crc16(adu, txIdx);
      adu[txIdx++] = crc;
      adu[txIdx++] = crc >> 8;

      // Serial Transaction
      // ============================================
      stx.transact(adu, txIdx,
                   [this](auto result)
                   {
                      if (result != STX::Result::success)
                      {
                         complete((Result)result);
                         return;
                      }

                      uint32_t rxLen = stx.rxIdx;
                      if (postReceive)
                         rxLen = postReceive(stx.rxBuf, rxLen);

                      if (rxLen < 4)
                      {
                         complete(Result::incompleteResponse);
                         return;
                      }

                      if (checkCrc)
                      {
                         uint32_t compCRC = crc16(stx.rxBuf, rxLen - 2);
                         uint16_t recvCRC = stx.rxBuf[rxLen - 2] | (stx.rxBuf[rxLen - 1] << 8);
                         if (recvCRC != compCRC)
                         {
                            complete(Result::invalidCRC);
                            return;
                         }
                      }

                      if (stx.rxBuf[0] != slaveId)
                      {
                         complete(Result::invalidSlaveId);
                         return;
                      }

                      if (static_cast<FC>(stx.rxBuf[1] & 0x7F) != reqFc)
                      {
                         complete(Result::invalidFunction);
                         return;
                      }

                      if (stx.rxBuf[1] & 0x80)
                      { // Exception
                         complete(static_cast<Result>(stx.rxBuf[2]));
                         return;
                      }

                      switch (reqFc)
                      {
                      case FC::readHoldingRegisters:
                      case FC::readInputRegisters:
                      {
                         uint16_t* reg      = getRegisters();
                         uint8_t   regCount = (rxLen - 5) / 2;
                         for (int i = 0; i < regCount; i++)
                         {
                            reg[i] = __builtin_bswap16(reg[i]);
                         }
                      }
                      default:
                         break;
                      }

                      complete(Result::success);
                   });
   }

   void readCoils(uint16_t addr, uint16_t num, CB cb)
   {
      transaction(FC::readCoils, addr, num, NULL, cb);
   }
   void readDiscreteInputs(uint16_t addr, uint16_t num, CB cb)
   {
      transaction(FC::readDiscreteInputs, addr, num, NULL, cb);
   }
   void writeSingleCoil(uint16_t addr, bool val, CB cb)
   {
      uint16_t payload = __builtin_bswap16(val ? 0xFF00 : 0x0000);
      transaction(FC::writeSingleCoil, addr, 1, reinterpret_cast<uint8_t*>(&payload), cb);
   }
   void writeMultipleCoils(uint16_t addr, uint16_t num, uint8_t* val, CB cb)
   {
      transaction(FC::writeMultipleCoils, addr, num, val, cb);
   }

   void readHoldingRegisters(uint16_t addr, uint16_t num, CB cb)
   {
      transaction(FC::readHoldingRegisters, addr, num, NULL, cb);
   }
   void readInputRegisters(uint16_t addr, uint16_t num, CB cb)
   {
      transaction(FC::readInputRegisters, addr, num, NULL, cb);
   }
   void writeSingleRegister(uint16_t addr, uint16_t val, CB cb)
   {
      val              = __builtin_bswap16(val);
      uint8_t* payload = reinterpret_cast<uint8_t*>(&val);
      transaction(FC::writeSingleRegister, addr, 1, payload, cb);
   }
   void writeMultipleRegisters(uint16_t addr, uint16_t num, uint16_t* val, CB cb)
   {
      for (int i = 0; i < num; i++) val[i] = __builtin_bswap16(val[i]);
      uint8_t* payload = reinterpret_cast<uint8_t*>(val);
      transaction(FC::writeMultipleRegisters, addr, num, payload, cb);
   }
};
