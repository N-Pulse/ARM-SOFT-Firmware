# Individual finger tests through the MASTER

The PC stays connected to the MASTER's existing COM port. The MASTER executes
LITTLE commands locally and relays THUMB, INDEX, MIDDLE and RING commissioning
commands/replies through USART3. The owning board retains its local encoder,
PID, output driver and safety checks. No direct SLAVE COM connection is needed.

## Temporary direct-PWM diagnostic test

This explicit profile is for the stalled finger investigation. It disables the
stall GUARD, configured travel bounds, maximum target step, preflight gate and
encoder requirement by default. It applies the requested **constant PWM** for a
calculated duration, like `simulation_pipeline`. Angles are **estimates**, not
encoder measurements. The empirical conversion is 120 degrees/second at 100%
PWM, scaled by the requested PWM; a 30-degree change at 25% takes about 1 second.

The estimated position starts at zero when the boards reboot. Place the fingers
at the intended zero first, or explicitly supply `--from-angle` for the selected
finger after reboot if its current position is known approximately.
Build once and flash the same diagnostic image on both boards:

```powershell
.\build.ps1 -Fingers -Diagnostic
.\flash.ps1 -Fingers -Diagnostic -SerialNumber 0033002C4142500620353451
.\flash.ps1 -Fingers -Diagnostic -SerialNumber 003900254142500620353451
python tools/send_action.py --port COM8 --raw 10 --angle 30 --max-angle 30 --pwm 25 --run --diagnostic
```

This requests THUMB at **30 degrees from the power-on zero**, driven at **25% PWM**.
The firmware cuts drive after the calculated duration, with an absolute maximum
of **2 seconds**. STOP/Ctrl+C, one selected motor, host/link watchdogs, controller
timing and output-driver checks remain active. Encoder validity/freshness do not
block this timed profile; invalid encoder data stays unavailable in telemetry.
There is no powered hold after completion and no automatic retry. The diagnostic
PWM range is 1..40%. Keep the mechanism unloaded and use a physical cutoff.
The timing conversion cannot guarantee 30 degrees of physical finger motion.
Load, supply voltage and friction change the actual distance traveled.

To return to the estimated zero, run a second command without rebooting:

```powershell
python tools/send_action.py --port COM8 --raw 20 --angle 0 --max-angle 30 --pwm 25 --run --diagnostic
```

The estimate survives STOP, console restarts and selection changes, independently
for each finger, until a board reset. It follows elapsed drive time even after an
interrupted test; it is never claimed to be measured. If the pouce is already
closed when this new firmware is flashed, an explicit approximate starting angle
allows a reverse test without redefining the encoder zero:

```powershell
python tools/send_action.py --port COM8 --raw 20 --angle 0 --from-angle 30 --max-angle 30 --pwm 25 --run --diagnostic
```

`--from-angle` updates only the timed estimate while disabled, through the MASTER
and USART3. It is an operator-supplied estimate and never an automatic recovery of
lost encoder position. Omit it during ordinary close/open cycles. A capped test
that stops before the estimated target exits with failure; no retry/rearm occurs.

The image is `build/HW-fingers-optin-diagnostic/n-pulse.elf`. Both flashing and
running it require the explicit diagnostic option; the ordinary image retains
its existing checks and 5% common-finger PWM ceiling. No rebuild is needed when
selecting another finger: raw 10/11/12/13/14 selects thumb/index/middle/ring/little.

Restore checks individually, then flash **both** boards again with
`-Fingers -Diagnostic`. The switches can be combined:

```powershell
.\build.ps1 -Fingers -Diagnostic -CheckGuard
# Other switches: -CheckTravel -CheckStep -CheckPreflight -CheckEncoder
```

