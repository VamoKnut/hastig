#!/usr/bin/env python3
"""
Hastig single-file MQTT simulator.

Implements the core runtime behavior of the embedded node:
- MQTT topics and payload shapes compatible with Hastig-H7-1
- Command handling (/cmd) and config patching (/cfg)
- aware/sampling/hibernating state transitions
- fake sensor data + aggregation

This version can host multiple virtual nodes in parallel over one MQTT connection.
"""

import argparse
import json
import math
import signal
import sys
import time
from dataclasses import dataclass
from typing import Any, Dict, Optional

try:
    import paho.mqtt.client as mqtt
except ImportError:
    print("Missing dependency: paho-mqtt. Install with: pip install paho-mqtt", file=sys.stderr)
    raise


MIN_SAMPLE_PERIOD_MS = 200
MAX_CONFIG_PAYLOAD_BYTES = 320
CONFIG_CHUNK_TOTAL = 5

MODE_AWARE = "aware"
MODE_SAMPLING = "sampling"
MODE_HIBERNATING = "hibernating"


def now_ms() -> int:
    return int(time.monotonic() * 1000.0)


def clamp(value: float, lo: float, hi: float) -> float:
    return max(lo, min(hi, value))


def parse_u32(value: Any) -> Optional[int]:
    if isinstance(value, bool):
        return None
    if isinstance(value, int):
        if value < 0:
            return None
        return value
    if isinstance(value, float):
        if value < 0:
            return None
        return int(value)
    if isinstance(value, str):
        v = value.strip()
        if not v:
            return None
        try:
            n = float(v)
        except ValueError:
            return None
        if n < 0:
            return None
        return int(n)
    return None


def parse_f32(value: Any) -> Optional[float]:
    if isinstance(value, bool):
        return None
    if isinstance(value, (int, float)):
        return float(value)
    if isinstance(value, str):
        v = value.strip()
        if not v:
            return None
        try:
            return float(v)
        except ValueError:
            return None
    return None


def mask_if_set(text: str) -> str:
    return "***" if text else ""


@dataclass
class AppSettings:
    version: int = 1

    sensor_addr: int = 1
    sensor_baud: int = 9600

    sensor_warmup_ms: int = 4000
    sensor_type: int = 1

    sample_period_ms: int = 1000
    agg_period_s: int = 15

    sim_pin: str = "0000"
    apn: str = "telenor.smart"
    apn_user: str = ""
    apn_pass: str = ""

    mqtt_host: str = "mqtt.vamotech.no"
    mqtt_port: int = 1883
    mqtt_user: str = "guest"
    mqtt_pass: str = "guest"
    mqtt_client_id: str = "HastigClient"

    device_name: str = ""

    aware_timeout_s: int = 600
    default_sleep_s: int = 3600
    status_interval_s: int = 120
    low_batt_min_v: float = 2.8
    max_charging_current: int = 1000
    max_charging_voltage: float = 3.64
    emergency_delay_s: int = 60
    emergency_sleep_s: int = 43200
    max_forced_sleep_s: int = 43200
    max_unacked_packets: int = 10

    def clamp_runtime(self) -> None:
        if self.sample_period_ms < MIN_SAMPLE_PERIOD_MS:
            self.sample_period_ms = MIN_SAMPLE_PERIOD_MS

        if self.aware_timeout_s < 60:
            self.aware_timeout_s = 600
        if self.default_sleep_s < 60:
            self.default_sleep_s = 3600
        if self.status_interval_s < 30:
            self.status_interval_s = 120

        if self.sensor_addr <= 0 or self.sensor_addr > 247:
            self.sensor_addr = 1

        if self.max_forced_sleep_s <= 0 or self.max_forced_sleep_s > 43200:
            self.max_forced_sleep_s = 43200
        if self.emergency_sleep_s <= 0 or self.emergency_sleep_s > 43200:
            self.emergency_sleep_s = 43200


def json_len_compact(doc: Dict[str, Any]) -> int:
    return len(json.dumps(doc, separators=(",", ":"), ensure_ascii=True))


