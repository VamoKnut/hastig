#include "SettingsManager.h"

#include "Logger.h"
#include "AppConfig.h"
#include <Arduino.h>
#include <platform/ScopedLock.h>
#include <mbed.h>
#include <string.h>
#include <platform/ScopedLock.h>

static const char* TAG = "SET";

struct StoredBlob {
  uint32_t    magic;
  uint32_t    crc;
  AppSettings settings;
};

static constexpr uint32_t SETTINGS_MAGIC = 0x53455453;  // 'SETS'

/**
 * @brief Compute CRC32.
 */
uint32_t SettingsManager::crc32(const uint8_t* data, size_t len)
{
  uint32_t crc = 0xFFFFFFFFu;
  for (size_t i = 0; i < len; i++) {
    crc ^= data[i];
    for (int b = 0; b < 8; b++) {
      const uint32_t mask = -(crc & 1u);
      crc = (crc >> 1) ^ (0xEDB88320u & mask);
    }
  }
  return ~crc;
}

/**
 * @brief Initialize settings store.
 */
void SettingsManager::begin()
{
  mbed::ScopedLock<rtos::Mutex> lock(_mx);

  if (!loadFromFlash()) {
    setDefaults();
    (void)save();
  }

  
  // Sanity-clamp settings (also helps when upgrading from older stored configs).
  if (_s.aware_timeout_s < 60u) {
    _s.aware_timeout_s = 600u;
  }
  // Project default: keep status cadence stable for backend consumption.
  _s.status_interval_s = 120u;
  if (_s.default_sleep_s < 60u) {
    _s.default_sleep_s = 3600u;
  }
LOGI(TAG,
       "Settings loaded: apn=%s mqtt=%s:%u sample_ms=%lu agg_s=%lu",
       _s.apn,
       _s.mqtt_host,
       (unsigned)_s.mqtt_port,
       (unsigned long)_s.sample_period_ms,
       (unsigned long)_s.agg_period_s);
}

/**
 * @brief Return a copy of current settings.
 */
AppSettings SettingsManager::getCopy() const
{
  mbed::ScopedLock<rtos::Mutex> lock(_mx);
  return _s;
}

/**
 * @brief Apply JSON patch to settings.
 */
