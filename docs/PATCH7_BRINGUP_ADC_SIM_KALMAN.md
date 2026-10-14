# Patch 7 — single-motor commissioning, ADC, diagnostics, SIM and Kalman

Implemented and verified on Windows on 13 October 2026, on `closed-loop`.
No board was accessed, flashed or reset. No commit, push, checkout or clean was
performed. Pre-existing IDE/CMSIS/HAL working-tree edits were preserved.
This document describes software readiness separately from physical validation.

## Implemented behavior

Production HW still starts disarmed and has no reachable commissioning arm,
enable or encoder-reference-change path. An explicit test image retains all
eight initialized controllers but permits only one independent finger, INDEX
(ID 1) by default. Other controllers cannot enable or deliver nonzero drive;
the driver also enforces selection and PWM limits. Disconnected disabled
encoders do not gate the selected motor. Physical driver faults remain global.

Commissioning uses additive protobuf messages on UART4, preserving Hello v1
and the five existing action IDs. Production rejects every commissioning
operation with `PRODUCTION`. A separate framed STOP is accepted in all HW
builds without Hello, valid epoch, calibration, arm or enable. Legacy actions
are rejected in the test image to avoid a pose command changing its target.
There is no raw PWM command and no automatic homing or stage progression.

Arm requires an operator-established encoder calibration, fresh feedback,
healthy Hello session, no latched/pending safety fault, verified travel bounds,
maximum target step, reference rate and PWM ceiling. Enable additionally uses
the existing control-timing and backend interlocks. Enable starts at the
measured position with zero PWM limit. A new explicit target is required.
Targets must lie inside the supplied travel bounds and within the maximum
step of both measured and reference positions. Measured travel violations
while enabled latch FEEDBACK and inhibit all outputs.

STOP, driver faults, feedback loss, control delay, guard and link loss retain
the global inhibition/STOP epoch behavior. Recovery completes deferred cleanup,
requires safe inputs and never arms or enables. Requests contain the latest
diagnostic epoch and increasing request IDs; stale or queued work is rejected
after STOP/recovery. Reconnection only restores communication.

## Build and deployment boundary

From PowerShell in the repository root (ARM GCC, GNU Make and Python required):

```powershell
.\build.ps1 -Mode HW
.\build.ps1 -Mode SIM
mingw32-make -j8 SIM_MODE=0 HW_SINGLE_MOTOR_TEST=1 HW_TEST_MOTOR=1 CURRENT_ACQUISITION=0
mingw32-make -j8 SIM_MODE=0 HW_SINGLE_MOTOR_TEST=1 HW_TEST_MOTOR=1 CURRENT_ACQUISITION=1
```

| Configuration | Artifact |
| --- | --- |
| Standard HW | `build/HW/n-pulse.elf`; production alias `Debug/N-PULSE FIRMWARE.elf` |
| Standard SIM | `build/SIM/n-pulse.elf`; alias `Debug/SIM/N-PULSE FIRMWARE.elf` |
| INDEX test, ADC off | `build/HW-single-1/n-pulse.elf` |
| INDEX test, ADC on | `build/HW-single-1-current/n-pulse.elf` |
| Optional acquisition without commissioning | `build/HW-current/n-pulse.elf` |
| Optional SIM acquisition configuration | `build/SIM-current/n-pulse.elf` (no physical acquisition path in SIM) |

`HW_SINGLE_MOTOR_TEST=1` defines `N_PULSE_HW_SINGLE_MOTOR_TEST=1`.
`HW_TEST_MOTOR` supports IDs 0..4 only. SIM plus commissioning is rejected.
`HW_TEST_PWM_CAP` defaults to 5 percent and is restricted to 1..10. The maximum
software step defaults to 0.1 rad in `hw_test_config.h`. These are containment
caps, **not validated safe motor power or travel values**. Runtime limits start
unavailable; the operator must supply lower verified limits before arm.
Reference rate and travel are additionally confined within the existing HW
finger software envelope. Changing compile flags forces a rebuild in that
configuration. Test/current images are never published to production aliases.

`flash.ps1` continues to require one explicit ST-LINK serial number and the
canonical standard HW path, manifest, flags and matching hashes. It additionally
requires `N_PULSE_HW_SINGLE_MOTOR_TEST=0`. It rejects test paths and commissioning
flags even at the canonical path. No commissioning flash shortcut was added.

