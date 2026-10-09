"""Explicit SIM v2 codec; no ROS2, protobuf or serial dependency."""
import math
import struct

VERSION = 2
MAX_PAYLOAD = 144
HELLO, HEARTBEAT, CONFIG, RECOVER, ARM, ENABLE, TARGET, STOP, FEEDBACK = range(1, 10)
HELLO_ACK, STATUS, OUTPUT = 0x81, 0x82, 0x83
POSITION, VELOCITY, FORCE, APPLIED = 1, 2, 4, 8
NAMES = ('thumb', 'index', 'middle', 'ring', 'little', 'rotor_5', 'rotor_6', 'palm')
BAUD = 115200
LINK_MS, COMMAND_MS, LEASE_MS, SAMPLE_MS = 150, 100, 80, 50

def newer(a, b):
    return 0 < ((a-b) & 0xffffffff) < 0x80000000

def crc16(data):
    crc = 0xffff
    for b in data:
        crc ^= b << 8
        for _ in range(8):
            crc = ((crc << 1) ^ (0x1021 if crc & 0x8000 else 0)) & 0xffff
    return crc

def encode(kind, session, sequence, stamp, body=b''):
    if not 0 < session < 1 << 64 or not 0 <= sequence < 1 << 32 or not 0 <= stamp < 1 << 32:
        raise ValueError('session/sequence/stamp outside wire representation')
    payload = struct.pack('<QII', session, sequence, stamp) + body
    if len(payload) > MAX_PAYLOAD:
        raise ValueError('payload too large')
    content = bytes((VERSION, kind, len(payload))) + payload
    return b'\xa5\x5a' + content + struct.pack('<H', crc16(content))

def finite(value, limit=1000.0):
    value = float(value)
    if not math.isfinite(value) or abs(value) > limit:
        raise ValueError('nonfinite or unrepresentable SIM value')
    # Exercise binary32 representability, rather than relying on Python float.
    converted = struct.unpack('<f', struct.pack('<f', value))[0]
    if value != 0 and (converted == 0 or abs(converted) < 2**-126):
        raise ValueError('SIM binary32 underflow/subnormal')
    return converted

def targets(entries):
    if not 1 <= len(entries) <= 8 or len({e[0] for e in entries}) != len(entries):
        raise ValueError('count/duplicate motor')
    out = bytearray((len(entries),))
    for mid, position, limit in entries:
        if mid not in range(8) or not isinstance(limit, int) or not 0 <= limit <= 100:
            raise ValueError('motor/limit')
        out.extend(struct.pack('<BfB', mid, finite(position, 2), limit))
    return bytes(out)

def feedback(run, age_ms, entries):
    # Entries: (ID, valid_fields, position_rad, velocity_rad_s, force_mN, applied_percent).
    if not 0 <= run < 1 << 32 or not 0 <= age_ms <= SAMPLE_MS or not 1 <= len(entries) <= 8:
        raise ValueError('feedback header')
    if len({e[0] for e in entries}) != len(entries):
        raise ValueError('duplicate motor')
    out = bytearray(struct.pack('<IHB', run, age_ms, len(entries)))
    for mid, fields, pos, vel, force, applied in entries:
        if mid not in range(8) or not 0 < fields <= 15:
            raise ValueError('motor/valid fields')
        pos, vel, applied = finite(pos), finite(vel), finite(applied, 100)
        if not isinstance(force, int) or not 0 <= force <= 65535:
            raise ValueError('force')
        if ((not fields & POSITION and pos != 0) or (not fields & VELOCITY and vel != 0) or
            (not fields & FORCE and force != 0) or (not fields & APPLIED and applied != 0)):
            raise ValueError('absent fields must have canonical zero encoding')
        if not fields & POSITION: pos = 0.0
        if not fields & VELOCITY: vel = 0.0
        out.extend(struct.pack('<BBffHh', mid, fields, pos, vel, force, round(applied * 100)))
    return bytes(out)

class Parser:
    """Bounded suffix resynchronisation with 15-ms interbyte / 50-ms total limits."""
    def __init__(self):
        self.buf = bytearray()
        self.times = []
        self.invalid = self.timeouts = 0

    def poll(self, now):
        if self.buf and (now-self.times[-1] > 15 or now-self.times[0] > 50):
            self.buf.clear(); self.times.clear(); self.timeouts += 1

    def feed(self, data, now):
        frames = []
        for byte in data:
            self.poll(now)
            self.buf.append(byte); self.times.append(now)
            while self.buf:
                if self.buf[0] != 0xa5 or (len(self.buf) >= 2 and self.buf[1] != 0x5a):
                    del self.buf[0]; del self.times[0]; continue
                if len(self.buf) < 5:
                    break
                n = self.buf[4]
                if self.buf[2] != VERSION or not 16 <= n <= MAX_PAYLOAD:
                    del self.buf[0]; del self.times[0]; self.invalid += 1; continue
                if len(self.buf) < n+7:
                    break
                wire = self.buf[:n+7]
                if crc16(wire[2:-2]) != struct.unpack('<H', wire[-2:])[0]:
                    del self.buf[0]; del self.times[0]; self.invalid += 1; continue
                sid, seq, stamp = struct.unpack_from('<QII', wire, 5)
                frames.append((wire[3], sid, seq, stamp, bytes(wire[21:-2])))
                del self.buf[:n+7]; del self.times[:n+7]
        return frames
