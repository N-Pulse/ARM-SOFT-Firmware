"""HW protobuf Hello maintenance client; no arming/recovery/serial SIM framing.

Run manually on an electrically confirmed UART4 connection, motor power off.
Requires existing protobuf bindings and pyserial. No port opens on import.
"""
from pathlib import Path
import argparse
import sys
import time

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'fw/comm-stack/PyUART'))
import protocol_messages_pb2 as pb

def frame(message):
    payload = message.SerializeToString()
    if not 1 <= len(payload) <= 128:
        raise ValueError('HW protobuf payload must contain 1..128 bytes')
    return b'\xaa' + bytes([len(payload)]) + payload

def hello_frame(device_id=1):
    message = pb.DeviceMessage()
    message.hello.device_id = device_id
    message.hello.proto_ver = 1
    message.hello.supported_modes = 1
    return frame(message)

def action_frame(action):
    if action not in range(5):
        raise ValueError('Unknown action')
    message = pb.DeviceMessage()
    message.data.instruction.action = action
    return frame(message)

def main():
    import serial
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--port', required=True)
    parser.add_argument('--duration', type=float, default=10.0)
    parser.add_argument('--hello-period', type=float, default=0.5)
    parser.add_argument('--device-id', type=int, default=1)
    parser.add_argument('--action', type=int, choices=range(5), help='Optional single command after first Hello')
    args = parser.parse_args()
    if args.duration <= 0 or not 0 < args.hello_period < 2:
        parser.error('duration > 0 and 0 < hello-period < 2 seconds required')
    # One thread owns all whole-frame writes, with bounded serial waits.
    with serial.Serial(args.port, 115200, timeout=0.05, write_timeout=0.1) as port:
        end, next_hello = time.monotonic() + args.duration, 0.0
        command = args.action
        rx = bytearray()
        while time.monotonic() < end:
            now = time.monotonic()
            if now >= next_hello:
                outgoing = hello_frame(args.device_id)
                if port.write(outgoing) != len(outgoing): raise RuntimeError('Short Hello write')
                next_hello = now + args.hello_period
                if command is not None:
                    outgoing = action_frame(command)
                    if port.write(outgoing) != len(outgoing): raise RuntimeError('Short action write')
                    print('Action sent; this does not prove firmware acceptance/queueing/execution.')
                    command = None
            rx.extend(port.read(min(port.in_waiting or 1, 256)))
            while rx:
                if rx[0] != 0xAA: del rx[0]; continue
                if len(rx) < 2: break
                length = rx[1]
                if not 1 <= length <= 128: del rx[0]; continue
                if len(rx) < length + 2: break
                payload = bytes(rx[2:length+2]); del rx[:length+2]
                message = pb.DeviceMessage()
                try: message.ParseFromString(payload); print(message)
                except Exception as exc: print(f'Invalid received protobuf: {exc}')
    print('Traffic stopped. Firmware should latch LINK after its configured timeout; no automatic recovery.')

if __name__ == '__main__': main()
