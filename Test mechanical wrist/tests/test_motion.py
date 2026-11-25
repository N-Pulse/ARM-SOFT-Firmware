"""Operator sequence checks with simulated motor feedback, never real hardware."""
import copy
import threading
import time
import unittest
from unittest.mock import patch

import wrist_bench as bench
from wrist_motion import Cycle, MotionProfile, MotionCancelled, MotionRunner, Step, run_cycle, run_move, wait_for_target


def ready_status():
    return dict(armed=True, fault="none", fault_motor=0, target_reached=True, state="holding",
                host_monotonic_s=time.monotonic(), motors=[
        dict(target_deg=0, position_deg=0, calibrated=True, zeroed=True, feedback_valid=True,
             configuration=dict(limits_set=True, pid_set=True, safety_set=True,
                                limits=[-180.5, 180.5, 361, 10, 60000])) for _ in range(2)])


class SimulatedSession:
    """A move ACK is followed by moving feedback, then an actual arrival."""
    def __init__(self):
        self.stop_event = threading.Event()
        self.packet = ready_status()
        self.commands = []
        self.arrivals = []
        self.pending = None
        self.estopped = self.stopped = False
        self.fault_on_move = None
        self.cancel_on_move = None
        self.reject_on_move = None
        self.move_count = 0

    def snapshot(self, cancel=None):
        return copy.deepcopy(self.packet)

    def command(self, text, cancel=None):
        if cancel is not None and cancel.is_set():
            raise MotionCancelled("cancelled before send")
        self.commands.append(text)
        if not text.startswith("move wrist"):
            return
        # This assertion catches issuing the next command on ACK alone.
        if self.pending is not None:
            raise AssertionError("next move sent before both motors arrived")
        self.move_count += 1
        if self.reject_on_move == self.move_count:
            raise bench.CommandRejected("target_outside_motor_limits")
        flex, rotation = map(float, text.split()[2:])
        self.pending = (rotation + flex, rotation - flex)
        for motor, target in zip(self.packet["motors"], self.pending):
            motor["target_deg"] = target
        self.packet.update(target_reached=False, state="moving", host_monotonic_s=time.monotonic())
        if self.fault_on_move == self.move_count:
            self.packet.update(fault="stall", armed=False)
        if self.cancel_on_move == self.move_count:
            cancel.set()

    def current(self):
        if self.pending is not None:
            for motor, target in zip(self.packet["motors"], self.pending):
                motor["position_deg"] = target
            self.arrivals.append(self.pending)
            self.pending = None
            self.packet.update(target_reached=True, state="holding", host_monotonic_s=time.monotonic())
        return copy.deepcopy(self.packet), len(self.arrivals)

    def emergency_stop(self):
        self.estopped = True
        self.packet.update(armed=False, state="fault", fault="estop")

    def priority_stop(self):
        self.stopped = True
        self.packet.update(armed=False, state="disarmed")


class VirtualClock:
    def __init__(self):
        self.now = 0.0
        self.cancelled = False

    def is_set(self):
        return self.cancelled

    def set(self):
        self.cancelled = True

    def wait(self, seconds):
        self.now += seconds
        return self.cancelled


