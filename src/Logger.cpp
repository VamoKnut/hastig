#include "Logger.h"

#include <mbed.h>
#include <platform/ScopedLock.h>

#include <stdarg.h>

Stream*       Logger::_out = nullptr;
Logger::Level Logger::_lvl = Logger::Level::Info;
rtos::Mutex   Logger::_mx;

/**
 * @brief Initialize logger output stream.
 */
void Logger::begin(Stream& s, uint32_t baud)
{
  (void)baud;
  mbed::ScopedLock<rtos::Mutex> lock(_mx);
  _out = &s;
}

/**
 * @brief Set runtime log verbosity.
 */
void Logger::set_runtime_level(Level lvl) { _lvl = lvl; }

/**
 * @brief Print formatted log line.
 */
void Logger::log(Level lvl, const char* tag, const char* fmt, ...)
{
  if (_out == nullptr) {
    return;
  }
  if (lvl < _lvl) {
    return;
  }

  mbed::ScopedLock<rtos::Mutex> lock(_mx);

  const char* lvlStr = "?";
  switch (lvl) {
  case Level::Trace:
    lvlStr = "T";
    break;
  case Level::Debug:
    lvlStr = "D";
    break;
  case Level::Info:
    lvlStr = "I";
    break;
  case Level::Warn:
    lvlStr = "W";
    break;
  case Level::Error:
    lvlStr = "E";
    break;
  default:
    break;
  }

  char    buf[256];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);

  _out->print("[");
  _out->print(lvlStr);
  _out->print("] ");
  _out->print(tag);
  _out->print(": ");
  _out->println(buf);
}
