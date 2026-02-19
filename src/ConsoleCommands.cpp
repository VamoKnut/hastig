#include "ConsoleCommands.h"

#include <ctype.h>
#include <string.h>

static void printMasked(Stream& out, const char* key, const char* value)
{
  out.print(key);
  out.print('=');

  if (value == nullptr || value[0] == '\0') {
    out.println("");
    return;
  }

  out.println("***");
}

static void printKv(Stream& out, const char* key, const char* value)
{
  out.print(key);
  out.print('=');
  out.println(value != nullptr ? value : "");
}

static void printKvU32(Stream& out, const char* key, uint32_t value)
{
  out.print(key);
  out.print('=');
  out.println(value);
}

static void printKvF(Stream& out, const char* key, float value, int decimals)
{
  out.print(key);
  out.print('=');
  out.println(value, decimals);
}

void printSettingsToSerial(const SettingsManager& settingsManager, Stream& out)
{
  const AppSettings s = settingsManager.getCopy();

  out.println("--- Hastig config ---");

  // Sensor serial settings
  printKvU32(out, "sensorAddress", (uint32_t)s.sensor_addr);
  printKvU32(out, "sensorBaudrate", s.sensor_baud);
  printKvU32(out, "sensorWarmupMs", s.sensor_warmup_ms);
  printKvU32(out, "sensorType", s.sensor_type);

  // Sampling / aggregation
  printKvU32(out, "samplePeriodMs", s.sample_period_ms);
  printKvU32(out, "aggPeriodS", s.agg_period_s);

  // Power / behaviour
  printKvU32(out, "awareTimeoutS", s.aware_timeout_s);
  printKvU32(out, "defaultSleepS", s.default_sleep_s);
  printKvU32(out, "statusIntervalS", s.status_interval_s);

  printKvF(out, "lowBattMinV", s.low_batt_min_v, 3);
  printKvU32(out, "maxChargingCurrent", (uint32_t)s.max_charging_current);
  printKvF(out, "maxChargingVoltage", s.max_charging_voltage, 3);
  printKvU32(out, "emergencyDelayS", s.emergency_delay_s);
  printKvU32(out, "emergencySleepS", s.emergency_sleep_s);

  // Network / MQTT
  printKv(out, "apn", s.apn);
  printKv(out, "apnUser", s.apn_user);
  printMasked(out, "apnPass", s.apn_pass);

  printKv(out, "mqttHost", s.mqtt_host);
  printKvU32(out, "mqttPort", s.mqtt_port);
  printKv(out, "mqttUser", s.mqtt_user);
  printMasked(out, "mqttPass", s.mqtt_pass);
  printKv(out, "mqttClientId", s.mqtt_client_id);

  printKv(out, "deviceName", s.device_name);

  out.println("---------------------");
}

static void printHelp(Stream& out)
{
  out.println("Hastig serial console:");
  out.println("  help, ?          Show this help");
  out.println("  show             Print current config");
  out.println("  config           Alias for show");
  out.println("  settings         Alias for show");
}

static void trimInPlace(char* s)
{
  // trim leading
  size_t len = strlen(s);
  size_t i = 0;
  while (i < len && isspace((unsigned char)s[i])) {
    i++;
  }
  if (i > 0) {
    memmove(s, s + i, len - i + 1);
    len = strlen(s);
  }
  // trim trailing
  while (len > 0 && isspace((unsigned char)s[len - 1])) {
    s[len - 1] = '\0';
    len--;
  }
}

void handleSerialConsole(SettingsManager& settingsManager)
{
  static char line[128];
  static size_t idx = 0;

  while (Serial.available() > 0) {
    const int c = Serial.read();
    if (c < 0) {
      break;
    }

    if (c == '\r') {
      continue;
    }

    if (c == '\n') {
      line[idx] = '\0';
      idx = 0;
      trimInPlace(line);

      if (line[0] == '\0') {
        continue;
      }

      if (strcmp(line, "help") == 0 || strcmp(line, "?") == 0) {
        printHelp(Serial);
      } else if (strcmp(line, "show") == 0 || strcmp(line, "config") == 0 ||
                 strcmp(line, "settings") == 0) {
        printSettingsToSerial(settingsManager, Serial);
      } else {
        Serial.print("Unknown command: ");
        Serial.println(line);
        printHelp(Serial);
      }
      continue;
    }

    if (idx < sizeof(line) - 1) {
      line[idx++] = (char)c;
    } else {
      // overflow -> reset line
      idx = 0;
    }
  }
}
