#!/usr/bin/env python3
"""Standalone two-motor wrist console, plots and telemetry recorder.

Importing this module opens no serial port and imports no optional dependencies.
Movement is possible only through an explicitly entered command after connection.
"""
from __future__ import annotations

import argparse
import csv
from collections import deque
from datetime import datetime, timezone
import json
import math
from pathlib import Path
import queue
import sys
import threading
import time

from wrist_motion import (Cycle, Step, MotionProfile, MotionCancelled, MotionRunner, run_cycle,
                          run_move, wait_for_target)

MOTOR_FLOAT_FIELDS = ("target_deg", "position_deg", "error_deg", "velocity_deg_s", "pid_percent", "pwm_percent")
MOTOR_BOOL_FIELDS = ("encoder_available", "calibrated", "zeroed", "feedback_valid")
MOTOR_FIELDS = MOTOR_FLOAT_FIELDS + ("raw", "counts", "delta") + MOTOR_BOOL_FIELDS + ("goal_deg",)
FAULT_FLOAT_FIELDS = ("dt_s", "velocity_deg_s", "velocity_limit_deg_s", "position_deg", "target_deg", "pwm_percent")
FAULT_FIELDS = ("t_ms", "command_active") + FAULT_FLOAT_FIELDS + ("raw", "delta", "counts")
CSV_FIELDS = ("host_utc", "host_monotonic_s", "t_ms", "state", "armed", "target_reached", "dt_s", "fault", "fault_motor") + tuple(
    f"fault_{field}" for field in FAULT_FIELDS) + tuple(
    f"m{i}_{field}" for i in (1, 2) for field in MOTOR_FIELDS)


def finite(value):
    return type(value) in (int, float) and math.isfinite(value)


def parse_packet(line):
    def reject_constant(value):
        raise ValueError(f"invalid JSON constant: {value}")
    p = json.loads(line, parse_constant=reject_constant)
    if not isinstance(p, dict):
        raise ValueError("packet must be an object")
    kind = p.get("type")
    if kind == "reply":
        if not isinstance(p.get("command"), str) or type(p.get("ok")) is not bool or not isinstance(p.get("message"), str):
            raise ValueError("malformed reply")
    elif kind in ("telemetry", "status"):
        if p.get("firmware") != "stm32-wrist-bench/1":
            raise ValueError("wrong firmware: flash this project's wrist-bench.elf")
        if type(p.get("t_ms")) is not int or not 0 <= p["t_ms"] <= 0xffffffff:
            raise ValueError("invalid timestamp")
        if p.get("state") not in ("disarmed", "armed", "moving", "holding", "fault"):
            raise ValueError("invalid state")
        if type(p.get("armed")) is not bool or type(p.get("target_reached")) is not bool or not finite(p.get("dt_s")):
            raise ValueError("invalid flags or timing")
        if not isinstance(p.get("fault"), str) or type(p.get("fault_motor")) is not int or p["fault_motor"] not in (0, 1, 2):
            raise ValueError("invalid fault")
        if not isinstance(p.get("motors"), list) or len(p["motors"]) != 2:
            raise ValueError("two motor measurements required")
        for i, m in enumerate(p["motors"], 1):
            if not isinstance(m, dict) or type(m.get("id")) is not int or m["id"] != i:
                raise ValueError("invalid motor ID/order")
            for key in MOTOR_FLOAT_FIELDS:
                if key not in m or (m[key] is None and key in ("pid_percent", "pwm_percent")) or (m[key] is not None and not finite(m[key])):
                    raise ValueError(f"invalid {key}")
            for key in MOTOR_BOOL_FIELDS:
                if type(m.get(key)) is not bool:
                    raise ValueError(f"invalid {key}")
            if "goal_deg" in m and (p["armed"] or m["goal_deg"] is not None) and not finite(m["goal_deg"]):
                raise ValueError("invalid goal_deg")
            for key in ("raw", "counts", "delta"):
                if type(m.get(key)) is not int:
                    raise ValueError(f"invalid {key}")
            if not 0 <= m["raw"] <= (0xffffffff if i == 1 else 0xffff) or not -(1 << 63) <= m["counts"] < (1 << 63) or not -(1 << 31) <= m["delta"] < (1 << 31):
                raise ValueError("counter out of range")
            if m["feedback_valid"] and (not m["encoder_available"] or not m["calibrated"] or m["position_deg"] is None or m["velocity_deg_s"] is None):
                raise ValueError("inconsistent feedback")
            if p["armed"] and (not m["feedback_valid"] or not m["zeroed"] or m["target_deg"] is None):
                raise ValueError("armed without calibrated zeroed feedback")
        if p["armed"] != (p["state"] in ("armed", "moving", "holding")) or (p["fault"] != "none" and p["armed"]):
            raise ValueError("inconsistent armed state")
        detail = p.get("fault_detail")  # Optional extension: old firmware remains readable.
        if detail is not None:
            if not isinstance(detail, dict) or p["fault"] == "none" or p["fault_motor"] not in (1, 2):
                raise ValueError("invalid fault detail")
            if type(detail.get("command_active")) is not bool:
                raise ValueError("invalid fault command flag")
            for field, low, high in (("t_ms", 0, 0xffffffff), ("raw", 0, 0xffffffff if p["fault_motor"] == 1 else 0xffff),
                                     ("delta", -(1 << 31), (1 << 31) - 1), ("counts", -(1 << 63), (1 << 63) - 1)):
                if type(detail.get(field)) is not int or not low <= detail[field] <= high:
                    raise ValueError(f"invalid fault {field}")
            for field in FAULT_FLOAT_FIELDS:
                if field not in detail or (detail[field] is not None and not finite(detail[field])):
                    raise ValueError(f"invalid fault {field}")
    else:
        raise ValueError("unknown packet type")
    return p



