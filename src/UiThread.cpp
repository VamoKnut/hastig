#include "UiThread.h"

#include "Logger.h"
#include "TimeUtil.h"
#include "BoardHal.h"
#include "StopUtil.h"
#include <Arduino.h>
#include <platform/ScopedLock.h>
#include <cmsis_os2.h>
#include <U8g2lib.h>

#include <chrono>
using namespace std::chrono;

static const char* TAG = "UI";

// Full-buffer I2C 128x64
static U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);

/**
 * @brief Construct UI thread.
 */
UiThread::UiThread(EventBus& eventBus)
    : _eventBus(eventBus),
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

  u8g2.begin();
  u8g2.setFont(u8g2_font_6x12_tf);

  while (true) {
    // Wait for button activity or periodic UI refresh.
    (void)BoardHal::waitForButtonEvent(200);

    BoardHal::Button b;
    while (BoardHal::popButton(b)) {
      post_key(b);
    }

    char l1[22];
    char l2[22];
    {
      mbed::ScopedLock<rtos::Mutex> lock(_mx);
      strncpy(l1, _line1, sizeof(l1));
      strncpy(l2, _line2, sizeof(l2));
      l1[sizeof(l1) - 1] = '\0';
      l2[sizeof(l2) - 1] = '\0';
    }

    u8g2.clearBuffer();
    u8g2.drawStr(0, 14, l1);
    u8g2.drawStr(0, 28, l2);
    u8g2.sendBuffer();
  }
}

void UiThread::stop()
{
  stoputil::terminateThread("UiThread", _thread);
}
