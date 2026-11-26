#!/usr/bin/env python3
"""Whole-hand native C operator console; control/feedback run in the physics controller."""
import argparse
import csv
import math
from pathlib import Path
import threading
import time

MOTORS = ('thumb', 'index', 'middle', 'ring', 'little', 'wrist_x', 'wrist_y', 'palm')
ACTIONS = ('OPEN_HAND', 'CLOSE_HAND', 'PINCH', 'ROTATE_WRIST_R', 'ROTATE_WRIST_L',
           'THUMB_OPEN', 'THUMB_CLOSE', 'INDEX_OPEN', 'INDEX_CLOSE',
           'MIDDLE_OPEN', 'MIDDLE_CLOSE', 'RING_OPEN', 'RING_CLOSE',
           'LITTLE_OPEN', 'LITTLE_CLOSE', 'WRIST_PALM', 'WRIST_DORSI')
FIELDS = ('target', 'reference', 'position', 'velocity', 'requested_percent',
          'applied_percent', 'feedback_age_ms', 'kp', 'ki', 'kd')
GROUPS = ((0, 2), (2, 3), (5, 3), (8, 3), (11, 3))
HELP = ('start | neutral | rest | intent NAME | finger NAME DEG [PWM] | '
        'pos | status | test | stop | quit')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--limit', type=int, default=15)
    parser.add_argument('--csv', default='build/simulation/wholehand.csv')
    args = parser.parse_args()
    if not 1 <= args.limit <= 100:
        parser.error('--limit must be 1..100')
    import rclpy
    from rclpy.node import Node
    from rclpy.qos import QoSProfile, ReliabilityPolicy
    from npulse_sim_control.msg import HandCommand, HandState

    class Console(Node):
        def __init__(self):
            super().__init__('npulse_wholehand_console')
            self.lock = threading.RLock()
            self.sample = None
            self.session = time.time_ns()
            self.run = self.sequence = self.revision = 0
            self.enabled = False
            self.action, self.mask = -2, 255
            self.targets = [0.0] * 8
            self.limit = args.limit
            self.labels = {0: 'idle'}
            self.last_sim = self.last_phase = None
            self.progressed_at = None
            self.testing = False
            self.pub = self.create_publisher(HandCommand, '/fw_hand_controller/hand_command', 1)
            self.create_subscription(HandState, '/fw_hand_controller/hand_state', self.receive,
                                     QoSProfile(depth=1, reliability=ReliabilityPolicy.BEST_EFFORT))
            path = Path(args.csv)
            path.parent.mkdir(parents=True, exist_ok=True)
            self.file = path.open('w', newline='')
            self.csv = csv.writer(self.file)
            self.csv.writerow(['wall_ns', 'sim_ms', 'run', 'phase', 'causes', 'revision', 'command',
                               'preserve_others'] + [f'{motor}_{field}' for motor in MOTORS for field in FIELDS]
                              + [f'joint_{i}_{field}' for field in ('position', 'velocity', 'effort') for i in range(17)])
            self.create_timer(.05, self.send)
            print('CSV:', path.resolve())

        def receive(self, state):
            with self.lock:
                self.sample = state
                if state.sim_ms != self.last_sim:
                    self.last_sim = state.sim_ms
                    self.progressed_at = time.monotonic()
                    self.csv.writerow([state.wall_ns, state.sim_ms, state.firmware_run, state.phase,
                                       state.causes, state.revision, self.labels.get(state.revision, ''),
                                       state.preserve_others]
                                      + [getattr(state, field)[i] for i in range(8) for field in FIELDS]
                                      + list(state.joint_position) + list(state.joint_velocity) + list(state.joint_effort))
                    self.file.flush()
                if state.phase != self.last_phase:
                    self.last_phase = state.phase
                    print(f'\nC firmware phase={state.phase} (0 stopped, 1 arming, 2 active, 3 fault), causes={state.causes}', flush=True)
                if state.phase == 3 and state.operator_session == self.session and state.operator_run == self.run:
                    self.enabled = False

        def send(self):
            with self.lock:
                stale = (self.sample is None or time.monotonic_ns() - self.sample.wall_ns > 500_000_000
                         or self.progressed_at is None or time.monotonic() - self.progressed_at > .5)
                if self.enabled and stale:
                    self.enabled = False
                    print('Telemetry unavailable/frozen: STOP requested.', flush=True)
                self.sequence += 1
                msg = HandCommand()
                msg.session, msg.run, msg.sequence = self.session, self.run, self.sequence
                msg.wall_ns, msg.enabled, msg.revision = time.monotonic_ns(), self.enabled, self.revision
                msg.action, msg.target_mask, msg.targets, msg.limit = self.action, self.mask, self.targets, self.limit
                self.pub.publish(msg)

        def command(self, action, label, mask=255, targets=None, limit=None):
            self.action, self.mask = action, mask
            self.targets = list(targets or [0.0] * 8)
            self.limit = args.limit if limit is None else limit
            self.revision += 1
            self.labels[self.revision] = label
            self.send()
            print(f'Requested {label}; ceiling {self.limit}%. Acceptance is not convergence.')

        def print_state(self):
            s = self.sample
            if s is None:
                print('No controller state received.'); return
            print(f'phase={s.phase} armed={s.armed} enabled=0x{s.enabled_mask:02x} causes={s.causes} '
                  f'telemetry_age={(time.monotonic_ns()-s.wall_ns)/1e6:.0f} ms preserve_others={s.preserve_others}')
            for i, name in enumerate(MOTORS):
                spread = ''
                if i < 5:
                    first, n = GROUPS[i]
                    q = s.joint_position[first:first+n]
                    spread = f' spread={math.degrees(max(q)-min(q)):.2f} deg'
                print(f'  {name:8} target={math.degrees(s.target[i]):7.2f} ref={math.degrees(s.reference[i]):7.2f} '
                      f'measured={math.degrees(s.position[i]):7.2f} deg '
                      f'u={s.requested_percent[i]:+6.2f}% applied={s.applied_percent[i]:+6.2f}% '
                      f'C_age={s.feedback_age_ms[i]} ms{spread}')
            print('wrist_x/wrist_y above are differential MOTOR coordinates, not individual wrist joints.')

        def suite(self):
            """Check convergence against actual C targets, never force positions."""
            try:
                sequence = ['NEUTRAL', 'OPEN_HAND', 'CLOSE_HAND', 'OPEN_HAND', 'PINCH', 'NEUTRAL']
                sequence += [f'{finger}_{direction}' for finger in ('THUMB','INDEX','MIDDLE','RING','LITTLE')
                             for direction in ('CLOSE','OPEN')]
                sequence += ['WRIST_PALM', 'WRIST_DORSI', 'NEUTRAL']
                for name in sequence:
                    with self.lock:
                        if not self.enabled: raise RuntimeError('Not active; test aborted.')
                        self.command(-3 if name == 'NEUTRAL' else ACTIONS.index(name), 'HOLD_CURRENT' if name == 'NEUTRAL' else name)
                        revision, start_ms = self.revision, self.sample.sim_ms
                    settled_since = None
                    while True:
                        time.sleep(.05)
                        with self.lock:
                            s = self.sample
                            if not self.enabled or s.phase == 3: raise RuntimeError('Stopped/fault; test aborted.')
                            if s.sim_ms - start_ms > 15000: raise RuntimeError(f'{name}: no convergence in 15 simulated seconds.')
                            error = max(abs(a-b) for a,b in zip(s.target,s.position))
                            speed = max(abs(x) for x in s.velocity)
                            spread = max(max(s.joint_position[a:a+n])-min(s.joint_position[a:a+n]) for a,n in GROUPS)
                            good = (s.phase == 2 and s.enabled_mask == 255 and s.revision == revision
                                    and error < math.radians(2) and spread < math.radians(2) and speed < .05)
                            if good:
                                if settled_since is None: settled_since = s.sim_ms
                                if s.sim_ms - settled_since >= 500:
                                    print(f'PASS {name}: max error={math.degrees(error):.2f} deg, spread={math.degrees(spread):.2f} deg', flush=True)
                                    break
                            else: settled_since = None
                print('SUITE PASS: sampled convergence, no object/grasp-force validation.', flush=True)
            except RuntimeError as exc:
                with self.lock:
                    self.enabled = False; self.send()
                print(f'TEST STOP: {exc}', flush=True)
            finally:
                with self.lock: self.testing = False

        def execute(self, text):
            words = text.split()
            if not words: return True
            with self.lock:
                if words[0] in ('stop', 'quit') and len(words) == 1:
                    self.enabled = False; self.send(); print('STOP requested.')
                    return words[0] != 'quit'
                if words in (['pos'], ['status']): self.print_state(); return True
                if self.testing:
                    print('Test running; use pos, status or stop.'); return True
                if words == ['start']:
                    if self.enabled: print('Already active/requested.'); return True
                    if self.sample is None or time.monotonic_ns()-self.sample.wall_ns > 500_000_000:
                        print('No fresh state; no start.'); return True
                    self.run += 1; self.enabled = True
                    self.command(-3, 'HOLD_CURRENT'); return True
                if not self.enabled:
                    print('Use start explicitly first.'); return True
                if words == ['rest']:
                    print('REST: keep current targets; PID remains active. Use stop to release.'); return True
                if words == ['neutral']:
                    self.command(-3, 'HOLD_CURRENT'); return True
                if words == ['test']:
                    self.testing = True
                    threading.Thread(target=self.suite, daemon=True).start(); return True
                if len(words) == 2 and words[0] == 'intent':
                    name = words[1].upper()
                    if name in ('POWER_GRASP', 'CYLINDRICAL_GRASP'):
                        print('Alias to C CLOSE_HAND only; no distinct object-dependent power-grasp controller.')
                        name = 'CLOSE_HAND'
                    if name not in ACTIONS: raise ValueError('Available intents: ' + ', '.join(ACTIONS))
                    self.command(ACTIONS.index(name), name); return True
                if len(words) in (3,4) and words[0] == 'finger':
                    index = MOTORS.index(words[1]) if words[1] in MOTORS else int(words[1])
                    angle = math.radians(float(words[2]))
                    limit = int(words[3]) if len(words) == 4 else args.limit
                    if not 0 <= index < 8 or not math.isfinite(angle) or abs(angle)>2 or not 1<=limit<=100:
                        raise ValueError('Motor 0..7; finite angle +/-2 rad; ceiling 1..100%.')
                    targets = [0.0] * 8; targets[index] = angle
                    self.command(-1, f'{MOTORS[index]} {words[2]} deg', 1<<index, targets, limit); return True
                print(HELP); return True

    rclpy.init()
    node = Console()
    thread = threading.Thread(target=rclpy.spin, args=(node,), daemon=True); thread.start()
    print('Whole-hand native C effort loop. No gravity; uncalibrated motor mechanics.')
    print(HELP)
    try:
        while True:
            try:
                if not node.execute(input('hand-fw> ')): break
            except (ValueError, IndexError) as exc: print(exc)
    except (EOFError, KeyboardInterrupt): pass
    finally:
        with node.lock: node.enabled = False; node.send()
        # Allow an in-flight suite to observe STOP before shutting down ROS.
        time.sleep(.15)
        rclpy.shutdown(); thread.join(timeout=1)
        node.file.close(); node.destroy_node()


if __name__ == '__main__':
    main()