class LineDecoder:
    """Bounded byte decoder. Overlong/invalid lines never become commands."""
    def __init__(self, maximum=8192, synchronize=False):
        self.maximum = maximum
        self.buffer = bytearray()
        self.dropping = False
        self.errors = 0
        self.last_error = None
        self.synchronize = synchronize
        self.discarded_prefix = False

    def _invalid(self, reason):
        self.errors += 1
        self.last_error = reason

    def _packet(self):
        try:
            line = self.buffer.decode("ascii")
            # Only a syntactically incomplete first line can be a connection
            # starting in the middle of the board's existing telemetry stream.
            json.loads(line)
        except (ValueError, UnicodeError) as exc:
            if self.synchronize and not self.buffer.lstrip().startswith(b"{"):
                self.synchronize = False
                self.discarded_prefix = True
            else:
                self._invalid(f"{type(exc).__name__}: {exc}; début={bytes(self.buffer[:96])!r}")
            return None
        self.synchronize = False
        try:
            return parse_packet(line)
        except ValueError as exc:
            # Wrong firmware and semantically invalid full packets stay fatal,
            # including at startup; no relaxation after the first valid line.
            self._invalid(f"{exc}; début={line[:96]!r}")
            return None

    def feed(self, data):
        packets = []
        for byte in data:
            if byte == 10:
                if not self.dropping and self.buffer:
                    packet = self._packet()
                    if packet is not None:
                        packets.append(packet)
                self.buffer.clear()
                self.dropping = False
            elif not self.dropping:
                if len(self.buffer) >= self.maximum:
                    self.buffer.clear()
                    self.dropping = True
                    self._invalid(f"trame supérieure à {self.maximum} octets")
                    self.synchronize = False
                else:
                    self.buffer.append(byte)
        return packets


class CSVRecorder:
    def __init__(self, path):
        self.file = open(path, "w", newline="", encoding="utf-8")
        self.writer = csv.DictWriter(self.file, fieldnames=CSV_FIELDS, extrasaction="ignore")
        self.writer.writeheader()
        self.lock = threading.Lock()

    def write(self, packet):
        row = {key: value for key, value in packet.items() if key not in ("motors", "fault_detail")}
        detail = packet.get("fault_detail") or {}
        row.update({f"fault_{field}": detail.get(field) for field in FAULT_FIELDS})
        row.update(host_utc=datetime.now(timezone.utc).isoformat(), host_monotonic_s=f"{time.monotonic():.6f}")
        for m in packet["motors"]:
            row.update({f"m{m['id']}_{key}": m.get(key) for key in MOTOR_FIELDS})
        with self.lock:
            self.writer.writerow(row)
            self.file.flush()

    def close(self):
        with self.lock:
            self.file.close()


class CommandRejected(RuntimeError):
    pass


