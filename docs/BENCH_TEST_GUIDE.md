# Ordered commissioning guide

All hardware checks below remain **unexecuted**. Begin with the motor supply disconnected. Do not use old N-Pulse tools or instructions. Read [BENCH_WIRING.md](BENCH_WIRING.md) before connecting anything and [BENCH_PROTOCOL.md](BENCH_PROTOCOL.md) for exact command syntax and units. A fixture must remain safe when the bridge coasts with no holding torque.

## A. USB and disabled outputs, no motor supply

1. Install ARM GNU tools, GNU Make and Python 3.10+ in PATH. From the repository run `./build.ps1 -Rebuild` in PowerShell. For cleanup use `./build.ps1 -Clean`. Only `build/bench/motor-bench.elf`, `.hex` and `.bin` are current firmware outputs; older `Debug/` or `build/HW*` images are retired.
2. Connect the Nucleo ST-LINK USB with no motor supply. Manually program **that bench ELF** using STM32CubeProgrammer/ST-LINK. An optional manual helper is `./flash.ps1 -SerialNumber YOUR_STLINK_SERIAL`; add `-DryRun` to inspect its arguments. Neither building nor the Python tool flashes hardware.
3. Install the PC dependencies and find the port:

   ```powershell
   python -m venv .venv
   .\.venv\Scripts\python.exe -m pip install -r tools/requirements-motor-bench.txt
   .\.venv\Scripts\python.exe tools/motor_bench.py --list
   .\.venv\Scripts\python.exe tools/motor_bench.py --port COM6 --plot --csv first-test.csv --save-plot first-test.png
   ```

4. Substitute the listed COM port; close other serial programs. Enter `status`, `encoder`, `stream on 20`. The firmware identifies itself as `stm32-motor-bench/1`, disarmed with zero PWM, no valid calibration and null position/velocity. The tool maintains `ping` every 100 ms. Opening a port never arms or moves.
5. With a meter/scope verify PB7, PC4 and PC5 low and the module enable inactive. Check reset and programming behavior as well. An LED heartbeat is only a software liveness indication. If VCP fails, check ST-LINK drivers/cable and SB17/SB23 routing. No USART1 or USART3 is used.

## B. Encoder only

1. Establish the actual encoder supply voltage, pinout, electrical output type/levels, resolution and shaft location. Keep motor power off. If compatible power/interface hardware is unavailable, this phase is blocked; do not guess or attach 12 V to encoder/MCU pins.
2. Connect verified power/reference and A/B to PA0/PA1. Use `encoder` or `test encoder 10` while rotating the shaft manually. Observe `raw`, signed `counts`, `delta`, raw `direction` and `encoder_t_ms`; they work without calibration. Rollover is normal; accumulated counts should remain continuous. Do not force/backdrive a gearbox that cannot safely be turned.
3. Inspect actual A/B voltage levels and quadrature phase with suitable instruments, including direction reversals. A changing counter alone is insufficient. If no counts change, check power, ground, connector routing, output interface and whether you are rotating the encoder shaft.
4. Determine PPR as full cycles on **one channel per encoder-shaft revolution**. If a manufacturer gives decoded x4 CPR, divide that value by 4 before entering PPR. Confirm motor-versus-output location and gear ratio. Enter `calibrate PPR 4 GEAR SIGN motor` or `calibrate PPR 4 GEAR SIGN output`, replacing every uppercase value with the verified number and selecting `1` or `-1` for SIGN. The approximate 202.71657754 reduction is a candidate gear ratio only; it supplies no encoder PPR or voltage information.
5. **After calibrating**, manually rotate a known safe angle in both directions. Check the measured output position/velocity and scale against an independent reference. For motor-shaft location, position per count is `SIGN * 2*pi/(PPR*4*GEAR)`; for output location omit GEAR. Degree conversion is radians times `180/pi`.
6. Return to the intended relative reference and enter `zero`, then `verify encoder`. The firmware requires observed count changes after calibration and this explicit acknowledgement. These are human verification steps, not automatic signal-integrity tests. `zero` may be used uncalibrated for raw monitoring, but must be repeated after calibration. Zero is never physical home.

## Configuration before any powered motion

Set limits only after choosing and checking the zero. These commands show syntax; **angle/gain examples are unvalidated and must be adapted to the actual fixture**:

```text
limits -0.05 0.05 0.01 0.5 2000
pid 1 0 0 0 0.02 5 0.001
safety MAX_COUNT_DELTA STALL_PWM STALL_RAD STALL_MS SETTLE_VELOCITY
ack wiring
status
```

The example limits mean +/-0.05 rad travel, at most 0.01 rad per move, 0.5 rad/s maximum measured speed and 2000 ms maximum motion. The example controller is P-only, Kp=1 %/rad, no integral/derivative, 5% cap and 0.001 rad tolerance; it may be too weak to move and must never be described as tuned. Add Ki/Kd only from measured results. Choose tolerance compatible with encoder quantization, velocity measurement and mechanism clearance.

Replace **all** safety placeholders with physically justified values. For output counts/revolution `C`, a maximum velocity `V` rad/s gives approximately `C*V*dt/(2*pi)` counts per sample. Select the raw count anomaly allowance for the largest permitted sample interval (up to 0.05 s), with an explained margin; velocity is checked separately. `STALL_RAD` must be at least `2*pi/C` and reflect the displacement expected over the chosen stall interval at `STALL_PWM`; `STALL_PWM` must be positive and no greater than the configured PWM cap. Choose a settle velocity large enough for real encoder quantization/noise and small enough for safe completion. No universal physical thresholds are provided.

