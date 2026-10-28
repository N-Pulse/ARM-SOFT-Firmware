#!/usr/bin/env python3
"""Finger-by-finger SIM console for the firmware, with fictitious rotors.

By default no board is needed: the production SIM controller/PID/safety/guard C code
is compiled for the PC (as tools/build_native_sim.py does) and driven over SIM v2 IPC.
With --port, the same console drives a board flashed with the SIM build over UART4.
Either way the explicit CONFIG -> RECOVER -> ARM -> ENABLE -> TARGET sequence is used,
and the rotor model runs in its own process with its own watchdog
(tools/sim_model_server.py). Only Python 3.9+ and a native gcc/clang are required
(plus pyserial for --port). No ROS is imported here, so ROS can never slow this loop.

    python3 tools/sim_fingers.py            # console only
    python3 tools/sim_fingers.py --gazebo   # also feed tools/ros2_bridge/fw_gazebo_bridge.py

Then, for example:
    finger index 30     index to 30 degrees (configures/arms/enables first if needed)
    pos                 every axis angle and applied PWM
    intent CLOSE_HAND   whole-hand intent through intent_router.c / motor_map.c
    stop                STOP; type start to recover/arm/enable again
Rotors are fictitious: angles prove the C control path, not the real hand.
"""
import argparse
import json
import math
import socket
import subprocess
import sys
import threading
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
sys.path.insert(0, str(ROOT / 'tests/simulation'))
from sim.client import SerialWorker, submit_command  # noqa: E402
from sim.model_server import ModelConnection  # noqa: E402

MOTORS = ('thumb', 'index', 'middle', 'ring', 'little', 'wrist_x', 'wrist_y', 'palm')
INTENTS = ('OPEN_HAND', 'CLOSE_HAND', 'PINCH', 'ROTATE_WRIST_R', 'ROTATE_WRIST_L')
HELP = ('start | finger NAME DEG [PWM] | intent NAME | pos | status | stop | quit\n'
        '  finger NAME: ' + ', '.join(MOTORS) + ' (or 0..7); PWM = % ceiling, default 60\n'
        '  intent NAME: ' + ', '.join(INTENTS))
GAZEBO_PORT = 48170  # rotor angles -> fw_gazebo_bridge.py; commands come back on +1


class WatchedModel(ModelConnection):
    """ModelConnection that keeps the latest rotor state and optionally mirrors it."""
    def __init__(self, port, mirror_port=None):
        super().__init__(port)
        self.position = [0.0] * 8
        self.applied = [0.0] * 8
        self.mirror = None
        if mirror_port:
            self.mirror = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            self.mirror.setblocking(False)
            self.mirror_address = ('127.0.0.1', mirror_port)

    def update(self, peer, now):
        if peer.session != self.session:
            self.session = peer.session; self.seq = 0; self.permission = None
            self.send('begin', now)
        if peer.permitted_run != self.permission:
            self.permission = peer.permitted_run
            self.send('stop' if self.permission is None else 'arm', now, run=self.permission)
        if peer.output_revision != self.revision:
            self.revision = peer.output_revision
            if self.permission is not None:
                self.send('output', now, run=self.permission, values=peer.outputs)
        if now - self.last_query >= 20:
            self.last_query = now; self.send('query', now)
        for _ in range(8):
            try:
                data, address = self.socket.recvfrom(4096)
            except BlockingIOError:
                break
            try:
                m = json.loads(data)
                if address != self.address or m['session'] != self.session or not 0 <= now - m['ms'] <= 50:
                    continue
                self.position = list(m['position']); self.applied = list(m['applied'])
                entries = [(i, 11, m['position'][i], m['velocity'][i], 0, m['applied'][i]) for i in range(8)]
                peer.submit_feedback(entries, m['ms'])
                if self.mirror:
                    try:
                        self.mirror.sendto(json.dumps({'position': self.position}).encode(), self.mirror_address)
                    except OSError:
                        pass
            except (ValueError, KeyError, TypeError, IndexError):
                continue

    def close(self):
        super().close()
        if self.mirror:
            self.mirror.close()


def wait_for(predicate, timeout_s):
    end = time.monotonic() + timeout_s
    while time.monotonic() < end:
        if predicate():
            return True
        time.sleep(0.01)
    return False