bool SettingsManager::applyJson(const char* json, bool persist)
{
  if (json == nullptr) {
    return false;
  }

  JsonDocument doc;
  const DeserializationError err = deserializeJson(doc, json);
  if (err) {
    LOGW(TAG, "JSON parse failed: %s", err.c_str());
    return false;
  }

  mbed::ScopedLock<rtos::Mutex> lock(_mx);

  if (doc["sensorAddress"].is<uint8_t>()) {
    _s.sensor_addr = doc["sensorAddress"].as<uint8_t>();
  }
  if (doc["sensorBaudrate"].is<uint32_t>()) {
    _s.sensor_baud = doc["sensorBaudrate"].as<uint32_t>();
  }
  if (doc["sensorWarmupMs"].is<uint32_t>()) {
    _s.sensor_warmup_ms = doc["sensorWarmupMs"].as<uint32_t>();
  }
  if (doc["sensorType"].is<uint32_t>()) {
    _s.sensor_type = doc["sensorType"].as<uint32_t>();
  }

  if (doc["samplePeriodMs"].is<uint32_t>()) {
    _s.sample_period_ms = doc["samplePeriodMs"].as<uint32_t>();
    if (_s.sample_period_ms < MIN_SAMPLE_PERIOD_MS) {
      _s.sample_period_ms = MIN_SAMPLE_PERIOD_MS;
    }
  }
  if (doc["aggPeriodS"].is<uint32_t>()) {
    _s.agg_period_s = doc["aggPeriodS"].as<uint32_t>();
  }
  if (doc["aggregationMethod"].is<const char*>()) {
    LOGI(TAG, "aggregationMethod override requested: %s", doc["aggregationMethod"].as<const char*>());
  }

  if (doc["simPin"].is<const char*>()) {
    strncpy(_s.sim_pin, doc["simPin"].as<const char*>(), sizeof(_s.sim_pin));
    _s.sim_pin[sizeof(_s.sim_pin) - 1] = '\0';
  }
  if (doc["apn"].is<const char*>()) {
    strncpy(_s.apn, doc["apn"].as<const char*>(), sizeof(_s.apn));
    _s.apn[sizeof(_s.apn) - 1] = '\0';
  }
  if (doc["apnUser"].is<const char*>()) {
    strncpy(_s.apn_user, doc["apnUser"].as<const char*>(), sizeof(_s.apn_user));
    _s.apn_user[sizeof(_s.apn_user) - 1] = '\0';
  }
  if (doc["apnPass"].is<const char*>()) {
    strncpy(_s.apn_pass, doc["apnPass"].as<const char*>(), sizeof(_s.apn_pass));
    _s.apn_pass[sizeof(_s.apn_pass) - 1] = '\0';
  }

  if (doc["mqttHost"].is<const char*>()) {
    strncpy(_s.mqtt_host, doc["mqttHost"].as<const char*>(), sizeof(_s.mqtt_host));
    _s.mqtt_host[sizeof(_s.mqtt_host) - 1] = '\0';
  }
  if (doc["mqttPort"].is<uint16_t>()) {
    _s.mqtt_port = doc["mqttPort"].as<uint16_t>();
  }
  if (doc["mqttUser"].is<const char*>()) {
    strncpy(_s.mqtt_user, doc["mqttUser"].as<const char*>(), sizeof(_s.mqtt_user));
    _s.mqtt_user[sizeof(_s.mqtt_user) - 1] = '\0';
  }
  if (doc["mqttPass"].is<const char*>()) {
    strncpy(_s.mqtt_pass, doc["mqttPass"].as<const char*>(), sizeof(_s.mqtt_pass));
    _s.mqtt_pass[sizeof(_s.mqtt_pass) - 1] = '\0';
  }
  if (doc["mqttClientId"].is<const char*>()) {
    strncpy(_s.mqtt_client_id, doc["mqttClientId"].as<const char*>(), sizeof(_s.mqtt_client_id));
    _s.mqtt_client_id[sizeof(_s.mqtt_client_id) - 1] = '\0';
  }

  if (doc["deviceName"].is<const char*>()) {
    strncpy(_s.device_name, doc["deviceName"].as<const char*>(), sizeof(_s.device_name));
    _s.device_name[sizeof(_s.device_name) - 1] = '\0';
  }

  if (doc["awareTimeoutS"].is<uint32_t>()) {
    _s.aware_timeout_s = doc["awareTimeoutS"].as<uint32_t>();
  }
  if (doc["defaultSleepS"].is<uint32_t>()) {
    _s.default_sleep_s = doc["defaultSleepS"].as<uint32_t>();
  }
  if (doc["statusIntervalS"].is<uint32_t>()) {
    _s.status_interval_s = doc["statusIntervalS"].as<uint32_t>();
  }
  if (doc["lowBattMinV"].is<float>()) {
    _s.low_batt_min_v = doc["lowBattMinV"].as<float>();
  }
  if (doc["maxChargingCurrent"].is<uint16_t>()) {
    _s.max_charging_current = doc["maxChargingCurrent"].as<uint16_t>();
  }
  if (doc["maxChargingVoltage"].is<float>()) {
    _s.max_charging_voltage = doc["maxChargingVoltage"].as<float>();
  }
  if (doc["emergencyDelayS"].is<uint32_t>()) {
    _s.emergency_delay_s = doc["emergencyDelayS"].as<uint32_t>();
  }
  if (doc["emergencySleepS"].is<uint32_t>()) {
    _s.emergency_sleep_s = doc["emergencySleepS"].as<uint32_t>();
  }
  if (doc["maxForcedSleepS"].is<uint32_t>()) {
    _s.max_forced_sleep_s = doc["maxForcedSleepS"].as<uint32_t>();
  }
  if (doc["maxUnackedPackets"].is<uint32_t>()) {
    _s.max_unacked_packets = doc["maxUnackedPackets"].as<uint32_t>();
  }

  if (persist) {
    return save();
  }
  return true;
}

/**
 * @brief Save settings to flash (single-sector scheme).
 */
bool SettingsManager::save()
{
  mbed::ScopedLock<rtos::Mutex> lock(_mx);

  mbed::FlashIAP flash;
  if (flash.init() != 0) {
    LOGE(TAG, "Flash init failed");
    return false;
  }

  const uint32_t flashSize  = flash.get_flash_size();
  const uint32_t sectorSize = flash.get_sector_size(flash.get_flash_start() + flashSize - 1);
  const uint32_t base       = flash.get_flash_start() + flashSize - sectorSize;

  StoredBlob blob;
  blob.magic    = SETTINGS_MAGIC;
  blob.settings = _s;
  blob.crc      = crc32((const uint8_t*)&blob.settings, sizeof(blob.settings));

  if (flash.erase(base, sectorSize) != 0) {
    LOGE(TAG, "Flash erase failed");
    flash.deinit();
    return false;
  }

  const uint32_t pageSize = flash.get_page_size();
  const uint8_t* p = (const uint8_t*)&blob;
  size_t remaining = sizeof(blob);
  uint32_t addr = base;

  while (remaining > 0) {
    const size_t chunk = (remaining > pageSize) ? pageSize : remaining;
    if (flash.program(p, addr, chunk) != 0) {
      LOGE(TAG, "Flash program failed");
      flash.deinit();
      return false;
    }
    addr += chunk;
    p += chunk;
    remaining -= chunk;
  }

  flash.deinit();
  LOGI(TAG, "Settings saved");
  return true;
}

