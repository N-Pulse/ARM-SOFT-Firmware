#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
send_action.py — sender CLI pour la N-Pulse comm-stack.

Encode un DeviceMessage{data:{instruction:{action:X}}} en protobuf et l'envoie
sur l'UART au format [0xAA][length:1][payload:N].

Use:
    python tools/send_action.py --port COM3 --action close
    python tools/send_action.py --port COM3 --action open
    python tools/send_action.py --port COM3 --hello

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


def main() -> int:
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--port", default="COM3", help="Serial port (default: COM3)")
    p.add_argument("--baud", type=int, default=115200)
    g = p.add_mutually_exclusive_group(required=True)
    g.add_argument("--action", choices=ACTION_MAP.keys(), help="Send a ClassificationData action")
    g.add_argument("--hello", action="store_true", help="Send a Hello handshake message")
    p.add_argument("--device-id", type=int, default=1, help="device_id for Hello (default 1)")
    p.add_argument("--listen", type=float, default=1.0, help="Seconds to listen after sending (default 1.0)")
    args = p.parse_args()

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
