#pragma once


#include <mbed.h>
#include <stdint.h>

/**
 * @brief Hastig compile-time configuration.
 */

// ---------------- Pins ----------------
static constexpr int PIN_BTN_LEFT  = D5;
static constexpr int PIN_BTN_UP = D4;
static constexpr int PIN_BTN_DOWN    = D2;
static constexpr int PIN_BTN_RIGHT  = D1;

// RS485 / sensor power
static constexpr int PIN_RS485_DE_RE  = D6;
static constexpr int PIN_POWER_ENABLE = D3;

// UART pins (Portenta)
static constexpr int PIN_RS485_RX = D13;
static constexpr int PIN_RS485_TX = D14;

// ---------------- Thread priorities ----------------
static constexpr osPriority PRIO_ORCH  = osPriorityNormal;
static constexpr osPriority PRIO_COMMS = osPriorityAboveNormal;
static constexpr osPriority PRIO_AGG   = osPriorityNormal;
static constexpr osPriority PRIO_SENS  = osPriorityNormal;
static constexpr osPriority PRIO_UI    = osPriorityLow;

// ---------------- Thread stacks ----------------
static constexpr uint32_t STACK_ORCH  = 6 * 1024;
static constexpr uint32_t STACK_COMMS = 14 * 1024;
static constexpr uint32_t STACK_AGG   = 6 * 1024;
static constexpr uint32_t STACK_SENS  = 14 * 1024;
static constexpr uint32_t STACK_UI    = 6 * 1024;

// ---------------- Mail queue depths ----------------
static constexpr uint32_t QUEUE_DEPTH_SENSOR_TO_AGG = 32;
static constexpr uint32_t QUEUE_DEPTH_AGG_TO_COMMS  = 16;

static constexpr uint32_t QUEUE_DEPTH_UI_TO_ORCH      = 16;
static constexpr uint32_t QUEUE_DEPTH_COMMS_TO_ORCH   = 16;
static constexpr uint32_t QUEUE_DEPTH_WORKER_TO_ORCH  = 8;
static constexpr uint32_t QUEUE_DEPTH_ORCH_TO_COMMS   = 16;

// ---------------- MQTT topics ----------------
static constexpr const char* MQTT_TOPIC_PREFIX = "hastigNode";
static constexpr const char* MQTT_TOPIC_POSTFIX_CMD = "cmd";
static constexpr const char* MQTT_TOPIC_POSTFIX_CFG = "cfg";
static constexpr uint32_t MIN_SAMPLE_PERIOD_MS = 200;

// Grace time after publishing final status before hibernate.
static constexpr uint32_t HIBERNATE_STATUS_GRACE_MS = 1500;
// Comms boot gating
static constexpr uint32_t HASTIG_COMMS_READY_GRACE_MS = 30000UL;
static constexpr uint32_t HASTIG_MQTT_CONNECT_TIMEOUT_MS = 120000UL;
static constexpr uint32_t HASTIG_NO_NETWORK_HIBERNATE_S = 900UL;
