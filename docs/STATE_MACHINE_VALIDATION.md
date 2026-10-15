# State Machine Validation Record (2026-10-10)

## Passed

- `tests/lifecycle/run_tests.py --sanitize`: 13 scenario groups passed. The tests compile the actual HW application, communication decoder, session manager, PID, safety interlock, and driver-control code with mocked HAL/RTOS interfaces. AddressSanitizer and UndefinedBehaviorSanitizer were enabled.
- State machine scenarios covered READY approval/denial, RESTART in invalid states, sequential actions, PID hold while waiting, STOP return during an action, a fresh handshake and initialization after stopping, ESTOP, recovery denial for a persistent force fault, initialization failure, initial handshake timeout, periodic PONG validation, repeated Hello messages not replacing PONG, motion timeout, protocol/peer exceptions, and transmission of historical safety logs across stop epochs.
- `tests/safety/run_tests.py`: 24 existing safety scenarios passed for each of the asynchronous feedback fixture and the actual HW backend. Additional disarm, startup/cleanup concurrency, actual encoder BSP integration, and numeric checks passed. Integration checks passed ASan/UBSan.
- `tests/startup/run_tests.py`: 15 startup/encoder/ADC scenarios passed. Encoder numeric checks passed ASan/UBSan.
- Local firmware portion of `tests/comms/run_tests.py`: 21 UART/parser/interlock/transmit-concurrency scenarios passed. SIM bounds checks covering 24,576 input combinations passed ASan/UBSan. The SIM protocol parser passed a 100,000-byte noise test.
- Independently executed native SIM scenarios `stop_saturation`, `inflight_stop`, and `tx_timeout`: all passed. These are regression checks for the existing SIM protocol, not hardware execution of the HW state machine.
- New Python client: Hello, action 0/1, STOP encoding/decoding, and syntax checks passed. No physical serial port was opened.
- The root Makefile build plan includes the new lifecycle_protocol.c file.

## Incomplete or Not Passed

- The full communication suite cannot be reported as passing. The existing independent localhost UDP service subtest in `tests/simulation/test_host.py` failed to start inside the sandbox. After execution outside the sandbox was allowed, it still timed out waiting for a response. The UDP service and test were not modified to hide this failure. Local firmware and independently executed native SIM results are listed above.
- The arm-none-eabi toolchain was not found in this environment, so no updated STM32 ELF was produced. The deliverable is a source package without a new flashable binary.
- No tests were performed on a physical STM32, encoders, motors, or prosthetic hand mechanism. Default encoder scaling, direction, PALM LPTIM quadrature factor, PID, and return-motion parameters require hardware verification.

## Changes and Delivery Scope

The state machine is implemented in `fw/app/app.c`, control messages and session management in `fw/comm/comms_session.c`, and protocol extensions in `fw/comm/lifecycle_protocol.*`. Safety events are recorded in `motor_safety.c` and transmitted asynchronously in `comm.c`. MotorGuard now recognizes settled position hold, and UART transmit failures trigger LINK protection.

The original ZIP was not modified. The delivered `state-machine.patch` can be compared against the extracted original ZIP. The source package excludes Git history, old Debug/build outputs, and Python caches to prevent accidental use of an old ELF. See STATE_MACHINE_CN.md for the complete state and protocol guide.
