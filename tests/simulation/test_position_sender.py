"""SIM direction/speed TX and velocity feedback regression using production C.

Run: python3 tests/simulation/test_position_sender.py
"""
import math
import struct
from bench import Firmware, build
from sim import protocol as p


def run():
    fw = Firmware(build('position_sender'))
    now = sequence = 0
    outputs = []

    def send(kind, body=b''):
        nonlocal sequence
        sequence += 1
        wire = p.encode(kind, 1, 0 if kind == p.HELLO else sequence,
                        0 if kind == p.HELLO else now, body)
        fw.call('RX ' + wire.hex())

    def tick(positions=None):
        nonlocal now
        now += 10
        fw.call('TIME ' + str(now))
        send(p.HEARTBEAT)
        send(p.FEEDBACK, p.feedback(fw.state['run'], 0,
             [(i, 3, (positions or {}).get(i, 0.), 0., 0, 0.) for i in range(8)]))
        before = list(fw.motors)
        fw.call('STEP 1 1 1 0')
        for wire in fw.frames:
            kind, _, _, _, body = p.Parser().feed(wire, now)[0]
            if kind == p.OUTPUT:
                assert len(wire) == 80 and len(body) == 9 + 8 * 6
                run_id, epoch, mask = struct.unpack_from('<IIB', body)
                entries = [struct.unpack_from('<Bbf', body, 9 + 6*i) for i in range(8)]
                assert [e[0] for e in entries] == list(range(8))
                for mid, direction, speed in entries:
                    # Comms may flush staging before this tick's ControlTask.
                    assert any(direction == (motor['output'] > 0) - (motor['output'] < 0)
                               and math.isclose(speed, min(abs(motor['output']), 100.) * .05,
                                                rel_tol=1e-6, abs_tol=1e-8)
                               for motor in (before[mid], fw.motors[mid]))
                    assert direction in (-1, 0, 1) and math.isfinite(speed) and 0 <= speed <= 5
                    if not mask & (1 << mid):
                        assert direction == 0 and speed == 0
                outputs.append((run_id, epoch, mask, entries))
        fw.frames.clear()

    try:
        send(p.HELLO)
        fw.call('STEP 1 1 1 0')
        send(p.CONFIG, bytes((1, 0, 255, 0)))
        for _ in range(4): tick()
        send(p.RECOVER)
        tick()
        send(p.ARM)
        for _ in range(4): tick()
        send(p.ENABLE, b'\x85')  # IDs 0, 2, 7; others remain disabled.
        tick()
        send(p.TARGET, p.targets([(7, -.4, 60), (2, .5, 60), (0, .8, 1)]))
        for _ in range(8): tick()
        assert fw.state['armed'] and fw.state['causes'] == 0
        entries = outputs[-1][3]
        assert entries[7][1] == -1 and entries[7][2] > 0
        assert entries[2][1] == 1 and entries[2][2] > 0
        assert 0 < entries[0][2] <= .05000001  # 1% limit, fractional rad/s retained
        send(p.TARGET, p.targets([(0, 2., 30)]))
        for _ in range(80): tick()
        assert math.isclose(outputs[-1][3][0][2], 1.5, abs_tol=1e-7)  # 30% -> 1.5 rad/s
        for _ in range(4): tick({2: .8})
        assert fw.motors[2]['ref'] > 0 and outputs[-1][3][2][1] == -1
        count = len(outputs)
        send(p.STOP)
        for _ in range(4): tick()
        assert not fw.state['armed'] and len(outputs) == count
        # Inspect production feedback/guard state without changing the receiver.
        def sample(mid):
            fw.process.stdin.write('SAMPLE ' + str(mid) + '\n'); fw.process.stdin.flush()
            result = None
            while True:
                line = fw.process.stdout.readline().split()
                assert line, 'native firmware stopped during SAMPLE'
                if line[0] == 'G': result = tuple(map(float, line[1:]))
                if line[0] == 'E': return result

        velocities = [1.5, -1.5, 0., 5., -5., 6., -6., .0125]
        send(p.FEEDBACK, p.feedback(fw.state['run'], 0,
             [(i, 3, 0., v, 0, 0.) for i, v in enumerate(velocities)]))
        fw.call('STEP 0 0 1 0')
        for mid, expected in enumerate([30., -30., 0., 100., -100., 100., -100., .25]):
            valid, _, drive = sample(mid)
            assert valid == 1 and math.isclose(drive, expected, abs_tol=1e-5)
        send(p.FEEDBACK, p.feedback(fw.state['run'], 0, [(0, 11, 0., 1.5, 0, -40.)]))
        fw.call('STEP 0 0 1 0')
        assert sample(0)[2] == -40  # explicit APPLIED retains priority
        now += 101
        fw.call('TIME ' + str(now))
        send(p.HEARTBEAT)
        send(p.FEEDBACK, p.feedback(fw.state['run'], 0, [(0, 1, 0., 0., 0, 0.)]))
        fw.call('STEP 0 0 1 0')
        assert sample(0)[0] == 0  # position-only cannot refresh derived drive
        assert fw.diag[-2] == 0  # no physical hardware calls
    finally:
        fw.close()
    print('PASS SIM TX direction/speed, 30% -> 1.5 rad/s, zero/limits/STOP; RX velocity-only ARM, signed/clamped equivalent PWM, explicit APPLIED priority')


if __name__ == '__main__':
    run()
