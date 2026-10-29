# Implementation and verification report

Software verification date: **29 October 2026**. No board was flashed, no motor was driven, and no Git commit or push was made. Hardware behavior is unvalidated.

## Final architecture and files

The maintained firmware is one standalone STM32G474RET6 test bench. No N-Pulse board, second MCU, motor ID or hand application is required. The default Makefile contains an explicit, small source list; it does not discover/compile historical application directories.

New files: `fw/bench/bench.c/.h`, `bench_encoder.c/.h`, `bench_hw.c/.h`, `bench_protocol.c/.h`; `tools/bench_build.py`, `tools/motor_bench.py`, `tools/requirements-motor-bench.txt`; `tests/bench/run_tests.py`, `test_core.c`, `test_protocol.c`, `test_hw.c`, `test_motor_bench.py`, `mocks/main.h`; and six `docs/BENCH_*.md` documents.

Replaced/adapted integration files: `Core/Src/main.c`, `Core/Src/syscalls.c`, `Core/Inc/main.h`, `Core/Src/stm32g4xx_hal_msp.c`, `Core/Src/stm32g4xx_it.c`, `Core/Inc/stm32g4xx_it.h`, `Core/Inc/stm32g4xx_hal_conf.h`, `Core/Startup/startup_stm32g474retx.s`, `STM32G474RETX_FLASH.ld`, `tools/firmware.mk`, `build.ps1`, `flash.ps1`, `build-sim.ps1`, `N-PULSE FIRMWARE.ioc`, `.project`, `.cproject`, and `README.md`.

The existing pure `motor_pid.c/.h` is reused unchanged: one instance, measured elapsed time, derivative on measurement/filter, integral cap, saturation and conditional anti-windup. STM32 HAL/CMSIS, RET6 startup/vector table, system clock support and linker memory layout are retained. System SRAM is 96 KiB; CCM is left unused; 8 KiB of stack is reserved for JSON/printf call depth.

Old encoder and L298N implementations were inspected. Their rollover/calibration and gating patterns informed the scalar modules, but role-dependent mappings, multi-motor arrays and unsupported nominal encoder constants were not copied into the active target. FreeRTOS was removed from the active build because this application has one serialized foreground owner; the interrupt safety path remains independent. Third-party libraries and historical application sources were not deleted.

The pre-existing local edits to `.vscode/settings.json`, `fw/motors/motor_l298n/motor_l298n.c` and `fw/motors/motor_map/motor_map.c/.h` were left untouched. These legacy motor files are not compiled.

## Actual ARM build

Commands executed from the repository root:

```powershell
.\build.ps1 -Rebuild
.\build.ps1
arm-none-eabi-size build/bench/motor-bench.elf
```

**Result: clean ARM compile/link successful, exit 0; subsequent incremental build successful.** The clean invocation was launched as `powershell.exe -NoProfile -File build.ps1 -Rebuild`, with output saved in `build/bench-build-final.log`.

Compiler: GNU Tools for STM32 ARM GCC 14.3.1; Cortex-M4F, hard float, Thumb, GNU C11, `-Wall -Wextra -Werror`, newlib nano with floating-point JSON formatting. There is one output directory and no legacy mode flags. `-Clean` deletes only the resolved `build/bench` tree. The build stamp includes sources and flags; old Debug/HW/SIM objects are not reused. The first integration link exposed an old default-interrupt fatal callback; startup now branches directly to the bench register cutoff/halt, and the final link succeeds.

| ELF section | Bytes |
| --- | ---: |
| text | 46504 |
| data | 476 |
| bss, including reserved stack/heap | 24872 |

Artifacts: `build/bench/motor-bench.elf`, `.hex`, `.bin`, `.map`. Do not program retired `n-pulse.elf` images. The ELF symbol/source audit found no old application, BoardLink, FreeRTOS, motor-map, ADC driver or USART3 initialization implementation. The device-wide vendor vector table naturally retains weak names for unused peripheral IRQs, all routed to the fatal cutoff; they are not initialized or enabled.

## Actual automated tests

The unified entry point is:

```powershell
python tests/bench/run_tests.py
```

Native compiler in this environment: MinGW GCC 6.3.0, C11, `-Wall -Wextra -Werror`. Tests compile and execute the actual encoder, controller, unchanged PID, protocol and hardware adapter with isolated HAL/register mocks.

