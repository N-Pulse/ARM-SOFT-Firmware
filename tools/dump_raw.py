"""Dump raw bytes from the STM32 UART for 5 seconds — bypass any frame parsing."""
import serial, sys, time

port = sys.argv[1] if len(sys.argv) > 1 else "COM3"
duration = float(sys.argv[2]) if len(sys.argv) > 2 else 5.0
ser = serial.Serial(port, 115200, timeout=0.1)
print(f"listening on {port} for {duration}s...", flush=True)

t_end = time.time() + duration
last_print = time.time()
buf = bytearray()
while time.time() < t_end:
    data = ser.read(64)
    if data:
        buf.extend(data)
        # print as hex with timestamp
        ts = time.strftime("%H:%M:%S") + f".{int((time.time()%1)*1000):03d}"
        print(f"[{ts}] {data.hex(' ')}", flush=True)

ser.close()
print(f"\ntotal bytes: {len(buf)}")
print(f"all hex: {buf.hex(' ')}")