Before a future commissioning deployment, a responsible operator must review
the source, selected finger, build receipt and ELF hash; verify PCB revision,
SLEEP/FAULT wiring and physical cutoff; isolate motor power; identify exactly
one probe; and explicitly approve a separate supervised commissioning deployment
procedure. Do not copy a test ELF into the canonical HW location or weaken
`flash.ps1`. Deployment is outside this patch execution.

## UART4 cabling and terminal

The `.ioc` and active HAL initialization specify UART4 TX **PC10**, RX **PC11**,
115200 baud, 8 data bits, no parity, one stop bit. Connect confirmed board PC10
to adapter RX, PC11 to adapter TX, and a common signal ground. Use a compatible
logic-level USB–UART adapter and verify its voltage electrically. Do not connect
RS-232 voltage levels. Do not assume ST-LINK VCP is routed to UART4. Confirm the
connector/pad routing on the assembled PCB; USART2/USART3 retain existing roles.
Only one PC process may open this port.

```powershell
python -m pip install "protobuf>=7.36.2" pyserial
python tools/hw_monitor.py --port COM_CONFIRMED --motor 1 --duration 10
python tools/hw_monitor.py --port COM_CONFIRMED --motor 1
```

The first invocation is timed diagnostics; the second accepts explicit console
commands. Neither automatically calibrates, recovers, arms, enables or targets.
The sole I/O owner sends Hello every 0.5 s and whole frames. The client maintains
a bounded command queue and acknowledgement timeout; STOP has a separate urgent
flag. It attempts STOP on controlled exit and after command/diagnostic timeout.
Serial failure or process/PC failure can prevent delivery; hardware power cutoff
and independent current limiting remain essential.

Console syntax:

```text
status
raw
calibrate CPR QUADRATURE GEAR_RATIO DIRECTION ZERO_OFFSET_COUNTS
zero
limits MIN_RAD MAX_RAD MAX_STEP_RAD PWM_PERCENT REFERENCE_RATE_RAD_S
recover
arm
enable
target POSITION_RAD PWM_PERCENT
disable
stop
estimator-reset verified
estimator-stop
quit
```

Replace symbolic physical parameters with measured/approved values; the above
is syntax, not a runnable calibrated motion recipe. `zero` is a logical encoder
origin reset while disabled/disarmed, never a homing move. Calibration/reference
changes invalidate feedback and require a later fresh control sample. `disable`
zeros the selected output but retains driver arm; `stop` globally disarms and
latches MANUAL. Use STOP before changing calibration/limits.

Replies identify `OK`, `PRODUCTION`, `SESSION`, `STALE`, `WRONG_MOTOR`,
`PARAMETERS`, `DISABLE_FIRST`, `CALIBRATION`, `FEEDBACK`, `FAULT`, `ARM_FIRST`,
`ENABLE_FIRST` or `REJECTED`. A generic refusal requires inspecting diagnostic
faults and freshness; HAL startup/cleanup failures cannot be treated as success.
Only Hello maintains the commissioning link; diagnostics and rejected commands
do not bypass session supervision. Request IDs are monotonic within a firmware
boot; after ID wrap or a clock rollback, establish a fresh supervised boot rather
than replaying commands. No motor action is automatically retried.

## Mandatory INDEX acceptance gates

Every gate requires a recorded operator decision; the software never advances
stages or enables a motor on its own.

1. **STM32 powered, motor supply OFF.** Review the explicit test build and ID 1.
   Verify boot, all eight controllers disabled, armed=false, zero PWM, no fatal
   halt, stable Hello and readable diagnostics. ADC-off must report INVALID and
   unavailable current. ADC-on may report completed RAW_VALID readings without
   claiming amperes. Any unresolved driver/global fault blocks progression.
2. **Manual encoder direction/counts, supply OFF.** Use `raw`, rotate only the
   selected mechanism manually within mechanically verified free travel, record
   count changes, quadrature behavior and direction. Verify the other seven
   outputs remain inactive. Establish CPR, decoding factor, transmission ratio
   and sign from evidence; none is inferred by firmware.
3. **Angular calibration, supply OFF.** At a trustworthy stationary reference,
   disabled/disarmed, issue `calibrate ...` and, when appropriate, `zero`.
   Compare angular readings against independently measured displacements in
   both directions and check repeatability. Encoder radians represent the
   explicitly configured coordinate, not an inferred anatomical joint angle.
   Establish travel, maximum step, rate and power limits; enter `limits ...`.
   Require finite fresh feedback and no unintended position jumps.
