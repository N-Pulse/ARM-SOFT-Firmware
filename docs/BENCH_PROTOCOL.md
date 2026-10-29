# Motor bench serial protocol

The standalone NUCLEO-G474RE uses **LPUART1 PA2/PA3, 115200 baud, 8N1**, over the ST-LINK virtual COM port. There are no board addresses, motor IDs, binary frames, ADC readings or inter-board messages.

Commands are case-sensitive ASCII, separated by spaces or tabs and terminated by LF or CRLF. Maximum content length is 191 bytes. Complete a line within 250 ms. Overflow, non-ASCII input, embedded CR and incomplete timed-out lines are discarded **through the next LF**; a trailing fragment cannot become a command. Floating-point arguments accept finite decimal/scientific notation. Unsigned integers accept decimal digits only. Extra arguments are rejected. Invalid commands never initiate motion.

The `!` byte is an emergency stop, independent of line completion. The UART interrupt cuts the motor outputs before the foreground handles the fault. Use `!` followed by LF, or `estop` followed by LF. A reset or reconnection never starts a motor.

## Commands

All angles refer to the **gearbox output shaft**, in radians. Velocity is rad/s; time arguments ending in `_ms` are integer milliseconds; PWM arguments are percentages, not fractions. Configuration is volatile and must be repeated after reset. Configuration changes require disarmed, fault-free state and invalidate the wiring acknowledgement.

| Command | Meaning and constraints |
| --- | --- |
| `help` | Command names; use this document for parameter details. |
| `status` | Full measurement, state, fault and configuration snapshot. |
| `encoder` | Same snapshot, suitable for passive encoder inspection. |
| `fault` | Same snapshot, including latched fault cause. |
| `ping` | Renew the communication heartbeat. Send about every 100 ms. Other commands do not renew it. |
| `calibrate PPR 4 GEAR SIGN LOCATION` | PPR is **full electrical cycles of one channel per encoder-shaft revolution**, not x4 counts/revolution. PPR is 1..10000000. Quadrature must be 4. GEAR is motor turns per output turn, 1..100000. SIGN is exactly `1` or `-1`. LOCATION is `motor` or `output`. Gear factor is applied only for `motor`. No encoder calibration is supplied by default. |
| `zero` | Set a temporary relative zero at the current counter position. Works before calibration. This is not physical homing. Invalidates previous position limits. Calibration clears the zeroed acknowledgement, so zero again after calibrating. |
| `verify encoder` | Operator confirms the encoder is correctly powered and its direction and scale have been checked manually. Requires calibration, zero and observed count changes after calibration. Count changes alone are not electrical validation. |
| `limits MIN MAX STEP VELOCITY MOVE_MS` | Set allowed output position interval, maximum displacement of **both absolute and relative** commands, maximum measured speed, and maximum continuous motion duration. MIN < MAX, within ±1000000 rad; STEP > 0 and no larger than interval span; VELOCITY > 0; MOVE_MS is 10..60000. These broad software bounds are not safe motor specifications. Select much smaller physical limits for the bench. |
| `pid KP KI KD I_LIMIT TAU PWM TOLERANCE` | KP: %/rad; KI: %/(rad·s); KD: %·s/rad. Gains are nonnegative, and at least one is positive. I_LIMIT: 0..25% integral contribution; TAU: 0..10 s derivative filter constant. PWM: >0..25% configured cap; start at **5% or lower**. TOLERANCE: positive output radians. PID elapsed-time ceiling is fixed at 50 ms. Any trial gains are unvalidated until measured on hardware. |
| `safety DELTA STALL_PWM STALL_RAD STALL_MS SETTLE_VELOCITY` | DELTA: largest plausible raw x4 counter change per sample, 1..100000000. STALL_PWM: positive % threshold, no greater than configured PWM cap at arming. STALL_RAD: minimum net encoder displacement over the stall window; must be at least one calibrated count at arming. STALL_MS: 50..5000 ms. SETTLE_VELOCITY: positive rad/s threshold for target completion. Derive thresholds from the actual mechanics and encoder; supplied software ranges are not physical validation. |
| `ack wiring` | Operator confirms the actual motor rating, selected supply voltage, current limit, encoder electrical compatibility and wiring have been checked. This is a human acknowledgement, not automated proof. Send after configuration changes. |
| `arm` | Arm with zero PWM. Requires a fresh heartbeat/sample, valid calibration, relative zero, verified encoder, configured PID/limits/safety, acknowledged wiring and no fault. Arming itself does not move. |
| `move RAD` | Absolute bounded position target. Requires armed/stopped state. |
| `jog RAD` | Relative bounded position step. Requires armed/stopped state. |
| `pulse SIGNED_PWM DURATION_MS` | Explicit open-loop commissioning only. Requires the full arming preflight; cap is the smaller of configured cap and 5%; duration is 1..500 ms and cannot exceed MOVE_MS. Position, step, speed, encoder, stall and communication checks remain active. A short pulse does not bypass calibration. |
| `stop` | Immediately disable outputs, reset PID and disarm. |
| `disarm` | Same safe disabled state as stop. |
| `estop` | Disable outputs and latch emergency-stop fault. |
| `clear_fault` | Explicit recovery with outputs disabled. Pending hardware faults must first be serviced. Reinitializes the relative encoder reference, invalidates encoder verification, zero, motion limits and wiring acknowledgement. Inspect the cause, manually verify/zero, reconfigure limits and acknowledge before arming again. No automatic retry. |
| `stream on` | Request telemetry at 20 Hz. |
| `stream on HZ` | Request 1..20 Hz independently of 100 Hz control. The actual rate may be lower to fit serial bandwidth. |
| `stream off` | Disable periodic telemetry; command replies and fault notifications remain enabled. |

