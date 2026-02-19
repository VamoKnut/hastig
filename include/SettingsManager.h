#pragma once

#include <ArduinoJson.h>
#include <mbed.h>
#include <platform/ScopedLock.h>
#include <stdint.h>

/**
 * @brief Hastig settings stored in flash.
 */
struct AppSettings {
  uint32_t version = 1;

  // Sensor serial settings
  uint8_t  sensor_addr = 1;
  uint32_t sensor_baud = 9600;

  // Sensor behavior
  uint32_t sensor_warmup_ms = 4000;
  uint32_t sensor_type      = 1;

  // Sampling / aggregation
  uint32_t sample_period_ms = 1000;
  uint32_t agg_period_s     = 15;

  // Cellular
  char sim_pin[16] = "0000";
  char apn[64]     = "telenor.smart";
  char apn_user[32] = "";
  char apn_pass[32] = "";

  // MQTT
  char     mqtt_host[64] = "mqtt.vamotech.no";
  uint16_t mqtt_port     = 1883;
  char     mqtt_user[32] = "guest";
  char     mqtt_pass[32] = "guest";
  char     mqtt_client_id[48] = "HastigClient";

  // Device
  char device_name[48] = "Hastig";

  // Behavior
  uint32_t aware_timeout_s     = 600;
  uint32_t default_sleep_s     = 3600;
  uint32_t status_interval_s   = 120;
  float    low_batt_min_v      = 2.8f;
  uint16_t max_charging_current = 1000;
  float    max_charging_voltage = 3.64f;
  uint32_t emergency_delay_s   = 60;
  uint32_t emergency_sleep_s   = 43200;
  uint32_t max_forced_sleep_s  = 43200;
  uint32_t max_unacked_packets = 10;
};

/**
 * @brief Thread-safe settings storage with flash persistence.
 */
class SettingsManager {
public:
  /**
   * @brief Load settings from flash, or defaults.
   */
  void begin();

  /**
   * @brief Get a copy of settings (thread-safe).
   */
  AppSettings getCopy() const;

  /** @brief Update settings in RAM only (no flash write). */
  void setRuntime(const AppSettings& s);

  /**
   * @brief Apply JSON patch and optionally persist.
   */
  bool applyJson(const char* json, bool persist);

  /**
   * @brief Persist current settings to flash.
   */
  bool save();

  /** @brief Reset all persisted settings to factory defaults and save to flash. */
  void factoryReset();

  /**
   * @brief Sections for publishing a configuration snapshot (secrets masked).
   */
  enum class ConfigSection : uint8_t {
    All = 0,
    Network,
    Mqtt,
    Device,
    Schedule,
    Power
  };

  /**
   * @brief Add settings fields (with secrets masked) to a JsonDocument.
   *
   * Caller controls envelope fields like "type", "tsMs", "chunk", etc.
   */
  void addMaskedConfigFields(JsonDocument& doc, ConfigSection section) const;


private:
  mutable rtos::Mutex _mx;
  AppSettings         _s;

  bool loadFromFlash();
  void setDefaults();

  static uint32_t crc32(const uint8_t* data, size_t len);
};