4. **Scope/logic analyzer, supply OFF.** After all prerequisites, explicitly
   `recover` if required, `arm`, then `enable`. Verify all seven other motor
   pairs inactive. Issue only small approved position targets through PID.
   Manually place the encoder at known interior positions as needed to inspect
   both PID signs; never send raw duty commands. Confirm PWM frequency/duty,
   pair inactive levels and direction transitions against the DRV8848/PCB
   requirements. Targets/limits are software restrictions, not measured voltage
   or mechanical safety guarantees. A stationary encoder with PWM will trip
   the existing stall guard; do not defeat it to extend this test.
5. **STOP/fault/disarm, supply OFF.** Test `disable`, `stop`, controlled client
   exit, Hello timeout/disconnection and a supervised electrically valid FAULT
   injection. Scope all eight output pairs and SLEEP lines for inhibition.
   Restore the physical cause; verify reconnection alone cannot resume motion.
   Recover explicitly and confirm still disarmed/disabled; old targets must not
   return. New arm, enable and target are separate operator decisions. Do not
   short arbitrary MCU/driver pins to create a fault.
6. **Controlled powered test only after gates 1–5 pass.** Secure a freely moving
   mechanism inside independently verified travel, use an external current-limited
   supply with an accessible physical cutoff, and have an operator at that cutoff.
   Start with independently approved low voltage/current and the lowest useful
   operator PWM limit below the software cap. Explicitly repeat calibration/
   limits/session checks, recover, arm, enable, then one small target. Observe
   direction, current and travel, STOP immediately on unexpected behavior, and
   record results before another target. No autonomous sequences, stop-seeking
   homing or automatic recovery are authorized by this procedure.

## ADC acquisition, validity and calibration

The pin/rank mapping is unchanged:

| Motor ID/name | ADC rank | Current input |
| --- | --- | --- |
| 0 THUMB | ADC3 rank 1 | **PE13 / ADC3_IN3 (Curr5)** |
| 1 INDEX | ADC1 rank 1 | PA3 / OPAMP1 (Curr1) |
| 2 MIDDLE | ADC2 rank 1 | PA7 / OPAMP2 (Curr2) |
| 3 RING | ADC2 rank 2 | PB0 / OPAMP3 (Curr3) |
| 4 LITTLE | ADC5 rank 1 | PD11 / OPAMP4 (Curr4) |
| 5 WRIST_X | ADC5 rank 2 | PC3 / OPAMP5 (Curr7) |
| 6 WRIST_Y | ADC4 rank 3 | PD9 / OPAMP6 (Curr8) |
| 7 PALM | ADC4 rank 1 | PE14 (Curr6) |

ADC4 rank 2 is **PE15 / ADC4_IN2 / Batt_lvl**, included in the scan and excluded
from motor mappings. Resolution is 12 bits, right aligned; ranks, channels,
sampling time and OPAMP routing/PGA/factory trim are retained from the current
configuration. Main initializes all five ADCs and six OPAMPs; acquisition remains
off unless explicitly selected at build time.

The opt-in BSP overrides the experimental continuous/circular acquisition
scheduling with finite software-triggered scans and normal DMA. This fixes the
demonstrated inability to prove freshness from a continuously overwritten tiny
ring without excessive completion interrupts. It calibrates all five ADCs
single-ended before acquisition, starts all six OPAMPs, then requests one full
scan every 20 ms from the low-priority diagnostic task. Half-transfer IRQs are
disabled; at most five completion IRQs per period are expected (250/s). Actual
interrupt rate, sampling settling and timing require scope validation. No HAL
start/stop/calibration runs inside Motor_Lock or the PID task.

Completion copies the finished rank buffer and records HAL milliseconds and a
sequence number. Each ADC scan is coherent; the five ADCs are independently
triggered and are not claimed to be simultaneous. An outstanding scan at the
next request, HAL failure, DMA error callback or ADC overrun stops acquisition
with a diagnostic error. No automatic restart masks the fault. Explicit stop
and initialization failure clean up all five ADC DMA channels and six OPAMPs.

`ADC_DMA_BSP_GetSample()` returns raw count, voltage/current (NaN when
unavailable), active/raw-valid flags, timestamp, age, completion sequence, state
and errors. States: INVALID=0, RAW_VALID=1, CURRENT_CALIBRATED=2, STALE=3,
ERROR=4, SATURATED=5. Data expires after 100 ms without another completion.
Rails 0 and 4095 are saturation diagnostics and never valid zero amperes.
Acquisition-off and never-completed buffers are INVALID. A valid calibrated zero
requires a completed non-rail sample matching a verified offset. The legacy
`GetRaw()` compatibility wrapper returns zero when unavailable; new consumers
must use the typed interface. `GetCurrentA()` returns NaN unless calibrated and
fresh. ADC errors are diagnostic and are not silently promoted into an
uncalibrated protection threshold.

