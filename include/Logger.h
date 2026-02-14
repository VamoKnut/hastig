#pragma once

#include <Arduino.h>
#include <mbed.h>
#include <platform/ScopedLock.h>
#include <stdint.h>

/**
 * @brief Simple serial logger for Hastig.
 */
class Logger {
public:
  enum class Level : uint8_t { Trace = 0, Debug, Info, Warn, Error, None };

  /**
   * @brief Initialize logger output.
   */
  static void begin(Stream& s, uint32_t baud);

  /**
   * @brief Set runtime verbosity.
   */
  static void set_runtime_level(Level lvl);

  /**
   * @brief Print formatted log line.
   */
  static void log(Level lvl, const char* tag, const char* fmt, ...);

private:
  static Stream*     _out;
  static Level       _lvl;
  static rtos::Mutex _mx;
};

#define LOGT(TAG, FMT, ...) Logger::log(Logger::Level::Trace, TAG, FMT, ##__VA_ARGS__)
#define LOGD(TAG, FMT, ...) Logger::log(Logger::Level::Debug, TAG, FMT, ##__VA_ARGS__)
#define LOGI(TAG, FMT, ...) Logger::log(Logger::Level::Info, TAG, FMT, ##__VA_ARGS__)
#define LOGW(TAG, FMT, ...) Logger::log(Logger::Level::Warn, TAG, FMT, ##__VA_ARGS__)
#define LOGE(TAG, FMT, ...) Logger::log(Logger::Level::Error, TAG, FMT, ##__VA_ARGS__)