class SerialSession:
    """One reader/heartbeat thread and serialized complete writes.

    Serial must have bounded read and write timeouts. An I/O failure ends the
    session permanently; reconnection requires a new object and never rearms.
    """
    def __init__(self, serial_port, recorder=None, heartbeat_s=0.1):
        self.serial = serial_port
        self.recorder = recorder
        self.heartbeat_s = heartbeat_s
        self.decoder = LineDecoder(synchronize=True)
        self.write_lock = threading.Lock()
        self.command_lock = threading.Lock()
        self.condition = threading.Condition()
        self.stop_event = threading.Event()
        self.failure = None
        self.sequence = 0
        self.replies = deque(maxlen=100)
        self.samples = deque(maxlen=100000)
        self.events = queue.Queue(maxsize=100)
        self.latest = None
        self.latest_status = None
        self.latest_index = 0
        self.thread = threading.Thread(target=self._run, name="bench-serial", daemon=True)

    def start(self):
        self.thread.start()

    def _write(self, data, cancel=None):
        with self.write_lock:
            if cancel is not None and cancel.is_set():
                raise MotionCancelled("Opération arrêtée.")
            if self.stop_event.is_set():
                raise ConnectionError(self.failure or "serial session closed")
            count = self.serial.write(data)
            if count != len(data):
                raise ConnectionError("incomplete serial write")

    def send(self, command, cancel=None):
        if "\n" in command or "\r" in command or not command.isascii() or len(command) > 190:
            raise ValueError("command must be one ASCII line of at most 190 characters")
        self._write((command + "\n").encode("ascii"), cancel)

    def _run(self):
        next_ping = 0.0
        try:
            while not self.stop_event.is_set():
                now = time.monotonic()
                if now >= next_ping:
                    self.send("ping")
                    next_ping = now + self.heartbeat_s
                data = self.serial.read(1024)
                packets = self.decoder.feed(data)
                if self.decoder.errors:
                    raise ValueError(f"invalid telemetry/framing: {self.decoder.last_error}; session stopped")
                for packet in packets:
                    packet = dict(packet, host_monotonic_s=time.monotonic())
                    with self.condition:
                        self.sequence += 1
                        if packet["type"] == "reply":
                            self.replies.append((self.sequence, packet))
                            if packet["command"] == "fault" and not packet["ok"]:
                                self._event({"type": "fault", "message": packet["message"]})
                            if packet["command"] != "ping":
                                self._event(packet)
                        else:
                            previous_fault = self.latest.get("fault") if self.latest else None
                            self.latest = packet
                            self.latest_index = self.sequence
                            if packet["fault"] != previous_fault and packet["fault"] not in ("none", "NONE", ""):
                                self._event({"type": "fault", "message": packet["fault"]})
                            if packet["type"] == "telemetry":
                                self.samples.append(dict(packet))
                                if self.recorder:
                                    self.recorder.write(packet)
                            else:
                                self.latest_status = packet
                                self._event(packet)
                        self.condition.notify_all()
        except Exception as exc:
            self.failure = f"{type(exc).__name__}: {exc}"
            self._event({"type": "disconnect", "message": self.failure})
        finally:
            try:
                self.emergency_stop()
            except Exception:
                pass
            self.stop_event.set()
            with self.condition:
                self.condition.notify_all()

    def _event(self, packet):
        try:
            self.events.put_nowait(packet)
        except queue.Full:
            pass

    def command(self, text, timeout=1.5, cancel=None):
        """Wait for the reply to this command, ignoring telemetry/heartbeats."""
        name = text.split()[0] if text.split() else ""
        if not name:
            raise ValueError("empty command")
        with self.command_lock:
            with self.condition:
                after = self.sequence
            self.send(text, cancel)
            deadline = time.monotonic() + timeout
            with self.condition:
                while True:
                    for index, packet in self.replies:
                        if index > after and packet["command"] in (name, "invalid", "busy"):
                            # Drain this ACK even after cancellation. Otherwise a
                            # late reply could acknowledge the next same-name command.
                            if cancel is not None and cancel.is_set():
                                raise MotionCancelled("Opération arrêtée.")
                            if not packet["ok"]:
                                raise CommandRejected(packet["message"])
                            return packet
                    if self.stop_event.is_set():
                        raise ConnectionError(self.failure or "session closed")
                    remaining = deadline - time.monotonic()
                    if remaining <= 0:
                        try:
                            self.emergency_stop()
                        finally:
                            self.stop_event.set()
                        raise TimeoutError(f"no reply to {name}; both motors stopped, reopen session")
                    self.condition.wait(min(remaining, .1) if cancel is not None else remaining)

    def emergency_stop(self):
        self._write(b"!\nstop\ndisarm\n")

    def priority_stop(self):
        self._write(b"stop\ndisarm\n")

    def snapshot(self, cancel=None):
        self.command("status", cancel=cancel)
        with self.condition:
            if self.latest_status is None:
                raise RuntimeError("no status snapshot received")
            return dict(self.latest_status)

    def current(self):
        with self.condition:
            return (dict(self.latest) if self.latest else None, self.latest_index)

    def sample_copy(self):
        with self.condition:
            return list(self.samples)

    def close(self):
        if not self.stop_event.is_set():
            try:
                self.priority_stop()
                self.command("disarm", timeout=0.3)
            except Exception:
                pass
        self.stop_event.set()
        if self.thread.ident is not None:
            self.thread.join(timeout=1.0)
        self.serial.close()