Conversion is:

```text
Vadc = raw * measured_Vref / 4095
I = polarity * (Vadc - offset_V) * scale / (shunt_ohm * total_analog_gain)
```

Coefficient storage is explicit per motor in `adc_current_config.h`; the
defaults are intentionally invalid zero entries and Vref unknown. The typed
setters also permit application-driven RAM calibration while disabled, reject
nonfinite/nonpositive/invalid coefficients, and leave previous valid coefficients
unchanged after rejection. Vref changes require all motors disabled. There is no
flash persistence and no production UART calibration command.

Calibration procedure:

1. Confirm the populated board's shunt/current-sense transfer, ADC route and
   total analog gain, including any external stage. The configured OPAMP PGA
   macro is not sufficient evidence of the entire assembled analog gain.
2. Measure ADC reference/VDDA with suitable equipment. With motor power off and
   acquisition active, record completed offsets, noise, rails and drift.
3. Under independent current limiting, use a calibrated current reference at
   multiple known points and both relevant drive directions. Establish offset,
   scale, polarity, linear range, RC/filter delay and PWM/decay dependence.
   Determine whether the channel represents signed winding current, magnitude,
   averaged driver sense or another quantity; estimator torque input requires
   a validated signed-current relationship.
4. Populate only measured coefficients/Vref, rebuild the isolated acquisition
   image, recheck its hash/flags under a separately approved deployment, then
   compare reported amperes against the reference across the operating range.
   Include zero, saturation, missing scan, stopped ADC and DMA fault checks.
5. Record uncertainty and applicability. Do not derive contact force, electrical
   overcurrent thresholds, torque limits or a physical safety claim from these
   software tests. Independent current limiting remains mandatory initially.

Required missing measurements: actual Vref, populated shunts/sense transfer,
total gain for every current channel, offsets, scale/polarity, bandwidth/settling,
PWM-mode dependence and a reference-current comparison on the assembled PCB.

## Diagnostics and bounded transport

Telemetry is selected per motor (INDEX initially), at 5 Hz by default;
`N_PULSE_TELEMETRY_PERIOD_MS` is configurable with a minimum of 100 ms.
The task has priority idle+1, below PID (idle+3), communications (idle+2) and
SafetyTask (idle+5). It services ADC/shadow estimation every 10 ms and snapshots
the selected motor, encoder, driver and fault epoch under short locks. ADC and
estimator states carry their own validity; they are not asserted simultaneous
with the motor snapshot. Serialization and queue offers occur outside locks.

`MotorDiagnostic` contains timestamp, selected ID, epoch, enabled/armed and
calibration/freshness flags, faults, target/reference, raw counts, encoder
position/velocity, applied controller PWM, ADC raw/voltage/current/state/errors/
age, and Kalman position/velocity/status. Fault bits retain MANUAL, GUARD,
FEEDBACK, CONTROL_DELAY, FORCE, DRIVER, PID and LINK. Diagnostic values describe
MCU state, not measured electrical PWM or confirmed mechanical actuation.

The worst schema payload is 123 bytes, within the existing 128-byte protobuf
limit (125 including the HW envelope), about 625 bytes/s at 5 Hz. Replies and
diagnostics use additive ProsthesisMessage fields; Hello/action encoding is
unchanged. The queue offer has zero RTOS wait; saturation drops telemetry and
does not hold up PID or incoming STOP. UART4Transport remains the only writer.
The monitor uses flags/state as well as NaN, so omitted/unavailable protobuf
values are not displayed as measured zero.

Bindings are regenerated together by `python tools/generate_protocol.py` using
grpcio-tools and the bundled EmbeddedProto generator (also needs jinja2, six and
toposort). Python bindings currently require protobuf >=7.36.2. No generator
modifications were made to third-party sources.

## SIM serial and native-PC paths

The original path remains: STM32 SIM firmware → UART4 → ROS 2 bridge → Gazebo
effort controller. The new optional path substitutes a native C process and
binary stdin/stdout IPC for the physical UART. Both use the same SIM v2 CRC,
session, sequence, leases, command messages, eight-axis feedback and Peer/effort
adapter. Neither sends position targets to a Gazebo-owned PID. PID remains the
production C controller using its existing SIM configuration.

