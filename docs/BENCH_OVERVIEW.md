# Standalone motor bench

The only target is STM32G474RET6 on an unconnected NUCLEO-G474RE. One standard L298N module drives one motor; TIM2 measures the incremental encoder. The former N-Pulse board, second STM32, finger model and application protocol have no role in this firmware. The gearbox marking `14GPT 203:1 8016846` does not identify the motor voltage or encoder electrical specifications.

## Active architecture

| Source | Responsibility |
| --- | --- |
| `Core/Src/main.c` | One serialized foreground owner, measured 10 ms control scheduling, bounded RX processing and status LED |
| `fw/bench/bench_hw.c` | Board clock, only the required GPIO/TIM2/TIM4/LPUART1, nonblocking UART rings, safe bridge direction transitions and interrupt cutoff |
| `fw/bench/bench_encoder.c` | 32-bit rollover-aware raw deltas, 64-bit counts, timestamped samples, explicit calibration and relative zero |
| `fw/bench/bench.c` | One PID instance, state machine, limits, motion/stall checks, arming and latched faults |
| `fw/bench/bench_protocol.c` | Strict ASCII commands, JSON responses, bounded telemetry and configuration inspection |
| `fw/motors/motor_control/motor_pid.c` | Reused unchanged P/I/D algorithm, derivative on measurement/filter, saturation and conditional anti-windup |
| `tools/motor_bench.py` | Serial heartbeat, operator commands, live plots, CSV and recorded step-response analysis |

Control flow: output-shaft target -> PID -> signed PWM -> L298N -> motor -> real TIM2 encoder counts -> measured output-shaft radians -> PID. There is no time-only estimated position or simulated measurement in the target.

The initial PWM setting is 5%; PID, encoder, physical travel, maximum velocity, count anomaly and stall settings must all be entered explicitly. The hard software PWM ceiling is 25%. Configuration changes are accepted only while disarmed and fault-free. There is no persistent configuration or restored movement after reset.

FreeRTOS is no longer needed for this small application: one foreground owner avoids shared state/transport races. SysTick is the sole HAL timebase at 1 kHz, priority 0; LPUART1 interrupt priority is 2. A measured 100 Hz foreground update runs the controller; it uses actual elapsed milliseconds. The interrupt checks armed foreground starvation beyond 50 ms, heartbeat age beyond 500 ms and the encoder timer running state, then cuts the bridge using fixed registers. UART reception of `!`, RX overflow and UART errors also shut down immediately. Foreground code records and reports the cause. Fatal CPU faults and unhandled interrupts use the same register cutoff and halt. No scheduler is needed for these cutoff paths.

Startup assembly forces the three bridge pins low immediately after SystemInit, before RAM initialization. HAL/peripheral initialization repeats the disabled state. Arming leaves PWM zero. Every target completion, pulse completion, stop and fault disables and disarms. Targets settle only after both position and velocity stay within configured tolerance for 100 ms. No holding torque or automatic homing is supplied. The test fixture must be safe when torque is removed.

At zero PWM and faults PB7 is driven low as GPIO, IN1/IN2 low, TIM4 channel 2 disabled, compare zero and timer stopped. Reversal de-energizes for at least 2 ms and resumes on a later control update. Quantization truncates PWM to avoid exceeding its requested cap.

## Removed active dependencies

The default build no longer compiles `old_hw.c`, any motor map, multi-motor driver/backend, commissioning/hand/EMG/application logic, board roles, PC0 role detection, USART3, EmbeddedProto, simulation, motor estimation, ADC/DMA acquisition or current-based safety. There are no motor-ID arrays or hand loops. HAL DMA support remains a library dependency of HAL TIM/UART, but no DMA channel is configured. HAL, CMSIS, the RET6 startup/linker and the existing pure PID implementation are reused. The old motor/encoder architecture was audited; its nominal 12 PPR/61:1 calibration and role-dependent L298N driver were unsuitable and are not reused.

Historical sources and third-party libraries are left unbuilt as reference. Local edits already present in the old motor-map/driver and VS Code settings were preserved. The old simulation entry point now reports its retirement. The programming helper selects only the bench image.

## CubeMX and IDE maintenance

`N-PULSE FIRMWARE.ioc` retains its filename for discovery but records only the new bench pins, timers, clock, UART and SysTick; FreeRTOS/TIM6 and unused pins are removed. Actual initialization is maintained in `bench_hw.c`, outside Cube-generated application logic. CubeMX is unavailable in this environment, so opening/regenerating the edited manifest has not been validated. Regenerate only into a separate scratch directory, compare AF/clock/filter/UART-prescaler settings and transfer reviewed changes manually. Never regenerate over this working tree: generic generated main/interrupt files omit the bench cutoff, UART rings and control application.

CubeIDE metadata selects an external Makefile target with the RET6 device. Its GUI import/build was not exercised; the verified build is `./build.ps1` / the repository Makefile, not stale generated subdirectory makefiles. `make -C Debug` also delegates to the repository target. Old build artifacts outside `build/bench` are never used or published.

## Limits of software evidence

No physical board, driver, motor or encoder was operated. Timer initialization/running does not prove connected encoder wires, supply compatibility, direction or signal quality. An idle encoder cannot be distinguished from a disconnected stationary encoder in software. Manual encoder verification is required; missing motion under sufficiently high configured PWM trips the configurable stall detector. Low PWM below that threshold still has a maximum motion duration. The external laboratory supply provides the only current limiting; there is no current sensor, ADC measurement or firmware overcurrent protection. Interrupt watchdogs cannot replace a physical power cutoff or protect against every MCU/hardware failure.
