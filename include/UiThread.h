#pragma once

#include <mbed.h>
#include <platform/ScopedLock.h>
#include <atomic>

#include "AppConfig.h"
#include "BoardHal.h"
#include "EventBus.h"
#include "LcdMenu.h"
#include "Messages.h"
#include "RuntimeStatus.h"
#include "SettingsManager.h"

/**
 * @brief UI thread: OLED (U8G2) + 4 keys.
 */
class UiThread : public IMenuEventListener {
public:
  UiThread(EventBus& eventBus, SettingsManager& settings, RuntimeStatus& runtimeStatus);

  /**
   * @brief Start RTOS thread.
   */
  void start();
  bool stop(uint32_t timeoutMs = 3000u);

  /**
   * @brief Update status lines.
   */
  void set_status(const char* line1, const char* line2);
  void onItemSelectedEvent(const JsonVariantConst itemRetVal) override;

private:
  EventBus& _eventBus;
  SettingsManager& _settings;
  RuntimeStatus& _runtimeStatus;
  LcdMenu   _menu;
  uint32_t  _lastSettingsRevision = 0;
  bool      _statusMode = true;

  rtos::Thread _thread;
  rtos::EventFlags _completion;
  std::atomic<bool> _stopRequested{false};

  static constexpr uint32_t FLAG_STOPPED = 1u << 0;

  mutable rtos::Mutex _mx;
  char                _line1[22] = "Hastig boot...";
  char                _line2[22] = "";

  static void threadEntry(void* ctx);
  void run();
  void renderStatus();

  void post_key(BoardHal::Button b);
  LcdMenu::Key toMenuKey(BoardHal::Button b) const;
};