```powershell
python tools/run_simulation.py
python tools/build_native_sim.py
```

The first is the deterministic virtual-time rotor regression bench. The second
builds `build/simulation/firmware-realtime.exe` on Windows (no suffix on Linux).
The realtime executable has an explicit `--realtime` mode, its own monotonic
clock and C control/safety scheduling. It has no fictitious rotor model. The
default native harness mode remains virtual-time and is never selected by the
ROS bridge. The realtime and bench executables have separate artifact names.

Bridge syntax (replace every external placeholder with validated model data):

```text
python tools/ros2_bridge/serial_bridge.py --transport serial --port COM_CONFIRMED --joint-names <eight actuator names in ID order> --effort-per-percent <eight positive scales> --effort-topic <controller topic> --applied-effort-topic <stamped actual effort topic> --model-watchdog-ms <verified value <=100>
python tools/ros2_bridge/serial_bridge.py --transport native --native-binary build/simulation/firmware-realtime.exe --joint-names <eight actuator names in ID order> --effort-per-percent <eight positive scales> --effort-topic <controller topic> --applied-effort-topic <stamped actual effort topic> --model-watchdog-ms <verified value <=100> --use-sim-time
```

ROS 2 and its message packages must be installed/sourced externally. Use
`--use-sim-time` when model stamps use `/clock`. Source stamps must be positive,
strictly increasing per source and no older than 50 ms. This bridge requires
ROS model time to progress at wall-clock rate (1x); pauses, jumps, duplicate
stamps or accumulated drift above 50 ms cause STOP/refusal and require
resynchronization/restart. Firmware/session watchdogs use monotonic wall time,
not virtual harness time or a paused Gazebo clock.

Serial keeps its existing baud pacing and cached firmware timestamp behavior.
Native IPC has no simulated UART delay and extrapolates its Hello clock anchor
at the same PC monotonic rate with a 20 ms scheduling margin, retaining the
unchanged firmware 80 ms lease checks. Native RX drains bounded chunks between
parser calls. C stdout writes run in a separate thread with seven usable frame
slots, so pipe backpressure cannot block C safety scheduling. Python pipe reads
have an eight-chunk queue; writes have a 25 ms completion deadline. Overflow,
write timeout and child failure inhibit host permissions; shutdown is bounded.
Desktop OS scheduling stalls can still trigger watchdogs; no hard real-time
guarantee is claimed.

Connection sends Hello only. Operator commands are published as String data
to `/n_pulse/sim_control`, one at a time with acknowledgement/status inspection:

```text
config
recover
arm
enable 255
target 0 0.02 3
target 1 0.02 3
target 2 0.02 3
target 3 0.02 3
target 4 0.02 3
target 5 0.02 3
target 6 0.02 3
target 7 0.02 3
intent OPEN_HAND
intent CLOSE_HAND
intent PINCH
intent ROTATE_WRIST_R
intent ROTATE_WRIST_L
stop
```

These numeric SIM targets are synthetic examples, not HW commissioning limits.
Fresh position and actual applied-effort feedback for all eight coordinates
are required before arm, then again for the new run before enable. Recovery
and run changes may discard feedback already in flight; wait for new samples,
and inspect rejection instead of automatically retrying arm/enable. Intents
require all eight controllers enabled. OPEN/CLOSE/PINCH use the authoritative
pose table; wrist rotations continue through `drive_wrist()` with
`WRIST_X=rotation+flexion`, `WRIST_Y=rotation-flexion`. POINT/NEUTRAL are not public
actions. New SIM type 10 transports the existing five intentions without
changing HW action IDs or SIM v2 framing. Older SIM firmware may reject it.

### External Gazebo contract and blockers

| ID | Firmware coordinate | External name | Position / velocity | Command effort |
| --- | --- | --- | --- | --- |
| 0 | THUMB | required `--joint-names` entry 0 | calibrated motor coordinate rad / rad/s | percent × entry 0 scale |
| 1 | INDEX | required entry 1 | rad / rad/s | percent × entry 1 scale |
| 2 | MIDDLE | required entry 2 | rad / rad/s | percent × entry 2 scale |
| 3 | RING | required entry 3 | rad / rad/s | percent × entry 3 scale |
| 4 | LITTLE | required entry 4 | rad / rad/s | percent × entry 4 scale |
| 5 | WRIST_X | required entry 5 | rad / rad/s | percent × entry 5 scale |
| 6 | WRIST_Y | required entry 6 | rad / rad/s | percent × entry 6 scale |
| 7 | PALM | required entry 7 | rad / rad/s | percent × entry 7 scale |