import bench_config as config


def motion_profile():
    return MotionProfile(config.MOTION_SPEED_DEG_S, config.MOTION_ACCEL_DEG_S2)


def fault_diagnostic(packet, board_status=None):
    detail = packet.get("fault_detail")
    if not detail:
        return "Mesure au déclenchement indisponible pour ce défaut."
    def value(field, unit=""):
        number = detail.get(field)
        return "indisponible" if number is None else f"{number:.4g}{unit}"
    action = "consigne active" if detail["command_active"] else "aucune consigne moteur active"
    if packet.get("fault") != "velocity_limit":
        target, position = detail.get("target_deg"), detail.get("position_deg")
        error = f"{abs(target - position):.4g}°" if finite(target) and finite(position) else "indisponible"
        message = (f"Au déclenchement t={detail['t_ms']} ms : cible {value('target_deg', '°')}, "
                   f"position {value('position_deg', '°')}, erreur {error}, "
                   f"PWM {value('pwm_percent', '%')}, {action}.")
        if packet.get("fault") == "stall":
            motor = packet.get("fault_motor", 0) - 1
            for source in (packet, board_status):
                motors = source.get("motors", []) if source else []
                cfg = motors[motor].get("configuration", {}) if 0 <= motor < len(motors) else {}
                safety, pid = cfg.get("safety"), cfg.get("pid")
                if isinstance(safety, (tuple, list)) and len(safety) == 7 and isinstance(pid, (tuple, list)) and len(pid) == 7:
                    message += (f" Critère stall chargé : progression < {safety[2]:g}° sur {safety[3] / 1000:g} s, "
                                f"avec PWM ≥ {safety[1]:g}% et erreur > {pid[6]:g}°.")
                    break
        return message
    return (f"Au déclenchement t={detail['t_ms']} ms : vitesse {value('velocity_deg_s', '°/s')}, "
            f"limite {value('velocity_limit_deg_s', '°/s')}, Δ={detail['delta']} comptes "
            f"sur {value('dt_s', ' s')}, PWM {value('pwm_percent', '%')}, {action}.")