/**
 * @brief Load settings from flash if valid.
 */
bool SettingsManager::loadFromFlash()
{
  mbed::FlashIAP flash;
  if (flash.init() != 0) {
    return false;
  }

  const uint32_t flashSize  = flash.get_flash_size();
  const uint32_t sectorSize = flash.get_sector_size(flash.get_flash_start() + flashSize - 1);
  const uint32_t base       = flash.get_flash_start() + flashSize - sectorSize;

  const StoredBlob* blob = (const StoredBlob*)base;

  if (blob->magic != SETTINGS_MAGIC) {
    flash.deinit();
    return false;
  }

  const uint32_t got = blob->crc;
  const uint32_t exp = crc32((const uint8_t*)&blob->settings, sizeof(blob->settings));
  if (got != exp) {
    flash.deinit();
    return false;
  }

  _s = blob->settings;
  flash.deinit();
  return true;
}

/**
 * @brief Restore default settings.
 */
void SettingsManager::setDefaults()
{
  _s = AppSettings{};
  strncpy(_s.device_name, "", sizeof(_s.device_name));
  _s.device_name[sizeof(_s.device_name) - 1] = '\0';
}

void SettingsManager::setRuntime(const AppSettings& s)
{
  mbed::ScopedLock<rtos::Mutex> lock(_mx);
  _s = s;
}

/**
 * @brief Reset settings to factory defaults and persist to flash.
 */
void SettingsManager::factoryReset()
{
  mbed::ScopedLock<rtos::Mutex> lock(_mx);
  setDefaults();
  (void)save();
}

static const char* maskIfSet(const char* v)
{
  return (v != nullptr && v[0] != '\0') ? "***" : "";
}

void SettingsManager::addMaskedConfigFields(JsonDocument& doc, ConfigSection section) const
{
  const AppSettings s = getCopy();

  const auto addNetwork = [&]() {
    doc["apn"]     = s.apn;
    doc["simPin"]  = maskIfSet(s.sim_pin);
    doc["apnUser"] = maskIfSet(s.apn_user);
    doc["apnPass"] = maskIfSet(s.apn_pass);
  };

  const auto addMqtt = [&]() {
    doc["mqttHost"]     = s.mqtt_host;
    doc["mqttPort"]     = s.mqtt_port;
    doc["mqttClientId"] = s.mqtt_client_id;
    doc["mqttUser"]     = maskIfSet(s.mqtt_user);
    doc["mqttPass"]     = maskIfSet(s.mqtt_pass);
  };

  const auto addDevice = [&]() {
    doc["deviceName"]     = s.device_name;
    doc["sensorAddress"]  = s.sensor_addr;
    doc["sensorBaudrate"] = s.sensor_baud;
    doc["sensorWarmupMs"] = s.sensor_warmup_ms;
    doc["sensorType"]     = s.sensor_type;
  };

  const auto addSchedule = [&]() {
    doc["samplePeriodMs"]  = s.sample_period_ms;
    doc["aggPeriodS"]      = s.agg_period_s;
    doc["awareTimeoutS"]   = s.aware_timeout_s;
    doc["defaultSleepS"]   = s.default_sleep_s;
    doc["statusIntervalS"] = s.status_interval_s;
  };

  const auto addPower = [&]() {
    doc["lowBattMinV"]       = s.low_batt_min_v;
    doc["maxChargingCurrent"] = s.max_charging_current;
    doc["maxChargingVoltage"] = s.max_charging_voltage;
    doc["emergencyDelayS"]   = s.emergency_delay_s;
    doc["emergencySleepS"]   = s.emergency_sleep_s;
    doc["maxForcedSleepS"]   = s.max_forced_sleep_s;
    doc["maxUnackedPackets"] = s.max_unacked_packets;
  };

  switch (section) {
    case ConfigSection::All:
      addNetwork();
      addMqtt();
      addDevice();
      addSchedule();
      addPower();
      break;
    case ConfigSection::Network:
      addNetwork();
      break;
    case ConfigSection::Mqtt:
      addMqtt();
      break;
    case ConfigSection::Device:
      addDevice();
      break;
    case ConfigSection::Schedule:
      addSchedule();
      break;
    case ConfigSection::Power:
      addPower();
      break;
  }
}