Effort scales are explicit positive external effort units per firmware percent,
normally N·m/percent for an angular effort actuator only if the external model
confirms that convention. Applied effort must be stamped actual controller/model
effort in the same coordinate/units, converted back by dividing by the scale;
echoing desired command is insufficient. Feedback validity bits accompany
position, velocity, force (optional) and applied percent; absent data is not zero.
Eight unique names must identify actuator coordinates consistent with firmware
targets. Any motor/joint transmission, sign or differential Jacobian belongs in
a validated external mapping; direct identity is acceptable only when the model
already exposes these motor coordinates. No anatomical joint angles, torque
constants, reduction ratios or robot-specific names are invented here.

The external team must supply the robot/actuator model, exact names and topics,
coordinate/transmission convention, effort scales and signs, stamped applied
effort feedback, ROS launch/controller configuration, and an **independent
controller/model watchdog <=100 ms** that zeros effort on lost commands even
if this bridge dies. `--model-watchdog-ms` is only a declaration; it does not
implement or verify that watchdog. The Python rotor is not a Gazebo integration.
ROS 2/Gazebo end-to-end execution, effort units, model clock behavior and the
controller-side cutoff remain untested external requirements.

## Kalman estimator

`motor_kalman.c` is a reusable allocation-free C module without motor actuation,
HAL or PID dependencies. State is motor angular position and velocity:

```text
x = [theta_motor, omega_motor]^T
theta_dot = omega
omega_dot = (Kt/J)*current - (b/J)*omega + unknown load/model disturbance
P_predict = F * P * F^T + Q * dt
innovation = encoder_motor_position - theta_predict
S = P00_predict + R
K = [P00_predict, P01_predict]^T / S
x_correct = x_predict + K * innovation
```

For valid current it uses exact constant-input discretization of the linear
viscous model; for zero damping, `F=[[1,dt],[0,1]]` and
`B=[0.5*(Kt/J)*dt^2,(Kt/J)*dt]^T`. The covariance correction uses Joseph form.
Q is a symmetric positive semidefinite per-second matrix, R is encoder position
variance, and initial covariance is explicit. Unknown load torque is process
uncertainty. With missing, stale or nonfinite current the estimator uses a
degraded constant-velocity model and inflated Q, rather than claiming zero amps.

J, b, Kt, Q, R, initial covariance, absolute innovation limit, consecutive limit,
current/encoder age limits, maximum dt and **encoder-to-motor ratio** are
configured per motor. `motor_kalman_config.h` intentionally contains invalid
zero HW entries; synthetic defaults exist only in native tests. Encoder values
are multiplied by the explicit ratio before shadow estimation; the estimator
does not silently interpret a geared encoder coordinate as motor or joint angle.

`MotorEstimation_Configure()` refuses enabled/armed motors. Configuration does
not start estimation. `estimator-reset verified` is available only in the test
image and requires a configured estimator, disabled/disarmed motor, fresh
encoder reference, and an explicit operator declaration that the reference is
trustworthy and the mechanism stationary. It initializes position/zero velocity
and covariance. There is no automated homing, stationary verification or reset
while moving. `estimator-stop` stops diagnostics only and cannot actuate a motor.

An excessive innovation is rejected, increments diagnostic counters and marks
the estimate inconsistent/unavailable. The configurable consecutive criterion
latches INCONSISTENT; only controlled reset can clear that latch. No automatic
re-zeroing is implemented. Invalid observations, nonfinite states, backward or
excessive timestamp steps and invalid covariance have explicit status; duplicate
time causes no step. Correction requires a fresh observation at the prediction
timestamp, preventing relabeling old data as new. Snapshots expose covariance,
counters, innovation and validity; telemetry exposes estimate/status.

The lower-priority shadow service uses per-motor snapshots and revisions to
avoid committing over an explicit reset. PID continues to read its original
encoder feedback. Raw sensor diagnostics and existing safety limits remain the
authority. Identified physical parameters and a validated signed current path
are still required for hardware model-based estimates.

## Commands executed and results

Local generator/client dependencies were installed into ignored
`build/patch7-pydeps` (protobuf 7.36.2, grpcio-tools 1.84.0, jinja2 3.1.6,
pyserial 3.5, six 1.17.0 and toposort 1.10). For the Python codec tests:

