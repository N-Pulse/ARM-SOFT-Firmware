"""Exercise operator actions with a fake session; never open a serial port."""
import copy
import time
from types import SimpleNamespace
import unittest
from unittest.mock import patch

try:
    import tkinter as tk
    from wrist_gui import WristApp
except ImportError:
    tk = None

from wrist_motion import MotionRunner
from test_motion import SimulatedSession


@unittest.skipIf(tk is None, "Tkinter not installed")
class GuiTests(unittest.TestCase):
    def setUp(self):
        try:
            self.root = tk.Tk()
        except tk.TclError as exc:
            self.skipTest(f"Tk unavailable in this environment: {exc}")
        self.root.withdraw()
        with patch("serial.tools.list_ports.comports", return_value=[]):
            self.app = WristApp(self.root, SimpleNamespace(port=None, csv=None, save_plot=None))
        self.session = SimulatedSession()
        self.session.latest_status = copy.deepcopy(self.session.packet)
        self.session.close = self.session.stop_event.set
        self.app.session = self.session
        self.app.runner = MotionRunner(self.session, self.app.events.put)

    def tearDown(self):
        if hasattr(self, "app"):
            self.app.close()

    def test_controls_require_preparation_and_explicit_arm(self):
        self.session.packet.update(armed=False, state="disarmed")
        self.session.packet["motors"][0]["calibrated"] = False
        self.app._update_status()
        self.assertTrue(self.app.cycle_button.instate(["disabled"]))
        self.assertTrue(self.app.arm_button.instate(["disabled"]))
        self.assertFalse(self.app.calibrate_button.instate(["disabled"]))
        self.session.packet["motors"][0]["calibrated"] = True
        self.app._update_status()
        self.assertFalse(self.app.arm_button.instate(["disabled"]))
        self.assertTrue(self.app.cycle_button.instate(["disabled"]))
        self.session.packet.update(armed=True, state="armed")
        self.app._update_status()
        self.assertFalse(self.app.cycle_button.instate(["disabled"]))
        self.session.packet.update(armed=False, state="fault", fault="uart_error")
        self.app._update_status()
        self.assertIn("global", self.app.state.get())
        self.assertIn("liaison série", self.app.state.get())
        self.assertFalse(self.app.clear_button.instate(["disabled"]))
        self.assertTrue(self.app.cycle_button.instate(["disabled"]))

    def test_cycle_button_completes_then_stop_requires_rearm(self):
        self.app.count.set("2")
        self.app.flexion.set("12,5")
        self.app.extension.set("8")
        self.app._update_status()
        # Trajectory dynamics are covered with a virtual clock in test_motion;
        # this test exercises Tk controls and the complete sequence wiring.
        with patch("wrist_gui.bench.motion_profile", return_value=None):
            self.app.cycle_button.invoke()
            self.app._update_status()
            self.assertTrue(self.app.cycle_button.instate(["disabled"]))
            self.assertFalse(self.app.stop_button.instate(["disabled"]))
            # The worker reads the profile asynchronously.
            deadline = time.monotonic() + 5
            while self.app.runner.busy and time.monotonic() < deadline:
                self.root.update()
                time.sleep(.01)
        self.app._update_status()
        self.assertFalse(self.app.stop_button.instate(["disabled"]))
        self.assertFalse(self.app.runner.busy)
        self.root.update()
        self.assertEqual(self.session.arrivals, [(0, 0)] + [(12.5, -12.5), (-8, 8), (0, 0), (180, 180), (0, 0)] * 2)
        self.app.stop_button.invoke()
        self.app._update_status()
        self.assertTrue(self.session.stopped)
        self.assertTrue(self.app.cycle_button.instate(["disabled"]))
        self.assertFalse(any(command == "arm" for command in self.session.commands))


if __name__ == "__main__":
    unittest.main()
