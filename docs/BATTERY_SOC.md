# Battery percentage warning and shutdown

The agreed policy uses actual, calibrated state of charge (SOC):

| SOC | Arm LED1 | Motor behavior |
| --- | --- | --- |
| >=20% | Existing board/safety state | Existing control |
| <20%, >=10% | Solid red | Continue normal control |
| <10% | Red/off at 5 Hz for 30 seconds | Continue until deadline unless another safety fault stops earlier |
| Countdown complete | Solid red | All motors stopped, recovery/arming blocked until restart |

Only LED1 is overridden; Haptics LED2 and EMG LED3 retain their own status.
Thresholds are strict: 20% itself does not warn, 10% itself does not start the
countdown. Red/off half-period is 100 ms; time calculations use HAL milliseconds
and handle wraparound. The existing 10 ms task/50 ms LED update cadence adds
normal scheduling latency. There is no 30-second blocking delay.

`Battery_PublishStateOfCharge(percent, acquisition_ms)` accepts 0..100 percent
with the original acquisition time. `Battery_ReadStateOfCharge` is a weak,
nonblocking board-reader hook, polled every 100 ms. No default reader exists:
the Keeppower product page does not expose a fuel-gauge interface or SOC curve.
Voltage samples do not fabricate SOC. BMS protection alone is not treated as a
fuel-gauge reading. Use a characterized battery model or a real fuel gauge.
Until then the physical 20/10-percent policy has no measured input.

Duplicate, out-of-order and future readings are rejected. Out-of-range/NaN or
older-than-1000-ms readings are invalid; stale/invalid input cannot clear a low
warning. Once an SOC source has been used it remains the authority for the
displayed battery state, rather than falling back to voltage-based NORMAL.
Before any SOC input, the optional existing voltage-warning interface remains
available; its default thresholds are still unconfigured.

The first valid below-10-percent sample latches one deadline. Repeated packets,
voltage sag/rebound, later higher SOC, missing readings or decoding reconnection
cannot cancel or extend the countdown. At the deadline `Battery_Service` uses
the common immediate motor interlock with `MOTOR_STOP_BATTERY`. This cause cannot
be cleared by normal recovery. Other safety faults still stop immediately.

After deferred motor cleanup completes and the driver is disarmed,
`Battery_BoardRequestPowerOff()` is called once, outside the motor lock. The
default returns false: no verified STM32-controlled whole-system power latch
or eFuse interface is assigned. This is not a BMS command and does not claim to
disconnect the battery. A board-specific implementation must return true only
when a real request has been accepted. Failure/unavailability leaves motors
stopped. Snapshots expose shutdown phase, start time and power-off acceptance.

Configuration lives in `fw/bsp/battery/battery_config.h`: low=20%, critical=10%,
delay=30000 ms, blink half-period=100 ms, SOC expiry=1000 ms. Pack metadata now
uses the user's 4000 mAh model; capacity is not used to guess SOC.

Validation (mocked HAL/board, no physical battery or LED measurement):

```
python3 tests/lifecycle/run_tests.py --battery-soc --sanitize
python3 tests/lifecycle/run_tests.py --battery --sanitize
python3 tests/lifecycle/run_tests.py --battery-defaults --sanitize
python3 tests/lifecycle/run_tests.py --leds --sanitize
python3 tests/leds/run_tests.py
```

Tests cover boundaries, timestamp/range validation, 30-second deadline, blink,
stale/high-SOC retention, tick wrap, source clock advance, common motor stop,
recovery refusal, one-shot power-off ordering, GRB pulse encoding, DMA buffer
ownership, reset gaps and failed-transmit quarantine.
