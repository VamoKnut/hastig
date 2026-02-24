#include "UiThread.h"

#include "Display.h"
#include "Logger.h"
#include "MenuDef.h"
#include "TimeUtil.h"
#include "BoardHal.h"
#include "StopUtil.h"
#include <Arduino.h>
#include <platform/ScopedLock.h>
#include <cmsis_os2.h>

#include <chrono>
using namespace std::chrono;

static const char* TAG = "UI";

/**
 * @brief Construct UI thread.
 */
UiThread::UiThread(EventBus& eventBus, SettingsManager& settings)
    : _eventBus(eventBus),
      _settings(settings),
      _thread(PRIO_UI, STACK_UI, nullptr, "UI")
{
}

/**
 * @brief Start UI thread.
 */
void UiThread::start()
{
  _thread.start(mbed::callback(UiThread::threadEntry, this));
  _thread.set_priority(osPriorityAboveNormal);
}

/**
 * @brief Update UI status lines.
 */
void UiThread::set_status(const char* line1, const char* line2)
{
  mbed::ScopedLock<rtos::Mutex> lock(_mx);

  strncpy(_line1, line1 ? line1 : "", sizeof(_line1));
  strncpy(_line2, line2 ? line2 : "", sizeof(_line2));

  _line1[sizeof(_line1) - 1] = '\0';
  _line2[sizeof(_line2) - 1] = '\0';
}

void UiThread::onItemSelectedEvent(const JsonVariantConst itemRetVal)
{
  UiEventMsg e{};
  e.ts_ms = timeutil::nowMs();

  const char* topic = itemRetVal["topic"] | "menu";
  strncpy(e.topic, topic, sizeof(e.topic));
  e.topic[sizeof(e.topic) - 1] = '\0';

  const size_t n = serializeJson(itemRetVal, e.value, sizeof(e.value));
  if (n == 0) {
    e.value[0] = '\0';
  } else {
    e.value[sizeof(e.value) - 1] = '\0';
  }

  _eventBus.publishUi(e);
}

/**
 * @brief RTOS thread entry.
 */
void UiThread::threadEntry(void* ctx)
{
  static_cast<UiThread*>(ctx)->run();
}

/**
 * @brief Post key event to orchestrator mail.
 */
void UiThread::post_key(BoardHal::Button b)
{
  UiEventMsg e{};
  e.ts_ms = timeutil::nowMs();
  strncpy(e.topic, "key", sizeof(e.topic));

  const char* v = "none";
  if (b == BoardHal::Button::Left) {
    v = "left";
  } else if (b == BoardHal::Button::Right) {
    v = "right";
  } else if (b == BoardHal::Button::Up) {
    v = "up";
  } else if (b == BoardHal::Button::Down) {
    v = "down";
  }

  strncpy(e.value, v, sizeof(e.value));
  e.topic[sizeof(e.topic) - 1] = '\0';
  e.value[sizeof(e.value) - 1] = '\0';

  _eventBus.publishUi(e);
}

/**
 * @brief UI thread loop.
 */
void UiThread::run()
{
  LOGI(TAG, "Thread started");

  Display::getInstance().beginHardware();
  _lastSettingsRevision = _settings.revision();

  _menu.setEventListener(this);
  _menu.setDynamicProvider(&_settings);
  const bool menuReady = _menu.init(kLedMenuJson);
  if (!menuReady) {
    Display::getInstance().showMessage("Menu parse fail");
  } else {
    _menu.action(LcdMenu::Key::None);
  }

  while (true) {
    // Wait for button activity or periodic wakeup.
    (void)BoardHal::waitForButtonEvent(200);

    BoardHal::Button b;
    while (BoardHal::popButton(b)) {
      post_key(b);
      if (menuReady) {
        _menu.action(toMenuKey(b));
      }
    }

    if (menuReady) {
      const uint32_t settingsRevision = _settings.revision();
      if (settingsRevision != _lastSettingsRevision) {
        _lastSettingsRevision = settingsRevision;
        _menu.refresh();
      }
    }
  }
}

void UiThread::stop()
{
  stoputil::terminateThread("UiThread", _thread);
}

LcdMenu::Key UiThread::toMenuKey(BoardHal::Button b) const
{
  if (b == BoardHal::Button::Up) {
    return LcdMenu::Key::Up;
  }
  if (b == BoardHal::Button::Down) {
    return LcdMenu::Key::Down;
  }
  if (b == BoardHal::Button::Left) {
    return LcdMenu::Key::Back;
  }
  if (b == BoardHal::Button::Right) {
    return LcdMenu::Key::Confirm;
  }
  return LcdMenu::Key::None;
}
