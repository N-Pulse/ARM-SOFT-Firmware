# Windows PC tool

`tools/motor_bench.py` controls the single STM32 bench through ST-LINK VCP at 115200 baud. It never arms, zeros, changes calibration or commands movement on startup. Opening a serial port may reset some boards; firmware always starts disabled. Use the ST-LINK USB connector, not a USB-to-L298N connection.

From the repository root, with Python 3.10 or newer:

```powershell
python -m venv .venv
.\.venv\Scripts\python.exe -m pip install -r tools/requirements-motor-bench.txt
.\.venv\Scripts\python.exe tools/motor_bench.py --list
.\.venv\Scripts\python.exe tools/motor_bench.py --port COM6 --plot --csv first-test.csv --save-plot first-test.png
```

Replace COM6 with the listed ST-LINK port. Close other serial terminals first. `--plot` is optional; the console alone can inspect raw encoder counts before calibration. The four panels show target/measured output position, position error, signed applied PWM and measured velocity. Missing calibrated measurements remain unavailable; the tool does not invent a zero position.

The reader sends `ping` every 100 ms and serializes writes. The firmware heartbeat timeout is 500 ms. On an I/O error the session ends and does not reconnect or rearm. Closing the plot, `quit`, EOF and Ctrl+C send priority `!`, stop and disarm before closing where the connection permits. A lost cable prevents transmitting shutdown, so firmware independently enforces its timeout. After an unexpected disconnect, verify the bridge is disabled physically before reconnecting. A desktop program or USB link is not a physical emergency stop.

Type `help` for firmware syntax and `pc_help` for client-only commands. Every command waits for a structured success/failure reply. Exact firmware units and parameters are in [BENCH_PROTOCOL.md](BENCH_PROTOCOL.md). The status packet includes firmware identification, current configuration, fault and validity flags.

## Passive verification

With motor power off and encoder electrical specifications verified:

```text
encoder
test encoder 10
zero
test zero
```

The passive test first requires disarmed status, then observes counts while the operator turns the shaft manually. Its reported count sign is an observation, not proof of electrical integrity or mechanical direction. Calibrate using verified PPR, x4 decoding, location and ratio; do not copy values from another motor. `test zero` sets a relative zero and reports the result; raw accumulated counts are not reset. No hard-stop homing is performed.

## Explicit movement tests

Complete wiring, encoder, limits, safety and PID commissioning in [BENCH_TEST_GUIDE.md](BENCH_TEST_GUIDE.md). Type `arm` yourself. The following are optional explicit console commands; none executes when the program starts:

```text
test positive 0.01
arm
test negative 0.01
arm
test step 0.01
arm
test repeat 0.01 4
```

Angles are radians and the test magnitude must be positive. Every test also asks the operator to type `RUN` before sending a target. These example angles have not been validated for the physical mechanism. The firmware rejects targets beyond configured position and relative step limits. Repeated tests alternate about the initial measured position and return to it; at most 20 alternating targets are accepted. Adjacent targets can be twice the entered magnitude, so configure a justified limit or select a smaller test. Firmware disarms when the target settles; an explicitly authorized repeated batch sends a new `arm` only after a reported completed target. It stops the batch on fault, rejected command, loss of link or any uncompleted disarming. There is no automatic recovery/restart.

Each test records actual telemetry if `--csv` was supplied. `test step` prints single-step metrics. `save response.png` saves the four-panel plot during a session. `analyze` analyzes a session containing exactly one target transition. `!` performs priority shutdown; `stop` and `disarm` also disable the motor. Any movement-test exit sends stop.

## CSV and offline analysis

CSV contains device milliseconds, host UTC/monotonic timestamps, target, measured position, error, measured velocity, PID request, applied signed PWM, raw/accumulated counts, loop interval, state, validity and fault. Unavailable numeric data is written as an empty field. Counts retain integer precision. Streaming runs at 20 Hz, independently of the 100 Hz controller; the serial link has finite bandwidth.

```powershell
python tools/motor_bench.py --analyze first-test.csv --save-plot first-test.png
python tools/motor_bench.py --analyze cropped-step.csv --target 0.01 --initial 0 --step-time 0 --tolerance 0.0002
```

Use a CSV containing one target transition, or crop to one response and specify target/initial position. `--step-time` is seconds from the first valid sample. Metrics include interpolated 10–90% rise time, overshoot relative to signed step amplitude, measured maximum absolute error, and mean error over the final 20% (up to 0.5 seconds). Settling uses a default band of 2% of step amplitude, with at least 0.2 seconds of remaining in-band samples. An explicit `--tolerance` sets an absolute radian band. Settling describes only the recorded interval; it is not proof of later stability. Missing crossings and unsettled records produce `null`, and insufficient/ambiguous data produces an unavailable result. Telemetry sampling limits timing accuracy. The tool does not generate fake response curves or claim hardware performance without measurements.

## Software verification

```powershell
python tests/bench/run_tests.py
python -m unittest discover -s tests/bench -p test_motor_bench.py -v
```

The complete runner requires native GCC or Clang in PATH. It builds isolated C encoder/controller/protocol tests and production hardware-register tests with mock HAL/registers, feeds actual C-emitted JSON through the Python parser, and runs Python tests. Python tests cover partial lines, overflow, malformed/nonfinite packets, missing calibration, CSV, heartbeat/disconnection behavior, explicit batch cancellation/rearming, PNG export and independently known positive/negative synthetic response metrics. Synthetic unit-test traces and native register models are software evidence only; they do not validate electrical signals or real motor behavior.