```powershell
$env:PYTHONPATH = (Join-Path (Get-Location) 'build/patch7-pydeps')
```

The sandbox initially blocked package downloads; approved escalation installed
only these local build dependencies. The installed GNU Make 3.82.90 cannot
enumerate this sandboxed workspace (`INTERNAL: readdir: Invalid argument`), so
actual Make builds and alternation checks ran successfully outside that sandbox.
No programmer or real serial port was invoked. Flash tests used only the fake
programmer executable in the isolated fixture.

| Executed command | Result |
| --- | --- |
| `python tools/generate_protocol.py` | PASS: EmbeddedProto and both Python bindings regenerated from one schema |
| `python tests/startup/run_tests.py` | PASS: 31 host invocations, including all five ADC start failures, six OPAMP failures, five calibration and five ADC-init failures |
| `python tests/safety/run_tests.py` | PASS: 24 asynchronous + 24 real-HW-backend scenarios, disarm/cleanup cases and real encoder BSP integration |
| `python tests/comms/run_tests.py` | PASS: 24 comms scenarios, SIM bounds/parser tests, 3 targeted SIM scenarios and Python HW codec |
| `python tests/outputs/run_tests.py` | PASS: fatal/assert/register/DRV8848 tests and 20 fake-programmer script scenarios |
| `python tools/run_simulation.py` | PASS: 27 scenarios including all five authoritative intentions, plus parser/host tests |
| `python tests/patch7/run_tests.py` | PASS: commissioning path and 11 grouped Kalman scenarios, including 100000 covariance updates and eight instances |
| `python tests/patch7/test_monitor.py` | PASS: ten commands, codec bounds, signed counts, unavailable values, fragmented/noisy/expired packets |
| `python tests/patch7/test_native.py` | PASS: real monotonic C IPC, all eight axes, explicit target/intent, STOP/recovery, child session watchdog and ROS clock policy |
| `python tools/verify_startup_build.py` | PASS: HW/SIM ELF vectors, ticks, backend and default disarm/ADC paths |
| `python tests/safety/verify_build.py` | PASS: HW/SIM linked interlocks and physical separation |
| `python tests/comms/verify_build.py` | PASS: HW/SIM UART4 owner/vector/RTOS/epoch checks |
| `python tests/outputs/verify_build.py` | PASS: HW/SIM fatal vectors/register cutoff and output writers |
| `python tests/patch7/verify_build.py` | PASS: four build modes, commissioning/current symbol reachability, canonical HW hash, Curr5/UART4 mappings |
| `python tests/startup/test_build_modes.py` with `MAKE=mingw32-make` | PASS: 148 HW/SIM objects unchanged during HW → SIM → Debug/HW alternation |

Actual ARM Make configurations were built successfully with `-j8`:

| Flags | text / data / bss bytes |
| --- | --- |
| `SIM_MODE=0 HW_SINGLE_MOTOR_TEST=0 CURRENT_ACQUISITION=0` | 123560 / 388 / 33384 |
| `SIM_MODE=1 HW_SINGLE_MOTOR_TEST=0 CURRENT_ACQUISITION=0` | 91624 / 96 / 33816 |
| `SIM_MODE=0 HW_SINGLE_MOTOR_TEST=1 CURRENT_ACQUISITION=0` | 130560 / 388 / 33408 |
| `SIM_MODE=0 HW_SINGLE_MOTOR_TEST=1 CURRENT_ACQUISITION=1` | 134936 / 388 / 33440 |

Sanitizer variants are **NOT RUN**: installed host compilers/runtime probes
could not compile and execute ASan/UBSan. Ordinary C/Python tests passed; no
sanitizer pass is claimed. ROS 2/Gazebo, physical UART, scope, encoder metrology,
analog calibration, powered motors and electrical protection are **NOT TESTED**.
Early development tests exposed client request-erasure, native timestamp/burst
handling and fixture timing issues; these were fixed before the final passing
runs. The structured exit-code record is `build/patch7-final-results.json`. Logs are in
`build/patch7-*.log`; simulation artifacts are in
`build/simulation/`. Compilation verifies software, not stack high-water marks,
electrical levels or measured STOP latency. Those remain board acceptance work.

## Exact changed files

Modified (40):