def command(peer, text, timeout_s=1.0):
    """Queue one explicit command and wait for the firmware's reply to it."""
    previous = set(peer.replies)
    if not submit_command(peer, text):
        return False
    if not wait_for(lambda: bool(set(peer.replies) - previous), timeout_s):
        return False
    return peer.replies[next(iter(set(peer.replies) - previous))]


def motor_id(word):
    if word in MOTORS:
        return MOTORS.index(word)
    value = int(word)
    if not 0 <= value < 8:
        raise ValueError('motor 0..7 or a name')
    return value


class Session:
    """Rotor model process + firmware (PC process or board) + the single I/O worker."""
    def __init__(self, args):
        self.args = args
        self.losses = 0
        self.lock = threading.Lock()  # console and Gazebo commands never interleave
        self.binary = None
        if not args.port:
            if args.no_build:
                self.binary = ROOT / 'build/simulation/firmware-realtime'
            else:
                from bench import build
                print('Building the native SIM firmware (real C)...')
                self.binary = build('firmware-realtime')
        self.server = subprocess.Popen([sys.executable, str(ROOT / 'tools/sim_model_server.py'),
                                        '--port', str(args.model_port)], cwd=ROOT / 'tools')
        time.sleep(0.3)
        if self.server.poll() is not None:
            raise SystemExit(f'The rotor model could not start on UDP {args.model_port}: is another '
                             'sim_fingers.py still running? Quit it (or pkill -f sim_model_server) and retry.')
        self.model = WatchedModel(args.model_port, GAZEBO_PORT if args.gazebo else None)
        self.worker = None
        self.spawn()

    def spawn(self):
        """(Re)start the firmware link. A failed PC firmware process is replaced by a new one."""
        if self.worker:
            self.worker.close()
        transport = None
        if self.binary:
            from sim.native_transport import NativeTransport
            transport = NativeTransport(self.binary)
        self.worker = SerialWorker(self.args.port, transport=transport, on_state=self.model.update)
        self.worker.start()
        print('Firmware link up.' if self.connect() else 'Firmware not answering yet; type start to retry.')

    def connect(self, attempts=10):
        """Hello must be answered within the 80 ms lease. A firmware that is still starting
        misses the first one, so open a NEW session and retry."""
        peer = self.worker.peer
        for _ in range(attempts):
            if wait_for(lambda: peer.live, 0.5):
                return True
            with peer.lock:
                peer.connect(max(peer.session + 1, time.time_ns()), int(time.monotonic_ns() // 1_000_000))
        return wait_for(lambda: peer.live, 0.5)

    def start(self):
        if not self.worker.running:
            print(f'Firmware I/O stopped ({self.worker.error}); restarting it...')
            self.spawn()
        peer = self.worker.peer
        if not peer.live and not self.connect():
            print('No Hello ACK from the firmware (is it running? check the build output).'); return False
        if not peer.configured:  # once per session; after a STOP only RECOVER is needed
            if not command(peer, 'config') or not wait_for(lambda: peer.configured, 1):
                print('CONFIG not acknowledged.'); return False
        time.sleep(0.1)  # fresh measurements before recovery
        if not command(peer, 'recover'):
            print(f'RECOVER refused, causes={peer.causes}.'); return False
        time.sleep(0.1)
        if not command(peer, 'arm') or peer.permitted_run is None:
            print(f'ARM refused, causes={peer.causes}.'); return False
        time.sleep(0.1)  # measurements of the new run before ENABLE
        if not command(peer, 'enable 0xff'):
            print(f'ENABLE refused, causes={peer.causes}.'); return False
        print('Ready: 8 simulated axes enabled.')
        return True

    def ensure_ready(self):
        """After a LINK loss (the PC/VM paused us), redo the explicit sequence. A STOP or a
        firmware safety stop (causes != 0) is never cleared without the operator's start."""
        peer = self.worker.peer
        if self.worker.running and peer.live and peer.permitted_run is not None and not peer.causes:
            return True
        if self.worker.running and peer.live and peer.causes:
            print(f'Firmware stopped (causes={peer.causes}: {describe(peer.causes)}). '
                  'Type start to recover/arm/enable.')
            return False
        self.losses += 1
        reason = self.worker.error or peer.watchdog
        print(f'Firmware link lost ({reason}); reconnecting...')
        return self.start()

    def move(self, mid, deg, pwm=60):
        if not self.ensure_ready():
            return False
        ok = command(self.worker.peer, f'target {mid} {math.radians(deg):.6f} {pwm}')
        print(f'{MOTORS[mid]} -> {deg:g} deg (pwm <= {pwm}%)' if ok else
              f'Refused (causes={self.worker.peer.causes}).')
        return ok

    def intent(self, name):
        if not self.ensure_ready():
            return False
        ok = command(self.worker.peer, f'intent {name}')
        print(f'{name} accepted' if ok else f'Refused (causes={self.worker.peer.causes}).')
        return ok

    def handle(self, words):
        """Execute one console line. Returns False on quit."""
        if not words:
            return True
        with self.lock:
            try:
                if words[0] == 'quit':
                    return False
                if words[0] == 'start':
                    self.start()
                elif words[0] == 'pos':
                    for i, name in enumerate(MOTORS):
                        print(f'  {i} {name:8s} {math.degrees(self.model.position[i]):7.2f} deg   '
                              f'pwm {self.model.applied[i]:6.2f} %')
                elif words[0] == 'status':
                    p = self.worker.peer
                    print(f'live={p.live} configured={p.configured} armed={p.permitted_run is not None} '
                          f'enabled=0x{p.enabled:02x} causes={p.causes} ({describe(p.causes)}) '
                          f'last_inhibit={p.watchdog!r} link_losses={self.losses}')
                elif words[0] == 'stop':
                    submit_command(self.worker.peer, 'stop')
                    print('STOP sent. Type start to recover/arm/enable again.')
                elif words[0] in ('finger', 'motor') and len(words) in (3, 4):
                    self.move(motor_id(words[1]), float(words[2]), int(words[3]) if len(words) == 4 else 60)
                elif words[0] == 'intent' and len(words) == 2 and words[1] in INTENTS:
                    self.intent(words[1])
                else:
                    print(HELP)
            except ValueError as exc:
                print(exc)
        return True

    def close(self):
        self.worker.close(); self.model.close(); self.server.terminate()


CAUSES = ('MANUAL', 'GUARD', 'FEEDBACK', 'CONTROL_DELAY', 'FORCE', 'DRIVER', 'PID', 'LINK')


def describe(causes):
    return '|'.join(name for i, name in enumerate(CAUSES) if causes & (1 << i)) or 'none'


def serve_gazebo_commands(session, sock):
    """Commands from fw_gazebo_bridge.py (/firmware_intent), same syntax as the console."""
    while True:
        data, _ = sock.recvfrom(256)
        words = data.decode(errors='replace').split()
        if words and words[0] in ('finger', 'motor', 'intent', 'stop'):
            print(f'\n[from ROS] {" ".join(words)}')
            session.handle(words)


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--port', help='board flashed with the SIM build (UART4); default: firmware on this PC')
    parser.add_argument('--model-port', type=int, default=48160)
    parser.add_argument('--no-build', action='store_true', help='reuse build/simulation/firmware-realtime')
    parser.add_argument('--gazebo', action='store_true',
                        help=f'send rotor angles to fw_gazebo_bridge.py (UDP {GAZEBO_PORT}) and accept its commands')
    args = parser.parse_args()
    commands = None
    if args.gazebo:
        commands = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        try:
            commands.bind(('127.0.0.1', GAZEBO_PORT + 1))
        except OSError:
            raise SystemExit(f'UDP {GAZEBO_PORT + 1} is busy: is another sim_fingers.py --gazebo running?')
    session = Session(args)
    if args.gazebo:
        threading.Thread(target=serve_gazebo_commands, args=(session, commands), daemon=True).start()
        print(f'Sending rotor angles to fw_gazebo_bridge.py on UDP {GAZEBO_PORT}.')
    session.start()
    print(HELP)
    try:
        while True:
            try:
                words = input('sim> ').split()
            except EOFError:
                break
            if not session.handle(words):
                break
    except KeyboardInterrupt:
        pass
    finally:
        session.close()


if __name__ == '__main__':
    main()