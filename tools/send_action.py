#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
send_action.py — sender CLI pour la N-Pulse comm-stack.

Encode un DeviceMessage{data:{instruction:{action:X}}} en protobuf et l'envoie
sur l'UART au format [0xAA][length:1][payload:N].

Use:
    python tools/send_action.py --port COM8 --raw 11 --angle 5 --pwm 3 --run
    python tools/send_action.py --port COM8 --raw 10 --angle 30 --max-angle 30 --pwm 25 --run --diagnostic
    python tools/send_action.py --port COM3 --action close
    python tools/send_action.py --port COM3 --action open
    python tools/send_action.py --port COM3 --hello
    python tools/send_action.py --port COM3 --finger index --angle 30 --pwm 3
    python tools/send_action.py --port COM3 --raw 11 --angle 30 --pwm 3

With --run, one explicitly requested test performs setup, arms, moves, sends
STOP and exits. Default test travel is 0..10 degrees; --max-angle overrides it.
The option confirms the operator's output preflight. It does not measure wiring.
--diagnostic requires build/flash -Fingers -Diagnostic on both boards, applies
constant requested PWM (1..40%) for an estimated travel duration, at most 2s.
--from-angle explicitly supplies an estimated starting position after reboot.
Without --run, single-finger commands open the commissioning console on MASTER.
The common -Fingers image uses the legacy nominal encoder scale and boot zero.
Enter measured limits, link-recover, preflight verified, arm,
enable, then move. --raw 10..14/20..24 are legacy finger aliases.

Pre-req:
    pip install pyserial protobuf