`-CheckGuard` restores the 6-count/180-ms stall check. `-CheckTravel` restores
the configured travel bounds. `-CheckStep` restores the maximum target step,
so a direct 30-degree jump will be rejected; use a target within 5 degrees of
the current reference to test it. In timed mode travel/step checks use estimates.
`-CheckPreflight` restores the preflight gate
(the automatic host sequence still sends preflight). Telemetry prints the
enabled checks as `diagnostic_checks=...`.
`-CheckEncoder` restores the previous encoder-dependent diagnostic controller:
fixed PWM stops at the measured target/crossing or 2 seconds, and invalid/stale
feedback prevents actuation. `--from-angle` is rejected in that profile.

To restore the standard profile completely, rebuild with `build.ps1 -Fingers`,
flash both boards with `flash.ps1 -Fingers -SerialNumber ...` **without**
`-Diagnostic`, and omit `--diagnostic` from the Python command. The sections below
describe that standard profile.

## Build once and load the common image on both boards

```powershell
.\build.ps1 -Fingers
```

This produces `build/HW-fingers-optin/n-pulse.elf`. Select the finger at runtime
with `--raw XX` or `--finger NAME`; no rebuild or reflash is needed between fingers.
Load the **same common image on both boards**. USART3 protocol is now
**v3**; both boards must be updated together. List ST-LINK serial numbers with
STM32CubeProgrammer's `-l stlink`, identify each board, then select each explicitly:

```powershell
.\flash.ps1 -Fingers -SerialNumber YOUR_MASTER_STLINK_SN -DryRun
.\flash.ps1 -Fingers -SerialNumber YOUR_SLAVE_STLINK_SN -DryRun
# After checking the two selections, program and verify one board per call:
.\flash.ps1 -Fingers -SerialNumber YOUR_MASTER_STLINK_SN
.\flash.ps1 -Fingers -SerialNumber YOUR_SLAVE_STLINK_SN
```

Replace both serial-number placeholders. `-DryRun` only enumerates probes and
shows the programming command. Remove it to write, verify and reset the selected
board. Board role still comes from the PC0 strap; the binary is identical.

Only one finger can be enabled across both boards. Selection stops the old
finger on both boards before confirming the new selection. Calibration and
limits are kept independently per finger in RAM; preflight and arming must be
explicit after a switch. The ordinary `build.ps1` image stays inhibited.
The older `-Finger NAME` fixed-finger build/flash option remains available.

The common `-Fingers` image automatically loads the same nominal encoder
conversion as `simulation_pipeline`: 12 pulses/channel, quadrature x4,
61:1 reduction, positive direction (2928 counts/output-shaft revolution).
No manual `calibrate` command is required. **The power-on position is zero**:
place the fingers at the intended open reference before powering/resetting the
boards. There is no automatic homing. Selecting another finger never re-zeros it.
These are nominal motor output-shaft angles, not a measured finger linkage ratio.

## Select the angle from PowerShell

For a single test of INDEX at 5 degrees, close any existing console with Ctrl+C,
then run this on the MASTER's COM port:

```powershell
python tools/send_action.py --port COM8 --raw 11 --angle 5 --pwm 3 --run
```

No further terminal input is needed. `--run` explicitly authorizes one test and
confirms the operator's electrical preflight. It selects the finger, checks fresh
encoder status, recovers once, sets limits **0..10 degrees**, maximum step
**5 degrees**, requested PWM ceiling (maximum **5%**) and reference rate
**45 degrees/second**, then preflights, arms, enables and moves. Each next setup
step waits for its acknowledgement and updated diagnostics; no typing delay
consumes the 5-second arm window. The script sends STOP at the target and exits.
Any rejection, fault, missing feedback or acknowledgement timeout aborts with STOP
and a nonzero exit code; there is no retry/rearm. Ctrl+C also sends STOP.

The zero remains the power-on position. To return to it, use `--angle 0` in the
same command. Other fingers use `--raw 12`, `13`, `14`; THUMB uses `--raw 10`.
The current common `-Fingers` image permits a THUMB bench test with PWM capped
at 5%, the existing 5-second arm deadline, encoder health/stall checks and STOP.
This firmware change requires reflashing both boards once. An older image can
still report `thumb_blocked=True`; changing the Python command cannot remove
that firmware gate.
Use `--max-angle DEG` only to explicitly supply a different verified travel ceiling
(up to 90 degrees). Missing encoder conversion requires updating both boards;
`--run` neither invents calibration nor zeroes the encoder. It requires the current
common `-Fingers` firmware, already loaded for the automatic encoder setup.
Adding this PC option does not itself require another build or flash.