class VirtualNode:
    def __init__(
        self,
        node_index: int,
        topic_prefix: str,
        cond_amplitude: float,
        cond_period_min: float,
        fixed_temp: float,
        publish_fn,
        verbose: bool,
    ) -> None:
        self.node_index = node_index
        self.device_id = f"{node_index:024x}"
        self.topic_prefix = topic_prefix
        self.topic_cmd = f"{topic_prefix}/{self.device_id}/cmd"
        self.topic_cfg = f"{topic_prefix}/{self.device_id}/cfg"
        self.topic_data = f"{topic_prefix}/{self.device_id}/data"
        self.topic_status = f"{topic_prefix}/{self.device_id}/status"

        self._publish_fn = publish_fn
        self._verbose = verbose

        self.settings = AppSettings()
        self.settings.device_name = ""
        self.settings.clamp_runtime()

        self.cond_amplitude = max(0.0, cond_amplitude)
        self.cond_period_ms = max(1.0, cond_period_min * 60.0 * 1000.0)
        self.fixed_temp = fixed_temp

        self.boot_ms = now_ms()
        self.session_start_ms = self.boot_ms

        self.state = MODE_AWARE
        self.last_activity_ms = self.boot_ms
        self.last_status_ms = 0
        self.last_ack_ms = 0
        self.unacked_aggregate_count = 0
        self.hibernate_until_ms = 0

        self.next_sample_ms = self.boot_ms
        self.agg_window_start_wall_ms = self.boot_ms
        self.agg_t0 = 0
        self.agg_t1 = 0
        self.agg_n = 0
        self.agg_ok = True
        self.agg_k0 = "cond"
        self.agg_k1 = "temp"
        self.agg_v0_sum = 0.0
        self.agg_v0_min = 1e30
        self.agg_v0_max = -1e30
        self.agg_v1_sum = 0.0
        self.agg_v1_min = 1e30
        self.agg_v1_max = -1e30

        self.battery_voltage = 3.95
        self.minimum_voltage = self.battery_voltage
        self.battery_current = 0.0
        self.average_current = 0.0

    def log(self, msg: str) -> None:
        if self._verbose:
            print(f"[{self.device_id}] {msg}")

    def rel_ms(self, wall_ms: Optional[int] = None) -> int:
        if wall_ms is None:
            wall_ms = now_ms()
        return int(max(0, wall_ms - self.session_start_ms))

    def publish_json(self, topic: str, payload: Dict[str, Any]) -> None:
        self._publish_fn(topic, payload)

    def publish_status(self, mode: str, extra: Optional[Dict[str, Any]] = None) -> None:
        doc: Dict[str, Any] = {
            "type": "status",
            "tsMs": self.rel_ms(),
            "mode": mode,
        }
        if extra:
            doc.update(extra)
        self.publish_json(self.topic_status, doc)

    def publish_mode_change(self, mode: str, previous_mode: str) -> None:
        extra: Dict[str, Any] = {
            "type": "modeChange",
            "previousMode": previous_mode,
        }
        if mode:
            extra["mode"] = mode

        if mode == MODE_HIBERNATING:
            self.publish_status(MODE_HIBERNATING, extra)
        else:
            self.publish_status(MODE_AWARE, extra)

    def publish_hibernating(self, reason: str, expected_duration_s: int) -> None:
        extra = {
            "reason": reason,
            "expectedDuration": int(expected_duration_s),
        }
        self.publish_status(MODE_HIBERNATING, extra)

    def publish_hibernate_mode_change(self, previous_mode: str, reason: str, expected_duration_s: int) -> None:
        extra = {
            "type": "modeChange",
            "mode": MODE_HIBERNATING,
            "previousMode": previous_mode,
            "reason": reason,
            "expectedDuration": int(expected_duration_s),
        }
        self.publish_status(MODE_HIBERNATING, extra)

    def publish_awake(self) -> None:
        self.publish_status(MODE_AWARE, None)

    def enter_state(
        self,
        new_state: str,
        hibernate_reason: str = "inactivity",
        expected_duration_s: int = 0,
    ) -> None:
        previous = self.state
        changed = previous != new_state

        self.state = new_state
        self.last_activity_ms = now_ms()

        if new_state == MODE_AWARE:
            self.unacked_aggregate_count = 0
            self.last_ack_ms = 0
            if changed:
                self.publish_mode_change(MODE_AWARE, previous)
                self.log(f"mode -> aware (from {previous})")
            else:
                self.publish_awake()
                self.log("mode aware (startup)")
            return

        if new_state == MODE_SAMPLING:
            self.unacked_aggregate_count = 0
            self.last_ack_ms = now_ms()
            self.reset_aggregate_window(now_ms())
            self.next_sample_ms = now_ms()
            if changed:
                self.publish_mode_change(MODE_SAMPLING, previous)
            self.log(f"mode -> sampling (from {previous})")
            return

        if expected_duration_s <= 0:
            expected_duration_s = int(self.settings.default_sleep_s)

        self.hibernate_until_ms = now_ms() + int(expected_duration_s * 1000)
        if changed:
            self.publish_hibernate_mode_change(previous, hibernate_reason, expected_duration_s)
        else:
            self.publish_hibernating(hibernate_reason, expected_duration_s)
        self.log(
            f"mode -> hibernating reason={hibernate_reason} duration={expected_duration_s}s (from {previous})"
        )

    def reset_aggregate_window(self, wall_ms: int) -> None:
        rel = self.rel_ms(wall_ms)
        self.agg_window_start_wall_ms = wall_ms
        self.agg_t0 = rel
        self.agg_t1 = rel
        self.agg_n = 0
        self.agg_ok = True
        self.agg_k0 = "cond"
        self.agg_k1 = "temp"
        self.agg_v0_sum = 0.0
        self.agg_v0_min = 1e30
        self.agg_v0_max = -1e30
        self.agg_v1_sum = 0.0
        self.agg_v1_min = 1e30
        self.agg_v1_max = -1e30

    def fake_sensor_sample(self, wall_ms: int) -> Dict[str, Any]:
        t_rel = self.rel_ms(wall_ms)
        phase = 2.0 * math.pi * (float(t_rel) / self.cond_period_ms)
        cond = self.cond_amplitude + (self.cond_amplitude * math.cos(phase))
        cond = max(0.0, cond)
        temp = self.fixed_temp
        return {
            "relMs": t_rel,
            "k0": "cond",
            "v0": float(cond),
            "k1": "temp",
            "v1": float(temp),
            "ok": True,
        }

    def add_sample(self, sample: Dict[str, Any]) -> None:
        if self.agg_n == 0:
            self.agg_t0 = int(sample["relMs"])
            self.agg_k0 = str(sample.get("k0", "cond"))[:8]
            self.agg_k1 = str(sample.get("k1", "temp"))[:8]

        self.agg_t1 = int(sample["relMs"])
        v0 = float(sample["v0"])
        v1 = float(sample["v1"])

        self.agg_v0_sum += v0
        self.agg_v0_min = min(self.agg_v0_min, v0)
        self.agg_v0_max = max(self.agg_v0_max, v0)

        self.agg_v1_sum += v1
        self.agg_v1_min = min(self.agg_v1_min, v1)
        self.agg_v1_max = max(self.agg_v1_max, v1)

        self.agg_ok = self.agg_ok and bool(sample.get("ok", False))
        self.agg_n += 1

    def emit_aggregate_payload(self) -> Optional[Dict[str, Any]]:
        if self.agg_n <= 0:
            return None

        n = float(self.agg_n)
        payload: Dict[str, Any] = {
            "type": "data",
            "t0": int(self.agg_t0),
            "t1": int(self.agg_t1),
            "n": int(self.agg_n),
            "ok": 1 if self.agg_ok else 0,
        }

        k0 = self.agg_k0 if self.agg_k0 else "cond"
        k1 = self.agg_k1 if self.agg_k1 else "temp"

        payload[f"{k0}Avg"] = round(self.agg_v0_sum / n, 2)
        payload[f"{k0}Min"] = round(self.agg_v0_min, 2)
        payload[f"{k0}Max"] = round(self.agg_v0_max, 2)

        mul = 10.0 if k1 == "temp" else 100.0
        payload[f"{k1}Avg"] = round((self.agg_v1_sum / n) * mul) / mul
        payload[f"{k1}Min"] = round(self.agg_v1_min * mul) / mul
        payload[f"{k1}Max"] = round(self.agg_v1_max * mul) / mul

        return payload

    def update_fake_battery(self) -> None:
        if self.state == MODE_SAMPLING:
            self.battery_current = 0.26
            self.average_current = 0.21
            self.battery_voltage = clamp(self.battery_voltage - 0.0006, 2.7, 4.2)
        elif self.state == MODE_AWARE:
            self.battery_current = 0.08
            self.average_current = 0.07
            self.battery_voltage = clamp(self.battery_voltage - 0.0002, 2.7, 4.2)
        else:
            self.battery_current = 0.01
            self.average_current = 0.02

        self.minimum_voltage = min(self.minimum_voltage, self.battery_voltage)

    def publish_periodic_status_if_due(self, wall_ms: int) -> None:
        if self.state not in (MODE_AWARE, MODE_SAMPLING):
            return

        due = self.last_status_ms == 0 or (wall_ms - self.last_status_ms) > (self.settings.status_interval_s * 1000)
        if not due:
            return

        self.update_fake_battery()
        payload = {
            "type": "status",
            "mode": self.state,
            "tsMs": self.rel_ms(wall_ms),
            "batteryVoltage": round(self.battery_voltage, 3),
            "minimumVoltage": round(self.minimum_voltage, 3),
            "batteryCurrent": round(self.battery_current, 3),
            "averageCurrent": round(self.average_current, 3),
        }
        self.publish_json(self.topic_status, payload)
        self.last_status_ms = wall_ms

        if self.minimum_voltage < self.settings.low_batt_min_v:
            alert = {
                "type": "alert",
                "message": "Critically low battery detected. Emergency hibernate soon.",
                "mode": self.state,
                "minimumVoltage": round(self.minimum_voltage, 3),
            }
            self.publish_json(self.topic_status, alert)

    def handle_keep_sampling_ack(self) -> None:
        self.last_ack_ms = now_ms()
        self.unacked_aggregate_count = 0

    def apply_cfg_patch(self, doc: Dict[str, Any]) -> None:
        s = self.settings

        v = parse_u32(doc.get("sensorAddress"))
        if v is not None:
            s.sensor_addr = int(v)
        v = parse_u32(doc.get("sensorBaudrate"))
        if v is not None:
            s.sensor_baud = int(v)
        v = parse_u32(doc.get("sensorWarmupMs"))
        if v is not None:
            s.sensor_warmup_ms = int(v)
        v = parse_u32(doc.get("sensorType"))
        if v is not None:
            s.sensor_type = int(v)

        v = parse_u32(doc.get("samplingInterval"))
        if v is not None:
            s.sample_period_ms = int(v)
        v = parse_u32(doc.get("aggPeriodS"))
        if v is not None:
            s.agg_period_s = int(v)

        if isinstance(doc.get("simPin"), str):
            s.sim_pin = doc["simPin"][:15]
        if isinstance(doc.get("apn"), str):
            s.apn = doc["apn"][:63]
        if isinstance(doc.get("apnUser"), str):
            s.apn_user = doc["apnUser"][:31]
        if isinstance(doc.get("apnPass"), str):
            s.apn_pass = doc["apnPass"][:31]

        if isinstance(doc.get("mqttHost"), str):
            s.mqtt_host = doc["mqttHost"][:63]
        v = parse_u32(doc.get("mqttPort"))
        if v is not None:
            s.mqtt_port = int(v)
        if isinstance(doc.get("mqttUser"), str):
            s.mqtt_user = doc["mqttUser"][:31]
        if isinstance(doc.get("mqttPass"), str):
            s.mqtt_pass = doc["mqttPass"][:31]
        if isinstance(doc.get("mqttClientId"), str):
            s.mqtt_client_id = doc["mqttClientId"][:47]

        if isinstance(doc.get("deviceName"), str):
            s.device_name = doc["deviceName"][:47]

        v = parse_u32(doc.get("awareTimeoutS"))
        if v is not None:
            s.aware_timeout_s = int(v)
        v = parse_u32(doc.get("defaultSleepS"))
        if v is not None:
            s.default_sleep_s = int(v)
        v = parse_u32(doc.get("statusIntervalS"))
        if v is not None:
            s.status_interval_s = int(v)

        fv = parse_f32(doc.get("lowBattMinV"))
        if fv is not None:
            s.low_batt_min_v = float(fv)
        v = parse_u32(doc.get("maxChargingCurrent"))
        if v is not None:
            s.max_charging_current = int(v)
        fv = parse_f32(doc.get("maxChargingVoltage"))
        if fv is not None:
            s.max_charging_voltage = float(fv)

        v = parse_u32(doc.get("emergencyDelayS"))
        if v is not None:
            s.emergency_delay_s = int(v)
        v = parse_u32(doc.get("emergencySleepS"))
        if v is not None:
            s.emergency_sleep_s = int(v)
        v = parse_u32(doc.get("maxForcedSleepS"))
        if v is not None:
            s.max_forced_sleep_s = int(v)
        v = parse_u32(doc.get("maxUnackedPackets"))
        if v is not None:
            s.max_unacked_packets = int(v)

        s.clamp_runtime()

    def add_masked_config_fields(self, section: str) -> Dict[str, Any]:
        s = self.settings
        doc: Dict[str, Any] = {}

        include_all = section == "all"

        if include_all or section == "network":
            doc["apn"] = s.apn
            doc["simPin"] = mask_if_set(s.sim_pin)
            doc["apnUser"] = mask_if_set(s.apn_user)
            doc["apnPass"] = mask_if_set(s.apn_pass)

        if include_all or section == "mqtt":
            doc["mqttHost"] = s.mqtt_host
            doc["mqttPort"] = s.mqtt_port
            doc["mqttClientId"] = s.mqtt_client_id
            doc["mqttUser"] = mask_if_set(s.mqtt_user)
            doc["mqttPass"] = mask_if_set(s.mqtt_pass)

        if include_all or section == "device":
            doc["deviceName"] = s.device_name
            doc["sensorAddress"] = s.sensor_addr
            doc["sensorBaudrate"] = s.sensor_baud
            doc["sensorWarmupMs"] = s.sensor_warmup_ms
            doc["sensorType"] = s.sensor_type

        if include_all or section == "schedule":
            doc["samplingInterval"] = s.sample_period_ms
            doc["aggPeriodS"] = s.agg_period_s
            doc["awareTimeoutS"] = s.aware_timeout_s
            doc["defaultSleepS"] = s.default_sleep_s
            doc["statusIntervalS"] = s.status_interval_s

        if include_all or section == "power":
            doc["lowBattMinV"] = s.low_batt_min_v
            doc["maxChargingCurrent"] = s.max_charging_current
            doc["maxChargingVoltage"] = s.max_charging_voltage
            doc["emergencyDelayS"] = s.emergency_delay_s
            doc["emergencySleepS"] = s.emergency_sleep_s
            doc["maxForcedSleepS"] = s.max_forced_sleep_s
            doc["maxUnackedPackets"] = s.max_unacked_packets

        return doc

    def publish_config_snapshot(self) -> None:
        full_doc = {
            "type": "config",
            "tsMs": self.rel_ms(),
        }
        full_doc.update(self.add_masked_config_fields("all"))

        if json_len_compact(full_doc) <= MAX_CONFIG_PAYLOAD_BYTES:
            self.publish_json(self.topic_status, full_doc)
            return

        sections = [
            (1, "network"),
            (2, "mqtt"),
            (3, "device"),
            (4, "schedule"),
            (5, "power"),
        ]
        for chunk, section in sections:
            doc = {
                "type": "configChunk",
                "tsMs": self.rel_ms(),
                "chunk": chunk,
                "total": CONFIG_CHUNK_TOTAL,
                "section": section,
            }
            doc.update(self.add_masked_config_fields(section))
            self.publish_json(self.topic_status, doc)

    def handle_cmd_payload(self, payload_text: str) -> None:
        try:
            doc = json.loads(payload_text)
        except json.JSONDecodeError:
            self.log(f"invalid cmd json: {payload_text}")
            return

        if not isinstance(doc, dict):
            return

        cmd_type = str(doc.get("type", ""))
        if not cmd_type:
            return

        self.last_activity_ms = now_ms()

        if cmd_type == "nudge":
            return

        if cmd_type == "keepSampling":
            self.handle_keep_sampling_ack()
            return

        if cmd_type == "startSampling":
            v = parse_u32(doc.get("samplingInterval"))
            if v is not None:
                self.settings.sample_period_ms = int(v)
            v = parse_u32(doc.get("aggPeriodS"))
            if v is not None:
                self.settings.agg_period_s = int(v)
            self.settings.clamp_runtime()

            if isinstance(doc.get("sessionID"), str) and doc.get("sessionID"):
                pass
            self.session_start_ms = now_ms()

            self.enter_state(MODE_SAMPLING)
            return

        if cmd_type == "stopSampling":
            self.enter_state(MODE_AWARE)
            return

        if cmd_type == "getConfig":
            self.publish_config_snapshot()
            return

        if cmd_type == "hibernate":
            sleep_s = parse_u32(doc.get("sleepSeconds"))
            if sleep_s is None or sleep_s == 0:
                sleep_s = self.settings.default_sleep_s
            sleep_s = int(min(sleep_s, self.settings.max_forced_sleep_s))
            self.enter_state(MODE_HIBERNATING, "forced", sleep_s)
            return

        if cmd_type == "resetBatteryStatistics":
            self.minimum_voltage = self.battery_voltage
            return

        if cmd_type == "factoryReset":
            self.settings = AppSettings()
            self.settings.device_name = ""
            self.settings.clamp_runtime()
            return

        self.log(f"unknown command type: {cmd_type}")

    def handle_cfg_payload(self, payload_text: str) -> None:
        try:
            doc = json.loads(payload_text)
        except json.JSONDecodeError:
            self.log(f"invalid cfg json: {payload_text}")
            return

        if not isinstance(doc, dict):
            return

        self.apply_cfg_patch(doc)

    def tick(self, wall_ms: int) -> None:
        if self.state == MODE_HIBERNATING:
            if wall_ms >= self.hibernate_until_ms:
                self.enter_state(MODE_AWARE)
            return

        self.publish_periodic_status_if_due(wall_ms)

        if (wall_ms - self.last_activity_ms) > (self.settings.aware_timeout_s * 1000):
            self.enter_state(MODE_HIBERNATING, "inactivity", self.settings.default_sleep_s)
            return

        if self.state != MODE_SAMPLING:
            return

        sample_period_ms = max(self.settings.sample_period_ms, MIN_SAMPLE_PERIOD_MS)
        while wall_ms >= self.next_sample_ms:
            sample = self.fake_sensor_sample(self.next_sample_ms)
            self.add_sample(sample)
            self.next_sample_ms += sample_period_ms

        agg_window_ms = int(self.settings.agg_period_s * 1000)
        if (wall_ms - self.agg_window_start_wall_ms) >= agg_window_ms:
            aggregate_payload = self.emit_aggregate_payload()
            if aggregate_payload is not None:
                self.publish_json(self.topic_data, aggregate_payload)
                self.last_activity_ms = wall_ms
                self.unacked_aggregate_count += 1

            self.reset_aggregate_window(wall_ms)

        limit = self.settings.max_unacked_packets if self.settings.max_unacked_packets > 0 else 1
        if self.unacked_aggregate_count >= limit:
            self.log(
                f"max unacked aggregates reached ({self.unacked_aggregate_count}/{limit}), returning to aware"
            )
            self.enter_state(MODE_AWARE)


