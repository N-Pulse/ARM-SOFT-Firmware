#!/usr/bin/env python3
"""Standalone STM32 motor bench console, telemetry recorder and step analysis.

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

FLOAT_FIELDS = ("target_rad", "position_rad", "error_rad", "velocity_rad_s",
                "pid_percent", "pwm_percent", "dt_s")
BOOL_FIELDS = ("armed", "encoder_available", "calibrated", "feedback_valid")
CSV_FIELDS = ("host_utc", "host_monotonic_s", "t_ms", "target_rad", "position_rad",
              "error_rad", "velocity_rad_s", "pid_percent", "pwm_percent", "raw",
              "counts", "delta", "direction", "encoder_t_ms", "dt_s", "state", "armed",
              "target_reached", "encoder_available", "calibrated", "feedback_valid", "fault")


def finite(value):
    """Strict JSON numeric check (bool is not a number in this protocol)."""
    return type(value) in (int, float) and math.isfinite(value)


def parse_packet(line):
    """Validate a complete packet; reject nonstandard JSON NaN/Infinity."""
    def reject_constant(value):
        raise ValueError(f"invalid JSON constant: {value}")
    packet = json.loads(line, parse_constant=reject_constant)
    if not isinstance(packet, dict):
        raise ValueError("packet must be an object")
    kind = packet.get("type")
    if kind == "reply":
        if (not isinstance(packet.get("command"), str) or
                type(packet.get("ok")) is not bool or
                not isinstance(packet.get("message"), str)):
            raise ValueError("malformed command reply")
    elif kind in ("telemetry", "status"):
        for key in FLOAT_FIELDS:
            if key not in packet or (packet[key] is not None and not finite(packet[key])):
                raise ValueError(f"invalid {key}")
        for key in BOOL_FIELDS:
            if type(packet.get(key)) is not bool:
                raise ValueError(f"invalid {key}")
        for key in ("t_ms", "raw", "counts"):
            if type(packet.get(key)) is not int:
                raise ValueError(f"invalid {key}")
        if not 0 <= packet["raw"] <= 0xFFFFFFFF or not 0 <= packet["t_ms"] <= 0xFFFFFFFF:
            raise ValueError("counter or timestamp out of range")
        if not -(1 << 63) <= packet["counts"] < (1 << 63):
            raise ValueError("accumulated counts out of range")
        if not isinstance(packet.get("state"), str) or not isinstance(packet.get("fault"), str):
            raise ValueError("missing state or fault")
        if packet["feedback_valid"] and (not packet["calibrated"] or
                not packet["encoder_available"] or
                any(packet.get(key) is None for key in ("position_rad", "velocity_rad_s"))):
            raise ValueError("inconsistent valid feedback")
        if packet["armed"] and not packet["feedback_valid"]:
            raise ValueError("armed packet without valid feedback")
    else:
        raise ValueError(f"unknown packet type: {kind!r}")
    return packet


class LineDecoder:
    """Bounded byte decoder. Overlong/invalid lines never become commands."""
    def __init__(self, maximum=4096):
        self.maximum = maximum
        self.buffer = bytearray()
        self.dropping = False
        self.errors = 0

    def feed(self, data):
        packets = []
        for byte in data:
            if byte == 10:
                if not self.dropping and self.buffer:
                    try:
                        packets.append(parse_packet(self.buffer.decode("ascii")))
                    except (ValueError, UnicodeError):
                        self.errors += 1
                self.buffer.clear()
                self.dropping = False
            elif not self.dropping:
                if len(self.buffer) >= self.maximum:
                    self.buffer.clear()
                    self.dropping = True
                    self.errors += 1
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
        row = dict(packet, host_utc=datetime.now(timezone.utc).isoformat(),
                   host_monotonic_s=f"{time.monotonic():.6f}")
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
        self.decoder = LineDecoder()
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

    def _write(self, data):
        if self.stop_event.is_set():
            raise ConnectionError(self.failure or "serial session closed")
        with self.write_lock:
            count = self.serial.write(data)
            if count != len(data):
                raise ConnectionError("incomplete serial write")

    def send(self, command):
        if "\n" in command or "\r" in command or not command.isascii() or len(command) > 190:
            raise ValueError("command must be one ASCII line of at most 190 characters")
        self._write((command + "\n").encode("ascii"))

    def _run(self):
        next_ping = 0.0
        try:
            while not self.stop_event.is_set():
                now = time.monotonic()
                if now >= next_ping:
                    self.send("ping")
                    next_ping = now + self.heartbeat_s
                data = self.serial.read(1024)
                for packet in self.decoder.feed(data):
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
            self.stop_event.set()
            with self.condition:
                self.condition.notify_all()

    def _event(self, packet):
        try:
            self.events.put_nowait(packet)
        except queue.Full:
            pass

    def command(self, text, timeout=1.5):
        """Wait for the reply to this command, ignoring telemetry/heartbeats."""
        name = text.split()[0] if text.split() else ""
        if not name:
            raise ValueError("empty command")
        with self.command_lock:
            with self.condition:
                after = self.sequence
            self.send(text)
            deadline = time.monotonic() + timeout
            with self.condition:
                while True:
                    for index, packet in self.replies:
                        if index > after and packet["command"] in (name, "invalid", "busy"):
                            if not packet["ok"]:
                                raise CommandRejected(packet["message"])
                            return packet
                    if self.stop_event.is_set():
                        raise ConnectionError(self.failure or "session closed")
                    remaining = deadline - time.monotonic()
                    if remaining <= 0:
                        raise TimeoutError(f"no reply to {name}; stop/disarm before retrying motion")
                    self.condition.wait(remaining)

    def emergency_stop(self):
        self._write(b"!\nstop\ndisarm\n")

    def snapshot(self):
        self.command("status")
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
                self.emergency_stop()
                self.command("disarm", timeout=0.3)
            except Exception:
                pass
        self.stop_event.set()
        self.thread.join(timeout=1.0)
        self.serial.close()


def step_metrics(samples, target=None, initial=None, step_time_s=None, tolerance_rad=None):
    """Metrics from one actual step record (or explicitly synthetic unit data).

    Times use rollover-aware device milliseconds. A settling time is reported
    only when all later samples stay in the band for at least 0.2 s. Missing
    response/crossings produce None, never a made-up performance value.
    """
    good = [s for s in samples if s.get("feedback_valid", True) and
            finite(s.get("position_rad")) and finite(s.get("target_rad")) and
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
                   if abs(good[i]["target_rad"] - good[i - 1]["target_rad"]) > 1e-7]
        if len(changes) != 1:
            return {"available": False, "reason": "one target step required; crop CSV or supply step time"}
        start = changes[0]
    if start >= len(good) - 2:
        return {"available": False, "reason": "insufficient samples after the step"}
    before = good[max(0, start - 1)]["position_rad"] if initial is None else initial
    final = good[start]["target_rad"] if target is None else target
    if not finite(before) or not finite(final) or abs(final - before) < 1e-9:
        return {"available": False, "reason": "nonzero finite step amplitude required"}
    amplitude = final - before
    sign = 1.0 if amplitude > 0 else -1.0
    magnitude = abs(amplitude)
    measured = [s["position_rad"] for s in good[start:]]
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
    band = max(0.02 * magnitude, 1e-8) if tolerance_rad is None else tolerance_rad
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
            "initial_rad": before, "target_rad": final, "settling_band_rad": band,
            "rise_time_10_90_s": None if t10 is None or t90 is None else t90 - t10,
            "settling_time_s": relative_t[settle_index] if settled else None,
            "overshoot_percent": max(0.0, (max(progress) - 1.0) * 100.0),
            "steady_state_error_rad": final - sum(tail) / len(tail),
            "maximum_absolute_error_rad": max(abs(final - p) for p in measured)}


def read_csv(path):
    result = []
    with open(path, newline="", encoding="utf-8") as handle:
        for row in csv.DictReader(handle):
            for key in FLOAT_FIELDS:
                row[key] = float(row[key]) if row.get(key) else None
            row["t_ms"] = int(row["t_ms"])
            row["feedback_valid"] = row.get("feedback_valid", "True").lower() in ("true", "1")
            result.append(row)
    return result


def draw_samples(axes, samples):
    for axis in axes:
        axis.clear()
        axis.grid(True, alpha=0.25)
    if not samples:
        return
    times = [0.0]
    for a, b in zip(samples, samples[1:]):
        times.append(times[-1] + ((b["t_ms"] - a["t_ms"]) & 0xFFFFFFFF) / 1000.0)
    for key, label in (("target_rad", "Target"), ("position_rad", "Measured")):
        axes[0].plot(times, [s.get(key) for s in samples], label=label)
    axes[0].legend(loc="best")
    axes[0].set_ylabel("Position [rad]")
    for axis, key, label in zip(axes[1:], ("error_rad", "pwm_percent", "velocity_rad_s"),
                                ("Error [rad]", "Signed PWM [%]", "Velocity [rad/s]")):
        axis.plot(times, [s.get(key) for s in samples])
        axis.set_ylabel(label)
    axes[-1].set_xlabel("Device elapsed time [s]")


def save_plot(samples, path):
    from matplotlib.figure import Figure
    from matplotlib.backends.backend_agg import FigureCanvasAgg
    figure = Figure(figsize=(10, 9))
    FigureCanvasAgg(figure)
    axes = figure.subplots(4, 1, sharex=True)
    draw_samples(axes, samples)
    figure.tight_layout()
    figure.savefig(path, dpi=160)


def encoder_test(session, seconds):
    """Passive test: no movement commands; operator rotates the shaft by hand."""
    before = session.snapshot()
    if before["armed"]:
        raise RuntimeError("disarm before a passive encoder test")
    session.command("stream on 20")
    print("Motor disarmed. Rotate the shaft by hand now.")
    deadline = time.monotonic() + seconds
    next_display = 0.0
    while time.monotonic() < deadline and not session.stop_event.is_set():
        if session.latest and time.monotonic() >= next_display:
            current = dict(session.latest)
            print(json.dumps({key: current.get(key) for key in
                              ("encoder_t_ms", "raw", "counts", "delta", "direction",
                               "position_rad", "velocity_rad_s", "calibrated", "feedback_valid")}))
            next_display = time.monotonic() + 0.5
        time.sleep(0.05)
    after = session.snapshot()
    delta = after["counts"] - before["counts"]
    print(json.dumps({"counts_before": before["counts"], "counts_after": after["counts"],
                      "delta_counts": delta, "observed_direction": 1 if delta > 0 else -1 if delta < 0 else 0,
                      "note": "Count changes do not prove signal integrity or correct mechanical direction."}))


def zero_test(session):
    if session.snapshot()["armed"]:
        raise RuntimeError("disarm before zero test")
    session.command("zero")
    snapshot = session.snapshot()
    print(json.dumps({"position_rad": snapshot["position_rad"], "counts": snapshot["counts"],
                      "note": "Relative reference only. Position remains unavailable without calibration."}))


def motion_test(session, kind, delta, repetitions=3):
    """Explicit console action only. Firmware checks arm, limits and each move."""
    if not finite(delta) or delta <= 0 or not 1 <= repetitions <= 20:
        raise ValueError("step magnitude must be positive; repeats must be 1..20")
    snapshot = session.snapshot()
    if not snapshot["armed"] or not snapshot["feedback_valid"] or snapshot["state"] != "armed":
        raise RuntimeError("arm explicitly and wait for a stopped, valid bench first")
    origin = snapshot["position_rad"]
    offsets = [-delta] if kind == "negative" else [delta]
    if kind == "repeat":
        offsets = [delta if i % 2 == 0 else -delta for i in range(repetitions)] + [0.0]
    answer = input(f"Type RUN to send {len(offsets)} bounded position target(s), step {delta:g} rad: ")
    if answer != "RUN":
        print("Sequence cancelled")
        return
    session.command("stream on 20")
    start_index = len(session.samples)
    try:
        for number, offset in enumerate(offsets):
            if number:
                # The explicit RUN action authorizes only this bounded batch.
                # Rearm solely after confirmed completion, never fault/reconnect.
                session.command("arm")
            session.command(f"move {origin + offset:.9g}")
            _, after = session.current()
            deadline = time.monotonic() + 61.0
            while True:
                time.sleep(0.05)
                if session.stop_event.is_set():
                    raise ConnectionError(session.failure or "session closed")
                state, index = session.current()
                if state is None or index <= after:
                    if time.monotonic() >= deadline:
                        raise TimeoutError("no fresh movement telemetry; stopping")
                    continue
                if state["fault"] not in ("none", "NONE", "0", ""):
                    raise RuntimeError(f"sequence stopped: {state['state']} {state['fault']}")
                if state.get("target_reached") and not state["armed"]:
                    break
                if not state["armed"]:
                    raise RuntimeError("sequence disarmed before target completion")
                if time.monotonic() >= deadline:
                    raise TimeoutError("test sequence exceeded 61 seconds; stopping")
        captured = session.sample_copy()[start_index:]
        if len(offsets) == 1:
            print(json.dumps(step_metrics(captured, target=origin + offsets[0], initial=origin), indent=2))
        else:
            print("Sequence complete. Analyze individual steps from the CSV.")
    finally:
        session.command("stop")


CONSOLE_HELP = """Firmware commands pass through as ASCII; `help` lists exact syntax.
PC commands:
  test encoder [SECONDS]    Passive manual direction/count observation (disarmed)
  test zero                 Set/check relative zero (disarmed)
  test positive RAD         One positive step; requires arm and typed RUN
  test negative RAD         One negative step; requires arm and typed RUN
  test step RAD             Positive PID step and response metrics
  test repeat RAD [COUNT]   Alternating targets then return; requires typed RUN
  save PATH.png             Save four-panel plot of recorded session
  analyze                   Analyze one target transition in session samples
  !                         Emergency stop and disarm
  quit                      Stop/disarm and exit
