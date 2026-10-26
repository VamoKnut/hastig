#pragma once


#include <mbed.h>
#include <stdint.h>

/**
 * @brief Hastig compile-time configuration.
 */
static constexpr const char* HASTIG_AI_REVISION = "v158";

// ---------------- Pins ----------------
static constexpr PinName PIN_BTN_LEFT = PC_13;
static constexpr PinName PIN_BTN_UP = PC_15;
static constexpr PinName PIN_BTN_DOWN = PD_4;
static constexpr PinName PIN_BTN_RIGHT = PD_5;
static constexpr PinName PIN_WAKE = PC_13;

// Serial
static constexpr PinName PIN_RS485_DE_RE = PA_8;
static constexpr PinName PIN_RS485_RX = PA_10;
static constexpr PinName PIN_RS485_TX = PA_9;
static constexpr PinName PIN_232_TX = PG_14;
static constexpr PinName PIN_232_RX = PG_9;
static constexpr PinName PIN_232_Enable = PC_3C;
static constexpr PinName PIN_232_ForceOn = PA_1C;
static constexpr PinName PIN_232_Valid = PI_0;

// Power control
static constexpr PinName PIN_POWER_ENABLE = PG_7;
static constexpr PinName PIN_ExtPwr1_Disable = PI_4;
static constexpr PinName PIN_ExtPwr2_Disable = PI_6;
static constexpr PinName PIN_ExtPwr1_Monitor = PA_4;
static constexpr PinName PIN_ExtPwr2_Monitor = PC_3;

// Motor control
static constexpr PinName PIN_MOTORA_Enable = PG_3;
static constexpr PinName PIN_MOTORA_In1 = PC_6;
static constexpr PinName PIN_MOTORA_In2 = PC_7;
static constexpr PinName PIN_MOTORB_Enable = PG_10;
static constexpr PinName PIN_MOTORB_In1 = PJ_11;
static constexpr PinName PIN_MOTORB_In2 = PK_1;
static constexpr PinName PIN_MOTORA_Disabled = PC_2C;
static constexpr PinName PIN_MOTORB_Disabled = PA_1C;


// Other board I/O

static constexpr PinName PIN_DISPLAY_SDA = PH_8;
static constexpr PinName PIN_DISPLAY_SCL = PH_7;
static constexpr PinName PIN_LED_RED = PK_5;
static constexpr PinName PIN_LED_GREEN = PK_6;
static constexpr PinName PIN_LED_BLUE = PK_7;
static constexpr PinName PIN_SESSION_ENTROPY_A = PA_0C;
static constexpr PinName PIN_SESSION_ENTROPY_B = PA_1C;

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
static constexpr uint32_t HASTIG_COMMS_READY_GRACE_MS = 10000UL;
static constexpr uint32_t HASTIG_MQTT_CONNECT_TIMEOUT_MS = 120000UL;
static constexpr uint32_t HASTIG_NO_NETWORK_HIBERNATE_S = 900UL;

// Hardware watchdog. IWDG cannot be stopped once started, so require the
// STM32 option byte that freezes IWDG in standby before enabling it.
static constexpr bool HASTIG_WATCHDOG_ENABLE = true;
static constexpr bool HASTIG_WATCHDOG_PREPARE_OPTION_BYTES = true;
static constexpr bool HASTIG_WATCHDOG_REQUIRE_STANDBY_FREEZE = true;
static constexpr uint32_t HASTIG_WATCHDOG_TIMEOUT_MS = 30000UL;
static constexpr uint32_t HASTIG_WATCHDOG_LONG_OP_KICK_MS = 10000UL;
static constexpr uint32_t HASTIG_CELLULAR_CONNECT_WATCHDOG_GRACE_MS = 180000UL;