For a motor-shaft encoder, the conversion is `counts × SIGN × 2π / (PPR × 4 × GEAR)` relative to zero. For an output-shaft encoder, omit GEAR in that denominator. The approximate gearbox reduction 202.71657754 does **not** establish the encoder PPR, supply voltage, output type, location or nominal motor voltage. Obtain those specifications before entering calibration or powering the motor.

Every motion finishes disabled and disarmed. A closed-loop target requires error within TOLERANCE and speed within SETTLE_VELOCITY for 100 ms; `target_reached` then becomes true. Every subsequent motion needs a fresh explicit `arm`. `stop`, `disarm`, faults and new movement clear `target_reached`. There is no position-holding torque after completion and no automatic hard-stop homing.

## Responses

Every packet is one newline-terminated JSON object. Ordinary command acknowledgement:

```json
{"type":"reply","command":"ping","ok":true,"message":"OK"}
```

Unknown commands or broken framing use `command:"invalid"`, `ok:false`. Rejected arguments, configuration or state use the recognized command name and a diagnostic message. `status`, `encoder` and `fault` emit a `type:"status"` packet followed by their reply. Startup emits `command:"hello"` with firmware identification. A newly latched fault also emits an unsolicited `command:"fault", ok:false` reply, even with streaming off. Clients must distinguish these asynchronous notifications from replies to their pending command. A flooding or congested TX path rejects ordinary commands with `command:"busy"` and disarms; stop/emergency stop remains effective without a successful reply.

Periodic `type:"telemetry"` and requested `type:"status"` share these fields:

| Fields | Interpretation |
| --- | --- |
| `t_ms`, `encoder_t_ms` | STM32 unsigned 32-bit millisecond timestamps; wrap after about 49.7 days. |
| `raw`, `counts`, `delta`, `direction` | TIM2 unsigned 32-bit count; signed accumulated 64-bit raw counts; sample signed delta; raw direction −1, 0 or +1. Raw counts/direction are not multiplied by calibration sign. |
| `target_rad`, `position_rad`, `error_rad`, `velocity_rad_s` | Output-coordinate values. JSON `null` means unavailable, never zero substituted for missing feedback. Scaled position/velocity can be inspected after calibration before verification, while `feedback_valid` remains false. |
| `pid_percent`, `pwm_percent` | PID demand and actual signed PWM (the latter includes safe direction-transition delays). During a commissioning pulse, PID demand is zero and PWM reports the pulse. |
| `dt_s` | Actual elapsed control interval, in seconds. |
| `state` | `disarmed`, `armed`, `moving`, `pulse` or `fault`. |
| `armed`, `encoder_available`, `calibrated`, `feedback_valid`, `target_reached` | Separate booleans. Available means the hardware encoder timer initialized/runs; valid additionally requires calibration and operator verification. Neither proves attached-wire integrity. |
| `fault` | `none`, `estop`, `rx_overrun`, `loop_timeout`, `communication_timeout`, `uart_error`, `encoder_invalid`, `position_limit`, `velocity_limit`, `counter_delta`, `motion_timeout`, `stall` or `pid_invalid`. |
| `telemetry_dropped` | Periodic packets skipped because TX was busy. Old telemetry is never queued for later delivery. |

Status adds `firmware:"stm32-motor-bench/1"`, `zero_counts`, `zeroed`, `encoder_verified`, `wiring_ack`, `session_live` and `configuration`. Configuration contains `ppr`, `quadrature`, `gear_ratio`, `encoder_sign`, `encoder_location`, `limits_configured`, `min_rad`, `max_rad`, `max_step_rad`, `max_velocity_rad_s`, `max_move_ms`, `pid_configured`, `kp`, `ki`, `kd`, `integral_limit`, `derivative_tau_s`, `max_pwm_percent`, `position_tolerance_rad`, `safety_configured`, `max_count_delta`, `stall_pwm_percent`, `stall_displacement_rad`, `stall_ms`, `settle_velocity_rad_s`.

The periodic stream is capped at 20 Hz and dynamically slowed to a maximum telemetry budget of 9000 bytes/s, leaving UART capacity for heartbeat and replies. Reply transmission and reception use one owner and nonblocking bounded buffers. Streaming does not renew the heartbeat. Heartbeat loss beyond 500 ms, or control starvation beyond 50 ms while armed, cuts the bridge in the independent 1 kHz hardware interrupt path.

No serial command flashes firmware, configures persistent settings, acquires motor current or proves motor/encoder electrical safety.