def step_metrics(samples, target=None, initial=None, step_time_s=None, tolerance_deg=None):
    """Metrics from one actual step record (or explicitly synthetic unit data).

    Times use rollover-aware device milliseconds. A settling time is reported
    only when all later samples stay in the band for at least 0.2 s. Missing
    response/crossings produce None, never a made-up performance value.
    """
    good = [s for s in samples if s.get("feedback_valid", True) and
            finite(s.get("position_deg")) and finite(s.get("target_deg")) and
            finite(s.get("t_ms"))]
    if len(good) < 3:
        return {"available": False, "reason": "at least three valid measured samples required"}
    times = [0.0]
    for a, b in zip(good, good[1:]):
        delta = (int(b["t_ms"]) - int(a["t_ms"])) & 0xFFFFFFFF
        if delta == 0 or delta > 1000:
            return {"available": False, "reason": "duplicate timestamps or gap over one second"}
        times.append(times[-1] + delta / 1000.0)
    start = 0
    if step_time_s is not None:
        start = next((i for i, t in enumerate(times) if t >= step_time_s), len(times))
    elif target is None:
        changes = [i for i in range(1, len(good))
                   if abs(good[i]["target_deg"] - good[i - 1]["target_deg"]) > 1e-7]
        if len(changes) != 1:
            return {"available": False, "reason": "one target step required; crop CSV or supply step time"}
        start = changes[0]
    if start >= len(good) - 2:
        return {"available": False, "reason": "insufficient samples after the step"}
    before = good[max(0, start - 1)]["position_deg"] if initial is None else initial
    final = good[start]["target_deg"] if target is None else target
    if not finite(before) or not finite(final) or abs(final - before) < 1e-9:
        return {"available": False, "reason": "nonzero finite step amplitude required"}
    amplitude = final - before
    sign = 1.0 if amplitude > 0 else -1.0
    magnitude = abs(amplitude)
    measured = [s["position_deg"] for s in good[start:]]
    relative_t = [t - times[start] for t in times[start:]]
    progress = [sign * (p - before) / magnitude for p in measured]
    def crossing(level):
        for i, value in enumerate(progress):
            if value >= level:
                if i == 0:
                    return relative_t[0]
                earlier = progress[i - 1]
                fraction = (level - earlier) / (value - earlier)
                return relative_t[i - 1] + fraction * (relative_t[i] - relative_t[i - 1])
        return None
    t10, t90 = crossing(0.1), crossing(0.9)
    band = max(0.02 * magnitude, 1e-8) if tolerance_deg is None else tolerance_deg
    if not finite(band) or band <= 0:
        raise ValueError("settling tolerance must be positive and finite")
    last_outside = max((i for i, p in enumerate(measured)
                        if abs(final - p) > band and
                        not math.isclose(abs(final - p), band, rel_tol=1e-9, abs_tol=1e-12)), default=-1)
    settle_index = last_outside + 1
    settled = (settle_index < len(measured) and
               relative_t[-1] - relative_t[settle_index] >= 0.2)
    tail = [p for t, p in zip(relative_t, measured)
            if t >= max(0.0, relative_t[-1] - min(0.5, relative_t[-1] * 0.2))]
    return {"available": True, "sample_count": len(measured), "duration_s": relative_t[-1],
            "initial_deg": before, "target_deg": final, "settling_band_deg": band,
            "rise_time_10_90_s": None if t10 is None or t90 is None else t90 - t10,
            "settling_time_s": relative_t[settle_index] if settled else None,
            "overshoot_percent": max(0.0, (max(progress) - 1.0) * 100.0),
            "steady_state_error_deg": final - sum(tail) / len(tail),
            "maximum_absolute_error_deg": max(abs(final - p) for p in measured)}



def configuration_commands(calibration=False):
    """Validate the entire editable profile before sending its first command."""
    commands, missing = [], []
    for i in (1, 2):
        m = config.MOTORS[i]
        if calibration:
            if type(m.get("ppr")) is not int or m["ppr"] <= 0:
                missing.append(f"M{i}.ppr")
            if m.get("encoder_location") not in ("motor", "output"):
                missing.append(f"M{i}.encoder_location")
            if type(m.get("encoder_sign")) is not int or m["encoder_sign"] not in (-1, 1):
                missing.append(f"M{i}.encoder_sign")
            if not finite(m.get("gear_ratio")) or m["gear_ratio"] < 1:
                missing.append(f"M{i}.gear_ratio")
            commands.append(f"calibrate {i} {m.get('ppr')} 4 {m.get('gear_ratio')} {m.get('encoder_sign')} {m.get('encoder_location')}")
        else:
            for name, length, integer_fields in (("pid", 7, ()), ("safety", 7, (0, 3, 6)), ("limits", 5, (4,))):
                values = m.get(name)
                if (not isinstance(values, (tuple, list)) or len(values) != length or
                        not all(finite(x) for x in values) or
                        any(type(values[j]) is not int for j in integer_fields)):
                    missing.append(f"M{i}.{name}")
                else:
                    commands.append(f"{name} {i} " + " ".join(str(x) for x in values))
    if missing:
        raise ValueError("Compléter bench_config.py : " + ", ".join(missing))
    return commands


def apply_configuration(session, calibration=False, cancel=None):
    commands = configuration_commands(calibration)
    status = session.snapshot(cancel=cancel)
    if status["armed"] or status["fault"] != "none":
        raise ValueError("stop / clear_fault before changing configuration")
    for command in commands:
        session.command(command, cancel=cancel)
    print("Calibration applied: position the wrist at neutral, then zero." if calibration else "PID, safety and limits applied. Use status to inspect, then arm explicitly.")


