#pragma once

#include <mbed.h>
#include <platform/ScopedLock.h>

#include "AppConfig.h"
#include "BoardHal.h"
#include "EventBus.h"
#include "Messages.h"

/**
 * @brief UI thread: OLED (U8G2) + 4 keys.
 */
class UiThread {
public:
  UiThread(EventBus& eventBus);

  /**
   * @brief Start RTOS thread.
   */
  void start();
  void stop();

  /**
   * @brief Update status lines.
   */
  void set_status(const char* line1, const char* line2);

private:
    EventBus& _eventBus;

  rtos::Thread _thread;

  mutable rtos::Mutex _mx;
  char                _line1[22] = "Hastig boot...";
  char                _line2[22] = "";

  static void threadEntry(void* ctx);
  void run();

  void post_key(BoardHal::Button b);
};