- `CHANGEMENTS_FIRMWARE.md`
- `Core/Src/main.c`
- `build.ps1`
- `flash.ps1`
- `fw/app/app.c`
- `fw/bsp/adc_dma/adc_dma_bsp.c`
- `fw/bsp/adc_dma/adc_dma_bsp.h`
- `fw/comm-stack/PyUART/protocol_messages_pb2.py`
- `fw/comm-stack/comm_ws/src/bmi_gateway/bmi_gateway/protocol_messages_pb2.py`
- `fw/comm-stack/protocol_messages.h`
- `fw/comm-stack/protocol_messages.proto`
- `fw/comm-stack/rx.cpp`
- `fw/comm-stack/rx.h`
- `fw/comm-stack/tx.cpp`
- `fw/comm-stack/tx.h`
- `fw/comm/comm.c`
- `fw/comm/comms_session.c`
- `fw/comm/comms_session.h`
- `fw/comm/sim_protocol.h`
- `fw/comm/sim_session.c`
- `fw/motors/motor_control/drv8848.c`
- `fw/motors/motor_control/motor_control.c`
- `tests/comms/run_tests.py`
- `tests/comms/test_comms.c`
- `tests/outputs/test_flash.py`
- `tests/safety/mocks/adc_dma_bsp.h`
- `tests/safety/test_safety.c`
- `tests/simulation/bench.py`
- `tests/simulation/native_firmware.c`
- `tests/simulation/scenarios.py`
- `tests/startup/mocks/stm32g4xx_hal.h`
- `tests/startup/run_tests.py`
- `tests/startup/test_adc.c`
- `tools/build_artifacts.py`
- `tools/firmware.mk`
- `tools/ros2_bridge/serial_bridge.py`
- `tools/sim/client.py`
- `tools/sim/peer.py`
- `tools/sim/protocol.py`
- `tools/sim/ros_adapter.py`

Added (22):

- `docs/PATCH7_BRINGUP_ADC_SIM_KALMAN.md`
- `fw/app/hw_commission.c`
- `fw/app/hw_commission.h`
- `fw/app/hw_telemetry.c`
- `fw/app/hw_telemetry.h`
- `fw/bsp/adc_dma/adc_current_config.h`
- `fw/motors/estimation/motor_estimation.c`
- `fw/motors/estimation/motor_estimation.h`
- `fw/motors/estimation/motor_kalman.c`
- `fw/motors/estimation/motor_kalman.h`
- `fw/motors/estimation/motor_kalman_config.h`
- `fw/motors/motor_control/hw_test_config.h`
- `tests/patch7/run_tests.py`
- `tests/patch7/test_kalman.c`
- `tests/patch7/test_monitor.py`
- `tests/patch7/test_native.py`
- `tests/patch7/verify_build.py`
- `tests/safety/test_commission.c`
- `tools/build_native_sim.py`
- `tools/generate_protocol.py`
- `tools/hw_monitor.py`
- `tools/sim/native_transport.py`

## Readiness matrix

| Item | Status | Explanation |
| --- | --- | --- |
| HW build ready? | PASS | Standard ARM image built; startup disarmed, commissioning unreachable |
| HW test image ready? | PASS | Separate INDEX images with ADC off/on; production flash path rejects them |
| UART terminal ready? | PASS | Offline codec/client tests pass; physical UART cabling/session NOT TESTED |
| Encoder validated in software? | PASS | Native BSP, numeric and controller integration tests; physical scale/direction NOT TESTED |
| ADC raw acquisition ready? | PASS | Finite completed-scan path and failure/validity tests; assembled analog behavior NOT TESTED |
| Current calibration ready? | BLOCKED | API/coefficient validation ready; measured electrical coefficients/reference comparisons missing |
| PID single-motor path ready? | PASS | Explicit calibration/limits/arm/enable/target path tested with real controller/driver and mocked HAL |
| Single-motor physical test performed? | NOT TESTED | No hardware accessed; six operator acceptance gates remain |
| SIM native ready? | PASS | Actual C PID via bounded binary IPC and independent monotonic firmware watchdog tested |
| SIM serial path preserved? | PASS | Existing SIM v2 framing, eight axes, serial pacing and regressions retained; physical UART NOT TESTED |
| Gazebo end-to-end tested? | NOT TESTED | ROS/model assets, validated actuator mapping/scales and controller watchdog required externally |
| Kalman module ready? | PASS | Synthetic C tests pass; hardware model parameters intentionally unavailable |
| Kalman used by PID? | PASS | No: verified intentional shadow-only integration; existing encoder PID feedback remains authoritative |
