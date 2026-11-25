import copy
import json
from pathlib import Path
import queue
import tempfile
import time
import unittest
from unittest.mock import patch
import wrist_bench as bench

ROOT = Path(__file__).resolve().parents[1]

def status():
    lines = (ROOT / "build/tests/protocol.jsonl").read_text().splitlines()
    return next(bench.parse_packet(line) for line in lines if json.loads(line).get("type") == "status")

class FakeSerial:
    def __init__(self, reply=True):
        self.rx = queue.Queue()
        self.writes = []
        self.reply = reply
        self.closed = False
    def write(self, data):
        self.writes.append(data)
        for line in data.decode().splitlines():
            if self.reply and line:
                name = line.split()[0]
                if name == "status":
                    self.rx.put((json.dumps(status())+"\n").encode())
                self.rx.put((json.dumps(dict(type="reply", command=name, ok=True, message="OK"))+"\n").encode())
        return len(data)
    def read(self, maximum):
        try:
            return self.rx.get(timeout=.01)
        except queue.Empty:
            return b""
    def close(self):
        self.closed = True

class ClientTests(unittest.TestCase):
    def test_stall_diagnostic_explains_progress_instead_of_velocity_limit(self):
        packet = dict(fault="stall", fault_motor=1, fault_detail=dict(t_ms=1000, command_active=True,
                      target_deg=2, position_deg=.018, pwm_percent=3, velocity_deg_s=.086,
                      velocity_limit_deg_s=10))
        board = dict(motors=[dict(configuration=dict(safety=[5000, 2, .1, 1500, 1, 2, 500],
                                                   pid=[5, 0, .2, 0, .02, 3, .5]))])
        message = bench.fault_diagnostic(packet, board)
        self.assertIn("progression < 0.1° sur 1.5 s", message)
        self.assertIn("erreur 1.982°", message)
        self.assertIn("PWM 3%", message)
        self.assertNotIn("limite 10", message)
        self.assertNotIn("vitesse", message)
        self.assertIn("position 0.018°", bench.fault_diagnostic(packet))

    def test_protocol_and_partial_lines(self):
        p = status()
        line = (json.dumps(p)+"\n").encode()
        decoder = bench.LineDecoder()
        self.assertEqual(decoder.feed(line[:20]), [])
        self.assertEqual(decoder.feed(line[20:]), [p])
        decoder = bench.LineDecoder(10)
        self.assertEqual(decoder.feed(b"x"*20+b"\n"), [])
        self.assertEqual(decoder.errors, 1)

    def test_reject_malformed_and_wrong_firmware(self):
        for change in (lambda p: p.update(firmware="stm32-motor-bench/1"),
                       lambda p: p["motors"][1].update(position_deg=float("nan")),
                       lambda p: p["motors"][0].update(counts=1.2),
                       lambda p: p["motors"][1].update(id=1),
                       lambda p: p.update(armed=True)):
            p = status(); change(p)
            with self.assertRaises(ValueError):
                bench.parse_packet(json.dumps(p))

    def test_unknown_configuration_is_not_sent(self):
        incomplete = {i: dict(ppr=None, limits=None) for i in (1, 2)}
        with patch.object(bench.config, "MOTORS", incomplete):
            with self.assertRaisesRegex(ValueError, "ppr"):
                bench.configuration_commands(True)
            with self.assertRaisesRegex(ValueError, "limits"):
                bench.configuration_commands(False)
        profile = {i: dict(ppr=90, encoder_sign=1, encoder_location="motor", gear_ratio=203,
                          pid=[1, 0, 0, 0, .02, 5, .5], limits=[-5, 5, 1, 2, 1000],
                          safety=[100, 1, 1, 100, .1, 2, 100]) for i in (1, 2)}
        with patch.object(bench.config, "MOTORS", profile):
            self.assertEqual(len(bench.configuration_commands()), 6)
            self.assertEqual(bench.configuration_commands(True)[0], "calibrate 1 90 4 203 1 motor")
            profile[2]["limits"][-1] = 1.5
            with self.assertRaises(ValueError):
                bench.configuration_commands()

    def test_csv_and_plot(self):
        out = ROOT / "build/tests"
        with tempfile.TemporaryDirectory(dir=out) as tmp:
            p = status(); p["motors"][0]["counts"] = (1 << 60) + 7
            recorder = bench.CSVRecorder(Path(tmp)/"record.csv")
            recorder.write(p); recorder.close()
            samples = bench.read_csv(Path(tmp)/"record.csv")
            self.assertEqual(samples[0]["motors"][0]["counts"], (1 << 60) + 7)
            bench.save_plot(samples, Path(tmp)/"record.png")
            self.assertGreater((Path(tmp)/"record.png").stat().st_size, 10000)

    def test_fault_sample_from_production_protocol_reaches_csv_and_diagnostic(self):
        lines = (ROOT / "build/tests/protocol.jsonl").read_text().splitlines()
        packet = next(bench.parse_packet(line) for line in lines if json.loads(line).get("fault_detail"))
        detail = packet["fault_detail"]
        self.assertEqual(packet["fault"], "position_limit")
        self.assertAlmostEqual(detail["velocity_deg_s"], 100, places=2)
        self.assertEqual(detail["velocity_limit_deg_s"], 10)
        self.assertEqual(detail["delta"], 1)
        self.assertFalse(detail["command_active"])
        self.assertIn("aucune consigne moteur active", bench.fault_diagnostic(packet))
        with tempfile.TemporaryDirectory(dir=ROOT / "build/tests") as tmp:
            path = Path(tmp) / "fault.csv"
            recorder = bench.CSVRecorder(path)
            recorder.write(packet)
            recorder.close()
            with path.open(newline='', encoding='utf-8') as source:
                import csv
                row = next(csv.DictReader(source))
            self.assertAlmostEqual(float(row["fault_velocity_deg_s"]), 100, places=2)
            self.assertEqual(row["fault_delta"], "1")
        # Older firmware/CSV are still usable without the optional extension.
        packet.pop("fault_detail")
        self.assertEqual(bench.parse_packet(json.dumps(packet))["fault"], "position_limit")
        self.assertIn("indisponible", bench.fault_diagnostic(packet))

    def test_serial_heartbeat_and_shutdown(self):
        serial = FakeSerial()
        session = bench.SerialSession(serial, heartbeat_s=.01)
        session.start()
        try:
            self.assertEqual(session.snapshot()["firmware"], "stm32-wrist-bench/1")
            self.assertIn(b"ping\n", serial.writes)
            self.assertFalse(any(w.startswith(b"arm") for w in serial.writes))
        finally:
            session.close()
        self.assertTrue(serial.closed)
        self.assertIn(b"stop\ndisarm\n", serial.writes)

    def test_timeout_closes_session(self):
        serial = FakeSerial(reply=False)
        session = bench.SerialSession(serial)
        session.start()
        try:
            with self.assertRaises(TimeoutError):
                session.command("move flex 1", timeout=.03)
            self.assertTrue(session.stop_event.is_set())
            self.assertTrue(any(b"!\n" in w for w in serial.writes))
        finally:
            session.close()

    def test_corrupt_feedback_stops_heartbeat(self):
        serial = FakeSerial()
        serial.rx.put(b"{bad telemetry}\n")
        session = bench.SerialSession(serial)
        session.start()
        self.assertTrue(session.stop_event.wait(1))
        self.assertIsNotNone(session.failure)
        session.close()
        self.assertTrue(any(b"!\n" in w for w in serial.writes))

    def test_startup_partial_line_is_synchronized_once(self):
        serial = FakeSerial()
        serial.rx.put(b'pwm_percent":0}]}\n')
        session = bench.SerialSession(serial)
        session.start()
        try:
            self.assertEqual(session.snapshot()["firmware"], "stm32-wrist-bench/1")
            self.assertTrue(session.decoder.discarded_prefix)
            self.assertEqual(session.decoder.errors, 0)
            serial.rx.put(b'broken second packet\n')
            self.assertTrue(session.stop_event.wait(1))
            self.assertIn("broken second packet", session.failure)
        finally:
            session.close()

    def test_wrong_firmware_and_malformed_json_objects_are_not_skipped_at_startup(self):
        decoder = bench.LineDecoder(synchronize=True)
        self.assertEqual(decoder.feed(b'{bad telemetry}\n'), [])
        self.assertEqual(decoder.errors, 1)
        decoder = bench.LineDecoder(synchronize=True)
        packet = status()
        packet["firmware"] = "wrong/1"
        self.assertEqual(decoder.feed((json.dumps(packet) + '\n').encode()), [])
        self.assertEqual(decoder.errors, 1)
        self.assertIn("wrong firmware", decoder.last_error)

    def test_cancel_drains_ack_but_priority_stop_is_immediate(self):
        serial = FakeSerial(reply=False)
        session = bench.SerialSession(serial)
        session.start()
        cancel, result = bench.threading.Event(), []
        def pending_command():
            try:
                session.command("arm", cancel=cancel)
            except bench.MotionCancelled:
                result.append("cancelled")
        worker = bench.threading.Thread(target=pending_command)
        try:
            worker.start()
            deadline = time.monotonic() + 1
            while b"arm\n" not in serial.writes and time.monotonic() < deadline:
                time.sleep(.005)
            self.assertIn(b"arm\n", serial.writes)
            cancel.set()
            session.priority_stop()
            self.assertIn(b"stop\ndisarm\n", serial.writes)
            self.assertTrue(worker.is_alive())
            serial.rx.put(b'{"type":"reply","command":"arm","ok":true,"message":"OK"}\n')
            worker.join(1)
            self.assertEqual(result, ["cancelled"])
        finally:
            # Let orderly close receive its ACK too.
            serial.reply = True
            session.close()

    def test_aborted_batch_stops_without_rearming(self):
        class Session:
            commands = []
            stopped = False
            def snapshot(self, cancel=None):
                return {"armed": True, "fault": "none", "motors": [{"target_deg": 0}, {"target_deg": 0}]}
            def command(self, text, cancel=None):
                self.commands.append(text)
                if text.startswith("move"):
                    raise bench.CommandRejected("limit")
            def emergency_stop(self):
                self.stopped = True
        session = Session()
        import threading
        with self.assertRaises(bench.CommandRejected):
            bench.motion_test(session, "test repeat flex 1 4".split(), threading.Event())
        self.assertTrue(session.stopped)
        self.assertEqual(session.commands, ["stream on 10", "move flex 1.0"])

    def test_wait_uses_stream_instead_of_status_flood(self):
        class Session:
            calls = 0
            stop_event = bench.threading.Event()
            def snapshot(self, cancel=None):
                self.calls += 1
                return {"armed": True, "fault": "none", "target_reached": False,
                        "motors": [{"target_deg": 1}, {"target_deg": -1}]}
            def current(self):
                return ({"armed": True, "fault": "none", "target_reached": True,
                         "motors": [{"target_deg": 1}, {"target_deg": -1}]}, 2)
        session = Session()
        bench.wait_for_target(session, bench.threading.Event())
        self.assertEqual(session.calls, 1)

    def test_step_metrics_degrees(self):
        samples = [dict(t_ms=i*100, target_deg=0 if i < 2 else 10,
                        position_deg=0 if i < 2 else 10*(1-2**(-(i-2)))) for i in range(30)]
        result = bench.step_metrics(samples)
        self.assertTrue(result)
        self.assertTrue(result["available"])
        self.assertIsNotNone(result["rise_time_10_90_s"])

if __name__ == "__main__":
    unittest.main()