def draw_samples(axes, samples):
    if samples:
        end = samples[-1]["host_monotonic_s"]
        samples = [s for s in samples if end - s["host_monotonic_s"] <= config.PLOT_HISTORY_SECONDS]
    for i in range(2):
        for row in range(4):
            axes[row][i].clear()
            axes[row][i].grid(True, alpha=.3)
        axes[0][i].set_title(f"M{i+1} - gearbox output")
        if not samples:
            axes[0][i].set_ylabel("Position (deg; requires calibration)")
            continue
        times = [s["host_monotonic_s"] - samples[0]["host_monotonic_s"] for s in samples]
        motors = [s["motors"][i] for s in samples]
        def series(key):
            return [float("nan") if m.get(key) is None else m[key] for m in motors]
        if any(m["position_deg"] is not None for m in motors):
            axes[0][i].plot(times, series("position_deg"), label="Measured")
            axes[0][i].plot(times, series("target_deg"), "--", label="Target")
            axes[0][i].set_ylabel("Output position (deg)")
        else:
            axes[0][i].plot(times, series("counts"), label="Raw counts - uncalibrated")
            axes[0][i].set_ylabel("Encoder counts")
        axes[1][i].plot(times, series("error_deg"))
        axes[1][i].set_ylabel("Error (deg)")
        axes[2][i].plot(times, series("pid_percent"), "--", label="PID")
        axes[2][i].plot(times, series("pwm_percent"), label="Applied")
        axes[2][i].set_ylabel("Signed PWM (%)")
        axes[3][i].plot(times, series("velocity_deg_s"))
        axes[3][i].set_ylabel("Velocity (deg/s)")
        axes[3][i].set_xlabel("Host elapsed time (s)")
        axes[0][i].legend(loc="upper left")
        axes[2][i].legend(loc="upper left")


def save_plot(samples, path):
    from matplotlib.figure import Figure
    from matplotlib.backends.backend_agg import FigureCanvasAgg
    fig = Figure(figsize=(13, 10))
    FigureCanvasAgg(fig)
    draw_samples(fig.subplots(4, 2, sharex=True), samples)
    fig.tight_layout()
    fig.savefig(path, dpi=140)


def read_csv(path):
    samples = []
    with open(path, newline="", encoding="utf-8") as source:
        for row in csv.DictReader(source):
            p = {"host_monotonic_s": float(row["host_monotonic_s"]), "t_ms": int(row["t_ms"]), "motors": []}
            for i in (1, 2):
                m = {"id": i}
                for key in MOTOR_FIELDS:
                    column = f"m{i}_{key}"
                    value = row.get(column, "") if key == "goal_deg" else row[column]
                    m[key] = (value == "True") if key in MOTOR_BOOL_FIELDS else (
                        None if value == "" else int(value) if key in ("counts", "raw", "delta") else float(value))
                p["motors"].append(m)
            samples.append(p)
    return samples


def axis_command(axis, value, relative=False):
    verb = "jog" if relative else "move"
    if axis in ("m1", "m2"):
        return f"{verb} motor {axis[-1]} {value}"
    if axis not in ("flex", "rotation"):
        raise ValueError("Axis must be flex, rotation, m1 or m2")
    return f"{verb} {axis} {value}"


def axis_value(packet, axis):
    a, b = (m["target_deg"] for m in packet["motors"])
    return {"m1": a, "m2": b, "flex": (a-b)/2, "rotation": (a+b)/2}[axis]


def motion_test(session, words, finished):
    if len(words) not in (4, 5) or words[1] not in ("step", "positive", "negative", "repeat"):
        raise ValueError("test step|positive|negative AXIS DELTA; test repeat AXIS DELTA COUNT")
    kind, axis, delta = words[1], words[2], float(words[3])
    if axis not in ("flex", "rotation", "m1", "m2") or not finite(delta) or delta <= 0:
        raise ValueError("Use flex/rotation/m1/m2 and a positive finite delta")
    count = int(words[4]) if kind == "repeat" and len(words) == 5 else 1
    if not 1 <= count <= 20 or (kind == "repeat") != (len(words) == 5):
        raise ValueError("Repeat count must be 1..20")
    try:
        status = session.snapshot(cancel=finished)
        if not status["armed"] or status["fault"] != "none":
            raise ValueError("arm explicitly before starting a test")
        if finished.is_set():
            raise RuntimeError("Test cancelled")
        session.command(f"stream on {config.TELEMETRY_HZ}", cancel=finished)
        base = axis_value(status, axis)
        for j in range(count):
            if finished.is_set():
                raise RuntimeError("Test cancelled")
            target = base + delta * (-1 if kind == "negative" or (kind == "repeat" and j % 2) else 1)
            session.command(axis_command(axis, target), cancel=finished)
            wait_for_target(session, finished)
        if kind == "repeat":
            if finished.is_set():
                raise RuntimeError("Test cancelled")
            session.command(axis_command(axis, base), cancel=finished)
            wait_for_target(session, finished)
        print("Test complete; stop/disarm releases both motors.")
    except MotionCancelled:
        session.priority_stop()
        raise
    except BaseException:
        try:
            session.emergency_stop()
        except Exception:
            pass
        raise