class Simulator:
    def __init__(self, args: argparse.Namespace) -> None:
        self.args = args
        self.running = True
        self.connected = False
        self.next_reconnect_ms = 0

        try:
            self.client = mqtt.Client(
                callback_api_version=mqtt.CallbackAPIVersion.VERSION2,
                client_id=args.client_id,
                clean_session=True,
            )
        except Exception:
            self.client = mqtt.Client(
                client_id=args.client_id,
                clean_session=True,
            )

        if args.username:
            self.client.username_pw_set(args.username, args.password)

        self.client.on_connect = self.on_connect
        self.client.on_disconnect = self.on_disconnect
        self.client.on_message = self.on_message

        self.nodes: Dict[str, VirtualNode] = {}
        for i in range(1, args.nodes + 1):
            node = VirtualNode(
                node_index=i,
                topic_prefix=args.topic_prefix,
                cond_amplitude=args.cond_amplitude,
                cond_period_min=args.cond_period_min,
                fixed_temp=args.temperature,
                publish_fn=self.publish_json,
                verbose=args.verbose,
            )
            self.nodes[node.device_id] = node

    def log(self, msg: str) -> None:
        print(msg)

    def _extract_node_id_from_topic(self, topic: str) -> str:
        parts = topic.split("/")
        if len(parts) == 3 and parts[0] == self.args.topic_prefix:
            return parts[1]
        return "-"

    def _log_mqtt_tx(self, topic: str, payload_text: str) -> None:
        node_id = self._extract_node_id_from_topic(topic)
        self.log(f"TX [{node_id}] {topic} {payload_text}")

    def _log_mqtt_rx(self, topic: str, payload_text: str) -> None:
        node_id = self._extract_node_id_from_topic(topic)
        self.log(f"RX [{node_id}] {topic} {payload_text}")

    def stop(self, *_args: Any) -> None:
        self.running = False

    def publish_json(self, topic: str, payload: Dict[str, Any]) -> None:
        raw = json.dumps(payload, separators=(",", ":"), ensure_ascii=True)
        self._log_mqtt_tx(topic, raw)
        info = self.client.publish(topic, payload=raw, qos=self.args.qos, retain=self.args.retain)
        if info.rc != mqtt.MQTT_ERR_SUCCESS and self.args.verbose:
            self.log(f"publish failed rc={info.rc} topic={topic}")

    def on_connect(self, client: mqtt.Client, _userdata: Any, _flags: Any, reason_code: Any = 0, _properties: Any = None) -> None:
        self.connected = True
        self.log(f"MQTT connected: rc={reason_code}")

        sub_cmd = f"{self.args.topic_prefix}/+/cmd"
        sub_cfg = f"{self.args.topic_prefix}/+/cfg"
        client.subscribe(sub_cmd, qos=self.args.qos)
        client.subscribe(sub_cfg, qos=self.args.qos)
        self.log(f"Subscribed: {sub_cmd}")
        self.log(f"Subscribed: {sub_cfg}")

        for node in self.nodes.values():
            node.enter_state(MODE_AWARE)

    def on_disconnect(
        self,
        _client: mqtt.Client,
        _userdata: Any,
        _disconnect_flags: Any = None,
        reason_code: Any = 0,
        _properties: Any = None,
    ) -> None:
        if reason_code == 0 and isinstance(_disconnect_flags, (int, str)):
            reason_code = _disconnect_flags
        self.connected = False
        self.log(f"MQTT disconnected: rc={reason_code}")

    def on_message(self, _client: mqtt.Client, _userdata: Any, msg: mqtt.MQTTMessage) -> None:
        topic = msg.topic if isinstance(msg.topic, str) else msg.topic.decode("utf-8", errors="ignore")
        payload_text = msg.payload.decode("utf-8", errors="ignore")
        self._log_mqtt_rx(topic, payload_text)

        parts = topic.split("/")
        if len(parts) != 3:
            return
        prefix, node_id, postfix = parts
        if prefix != self.args.topic_prefix:
            return

        node = self.nodes.get(node_id)
        if node is None:
            return

        if postfix == "cmd":
            node.handle_cmd_payload(payload_text)
        elif postfix == "cfg":
            node.handle_cfg_payload(payload_text)

    def connect(self) -> None:
        self.client.connect(self.args.broker, self.args.port, keepalive=self.args.keepalive)

    def try_reconnect(self) -> None:
        now = now_ms()
        if now < self.next_reconnect_ms:
            return
        try:
            self.log("Attempting MQTT reconnect...")
            self.client.reconnect()
            self.next_reconnect_ms = now + 2000
        except Exception as ex:
            self.log(f"Reconnect failed: {ex}")
            self.next_reconnect_ms = now + 3000

    def run(self) -> int:
        self.connect()
        self.log(
            f"Simulator started: nodes={self.args.nodes} "
            f"id_range=000000000000000000000001..{self.args.nodes:024x} "
            f"cond_amp={self.args.cond_amplitude} cond_period_min={self.args.cond_period_min} temp={self.args.temperature}"
        )

        while self.running:
            rc = self.client.loop(timeout=0.01)
            if rc != mqtt.MQTT_ERR_SUCCESS:
                if self.args.verbose:
                    self.log(f"loop rc={rc}")
                self.try_reconnect()

            tick_ms = now_ms()
            for node in self.nodes.values():
                node.tick(tick_ms)

            time.sleep(self.args.tick_ms / 1000.0)

        try:
            self.client.disconnect()
        except Exception:
            pass
        return 0