The current common image uses a dedicated finger-test proportional gain of
1000 PWM-percent/radian, still clamped to the requested/firmware PWM ceiling.
The earlier generic gain of 10 produced only 0.35% for a 2-degree waypoint,
even when `--pwm 3` was entered: that option is a ceiling, not constant drive.
The bench gain is not a physically tuned production gain. At convergence within
half an encoder count, both reference and target must be close before drive is
released, preventing unresolvable position hunting. Encoder resolution comes
from the actual active scale, including any manual override.

Common-image host diagnostics now arrive every 100 ms, with the owning board
prioritizing its selected finger's USART3 feedback every 20 ms. `--run` prints
short `Movement:` lines showing measured angle, target, **actual applied PWM**
and raw count changes. The guard remains **6 counts / 180 ms** and latches STOP
if motion is insufficient; no retry or automatic release of a stalled motor
is added. A persistent GUARD means hardware power/wiring, mechanical freedom
and encoder operation need checking; it does not prove that the mechanism moved.
Changing the bench gain/telemetry requires reflashing both boards.

The manual commissioning console is still available without `--run`:

```powershell
python tools/send_action.py --port COM3 --finger index --angle 30 --pwm 3
# Same entry point with the simulation branch's finger-number shorthand:
python tools/send_action.py --port COM3 --raw 11 --angle 30 --pwm 3
```

`--angle` is an absolute angle in **degrees relative to the calibrated zero**.
`--pwm` is the maximum PWM percentage, not velocity. Raw aliases 10..14 select
thumb/index/middle/ring/little and default to 30 degrees; 20..24 select the same
fingers and default to zero. An explicit `--angle` overrides either default.
The CLI sends commissioning messages rather than undefined protobuf action IDs.
In the open console, `raw 12 20 3` switches to MIDDLE and stages 20 degrees with
a 3% PWM ceiling; `raw 14 10 3` selects LITTLE. The common image selects and
stops both boards automatically. First-time limits and explicit
`link-recover`, `preflight verified`, `arm`, `enable`, `move` remain necessary.

This opens a persistent commissioning console. The requested angle is staged;
opening the port never arms or starts motion. The console maintains the host
heartbeat; the MASTER forwards that live lease to the SLAVE.
The console sends a startup STOP to discard any command left by a previous session.
Type the setup commands in this console even while diagnostics arrive. By default,
only status changes are printed; `--verbose` restores every diagnostic sample.
`selectable=False` means the connected firmware is a fixed-finger image;
runtime selection and nominal boot encoder defaults need the current `-Fingers`
image flashed on both boards. `encoder=unavailable` blocks motion; first check
that both boards were updated after the automatic encoder setup change.
`limit-deg` is accepted as an alias for `limits-deg`.

If setup/motion returns `STALE` with `LINK`, read both boards' latched causes:

```powershell
python tools/hw_monitor.py --port COM8 --link --duration 1
```

`transport_errors` is cumulative since boot, not an error rate. The current
firmware enables USART3's hardware FIFO (RX threshold one byte) and keeps its IRQ
disabled after a receive error until the worker clears/rearms it. This buffers
short masked intervals during motor/protocol work and prevents repeated callbacks
from starving recovery. Real link loss/errors still stop both boards.
`last_transport` identifies RX error/rearm/overflow, TX start/timeout, RX abort or
entropy failure; `last_uart_error` reports the last captured HAL UART error bits
(PARITY/NOISE/FRAME/OVERRUN/DMA/TIMEOUT). These are additive LinkDiagnostic flag
bits, without a protocol version change. FIFO configuration/recovery and STOP
were checked in native fixtures; actual interrupt latency and the source of
physical line errors still require checking on the boards. USART3 remains
PB10 TX / PB11 RX at 115200 baud, TX-to-RX between boards with a common ground.