PC_HELP = """Commandes simples (angles absolus en degrés de sortie de réducteur) :
  cycle N [FLEXION EXTENSION]  Cycle complet N fois, rotation à 180°.
  neutre                     Retour au zéro commun, sans refaire le zéro.
  flexion ANGLE              M1 = +ANGLE, M2 = -ANGLE ; rotation = 0.
  extension ANGLE            M1 = -ANGLE, M2 = +ANGLE ; rotation = 0.
  rotation ANGLE             M1 = M2 = ANGLE ; flexion = 0.
  stop                       Annule l'opération et libère les moteurs.
  !                          Arrêt d'urgence avec défaut mémorisé.
Préparation : calibrate_config ; placer au neutre ; zero ; setup ; arm.
Les amplitudes de cycle omises viennent de bench_config.py.
Firmware: help, status, encoder, fault, calibrate, zero, limits, pid, safety,
arm, move, jog, stop, disarm, !, estop, clear_fault, stream.
PC: show_config; calibrate_config; setup; save FILE.png;
test step|positive|negative AXIS DELTA; test repeat AXIS DELTA COUNT;
pc_help; quit. AXIS: flex, rotation, m1, m2.
All angles are gearbox-output degrees, including differential coordinates.
After movement the PID keeps holding until stop/disarm, fault or disconnect.
"""


def console(session, finished):
    print(PC_HELP)
    def notify(event):
        if event["kind"] == "progress":
            print(f"Cycle {event['cycle']}/{event['total']} — {event['stage']} ({event['completed']} terminé(s))")
        else:
            print(event["message"])
    runner = MotionRunner(session, notify)

    try:
        while not finished.is_set() and not session.stop_event.is_set():
            try:
                line = input("wrist> ").strip()
                words = line.split()
                if not words:
                    continue
                if line in ("quit", "exit"):
                    break
                if words[0] in ("!", "stop", "disarm", "estop"):
                    runner.stop(emergency=words[0] in ("!", "estop"))
                    print("Arrêt demandé ; moteurs désarmés.")
                    continue
                if runner.busy and words[0] not in ("status", "encoder", "fault", "pc_help", "help"):
                    raise ValueError("Une opération est en cours ; stop pour l'annuler.")
                if line == "pc_help":
                    print(PC_HELP)
                elif line == "show_config":
                    print(json.dumps(config.MOTORS, indent=2))
                elif line in ("calibrate_config", "setup"):
                    apply_configuration(session, line == "calibrate_config")
                elif words[0] == "save" and len(words) == 2:
                    save_plot(session.sample_copy(), words[1])
                elif words[0] == "test":
                    runner.start(lambda cancel, w=words: motion_test(session, w, cancel))
                elif words[0] == "cycle":
                    cycle = parse_cycle(words)
                    profile = motion_profile()
                    runner.start(lambda cancel, c=cycle, p=profile: run_cycle(session, c, cancel, notify, config.TELEMETRY_HZ, p))
                elif words[0] in ("neutre", "neutral", "flexion", "extension", "rotation"):
                    step = simple_step(words)
                    profile = motion_profile()
                    runner.start(lambda cancel, s=step, p=profile: run_move(session, s, cancel, config.TELEMETRY_HZ, p))
                else:
                    print(json.dumps(session.command(line)))
                    if words[0] in ("status", "encoder", "fault"):
                        print(json.dumps(session.current()[0], indent=2))
            except (CommandRejected, ValueError) as exc:
                print(f"Rejected: {exc}")
            except EOFError:
                break
    except Exception as exc:
        print(f"Session ended: {exc}", file=sys.stderr)
    finally:
        finished.set()
        try:
            runner.stop()
        except Exception:
            pass
        runner.join()


