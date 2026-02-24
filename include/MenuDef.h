#pragma once

static constexpr const char kLedMenuJson[] = R"JSON(
{
  "items": [
    {
      "label": "Start sampling",
      "type": "action",
      "retval": {"topic":"cmd", "value":"startSampling"}
    },
    {
      "label": "Stop sampling",
      "type": "action",
      "retval": {"topic":"cmd", "value":"stopSampling"}
    },
    {
      "label": "Hibernate 10m",
      "type": "action",
      "retval": {"topic":"cmd", "value":"hibernate"}
    },
    {
      "label": "Setup",
      "type": "sub",
      "items": [
        {
          "label": "Sensor address",
          "type": "sel",
          "items": [
            {"label":"1", "retval":{"topic":"setup","prop":"sensorAddress","value":1}},
            {"label":"2", "retval":{"topic":"setup","prop":"sensorAddress","value":2}},
            {"label":"3", "retval":{"topic":"setup","prop":"sensorAddress","value":3}},
            {"label":"4", "retval":{"topic":"setup","prop":"sensorAddress","value":4}},
            {"label":"5", "retval":{"topic":"setup","prop":"sensorAddress","value":5}},
            {"label":"6", "retval":{"topic":"setup","prop":"sensorAddress","value":6}},
            {"label":"7", "retval":{"topic":"setup","prop":"sensorAddress","value":7}},
            {"label":"8", "retval":{"topic":"setup","prop":"sensorAddress","value":8}},
            {"label":"9", "retval":{"topic":"setup","prop":"sensorAddress","value":9}}
          ]
        },
        {
          "label": "Sensor baudrate",
          "type": "sel",
          "items": [
            {"label":"1200",   "retval":{"topic":"setup","prop":"sensorBaudrate","value":1200}},
            {"label":"4800",   "retval":{"topic":"setup","prop":"sensorBaudrate","value":4800}},
            {"label":"9600",   "retval":{"topic":"setup","prop":"sensorBaudrate","value":9600}},
            {"label":"19200",  "retval":{"topic":"setup","prop":"sensorBaudrate","value":19200}},
            {"label":"38400",  "retval":{"topic":"setup","prop":"sensorBaudrate","value":38400}},
            {"label":"57600",  "retval":{"topic":"setup","prop":"sensorBaudrate","value":57600}},
            {"label":"115200", "retval":{"topic":"setup","prop":"sensorBaudrate","value":115200}}
          ]
        },
        {
          "label": "Sensor warmup (ms)",
          "type": "sel",
          "items": [
            {"label":"500",   "retval":{"topic":"setup","prop":"sensorWarmupMs","value":500}},
            {"label":"1000",  "retval":{"topic":"setup","prop":"sensorWarmupMs","value":1000}},
            {"label":"2000",  "retval":{"topic":"setup","prop":"sensorWarmupMs","value":2000}},
            {"label":"4000",  "retval":{"topic":"setup","prop":"sensorWarmupMs","value":4000}},
            {"label":"5000",  "retval":{"topic":"setup","prop":"sensorWarmupMs","value":5000}},
            {"label":"10000", "retval":{"topic":"setup","prop":"sensorWarmupMs","value":10000}}
          ]
        },
        {
          "label": "Sensor type",
          "type": "sel",
          "items": [
            {"label":"0", "retval":{"topic":"setup","prop":"sensorType","value":0}},
            {"label":"1", "retval":{"topic":"setup","prop":"sensorType","value":1}},
            {"label":"2", "retval":{"topic":"setup","prop":"sensorType","value":2}}
          ]
        },
        {
          "label": "Sampling interval",
          "type": "sel",
          "items": [
            {"label":"250",   "retval":{"topic":"setup","prop":"samplingInterval","value":250}},
            {"label":"500",   "retval":{"topic":"setup","prop":"samplingInterval","value":500}},
            {"label":"750",   "retval":{"topic":"setup","prop":"samplingInterval","value":750}},
            {"label":"1000",  "retval":{"topic":"setup","prop":"samplingInterval","value":1000}},
            {"label":"2000",  "retval":{"topic":"setup","prop":"samplingInterval","value":2000}},
            {"label":"5000",  "retval":{"topic":"setup","prop":"samplingInterval","value":5000}},
            {"label":"10000", "retval":{"topic":"setup","prop":"samplingInterval","value":10000}}
          ]
        },
        {
          "label": "Agg period (s)",
          "type": "sel",
          "items": [
            {"label":"10",  "retval":{"topic":"setup","prop":"aggPeriodS","value":10}},
            {"label":"15",  "retval":{"topic":"setup","prop":"aggPeriodS","value":15}},
            {"label":"20",  "retval":{"topic":"setup","prop":"aggPeriodS","value":20}},
            {"label":"30",  "retval":{"topic":"setup","prop":"aggPeriodS","value":30}},
            {"label":"60",  "retval":{"topic":"setup","prop":"aggPeriodS","value":60}},
            {"label":"120", "retval":{"topic":"setup","prop":"aggPeriodS","value":120}},
            {"label":"300", "retval":{"topic":"setup","prop":"aggPeriodS","value":300}},
            {"label":"600", "retval":{"topic":"setup","prop":"aggPeriodS","value":600}}
          ]
        },
        {
          "label": "Aware timeout (s)",
          "type": "sel",
          "items": [
            {"label":"60",   "retval":{"topic":"setup","prop":"awareTimeoutS","value":60}},
            {"label":"300",  "retval":{"topic":"setup","prop":"awareTimeoutS","value":300}},
            {"label":"600",  "retval":{"topic":"setup","prop":"awareTimeoutS","value":600}},
            {"label":"900",  "retval":{"topic":"setup","prop":"awareTimeoutS","value":900}},
            {"label":"1800", "retval":{"topic":"setup","prop":"awareTimeoutS","value":1800}},
            {"label":"3600", "retval":{"topic":"setup","prop":"awareTimeoutS","value":3600}}
          ]
        },
        {
          "label": "Default sleep (s)",
          "type": "sel",
          "items": [
            {"label":"600",  "retval":{"topic":"setup","prop":"defaultSleepS","value":600}},
            {"label":"900",  "retval":{"topic":"setup","prop":"defaultSleepS","value":900}},
            {"label":"1800", "retval":{"topic":"setup","prop":"defaultSleepS","value":1800}},
            {"label":"3600", "retval":{"topic":"setup","prop":"defaultSleepS","value":3600}}
          ]
        },
        {
          "label": "Status interval (s)",
          "type": "sel",
          "items": [
            {"label":"60",   "retval":{"topic":"setup","prop":"statusIntervalS","value":60}},
            {"label":"120",  "retval":{"topic":"setup","prop":"statusIntervalS","value":120}},
            {"label":"300",  "retval":{"topic":"setup","prop":"statusIntervalS","value":300}},
            {"label":"600",  "retval":{"topic":"setup","prop":"statusIntervalS","value":600}},
            {"label":"900",  "retval":{"topic":"setup","prop":"statusIntervalS","value":900}},
            {"label":"1800", "retval":{"topic":"setup","prop":"statusIntervalS","value":1800}}
          ]
        },
        {
          "label": "Max charge current",
          "type": "sel",
          "items": [
            {"label":"250",  "retval":{"topic":"setup","prop":"maxChargingCurrent","value":250}},
            {"label":"500",  "retval":{"topic":"setup","prop":"maxChargingCurrent","value":500}},
            {"label":"750",  "retval":{"topic":"setup","prop":"maxChargingCurrent","value":750}},
            {"label":"1000", "retval":{"topic":"setup","prop":"maxChargingCurrent","value":1000}}
          ]
        },
        {
          "label": "Max charge volt",
          "type": "sel",
          "items": [
            {"label":"3.5", "retval":{"topic":"setup","prop":"maxChargingVoltage","value":3.5}},
            {"label":"3.6", "retval":{"topic":"setup","prop":"maxChargingVoltage","value":3.6}},
            {"label":"3.7", "retval":{"topic":"setup","prop":"maxChargingVoltage","value":3.7}},
            {"label":"3.8", "retval":{"topic":"setup","prop":"maxChargingVoltage","value":3.8}},
            {"label":"3.9", "retval":{"topic":"setup","prop":"maxChargingVoltage","value":3.9}},
            {"label":"4.0", "retval":{"topic":"setup","prop":"maxChargingVoltage","value":4.0}},
            {"label":"4.1", "retval":{"topic":"setup","prop":"maxChargingVoltage","value":4.1}},
            {"label":"4.2", "retval":{"topic":"setup","prop":"maxChargingVoltage","value":4.2}}
          ]
        },
        {"label":"Device name",    "type":"edit", "maxLen":47, "retval":{"topic":"setup","prop":"deviceName"}},
        {"label":"SIM pin",        "type":"edit", "maxLen":15, "retval":{"topic":"setup","prop":"simPin"}},
        {"label":"APN",            "type":"edit", "maxLen":63, "retval":{"topic":"setup","prop":"apn"}},
        {"label":"APN user",       "type":"edit", "maxLen":31, "retval":{"topic":"setup","prop":"apnUser"}},
        {"label":"APN pass",       "type":"edit", "maxLen":31, "retval":{"topic":"setup","prop":"apnPass"}},
        {"label":"MQTT host",      "type":"edit", "maxLen":63, "retval":{"topic":"setup","prop":"mqttHost"}},
        {"label":"MQTT user",      "type":"edit", "maxLen":31, "retval":{"topic":"setup","prop":"mqttUser"}},
        {"label":"MQTT pass",      "type":"edit", "maxLen":31, "retval":{"topic":"setup","prop":"mqttPass"}},
        {"label":"MQTT client id", "type":"edit", "maxLen":47, "retval":{"topic":"setup","prop":"mqttClientId"}}
      ]
    },
    {
      "label": "Send config",
      "type": "action",
      "retval": {"topic":"cmd", "value":"getConfig"}
    }
  ]
}
)JSON";
