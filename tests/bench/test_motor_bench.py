"""Software-only PC tests. Synthetic traces are not hardware measurements."""
import importlib.util
import json
from pathlib import Path
import queue
import sys
import tempfile
import threading
import time
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
import motor_bench as bench


def sample(t=0, position=None, target=0.0, valid=False):
    return dict(type="telemetry", t_ms=t, target_rad=target, position_rad=position,
                error_rad=None if position is None else target - position,
                velocity_rad_s=0.0 if valid else None, pid_percent=0.0, pwm_percent=0.0,
                raw=0xFFFFFFFF, counts=-2, dt_s=0.01, state="disarmed", armed=False,
                encoder_available=True, calibrated=valid, feedback_valid=valid, fault="none")


class ParserTests(unittest.TestCase):
    def test_uncalibrated_null_is_preserved(self):
        result = bench.parse_packet(json.dumps(sample()))
        self.assertIsNone(result["position_rad"])
        self.assertEqual(result["counts"], -2)
        self.assertEqual(result["raw"], 0xFFFFFFFF)

    def test_nonfinite_and_invalid_types_rejected(self):
        for key, value in (("position_rad", float("nan")), ("raw", True),
                           ("armed", 1), ("t_ms", -1), ("counts", 1 << 64)):
            packet = sample()
            packet[key] = value
            with self.subTest(key=key), self.assertRaises(ValueError):
                bench.parse_packet(json.dumps(packet))
        packet = sample()
        del packet["pwm_percent"]
        with self.assertRaises(ValueError):
            bench.parse_packet(json.dumps(packet))

    def test_false_validity_rejected(self):
        packet = sample()
        packet["feedback_valid"] = True
        with self.assertRaises(ValueError):
            bench.parse_packet(json.dumps(packet))

    def test_partial_lines_and_overflow_recover_at_newline(self):
        decoder = bench.LineDecoder(1024)
        encoded = (json.dumps(sample()) + "\r\n").encode()
        self.assertEqual(decoder.feed(encoded[:20]), [])
        self.assertEqual(len(decoder.feed(encoded[20:])), 1)
        self.assertEqual(decoder.feed(b"x" * 1025 + b"\n"), [])
        self.assertEqual(len(decoder.feed(encoded)), 1)
        self.assertEqual(decoder.errors, 1)
        self.assertEqual(decoder.feed(b"\xff\n{bad}\n"), [])
        self.assertEqual(decoder.errors, 3)

    def test_reply_validation(self):
        packet = dict(type="reply", command="move", ok=False, message="not armed")
        self.assertFalse(bench.parse_packet(json.dumps(packet))["ok"])
        with self.assertRaises(ValueError):
            bench.parse_packet('{"type":"reply","command":"arm","ok":1}')


class MetricTests(unittest.TestCase):
    def trace(self, sign=1):
        # Exact synthetic linear 0->1 response over 1 second, then 1 second flat.
        return [sample(i * 10, sign * min(i / 100, 1), sign * 1.0, True)
                for i in range(201)]

    def test_positive_linear_response(self):
        result = bench.step_metrics(self.trace(), target=1, initial=0)
        self.assertTrue(result["available"])
        self.assertAlmostEqual(result["rise_time_10_90_s"], 0.8, places=6)
        self.assertAlmostEqual(result["settling_time_s"], 0.98, places=6)
        self.assertAlmostEqual(result["overshoot_percent"], 0)
        self.assertAlmostEqual(result["steady_state_error_rad"], 0)
        self.assertAlmostEqual(result["maximum_absolute_error_rad"], 1)

    def test_negative_step_has_same_metrics(self):
        positive = bench.step_metrics(self.trace(), target=1, initial=0)
        negative = bench.step_metrics(self.trace(-1), target=-1, initial=0)
        for key in ("rise_time_10_90_s", "settling_time_s", "overshoot_percent",
                    "maximum_absolute_error_rad"):
            self.assertAlmostEqual(positive[key], negative[key])

    def test_no_measurements_no_claim(self):
        self.assertFalse(bench.step_metrics([])["available"])
        self.assertFalse(bench.step_metrics([sample(i * 10) for i in range(20)])["available"])

    def test_unreached_target_has_no_rise_or_settle(self):
        result = bench.step_metrics([sample(i * 10, 0.2, 1.0, True) for i in range(100)],
                                    target=1.0, initial=0)
        self.assertIsNone(result["rise_time_10_90_s"])
        self.assertIsNone(result["settling_time_s"])
        self.assertAlmostEqual(result["steady_state_error_rad"], 0.8)

    def test_timestamp_rollover_and_duplicate(self):
        trace = self.trace()
        for row in trace:
            row["t_ms"] = (row["t_ms"] + 0xFFFFFFF0) & 0xFFFFFFFF
        self.assertTrue(bench.step_metrics(trace, target=1, initial=0)["available"])
        trace[1]["t_ms"] = trace[0]["t_ms"]
        self.assertFalse(bench.step_metrics(trace, target=1, initial=0)["available"])

    def test_only_single_target_transition_inferred(self):
        trace = self.trace()
        for row in trace[:10]:
            row["target_rad"] = 0
        self.assertTrue(bench.step_metrics(trace)["available"])
        trace[-1]["target_rad"] = 0
        self.assertFalse(bench.step_metrics(trace)["available"])

    def test_csv_headers_and_null_roundtrip(self):
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / "test.csv"
            recorder = bench.CSVRecorder(path)
            recorder.write(sample())
            recorder.close()
            rows = bench.read_csv(path)
            self.assertIsNone(rows[0]["position_rad"])
            self.assertFalse(rows[0]["feedback_valid"])
            self.assertIn("host_utc", path.read_text().splitlines()[0])

    @unittest.skipUnless(importlib.util.find_spec("matplotlib"), "matplotlib not installed")
    def test_png_export(self):
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / "response.png"
            bench.save_plot(self.trace(), path)
            self.assertEqual(path.read_bytes()[:8], b"\x89PNG\r\n\x1a\n")
            self.assertGreater(path.stat().st_size, 1000)