Use `ack wiring` **last**, after checking the actual motor rating, selected supply voltage/current limit, encoder interface, ENA jumper, outputs and fixture. Configuration changes invalidate that acknowledgement. There is a hard 25% PWM ceiling; first commissioning should stay at 5% or below. Requested limits or gains beyond software bounds are rejected. There is no current-based software protection.

## C. Brief open-loop direction commissioning

1. Confirm the motor nominal voltage and the specific L298N logic-power arrangement; 12 V is merely the proposed lab setting, not an approved motor rating. Configure and verify the supply current limit. Ensure accessible supply shutdown and no unsafe load when torque is removed.
2. With configuration complete and physical disabled outputs confirmed, enable the verified motor supply. Enter `ack wiring`, `arm`, then inspect `status`: armed, no fault, PWM zero.
3. Enter one deliberately short command such as `pulse 2 100` only if 2%/100 ms is suitable for the identified assembly. The pulse is limited to the smaller of 5% and the configured cap and at most 500 ms; all calibration, encoder, travel, speed, step and watchdog checks remain active. It automatically disables/disarms. Observe motor/encoder direction and actual ENA waveform.
4. Enter `stop` and verify all outputs low. Use `!` to latch emergency stop and verify immediate bridge disable. Wrong direction or missing encoder movement requires shutdown and investigation. Do not increase PWM simply to overcome a possible wiring/stall issue. If direction sign changes, disarm, recalibrate, manually verify/zero, set limits and acknowledge again.
5. Test communication timeout at a safe test condition: terminate the PC process while **leaving the USB cable connected and MCU powered**. PWM must cease within the 500 ms heartbeat bound plus the 1 ms interrupt granularity. Reconnect and inspect the latched communication fault; reconnection must not start motion. Clean tool exit sends stop immediately. Removing USB may remove MCU power, so independently verify the electrical enable state before considering that a watchdog test.

## D. First encoder-based position step

1. With motor power off again if anything changed, repeat calibration/zero/limits verification as required. Check conservative PID/safety settings and view all values with `status`. There is no automatic hard-stop homing.
2. After rating/supply/fixture checks, enable supply and enter `ack wiring`, `arm`, then a suitably small command such as `jog 0.005` only if it fits the justified limits. Position, target, error, velocity, actual signed PWM and fault are visible in the plot/CSV.
3. A settled target requires position tolerance plus low velocity continuously for 100 ms; the bridge then disables and disarms with `target_reached:true`. Each later movement needs `arm` again. A motion timeout or stall latches a fault instead of falsely reporting completion. Use `stop` or `!` whenever behavior disagrees with expectations.
4. Inspect the CSV and waveform. Manual encoder movement and voltage/PWM scope checks still do not constitute a completed PID validation; record the actual powered response.

## E. PID characterization and safety checks

Test separately signed small steps, starting with P-only. The tool supports `test positive RAD`, `test negative RAD`, `test step RAD` and `test repeat RAD COUNT`; each needs an explicitly armed bench and typed `RUN`. Repeat mode is a bounded explicitly authorized batch, stopping on fault, lost connection or operator stop. Choose smaller magnitudes if adjacent repeated targets would exceed the step cap.

Record CSV and export PNG. Analyze a single transition with `python tools/motor_bench.py --analyze response.csv --save-plot response.png`; crop multi-step logs to each response. See [BENCH_PC_TOOL.md](BENCH_PC_TOOL.md) for rise/settling time, overshoot, final recorded error and maximum error definitions. Missing or insufficient measurements yield unavailable metrics, not fabricated curves.

Verify rejection without arming, configuration while armed, excessive targets/PWM, malformed numeric commands and emergency stop. Verify motion timeout and encoder-motion stall shutdown only in a restrained, supervised fixture using safe supply limits; do not use mechanical hard stops as homing. Encoder timer failures, stale feedback, count/velocity anomalies and scheduler-independent cutoff are software-tested but still need physical fault-injection validation. Never unplug live encoder wiring as a casual stall test. Confirm disabled outputs after every fault and reset.

For recovery: inspect and remove the cause, `clear_fault`, manually rotate/check encoder, `zero`, `verify encoder`, set new `limits`, `ack wiring`, inspect `status`, then explicitly `arm`. Calibration and PID can remain configured after recovery; the relative origin, limits and acknowledgements cannot silently survive. A pending hardware fault may require one control service and a repeated explicit `clear_fault`. No automatic restart is supplied.

## Troubleshooting

- **Null position:** calibration is absent or acquisition failed. Raw counts remain useful; do not substitute zero for unavailable measurements.
- **Arm rejected:** inspect the reply, freshness/session, calibration, manual rotation/zero/verification, PID, limits, safety thresholds and final wiring acknowledgement. Safety stall displacement must be at least one decoded count.
- **No motion at small PWM:** inspect ENA, driver supply and voltage drop, mechanical friction and motor rating. A small cap is not a guarantee of rotation; do not bypass a fault.
- **Oscillation or wrong sign:** stop, remove power, confirm encoder/motor sign and scaling, then reduce/reassess gains. The existing PID assumes positive PWM increases the selected output coordinate.
- **Fault after delay/disconnect:** expected fail-safe behavior; inspect cause and repeat explicit recovery. The PC never reconnects/rearms automatically.
- **Dropped telemetry:** request `stream on 10`, reduce repeated status requests and close other UART users. Telemetry is throttled to preserve heartbeat/reply capacity and never blocks control.
- **CubeMX/IDE mismatch:** use the verified Makefile/PowerShell build; review the manifest/regeneration limitations in the overview. Never program stale old-firmware artifacts.