class MotionTests(unittest.TestCase):
    def test_trapezoid_speed_acceleration_and_endpoint(self):
        profile = MotionProfile(4, 8)
        for distance in (.2, 180):
            duration = profile.duration(distance)
            dt = .001
            positions = [profile.position(distance, j * dt) for j in range(int(duration / dt) + 1)]
            speeds = [(b - a) / dt for a, b in zip(positions, positions[1:])]
            self.assertTrue(all(-1e-8 <= v <= 4 + 1e-8 for v in speeds))
            self.assertTrue(all(abs(b - a) / dt <= 8 + 1e-6 for a, b in zip(speeds, speeds[1:])))
            self.assertEqual(profile.position(distance, duration), distance)
        self.assertAlmostEqual(profile.duration(180), 45.5)

    def test_ramped_moves_keep_both_motors_on_the_same_path_and_end_at_goal(self):
        clock = VirtualClock()
        with patch("wrist_motion.time.monotonic", side_effect=lambda: clock.now):
            session = SimulatedSession()
            sent = []
            command = session.command
            def record(text, cancel=None):
                if text.startswith("move wrist"):
                    flex, rotation = map(float, text.split()[2:])
                    sent.append((clock.now, rotation + flex, rotation - flex))
                return command(text, cancel)
            session.command = record
            run_move(session, Step("Flexion", 180, 0), clock, profile=MotionProfile(4, 8))
            self.assertGreater(len(sent), 400)
            self.assertTrue(all(abs(a + b) < 1e-9 for _, a, b in sent))
            previous = (0.0, 0.0, 0.0)
            for item in sent:
                dt = item[0] - previous[0]
                if dt:
                    self.assertLessEqual(abs(item[1] - previous[1]) / dt, 4.00001)
                previous = item
            self.assertEqual(session.arrivals[-1], (180, -180))
            self.assertFalse(session.estopped)

    def test_profile_checks_arrival_budget_only(self):
        from wrist_motion import validate_profile
        session = SimulatedSession()
        steps = [Step("Rotation", 0, 180)]
        validate_profile(session.packet, steps, MotionProfile(12, 8))
        for motor in session.packet["motors"]:
            motor["configuration"]["limits"][4] = 1000
        with self.assertRaises(ValueError):
            validate_profile(session.packet, steps, MotionProfile(4, 8))

    def test_cancel_or_fault_during_a_ramp_stops_before_the_final_target(self):
        for attribute in ("cancel_on_move", "fault_on_move"):
            clock = VirtualClock()
            with patch("wrist_motion.time.monotonic", side_effect=lambda: clock.now):
                session = SimulatedSession()
                setattr(session, attribute, 3)
                with self.assertRaises(RuntimeError):
                    run_move(session, Step("Rotation", 0, 180), clock, profile=MotionProfile(4, 8))
                self.assertEqual(session.move_count, 3)
                self.assertNotIn("move wrist 0 180", session.commands)
                self.assertTrue(session.stopped if attribute == "cancel_on_move" else session.estopped)

    def test_ramped_cycles_still_count_only_final_neutral_arrivals(self):
        clock, events = VirtualClock(), []
        with patch("wrist_motion.time.monotonic", side_effect=lambda: clock.now):
            session = SimulatedSession()
            run_cycle(session, Cycle(2, 2, 1), clock, events.append, profile=MotionProfile(4, 8))
            self.assertEqual([e["completed"] for e in events if e["stage"] == "Cycle terminé"], [1, 2])
            self.assertEqual(session.arrivals[-1], (0, 0))
            self.assertIn((180, 180), session.arrivals)

    def test_two_full_cycles_wait_and_end_at_neutral(self):
        session, events = SimulatedSession(), []
        result = run_cycle(session, Cycle(2, 20, 15), threading.Event(), events.append)
        self.assertEqual(session.arrivals, [(0, 0)] + [(20, -20), (-15, 15), (0, 0), (180, 180), (0, 0)] * 2)
        self.assertEqual(session.move_count, 11)
        self.assertEqual([e["completed"] for e in events if e["stage"] == "Cycle terminé"], [1, 2])
        self.assertIn("2 cycle(s) terminé(s)", result)
        self.assertFalse(session.estopped)
        self.assertFalse(session.stopped)

    def test_invalid_inputs_and_route_rejected_before_any_move(self):
        for args in ((0, 20, 20), (1.5, 20, 20), (True, 20, 20), (1, None, 20),
                     (1, 0, 20), (1, 20, float("nan")), (1, 20, float("inf"))):
            with self.subTest(args=args), self.assertRaises(ValueError):
                Cycle(*args)
        for change in (lambda p: p.update(armed=False),
                       lambda p: p["motors"][1]["configuration"]["limits"].__setitem__(1, 100),
                       lambda p: p["motors"][0]["configuration"]["limits"].__setitem__(2, 30),
                       lambda p: p["motors"][0].update(zeroed=False)):
            session = SimulatedSession()
            change(session.packet)
            with self.assertRaises(ValueError):
                run_cycle(session, Cycle(2, 20, 20), threading.Event())
            self.assertEqual(session.commands, [])

    def test_neutral_start_is_also_validated_from_measured_position(self):
        session = SimulatedSession()
        session.packet["motors"][0]["position_deg"] = 100
        session.packet["motors"][0]["configuration"]["limits"][2] = 40
        with self.assertRaisesRegex(ValueError, "Neutre initial"):
            run_cycle(session, Cycle(1, 10, 10), threading.Event())
        self.assertEqual(session.move_count, 0)

    def test_cancel_fault_or_reject_prevents_subsequent_steps(self):
        for attribute in ("cancel_on_move", "fault_on_move", "reject_on_move"):
            session = SimulatedSession()
            setattr(session, attribute, 3)
            events = []
            with self.subTest(attribute=attribute), self.assertRaises(RuntimeError):
                run_cycle(session, Cycle(4, 20, 15), threading.Event(), events.append)
            self.assertEqual(session.move_count, 3)
            self.assertFalse(any(e["completed"] for e in events))
            self.assertTrue(session.stopped if attribute == "cancel_on_move" else session.estopped)
            self.assertFalse(any(c == "arm" for c in session.commands))

    def test_simple_commands_reset_other_axis_and_accept_decimal_comma(self):
        session = SimulatedSession()
        for text in ("flexion 12,5", "extension 8", "rotation -90", "neutre"):
            run_move(session, bench.simple_step(text.split()), threading.Event())
        self.assertEqual(session.arrivals, [(12.5, -12.5), (-8, 8), (-90, -90), (0, 0)])
        for text in ("neutre 5", "extension -8", "flexion nan", "rotation inf"):
            with self.subTest(text=text), self.assertRaises(ValueError):
                bench.simple_step(text.split())

    def test_cycles_can_use_explicit_or_profile_amplitudes(self):
        self.assertEqual(bench.parse_cycle("cycle 25 20 15".split()), Cycle(25, 20, 15))
        with patch.object(bench.config, "CYCLE_FLEXION_DEG", 12), patch.object(bench.config, "CYCLE_EXTENSION_DEG", 9):
            self.assertEqual(bench.parse_cycle("cycle 3".split()), Cycle(3, 12, 9))
        with patch.object(bench.config, "CYCLE_FLEXION_DEG", None), self.assertRaises(ValueError):
            bench.parse_cycle("cycle 3".split())

    def test_stale_or_diverted_telemetry_cannot_complete_a_step(self):
        for change in (lambda p: p.update(host_monotonic_s=time.monotonic() - 2),
                       lambda p: p["motors"][1].update(target_deg=90)):
            session = SimulatedSession()
            change(session.packet)
            with self.assertRaises((TimeoutError, RuntimeError)):
                wait_for_target(session, threading.Event(), (0, 0))

    def test_runner_rejects_overlapping_actions_and_stop_bypasses_ack(self):
        session, events = SimulatedSession(), []
        runner = MotionRunner(session, events.append)
        entered = threading.Event()
        def action(cancel):
            entered.set()
            cancel.wait(2)
        runner.start(action)
        self.assertTrue(entered.wait(1))
        with self.assertRaises(ValueError):
            runner.start(action)
        runner.stop()
        runner.join()
        self.assertTrue(session.stopped)
        self.assertFalse(runner.busy)
        self.assertEqual(events[-1]["kind"], "cancelled")

    def test_cancelled_command_cannot_send_a_late_move_or_arm(self):
        class Serial:
            writes = []
            def write(self, data):
                self.writes.append(data)
                return len(data)
        session = bench.SerialSession(Serial())
        cancel = threading.Event()
        # Reproduce stop while the worker is waiting for the serial write lock.
        for text in ("move wrist 0 180", "arm"):
            cancel.clear()
            caught = []
            with session.write_lock:
                def send():
                    try:
                        session.command(text, cancel=cancel)
                    except MotionCancelled:
                        caught.append(True)
                thread = threading.Thread(target=send)
                thread.start()
                cancel.set()
            session.priority_stop()
            thread.join(1)
            self.assertEqual(caught, [True])
        self.assertEqual(session.serial.writes, [b"stop\ndisarm\n"] * 2)


if __name__ == "__main__":
    unittest.main()