"""

import argparse
import sys
import time
from pathlib import Path

# Make protocol_messages_pb2 importable from the comm-stack PyUART folder.
HERE = Path(__file__).resolve().parent
PROTO_DIR = HERE.parent / "fw" / "comm-stack" / "PyUART"
if str(PROTO_DIR) not in sys.path:
    sys.path.insert(0, str(PROTO_DIR))

try:
    import serial
except ImportError:
    print("[!] pyserial missing:  pip install pyserial")
    sys.exit(1)

try:
    import protocol_messages_pb2 as pb
except ImportError as e:
    print(f"[!] cannot import protocol_messages_pb2 from {PROTO_DIR}")
    print(f"    {e}")
    sys.exit(1)


SYNC_BYTE = 0xAA
ACTION_MAP = {
    "open":     pb.ClassificationData.OPEN_HAND,
    "close":    pb.ClassificationData.CLOSE_HAND,
    "pinch":    pb.ClassificationData.PINCH,
    "wrist_r":  pb.ClassificationData.ROTATE_WRIST_R,
    "wrist_l":  pb.ClassificationData.ROTATE_WRIST_L,
}


def build_action_frame(action_name: str) -> bytes:
    """Build [0xAA][len][DeviceMessage{data:{instruction:{action}}}]."""
    cls = pb.ClassificationData()
    cls.action = ACTION_MAP[action_name]

    data = pb.Data()
    data.instruction.CopyFrom(cls)

    dev = pb.DeviceMessage()
    dev.data.CopyFrom(data)

    payload = dev.SerializeToString()
    if len(payload) > 255:
        raise ValueError(f"payload too large for 1-byte length field: {len(payload)}")
    return bytes([SYNC_BYTE, len(payload)]) + payload


def build_hello_frame(device_id: int = 1, proto_ver: int = 1, supported_modes: int = 1) -> bytes:
    hello = pb.Hello()
    hello.device_id = device_id
    hello.proto_ver = proto_ver
    hello.supported_modes = supported_modes

    dev = pb.DeviceMessage()
    dev.hello.CopyFrom(hello)

    payload = dev.SerializeToString()
    return bytes([SYNC_BYTE, len(payload)]) + payload


def hex_dump(data: bytes) -> str:
    return " ".join(f"{b:02x}" for b in data)


def listen_briefly(ser: serial.Serial, duration_s: float = 0.5) -> None:
    """Print any bytes received in the next `duration_s`."""
    end = time.time() + duration_s
    chunks = []
    while time.time() < end:
        b = ser.read(64)
        if b:
            chunks.append(b)
    if chunks:
        all_b = b"".join(chunks)
        print(f"[<-] {len(all_b)} bytes:  {hex_dump(all_b)}")


def main(argv=None) -> int:
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--port", default="COM3", help="Serial port (default: COM3)")
    p.add_argument("--baud", type=int, default=115200)
    g = p.add_mutually_exclusive_group(required=True)
    g.add_argument("--action", choices=ACTION_MAP.keys(), help="Send a ClassificationData action")
    g.add_argument("--hello", action="store_true", help="Send a Hello handshake message")
    g.add_argument("--finger", choices=("thumb","index","middle","ring","little"), help="Open a single-finger commissioning console through the MASTER")
    g.add_argument("--raw", type=int, choices=(*range(10,15),*range(20,25)), help="Legacy single-finger alias: 10..14 default to 30 degrees; 20..24 to 0")
    p.add_argument("--angle", type=float, help="Single-finger target angle in degrees (0..90), staged until move")
    p.add_argument("--pwm", type=int, default=3, help="PWM ceiling, or constant PWM with --diagnostic (default 3 percent)")
    p.add_argument("--device-id", type=int, default=1, help="device_id for Hello (default 1)")
    p.add_argument("--verbose", action="store_true", help="Print every single-finger diagnostic sample")
    p.add_argument("--run", action="store_true", help="Authorize one finger test: configure, arm, move, stop and exit")
    p.add_argument("--diagnostic", action="store_true", help="Use diagnostic firmware: PWM 1..40, timed estimated movement, cutoff <=2s")
    p.add_argument("--max-angle", type=float, default=10, help="Travel ceiling for --run, in degrees (default 10)")
    p.add_argument("--from-angle", type=float, help="Estimated current angle for timed diagnostics after reboot; not encoder calibration")
    p.add_argument("--listen", type=float, default=1.0, help="Seconds to listen after sending (default 1.0)")
    args = p.parse_args(argv)

    if args.finger or args.raw is not None:
        if args.baud!=115200: p.error('OLD_HW commissioning uses 115200 baud')
        from hw_monitor import main as monitor
        fingers=("thumb","index","middle","ring","little")
        finger=args.finger or fingers[args.raw%10]
        angle=args.angle if args.angle is not None else (0 if args.raw is not None and args.raw>=20 else 30)
        return monitor(['--port',args.port,'--finger',finger,'--angle',str(angle),'--pwm',str(args.pwm),
                        '--device-id',str(args.device_id)]+(['--verbose'] if args.verbose else [])+
                       (['--run','--max-angle',str(args.max_angle)] if args.run else [])+
                       (['--diagnostic'] if args.diagnostic else [])+
                       (['--from-angle',str(args.from_angle)] if args.from_angle is not None else []))
    if args.angle is not None: p.error('--angle requires --finger or --raw')
    if args.run: p.error('--run requires --finger or --raw')
    if args.diagnostic: p.error('--diagnostic requires --finger or --raw and --run')
    if args.from_angle is not None: p.error('--from-angle requires --finger or --raw and --diagnostic --run')

    if args.hello:
        frame = build_hello_frame(device_id=args.device_id)
        label = f"Hello(device_id={args.device_id})"
    else:
        frame = build_action_frame(args.action)
        label = f"Action({args.action.upper()})"

    print(f"[->] {label}  on {args.port} @ {args.baud}")
    print(f"     frame = {hex_dump(frame)}  ({len(frame)} bytes)")

    try:
        with serial.Serial(args.port, args.baud, timeout=0.05) as ser:
            # Drain anything stale already on the line (boot beacons, leftover
            # bytes from previous runs) so the response we capture below is
            # only what the chip emitted IN REPLY to our message.
            pre_drain = bytearray()
            drain_deadline = time.monotonic() + 1.0
            quiet_deadline = drain_deadline
            while time.monotonic() < min(drain_deadline, quiet_deadline):
                b = ser.read(64)
                if b:
                    pre_drain.extend(b)
                    quiet_deadline = time.monotonic() + 0.3
            if pre_drain:
                print(f"[..] drained {len(pre_drain)} stale bytes:  {hex_dump(pre_drain)}")

            ser.write(frame)
            ser.flush()
            print(f"     sent OK")
            if args.listen > 0:
                listen_briefly(ser, args.listen)
    except serial.SerialException as e:
        print(f"[!] serial error: {e}")
        return 1

    return 0


if __name__ == "__main__":
    sys.exit(main())