Enter the following commands separately, waiting for each `result=OK` and fresh
diagnostics. Substitute verified travel limits (angles in degrees):

```text
link-recover
limits-deg MIN_DEG MAX_DEG STEP_DEG PWM_MAX RATE_DEG_PER_SECOND
status
preflight verified
arm
enable
move
```

`link-recover` explicitly recovers both boards. If a stop persists, inspect
`link` and `status`; resolve the reported cause before repeating recovery.
Use `status` to return from link diagnostics to the selected finger before
moving. `preflight verified` retains its existing electrical/reference checks;
it is not a calibration or automatic homing operation.

The original `limits MIN_RAD MAX_RAD STEP_RAD PWM_MAX RATE_RAD_PER_SECOND`
also works. Limits and optional encoder overrides are runtime settings and are
not persisted. The fixed `-Finger NAME` image still requires explicit
`calibrate CPR QUADRATURE GEAR_RATIO DIRECTION RAW_ZERO_OFFSET`; it does not
load the common image's defaults. The same command remains available to override
the nominal encoder conversion in `-Fingers` if the motor/wiring differs.
`zero` is optional, only at a verified mechanical reference; it resets
both raw count and calibration offset. Do not zero just to clear a fault.

`move` uses the angle/PWM chosen in PowerShell. Alternatively enter
`angle 12.5 3` to choose another degree target in the same console. The host
waits for measured position and reference to converge at successive small
targets; the firmware still enforces each target's original step limit.
The host sends STOP when the requested angle is reached. This releases drive;
it does not promise a powered position hold. `stop`, Ctrl+C or console exit also
send STOP. Another move after STOP requires explicit recovery/preflight/arm/enable.

## Calibration and conditions that block a test

- The old compile-time ASCII `MOTOR_CALIB_MODE` is already disabled. Leave it
  disabled: enabling it is rejected at compilation and would replace the normal
  communication pipeline. Encoder calibration remains available through
  `calibrate` as an optional override. In `-Fingers`, the nominal conversion
  and boot zero are already loaded; encoder health/freshness checks remain active.
- Default dedicated-build PWM ceiling: **5%**. Runtime PWM must not exceed it.
  The host does not raise it to the simulation branch's 40%.
- Maximum per-command step: **0.1 rad (5.73 degrees)**. A long requested angle is
  divided into feedback-paced targets within the entered step and travel limits.
  Remote feedback has 1-mrad resolution; the host requires a remote step limit
  of at least 0.005 rad. Targets must lie within the declared limits and 0..90 degrees.
- Arming lasts at most **5 seconds**. A slow/large movement may stop before its
  goal; no automatic rearm or recovery occurs.
- Missing/invalid encoder conversion, stale encoder feedback (100 ms), control delays (50 ms),
  a stall (180 ms without enough encoder counts), host loss (2 s), USART3 loss
  (150 ms), STOP/fault latches or missing preflight block/stop motion. Unselected
  motors stay disabled. Only local fingers receive the nominal defaults;
  wrists/palm and remote slots do not.
- **THUMB is permitted for bench testing in the common `-Fingers` image only**,
  with an independent PWM ceiling of 5%. The qualification flag stays
  `THUMB_EXTI_MOTION_QUALIFIED=0`: the software quadrature decoder's latency
  under real load has not been physically qualified. Ordinary and fixed-finger
  images retain their qualification gate. `thumb_test=True` distinguishes the
  common bench permission; invalid transitions or a disabled EXTI interrupt
  invalidate feedback and stop the drive. The bench option does not establish
  physical encoder accuracy or latency qualification.
- The old hardware has no calibrated current measurement/protection in this
  firmware. The software limits do not establish safe mechanical travel/power.

Verification uses native fixtures of the real two-board adapter/controller and
host codec/planner tests. No real motors, probes or serial devices are used by
those tests; physical operation still needs verification on the actual hardware.