class FakeSerial:
    def __init__(self):
        self.incoming = queue.Queue()
        self.writes = []
        self.fail = False
        self.closed = False

    def write(self, data):
        if self.fail:
            raise OSError("USB disconnected")
        self.writes.append(data)
        for line in data.decode().splitlines():
            if line == "!":
                continue
            name = line.split()[0]
            if name == "status":
                status = dict(sample(), type="status")
                self.incoming.put((json.dumps(status) + "\n").encode())
            if name != "silent":
                reply = dict(type="reply", command=name, ok=name != "move", message="ok" if name != "move" else "not armed")
                self.incoming.put((json.dumps(reply) + "\n").encode())
        return len(data)

    def read(self, _size):
        if self.fail:
            raise OSError("USB disconnected")
        try:
            return self.incoming.get(timeout=0.005)
        except queue.Empty:
            return b""

    def close(self):
        self.closed = True


class SessionTests(unittest.TestCase):
    def test_startup_heartbeat_does_not_arm(self):
        port = FakeSerial()
        session = bench.SerialSession(port, heartbeat_s=0.02)
        session.start()
        try:
            status = session.snapshot()
            self.assertFalse(status["armed"])
            time.sleep(0.05)
            self.assertTrue(any(data == b"ping\n" for data in port.writes))
            self.assertFalse(any(data.startswith((b"arm", b"move", b"jog", b"pulse")) for data in port.writes))
            with self.assertRaises(bench.CommandRejected):
                session.command("move 0.1")
            with self.assertRaises(TimeoutError):
                session.command("silent", timeout=0.02)
            port.incoming.put(b'{"type":"reply","command":"fault","ok":false,"message":"stall"}\n')
            deadline = time.monotonic() + 0.5
            event = None
            while time.monotonic() < deadline:
                candidate = session.events.get(timeout=0.5)
                if candidate["type"] == "fault":
                    event = candidate
                    break
            self.assertEqual(event, {"type": "fault", "message": "stall"})
        finally:
            session.close()
        self.assertIn(b"!\nstop\ndisarm\n", port.writes)
        self.assertTrue(port.closed)

    def test_disconnect_ends_heartbeats_without_reconnect(self):
        port = FakeSerial()
        session = bench.SerialSession(port, heartbeat_s=0.01)
        session.start()
        session.snapshot()
        port.fail = True
        self.assertTrue(session.stop_event.wait(0.5))
        writes = len(port.writes)
        time.sleep(0.04)
        self.assertEqual(len(port.writes), writes)
        with self.assertRaises(ConnectionError):
            session.command("arm")
        session.close()
        self.assertIn("disconnected", session.failure)

    def test_multiline_command_rejected(self):
        session = bench.SerialSession(FakeSerial())
        with self.assertRaises(ValueError):
            session.send("ping\narm")


class SequenceSession:
    def __init__(self, fault=False, stopped=False):
        self.commands = []
        self.samples = []
        self.stop_event = threading.Event()
        self.failure = None
        self.index = 0
        self.fault = fault
        self.stopped = stopped

    def snapshot(self):
        return dict(sample(0, 0, 0, True), armed=True, state="armed")

    def command(self, text):
        self.commands.append(text)

    def current(self):
        self.index += 1
        state = dict(sample(self.index * 10, 0.01, 0.01, True), armed=False,
                     target_reached=not self.stopped, state="disarmed",
                     fault="stall" if self.fault else "none")
        return state, self.index

    def sample_copy(self):
        return []


class SequenceTests(unittest.TestCase):
    def test_cancelled_run_does_not_send_motion_or_arm(self):
        session = SequenceSession()
        with patch("builtins.input", return_value="cancel"), patch("builtins.print"):
            bench.motion_test(session, "positive", 0.01)
        self.assertEqual(session.commands, [])

    def test_batch_rearms_only_after_completed_targets(self):
        session = SequenceSession()
        with patch("builtins.input", return_value="RUN"), patch("builtins.print"), patch("time.sleep"):
            bench.motion_test(session, "repeat", 0.01, 2)
        self.assertEqual(session.commands, ["stream on 20", "move 0.01", "arm", "move -0.01",
                                            "arm", "move 0", "stop"])

    def test_fault_or_operator_stop_never_rearms(self):
        for options in ({"fault": True}, {"stopped": True}):
            session = SequenceSession(**options)
            with patch("builtins.input", return_value="RUN"), patch("time.sleep"), self.assertRaises(RuntimeError):
                bench.motion_test(session, "repeat", 0.01, 2)
            self.assertNotIn("arm", session.commands)
            self.assertEqual(session.commands[-1], "stop")


if __name__ == "__main__":
    unittest.main()
