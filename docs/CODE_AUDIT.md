# Firmware consistency audit — 2026-11-11

Scope: maintained first-party firmware (`fw`, production portions of `Core`),
protocol bindings, host clients, build/test entry points and current feature
notes. Third-party HAL/FreeRTOS/EmbeddedProto internals were exercised through
tests and compilation, not rewritten or exhaustively audited line by line.
All changes remain in the existing working tree; no user changes were discarded.

## Confirmed problems fixed

1. **CRC bypass through the old receive envelope.** Normal HW accepted legacy
   `AA + protobuf` intents alongside the new CRC-protected DATA protocol.
   Production now requires the new DATA envelope; the protobuf class vector
   itself is unchanged. The isolated commissioning bench retains its old wire
   interface. Removed remote control/STOP commands remain ignored.
2. **DATA received before ACK could execute later.** Queued pre-handshake RX
   shared the generation of the eventual WORK session. The generation now
   advances at reply TX completion as well as on timeout, invalidating early DATA.
3. **Grip startup failure was counted as success.** IntentRouter now returns
   success/failure; the session records a new current intent only after a
   successful dispatch, so an identical retry can start a previously rejected grip.
4. **Grip could retain previous high PWM during the transition.** Entering a new
   grip now immediately zeros all prior enabled outputs before the next tick
   applies the lower grip duty. It still stops nonparticipants and preserves
   per-finger contact bits for repeated identical grips.
5. **Contact seek bypassed fixed-target travel clamping.** The open-loop closing
   branch now checks the configured upper position limit and latches a separate
   POSITION safety cause on overtravel. Force, feedback, driver and timing faults
   remain unconditional all-motor stops. Cause-array sizing now has one definition.
6. **Concurrent force-reader ownership.** Both haptics and telemetry tasks called
   the same board reader. TelemetryTask is now the sole task owner. Pre-scheduler
   boot homing separately polls the hook before tasks exist, closing the boot-time
   force-supervision gap. Stale voltage input is explicitly invalidated.
7. **Contradictory outbound status meaning.** Haptics Status byte 0 was a nonzero
   validity bitmap even when healthy. It now uses the same shared controller
   state as the LEDs: 0 healthy; bits 0/1/2 mean safety fault, low battery and
   unavailable battery. Optional following bytes carry validity bits; NaN still
   marks missing telemetry. LED and telemetry share the same status function.
8. **Haptics suffix resynchronization used an obsolete timestamp.** Noise removal
   did not advance the candidate frame deadline. Per-byte timestamps now track
   the surviving candidate, with a fragmented-frame regression test.
9. **SIM still rejected new actions.** SIM validation/client names still allowed
   only five actions. All 19 actions now use the same public mapping; SIM includes
   a contact-stop/normal-guard-restoration scenario. Python action names/counts
   are centralized and checked against the schema and both descriptor bindings.
10. **Host CLI still used the retired lifecycle.** The maintained clients now
    delegate to `tools/decoding_link.py`: SYNC/HELLO/ACK-or-NACK and repeated raw
    DATA, without remote approval/stop/restart/PONG commands. A codec mismatch
    (such as production's still-unconfigured data_len=0) fails explicitly.
11. **Old tests no longer linked or asserted current behavior.** Safety/output
    benches now link the actual router and an explicit post-startup homing mock;
    commissioning tests reject remote recovery and exercise local recovery instead.
    Current communication and commissioning entry points select maintained tests.
    Float conversion in the legacy force fixture was corrected to avoid a false
    hard-limit boundary failure. Missing PowerShell is reported as a skip.
12. **Invalid editable force thresholds could silently disable protection.**
    NaN, nonfinite or unordered voltage thresholds now prevent arming, with an
    initialization-failure regression test.

Additional cleanup removed redundant activity updates and the duplicate
handshake-to-WORK transition, simplified test mode selection, corrected obsolete
README/main.c explanations, and replaced the Chinese hardware-output comment
with English. No Chinese comments remain in the scanned first-party C/C++,
Python and protobuf sources under `fw`, `Core`, `tools` and `tests`.

## Compatibility and intentionally retained code

Protobuf field definitions and helper encoders for retired control messages remain
for wire-schema compatibility and rejection tests; they are not an execution path.
Historical FIFO/PING C fixtures and state-machine documents are reference snapshots,
not the current test entry points. Disabled legacy calibration code in main.c
remains guarded by a compile-time error and is explicitly distinguished from the
production encoder mapping. Default hardware hooks remain disconnected as agreed.

## Validation

- Ten native modes with ASan/UBSan: streaming, handshake, grasp/pinch, haptics,
  LEDs, battery configured/defaults, commissioning app, real boot-homing force
  abort, and invalid force configuration.
- Safety/interlock/encoder integration, startup/ADC, fatal/PWM output and Patch 7
  commissioning/Kalman suites.
- Complete offline SIM scenario matrix, including new grip contact behavior,
  parser bounds/noise, force/timing faults, reconnect, packet loss and sequence wrap.
- Host raw-link/protobuf helper tests and structural consistency checks of both
  Python protobuf descriptors. No installed Python protobuf runtime was available
  for a separate import/runtime test; actual firmware protobuf RX/TX is compiled
  and exercised by the native C++ tests.
- HW, SIM and single-motor C/C++ syntax checks; all 69 first-party Python files parsed successfully; Clang static analysis of 13 core
  first-party production units reported no diagnostics; `git diff --check` clean.

PowerShell flash-script tests were skipped because powershell.exe is unavailable.
The optional localhost UDP subprocess test is explicitly skipped in offline SIM
mode; its codec/worker tests and all offline firmware scenarios still execute.
Full ARM linking remains unavailable because arm-none-eabi-gcc is not installed.
No physical motor, LED, ADC, Bluetooth, battery-discharge or flashing test occurred.

Useful commands:

```sh
python3 tests/comms/run_tests.py
python3 tests/safety/run_tests.py
python3 tests/startup/run_tests.py
python3 tests/outputs/run_tests.py
python3 tests/patch7/run_tests.py
python3 tests/lifecycle/run_tests.py --grip --sanitize
python3 tests/lifecycle/run_tests.py --haptics --sanitize
python3 tests/lifecycle/run_tests.py --leds --sanitize
python3 tests/lifecycle/run_tests.py --battery --sanitize
python3 tests/lifecycle/run_tests.py --battery-defaults --sanitize
python3 tests/lifecycle/run_tests.py --commissioning --sanitize
python3 tests/lifecycle/run_tests.py --boot-safety --sanitize
python3 tests/lifecycle/run_tests.py --bad-force-config --sanitize
python3 tools/run_simulation.py --offline --no-sanitizers
```

## Remaining work requiring actual hardware/protocol values

- Decoding Status/Data lengths still default to zero. Set the agreed values before
  real control; the explicit-action host codec needs six class-vector bytes.
- Haptics UART, force ADC channels/reference and three red/green LED outputs are
  still intentionally unbound. Their software interfaces are present, not a
  completed electrical connection.
- Battery divider/reference and low/recovery thresholds remain unconfigured;
  actual battery monitoring/warnings cannot be claimed active yet.
- Encoder calibration/direction, mechanical travel limits, homing wrist spans,
  grip PWM/contact behavior and the force-voltage fit require board/mechanism
  validation. A contact stop removes motor output; physical retention is untested.
- Haptics state encoding and incoming-status timeout remain provisional.

The audit removes the reproducible software inconsistencies above; passing mocked
checks and static analysis is not proof of absence of faults or hardware readiness.