| Suite | Final result and coverage |
| --- | --- |
| Core | **72 scenarios passed**: signed counts/reversals, rollover, ambiguous delta, zero/origin, location/gear/sign conversion, invalid calibration, velocity/timestamps, unavailable feedback; P/I/D/filter/saturation/anti-windup/invalid inputs; no startup movement, arming preflight, fault latching/recovery, cap enforcement, limits, stale loop/link/feedback, count/velocity anomalies, stall, bounded pulse, settled target and PID reset |
| Protocol | **Passed**; **73 production C JSON packets** accepted by the real Python parser. Strict numeric/argument validation, line fragments/timeouts/overflow, invalid bytes, calibration/arming/movement gates, stop/emergency stop, congestion shutdown and fault events |
| Hardware adapter | **Passed** against production `bench_hw.c`: required pins/AF/clock/PWM initialization, no heartbeat arm rejection, zero/positive/negative PWM, fractional cap truncation, CCR preload update, 2 ms reversal dead interval, independent 50/500 ms cutoff boundaries, runtime encoder disable, emergency-stop gate latching, UART RX error/overflow/stale arrival poisoning and TX all-or-nothing framing |
| Python | **19/19 passed, no skips**: strict packet parser, null measurements, partial/overflow lines, CSV, positive/negative independently known synthetic step metrics, rollover/missing measurements, heartbeat/disconnect, operator-cancel/fault/stop batch behavior, and real matplotlib PNG export |

The restricted shell initially lacked matplotlib. The documented dependencies were downloaded only into build-local directories, without changing global Python. The final full plotting-enabled test command was:

```powershell
$env:PYTHONPATH = Join-Path (Get-Location).Path 'build\bench-python'
$env:MPLCONFIGDIR = Join-Path (Get-Location).Path 'build\bench-mpl-cache'
python tests/bench/run_tests.py
```

This command ran through approved execution because the restricted Windows process could not read dependency files created by the approved installer. That was an environment access limitation, not a remaining software test failure. Normal local virtualenv installation is documented for users. `motor_bench.py --help` and `--list` were also executed without opening a serial port. The programming helper dry run selected only `build/bench/motor-bench.elf` and did not invoke STM32CubeProgrammer. No COM port was attached. `git diff --check` found no whitespace errors (Git reported only expected LF/CRLF normalization notices).

Synthetic unit traces exercise algorithms only; they are not measured motor responses. No physical encoder measurement, PID response curve or performance metric is claimed. The PNG test uses explicitly synthetic samples and temporary output.

## Hardware and regeneration limitations

The actual compiled configuration uses HSI16 / 4 * 85 / 2 = 170 MHz SYSCLK, APB1/APB2 undivided. TIM4 has PSC=0, ARR=8499, giving 20 kHz. TIM2 is 32-bit hardware x4 TI12 quadrature on AF1 PA0/PA1 with filter 4 and no internal pull-ups. PB7 is AF2 TIM4_CH2 only during applied PWM. PC4/PC5 are GPIO direction outputs. LPUART1 uses AF12 PA2/PA3, PCLK1 with UART prescaler 4, 115200 8N1. SysTick is the only HAL tick, priority 0 at 1 kHz; UART priority 2. These are compiled configuration checks, not oscilloscope measurements.

Connector numbers/AFs/VCP bridges were checked against ST's device datasheet and UM2505; see the direct source links in [BENCH_WIRING.md](BENCH_WIRING.md). Actual board revision/continuity and signal electrical behavior still need checking. CubeMX was unavailable: the hand-maintained `.ioc` reflects the intended peripherals, but CubeMX import/regeneration, CubeIDE GUI integration and generated output comparisons remain unexecuted. Regenerate only in a scratch directory and preserve the reviewed application/interrupt safety code.

Unresolved physical specifications: exact motor nominal voltage/current, encoder model/resolution/supply/output interface/pinout/shaft location, assembly gear ratio and mechanical travel. No current acquisition or software overcurrent protection exists. A stationary disconnected encoder cannot be identified from a stationary counter alone. Operator verification, configurable feedback/motion checks and the lab supply current limit are necessary but do not replace electrical compatibility checks or a physical shutdown. Output pins begin high impedance at MCU reset before startup configures them low; reset/power-loss bridge behavior must be physically established.

## Work before motor power

Follow [BENCH_TEST_GUIDE.md](BENCH_TEST_GUIDE.md) in order: USB/manual flash with supply off; physically verify disabled outputs/VCP routing; obtain encoder specifications and verify passive counts/scale/direction; identify motor rating and module power arrangement; configure relative zero, travel/speed/step/time limits, stall thresholds and conservative unvalidated PID/PWM; set and verify the hardware supply current limit; acknowledge wiring; supervise a bounded pulse; verify stop/heartbeat loss; then request a small signed closed-loop step and record actual results. Do not automatically home against hard stops.