All motion limits and all firmware preflight checks remain active.
"""


def console(session, finished):
    print(CONSOLE_HELP)
    try:
        while not finished.is_set() and not session.stop_event.is_set():
            text = input("bench> ").strip()
            if not text:
                continue
            words = text.split()
            try:
                if text in ("quit", "exit"):
                    break
                if text == "!":
                    session.emergency_stop()
                elif text in ("pc_help", "?"):
                    print(CONSOLE_HELP)
                elif words[0] == "save" and len(words) == 2:
                    save_plot(session.sample_copy(), words[1])
                    print(f"Saved {words[1]}")
                elif text == "analyze":
                    print(json.dumps(step_metrics(session.sample_copy()), indent=2))
                elif words[0] == "test":
                    if len(words) >= 2 and words[1] == "encoder" and len(words) <= 3:
                        seconds = float(words[2]) if len(words) == 3 else 10.0
                        if not finite(seconds) or not 0 < seconds <= 300:
                            raise ValueError("test duration must be 0..300 seconds")
                        encoder_test(session, seconds)
                    elif words[1:] == ["zero"]:
                        zero_test(session)
                    elif len(words) in (3, 4) and words[1] in ("positive", "negative", "step", "repeat"):
                        motion_test(session, words[1], float(words[2]), int(words[3]) if len(words) == 4 else 3)
                    else:
                        raise ValueError("type pc_help for test syntax")
                else:
                    print(json.dumps(session.command(text)))
                    if words[0] in ("status", "encoder", "fault") and session.latest_status:
                        print(json.dumps(session.latest_status, indent=2))
            except (ValueError, RuntimeError, TimeoutError) as exc:
                print(f"Command failed: {exc}")
                if isinstance(exc, TimeoutError):
                    session.emergency_stop()
    except (EOFError, KeyboardInterrupt):
        pass
    except Exception as exc:
        print(f"Console ended: {exc}")
    finally:
        finished.set()


def report_events(session, finished):
    while True:
        try:
            event = session.events.get_nowait()
        except queue.Empty:
            return
        if event["type"] in ("fault", "disconnect"):
            print(f"\n{event['type'].upper()}: {event['message']}", flush=True)
        if event["type"] == "disconnect":
            finished.set()


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--list", action="store_true", help="list serial ports and exit")
    parser.add_argument("--port", help="ST-LINK VCP, e.g. COM6")
    parser.add_argument("--csv", help="record timestamped telemetry to CSV")
    parser.add_argument("--plot", action="store_true", help="live four-panel monitoring window")
    parser.add_argument("--save-plot", help="save session plot on exit, or offline CSV plot")
    parser.add_argument("--analyze", metavar="CSV", help="offline step-response metrics; no serial connection")
    parser.add_argument("--target", type=float, help="offline explicit final target in radians")
    parser.add_argument("--initial", type=float, help="offline explicit position before step in radians")
    parser.add_argument("--step-time", type=float, help="offline step time in seconds from first CSV sample")
    parser.add_argument("--tolerance", type=float, help="offline settling band in radians (default 2%% step)")
    args = parser.parse_args(argv)
    if args.analyze:
        samples = read_csv(args.analyze)
        print(json.dumps(step_metrics(samples, args.target, args.initial, args.step_time, args.tolerance), indent=2))
        if args.save_plot:
            save_plot(samples, args.save_plot)
        return 0
    try:
        import serial
        from serial.tools import list_ports
    except ImportError:
        parser.error("Install dependencies: python -m pip install -r tools/requirements-motor-bench.txt")
    if args.list:
        for port in list_ports.comports():
            print(f"{port.device}: {port.description}")
        return 0
    if not args.port:
        parser.error("select --port COMx, or use --list")
    recorder = CSVRecorder(args.csv) if args.csv else None
    session = None
    figure = None
    finished = threading.Event()
    try:
        port = serial.Serial(args.port, 115200, timeout=0.02, write_timeout=0.2)
        session = SerialSession(port, recorder)
        session.start()
        # No arming, zeroing, calibration, gain changes or motion on startup.
        print(json.dumps(session.command("help")))
        print(json.dumps(session.snapshot(), indent=2))
        session.command("stream on 20")
        if args.plot:
            import matplotlib.pyplot as plt
            from matplotlib.animation import FuncAnimation
            figure, axes = plt.subplots(4, 1, sharex=True, figsize=(10, 9))
            figure.canvas.manager.set_window_title("STM32 motor bench — encoder measurements")
            def update(_):
                report_events(session, finished)
                draw_samples(axes, session.sample_copy()[-1200:])
                figure.tight_layout()
                if finished.is_set() or session.stop_event.is_set():
                    plt.close(figure)
            animation = FuncAnimation(figure, update, interval=200, cache_frame_data=False)
            terminal = threading.Thread(target=console, args=(session, finished), daemon=True)
            terminal.start()
            try:
                plt.show()
            finally:
                finished.set()
                session.emergency_stop()
            _ = animation
        else:
            terminal = threading.Thread(target=console, args=(session, finished), daemon=True)
            terminal.start()
            while not finished.is_set() and not session.stop_event.is_set():
                report_events(session, finished)
                finished.wait(0.05)
    except KeyboardInterrupt:
        print("Stopping and disarming")
    except Exception as exc:
        print(f"Connection ended: {exc}", file=sys.stderr)
        return 1
    finally:
        finished.set()
        if session:
            session.close()
            if args.save_plot and session.samples:
                save_plot(session.sample_copy(), args.save_plot)
            if session.failure:
                print(f"Disconnected: {session.failure}. Hardware watchdog must disable the bridge; reconnect and rearm explicitly.", file=sys.stderr)
        if recorder:
            recorder.close()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