def build_arg_parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(description="Hastig MQTT virtual node simulator (single-file, multi-node)")

    p.add_argument("--broker", default="127.0.0.1", help="MQTT broker host")
    p.add_argument("--port", type=int, default=1883, help="MQTT broker port")
    p.add_argument("--username", default="", help="MQTT username")
    p.add_argument("--password", default="", help="MQTT password")
    p.add_argument("--keepalive", type=int, default=30, help="MQTT keepalive seconds")

    p.add_argument("--topic-prefix", default="hastigNode", help="MQTT topic prefix")
    p.add_argument("--client-id", default="HastigPythonSimulator", help="MQTT client ID")
    p.add_argument("--qos", type=int, choices=[0, 1], default=0, help="MQTT QoS")
    p.add_argument("--retain", action="store_true", help="Publish retained messages")

    p.add_argument("--nodes", type=int, default=1, help="Number of virtual nodes to simulate in parallel")

    p.add_argument(
        "--cond-amplitude",
        type=float,
        default=1.0,
        help="Conductivity cosine amplitude (always positive waveform)",
    )
    p.add_argument(
        "--cond-period-min",
        type=float,
        default=10.0,
        help="Conductivity cosine period in minutes",
    )
    p.add_argument(
        "--temperature",
        type=float,
        default=17.5,
        help="Fixed temperature value used for all fake samples",
    )

    p.add_argument("--tick-ms", type=int, default=50, help="Main simulation loop period (ms)")
    p.add_argument("--verbose", action="store_true", help="Verbose logging")

    return p


def validate_args(args: argparse.Namespace) -> None:
    if args.nodes < 1:
        raise SystemExit("--nodes must be >= 1")
    if args.cond_amplitude < 0.0:
        raise SystemExit("--cond-amplitude must be >= 0")
    if args.cond_period_min <= 0.0:
        raise SystemExit("--cond-period-min must be > 0")
    if args.tick_ms < 1:
        raise SystemExit("--tick-ms must be >= 1")


def main() -> int:
    parser = build_arg_parser()
    args = parser.parse_args()
    validate_args(args)

    sim = Simulator(args)
    signal.signal(signal.SIGINT, sim.stop)
    signal.signal(signal.SIGTERM, sim.stop)
    return sim.run()


if __name__ == "__main__":
    raise SystemExit(main())