def parse_cycle(words):
    if len(words) not in (2, 4):
        raise ValueError("Utilisation : cycle N [FLEXION EXTENSION]")
    flex, extension = ((float(v.replace(",", ".")) for v in words[2:]) if len(words) == 4 else
                       (config.CYCLE_FLEXION_DEG, config.CYCLE_EXTENSION_DEG))
    return Cycle(int(words[1]), flex, extension)


def simple_step(words):
    if words[0] in ("neutre", "neutral") and len(words) == 1:
        return Step("Retour au neutre", 0, 0)
    if len(words) != 2 or words[0] not in ("flexion", "extension", "rotation"):
        raise ValueError("Utilisation : neutre | flexion ANGLE | extension ANGLE | rotation ANGLE")
    value = float(words[1].replace(",", "."))
    if not finite(value) or (words[0] != "rotation" and value <= 0):
        raise ValueError("Angle fini requis ; amplitude de flexion/extension strictement positive.")
    if words[0] == "rotation":
        return Step("Rotation", 0, value)
    return Step(words[0].capitalize(), value if words[0] == "flexion" else -value, 0)


def report_events(session):
    while True:
        try:
            packet = session.events.get_nowait()
        except queue.Empty:
            return
        if packet.get("type") in ("fault", "disconnect"):
            print(json.dumps(packet), file=sys.stderr)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--list", action="store_true")
    parser.add_argument("--console", action="store_true", help="console technique au lieu de l'interface graphique")
    parser.add_argument("--port", default=config.PORT)
    parser.add_argument("--csv")
    parser.add_argument("--plot", action=argparse.BooleanOptionalAction, default=True,
                        help="courbes automatiques en mode --console ; bouton Courbes dans l'interface")
    parser.add_argument("--save-plot")
    parser.add_argument("--analyze", metavar="CSV", help="offline per-motor step metrics and optional PNG")
    args = parser.parse_args(argv)
    if args.analyze:
        samples = read_csv(args.analyze)
        print(json.dumps({f"m{i+1}": step_metrics([
            dict(s["motors"][i], t_ms=s["t_ms"]) for s in samples]) for i in range(2)}, indent=2))
        if args.save_plot:
            save_plot(samples, args.save_plot)
        return 0
    try:
        import serial
        from serial.tools import list_ports
    except ImportError:
        parser.error("Install dependencies: python -m pip install -r requirements.txt")
    if args.list:
        for port in list_ports.comports():
            print(f"{port.device}: {port.description}")
        return 0
    if not args.console:
        from wrist_gui import launch
        return launch(args)
    if not args.port:
        parser.error("Select --port COMx or set PORT in bench_config.py; use --list")
    session = recorder = None
    finished = threading.Event()
    try:
        recorder = CSVRecorder(args.csv) if args.csv else None
        port = serial.Serial(args.port, config.BAUDRATE, timeout=.02, write_timeout=.2)
        session = SerialSession(port, recorder)
        # A newly opened PC client starts disabled, even if another session had
        # left a powered target active. No configuration/zero/arm at startup.
        session.send("stop")
        session.start()
        print(json.dumps(session.snapshot(), indent=2))
        session.command(f"stream on {config.TELEMETRY_HZ}")
        terminal = threading.Thread(target=console, args=(session, finished), daemon=True)
        if args.plot:
            import matplotlib.pyplot as plt
            from matplotlib.animation import FuncAnimation
            fig, axes = plt.subplots(4, 2, sharex=True, figsize=(13, 10))
            fig.canvas.manager.set_window_title("Mechanical wrist - M1 / M2 gearbox outputs")
            def update(_):
                report_events(session)
                draw_samples(axes, session.sample_copy())
                if finished.is_set() or session.stop_event.is_set():
                    plt.close(fig)
            animation = FuncAnimation(fig, update, interval=200, cache_frame_data=False)
            fig.tight_layout()
            terminal.start()
            plt.show()
            _ = animation
        else:
            terminal.start()
            while not finished.is_set() and not session.stop_event.is_set():
                report_events(session)
                finished.wait(.05)
    except KeyboardInterrupt:
        pass
    except Exception as exc:
        print(f"Connection ended: {exc}", file=sys.stderr)
        return 1
    finally:
        finished.set()
        if session:
            session.close()
        if recorder:
            recorder.close()
        if session and args.save_plot and session.samples:
            save_plot(session.sample_copy(), args.save_plot)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
